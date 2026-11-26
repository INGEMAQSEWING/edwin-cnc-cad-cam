/*****************************************************************************/
/*  arq.h - Cálculos de DuctusArq 2D (sin Qt, se prueban solos)               */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*****************************************************************************/
//
// Todo en unidades del dibujo:
//   - sistema métrico: METROS (se escribe 3.50 o 3,50);
//   - sistema imperial: PULGADAS, como el AutoCAD de EE. UU.; se escribe 12'6", 12'-6", 6",
//     12.5' o un número solo, que se toma en PIES (12.5 = 12'-6").
// Los tamaños de papel (texto, burbujas, separación de cotas) se dan en mm de papel y se pasan
// a unidades del dibujo con la escala (1:50, 1/4" = 1'-0" = 1:48, ...).

#ifndef DUCTUSARQ_ARQ_H
#define DUCTUSARQ_ARQ_H

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace arq {

struct P {
    double x = 0, y = 0;
    P() = default;
    P(double x_, double y_) : x(x_), y(y_) {}
};

inline P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }
inline P operator-(P a, P b) { return {a.x - b.x, a.y - b.y}; }
inline P operator*(P a, double k) { return {a.x * k, a.y * k}; }
inline double largo(P a) { return std::hypot(a.x, a.y); }

struct Seg {
    P a, b;
};

enum class Sistema { Metrico, Imperial };

// ---------------------------------------------------------------------------
// Unidades y escala
struct Unidades {
    Sistema sistema = Sistema::Metrico;
    double escala = 50;  // denominador: 1:50 (métrico) o 1:48 (1/4" = 1'-0")

    // mm de papel -> unidades del dibujo
    double papel(double mm) const {
        return sistema == Sistema::Metrico ? mm * escala / 1000.0 : mm * escala / 25.4;
    }
    // metros -> unidades del dibujo (para valores sugeridos)
    double metros(double m) const {
        return sistema == Sistema::Metrico ? m : m / 0.0254;
    }
};

