/*****************************************************************************/
/*  nesting.h - EDWIN CNC: acomodo automático de piezas en la lámina        */
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
// Acomoda piezas de CUALQUIER forma (con huecos) en láminas rectangulares para gastar la menor cantidad de
// material. Sin Qt: se prueba solo (pruebas_nesting.cpp).
//
// Cómo trabaja (nesting por rejilla, "raster"):
//  1) La lámina se ve como una cuadrícula de celdas de `celda` mm. Cada pieza, en cada giro permitido, se
//     convierte en las celdas que toca (si la pieza toca una celda, aunque sea un poquito, la celda cuenta:
//     así nunca se montan dos piezas). Los huecos de la pieza quedan libres, de modo que una pieza pequeña
//     puede caer DENTRO del hueco de una grande.
//  2) La separación entre piezas (`separacion`, mm: ancho del corte + lo que se quiera dejar) se garantiza
//     engordando la pieza nueva esa distancia antes de buscar sitio. El borde de la lámina deja `margen`.
//  3) Las piezas van de la más grande a la más chica. Cada una se pone en la primera lámina donde quepa,
//     lo más a la izquierda posible y después lo más abajo (así lo que sobra queda junto, a la derecha,
//     como un retal aprovechable). Si no cabe en ninguna, se abre otra lámina.
// Las filas de la cuadrícula se guardan como bits (64 celdas por número), así revisar si una posición
// está libre es rápido aun con cientos de piezas.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nesting {

constexpr double PI = 3.14159265358979323846;

struct Pt {
    double x = 0, y = 0;
};
using Poligono = std::vector<Pt>;

// Una pieza: su contorno de afuera y sus huecos (polígonos; los arcos ya convertidos en tramos cortos).
struct Pieza {
    Poligono afuera;
    std::vector<Poligono> huecos;
    int cantidad = 1;
};

struct Opciones {
    double ancho = 2440, alto = 1220;   // lámina (mm)
    double margen = 10;                 // distancia mínima al borde de la lámina (mm)
    double separacion = 6;              // distancia mínima entre piezas (mm)
    double celda = 0;                   // tamaño de celda (mm); 0 = automático según la lámina
    int giros = 4;                      // giros a probar: 1 (no gira), 2 (0/180), 4 (cada 90°), 8 (cada 45°)…
    int maxLaminas = 200;
};

// Dónde quedó una copia de una pieza: se gira `angulo` (radianes, contra el reloj) alrededor del origen
// del dibujo y después se mueve (dx, dy). Las coordenadas son las de la lámina `lamina` (0, 1, 2…), con
// la esquina de abajo a la izquierda de la lámina en (0, 0).
struct Colocacion {
    int pieza = 0, copia = 0, lamina = 0;
    double angulo = 0, dx = 0, dy = 0;
};

struct Resultado {
    std::vector<Colocacion> colocadas;
    std::vector<int> noCaben;           // piezas (índice) que no caben ni en una lámina vacía
    int laminas = 0;
    std::vector<double> usoPorLamina;   // área de piezas / área de la lámina (0..1)
    double areaPiezas = 0;
    double celda = 0;
};

inline double area(const Poligono &p) {
    double a = 0;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) a += p[j].x * p[i].y - p[i].x * p[j].y;
    return a / 2;
}
inline double areaNeta(const Pieza &p) {
    double a = std::fabs(area(p.afuera));
    for (const auto &h : p.huecos) a -= std::fabs(area(h));
    return std::max(0.0, a);
}
inline Pt girar(Pt p, double ang) {
    const double c = std::cos(ang), s = std::sin(ang);
    return {p.x * c - p.y * s, p.x * s + p.y * c};
}

// ─── Cuadrícula de bits ───────────────────────────────────────────────────────────────────────────────
struct Bits {
    int w = 0, h = 0, palabras = 0;
    std::vector<uint64_t> d;
    Bits() = default;
    Bits(int w_, int h_) : w(w_), h(h_), palabras((w_ + 63) / 64), d(size_t(h_) * ((w_ + 63) / 64), 0) {}
    bool get(int x, int y) const { return (d[size_t(y) * palabras + (x >> 6)] >> (x & 63)) & 1u; }
    void set(int x, int y) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        d[size_t(y) * palabras + (x >> 6)] |= uint64_t(1) << (x & 63);
    }
    const uint64_t *fila(int y) const { return &d[size_t(y) * palabras]; }
};

