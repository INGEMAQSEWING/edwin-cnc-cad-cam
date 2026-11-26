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

} // namespace arq

#endif // DUCTUSARQ_ARQ_H
