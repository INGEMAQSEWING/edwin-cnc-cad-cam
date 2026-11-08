/*****************************************************************************/
/*  cam.h - Generador de código G de EDWIN CNC para corte por plasma (Mach3)  */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC                                             */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/
//
// Reemplazo de SheetCAM para mesas de plasma X/Y (sin Z) con Mach3. Sin Qt: se prueba solo (pruebas_cam.cpp).
// Pasos: 1) piezas del dibujo → contornos cerrados; 2) cuáles son huecos y cuáles el contorno de afuera;
// 3) separación de la silueta (kerf: el recorrido se corre la mitad del ancho del corte hacia el desperdicio);
// 4) colita de entrada y de salida del lado del desperdicio; 5) orden (huecos antes que su contorno, el más
// cercano primero); 6) código G con el mismo formato del post "Mach3 plasma no Z" de SheetCAM
// (edwin/referencia/ELIPSE-sheetcam.TAP): N0010…, G21, G90 G91.1, M03, G04 P(segundos), G02/G03 con I J.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace cam {

constexpr double PI = 3.14159265358979323846;

struct P {
    double x = 0, y = 0;
};
inline P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }
inline P operator-(P a, P b) { return {a.x - b.x, a.y - b.y}; }
inline P operator*(P a, double k) { return {a.x * k, a.y * k}; }
inline double cruz(P a, P b) { return a.x * b.y - a.y * b.x; }
inline double punto(P a, P b) { return a.x * b.x + a.y * b.y; }
inline double largo(P a) { return std::hypot(a.x, a.y); }
inline double dist(P a, P b) { return largo(a - b); }
inline P unitario(P a) {
    const double l = largo(a);
    return l > 1e-12 ? a * (1.0 / l) : P{0, 0};
}
inline P izquierda(P a) { return {-a.y, a.x}; }  // normal a la izquierda del avance

// Un tramo: recta de a a b, o arco de a a b con centro c (ccw = contra el reloj).
struct Seg {
    bool arco = false;
    P a, b, c;
    bool ccw = true;
};

inline double radio(const Seg &s) { return dist(s.a, s.c); }
inline double angulo(P c, P p) { return std::atan2(p.y - c.y, p.x - c.x); }
// Ángulo barrido por un arco (positivo, de 0 a 2π; 2π si empieza y termina en el mismo punto).
inline double barrido(const Seg &s) {
    double a1 = angulo(s.c, s.a), a2 = angulo(s.c, s.b);
    double b = s.ccw ? a2 - a1 : a1 - a2;
    while (b <= 1e-12) b += 2 * PI;
    while (b > 2 * PI + 1e-12) b -= 2 * PI;
    return b;
}
inline double largo(const Seg &s) { return s.arco ? radio(s) * barrido(s) : dist(s.a, s.b); }
inline Seg invertir(Seg s) {
    std::swap(s.a, s.b);
    s.ccw = !s.ccw;
    return s;
}
// Punto a una fracción t (0..1) del tramo.
inline P puntoEn(const Seg &s, double t) {
    if (!s.arco) return s.a + (s.b - s.a) * t;
    const double r = radio(s), a = angulo(s.c, s.a) + (s.ccw ? 1 : -1) * barrido(s) * t;
    return {s.c.x + r * std::cos(a), s.c.y + r * std::sin(a)};
}
// Dirección de avance al principio / al final del tramo.
inline P tangenteInicio(const Seg &s) {
    if (!s.arco) return unitario(s.b - s.a);
    const P r = s.a - s.c;
    return unitario(s.ccw ? izquierda(r) : izquierda(r) * -1.0);
}
inline P tangenteFin(const Seg &s) {
    if (!s.arco) return unitario(s.b - s.a);
    const P r = s.b - s.c;
    return unitario(s.ccw ? izquierda(r) : izquierda(r) * -1.0);
}
// Parte el tramo en t: [a..p] y [p..b].
inline void partir(const Seg &s, double t, Seg &uno, Seg &dos) {
    const P p = puntoEn(s, t);
    uno = s;
    dos = s;
    uno.b = p;
    dos.a = p;
}
// Arco que pasa por tres puntos (para convertir elipses en arcos, como SheetCAM).
inline bool arcoPor3(P a, P m, P b, Seg &s) {
    const double d = 2 * (a.x * (m.y - b.y) + m.x * (b.y - a.y) + b.x * (a.y - m.y));
    if (std::fabs(d) < 1e-12) return false;
    const double a2 = a.x * a.x + a.y * a.y, m2 = m.x * m.x + m.y * m.y, b2 = b.x * b.x + b.y * b.y;
    s.arco = true;
    s.a = a;
    s.b = b;
    s.c = {(a2 * (m.y - b.y) + m2 * (b.y - a.y) + b2 * (a.y - m.y)) / d, (a2 * (b.x - m.x) + m2 * (a.x - b.x) + b2 * (m.x - a.x)) / d};
    s.ccw = cruz(m - a, b - m) > 0;
    return true;
}

