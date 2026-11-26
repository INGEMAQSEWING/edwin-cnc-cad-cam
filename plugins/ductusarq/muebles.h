/*****************************************************************************/
/*  muebles.h - Catálogo de mobiliario, electrodomésticos y aparatos          */
/*              sanitarios de DuctusArq 2D (sin Qt, se prueba solo)           */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*****************************************************************************/
//
// Cada pieza se dibuja con su ancho (X) y fondo (Y) reales, en coordenadas locales: el
// centro en (0, 0) y el FRENTE hacia -Y (la espalda, cabecera o pared va hacia +Y). Cada pieza
// guarda también su ALTO, que servirá para el render 3D de la planta amoblada.
// Medidas: tallas usuales en Colombia (metros) y en EE. UU. (pulgadas).

#ifndef DUCTUSARQ_MUEBLES_H
#define DUCTUSARQ_MUEBLES_H

#include "arq.h"

#include <cmath>
#include <string>
#include <vector>

namespace arq {

enum class Grupo { Mobiliario = 0, Electrodomestico = 1, Sanitario = 2 };

inline const char *capaGrupo(Grupo g) {
    switch (g) {
    case Grupo::Electrodomestico: return "A-ELECTRODOMESTICOS";
    case Grupo::Sanitario: return "A-SANITARIOS";
    default: return "A-MOBILIARIO";
    }
}

struct Pieza {
    const char *id;
    const char *nombre;     // español
    const char *ingles;
    Grupo grupo;
    double anchoM, fondoM, altoM;      // metros (Colombia)
    double anchoIn, fondoIn, altoIn;   // pulgadas (EE. UU.)
};

inline const std::vector<Pieza> &catalogo() {
    static const std::vector<Pieza> c = {
        // --- Mobiliario
        {"cama_sencilla", "Cama sencilla", "Twin bed", Grupo::Mobiliario, 1.00, 1.90, 0.55, 39, 75, 22},
        {"cama_doble", "Cama doble", "Full bed", Grupo::Mobiliario, 1.40, 1.90, 0.55, 54, 75, 22},
        {"cama_queen", "Cama queen", "Queen bed", Grupo::Mobiliario, 1.60, 1.90, 0.55, 60, 80, 22},
        {"cama_king", "Cama king", "King bed", Grupo::Mobiliario, 2.00, 2.00, 0.55, 76, 80, 22},
        {"mesa_noche", "Mesa de noche", "Nightstand", Grupo::Mobiliario, 0.45, 0.40, 0.55, 18, 16, 24},
        {"closet", "Closet / armario", "Closet", Grupo::Mobiliario, 1.50, 0.60, 2.20, 60, 24, 84},
        {"comoda", "Cómoda", "Dresser", Grupo::Mobiliario, 1.20, 0.50, 0.85, 48, 20, 34},
        {"sofa3", "Sofá 3 puestos", "Sofa", Grupo::Mobiliario, 2.00, 0.85, 0.85, 84, 36, 34},
        {"sofa2", "Sofá 2 puestos", "Loveseat", Grupo::Mobiliario, 1.50, 0.85, 0.85, 60, 36, 34},
        {"poltrona", "Poltrona", "Armchair", Grupo::Mobiliario, 0.80, 0.80, 0.85, 32, 34, 34},
        {"mesa_centro", "Mesa de centro", "Coffee table", Grupo::Mobiliario, 1.00, 0.50, 0.40, 42, 22, 18},
        {"mueble_tv", "Mueble de TV", "TV stand", Grupo::Mobiliario, 1.60, 0.40, 0.55, 60, 18, 24},
        {"comedor4", "Comedor 4 puestos", "Dining table (4)", Grupo::Mobiliario, 1.20, 0.80, 0.75, 48, 32, 30},
        {"comedor6", "Comedor 6 puestos", "Dining table (6)", Grupo::Mobiliario, 1.80, 0.90, 0.75, 72, 36, 30},
        {"escritorio", "Escritorio con silla", "Desk", Grupo::Mobiliario, 1.20, 0.60, 0.75, 48, 24, 30},
        {"meson", "Mesón de cocina", "Kitchen counter", Grupo::Mobiliario, 2.40, 0.60, 0.90, 96, 25, 36},
        // --- Electrodomésticos
        {"nevera", "Nevera", "Refrigerator", Grupo::Electrodomestico, 0.70, 0.70, 1.75, 36, 30, 70},
        {"estufa", "Estufa 4 puestos", "Range", Grupo::Electrodomestico, 0.60, 0.60, 0.90, 30, 26, 36},
        {"lavaplatos", "Lavaplatos (poceta)", "Kitchen sink", Grupo::Electrodomestico, 0.80, 0.50, 0.90, 33, 22, 36},
        {"lavavajillas", "Lavavajillas", "Dishwasher", Grupo::Electrodomestico, 0.60, 0.60, 0.85, 24, 24, 34},
        {"microondas", "Horno microondas", "Microwave", Grupo::Electrodomestico, 0.50, 0.40, 0.30, 24, 16, 12},
        {"lavadora", "Lavadora", "Washer", Grupo::Electrodomestico, 0.60, 0.65, 0.90, 27, 30, 38},
        {"secadora", "Secadora", "Dryer", Grupo::Electrodomestico, 0.60, 0.65, 0.90, 27, 30, 38},
        {"calentador", "Calentador de agua", "Water heater", Grupo::Electrodomestico, 0.50, 0.50, 1.50, 22, 22, 60},
        {"aire", "Aire acondicionado (minisplit)", "A/C wall unit", Grupo::Electrodomestico, 0.90, 0.25, 0.30, 36, 10, 12},
        {"tv", "Televisor", "TV", Grupo::Electrodomestico, 1.25, 0.10, 0.75, 50, 4, 30},
        // --- Aparatos sanitarios
        {"sanitario", "Sanitario", "Toilet", Grupo::Sanitario, 0.40, 0.70, 0.75, 20, 28, 30},
        {"lavamanos", "Lavamanos", "Lavatory", Grupo::Sanitario, 0.50, 0.45, 0.85, 20, 18, 34},
        {"lavamanos_doble", "Mueble con 2 lavamanos", "Double vanity", Grupo::Sanitario, 1.50, 0.55, 0.85, 60, 22, 34},
        {"ducha", "Ducha", "Shower", Grupo::Sanitario, 0.90, 0.90, 0.05, 36, 36, 2},
        {"tina", "Tina / bañera", "Bathtub", Grupo::Sanitario, 1.70, 0.75, 0.55, 60, 30, 20},
        {"lavadero", "Lavadero", "Laundry sink", Grupo::Sanitario, 0.60, 0.60, 0.85, 24, 22, 34},
    };
    return c;
}

inline const Pieza *buscarPieza(const std::string &id) {
    for (const Pieza &p : catalogo()) if (id == p.id) return &p;
    return nullptr;
}

struct Dibujo {
    std::vector<Seg> lineas;
    std::vector<std::pair<P, double>> circulos;  // centro, radio
    std::vector<std::pair<P, std::string>> textos;
    double altoTexto = 0;
};

// ---------------------------------------------------------------------------
// Formas de apoyo (coordenadas locales con el centro en 0,0)
inline void caja(Dibujo &d, double x1, double y1, double x2, double y2) {
    d.lineas.push_back({P(x1, y1), P(x2, y1)});
    d.lineas.push_back({P(x2, y1), P(x2, y2)});
    d.lineas.push_back({P(x2, y2), P(x1, y2)});
    d.lineas.push_back({P(x1, y2), P(x1, y1)});
}

// Rectángulo con esquinas redondeadas (r) hecho con tramos rectos.
inline void rectRedondo(Dibujo &d, double x1, double y1, double x2, double y2, double r) {
    r = std::min(r, std::min(x2 - x1, y2 - y1) / 2);
    std::vector<P> pts;
    const double PI_ = 3.14159265358979323846;
    const P centros[4] = {P(x2 - r, y1 + r), P(x2 - r, y2 - r), P(x1 + r, y2 - r), P(x1 + r, y1 + r)};
    for (int k = 0; k < 4; ++k)
        for (int i = 0; i <= 4; ++i) {
            const double a = -PI_ / 2 + k * PI_ / 2 + i * (PI_ / 2) / 4;
            pts.push_back(centros[k] + P(std::cos(a), std::sin(a)) * r);
        }
    for (size_t i = 0; i < pts.size(); ++i) d.lineas.push_back({pts[i], pts[(i + 1) % pts.size()]});
}

inline void elipse(Dibujo &d, P c, double rx, double ry, int n = 24) {
    const double PI_ = 3.14159265358979323846;
    for (int i = 0; i < n; ++i) {
        const double a = 2 * PI_ * i / n, b = 2 * PI_ * (i + 1) / n;
        d.lineas.push_back({c + P(rx * std::cos(a), ry * std::sin(a)), c + P(rx * std::cos(b), ry * std::sin(b))});
    }
}

inline void silla(Dibujo &d, P c, double lado, double angulo) {
    // Silla vista desde arriba: asiento y espaldar del lado de "angulo" (hacia afuera de la mesa).
    const double h = lado / 2;
    const P ex(std::cos(angulo), std::sin(angulo)), ey(-std::sin(angulo), std::cos(angulo));
    auto q = [&](double a, double b) { return c + ex * a + ey * b; };
    d.lineas.push_back({q(-h, -h), q(h, -h)});
    d.lineas.push_back({q(h, -h), q(h, h)});
    d.lineas.push_back({q(h, h), q(-h, h)});
    d.lineas.push_back({q(-h, h), q(-h, -h)});
    d.lineas.push_back({q(h * 0.55, -h), q(h * 0.55, h)});  // espaldar
}

// ---------------------------------------------------------------------------
// Dibujo de cada pieza con su ancho (w) y fondo (f). u = una unidad de "detalle" pequeño
// (5 cm o 2"), para que las formas se vean igual en metros y en pulgadas.
inline Dibujo dibujarPieza(const Pieza &pz, double w, double f, double u, bool ingles) {
    Dibujo d;
    const double x1 = -w / 2, x2 = w / 2, y1 = -f / 2, y2 = f / 2;
    const std::string id = pz.id;
    d.altoTexto = std::min(w, f) * 0.18;
    auto etiqueta = [&](const char *es, const char *en) { d.textos.push_back({P(0, 0), ingles ? en : es}); };
    if (id.rfind("cama", 0) == 0) {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y2 - u), P(x2, y2 - u)});               // cabecera
        const int almohadas = id == "cama_sencilla" ? 1 : 2;
        const double aw = (w - 2 * u) / almohadas;   // ancho de cada almohada con su espacio
        const double alto = std::min(0.16 * f, 7 * u);
        for (int i = 0; i < almohadas; ++i) {
            const double ax = x1 + u + i * aw;
            rectRedondo(d, ax + 0.4 * u, y2 - 1.6 * u - alto, ax + aw - 0.4 * u, y2 - 1.6 * u, u);
        }
        // Sábana doblada
        const double ys = y2 - 0.30 * f;
        d.lineas.push_back({P(x1, ys), P(x2, ys)});
        d.lineas.push_back({P(x1, ys - 0.12 * f), P(x1 + 0.25 * w, ys)});
    } else if (id == "mesa_noche") {
        caja(d, x1, y1, x2, y2);
        d.circulos.push_back({P(0, 0), std::min(w, f) * 0.25});
    } else if (id == "closet") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1 + u), P(x2, y1 + u)});               // puertas
        d.lineas.push_back({P(x1 + u, (y1 + y2) / 2 + u), P(x2 - u, (y1 + y2) / 2 + u)});  // tubo
        for (double x = x1 + 2 * u; x < x2 - u; x += 2.5 * u)
            d.lineas.push_back({P(x - 0.8 * u, (y1 + y2) / 2 - 1.5 * u + u), P(x + 0.8 * u, (y1 + y2) / 2 + 3.5 * u)});
    } else if (id == "comoda") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1 + u), P(x2, y1 + u)});
        d.lineas.push_back({P(0, y1), P(0, y1 + u)});
    } else if (id.rfind("sofa", 0) == 0 || id == "poltrona") {
        rectRedondo(d, x1, y1, x2, y2, u);
        const double brazo = 3 * u, espaldar = 3.5 * u;
        d.lineas.push_back({P(x1 + brazo, y1), P(x1 + brazo, y2 - espaldar)});
        d.lineas.push_back({P(x2 - brazo, y1), P(x2 - brazo, y2 - espaldar)});
        d.lineas.push_back({P(x1, y2 - espaldar), P(x2, y2 - espaldar)});
        const int puestos = id == "sofa3" ? 3 : (id == "sofa2" ? 2 : 1);
        const double cw = (w - 2 * brazo) / puestos;
        for (int i = 1; i < puestos; ++i)
            d.lineas.push_back({P(x1 + brazo + i * cw, y1), P(x1 + brazo + i * cw, y2 - espaldar)});
    } else if (id == "mesa_centro") {
        rectRedondo(d, x1, y1, x2, y2, u * 0.6);
        rectRedondo(d, x1 + u, y1 + u, x2 - u, y2 - u, u * 0.4);
    } else if (id == "mueble_tv") {
        caja(d, x1, y1, x2, y2);
        caja(d, x1 + 0.2 * w, y2 - 1.2 * u, x2 - 0.2 * w, y2 - 0.4 * u);  // pantalla
    } else if (id.rfind("comedor", 0) == 0) {
        caja(d, x1, y1, x2, y2);
        const double s = std::min(f * 0.55, 9 * u);
        const int porLado = id == "comedor6" ? 2 : 1;
        const double PI_ = 3.14159265358979323846;
        for (int i = 0; i < porLado; ++i) {
            const double x = porLado == 1 ? 0 : (i == 0 ? -w / 4 : w / 4);
            silla(d, P(x, y1 - s * 0.35), s, -PI_ / 2);
            silla(d, P(x, y2 + s * 0.35), s, PI_ / 2);
        }
        silla(d, P(x1 - s * 0.35, 0), s, PI_);
        silla(d, P(x2 + s * 0.35, 0), s, 0);
    } else if (id == "escritorio") {
        caja(d, x1, y1, x2, y2);
        const double s = std::min(f * 0.75, 9 * u);
        silla(d, P(0, y1 - s * 0.3), s, -3.14159265358979323846 / 2);
    } else if (id == "meson") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1 + 0.6 * u), P(x2, y1 + 0.6 * u)});    // borde del mesón
    } else if (id == "nevera") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1 + u), P(x2, y1 + u)});
        etiqueta("NEV", "REF");
    } else if (id == "estufa") {
        caja(d, x1, y1, x2, y2);
        const double r = std::min(w, f) * 0.15;
        for (double sx : {-1.0, 1.0})
            for (double sy : {-1.0, 1.0})
                d.circulos.push_back({P(sx * w * 0.22, sy * f * 0.2 + u * 0.4), r});
        d.lineas.push_back({P(x1, y1 + u * 0.8), P(x2, y1 + u * 0.8)});
    } else if (id == "lavaplatos" || id == "lavadero") {
        caja(d, x1, y1, x2, y2);
        rectRedondo(d, x1 + u, y1 + u, x2 - u, y2 - 1.6 * u, u);
        d.circulos.push_back({P(0, (y1 + y2) / 2 - 0.2 * u), u * 0.5});
        d.circulos.push_back({P(0, y2 - 0.8 * u), u * 0.35});                // grifo
    } else if (id == "lavavajillas") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1 + u), P(x2, y1 + u)});
        etiqueta("LV", "DW");
    } else if (id == "microondas") {
        caja(d, x1, y1, x2, y2);
        caja(d, x1 + u * 0.6, y1 + u * 0.6, x2 - 3 * u, y2 - u * 0.6);
    } else if (id == "lavadora" || id == "secadora") {
        caja(d, x1, y1, x2, y2);
        d.circulos.push_back({P(0, -u * 0.3), std::min(w, f) * 0.32});
        d.lineas.push_back({P(x1, y2 - 1.5 * u), P(x2, y2 - 1.5 * u)});
        etiqueta(id == "lavadora" ? "L" : "S", id == "lavadora" ? "W" : "D");
    } else if (id == "calentador") {
        d.circulos.push_back({P(0, 0), std::min(w, f) / 2});
        etiqueta("CAL", "WH");
    } else if (id == "aire") {
        rectRedondo(d, x1, y1, x2, y2, u * 0.5);
        d.lineas.push_back({P(x1 + u, y1 + u * 0.8), P(x2 - u, y1 + u * 0.8)});
        etiqueta("A/A", "A/C");
    } else if (id == "tv") {
        caja(d, x1, y1, x2, y2);
        etiqueta("TV", "TV");
    } else if (id == "sanitario") {
        rectRedondo(d, x1, y2 - 0.25 * f, x2, y2, u * 0.4);                 // tanque
        elipse(d, P(0, y1 + 0.38 * f), w * 0.45, 0.38 * f);                  // taza
        elipse(d, P(0, y1 + 0.40 * f), w * 0.32, 0.28 * f);
    } else if (id == "lavamanos") {
        rectRedondo(d, x1, y1, x2, y2, u * 0.6);
        elipse(d, P(0, -0.08 * f), w * 0.35, f * 0.3);
        d.circulos.push_back({P(0, y2 - u * 0.7), u * 0.35});
    } else if (id == "lavamanos_doble") {
        caja(d, x1, y1, x2, y2);
        for (double sx : {-1.0, 1.0}) {
            elipse(d, P(sx * w / 4, -0.05 * f), std::min(w / 4 - u, 0.22 * w) * 0.8, f * 0.3);
            d.circulos.push_back({P(sx * w / 4, y2 - u * 0.7), u * 0.35});
        }
    } else if (id == "ducha") {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1), P(x2, y2)});
        d.lineas.push_back({P(x1, y2), P(x2, y1)});
        d.circulos.push_back({P(0, 0), u * 0.6});
    } else if (id == "tina") {
        caja(d, x1, y1, x2, y2);
        rectRedondo(d, x1 + u, y1 + u, x2 - u, y2 - u, std::min(w, f) * 0.3);
        d.circulos.push_back({P(x1 + 3 * u, 0), u * 0.5});
    } else {
        caja(d, x1, y1, x2, y2);
        d.lineas.push_back({P(x1, y1), P(x2, y2)});
    }
    return d;
}