// Celdas que toca un polígono con huecos, en una cuadrícula con origen (ox, oy) y celdas de `c` mm.
// Una celda cuenta si su centro queda dentro (pares/impares con los huecos) o si un borde pasa por ella.
inline void rasterizar(const std::vector<const Poligono *> &anillos, double ox, double oy, double c, Bits &b) {
    // 1) centros dentro (barrido por filas)
    for (int y = 0; y < b.h; ++y) {
        const double yc = oy + (y + 0.5) * c;
        std::vector<double> xs;
        for (const Poligono *pp : anillos) {
            const Poligono &p = *pp;
            for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
                const Pt a = p[j], q = p[i];
                if ((a.y > yc) != (q.y > yc)) xs.push_back(a.x + (yc - a.y) * (q.x - a.x) / (q.y - a.y));
            }
        }
        std::sort(xs.begin(), xs.end());
        for (size_t k = 0; k + 1 < xs.size(); k += 2) {
            int x0 = int(std::ceil((xs[k] - ox) / c - 0.5)), x1 = int(std::floor((xs[k + 1] - ox) / c - 0.5));
            for (int x = std::max(0, x0); x <= std::min(b.w - 1, x1); ++x) b.set(x, y);
        }
    }
    // 2) celdas por donde pasa cada borde (recorrido exacto de celdas, sin saltarse ninguna)
    for (const Poligono *pp : anillos) {
        const Poligono &p = *pp;
        for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
            double x0 = (p[j].x - ox) / c, y0 = (p[j].y - oy) / c, x1 = (p[i].x - ox) / c, y1 = (p[i].y - oy) / c;
            int cx = int(std::floor(x0)), cy = int(std::floor(y0));
            const int ex = int(std::floor(x1)), ey = int(std::floor(y1));
            const double dx = x1 - x0, dy = y1 - y0;
            const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
            double tMaxX = dx != 0 ? ((sx > 0 ? cx + 1 - x0 : x0 - cx) / std::fabs(dx)) : 1e30;
            double tMaxY = dy != 0 ? ((sy > 0 ? cy + 1 - y0 : y0 - cy) / std::fabs(dy)) : 1e30;
            const double tdx = dx != 0 ? 1 / std::fabs(dx) : 1e30, tdy = dy != 0 ? 1 / std::fabs(dy) : 1e30;
            b.set(cx, cy);
            int guard = 0;
            while ((cx != ex || cy != ey) && guard++ < 100000) {
                if (tMaxX < tMaxY) { cx += sx; tMaxX += tdx; }
                else if (tMaxY < tMaxX) { cy += sy; tMaxY += tdy; }
                else { b.set(cx + sx, cy); b.set(cx, cy + sy); cx += sx; cy += sy; tMaxX += tdx; tMaxY += tdy; }
                b.set(cx, cy);
            }
        }
    }
}

// Engorda las celdas ocupadas r celdas en todas direcciones (disco).
inline Bits engordar(const Bits &b, int r) {
    if (r <= 0) return b;
    Bits o(b.w + 2 * r, b.h + 2 * r);
    std::vector<int> ancho(r + 1);
    for (int dy = 0; dy <= r; ++dy) ancho[dy] = int(std::floor(std::sqrt(double(r) * r - double(dy) * dy) + 1e-9));
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x) {
            if (!b.get(x, y)) continue;
            for (int dy = -r; dy <= r; ++dy) {
                const int a = ancho[std::abs(dy)];
                for (int dx = -a; dx <= a; ++dx) o.set(x + r + dx, y + r + dy);
            }
        }
    return o;
}