using Contorno = std::vector<Seg>;

// ─── 1) Entrada: piezas del dibujo ───────────────────────────────────────────────────────────────────────
struct Entrada {
    std::vector<Seg> sueltos;           // rectas y arcos sueltos (se encadenan)
    std::vector<Contorno> cerrados;     // círculos, polilíneas cerradas y elipses completas
};

inline void agregarLinea(Entrada &e, P a, P b) {
    if (dist(a, b) > 1e-9) e.sueltos.push_back({false, a, b, {}, true});
}
// Arco del dibujo (siempre contra el reloj, ángulos en radianes).
inline void agregarArco(Entrada &e, P c, double r, double a1, double a2) {
    Seg s{true, {c.x + r * std::cos(a1), c.y + r * std::sin(a1)}, {c.x + r * std::cos(a2), c.y + r * std::sin(a2)}, c, true};
    e.sueltos.push_back(s);
}
inline void agregarCirculo(Entrada &e, P c, double r) {
    const P der{c.x + r, c.y}, izq{c.x - r, c.y};
    e.cerrados.push_back({{true, der, izq, c, true}, {true, izq, der, c, true}});
}
// Tramo de polilínea con "bulge" (tan(ángulo/4); positivo = contra el reloj).
inline Seg tramoBulge(P a, P b, double bulge) {
    if (std::fabs(bulge) < 1e-9) return {false, a, b, {}, true};
    const double th = 4 * std::atan(bulge), cuerda = dist(a, b);
    const double r = cuerda / (2 * std::sin(std::fabs(th) / 2));
    const P m = (a + b) * 0.5, n = unitario(izquierda(b - a));
    const double h = std::sqrt(std::max(0.0, r * r - cuerda * cuerda / 4));
    // Centro a la izquierda si el arco es menor de 180° y contra el reloj (o mayor y a favor).
    const double lado = ((bulge > 0) == (std::fabs(th) < PI)) ? 1.0 : -1.0;
    return {true, a, b, m + n * (h * lado), bulge > 0};
}
inline void agregarPolilinea(Entrada &e, const std::vector<P> &v, const std::vector<double> &bulges, bool cerrada) {
    Contorno c;
    const size_t n = v.size();
    for (size_t i = 0; i + 1 < n; ++i)
        if (dist(v[i], v[i + 1]) > 1e-9) c.push_back(tramoBulge(v[i], v[i + 1], bulges[i]));
    if (cerrada && n > 2 && dist(v[n - 1], v[0]) > 1e-9) c.push_back(tramoBulge(v[n - 1], v[0], bulges[n - 1]));
    if (c.empty()) return;
    if (cerrada || dist(c.front().a, c.back().b) < 1e-6) e.cerrados.push_back(c);
    else for (const Seg &s : c) e.sueltos.push_back(s);
}
// Elipse (centro, extremo del eje mayor relativo al centro, proporción del menor, parámetros t1→t2 en radianes)
// convertida en arcos con error menor a tol (como SheetCAM, que la entrega en arcos G02/G03).
inline void agregarElipse(Entrada &e, P c, P mayor, double ratio, double t1, double t2, double tol = 0.02) {
    const P menor{-mayor.y * ratio, mayor.x * ratio};
    auto en = [&](double t) { return c + mayor * std::cos(t) + menor * std::sin(t); };
    bool completa = std::fabs(t2 - t1) < 1e-9 || std::fabs(std::fabs(t2 - t1) - 2 * PI) < 1e-6;
    double desde = t1, hasta = t2;
    if (completa) { desde = 0; hasta = 2 * PI; }
    while (hasta <= desde) hasta += 2 * PI;
    int n = 4;
    Contorno arcos;
    for (; n <= 512; n *= 2) {
        arcos.clear();
        double peor = 0;
        for (int i = 0; i < n; ++i) {
            const double ta = desde + (hasta - desde) * i / n, tb = desde + (hasta - desde) * (i + 1) / n, tm = (ta + tb) / 2;
            Seg s;
            if (!arcoPor3(en(ta), en(tm), en(tb), s)) s = {false, en(ta), en(tb), {}, true};
            for (double q : {0.25, 0.75}) {
                const P p = en(ta + (tb - ta) * q);
                peor = std::max(peor, s.arco ? std::fabs(dist(p, s.c) - radio(s)) : 0.0);
            }
            arcos.push_back(s);
        }
        if (peor <= tol) break;
    }
    if (completa) e.cerrados.push_back(arcos);
    else for (const Seg &s : arcos) e.sueltos.push_back(s);
}

