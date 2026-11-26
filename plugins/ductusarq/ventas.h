/*****************************************************************************/
/*  ventas.h - Plano de ventas a color (DuctusArq 2D), sin Qt                 */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*****************************************************************************/
//
// Lámina para el equipo de ventas y mercadeo: muros llenos, piso de color según el espacio
// (cálido en alcobas y sala, cerámica en baños y cocina, más oscuro en closets), puertas y
// ventanas, muebles y el nombre de cada espacio con sus medidas. Sin ejes, cotas ni rótulo.
// Aquí solo se arma la geometría (coordenadas del dibujo); ventas.cpp la pinta con Qt.

#ifndef DUCTUSARQ_VENTAS_H
#define DUCTUSARQ_VENTAS_H

#include "arq.h"
#include "muebles.h"

#include <cctype>

namespace arq {

struct Color {
    unsigned char r, g, b;
};

enum class TipoPiso { Calido, Bano, Cocina, Closet, Exterior, Vacio };

inline std::string mayusculasAscii(const std::string &s) {
    std::string r = s;
    for (char &c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

// El tipo de piso sale del nombre del espacio (español o inglés).
inline TipoPiso pisoPorNombre(const std::string &nombre) {
    const std::string n = mayusculasAscii(nombre);
    auto tiene = [&](std::initializer_list<const char *> l) {
        for (const char *x : l) if (n.find(x) != std::string::npos) return true;
        return false;
    };
    if (tiene({"VAC\xC3\x8D" "O", "VACIO", "OPEN TO BELOW", "DOBLE ALTURA", "DOUBLE HEIGHT"})) return TipoPiso::Vacio;
    if (tiene({"CLOSET", "VESTIER", "WALK", "LINEN", "DEP\xC3\x93SITO", "DEPOSITO", "STORAGE", "PANTRY", "ALACENA",
               "BODEGA"}))
        return TipoPiso::Closet;
    if (tiene({"BA\xC3\x91" "O", "BA\xC3\xB1" "O", "BANO", "BATH", "ASEO", "TOILET", "POWDER", "W.C", "WC", "DUCHA",
               "SHOWER"}))
        return TipoPiso::Bano;
    if (tiene({"COCINA", "KITCHEN", "LAVANDER", "LAUNDRY", "ROPAS", "UTILITY"})) return TipoPiso::Cocina;
    if (tiene({"TERRAZA", "BALC", "PATIO", "PORCH", "LANAI", "JARD", "GARAJE", "GARAGE", "PARQUEADERO", "DECK"}))
        return TipoPiso::Exterior;
    return TipoPiso::Calido;
}

// Paleta: 0 cálida (como los folletos de ventas), 1 gris claro (moderna).
inline Color colorPiso(TipoPiso t, int paleta) {
    if (paleta == 1) {
        switch (t) {
        case TipoPiso::Bano: return {214, 226, 232};
        case TipoPiso::Cocina: return {226, 226, 222};
        case TipoPiso::Closet: return {200, 200, 196};
        case TipoPiso::Exterior: return {220, 228, 214};
        case TipoPiso::Vacio: return {250, 250, 250};
        default: return {238, 236, 232};
        }
    }
    switch (t) {
    case TipoPiso::Bano: return {209, 226, 232};
    case TipoPiso::Cocina: return {232, 226, 212};
    case TipoPiso::Closet: return {214, 190, 150};
    case TipoPiso::Exterior: return {222, 226, 206};
    case TipoPiso::Vacio: return {251, 246, 234};
    default: return {243, 223, 180};
    }
}

inline Color oscurecer(Color c, double f) {
    return {static_cast<unsigned char>(c.r * f), static_cast<unsigned char>(c.g * f), static_cast<unsigned char>(c.b * f)};
}

// ---------------------------------------------------------------------------
// Lámina: lo que se pinta, en coordenadas del dibujo. Los grosores van en unidades del dibujo.
// capa = orden de pintado: 0 pisos, 1 cerámica, 2 muros, 3 vanos, 4 puertas y ventanas,
// 5 fondo de muebles, 6 líneas de muebles.
struct Relleno {
    std::vector<P> pol;
    Color c;
    int capa;
};
struct Trazo {
    P a, b;
    Color c;
    double grosor;
    int capa;
};
struct TrazoArco {
    Arco a;
    Color c;
    double grosor;
    int capa;
};
struct TextoLamina {
    P pos;              // centro del texto
    std::string texto;
    double alto;        // alto de letra deseado
    double anchoMax;    // si no cabe, se achica
    Color c;
    bool negrita;
};
struct Lamina {
    std::vector<Relleno> rellenos;
    std::vector<Trazo> trazos;
    std::vector<TrazoArco> arcos;
    std::vector<TextoLamina> textos;
    Rect caja{0, 0, 0, 0};
    bool vacia() const { return rellenos.empty(); }
};

struct EspacioVentas {
    std::string nombre;
    P etiqueta;
    std::vector<Rect> celdas;  // vacío si no se pudo recalcular (se pinta sin piso propio)
    double area = 0;           // unidades del dibujo al cuadrado
};

struct MuebleVentas {
    Dibujo d;                  // ya colocado (coordenadas del dibujo)
    std::vector<P> contorno;   // rectángulo girado
};

struct OpcionesVentas {
    bool medidas = true;   // ancho x fondo de cada espacio
    bool area = false;     // área de cada espacio
    bool muebles = true;
    int paleta = 0;
};

// Puerta o ventana a partir de su dato (DUCTUS-VANO|marca|tipo|ancho|alto|antepecho|aprob|esquinas[|lado,bisagra]).
inline bool vanoDeDato(const std::string &t, Vano &v) {
    const auto p = partir(t, '|');
    if (p.size() < 8 || p[0] != "DUCTUS-VANO") return false;
    const auto n = partir(p[7], ',');
    if (n.size() != 8) return false;
    double c[8];
    for (int i = 0; i < 8; ++i) if (!leerNumero(n[static_cast<size_t>(i)], c[i])) return false;
    const P a(c[0], c[1]), b(c[2], c[3]), d(c[6], c[7]);
    const double ancho = largo(b - a), espesor = largo(d - a);
    if (ancho <= 0 || espesor <= 0) return false;
    v.o = a;
    v.dir = (b - a) * (1.0 / ancho);
    v.n = (d - a) * (1.0 / espesor);
    v.ancho = ancho;
    v.espesor = espesor;
    v.tipo = TipoVano::Vano;
    for (int k = 0; k <= 4; ++k)
        if (p[2] == nombreTipoVano(static_cast<TipoVano>(k))) v.tipo = static_cast<TipoVano>(k);
    v.lado = 1;
    v.bisagraInicio = true;
    if (p.size() >= 9) {
        const auto lb = partir(p[8], ',');
        if (lb.size() == 2) {
            v.lado = std::atoi(lb[0].c_str()) < 0 ? -1 : 1;
            v.bisagraInicio = std::atoi(lb[1].c_str()) != 0;
        }
    }
    return true;
}

inline std::string datoLadoVano(const Vano &v) {
    return std::string(v.lado < 0 ? "-1" : "1") + "," + (v.bisagraInicio ? "1" : "0");
}

inline Rect cajaCeldas(const std::vector<Rect> &celdas) {
    Rect r = celdas.front();
    for (const Rect &c : celdas) {
        r.x1 = std::min(r.x1, c.x1); r.y1 = std::min(r.y1, c.y1);
        r.x2 = std::max(r.x2, c.x2); r.y2 = std::max(r.y2, c.y2);
    }
    return r;
}

inline bool dentroDe(const std::vector<Rect> &celdas, P p) {
    for (const Rect &c : celdas)
        if (p.x >= c.x1 && p.x <= c.x2 && p.y >= c.y1 && p.y <= c.y2) return true;
    return false;
}

// "15'-0\" X 14'-0\"" o "3.50 x 4.00 m"
inline std::string medidasEspacio(double ancho, double fondo, Sistema s) {
    if (s == Sistema::Imperial) {
        // Al pie más cercano en pulgadas enteras, como en los folletos de EE. UU.
        auto pies = [](double pulg) {
            const long t = static_cast<long>(std::lround(pulg));
            return std::to_string(t / 12) + "'-" + std::to_string(t % 12) + "\"";
        };
        return pies(ancho) + " X " + pies(fondo);
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f x %.2f m", ancho, fondo);
    return buf;
}

inline std::string textoArea(double area, Sistema s) {
    char buf[64];
    if (s == Sistema::Imperial) std::snprintf(buf, sizeof buf, "%.0f ft\xC2\xB2", area / 144.0);
    else std::snprintf(buf, sizeof buf, "%.2f m\xC2\xB2", area);
    return buf;
}

inline size_t utf8Largo(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

inline double solape(const Rect &a, const Rect &b) {
    const double w = std::min(a.x2, b.x2) - std::max(a.x1, b.x1), h = std::min(a.y2, b.y2) - std::max(a.y1, b.y1);
    return w > 0 && h > 0 ? w * h : 0;
}

// Centro para el nombre del espacio: dentro del espacio, sin tapar muebles y lo más cerca
// posible del punto donde se hizo clic. Si no hay lugar libre, el de menos solape.
inline P lugarLibre(const EspacioVentas &e, double w, double h, const std::vector<Rect> &obst) {
    const Rect c = cajaCeldas(e.celdas);
    const double margen = std::min(w, h) * 0.15;
    P mejor = e.etiqueta;
    double mejorCosto = 1e300;
    const int n = 24;
    for (int i = 0; i <= n; ++i)
        for (int j = 0; j <= n; ++j) {
            const P p(c.x1 + (c.x2 - c.x1) * i / n, c.y1 + (c.y2 - c.y1) * j / n);
            const Rect t{p.x - w / 2 - margen, p.y - h / 2 - margen, p.x + w / 2 + margen, p.y + h / 2 + margen};
            bool dentro = true;
            for (const P &q : {P(t.x1, t.y1), P(t.x2, t.y1), P(t.x2, t.y2), P(t.x1, t.y2), p})
                if (!dentroDe(e.celdas, q)) { dentro = false; break; }
            if (!dentro) continue;
            double tapa = 0;
            for (const Rect &o : obst) tapa += solape(t, o);
            const double dx = p.x - e.etiqueta.x, dy = p.y - e.etiqueta.y;
            const double costo = tapa * 1e6 + std::sqrt(dx * dx + dy * dy);
            if (costo < mejorCosto) { mejorCosto = costo; mejor = p; }
        }
    return mejor;
}

inline Lamina planoVentas(const std::vector<std::vector<P>> &muros, const std::vector<Vano> &vanos,
                          const std::vector<EspacioVentas> &espacios, const std::vector<MuebleVentas> &muebles,
                          const OpcionesVentas &op, Sistema sistema) {
    Lamina L;
    if (muros.empty()) return L;
    // Caja del plano
    Rect caja{muros[0][0].x, muros[0][0].y, muros[0][0].x, muros[0][0].y};
    for (const auto &m : muros)
        for (const P &p : m) {
            caja.x1 = std::min(caja.x1, p.x); caja.y1 = std::min(caja.y1, p.y);
            caja.x2 = std::max(caja.x2, p.x); caja.y2 = std::max(caja.y2, p.y);
        }
    L.caja = caja;
    const double tam = std::max(caja.x2 - caja.x1, caja.y2 - caja.y1);
    const double fino = tam / 1400, medio = tam / 700;
    const double metro = sistema == Sistema::Imperial ? 39.37007874 : 1.0;
    const Color muro{46, 42, 40}, tinta{70, 56, 44}, puerta{122, 92, 60}, vidrio{96, 142, 176}, mueble{112, 100, 86};
    const Color calido = colorPiso(TipoPiso::Calido, op.paleta);

    // Pisos
    for (const EspacioVentas &e : espacios) {
        if (e.celdas.empty()) continue;
        const TipoPiso t = pisoPorNombre(e.nombre);
        const Color c = colorPiso(t, op.paleta);
        for (const Rect &r : e.celdas) L.rellenos.push_back({esquinas(r), c, 0});
        // Cerámica: cuadrícula de 30 cm (12") en baños y cocinas.
        if (t == TipoPiso::Bano || t == TipoPiso::Cocina) {
            const double paso = 0.30 * metro;
            const Color linea = oscurecer(c, 0.93);
            for (const Rect &r : e.celdas) {
                for (double x = std::ceil(r.x1 / paso) * paso; x < r.x2; x += paso)
                    L.trazos.push_back({P(x, r.y1), P(x, r.y2), linea, fino * 0.8, 1});
                for (double y = std::ceil(r.y1 / paso) * paso; y < r.y2; y += paso)
                    L.trazos.push_back({P(r.x1, y), P(r.x2, y), linea, fino * 0.8, 1});
            }
        }
    }
    // Muros llenos
    for (const auto &m : muros) L.rellenos.push_back({m, muro, 2});

    // Puertas y ventanas: se abre el muro y se dibuja el símbolo.
    std::vector<Rect> barridos;
    for (const Vano &v : vanos) {
        const P c1 = local(v, 0, 0), c2 = local(v, v.ancho, 0), c3 = local(v, v.ancho, v.espesor), c4 = local(v, 0, v.espesor);
        if (v.tipo == TipoVano::Ventana) {
            L.rellenos.push_back({{c1, c2, c3, c4}, Color{255, 255, 255}, 3});
            const SimboloVano s = simboloVano(v, 0);
            for (const Seg &x : s.lineas) L.trazos.push_back({x.a, x.b, vidrio, fino * 1.3, 4});
            continue;
        }
        // Color del piso a cada lado del vano (el del espacio de donde está el centro).
        Color piso = calido;
        const P centro = local(v, v.ancho / 2, v.espesor / 2);
        for (const EspacioVentas &e : espacios) {
            if (e.celdas.empty()) continue;
            if (dentroDe(e.celdas, centro + v.n * (v.espesor * 0.5 + 0.05 * metro))
                || dentroDe(e.celdas, centro - v.n * (v.espesor * 0.5 + 0.05 * metro))) {
                piso = colorPiso(pisoPorNombre(e.nombre), op.paleta);
                break;
            }
        }
        L.rellenos.push_back({{c1, c2, c3, c4}, piso, 3});
        const SimboloVano s = simboloVano(v, 0);
        for (size_t i = 0; i < s.lineas.size(); ++i) {
            const bool jamba = i < 2;  // las dos primeras son las jambas
            L.trazos.push_back({s.lineas[i].a, s.lineas[i].b, jamba ? muro : puerta, jamba ? fino : medio, 4});
        }
        for (const Arco &a : s.arcos) {
            L.arcos.push_back({a, puerta, fino, 4});
            // El barrido de la hoja también se esquiva con el nombre del espacio.
            double a2 = a.a2;
            if (a2 < a.a1) a2 += 6.283185307179586;
            Rect b{a.c.x, a.c.y, a.c.x, a.c.y};
            for (int k = 0; k <= 8; ++k) {
                const double t = a.a1 + (a2 - a.a1) * k / 8;
                const P q(a.c.x + a.r * std::cos(t), a.c.y + a.r * std::sin(t));
                b.x1 = std::min(b.x1, q.x); b.y1 = std::min(b.y1, q.y);
                b.x2 = std::max(b.x2, q.x); b.y2 = std::max(b.y2, q.y);
            }
            barridos.push_back(b);
        }
    }

    // Muebles
    if (op.muebles) {
        for (const MuebleVentas &m : muebles) {
            if (m.contorno.size() >= 3) L.rellenos.push_back({m.contorno, Color{253, 250, 243}, 5});
            for (const Seg &s : m.d.lineas) L.trazos.push_back({s.a, s.b, mueble, fino, 6});
            for (const auto &k : m.d.circulos) {
                Arco a{k.first, k.second, 0, 6.283185307179586};
                L.arcos.push_back({a, mueble, fino, 6});
            }
        }
    }

    // Nombre y medidas de cada espacio
    const double altoNombre = 0.24 * metro, altoDato = 0.16 * metro;
    std::vector<Rect> obstaculos = barridos;  // puertas y muebles: el nombre no les cae encima
    if (op.muebles)
        for (const MuebleVentas &m : muebles) {
            if (m.contorno.empty()) continue;
            Rect r{m.contorno[0].x, m.contorno[0].y, m.contorno[0].x, m.contorno[0].y};
            for (const P &q : m.contorno) {
                r.x1 = std::min(r.x1, q.x); r.y1 = std::min(r.y1, q.y);
                r.x2 = std::max(r.x2, q.x); r.y2 = std::max(r.y2, q.y);
            }
            obstaculos.push_back(r);
        }
    for (const EspacioVentas &e : espacios) {
        double ancho = 0, fondo = 0, anchoMax = 3 * metro;
        if (!e.celdas.empty()) {
            const Rect r = cajaCeldas(e.celdas);
            ancho = r.x2 - r.x1;
            fondo = r.y2 - r.y1;
            anchoMax = ancho * 0.86;
        }
        std::vector<std::string> datos;
        if (op.medidas && ancho > 0) datos.push_back(medidasEspacio(ancho, fondo, sistema));
        if (op.area && e.area > 0) datos.push_back(textoArea(e.area, sistema));
        const double total = altoNombre + datos.size() * altoDato * 1.35;
        // Ancho estimado del bloque de texto (letra con serifa en negrita: ~0.66 del alto).
        size_t letras = utf8Largo(e.nombre);
        double wTexto = std::min(anchoMax, letras * altoNombre * 0.66);
        for (const std::string &d : datos) wTexto = std::max(wTexto, std::min(anchoMax, utf8Largo(d) * altoDato * 0.6));
        const P centro = !e.celdas.empty() ? lugarLibre(e, wTexto, total, obstaculos) : e.etiqueta;
        double y = centro.y + total / 2 - altoNombre / 2;
        L.textos.push_back({P(centro.x, y), e.nombre, altoNombre, anchoMax, tinta, true});
        y -= altoNombre * 0.6 + altoDato * 0.85;
        for (const std::string &d : datos) {
            L.textos.push_back({P(centro.x, y), d, altoDato, anchoMax, tinta, false});
            y -= altoDato * 1.35;
        }
    }
    return L;
}

// Rectángulo girado de un mueble (para el fondo blanco debajo de sus líneas).
inline std::vector<P> contornoMueble(P c, double w, double f, double grados) {
    const double a = grados * 3.14159265358979323846 / 180.0, ca = std::cos(a), sa = std::sin(a);
    std::vector<P> r;
    for (const P &q : {P(-w / 2, -f / 2), P(w / 2, -f / 2), P(w / 2, f / 2), P(-w / 2, f / 2)})
        r.emplace_back(c.x + q.x * ca - q.y * sa, c.y + q.x * sa + q.y * ca);
    return r;
}

} // namespace arq

#endif // DUCTUSARQ_VENTAS_H