// Una forma lista para probar: la pieza girada y engordada, con sus filas corridas a cada posición x.
struct Forma {
    double angulo = 0;
    Bits mascara;           // pieza engordada (la separación ya incluida)
    int nucleoX = 0, nucleoY = 0, nucleoW = 0, nucleoH = 0;  // dónde está la pieza real dentro de la máscara
    double minX = 0, minY = 0;  // esquina de la pieza girada (mm) que corresponde a la celda (r, r)
    // rangos de cada fila de la máscara [primera, última] celda ocupada (para descartar rápido)
    std::vector<int> fx0, fx1;
};

inline bool choca(const Bits &ocupado, const Forma &f, int px, int py) {
    // px, py = posición de la celda (0,0) de la máscara en la lámina (puede ser negativa: el engorde puede
    // salirse de la lámina; la pieza real no).
    const Bits &m = f.mascara;
    for (int y = 0; y < m.h; ++y) {
        const int ly = py + y;
        if (f.fx0[y] > f.fx1[y]) continue;
        if (ly < 0 || ly >= ocupado.h) continue;  // fuera de la lámina: solo el engorde, que puede salirse
        const uint64_t *fila = ocupado.fila(ly);
        const uint64_t *mf = m.fila(y);
        // recorrer las palabras de la máscara corridas px celdas
        for (int wi = f.fx0[y] >> 6; wi <= (f.fx1[y] >> 6); ++wi) {
            uint64_t v = mf[wi];
            if (!v) continue;
            const int base = px + wi * 64;   // celda de la lámina donde cae el bit 0 de esta palabra
            if (base >= ocupado.w || base + 63 < 0) continue;
            if (base >= 0) {
                const int w0 = base >> 6, s = base & 63;
                if (s == 0) { if (fila[w0] & v) return true; }
                else {
                    if (fila[w0] & (v << s)) return true;
                    if (w0 + 1 < ocupado.palabras && (fila[w0 + 1] & (v >> (64 - s)))) return true;
                }
            } else {
                const int s = -base;          // corrimiento a la derecha
                const uint64_t vv = s >= 64 ? 0 : (v >> s);
                if (fila[0] & vv) return true;
            }
        }
    }
    return false;
}

inline void marcar(Bits &ocupado, const Bits &nucleo, int px, int py) {
    for (int y = 0; y < nucleo.h; ++y)
        for (int x = 0; x < nucleo.w; ++x)
            if (nucleo.get(x, y)) ocupado.set(px + x, py + y);
}