// Une los tramos sueltos que se tocan (a menos de tol) en contornos cerrados. Lo que no cierra se cuenta.
inline std::vector<Contorno> encadenar(std::vector<Seg> sueltos, double tol, int &abiertos) {
    std::vector<Contorno> salida;
    std::vector<bool> usado(sueltos.size(), false);
    abiertos = 0;
    for (size_t i = 0; i < sueltos.size(); ++i) {
        if (usado[i]) continue;
        usado[i] = true;
        Contorno c{sueltos[i]};
        bool crece = true;
        while (crece && dist(c.back().b, c.front().a) > tol) {
            crece = false;
            for (size_t j = 0; j < sueltos.size(); ++j) {
                if (usado[j]) continue;
                if (dist(sueltos[j].a, c.back().b) <= tol) { c.push_back(sueltos[j]); }
                else if (dist(sueltos[j].b, c.back().b) <= tol) { c.push_back(invertir(sueltos[j])); }
                else continue;
                c.back().a = c[c.size() - 2].b;  // cierra el hueco diminuto
                usado[j] = true;
                crece = true;
                break;
            }
        }
        if (dist(c.back().b, c.front().a) <= tol) {
            c.back().b = c.front().a;
            salida.push_back(c);
        } else {
            ++abiertos;
        }
    }
    return salida;
}