// Pasa el dibujo local a la posición del plano: gira "grados" y lo lleva al punto "c".
inline Dibujo colocar(const Dibujo &d, P c, double grados) {
    const double a = grados * 3.14159265358979323846 / 180.0;
    const double ca = std::cos(a), sa = std::sin(a);
    auto t = [&](P p) { return P(c.x + p.x * ca - p.y * sa, c.y + p.x * sa + p.y * ca); };
    Dibujo r;
    r.altoTexto = d.altoTexto;
    for (const Seg &s : d.lineas) r.lineas.push_back({t(s.a), t(s.b)});
    for (const auto &k : d.circulos) r.circulos.push_back({t(k.first), k.second});
    for (const auto &k : d.textos) r.textos.push_back({t(k.first), k.second});
    return r;
}

// Dato de la pieza para el render 3D:
//   DUCTUS-MUEBLE|id|cx|cy|grados|ancho|fondo|alto
inline std::string datoMueble(const Pieza &pz, P c, double grados, double w, double f, double h) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "DUCTUS-MUEBLE|%s|%.6f|%.6f|%.3f|%.6f|%.6f|%.6f", pz.id, c.x, c.y, grados, w, f, h);
    return buf;
}

} // namespace arq

#endif // DUCTUSARQ_MUEBLES_H
