#pragma once
// Fachadas y cortes a partir de la planta de DuctusArq (sin Qt; se prueba en pruebas_arq.cpp).
// Usan los muros guardados (DUCTUS-MURO), las puertas y ventanas con su alto y antepecho
// (DUCTUS-VANO) y los espacios con su nombre. Coordenadas del alzado: x a lo ancho de la vista,
// y hacia arriba con 0 = nivel del piso terminado.

#include "cantidades.h"  // VanoCantidad

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace arq {

struct Alzado {
    std::string titulo;
    std::vector<Seg> contorno;                 // muros y losa (línea gruesa)
    std::vector<std::vector<P>> cortado;       // lo que corta el corte: muros y losas (relleno)
    std::vector<Seg> puertas, ventanas, vidrio;
    std::vector<Seg> terreno;                  // línea de terreno / piso
    struct Nivel { P p; std::string texto; };
    std::vector<Nivel> niveles;                // marca de nivel (triángulo + texto)
    std::vector<std::pair<P, std::string>> textos;  // nombres de espacios en el corte
    double ancho = 0, alto = 0;                // tamaño (sin márgenes)
};

enum class Lado { Frente = 0, Fondo = 1, Izquierda = 2, Derecha = 3 };

inline const char *nombreLado(Lado l, bool ingles) {
    switch (l) {
    case Lado::Frente: return ingles ? "FRONT ELEVATION / FACHADA PRINCIPAL" : "FACHADA PRINCIPAL";
    case Lado::Fondo: return ingles ? "REAR ELEVATION / FACHADA POSTERIOR" : "FACHADA POSTERIOR";
    case Lado::Izquierda: return ingles ? "LEFT ELEVATION / FACHADA LATERAL IZQUIERDA" : "FACHADA LATERAL IZQUIERDA";
    default: return ingles ? "RIGHT ELEVATION / FACHADA LATERAL DERECHA" : "FACHADA LATERAL DERECHA";
    }
}

inline Rect cajaDe(const std::vector<std::vector<P>> &polis) {
    Rect r{1e300, 1e300, -1e300, -1e300};
    for (const auto &w : polis)
        for (const P &p : w) {
            r.x1 = std::min(r.x1, p.x); r.y1 = std::min(r.y1, p.y);
            r.x2 = std::max(r.x2, p.x); r.y2 = std::max(r.y2, p.y);
        }
    return r;
}

inline Rect cajaVano(const Vano &v) {
    const P q[4] = {local(v, 0, 0), local(v, v.ancho, 0), local(v, v.ancho, v.espesor), local(v, 0, v.espesor)};
    Rect r{q[0].x, q[0].y, q[0].x, q[0].y};
    for (const P &p : q) {
        r.x1 = std::min(r.x1, p.x); r.y1 = std::min(r.y1, p.y);
        r.x2 = std::max(r.x2, p.x); r.y2 = std::max(r.y2, p.y);
    }
    return r;
}

inline void rectAlzado(std::vector<Seg> &s, double x1, double y1, double x2, double y2) {
    s.push_back({P(x1, y1), P(x2, y1)});
    s.push_back({P(x2, y1), P(x2, y2)});
    s.push_back({P(x2, y2), P(x1, y2)});
    s.push_back({P(x1, y2), P(x1, y1)});
}

// Dibujo de una puerta o ventana en el alzado, entre u1 y u2.
inline void vanoEnAlzado(Alzado &a, const VanoCantidad &v, double u1, double u2, double altoMuro, double metro) {
    const double marco = 0.05 * metro;
    if (v.tipo == "VENTANA") {
        const double y1 = v.antepecho, y2 = std::min(v.antepecho + v.alto, altoMuro);
        rectAlzado(a.ventanas, u1, y1, u2, y2);
        rectAlzado(a.vidrio, u1 + marco, y1 + marco, u2 - marco, y2 - marco);
        if (u2 - u1 > 1.0 * metro) a.vidrio.push_back({P((u1 + u2) / 2, y1 + marco), P((u1 + u2) / 2, y2 - marco)});
        a.ventanas.push_back({P(u1 - marco, y1), P(u2 + marco, y1)});  // alféizar
    } else {
        const double y2 = std::min(v.alto > 0 ? v.alto : 2.10 * metro, altoMuro);
        rectAlzado(a.puertas, u1, 0, u2, y2);
        if (v.tipo == "VANO") return;
        rectAlzado(a.puertas, u1 + marco, 0, u2 - marco, y2 - marco);
        // Manija
        const double xm = u2 - marco - 0.10 * metro;
        a.puertas.push_back({P(xm, 0.95 * metro), P(xm, 1.10 * metro)});
        if (v.tipo == "DOBLE" || v.tipo == "CORREDIZA")
            a.puertas.push_back({P((u1 + u2) / 2, 0), P((u1 + u2) / 2, y2 - marco)});
    }
}

// losa: espesor de la losa o cubierta sobre los muros.
inline Alzado fachada(const std::vector<std::vector<P>> &muros, const std::vector<VanoCantidad> &vanos, Lado lado,
                      double altoMuro, double losa, double metro, bool ingles = false) {
    Alzado a;
    a.titulo = nombreLado(lado, ingles);
    const Rect B = cajaDe(muros);
    const bool horizontal = lado == Lado::Frente || lado == Lado::Fondo;
    a.ancho = horizontal ? B.x2 - B.x1 : B.y2 - B.y1;
    a.alto = altoMuro + losa;
    auto U = [&](const P &p) {
        switch (lado) {
        case Lado::Frente: return p.x - B.x1;
        case Lado::Fondo: return B.x2 - p.x;
        case Lado::Izquierda: return B.y2 - p.y;
        default: return p.y - B.y1;
        }
    };
    rectAlzado(a.contorno, 0, 0, a.ancho, altoMuro);
    rectAlzado(a.contorno, -0.05 * metro, altoMuro, a.ancho + 0.05 * metro, altoMuro + losa);  // losa con alero corto
    a.terreno.push_back({P(-0.6 * metro, 0), P(a.ancho + 0.6 * metro, 0)});
    const double tol = 0.03 * metro;
    for (const VanoCantidad &v : vanos) {
        const Rect r = cajaVano(v.v);
        bool aqui = false;
        switch (lado) {
        case Lado::Frente: aqui = std::fabs(r.y1 - B.y1) < tol; break;
        case Lado::Fondo: aqui = std::fabs(r.y2 - B.y2) < tol; break;
        case Lado::Izquierda: aqui = std::fabs(r.x1 - B.x1) < tol; break;
        default: aqui = std::fabs(r.x2 - B.x2) < tol; break;
        }
        if (!aqui) continue;
        const double ua = U(P(r.x1, r.y1)), ub = U(P(r.x2, r.y2));
        vanoEnAlzado(a, v, std::min(ua, ub), std::max(ua, ub), altoMuro, metro);
    }
    a.niveles.push_back({P(a.ancho + 0.4 * metro, 0), "N +0.00"});
    a.niveles.push_back({P(a.ancho + 0.4 * metro, altoMuro), "N +" + textoLongitud(altoMuro, metro > 1.5 ? Sistema::Imperial : Sistema::Metrico)});
    a.niveles.push_back({P(a.ancho + 0.4 * metro, altoMuro + losa),
                         "N +" + textoLongitud(altoMuro + losa, metro > 1.5 ? Sistema::Imperial : Sistema::Metrico)});
    return a;
}

// Corte por la línea p1-p2 (horizontal o vertical según su dirección mayor). Mira hacia +y
// (corte a lo ancho) o hacia +x (corte a lo largo).
inline Alzado corte(const std::vector<std::vector<P>> &muros, const std::vector<VanoCantidad> &vanos,
                    const std::vector<EspacioVentas> &esp, P p1, P p2, const std::string &letra, double altoMuro,
                    double losa, double metro) {
    Alzado a;
    a.titulo = "CORTE " + letra + "-" + letra + "'";
    const Rect B = cajaDe(muros);
    const bool aLoAncho = std::fabs(p2.x - p1.x) >= std::fabs(p2.y - p1.y);
    const double c = aLoAncho ? (p1.y + p2.y) / 2 : (p1.x + p2.x) / 2;  // posición del corte
    a.ancho = aLoAncho ? B.x2 - B.x1 : B.y2 - B.y1;
    a.alto = altoMuro + losa;
    auto U = [&](double s) { return aLoAncho ? s - B.x1 : B.y2 - s; };
    auto caja = [](double x1, double y1, double x2, double y2) {
        return std::vector<P>{P(x1, y1), P(x2, y1), P(x2, y2), P(x1, y2)};
    };
    // Losa de cubierta y contrapiso, cortados.
    a.cortado.push_back(caja(0, altoMuro, a.ancho, altoMuro + losa));
    a.cortado.push_back(caja(-0.3 * metro, -0.10 * metro, a.ancho + 0.3 * metro, 0));
    a.terreno.push_back({P(-0.6 * metro, -0.10 * metro), P(a.ancho + 0.6 * metro, -0.10 * metro)});
    // Muros que cruza la línea.
    for (const auto &w : muros) {
        if (w.size() < 3) continue;
        Rect r{w[0].x, w[0].y, w[0].x, w[0].y};
        for (const P &p : w) {
            r.x1 = std::min(r.x1, p.x); r.y1 = std::min(r.y1, p.y);
            r.x2 = std::max(r.x2, p.x); r.y2 = std::max(r.y2, p.y);
        }
        const double lo = aLoAncho ? r.y1 : r.x1, hi = aLoAncho ? r.y2 : r.x2;
        if (c < lo || c > hi) continue;
        double u1 = U(aLoAncho ? r.x1 : r.y1), u2 = U(aLoAncho ? r.x2 : r.y2);
        if (u1 > u2) std::swap(u1, u2);
        // Vanos del muro que también cruza la línea: el muro se corta por partes.
        std::vector<std::pair<double, double>> huecos;
        std::vector<const VanoCantidad *> deHueco;
        for (const VanoCantidad &v : vanos) {
            const Rect q = cajaVano(v.v);
            const double qlo = aLoAncho ? q.y1 : q.x1, qhi = aLoAncho ? q.y2 : q.x2;
            if (c < qlo || c > qhi) continue;
            double h1 = U(aLoAncho ? q.x1 : q.y1), h2 = U(aLoAncho ? q.x2 : q.y2);
            if (h1 > h2) std::swap(h1, h2);
            if (h2 <= u1 || h1 >= u2) continue;
            huecos.push_back({std::max(h1, u1), std::min(h2, u2)});
            deHueco.push_back(&v);
        }
        // Tramos llenos entre huecos
        std::vector<double> cortes{u1};
        std::vector<size_t> orden(huecos.size());
        for (size_t i = 0; i < orden.size(); ++i) orden[i] = i;
        std::sort(orden.begin(), orden.end(), [&](size_t x, size_t y) { return huecos[x].first < huecos[y].first; });
        double desde = u1;
        for (size_t i : orden) {
            if (huecos[i].first > desde) a.cortado.push_back(caja(desde, 0, huecos[i].first, altoMuro));
            const VanoCantidad &v = *deHueco[i];
            const double h1 = huecos[i].first, h2 = huecos[i].second;
            if (v.tipo == "VENTANA") {
                const double y1 = v.antepecho, y2 = std::min(v.antepecho + v.alto, altoMuro);
                if (y1 > 0) a.cortado.push_back(caja(h1, 0, h2, y1));
                if (y2 < altoMuro) a.cortado.push_back(caja(h1, y2, h2, altoMuro));
                a.vidrio.push_back({P((h1 + h2) / 2, y1), P((h1 + h2) / 2, y2)});
            } else {
                const double y2 = std::min(v.alto > 0 ? v.alto : 2.10 * metro, altoMuro);
                if (y2 < altoMuro) a.cortado.push_back(caja(h1, y2, h2, altoMuro));
            }
            desde = std::max(desde, h2);
        }
        if (desde < u2) a.cortado.push_back(caja(desde, 0, u2, altoMuro));
    }
    // Nombres de los espacios que cruza la línea.
    for (const EspacioVentas &e : esp) {
        double lo = 1e300, hi = -1e300;
        for (const Rect &k : e.celdas) {
            const double klo = aLoAncho ? k.y1 : k.x1, khi = aLoAncho ? k.y2 : k.x2;
            if (c < klo || c > khi) continue;
            lo = std::min(lo, U(aLoAncho ? k.x1 : k.y1));
            hi = std::max(hi, U(aLoAncho ? k.x1 : k.y1));
            lo = std::min(lo, U(aLoAncho ? k.x2 : k.y2));
            hi = std::max(hi, U(aLoAncho ? k.x2 : k.y2));
        }
        if (hi > lo) a.textos.push_back({P((lo + hi) / 2, altoMuro * 0.45), e.nombre});
    }
    const Sistema s = metro > 1.5 ? Sistema::Imperial : Sistema::Metrico;
    a.niveles.push_back({P(a.ancho + 0.4 * metro, 0), "N +0.00"});
    a.niveles.push_back({P(a.ancho + 0.4 * metro, altoMuro), "N +" + textoLongitud(altoMuro, s)});
    a.niveles.push_back({P(a.ancho + 0.4 * metro, altoMuro + losa), "N +" + textoLongitud(altoMuro + losa, s)});
    return a;
}

} // namespace arq