inline Resultado acomodar(const std::vector<Pieza> &piezas, const Opciones &op) {
    Resultado res;
    const double W = op.ancho - 2 * op.margen, H = op.alto - 2 * op.margen;
    if (W <= 0 || H <= 0 || piezas.empty()) return res;
    // Celda automática: unas 600 columnas para la lámina, pero nunca menor de 1 mm ni mayor que la mitad
    // de la separación más 2 mm (con celdas grandes se desperdicia).
    double c = op.celda > 0 ? op.celda : std::max(1.0, std::max(W, H) / 600.0);
    c = std::max(0.5, c);
    res.celda = c;
    const int cw = int(std::floor(W / c)), ch = int(std::floor(H / c));
    const int r = int(std::ceil(op.separacion / c));
    const int giros = std::max(1, op.giros);

    // Lista de copias, de la más grande a la más chica.
    struct Copia { int pieza, copia; double a; };
    std::vector<Copia> copias;
    for (size_t i = 0; i < piezas.size(); ++i)
        for (int k = 0; k < std::max(1, piezas[i].cantidad); ++k) copias.push_back({int(i), k, areaNeta(piezas[i])});
    std::stable_sort(copias.begin(), copias.end(), [](const Copia &a, const Copia &b) { return a.a > b.a; });

    // Formas por pieza y giro (se calculan una vez).
    std::vector<std::vector<Forma>> formas(piezas.size());
    std::vector<std::vector<Bits>> nucleos(piezas.size());
    for (size_t i = 0; i < piezas.size(); ++i) {
        for (int g = 0; g < giros; ++g) {
            const double ang = 2 * PI * g / giros;
            Poligono af; for (Pt p : piezas[i].afuera) af.push_back(girar(p, ang));
            std::vector<Poligono> hs;
            for (const auto &h : piezas[i].huecos) { Poligono q; for (Pt p : h) q.push_back(girar(p, ang)); hs.push_back(q); }
            double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
            for (Pt p : af) { x0 = std::min(x0, p.x); y0 = std::min(y0, p.y); x1 = std::max(x1, p.x); y1 = std::max(y1, p.y); }
            const int nw = int(std::floor((x1 - x0) / c)) + 1, nh = int(std::floor((y1 - y0) / c)) + 1;
            Bits nuc(nw, nh);
            std::vector<const Poligono *> anillos{&af};
            for (const auto &h : hs) anillos.push_back(&h);
            rasterizar(anillos, x0, y0, c, nuc);
            Forma f;
            f.angulo = ang;
            f.mascara = engordar(nuc, r);
            f.nucleoX = r; f.nucleoY = r; f.nucleoW = nw; f.nucleoH = nh;
            f.minX = x0; f.minY = y0;
            f.fx0.assign(f.mascara.h, f.mascara.w); f.fx1.assign(f.mascara.h, -1);
            for (int y = 0; y < f.mascara.h; ++y)
                for (int x = 0; x < f.mascara.w; ++x)
                    if (f.mascara.get(x, y)) { f.fx0[y] = std::min(f.fx0[y], x); f.fx1[y] = std::max(f.fx1[y], x); }
            formas[i].push_back(std::move(f));
            nucleos[i].push_back(std::move(nuc));
        }
    }

    std::vector<Bits> laminas;
    std::vector<double> usado;
    auto buscar = [&](const Bits &oc, const Forma &f, int &bx, int &by) -> bool {
        // la pieza real (núcleo) debe quedar dentro: núcleo en [0, cw-nw] x [0, ch-nh]
        const int maxX = cw - f.nucleoW, maxY = ch - f.nucleoH;
        if (maxX < 0 || maxY < 0) return false;
        for (int x = 0; x <= maxX; ++x)
            for (int y = 0; y <= maxY; ++y)
                if (!choca(oc, f, x - f.nucleoX, y - f.nucleoY)) { bx = x; by = y; return true; }
        return false;
    };
    for (const Copia &cp : copias) {
        bool puesto = false;
        // ¿cabe en una lámina vacía con algún giro?
        bool cabe = false;
        for (const Forma &f : formas[cp.pieza]) if (f.nucleoW <= cw && f.nucleoH <= ch) { cabe = true; break; }
        if (!cabe) { if (cp.copia == 0) res.noCaben.push_back(cp.pieza); continue; }
        for (size_t L = 0; L <= laminas.size() && !puesto; ++L) {
            if (L == laminas.size()) {
                if (int(laminas.size()) >= op.maxLaminas) break;
                laminas.emplace_back(cw, ch); usado.push_back(0);
            }
            // el mejor giro: el que quede más a la izquierda (y luego más abajo)
            int mejor = -1, mx = 0, my = 0;
            for (size_t g = 0; g < formas[cp.pieza].size(); ++g) {
                int x, y;
                if (buscar(laminas[L], formas[cp.pieza][g], x, y))
                    if (mejor < 0 || x < mx || (x == mx && y < my)) { mejor = int(g); mx = x; my = y; }
            }
            if (mejor < 0) continue;
            const Forma &f = formas[cp.pieza][mejor];
            marcar(laminas[L], nucleos[cp.pieza][mejor], mx, my);
            Colocacion col;
            col.pieza = cp.pieza; col.copia = cp.copia; col.lamina = int(L); col.angulo = f.angulo;
            // la celda (mx, my) del núcleo corresponde a la esquina (minX, minY) de la pieza girada
            col.dx = op.margen + mx * c - f.minX;
            col.dy = op.margen + my * c - f.minY;
            res.colocadas.push_back(col);
            usado[L] += cp.a;
            res.areaPiezas += cp.a;
            puesto = true;
        }
    }
    res.laminas = int(laminas.size());
    for (double a : usado) res.usoPorLamina.push_back(a / (op.ancho * op.alto));
    return res;
}

} // namespace nesting
