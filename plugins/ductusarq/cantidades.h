#pragma once
// Cantidades de obra a partir del plano de DuctusArq (sin Qt; se prueba en pruebas_arq.cpp).
// Son cantidades APROXIMADAS para presupuesto preliminar: muros, columnas, pisos por material,
// cielo raso, pintura, zócalo, puertas y ventanas. No reemplazan el presupuesto del profesional.

#include "ventas.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace arq {

struct VanoCantidad {
    std::string marca, tipo;  // tipo: BATIENTE, DOBLE, CORREDIZA, VENTANA, VANO
    Vano v;                   // posición y ancho (para saber a qué espacios da)
    double alto = 0, antepecho = 0;
};

struct Cantidades {
    // Muros (sin columnas)
    double largoMuros = 0;        // largo de eje
    double areaMurosBruta = 0;    // largo x alto (una cara)
    double areaVanosEnMuros = 0;  // puertas y ventanas
    double areaMurosNeta = 0;
    double volumenMuros = 0;
    // Columnas
    int columnas = 0;
    double volumenColumnas = 0;
    // Espacios
    double areaPisos = 0;
    std::map<std::string, double> pisoPorMaterial;  // "Madera / laminado", "Cerámica"...
    double cieloRaso = 0;
    double pintura = 0;        // muros interiores, caras hacia los espacios secos
    double enchapeBanos = 0;   // muros de baños (para enchape hasta el techo)
    double zocalo = 0;         // perímetro de espacios menos puertas
    // Puertas y ventanas
    std::map<std::string, int> puertasPorMarca, ventanasPorMarca;
    double areaVidrio = 0;
};

inline std::string materialDePiso(TipoPiso t) {
    switch (t) {
    case TipoPiso::Bano: return "Cerámica antideslizante (baños)";
    case TipoPiso::Cocina: return "Porcelanato (cocina y ropas)";
    case TipoPiso::Closet: return "Madera / laminado (closets)";
    case TipoPiso::Exterior: return "Piso exterior (terrazas, patios)";
    case TipoPiso::Vacio: return "Vacío (sin piso)";
    default: return "Madera / laminado o porcelanato (zonas secas)";
    }
}

// metro: unidades del dibujo por metro. altoMuro: altura libre de muros (unidades del dibujo).
inline Cantidades calcularCantidades(const std::vector<std::vector<P>> &muros, const std::vector<VanoCantidad> &vanos,
                                     const std::vector<EspacioVentas> &esp, double altoMuro, double metro) {
    Cantidades c;
    const double ladoColumnaMax = 0.80 * metro;
    for (const auto &w : muros) {
        if (w.size() < 3) continue;
        Rect r{w[0].x, w[0].y, w[0].x, w[0].y};
        for (const P &p : w) {
            r.x1 = std::min(r.x1, p.x); r.y1 = std::min(r.y1, p.y);
            r.x2 = std::max(r.x2, p.x); r.y2 = std::max(r.y2, p.y);
        }
        const double a = r.x2 - r.x1, b = r.y2 - r.y1;
        const double largo = std::max(a, b), espesor = std::min(a, b);
        // Columna: casi cuadrada y pequeña.
        if (largo <= ladoColumnaMax && espesor >= 0.6 * largo) {
            ++c.columnas;
            c.volumenColumnas += a * b * altoMuro;
            continue;
        }
        c.largoMuros += largo;
        c.volumenMuros += largo * espesor * altoMuro;
    }
    c.areaMurosBruta = c.largoMuros * altoMuro;

    for (const VanoCantidad &v : vanos) {
        const bool ventana = v.tipo == "VENTANA";
        const bool abierto = v.tipo == "VANO";
        const double alto = std::min(v.alto > 0 ? v.alto : 2.10 * metro, altoMuro);
        c.areaVanosEnMuros += v.v.ancho * alto;
        if (ventana) {
            ++c.ventanasPorMarca[v.marca];
            c.areaVidrio += v.v.ancho * alto;
        } else if (!abierto) {
            ++c.puertasPorMarca[v.marca];
        }
    }
    c.areaMurosNeta = std::max(0.0, c.areaMurosBruta - c.areaVanosEnMuros);

    for (const EspacioVentas &e : esp) {
        if (e.celdas.empty()) continue;
        const TipoPiso t = pisoPorNombre(e.nombre);
        if (t == TipoPiso::Vacio) continue;
        double area = 0;
        for (const Rect &k : e.celdas) area += (k.x2 - k.x1) * (k.y2 - k.y1);
        c.areaPisos += area;
        c.pisoPorMaterial[materialDePiso(t)] += area;
        if (t != TipoPiso::Exterior) c.cieloRaso += area;
        // Perímetro de la caja del espacio (los cuartos son casi siempre rectángulos).
        const Rect b = cajaCeldas(e.celdas);
        const double perimetro = 2 * ((b.x2 - b.x1) + (b.y2 - b.y1));
        // Vanos que dan a este espacio (a un lado u otro del muro).
        double anchoPuertas = 0, areaVanos = 0;
        for (const VanoCantidad &v : vanos) {
            const P m = local(v.v, v.v.ancho / 2, v.v.espesor / 2);
            const double sale = v.v.espesor / 2 + 0.20 * metro;
            if (!dentroDe(e.celdas, m - v.v.n * sale) && !dentroDe(e.celdas, m + v.v.n * sale)) continue;
            const double alto = std::min(v.alto > 0 ? v.alto : 2.10 * metro, altoMuro);
            areaVanos += v.v.ancho * alto;
            if (v.tipo != "VENTANA") anchoPuertas += v.v.ancho;
        }
        const double muro = std::max(0.0, perimetro * altoMuro - areaVanos);
        if (t == TipoPiso::Bano) c.enchapeBanos += muro;
        else if (t != TipoPiso::Exterior) c.pintura += muro;
        if (t != TipoPiso::Bano && t != TipoPiso::Exterior) c.zocalo += std::max(0.0, perimetro - anchoPuertas);
    }
    return c;
}

} // namespace arq
