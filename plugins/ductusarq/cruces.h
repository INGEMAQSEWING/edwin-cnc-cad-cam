#pragma once
// Revisión de cruces entre el plano estructural superpuesto y el arquitectónico (sin Qt; se
// prueba en pruebas_arq.cpp).
//
// Columnas del estructural: polígonos cerrados casi rectangulares (o círculos) de tamaño de
// columna. Si alguna está en una capa que se llame COLUMNA / COL / PILAR, se usan solo esas.
// Se revisa cada columna contra los muros, las puertas y ventanas y los espacios del
// arquitectónico que dibujó DuctusArq.

#include "ventas.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace arq {

struct Poli {
    std::vector<P> pts;
    std::string capa;
};

struct Columna {
    Rect r;
    std::string capa;
};

struct VanoArq {
    std::string marca;
    Rect r;
};

struct Cruce {
    int nivel;  // 2 = problema, 1 = revisar, 0 = bien
    P donde;
    std::string texto;
};

inline Rect cajaPuntos(const std::vector<P> &v) {
    Rect r{v.front().x, v.front().y, v.front().x, v.front().y};
    for (const P &p : v) {
        r.x1 = std::min(r.x1, p.x); r.y1 = std::min(r.y1, p.y);
        r.x2 = std::max(r.x2, p.x); r.y2 = std::max(r.y2, p.y);
    }
    return r;
}

inline bool capaDeColumnas(const std::string &capa) {
    const std::string n = mayusculasAscii(capa);
    for (const char *x : {"COLUMN", "PILAR", "COL-", "-COL", "_COL", "COL_", "S-COL"})
        if (n.find(x) != std::string::npos) return true;
    // Capa que es exactamente COL (con el prefijo del plano superpuesto, p. ej. EST-COL)
    return n.size() >= 3 && n.compare(n.size() - 3, 3, "COL") == 0;
}

inline Rect crecer(const Rect &r, double d) { return {r.x1 - d, r.y1 - d, r.x2 + d, r.y2 + d}; }
inline P centroRect(const Rect &r) { return P((r.x1 + r.x2) / 2, (r.y1 + r.y2) / 2); }

// minLado / maxLado: tamaño de columna en unidades del dibujo (p. ej. 0.15 y 1.50 m).
inline std::vector<Columna> detectarColumnas(const std::vector<Poli> &polis,
                                             const std::vector<std::pair<P, double>> &circulos, double minLado,
                                             double maxLado) {
    std::vector<Columna> c;
    const double tol = minLado * 0.02;
    for (const Poli &p : polis) {
        if (p.pts.size() < 4) continue;
        const P a = p.pts.front(), b = p.pts.back();
        if (std::fabs(a.x - b.x) > tol || std::fabs(a.y - b.y) > tol) continue;  // no es cerrado
        const Rect r = cajaPuntos(p.pts);
        const double w = r.x2 - r.x1, h = r.y2 - r.y1;
        if (w < minLado || h < minLado || w > maxLado || h > maxLado) continue;
        if (areaPoligono(p.pts) < 0.75 * w * h) continue;  // no es casi un rectángulo
        c.push_back({r, p.capa});
    }
    for (const auto &k : circulos) {
        const double d = 2 * k.second;
        if (d < minLado || d > maxLado) continue;
        c.push_back({{k.first.x - k.second, k.first.y - k.second, k.first.x + k.second, k.first.y + k.second}, ""});
    }
    // Si hay capa de columnas, solo esas.
    if (std::any_of(c.begin(), c.end(), [](const Columna &x) { return capaDeColumnas(x.capa); }))
        c.erase(std::remove_if(c.begin(), c.end(), [](const Columna &x) { return !capaDeColumnas(x.capa); }), c.end());
    // Sin repetidas (la misma columna dibujada dos veces: contorno y relleno).
    std::vector<Columna> u;
    for (const Columna &x : c) {
        bool rep = false;
        for (const Columna &y : u)
            if (std::fabs(x.r.x1 - y.r.x1) < tol * 5 && std::fabs(x.r.y1 - y.r.y1) < tol * 5
                && std::fabs(x.r.x2 - y.r.x2) < tol * 5 && std::fabs(x.r.y2 - y.r.y2) < tol * 5)
                rep = true;
        if (!rep) u.push_back(x);
    }
    // Numeradas de arriba hacia abajo y de izquierda a derecha (como se leen los ejes).
    std::sort(u.begin(), u.end(), [&](const Columna &x, const Columna &y) {
        const P a = centroRect(x.r), b = centroRect(y.r);
        if (std::fabs(a.y - b.y) > maxLado / 2) return a.y > b.y;
        return a.x < b.x;
    });
    return u;
}

// tol: holgura para decir que una columna «toca» un muro (p. ej. 0.02 m).
inline std::vector<Cruce> revisarCruces(const std::vector<Columna> &cols, const std::vector<std::vector<P>> &muros,
                                        const std::vector<VanoArq> &vanos, const std::vector<EspacioVentas> &esp,
                                        double tol) {
    std::vector<Cruce> r;
    int problemas = 0, revisar = 0;
    for (size_t i = 0; i < cols.size(); ++i) {
        const Rect &c = cols[i].r;
        const P m = centroRect(c);
        const std::string n = "Columna " + std::to_string(i + 1);
        const double areaC = (c.x2 - c.x1) * (c.y2 - c.y1);
        bool enVano = false;
        for (const VanoArq &v : vanos)
            if (solape(crecer(c, -tol), v.r) > 0) {
                r.push_back({2, m, n + ": cae en la " + (v.marca.rfind("V", 0) == 0 ? "ventana " : "puerta o vano ")
                                       + v.marca + "."});
                ++problemas;
                enVano = true;
                break;
            }
        if (enVano) continue;
        bool tocaMuro = false;
        for (const auto &w : muros)
            if (w.size() >= 3 && solape(crecer(c, tol), cajaPuntos(w)) > 0) { tocaMuro = true; break; }
        const EspacioVentas *dentro = nullptr;
        double metido = 0;
        for (const EspacioVentas &e : esp) {
            double s = 0;
            for (const Rect &k : e.celdas) s += solape(c, k);
            if (s > metido) { metido = s; dentro = &e; }
        }
        if (!tocaMuro) {
            if (dentro && dentroDe(dentro->celdas, m)) {
                r.push_back({2, m, n + ": queda suelta dentro de «" + dentro->nombre + "», lejos de los muros."});
                ++problemas;
            } else {
                r.push_back({1, m, n + ": no coincide con ningún muro del arquitectónico."});
                ++revisar;
            }
        } else if (dentro && metido > 0.4 * areaC) {
            r.push_back({1, m, n + ": se sale del muro y se mete en «" + dentro->nombre + "»."});
            ++revisar;
        }
    }
    if (cols.empty())
        r.insert(r.begin(), {1, P(), "No encontré columnas en el estructural (rectángulos cerrados o círculos del "
                                     "tamaño de una columna)."});
    else if (problemas == 0 && revisar == 0)
        r.insert(r.begin(), {0, P(), "Las " + std::to_string(cols.size()) + " columnas caen dentro de los muros y "
                                     "ninguna se cruza con puertas ni ventanas."});
    return r;
}

} // namespace arq
