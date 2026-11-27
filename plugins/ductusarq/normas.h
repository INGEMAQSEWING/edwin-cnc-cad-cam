#pragma once
// Revisión de normas que se pueden medir en la planta (sin Qt; se prueba en pruebas_arq.cpp).
// Valores tomados de la investigación del 2026-11-26 (C:\EDWIN DEVELOPMENTS\DuctusArq\
// Investigacion normas): NSR-10 título K, NTC 6047/4143, Decreto 555 de Bogotá, Florida Building
// Code Residential, IRC 2021 y Ley de Florida 553.504. Son AVISOS de ayuda: no reemplazan la
// revisión del profesional ni la versión vigente de cada norma.

#include "cantidades.h"   // VanoCantidad
#include "circulacion.h"  // esPasillo

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace arq {

enum class Norma { Colombia = 0, Bogota = 1, Florida = 2, MiamiDade = 3 };

struct VanoNorma {
    VanoCantidad c;
    std::string aprobacion;  // NOA / FL# (Miami-Dade)
};

struct AvisoNorma {
    int nivel;  // 2 = no cumple, 1 = revisar, 0 = bien
    std::string texto;
    std::string fuente;
};

inline bool esAlcoba(const std::string &nombre) {
    // «MASTER BATH» o «CLOSET ALCOBA» no son alcobas.
    const TipoPiso t = pisoPorNombre(nombre);
    if (t == TipoPiso::Bano || t == TipoPiso::Closet) return false;
    const std::string n = mayusculasAscii(nombre);
    for (const char *x : {"ALCOBA", "HABITACI", "DORMITORIO", "BEDROOM", "MASTER", "SUITE", "CUARTO", "RECAMARA",
                          "REC\xC3\x81MARA"})
        if (n.find(x) != std::string::npos) return true;
    return false;
}

inline bool esHabitable(const std::string &nombre) {
    const TipoPiso t = pisoPorNombre(nombre);
    if (t == TipoPiso::Bano || t == TipoPiso::Closet || t == TipoPiso::Cocina || t == TipoPiso::Exterior
        || t == TipoPiso::Vacio)
        return false;
    if (esPasillo(nombre) || esEscalera(nombre)) return false;
    return true;
}

// Espacios a los que da un vano (-1 = afuera), uno por cada lado del muro.
inline std::pair<int, int> ladosVano(const std::vector<EspacioVentas> &esp, const Vano &v, double metro) {
    auto donde = [&](P p) {
        for (size_t i = 0; i < esp.size(); ++i)
            if (dentroDe(esp[i].celdas, p)) return static_cast<int>(i);
        return -1;
    };
    const double sale = v.espesor / 2 + 0.20 * metro;
    const P m = local(v, v.ancho / 2, v.espesor / 2);
    return {donde(m - v.n * sale), donde(m + v.n * sale)};
}

inline std::vector<AvisoNorma> revisarNormas(const std::vector<EspacioVentas> &esp, const std::vector<VanoNorma> &vanos,
                                             double altoMuro, double metro, Norma norma, bool terceraEdad) {
    std::vector<AvisoNorma> r;
    const bool eeuu = norma == Norma::Florida || norma == Norma::MiamiDade;
    const Sistema s = metro > 1.5 ? Sistema::Imperial : Sistema::Metrico;
    auto L = [&](double v) { return textoLongitud(v, s); };
    auto A = [&](double v) {
        char b[32];
        if (s == Sistema::Metrico) std::snprintf(b, sizeof b, "%.2f m\xC2\xB2", v);
        else std::snprintf(b, sizeof b, "%.0f ft\xC2\xB2", v / 144.0);
        return std::string(b);
    };
    int problemas = 0;
    auto mal = [&](int nivel, const std::string &t, const char *f) {
        r.push_back({nivel, t, f});
        if (nivel == 2) ++problemas;
    };

    // Lados de cada vano y ventanas por espacio.
    std::vector<std::vector<const VanoNorma *>> ventanas(esp.size());
    std::vector<std::pair<int, int>> lados;
    for (const VanoNorma &v : vanos) {
        lados.push_back(ladosVano(esp, v.c.v, metro));
        if (v.c.tipo == "VENTANA")
            for (int i : {lados.back().first, lados.back().second})
                if (i >= 0) ventanas[i].push_back(&v);
    }

    // --- Puertas
    for (size_t k = 0; k < vanos.size(); ++k) {
        const VanoNorma &v = vanos[k];
        if (v.c.tipo == "VENTANA" || v.c.tipo == "VANO") continue;
        const bool exterior = lados[k].first < 0 || lados[k].second < 0;
        bool aAlcoba = false, aBano = false;
        for (int i : {lados[k].first, lados[k].second})
            if (i >= 0) {
                if (esAlcoba(esp[i].nombre)) aAlcoba = true;
                if (pisoPorNombre(esp[i].nombre) == TipoPiso::Bano) aBano = true;
            }
        const double ancho = v.c.v.ancho;
        if (eeuu) {
            if (exterior && (ancho < 32 * 0.0254 * metro || (v.c.alto > 0 && v.c.alto < 78 * 0.0254 * metro)))
                mal(2, "Puerta principal " + v.c.marca + ": mide " + L(ancho) + " x " + L(v.c.alto)
                           + "; el mínimo es 32\" de paso y 78\" de alto.", "IRC R311.2");
        } else {
            const double minimo = (terceraEdad || exterior || !aAlcoba) ? 0.80 * metro : 0.70 * metro;
            if (ancho + 1e-9 < minimo)
                mal(2, "Puerta " + v.c.marca + ": mide " + L(ancho) + "; el mínimo es " + L(minimo)
                           + (terceraEdad ? " (modo 3.ª edad)." : "."), "NSR-10 K.3.8.2.1 / NTC 6047");
        }
        (void)aBano;
    }

    // --- Espacios
    int alcobas = 0;
    double areaTotal = 0;
    bool banoAccesible = false, hayBano = false;
    for (size_t i = 0; i < esp.size(); ++i) {
        const EspacioVentas &e = esp[i];
        if (e.celdas.empty()) continue;
        double area = 0;
        for (const Rect &k : e.celdas) area += (k.x2 - k.x1) * (k.y2 - k.y1);
        areaTotal += area;
        const Rect b = cajaCeldas(e.celdas);
        const double ladoMin = std::min(b.x2 - b.x1, b.y2 - b.y1);
        const bool alcoba = esAlcoba(e.nombre);
        if (alcoba) ++alcobas;
        const TipoPiso t = pisoPorNombre(e.nombre);
        if (t == TipoPiso::Bano) {
            hayBano = true;
            for (size_t k = 0; k < vanos.size(); ++k)
                if ((lados[k].first == (int)i || lados[k].second == (int)i) && vanos[k].c.tipo != "VENTANA"
                    && vanos[k].c.v.ancho + 1e-9 >= 29 * 0.0254 * metro)
                    banoAccesible = true;
        }
        if (esPasillo(e.nombre)) {
            const double minimo = eeuu ? 36 * 0.0254 * metro : 0.90 * metro;
            if (ladoMin + 1e-9 < minimo)
                mal(2, "«" + e.nombre + "» mide " + L(ladoMin) + " de ancho; el mínimo es " + L(minimo) + ".",
                    eeuu ? "IRC R311.6" : "NSR-10 K.3.3.4");
        }
        if (eeuu && esHabitable(e.nombre)) {
            if (area + 1e-9 < 6.5 * metro * metro)
                mal(2, "«" + e.nombre + "»: " + A(area) + "; un cuarto habitable necesita al menos 70 ft\xC2\xB2 (6,5 m\xC2\xB2).",
                    "FBC-R R304.1");
            if (ladoMin + 1e-9 < 7 * 12 * 0.0254 * metro)
                mal(2, "«" + e.nombre + "»: el lado más corto mide " + L(ladoMin) + "; el mínimo es 7'-0\".", "FBC-R R304.2");
        }
        // Ventilación e iluminación: alcobas y sala con ventana.
        const std::string n = mayusculasAscii(e.nombre);
        const bool sala = n.find("SALA") != std::string::npos || n.find("LIVING") != std::string::npos
                          || n.find("ESTAR") != std::string::npos || n.find("FAMILY") != std::string::npos;
        if ((alcoba || sala) && ventanas[i].empty())
            mal(2, "«" + e.nombre + "» no tiene ventana: alcobas y sala necesitan ventana a fachada o patio.",
                eeuu ? "FBC-R R303 / R310" : "NSR-10 / Decreto 555 Bogotá, anexo 5");
        // Ventana de escape (EE. UU.): en cada alcoba.
        if (eeuu && alcoba && !ventanas[i].empty()) {
            bool escape = false;
            for (const VanoNorma *v : ventanas[i]) {
                const double a = v->c.v.ancho * v->c.alto;
                if (a + 1e-9 >= 5.7 * 144 * 0.0254 * 0.0254 * metro * metro && v->c.alto + 1e-9 >= 24 * 0.0254 * metro
                    && v->c.v.ancho + 1e-9 >= 20 * 0.0254 * metro && v->c.antepecho <= 44 * 0.0254 * metro + 1e-9)
                    escape = true;
            }
            if (!escape)
                mal(2, "«" + e.nombre + "»: ninguna ventana sirve de escape (5,7 ft\xC2\xB2 de abertura, 24\" de alto, 20\" de "
                       "ancho y antepecho de 44\" o menos).", "FBC-R R310.2");
        }
        // Modo 3.ª edad: círculo de giro de 1,50 m en baños, alcobas y pasillos.
        if (terceraEdad && (alcoba || t == TipoPiso::Bano || esPasillo(e.nombre))) {
            const double giro = eeuu ? 60 * 0.0254 * metro : 1.50 * metro;
            if (ladoMin + 1e-9 < giro)
                mal(1, "«" + e.nombre + "»: no cabe el círculo de giro de una silla de ruedas (" + L(giro) + "); lado de "
                       + L(ladoMin) + ".", eeuu ? "ADA 304 (referencia)" : "NTC 6047 / NTC 4143");
        }
    }
    // Altura libre
    if (eeuu) {
        if (altoMuro + 1e-9 < 7 * 12 * 0.0254 * metro)
            mal(2, "Alto libre de " + L(altoMuro) + ": los cuartos habitables y pasillos piden 7'-0\" o más.", "FBC-R R305.1");
    } else if (altoMuro + 1e-9 < 2.30 * metro) {
        mal(norma == Norma::Bogota ? 2 : 1, "Alto libre de " + L(altoMuro) + ": en Bogotá se piden 2,30 m entre placas.",
            "Decreto 555 Bogotá, anexo 5");
    }
    // Área mínima de la vivienda (Bogotá)
    if (norma == Norma::Bogota) {
        const double minimo = std::max(36.0, 18.0 * alcobas) * metro * metro;
        if (areaTotal + 1e-9 < minimo)
            mal(2, "Área de la vivienda " + A(areaTotal) + ": con " + std::to_string(alcobas) + " alcoba(s) el mínimo es "
                       + A(minimo) + ".", "Decreto 555 de 2021, art. 384");
    }
    // Baño en el nivel de acceso con puerta de 29" (Florida)
    if (eeuu && hayBano && !banoAccesible)
        mal(2, "Ningún baño tiene puerta de 29\" (73,7 cm) libres o más: la ley de Florida pide uno en el nivel de acceso.",
            "Florida §553.504");
    // Miami-Dade: aprobación de producto en aberturas exteriores
    if (norma == Norma::MiamiDade)
        for (size_t k = 0; k < vanos.size(); ++k) {
            const bool exterior = lados[k].first < 0 || lados[k].second < 0;
            if (exterior && vanos[k].c.tipo != "VANO" && vanos[k].aprobacion.empty())
                mal(1, vanos[k].c.marca + ": abertura exterior sin NOA / FL# (zona de huracanes HVHZ: impacto o protección).",
                    "FBC, HVHZ");
        }
    if (problemas == 0)
        r.insert(r.begin(), {0, "No encontré incumplimientos en lo que se puede medir en la planta.", ""});
    return r;
}

} // namespace arq