inline std::string recortar(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// Lee un número con punto o coma decimal y fracción opcional ("1/2", "6 1/2").
inline bool leerNumero(const std::string &texto, double &valor) {
    std::string s = recortar(texto);
    if (s.empty()) return false;
    for (char &c : s) if (c == ',') c = '.';
    // "6 1/2" o "1/2"
    double entero = 0;
    std::string resto = s;
    const size_t esp = s.find(' ');
    if (esp != std::string::npos) {
        if (!leerNumero(s.substr(0, esp), entero)) return false;
        resto = recortar(s.substr(esp + 1));
    }
    const size_t barra = resto.find('/');
    if (barra != std::string::npos) {
        char *fin = nullptr;
        const std::string n = resto.substr(0, barra), d = resto.substr(barra + 1);
        const double num = std::strtod(n.c_str(), &fin);
        if (fin == n.c_str() || *fin != '\0') return false;
        const double den = std::strtod(d.c_str(), &fin);
        if (fin == d.c_str() || *fin != '\0' || den == 0) return false;
        valor = entero + (entero < 0 ? -num / den : num / den);
        return true;
    }
    if (esp != std::string::npos) return false;  // "6 7" no es válido
    char *fin = nullptr;
    const double v = std::strtod(resto.c_str(), &fin);
    if (fin == resto.c_str() || *fin != '\0' || !std::isfinite(v)) return false;
    valor = v;
    return true;
}

// Longitud escrita por el usuario -> unidades del dibujo (m o pulgadas).
inline bool leerLongitud(const std::string &texto, Sistema sistema, double &valor) {
    std::string s = recortar(texto);
    if (s.empty()) return false;
    if (sistema == Sistema::Metrico) {
        // Se aceptan "3.5", "3,5", "3.5m" y "350cm".
        std::string t = s;
        double factor = 1;
        if (t.size() > 2 && t.compare(t.size() - 2, 2, "cm") == 0) { factor = 0.01; t.resize(t.size() - 2); }
        else if (t.size() > 2 && t.compare(t.size() - 2, 2, "mm") == 0) { factor = 0.001; t.resize(t.size() - 2); }
        else if (!t.empty() && t.back() == 'm') t.pop_back();
        double v;
        if (!leerNumero(t, v)) return false;
        valor = v * factor;
        return true;
    }
    // Imperial: pies ' y pulgadas ". Comillas tipográficas también.
    std::string t;
    for (size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        // ’ ” ″ ′ en UTF-8 (E2 80 99, E2 80 9D, E2 80 B3, E2 80 B2)
        if (c == 0xE2 && i + 2 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x80) {
            const unsigned char d = static_cast<unsigned char>(s[i + 2]);
            if (d == 0x99 || d == 0xB2) { t += '\''; i += 2; continue; }
            if (d == 0x9D || d == 0xB3) { t += '"'; i += 2; continue; }
        }
        t += s[i];
    }
    const size_t pie = t.find('\'');
    double pies = 0, pulgadas = 0;
    std::string resto = t;
    bool hayPies = false;
    if (pie != std::string::npos) {
        if (!leerNumero(t.substr(0, pie), pies)) return false;
        hayPies = true;
        resto = recortar(t.substr(pie + 1));
        if (!resto.empty() && resto[0] == '-') resto = recortar(resto.substr(1));
    }
    if (!resto.empty()) {
        const bool conComillas = resto.back() == '"';
        if (conComillas) resto = recortar(resto.substr(0, resto.size() - 1));
        double v;
        if (!leerNumero(resto, v)) return false;
        if (conComillas || hayPies) pulgadas = v;   // 12'6 = 12'-6"
        else pies = v;                              // número solo = pies
    }
    valor = (pies < 0 ? -1 : 1) * (std::fabs(pies) * 12.0 + pulgadas);
    return true;
}

// Lista de longitudes separadas por espacios o punto y coma ("3.50 3.00 3.50", "12'-6\" 10'").
inline bool leerLista(const std::string &texto, Sistema sistema, std::vector<double> &valores) {
    valores.clear();
    std::string actual;
    auto cerrar = [&]() -> bool {
        const std::string t = recortar(actual);
        actual.clear();
        if (t.empty()) return true;
        double v;
        if (!leerLongitud(t, sistema, v)) return false;
        valores.push_back(v);
        return true;
    };
    for (size_t i = 0; i < texto.size(); ++i) {
        const char c = texto[i];
        // En imperial "12' 6\"" lleva espacio: el espacio después de ' no separa.
        const bool separa = c == ';' || c == '\n' || c == '\t'
            || (c == ' ' && !(sistema == Sistema::Imperial && !actual.empty() && actual.back() == '\''))
            || (c == ' ' && sistema == Sistema::Metrico);
        if (separa) { if (!cerrar()) return false; }
        else actual += c;
    }
    return cerrar();
}

// Fracción de pulgada más cercana a 1/den (den = 16).
inline std::string pulgadasTexto(double pulg, int den = 16) {
    long total = std::lround(pulg * den);
    const long entero = total / den;
    long num = total % den;
    int d = den;
    while (num != 0 && num % 2 == 0 && d % 2 == 0) { num /= 2; d /= 2; }
    char buf[64];
    if (num == 0) std::snprintf(buf, sizeof buf, "%ld\"", entero);
    else if (entero == 0) std::snprintf(buf, sizeof buf, "%ld/%d\"", num, d);
    else std::snprintf(buf, sizeof buf, "%ld %ld/%d\"", entero, num, d);
    return buf;
}

// Unidades del dibujo -> texto: "3.50" (m) o "12'-6\"" (pulgadas).
inline std::string textoLongitud(double v, Sistema sistema) {
    char buf[64];
    if (sistema == Sistema::Metrico) {
        std::snprintf(buf, sizeof buf, "%.2f", v);
        return buf;
    }
    const bool neg = v < 0;
    double a = std::fabs(v);
    long pies = static_cast<long>(std::floor(a / 12.0 + 1e-9));
    double pulg = a - pies * 12.0;
    if (pulg > 12.0 - 1.0 / 32) { ++pies; pulg = 0; }
    std::string r = neg ? "-" : "";
    if (pies > 0) r += std::to_string(pies) + "'-";
    r += pulgadasTexto(pulg);
    return r;
}

// ---------------------------------------------------------------------------
// Ejes: nombres y posiciones
// Letras A, B, C ... (sin I ni O, para no confundirlas con 1 y 0), luego AA, AB, ...
inline std::string letraEje(int i) {
    static const char letras[] = "ABCDEFGHJKLMNPQRSTUVWXYZ";  // 24 letras
    const int n = 24;
    std::string r;
    if (i < n) r += letras[i];
    else { r += letras[(i / n - 1) % n]; r += letras[i % n]; }
    return r;
}

// Distancias entre ejes -> posiciones acumuladas. Un 0 al inicio se ignora ("0 3.50 3.00").
inline std::vector<double> posicionesEjes(const std::vector<double> &distancias) {
    std::vector<double> pos{0.0};
    for (size_t i = 0; i < distancias.size(); ++i) {
        if (i == 0 && distancias[i] == 0) continue;
        pos.push_back(pos.back() + distancias[i]);
    }
    return pos;
}

// ---------------------------------------------------------------------------
// Unión de rectángulos alineados (muros, columnas, zapatas, vigas): devuelve el CONTORNO
// limpio, así las uniones en L, T y X quedan sin líneas por dentro.
struct Rect {
    double x1, y1, x2, y2;  // x1 < x2, y1 < y2
};

inline Rect rect(double xa, double ya, double xb, double yb) {
    return {std::min(xa, xb), std::min(ya, yb), std::max(xa, xb), std::max(ya, yb)};
}

inline std::vector<Seg> contornoUnion(const std::vector<Rect> &rs, double tol = 1e-9) {
    std::vector<double> xs, ys;
    for (const Rect &r : rs) {
        if (r.x2 - r.x1 <= tol || r.y2 - r.y1 <= tol) continue;
        xs.push_back(r.x1); xs.push_back(r.x2);
        ys.push_back(r.y1); ys.push_back(r.y2);
    }
    auto unicos = [tol](std::vector<double> &v) {
        std::sort(v.begin(), v.end());
        std::vector<double> u;
        for (double a : v) if (u.empty() || a - u.back() > tol) u.push_back(a);
        v.swap(u);
    };
    unicos(xs);
    unicos(ys);
    std::vector<Seg> out;
    if (xs.size() < 2 || ys.size() < 2) return out;
    const size_t nx = xs.size() - 1, ny = ys.size() - 1;
    std::vector<char> lleno(nx * ny, 0);
    for (size_t i = 0; i < nx; ++i) {
        const double cx = (xs[i] + xs[i + 1]) / 2;
        for (size_t j = 0; j < ny; ++j) {
            const double cy = (ys[j] + ys[j + 1]) / 2;
            for (const Rect &r : rs)
                if (cx > r.x1 && cx < r.x2 && cy > r.y1 && cy < r.y2) { lleno[i * ny + j] = 1; break; }
        }
    }
    auto celda = [&](long i, long j) -> bool {
        if (i < 0 || j < 0 || i >= static_cast<long>(nx) || j >= static_cast<long>(ny)) return false;
        return lleno[static_cast<size_t>(i) * ny + static_cast<size_t>(j)] != 0;
    };
    // Bordes horizontales (y = ys[j]) entre celda de abajo (j-1) y de arriba (j), unidos a lo largo.
    for (size_t j = 0; j <= ny; ++j) {
        long ini = -1;
        int lado = 0;
        for (size_t i = 0; i <= nx; ++i) {
            int l = 0;
            if (i < nx) {
                const bool ab = celda(static_cast<long>(i), static_cast<long>(j) - 1);
                const bool ar = celda(static_cast<long>(i), static_cast<long>(j));
                l = ab == ar ? 0 : (ar ? 1 : -1);
            }
            if (l != lado) {
                if (lado != 0) out.push_back({P(xs[static_cast<size_t>(ini)], ys[j]), P(xs[i], ys[j])});
                lado = l;
                ini = static_cast<long>(i);
            }
        }
    }
    // Bordes verticales (x = xs[i]).
    for (size_t i = 0; i <= nx; ++i) {
        long ini = -1;
        int lado = 0;
        for (size_t j = 0; j <= ny; ++j) {
            int l = 0;
            if (j < ny) {
                const bool iz = celda(static_cast<long>(i) - 1, static_cast<long>(j));
                const bool de = celda(static_cast<long>(i), static_cast<long>(j));
                l = iz == de ? 0 : (de ? 1 : -1);
            }
            if (l != lado) {
                if (lado != 0) out.push_back({P(xs[i], ys[static_cast<size_t>(ini)]), P(xs[i], ys[j])});
                lado = l;
                ini = static_cast<long>(j);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Muro por puntos (cualquier ángulo): las dos caras paralelas con esquinas a inglete.
// justificacion: 0 = el eje va al centro, 1 = los puntos son la cara izquierda (el muro crece a
// la derecha del recorrido), -1 = los puntos son la cara derecha.
inline std::vector<P> desfasar(const std::vector<P> &pts, bool cerrado, double d) {
    const size_t n = pts.size();
    std::vector<P> r;
    if (n < 2 || d == 0) return pts;
    auto normal = [](P a, P b) {
        const P v = b - a;
        const double l = largo(v);
        return l < 1e-12 ? P(0, 0) : P(-v.y / l, v.x / l);
    };
    for (size_t i = 0; i < n; ++i) {
        const bool primero = i == 0, ultimo = i == n - 1;
        if (!cerrado && (primero || ultimo)) {
            const P nn = primero ? normal(pts[0], pts[1]) : normal(pts[n - 2], pts[n - 1]);
            r.push_back(pts[i] + nn * d);
            continue;
        }
        const P prev = pts[(i + n - 1) % n], cur = pts[i], next = pts[(i + 1) % n];
        const P n1 = normal(prev, cur), n2 = normal(cur, next);
        P bis = n1 + n2;
        const double lb = largo(bis);
        if (lb < 1e-9) { r.push_back(cur + n1 * d); continue; }  // vuelta en U
        bis = bis * (1.0 / lb);
        double cosMedio = bis.x * n1.x + bis.y * n1.y;
        if (cosMedio < 0.2) cosMedio = 0.2;  // inglete muy agudo: se limita
        r.push_back(cur + bis * (d / cosMedio));
    }
    return r;
}

struct Muro {
    std::vector<P> izquierda, derecha;  // caras del muro
    bool cerrado = false;
};

inline Muro muroPorPuntos(const std::vector<P> &pts, double espesor, int justificacion, bool cerrado) {
    Muro m;
    m.cerrado = cerrado;
    double dIzq, dDer;
    if (justificacion == 0) { dIzq = espesor / 2; dDer = -espesor / 2; }
    else if (justificacion == 1) { dIzq = 0; dDer = -espesor; }
    else { dIzq = espesor; dDer = 0; }
    m.izquierda = desfasar(pts, cerrado, dIzq);
    m.derecha = desfasar(pts, cerrado, dDer);
    return m;
}

// ---------------------------------------------------------------------------
// Casa rápida: rectángulo de la casa con muros exteriores, divisiones interiores por ejes
// intermedios, ejes en el centro de los muros y columnas opcionales en los cruces.
struct Casa {
    double ancho = 6, largo = 15;          // medidas EXTERIORES de la casa
    double muroExt = 0.15, muroInt = 0.10; // espesores
    std::vector<double> divX, divY;        // distancias de los ejes interiores desde el borde
    double columna = 0;                    // lado de columna cuadrada (0 = sin columnas)
};

struct CasaGeom {
    std::vector<Rect> muros;               // para contornoUnion
    std::vector<Rect> columnas;
    std::vector<double> ejesX, ejesY;      // posición de cada eje (centro de muro)
};

inline CasaGeom casaGeom(const Casa &c) {
    CasaGeom g;
    const double e = c.muroExt, W = c.ancho, L = c.largo;
    g.muros.push_back(rect(0, 0, W, e));           // abajo
    g.muros.push_back(rect(0, L - e, W, L));       // arriba
    g.muros.push_back(rect(0, 0, e, L));           // izquierda
    g.muros.push_back(rect(W - e, 0, W, L));       // derecha
    g.ejesX.push_back(e / 2);
    for (double x : c.divX)
        if (x > e && x < W - e) {
            g.muros.push_back(rect(x - c.muroInt / 2, e, x + c.muroInt / 2, L - e));
            g.ejesX.push_back(x);
        }
    g.ejesX.push_back(W - e / 2);
    g.ejesY.push_back(e / 2);
    for (double y : c.divY)
        if (y > e && y < L - e) {
            g.muros.push_back(rect(e, y - c.muroInt / 2, W - e, y + c.muroInt / 2));
            g.ejesY.push_back(y);
        }
    g.ejesY.push_back(L - e / 2);
    std::sort(g.ejesX.begin(), g.ejesX.end());
    std::sort(g.ejesY.begin(), g.ejesY.end());
    if (c.columna > 0)
        for (double x : g.ejesX)
            for (double y : g.ejesY) {
                // La columna no se sale de la casa: se corre hacia adentro en el borde.
                double cx = std::min(std::max(x, c.columna / 2), W - c.columna / 2);
                double cy = std::min(std::max(y, c.columna / 2), L - c.columna / 2);
                g.columnas.push_back(rect(cx - c.columna / 2, cy - c.columna / 2,
                                          cx + c.columna / 2, cy + c.columna / 2));
            }
    return g;
}

// ---------------------------------------------------------------------------
// Pórtico en alzado: columnas, vigas por piso y zapatas.
struct Portico {
    std::vector<double> vanos;     // distancias entre ejes de columnas
    std::vector<double> alturas;   // altura de cada piso (de nivel a nivel)
    double columna = 0.30;         // ancho de columna en el alzado
    double viga = 0.40;            // alto de la viga (la parte de arriba = nivel del piso)
    double zapataAncho = 1.20, zapataAlto = 0.40, profundidad = 1.20;  // 0 = sin zapata
};

struct PorticoGeom {
    std::vector<Rect> piezas;      // columnas + vigas (contorno limpio)
    std::vector<Rect> zapatas;
    std::vector<double> ejes;      // x de cada eje
    std::vector<double> niveles;   // y de cada nivel (0 = piso fino del primer piso)
};

inline PorticoGeom porticoGeom(const Portico &p) {
    PorticoGeom g;
    g.ejes = posicionesEjes(p.vanos);
    g.niveles.push_back(0);
    for (double h : p.alturas) g.niveles.push_back(g.niveles.back() + h);
    const double arriba = g.niveles.back();
    const double fondo = p.zapataAncho > 0 ? -p.profundidad + p.zapataAlto : 0;
    for (double x : g.ejes) {
        g.piezas.push_back(rect(x - p.columna / 2, fondo, x + p.columna / 2, arriba));
        if (p.zapataAncho > 0)
            g.zapatas.push_back(rect(x - p.zapataAncho / 2, -p.profundidad, x + p.zapataAncho / 2, fondo));
    }
    if (g.ejes.size() >= 2)
        for (size_t k = 1; k < g.niveles.size(); ++k)
            g.piezas.push_back(rect(g.ejes.front() - p.columna / 2, g.niveles[k] - p.viga,
                                    g.ejes.back() + p.columna / 2, g.niveles[k]));
    return g;
}

// ---------------------------------------------------------------------------
// Cimentación en planta: zapatas en cada cruce de ejes y vigas de amarre entre ellas.
struct Cimentacion {
    std::vector<double> ejesX, ejesY;   // posiciones
    double zapata = 1.20;               // lado de zapata cuadrada
    double viga = 0.30;                 // ancho de la viga de cimentación (0 = sin vigas)
    double columna = 0.30;
};

struct CimentacionGeom {
    std::vector<Rect> zapatas, vigas, columnas;
};

inline CimentacionGeom cimentacionGeom(const Cimentacion &c) {
    CimentacionGeom g;
    for (double x : c.ejesX)
        for (double y : c.ejesY) {
            g.zapatas.push_back(rect(x - c.zapata / 2, y - c.zapata / 2, x + c.zapata / 2, y + c.zapata / 2));
            if (c.columna > 0)
                g.columnas.push_back(rect(x - c.columna / 2, y - c.columna / 2, x + c.columna / 2, y + c.columna / 2));
        }
    if (c.viga > 0) {
        for (double y : c.ejesY)
            for (size_t i = 1; i < c.ejesX.size(); ++i)
                g.vigas.push_back(rect(c.ejesX[i - 1], y - c.viga / 2, c.ejesX[i], y + c.viga / 2));
        for (double x : c.ejesX)
            for (size_t j = 1; j < c.ejesY.size(); ++j)
                g.vigas.push_back(rect(x - c.viga / 2, c.ejesY[j - 1], x + c.viga / 2, c.ejesY[j]));
    }
    return g;
}

// ---------------------------------------------------------------------------
// Rótulo (cajetín): hojas, disciplinas y la grilla de casillas.
struct Hoja {
    const char *nombre;
    double ancho, alto;   // mm, horizontal
    bool imperial;        // márgenes en pulgadas
    bool grande;          // rótulo grande (con tabla de revisiones)
};

inline const std::vector<Hoja> &hojas() {
    static const std::vector<Hoja> h = {
        {"A4 (297 x 210 mm)", 297, 210, false, false},
        {"A3 (420 x 297 mm)", 420, 297, false, false},
        {"A2 (594 x 420 mm)", 594, 420, false, false},
        {"A1 (841 x 594 mm)", 841, 594, false, true},
        {"A0 (1189 x 841 mm)", 1189, 841, false, true},
        {"Carta / Letter (11 x 8.5 in)", 279.4, 215.9, true, false},
        {"Tabloide / Tabloid (17 x 11 in)", 431.8, 279.4, true, false},
        {"ARCH C (24 x 18 in)", 609.6, 457.2, true, false},
        {"ARCH D (36 x 24 in)", 914.4, 609.6, true, true},
        {"ARCH E (48 x 36 in)", 1219.2, 914.4, true, true},
    };
    return h;
}

struct Disciplina {
    const char *letra;      // código NCS (EE. UU.)
    const char *local;      // prefijo usual en Colombia / México
    const char *espanol;
    const char *ingles;
};

inline const std::vector<Disciplina> &disciplinas() {
    static const std::vector<Disciplina> d = {
        {"A", "ARQ", "Arquitectónico", "Architectural"},
        {"S", "EST", "Estructural (incluye cimentación)", "Structural"},
        {"E", "IE", "Eléctrico", "Electrical"},
        {"P", "HS", "Hidrosanitario", "Plumbing"},
        {"PG", "GAS", "Gas", "Fuel gas"},
        {"T", "TEL", "Telecomunicaciones", "Telecommunications"},
        {"M", "AA", "Mecánico / aire acondicionado", "Mechanical / HVAC"},
        {"F", "RCI", "Contra incendio", "Fire protection"},
        {"I", "INT", "Interiores / acabados", "Interiors"},
        {"C", "URB", "Civil / urbanismo", "Civil"},
        {"L", "PAI", "Paisajismo", "Landscape"},
        {"V", "TOP", "Topografía", "Survey"},
        {"B", "GEO", "Geotecnia / suelos", "Geotechnical"},
        {"G", "G", "General (portada, índice)", "General"},
        {"AD", "DEM", "Demoliciones", "Demolition"},
        {"W", "FV", "Energía solar", "Distributed energy"},
        {"Q", "EQ", "Equipos", "Equipment"},
        {"X", "X", "Otro", "Other"},
    };
    return d;
}

// Casilla del rótulo en mm de papel, medida desde la esquina inferior izquierda del rótulo.
struct Casilla {
    double x, y, w, h;
    std::string etiqueta;   // texto pequeño arriba a la izquierda ("PROYECTO")
    std::string clave;      // campo que va adentro
    double alto;            // altura del texto del valor (mm)
};

struct Rotulo {
    double ancho = 180, alto = 60;
    std::vector<Casilla> casillas;
};

// Rótulo base 180 x 60 mm (A4 a A2, Carta, Tabloide, ARCH C) o grande 180 x 100 mm (A1, A0,
// ARCH D, ARCH E) con tabla de revisiones y espacio para el sello de la curaduría / permit stamp.
inline Rotulo rotulo(bool grande) {
    Rotulo r;
    r.alto = grande ? 100 : 60;
    auto c = [&r](double x, double y, double w, double h, const char *et, const char *cl, double alto) {
        r.casillas.push_back({x, y, w, h, et, cl, alto});
    };
    // Fila de abajo (0..30 mm)
    c(0, 20, 50, 10, "ELABORÓ / DRAWN", "elaboro", 2.5);
    c(0, 10, 50, 10, "REVISÓ / CHECKED", "reviso", 2.5);
    c(0, 0, 25, 10, "FECHA / DATE", "fecha", 2.0);
    c(25, 0, 25, 10, "REVISIÓN / REV.", "revision", 2.5);
    c(50, 15, 90, 15, "CONTENIDO / SHEET TITLE", "contenido", 3.5);
    c(50, 7.5, 90, 7.5, "DISCIPLINA / DISCIPLINE", "disciplina", 2.2);
    c(50, 0, 45, 7.5, "ESCALA / SCALE", "escala", 2.2);
    c(95, 0, 45, 7.5, "FASE / PHASE", "fase", 2.2);
    c(140, 10, 40, 20, "PLANCHA / SHEET", "plancha", 7.0);
    c(140, 0, 40, 10, "HOJA / OF", "hoja", 2.5);
    // Fila de arriba (30..60 mm)
    c(0, 30, 40, 30, "EMPRESA / FIRM", "empresa", 3.0);
    c(40, 50, 100, 10, "PROYECTO / PROJECT", "proyecto", 3.5);
    c(40, 40, 100, 10, "DIRECCIÓN / ADDRESS", "direccion", 2.2);
    c(40, 30, 100, 10, "PROPIETARIO / OWNER", "propietario", 2.5);
    c(140, 30, 40, 30, "PROFESIONAL - FIRMA Y SELLO / STAMP", "profesional", 2.2);
    if (grande) {
        c(0, 60, 120, 40, "REVISIONES / REVISIONS  (N.º - FECHA - DESCRIPCIÓN - INICIALES)", "revisiones", 2.2);
        c(120, 60, 60, 40, "SELLO CURADURÍA / PERMIT STAMP", "sello", 2.2);
    }
    return r;
}

// Márgenes de la hoja (mm): ISO 20 izquierda y 10 el resto; EE. UU. 0.75 in y 0.4 in.
inline void margenes(const Hoja &h, double &izq, double &otros) {
    if (h.imperial) { izq = 0.75 * 25.4; otros = 0.4 * 25.4; }
    else { izq = 20; otros = 10; }
}

// "A-101": disciplina + número.
inline std::string numeroPlancha(const std::string &prefijo, int numero) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s-%d", prefijo.c_str(), numero);
    return buf;
}

// Escala como texto: "1:50" o "1/4\" = 1'-0\"" (denominador 48).
inline std::string textoEscala(double escala, Sistema sistema) {
    char buf[48];
    if (sistema == Sistema::Imperial) {
        // Escala de arquitecto: escala = 12 / (pulgadas de papel por pie).
        struct E { double den; const char *texto; };
        static const E conocidas[] = {
            {192, "1/16\" = 1'-0\""}, {128, "3/32\" = 1'-0\""}, {96, "1/8\" = 1'-0\""},
            {64, "3/16\" = 1'-0\""}, {48, "1/4\" = 1'-0\""}, {32, "3/8\" = 1'-0\""},
            {24, "1/2\" = 1'-0\""}, {16, "3/4\" = 1'-0\""}, {12, "1\" = 1'-0\""},
            {8, "1 1/2\" = 1'-0\""}, {4, "3\" = 1'-0\""}, {1, "FULL SIZE"},
        };
        for (const E &e : conocidas)
            if (std::fabs(e.den - escala) < 1e-6) return e.texto;
        std::snprintf(buf, sizeof buf, "1:%g", escala);
        return buf;
    }
    std::snprintf(buf, sizeof buf, "1:%g", escala);
    return buf;
}

// ---------------------------------------------------------------------------
// Rótulo en FRANJA VERTICAL a la derecha (estilo EE. UU. / NCS, el que usan en Miami):
// arriba queda libre el cuadro de 4.5" x 4.5" para el sello de la ciudad (City of Miami pide
// ese hueco en la esquina de arriba a la derecha) y abajo el número de plancha grande.
// altoMarco = alto del marco en mm. Necesita hojas de ARCH C (18") o más grandes.
inline bool franjaCabe(double altoMarco) { return altoMarco >= 400; }

inline Rotulo rotuloFranja(double altoMarco) {
    Rotulo r;
    r.ancho = 114.3;  // 4.5"
    r.alto = altoMarco;
    const double w = r.ancho;
    double y = 0;
    auto c = [&](double h, const char *et, const char *cl, double alto) {
        r.casillas.push_back({0, y, w, h, et, cl, alto});
        y += h;
    };
    auto dos = [&](double h, const char *et1, const char *cl1, const char *et2, const char *cl2, double alto) {
        r.casillas.push_back({0, y, w / 2, h, et1, cl1, alto});
        r.casillas.push_back({w / 2, y, w / 2, h, et2, cl2, alto});
        y += h;
    };
    c(32, "SHEET / PLANCHA", "plancha", 12.0);
    dos(10, "SHEET / HOJA", "hoja", "DATE / FECHA", "fecha", 2.5);
    dos(10, "SCALE / ESCALA", "escala", "REV.", "revision", 2.5);
    dos(10, "DRAWN / ELABORÓ", "elaboro", "CHECKED / REVISÓ", "reviso", 2.5);
    c(18, "SHEET TITLE / CONTENIDO", "contenido", 3.5);
    c(10, "DISCIPLINE / DISCIPLINA", "disciplina", 2.2);
    c(10, "PHASE / FASE", "fase", 2.2);
    c(40, "REVISIONS / REVISIONES", "revisiones", 2.2);
    c(14, "PROJECT / PROYECTO", "proyecto", 3.5);
    c(10, "ADDRESS / DIRECCIÓN", "direccion", 2.2);
    c(10, "OWNER / PROPIETARIO", "propietario", 2.5);
    c(84, "PROFESSIONAL SEAL / SELLO Y FIRMA (3\" x 3\")", "profesional", 2.2);
    c(22, "FIRM / EMPRESA", "empresa", 3.0);
    const double ciudad = 114.3;
    const double notas = altoMarco - y - ciudad;
    if (notas > 0) c(notas, "GENERAL NOTES / NOTAS", "notas", 2.2);
    c(ciudad, "RESERVED FOR CITY STAMP / RESERVADO SELLO DE LA CIUDAD", "sello", 2.2);
    return r;
}

// ---------------------------------------------------------------------------
// Puertas, ventanas y vanos dentro de un muro.
// Coordenadas locales del vano: u a lo largo del muro (0..ancho) y w a través (0 = la cara
// donde se hizo clic, espesor = la otra cara).
enum class TipoVano { Batiente = 0, Doble = 1, Corrediza = 2, Ventana = 3, Vano = 4 };

inline const char *nombreTipoVano(TipoVano t) {
    switch (t) {
    case TipoVano::Batiente: return "PUERTA BATIENTE 1 HOJA";
    case TipoVano::Doble: return "PUERTA BATIENTE 2 HOJAS";
    case TipoVano::Corrediza: return "PUERTA CORREDIZA";
    case TipoVano::Ventana: return "VENTANA";
    default: return "VANO (SIN PUERTA)";
    }
}

struct Vano {
    TipoVano tipo = TipoVano::Batiente;
    P o;              // inicio del vano sobre la cara donde se hizo clic
    P dir{1, 0};      // a lo largo del muro (unitario)
    P n{0, 1};        // hacia la otra cara (unitario)
    double ancho = 0.9, espesor = 0.15;
    int lado = 1;            // puertas: +1 abre hacia el lado de la otra cara, -1 hacia el lado del clic
    bool bisagraInicio = true;  // bisagra en u = 0 (si no, en u = ancho)
};

inline P local(const Vano &v, double uu, double ww) { return v.o + v.dir * uu + v.n * ww; }
inline double punto(P a, P b) { return a.x * b.x + a.y * b.y; }

struct Arco {
    P c;
    double r, a1, a2;  // radianes, sentido antihorario de a1 a a2
};

// Busca la otra cara del muro: la línea paralela más cercana (no la misma cara) que pasa por
// el tramo del vano, a menos de maxEspesor. Devuelve false si no hay.
inline bool buscarOtraCara(const std::vector<Seg> &segs, P o, P dir, double ancho, double maxEspesor,
                           double tol, double &w) {
    const P perp(-dir.y, dir.x);
    bool hay = false;
    double mejor = 0;
    for (const Seg &s : segs) {
        const P d = s.b - s.a;
        const double l = largo(d);
        if (l < tol) continue;
        if (std::fabs(d.x * dir.y - d.y * dir.x) / l > 1e-3) continue;  // no paralela
        const double wa = punto(s.a - o, perp);
        if (std::fabs(wa) < tol || std::fabs(wa) > maxEspesor) continue;
        double ua = punto(s.a - o, dir), ub = punto(s.b - o, dir);
        if (ua > ub) std::swap(ua, ub);
        if (ub < tol || ua > ancho - tol) continue;  // no pasa por el vano
        if (!hay || std::fabs(wa) < std::fabs(mejor)) { mejor = wa; hay = true; }
    }
    if (hay) w = mejor;
    return hay;
}

// Corta un segmento de cara de muro donde cae el vano. Devuelve los pedazos que quedan
// (el mismo segmento si no lo toca).
inline std::vector<Seg> cortarPorVano(const Seg &s, const Vano &v, double tol) {
    const double wa = punto(s.a - v.o, v.n), wb = punto(s.b - v.o, v.n);
    const double ua = punto(s.a - v.o, v.dir), ub = punto(s.b - v.o, v.dir);
    const bool paralela = std::fabs(wa - wb) < tol;
    const bool enMuro = wa > -tol && wa < v.espesor + tol;
    const double u1 = std::min(ua, ub), u2 = std::max(ua, ub);
    if (!paralela || !enMuro || u2 <= tol || u1 >= v.ancho - tol) return {s};
    std::vector<Seg> r;
    if (u1 < -tol) r.push_back({local(v, u1, wa), local(v, 0, wa)});
    if (u2 > v.ancho + tol) r.push_back({local(v, v.ancho, wa), local(v, u2, wa)});
    return r;
}

struct SimboloVano {
    std::vector<Seg> lineas;
    std::vector<Arco> arcos;
    P marca;  // donde va la marca P-1 / V-1
};

inline double angulo(P d) { return std::atan2(d.y, d.x); }

// Arco de 90° de la hoja: de la hoja abierta a la posición cerrada, por el lado corto.
inline Arco arcoHoja(P centro, P abierta, P cerrada) {
    double a = angulo(abierta - centro), b = angulo(cerrada - centro);
    const double PI2 = 6.283185307179586;
    double barrido = std::fmod(b - a + 2 * PI2, PI2);
    if (barrido > PI2 / 2) std::swap(a, b);
    return {centro, largo(abierta - centro), a, b};
}

inline SimboloVano simboloVano(const Vano &v, double separacionMarca) {
    SimboloVano s;
    const double e = v.espesor, A = v.ancho;
    auto L = [&](double u1, double w1, double u2, double w2) { s.lineas.push_back({local(v, u1, w1), local(v, u2, w2)}); };
    // Jambas: cierran el muro a los dos lados del vano.
    L(0, 0, 0, e);
    L(A, 0, A, e);
    const double cara = v.lado < 0 ? 0 : e;  // cara por donde abre la puerta
    auto hoja = [&](double uBisagra, double uCierre) {
        const double wAbierta = cara + (v.lado < 0 ? -1 : 1) * std::fabs(uCierre - uBisagra);
        L(uBisagra, cara, uBisagra, wAbierta);
        s.arcos.push_back(arcoHoja(local(v, uBisagra, cara), local(v, uBisagra, wAbierta), local(v, uCierre, cara)));
    };
    switch (v.tipo) {
    case TipoVano::Batiente:
        if (v.bisagraInicio) hoja(0, A); else hoja(A, 0);
        break;
    case TipoVano::Doble:
        hoja(0, A / 2);
        hoja(A, A / 2);
        break;
    case TipoVano::Corrediza: {
        // Dos hojas que se cruzan dentro del espesor del muro.
        auto rect = [&](double u1, double u2, double w1, double w2) {
            L(u1, w1, u2, w1); L(u2, w1, u2, w2); L(u2, w2, u1, w2); L(u1, w2, u1, w1);
        };
        rect(0, A * 0.55, e * 0.30, e * 0.45);
        rect(A * 0.45, A, e * 0.55, e * 0.70);
        break;
    }
    case TipoVano::Ventana:
        L(0, 0, A, 0);
        L(0, e, A, e);
        L(0, e * 0.45, A, e * 0.45);  // vidrio (doble línea al centro)
        L(0, e * 0.55, A, e * 0.55);
        break;
    case TipoVano::Vano:
        break;
    }
    // La marca va al lado contrario de donde abre la puerta (en ventanas, del lado del clic).
    const bool afuera = v.tipo == TipoVano::Ventana || v.tipo == TipoVano::Vano || v.lado > 0;
    s.marca = local(v, A / 2, afuera ? -separacionMarca : e + separacionMarca);
    return s;
}

// ---------------------------------------------------------------------------
// Área de un espacio con un clic adentro (muros rectos, como «sombreado por punto interior»).
// Se usan las líneas horizontales y verticales cercanas; las de otros ángulos no cierran.
struct AreaEspacio {
    bool cerrado = false;
    double area = 0;
    std::vector<Rect> celdas;
};

inline AreaEspacio areaPorPunto(const std::vector<Seg> &segs, P punto, double radio, double tol) {
    AreaEspacio r;
    struct Lin { double fija, a, b; };
    std::vector<Lin> verticales, horizontales;
    std::vector<double> xs{punto.x - radio, punto.x + radio}, ys{punto.y - radio, punto.y + radio};
    for (const Seg &s : segs) {
        const double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
        if (std::fabs(dx) < tol && std::fabs(dy) >= tol) {
            const double x = (s.a.x + s.b.x) / 2;
            if (std::fabs(x - punto.x) > radio) continue;
            verticales.push_back({x, std::min(s.a.y, s.b.y), std::max(s.a.y, s.b.y)});
            xs.push_back(x); ys.push_back(s.a.y); ys.push_back(s.b.y);
        } else if (std::fabs(dy) < tol && std::fabs(dx) >= tol) {
            const double y = (s.a.y + s.b.y) / 2;
            if (std::fabs(y - punto.y) > radio) continue;
            horizontales.push_back({y, std::min(s.a.x, s.b.x), std::max(s.a.x, s.b.x)});
            ys.push_back(y); xs.push_back(s.a.x); xs.push_back(s.b.x);
        }
    }
    auto unicos = [tol, &punto, radio](std::vector<double> &v, double centro) {
        std::vector<double> w;
        for (double a : v) if (std::fabs(a - centro) <= radio + tol) w.push_back(a);
        std::sort(w.begin(), w.end());
        std::vector<double> u;
        for (double a : w) if (u.empty() || a - u.back() > tol) u.push_back(a);
        v.swap(u);
        (void)punto;
    };
    unicos(xs, punto.x);
    unicos(ys, punto.y);
    if (xs.size() < 2 || ys.size() < 2) return r;
    const size_t nx = xs.size() - 1, ny = ys.size() - 1;
    auto indice = [tol](const std::vector<double> &v, double a) -> long {
        for (size_t i = 0; i < v.size(); ++i) if (std::fabs(v[i] - a) <= tol) return static_cast<long>(i);
        return -1;
    };
    // Bloqueos: bordeV[i][j] = hay línea vertical en x = xs[i] que tapa la fila j.
    std::vector<char> bordeV((nx + 1) * ny, 0), bordeH(nx * (ny + 1), 0);
    for (const Lin &l : verticales) {
        const long i = indice(xs, l.fija);
        if (i < 0) continue;
        for (size_t j = 0; j < ny; ++j) {
            const double m = (ys[j] + ys[j + 1]) / 2;
            if (m > l.a && m < l.b) bordeV[static_cast<size_t>(i) * ny + j] = 1;
        }
    }
    for (const Lin &l : horizontales) {
        const long j = indice(ys, l.fija);
        if (j < 0) continue;
        for (size_t i = 0; i < nx; ++i) {
            const double m = (xs[i] + xs[i + 1]) / 2;
            if (m > l.a && m < l.b) bordeH[i * (ny + 1) + static_cast<size_t>(j)] = 1;
        }
    }
    size_t ci = 0, cj = 0;
    while (ci + 1 < nx && xs[ci + 1] <= punto.x) ++ci;
    while (cj + 1 < ny && ys[cj + 1] <= punto.y) ++cj;
    std::vector<char> visto(nx * ny, 0);
    std::vector<std::pair<size_t, size_t>> pila{{ci, cj}};
    visto[ci * ny + cj] = 1;
    bool fuga = false;
    while (!pila.empty()) {
        const auto [i, j] = pila.back();
        pila.pop_back();
        r.celdas.push_back(rect(xs[i], ys[j], xs[i + 1], ys[j + 1]));
        r.area += (xs[i + 1] - xs[i]) * (ys[j + 1] - ys[j]);
        auto ir = [&](long a, long b, bool bloqueado) {
            if (bloqueado) return;
            if (a < 0 || b < 0 || a >= static_cast<long>(nx) || b >= static_cast<long>(ny)) { fuga = true; return; }
            const size_t k = static_cast<size_t>(a) * ny + static_cast<size_t>(b);
            if (visto[k]) return;
            visto[k] = 1;
            pila.push_back({static_cast<size_t>(a), static_cast<size_t>(b)});
        };
        ir(static_cast<long>(i) - 1, static_cast<long>(j), bordeV[i * ny + j] != 0);
        ir(static_cast<long>(i) + 1, static_cast<long>(j), bordeV[(i + 1) * ny + j] != 0);
        ir(static_cast<long>(i), static_cast<long>(j) - 1, bordeH[i * (ny + 1) + j] != 0);
        ir(static_cast<long>(i), static_cast<long>(j) + 1, bordeH[i * (ny + 1) + j + 1] != 0);
        if (fuga) break;
    }
    r.cerrado = !fuga;
    if (fuga) { r.area = 0; r.celdas.clear(); }
    return r;
}

// Área de un polígono (esquinas marcadas a mano), fórmula del zapato.
inline double areaPoligono(const std::vector<P> &p) {
    double a = 0;
    for (size_t i = 0; i < p.size(); ++i) {
        const P &s = p[i], &t = p[(i + 1) % p.size()];
        a += s.x * t.y - t.x * s.y;
    }
    return std::fabs(a) / 2;
}

// ---------------------------------------------------------------------------
// Datos guardados dentro del dibujo (texto en la capa A-DATOS, que no se imprime) para
// armar los cuadros de áreas y de puertas y ventanas:
//   DUCTUS-VANO|marca|tipo|ancho|alto|antepecho|aprobación
//   DUCTUS-ESPACIO|nombre|piso|área|clase   (clase: espacio, construida, lote)
inline std::vector<std::string> partir(const std::string &s, char sep) {
    std::vector<std::string> r;
    std::string actual;
    for (char c : s) {
        if (c == sep) { r.push_back(actual); actual.clear(); }
        else actual += c;
    }
    r.push_back(actual);
    return r;
}

struct DatoVano {
    std::string marca, tipo, aprobacion;
    double ancho = 0, alto = 0, antepecho = 0;
};

inline bool leerDatoVano(const std::string &t, DatoVano &d) {
    const auto p = partir(t, '|');
    if (p.size() < 7 || p[0] != "DUCTUS-VANO") return false;
    d.marca = p[1];
    d.tipo = p[2];
    d.ancho = std::atof(p[3].c_str());
    d.alto = std::atof(p[4].c_str());
    d.antepecho = std::atof(p[5].c_str());
    d.aprobacion = p[6];
    return !d.marca.empty();
}

struct DatoEspacio {
    std::string nombre, clase;
    int piso = 1;
    double area = 0;
};

inline bool leerDatoEspacio(const std::string &t, DatoEspacio &d) {
    const auto p = partir(t, '|');
    if (p.size() < 5 || p[0] != "DUCTUS-ESPACIO") return false;
    d.nombre = p[1];
    d.piso = std::atoi(p[2].c_str());
    d.area = std::atof(p[3].c_str());
    d.clase = p[4];
    return true;
}

// El dato de un vano lleva al final (campo 8) sus 4 esquinas "x,y,x,y,x,y,x,y": con ellas el
// área de un espacio se cierra por las dos caras del muro en las puertas.
inline std::string esquinasVano(const Vano &v) {
    char buf[256];
    const P a = local(v, 0, 0), b = local(v, v.ancho, 0), c = local(v, v.ancho, v.espesor), d = local(v, 0, v.espesor);
    std::snprintf(buf, sizeof buf, "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f", a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    return buf;
}

inline bool cierreVano(const std::string &t, std::vector<Seg> &segs) {
    const auto p = partir(t, '|');
    if (p.size() < 8 || p[0] != "DUCTUS-VANO") return false;
    const auto n = partir(p[7], ',');
    if (n.size() != 8) return false;
    double v[8];
    for (int i = 0; i < 8; ++i) if (!leerNumero(n[static_cast<size_t>(i)], v[i])) return false;
    segs.push_back({P(v[0], v[1]), P(v[2], v[3])});
    segs.push_back({P(v[6], v[7]), P(v[4], v[5])});
    return true;
}

// Muro para el render 3D: su polígono en planta.
//   DUCTUS-MURO|x,y;x,y;x,y;...
inline std::string datoMuro(const std::vector<P> &poligono) {
    std::string s = "DUCTUS-MURO|";
    char buf[64];
    for (size_t i = 0; i < poligono.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%s%.6f,%.6f", i ? ";" : "", poligono[i].x, poligono[i].y);
        s += buf;
    }
    return s;
}

inline bool leerDatoMuro(const std::string &t, std::vector<P> &poligono) {
    poligono.clear();
    const auto p = partir(t, '|');
    if (p.size() < 2 || p[0] != "DUCTUS-MURO") return false;
    for (const std::string &par : partir(p[1], ';')) {
        const auto xy = partir(par, ',');
        double x, y;
        if (xy.size() != 2 || !leerNumero(xy[0], x) || !leerNumero(xy[1], y)) return false;
        poligono.emplace_back(x, y);
    }
    return poligono.size() >= 3;
}

inline std::vector<P> esquinas(const Rect &r) {
    return {P(r.x1, r.y1), P(r.x2, r.y1), P(r.x2, r.y2), P(r.x1, r.y2)};
}

// Fila del cuadro de puertas y ventanas: una por marca, con la cantidad.
struct FilaVano {
    DatoVano dato;
    int cantidad = 0;
};

inline std::vector<FilaVano> agruparVanos(const std::vector<DatoVano> &vanos) {
    std::vector<FilaVano> f;
    for (const DatoVano &d : vanos) {
        bool hecho = false;
        for (FilaVano &x : f)
            if (x.dato.marca == d.marca) { ++x.cantidad; hecho = true; break; }
        if (!hecho) f.push_back({d, 1});
    }
    // Puertas primero (P-1, P-2 ... P-10) y luego ventanas, en orden de número.
    auto clave = [](const std::string &m) {
        const size_t g = m.find_first_of("0123456789");
        const std::string letra = g == std::string::npos ? m : m.substr(0, g);
        const int numero = g == std::string::npos ? 0 : std::atoi(m.c_str() + g);
        return std::make_pair(letra, numero);
    };
    std::sort(f.begin(), f.end(), [&](const FilaVano &a, const FilaVano &b) { return clave(a.dato.marca) < clave(b.dato.marca); });
    return f;
}

// Siguiente marca libre: «P-» con {"P-1","P-3"} -> «P-4».
inline std::string siguienteMarca(const std::string &prefijo, const std::vector<std::string> &usadas) {
    int mayor = 0;
    for (const std::string &m : usadas)
        if (m.compare(0, prefijo.size(), prefijo) == 0) mayor = std::max(mayor, std::atoi(m.c_str() + prefijo.size()));
    return prefijo + std::to_string(mayor + 1);
}

// Cuadro de áreas: totales por piso e índices.
struct ResumenAreas {
    std::vector<int> pisos;
    std::vector<double> utilPorPiso, construidaPorPiso;
    double lote = 0, construidaTotal = 0;
    double indiceOcupacion = 0, indiceConstruccion = 0;  // 0 si no hay lote
};

inline ResumenAreas resumirAreas(const std::vector<DatoEspacio> &es) {
    ResumenAreas r;
    for (const DatoEspacio &e : es)
        if (e.clase != "lote" && std::find(r.pisos.begin(), r.pisos.end(), e.piso) == r.pisos.end()) r.pisos.push_back(e.piso);
    std::sort(r.pisos.begin(), r.pisos.end());
    r.utilPorPiso.assign(r.pisos.size(), 0);
    r.construidaPorPiso.assign(r.pisos.size(), 0);
    for (const DatoEspacio &e : es) {
        if (e.clase == "lote") { r.lote += e.area; continue; }
        const size_t k = static_cast<size_t>(std::find(r.pisos.begin(), r.pisos.end(), e.piso) - r.pisos.begin());
        if (e.clase == "construida") r.construidaPorPiso[k] += e.area;
        else r.utilPorPiso[k] += e.area;
    }
    for (size_t k = 0; k < r.pisos.size(); ++k) {
        // Si no se marcó el área construida del piso, se usa la suma de los espacios.
        if (r.construidaPorPiso[k] <= 0) r.construidaPorPiso[k] = r.utilPorPiso[k];
        r.construidaTotal += r.construidaPorPiso[k];
    }
    if (r.lote > 0 && !r.pisos.empty()) {
        r.indiceOcupacion = r.construidaPorPiso.front() / r.lote;
        r.indiceConstruccion = r.construidaTotal / r.lote;
    }
    return r;
}

} // namespace arq

#endif // DUCTUSARQ_ARQ_H