// ─── 2) Geometría de contornos ────────────────────────────────────────────────────────────────────────────
// Área con signo (positiva = contra el reloj), contando el pedazo de cada arco.
inline double area(const Contorno &c) {
    double s = 0;
    for (const Seg &t : c) {
        s += cruz(t.a, t.b) / 2;
        if (t.arco) {
            const double r = radio(t), th = barrido(t);
            s += (t.ccw ? 1 : -1) * r * r / 2 * (th - std::sin(th));
        }
    }
    return s;
}
inline Contorno invertir(const Contorno &c) {
    Contorno r;
    for (auto it = c.rbegin(); it != c.rend(); ++it) r.push_back(invertir(*it));
    return r;
}
inline std::vector<P> poligono(const Contorno &c, int porArco = 24) {
    std::vector<P> v;
    for (const Seg &s : c) {
        if (!s.arco) { v.push_back(s.a); continue; }
        const int n = std::max(4, static_cast<int>(porArco * barrido(s) / (2 * PI)) + 1);
        for (int i = 0; i < n; ++i) v.push_back(puntoEn(s, double(i) / n));
    }
    return v;
}
inline bool dentro(P p, const std::vector<P> &pol) {
    bool in = false;
    for (size_t i = 0, j = pol.size() - 1; i < pol.size(); j = i++) {
        const P a = pol[i], b = pol[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}

// ─── 3) Separación de la silueta (offset) ──────────────────────────────────────────────────────────────────
// Corte entre dos tramos (rectas o arcos, prolongados): el punto más cercano a "cerca".
inline bool cortar(const Seg &s1, const Seg &s2, P cerca, P &salida) {
    std::vector<P> pts;
    auto lineaLinea = [&](P a, P b, P c, P d) {
        const P r = b - a, s = d - c;
        const double den = cruz(r, s);
        if (std::fabs(den) < 1e-12) return;
        pts.push_back(a + r * (cruz(c - a, s) / den));
    };
    auto lineaCirculo = [&](P a, P b, P c, double rad) {
        const P d = unitario(b - a), f = a - c;
        const double bb = punto(f, d), cc = punto(f, f) - rad * rad, disc = bb * bb - cc;
        if (disc < 0) return;
        const double q = std::sqrt(disc);
        pts.push_back(a + d * (-bb + q));
        pts.push_back(a + d * (-bb - q));
    };
    auto circuloCirculo = [&](P c1, double r1, P c2, double r2) {
        const double d = dist(c1, c2);
        if (d < 1e-12 || d > r1 + r2 + 1e-9 || d < std::fabs(r1 - r2) - 1e-9) return;
        const double a = (r1 * r1 - r2 * r2 + d * d) / (2 * d), h = std::sqrt(std::max(0.0, r1 * r1 - a * a));
        const P u = unitario(c2 - c1), m = c1 + u * a;
        pts.push_back(m + izquierda(u) * h);
        pts.push_back(m - izquierda(u) * h);
    };
    if (!s1.arco && !s2.arco) lineaLinea(s1.a, s1.b, s2.a, s2.b);
    else if (!s1.arco) lineaCirculo(s1.a, s1.b, s2.c, radio(s2));
    else if (!s2.arco) lineaCirculo(s2.a, s2.b, s1.c, radio(s1));
    else circuloCirculo(s1.c, radio(s1), s2.c, radio(s2));
    if (pts.empty()) return false;
    salida = *std::min_element(pts.begin(), pts.end(), [&](P a, P b) { return dist(a, cerca) < dist(b, cerca); });
    return true;
}
// Corre el contorno d hacia la DERECHA del avance (d negativo = izquierda). Esquinas que se abren: arco redondo
// alrededor de la esquina (como SheetCAM); esquinas que se cierran: se recortan donde se cruzan.
inline Contorno desplazar(const Contorno &c, double d, bool &problema) {
    problema = false;
    if (std::fabs(d) < 1e-9) return c;
    std::vector<Seg> o;
    std::vector<P> esquina;  // la esquina original al final de cada tramo
    for (const Seg &s : c) {
        Seg t = s;
        if (!s.arco) {
            const P n = izquierda(unitario(s.b - s.a)) * -d;
            t.a = s.a + n;
            t.b = s.b + n;
        } else {
            // Derecha del avance: hacia afuera si va contra el reloj, hacia adentro si va a favor.
            const double r = radio(s) + (s.ccw ? d : -d);
            if (r <= 1e-6) { problema = true; continue; }  // el arco desaparece (hueco más chico que el corte)
            t.a = s.c + unitario(s.a - s.c) * r;
            t.b = s.c + unitario(s.b - s.c) * r;
        }
        o.push_back(t);
        esquina.push_back(s.b);
    }
    Contorno r;
    const size_t n = o.size();
    if (n == 0) { problema = true; return r; }
    std::vector<Seg> t = o;
    for (size_t i = 0; i < n; ++i) {
        Seg &x = t[i], &y = t[(i + 1) % n];
        const P v = esquina[i];
        if (dist(x.b, y.a) < 1e-7) { y.a = x.b; continue; }
        const double giro = cruz(tangenteFin(x), tangenteInicio(y));
        const bool seAbre = (giro > 0) == (d > 0);
        P q;
        if (!seAbre && cortar(x, y, v, q) && dist(q, v) < 4 * std::fabs(d) + 1e-6) {
            x.b = q;
            y.a = q;
        }
    }
    // Arcos de las esquinas que se abren.
    for (size_t i = 0; i < n; ++i) {
        r.push_back(t[i]);
        const Seg &y = t[(i + 1) % n];
        if (dist(t[i].b, y.a) > 1e-7) {
            const P v = esquina[i];
            if (std::fabs(dist(t[i].b, v) - std::fabs(d)) < 1e-6 && std::fabs(dist(y.a, v) - std::fabs(d)) < 1e-6)
                r.push_back({true, t[i].b, y.a, v, d > 0});
            else
                r.push_back({false, t[i].b, y.a, {}, true});
        }
    }
    // Tramos que quedaron al revés (el corte es más ancho que el detalle): se avisa.
    for (const Seg &s : r)
        if (!s.arco && largo(s) < 1e-9) problema = true;
    return r;
}

// ─── 4) Parámetros del corte (los nombres de la pantalla) ─────────────────────────────────────────────────
struct Parametros {
    double kerf = 1.5;              // Separación de la silueta (Kerf): ancho del corte en mm; se corre la mitad
    double velocidad = 2500;        // Velocidad de corte (mm/min)
    double perforar = 0.5;          // Tiempo para perforar (Pierce delay), segundos
    int entradaTipo = 1;            // Colita de entrada (Lead in): 0 ninguna, 1 recta, 2 curva
    double entrada = 3;             // largo en mm
    int salidaTipo = 1;             // Colita al final (Lead out): 0 ninguna, 1 recta, 2 curva
    double salida = 2;
    double desacelerarMm = 0;       // Desaceleración al terminar: últimos mm…
    double desacelerarPct = 60;     // …a este % de la velocidad
    double pausaFinal = 0;          // Pausa al terminar el contorno, antes de apagar (s)
    bool volverAlOrigen = true;     // G00 X0 Y0 al final (como SheetCAM)
    std::string nombreArchivo = "PIEZA.TAP";
    std::string nombrePieza = "Pieza";
};

// Un contorno ya listo para cortar.
struct Corte {
    Contorno camino;                // recorrido de la antorcha (con la separación aplicada)
    Contorno entrada, salida;       // colitas
    bool hueco = false;             // hueco (corte por dentro) o contorno de afuera
    int profundidad = 0;
    bool problema = false;          // el corte es más ancho que algún detalle
};

// Empieza el contorno en el medio de su recta más larga; si no tiene rectas, en su punto más alto (como el .tap
// de SheetCAM con la elipse, que arranca arriba).
inline Contorno elegirInicio(const Contorno &c) {
    int mejor = -1;
    double l = 0;
    for (size_t i = 0; i < c.size(); ++i)
        if (!c[i].arco && largo(c[i]) > l) { l = largo(c[i]); mejor = int(i); }
    Contorno r;
    if (mejor >= 0) {
        Seg uno, dos;
        partir(c[mejor], 0.5, uno, dos);
        r.push_back(dos);
        for (size_t k = 1; k < c.size(); ++k) r.push_back(c[(mejor + k) % c.size()]);
        r.push_back(uno);
        return r;
    }
    // Sin rectas: el punto más alto. Primero el vértice más alto (donde se unen dos arcos); si el punto más alto de
    // algún arco (centro + radio hacia arriba) queda claramente por encima de todos los vértices, se parte ahí.
    size_t vi = 0;
    for (size_t i = 1; i < c.size(); ++i)
        if (c[i].a.y > c[vi].a.y + 1e-9) vi = i;
    double alto = c[vi].a.y, mt = -1;
    int mi = -1;
    for (size_t i = 0; i < c.size(); ++i) {
        const Seg &s = c[i];
        if (!s.arco || s.c.y + radio(s) <= alto + 1e-4) continue;
        double rel = s.ccw ? PI / 2 - angulo(s.c, s.a) : angulo(s.c, s.a) - PI / 2;
        while (rel < 0) rel += 2 * PI;
        while (rel >= 2 * PI) rel -= 2 * PI;
        const double b = barrido(s);
        if (rel > 1e-6 && rel < b - 1e-6) { alto = s.c.y + radio(s); mi = int(i); mt = rel / b; }
    }
    if (mi < 0) {
        for (size_t k = 0; k < c.size(); ++k) r.push_back(c[(vi + k) % c.size()]);
        return r;
    }
    Seg uno, dos;
    partir(c[mi], mt, uno, dos);
    r.push_back(dos);
    for (size_t k = 1; k < c.size(); ++k) r.push_back(c[(mi + k) % c.size()]);
    r.push_back(uno);
    return r;
}

// Colitas del lado del desperdicio. "afuera" = vector unitario hacia el desperdicio en el punto de inicio.
inline Contorno colitaEntrada(P inicio, P avance, P afuera, int tipo, double l) {
    if (tipo == 0 || l <= 1e-6) return {};
    if (tipo == 1) return {{false, inicio + afuera * l, inicio, {}, true}};
    // Curva: cuarto de círculo de radio l que llega tangente al recorrido.
    const P c = inicio + afuera * l, desde = c - avance * l;
    return {{true, desde, inicio, c, cruz(desde - c, inicio - c) > 0}};
}
inline Contorno colitaSalida(P fin, P avance, P afuera, int tipo, double l) {
    if (tipo == 0 || l <= 1e-6) return {};
    if (tipo == 1) return {{false, fin, fin + afuera * l, {}, true}};
    const P c = fin + afuera * l, hasta = c + avance * l;
    return {{true, fin, hasta, c, cruz(fin - c, hasta - c) > 0}};
}

// ─── 5) Preparar todos los cortes ─────────────────────────────────────────────────────────────────────────
struct Resultado {
    std::vector<Corte> cortes;      // en el orden de corte
    int abiertos = 0;               // contornos que no cierran (no se cortan)
    int problemas = 0;              // contornos donde el corte es más ancho que un detalle
};

inline Resultado preparar(const Entrada &e, const Parametros &prm, double tolUnion = 0.05) {
    Resultado res;
    std::vector<Contorno> cs = encadenar(e.sueltos, tolUnion, res.abiertos);
    for (const Contorno &c : e.cerrados) cs.push_back(c);
    const size_t n = cs.size();
    std::vector<std::vector<P>> pols(n);
    std::vector<double> areas(n);
    for (size_t i = 0; i < n; ++i) { pols[i] = poligono(cs[i]); areas[i] = std::fabs(area(cs[i])); }
    // Profundidad: cuántos contornos lo encierran (par = contorno de afuera de una pieza; impar = hueco).
    std::vector<int> prof(n, 0);
    std::vector<std::vector<int>> encierra(n);  // encierra[i] = contornos que están dentro de i
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j)
            if (i != j && areas[j] > areas[i] && dentro(pols[i][0], pols[j])) { ++prof[i]; encierra[j].push_back(int(i)); }
    std::vector<Corte> listos(n);
    for (size_t i = 0; i < n; ++i) {
        Corte &k = listos[i];
        k.hueco = prof[i] % 2 == 1;
        k.profundidad = prof[i];
        // Contorno de afuera: a favor del reloj (G02); hueco: contra el reloj (G03). Así va el .tap de SheetCAM.
        Contorno c = cs[i];
        const bool ccw = area(c) > 0;
        if (k.hueco != ccw) c = invertir(c);
        // A favor del reloj, la derecha del avance es adentro; contra el reloj, también adentro del hueco… el
        // desperdicio queda a la IZQUIERDA del avance en ambos casos: se corre hacia la izquierda.
        bool prob = false;
        c = desplazar(c, -prm.kerf / 2, prob);
        k.problema = prob || c.empty();
        if (c.empty()) continue;
        c = elegirInicio(c);
        k.camino = c;
        const P ini = c.front().a, tIni = tangenteInicio(c.front()), tFin = tangenteFin(c.back());
        const P afueraIni = izquierda(tIni), afueraFin = izquierda(tFin);
        // En huecos chicos la colita no puede ser más larga que un tercio del hueco.
        double lim = 1e300;
        if (k.hueco) lim = std::sqrt(std::fabs(area(c))) / 3;
        k.entrada = colitaEntrada(ini, tIni, afueraIni, prm.entradaTipo, std::min(prm.entrada, lim));
        k.salida = colitaSalida(c.back().b, tFin, afueraFin, prm.salidaTipo, std::min(prm.salida, lim));
        if (k.problema) ++res.problemas;
    }
    // Orden: un contorno no se corta antes que lo que tiene adentro; entre los disponibles, el más cercano.
    std::vector<bool> hecho(n, false);
    P donde{0, 0};
    for (size_t paso = 0; paso < n; ++paso) {
        int mejor = -1;
        double md = 1e300;
        for (size_t i = 0; i < n; ++i) {
            if (hecho[i]) continue;
            bool listo = true;
            for (int j : encierra[i]) if (!hecho[j]) { listo = false; break; }
            if (!listo) continue;
            const P inicio = !listos[i].entrada.empty() ? listos[i].entrada.front().a : (!listos[i].camino.empty() ? listos[i].camino.front().a : P{});
            const double dd = dist(donde, inicio);
            if (dd < md) { md = dd; mejor = int(i); }
        }
        if (mejor < 0) break;
        hecho[mejor] = true;
        const Corte &k = listos[mejor];
        if (k.camino.empty()) continue;
        res.cortes.push_back(k);
        donde = !k.salida.empty() ? k.salida.back().b : k.camino.back().b;
    }
    return res;
}

// ─── 6) Código G (formato del post "Mach3 plasma no Z" de SheetCAM) ────────────────────────────────────────
inline std::string f4(double v) {
    if (std::fabs(v) < 0.00005) v = 0;
    char b[32];
    std::snprintf(b, sizeof b, "%.4f", v);
    return b;
}
inline std::string corto(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.3f", v);
    std::string s = b;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s += '0';
    return s;
}

class Escritor {
public:
    std::string texto;
    void linea(const std::string &l) {
        char b[16];
        std::snprintf(b, sizeof b, "N%04d ", n);
        texto += b + l + "\r\n";
        n += 10;
    }
private:
    int n = 10;
};

// Arcos de más de 180° se parten en dos (Mach3 los acepta igual, pero así no hay dudas).
inline void moverA(Escritor &w, const Seg &s, std::string &f) {
    std::vector<Seg> trozos{s};
    if (s.arco && barrido(s) > PI + 1e-9) {
        Seg uno, dos;
        partir(s, 0.5, uno, dos);
        trozos = {uno, dos};
    }
    for (const Seg &t : trozos) {
        std::string l;
        if (!t.arco) l = "G01 X" + f4(t.b.x) + " Y" + f4(t.b.y);
        else l = std::string(t.ccw ? "G03" : "G02") + " X" + f4(t.b.x) + " Y" + f4(t.b.y) + " I" + f4(t.c.x - t.a.x) + " J" + f4(t.c.y - t.a.y);
        if (!f.empty()) { l += " " + f; f.clear(); }
        w.linea(l);
    }
}

inline std::string codigoG(const Resultado &r, const Parametros &prm) {
    Escritor w;
    w.linea("(Filename: " + prm.nombreArchivo + ")");
    w.linea("(Post processor: EDWIN CNC CAD-CAM - Mach3 plasma no Z)");
    w.linea("G21 (Units: Metric)");
    w.linea("G90 G91.1");
    w.linea("F1");
    w.linea("(Part: " + prm.nombrePieza + ")");
    const std::string fNormal = "F" + corto(prm.velocidad);
    const std::string fLenta = "F" + corto(prm.velocidad * prm.desacelerarPct / 100.0);
    for (const Corte &k : r.cortes) {
        w.linea(std::string("(Operation: ") + (k.hueco ? "Inside" : "Outside") + " Offset, " + corto(prm.kerf) + ", T1: Plasma)");
        const P inicio = !k.entrada.empty() ? k.entrada.front().a : k.camino.front().a;
        w.linea("G00 X" + f4(inicio.x) + " Y" + f4(inicio.y));
        w.linea("M03");
        if (prm.perforar > 0) w.linea("G04 P" + corto(prm.perforar));
        std::string f = fNormal;
        for (const Seg &s : k.entrada) moverA(w, s, f);
        // Desaceleración: los últimos mm del contorno a menor velocidad.
        double total = 0;
        for (const Seg &s : k.camino) total += largo(s);
        const double lento = std::min(prm.desacelerarMm, total);
        double recorrido = 0;
        bool yaLento = false;
        for (const Seg &s : k.camino) {
            const double l = largo(s);
            if (!yaLento && lento > 0 && recorrido + l > total - lento + 1e-9) {
                const double t = std::max(0.0, (total - lento - recorrido) / l);
                if (t > 1e-6) {
                    Seg uno, dos;
                    partir(s, t, uno, dos);
                    moverA(w, uno, f);
                    f = fLenta;
                    moverA(w, dos, f);
                } else {
                    f = fLenta;
                    moverA(w, s, f);
                }
                yaLento = true;
            } else {
                moverA(w, s, f);
            }
            recorrido += l;
        }
        for (const Seg &s : k.salida) moverA(w, s, f);
        if (prm.pausaFinal > 0) w.linea("G04 P" + corto(prm.pausaFinal));
        w.linea("M05");
    }
    if (prm.volverAlOrigen) w.linea("G00 X0.0000 Y0.0000");
    w.linea("M05 M30");
    return w.texto;
}

}  // namespace cam
