/*****************************************************************************/
/*  piezas.h - EDWIN CNC: piezas de lámina por medidas (desarrollos)         */
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
// Los mismos cálculos de las calculadoras de taller de la web (ingemaqsewing.github.io/calculadoras), en
// C++ sin Qt (se prueba solo con pruebas_piezas.cpp). Cada función devuelve las piezas PLANAS listas para
// cortar: contornos cerrados (con arcos de verdad donde la pieza es un arco: tolva cónica, tapa de la tolva
// en U, arandela de sinfín) y las líneas de doblez / rolado para la capa MARCADO. Medidas en mm.
#pragma once

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace piezas {

constexpr double PI = 3.14159265358979323846;
constexpr double GRAD = PI / 180;

struct Pt { double x = 0, y = 0; };
// Vértice de polilínea con "bulge" hacia el siguiente vértice (tan(ángulo/4); + contra el reloj; 0 = recta).
struct V { double x = 0, y = 0, bulge = 0; };
using Contorno = std::vector<V>;          // cerrado
struct Linea { Pt a, b; };

struct Plana {
    std::string nombre;                   // "Tolva cónica 1/2", "Tapa 1", …
    std::vector<Contorno> cortes;         // el primero es el de afuera; los demás, huecos
    std::vector<Linea> marcas;            // dobleces, rolado, ejes de referencia
    std::string nota;                     // medidas clave para el taller
    int cantidad = 1;                     // cuántas iguales hay que cortar
};

struct Resultado {
    std::vector<Plana> piezas;
    std::string error;                    // vacío si todo bien
};

inline std::string f0(double v) { char b[32]; std::snprintf(b, sizeof b, "%.0f", v); return b; }
inline std::string f1(double v) { char b[32]; std::snprintf(b, sizeof b, "%.1f", v); std::string s = b; for (char &c : s) if (c == '.') c = ','; return s; }

inline Contorno poli(const std::vector<Pt> &p) { Contorno c; for (Pt q : p) c.push_back({q.x, q.y, 0}); return c; }

// ─── 1) Tolva cónica (cono truncado), partida en n piezas iguales ──────────────────────────────────────
inline Resultado tolvaConica(double Dsup, double Dinf, double H, int n) {
    Resultado r;
    if (!(Dsup > 0) || !(Dinf >= 0) || !(H > 0) || n < 1) { r.error = "Revise las medidas: diámetros y alto mayores que cero."; return r; }
    const double R = std::max(Dsup, Dinf) / 2, rr = std::min(Dsup, Dinf) / 2;
    Plana p;
    p.cantidad = n;
    p.nombre = n == 1 ? "Tolva cónica" : "Tolva cónica 1/" + std::to_string(n);
    if (R - rr < 0.05) {                       // cilindro
        const double w = 2 * PI * R / n;
        p.cortes.push_back(poli({{0, 0}, {w, 0}, {w, H}, {0, H}}));
        p.nota = "Cilindro: " + f1(w) + " x " + f1(H) + " mm";
        r.piezas.push_back(p);
        return r;
    }
    const double g = std::hypot(H, R - rr), Ro = g * R / (R - rr), Ri = Ro - g, th = 2 * PI * R / Ro / n;
    const double a1 = PI / 2 + th / 2, a2 = PI / 2 - th / 2, bu = std::tan(th / 4);
    Contorno c;
    c.push_back({Ro * std::cos(a1), Ro * std::sin(a1), -bu});           // arco de afuera, a favor del reloj
    c.push_back({Ro * std::cos(a2), Ro * std::sin(a2), 0});
    if (Ri > 1e-6) {
        c.push_back({Ri * std::cos(a2), Ri * std::sin(a2), bu});         // arco de adentro, de vuelta
        c.push_back({Ri * std::cos(a1), Ri * std::sin(a1), 0});
    } else c.push_back({0, 0, 0});
    p.cortes.push_back(c);
    p.marcas.push_back({{0, Ri}, {0, Ro}});                             // eje de la pieza (referencia)
    p.nota = "Radio mayor " + f1(Ro) + " · radio menor " + f1(Ri) + " · ángulo " + f1(th / GRAD) + "°";
    r.piezas.push_back(p);
    return r;
}

// ─── 2) Tolva en U (canal de mezcladora): cuerpo de una lámina + 2 tapas ────────────────────────────────
// ancho = 2R (fondo redondo), alto = total (pared recta + R), largo de la tolva.
inline Resultado tolvaU(double ancho, double alto, double largo) {
    Resultado r;
    const double R = ancho / 2, h = alto - R;
    if (!(ancho > 0) || !(largo > 0) || h < 0) { r.error = "El alto debe ser por lo menos la mitad del ancho (el fondo es medio círculo)."; return r; }
    const double W = 2 * h + PI * R;
    Plana cu;
    cu.nombre = "Tolva en U · cuerpo";
    cu.cortes.push_back(poli({{0, 0}, {W, 0}, {W, largo}, {0, largo}}));
    if (h > 0.5) { cu.marcas.push_back({{h, 0}, {h, largo}}); cu.marcas.push_back({{h + PI * R, 0}, {h + PI * R, largo}}); }
    cu.nota = "Desarrollo " + f1(W) + " x " + f1(largo) + " mm · rolar entre las marcas (R " + f1(R) + ")";
    r.piezas.push_back(cu);
    Plana ta;
    ta.nombre = "Tolva en U · tapa";
    ta.cantidad = 2;
    Contorno c;
    c.push_back({0, R + h, 0});
    c.push_back({0, R, 1.0});            // medio círculo contra el reloj por abajo (bulge = tan(45°) = 1)
    c.push_back({2 * R, R, 0});
    c.push_back({2 * R, R + h, 0});
    ta.cortes.push_back(c);
    ta.nota = "Tapa " + f1(2 * R) + " x " + f1(alto) + " mm";
    r.piezas.push_back(ta);
    return r;
}

// ─── Utilidad: punto 2D a distancias dp de P y dq de Q, del lado contrario a R (o del lado `signo`) ─────
struct P3 { double x, y, z; };
inline double d3(P3 a, P3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }
inline int ladoDe(Pt P, Pt Q, Pt C) {
    const double v = (Q.x - P.x) * (C.y - P.y) - (Q.y - P.y) * (C.x - P.x);
    return v > 0 ? 1 : v < 0 ? -1 : 0;
}
inline Pt circulos(Pt P, Pt Q, double dp, double dq, const Pt *R, int signo = -1) {
    const double dx = Q.x - P.x, dy = Q.y - P.y, L = std::hypot(dx, dy);
    const double x = (dp * dp - dq * dq + L * L) / (2 * L), y = std::sqrt(std::max(0.0, dp * dp - x * x));
    const double ux = dx / L, uy = dy / L;
    const Pt c1{P.x + ux * x - uy * y, P.y + uy * x + ux * y}, c2{P.x + ux * x + uy * y, P.y + uy * x - ux * y};
    if (R) return ladoDe(P, Q, c1) != ladoDe(P, Q, *R) ? c1 : c2;
    return ladoDe(P, Q, c1) == signo ? c1 : c2;
}

// ─── 3) Tolva piramidal (boca rectangular A x B arriba, salida a x b abajo, centradas) ───────────────────
// partes: 1 (una sola pieza con 3 dobleces), 2 (dos piezas en L) o 4 (cuatro caras).
inline Resultado tolvaPiramidal(double A, double B, double a, double b, double H, int partes) {
    Resultado r;
    if (!(A > 0 && B > 0 && a >= 0 && b >= 0 && H > 0)) { r.error = "Revise las medidas: boca, salida y alto mayores que cero."; return r; }
    const int SG[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    P3 T[4], Bt[4];
    for (int i = 0; i < 4; ++i) { T[i] = {SG[i][0] * A / 2, SG[i][1] * B / 2, H}; Bt[i] = {SG[i][0] * a / 2, SG[i][1] * b / 2, 0}; }
    auto desarrollar = [&](int i0, int m) {
        std::vector<Pt> top, bot;
        const int i = i0 % 4, j = (i + 1) % 4;
        top.push_back({0, 0}); top.push_back({d3(T[i], T[j]), 0});
        bot.push_back(circulos(top[0], top[1], d3(T[i], Bt[i]), d3(T[j], Bt[i]), nullptr, -1));
        bot.push_back(circulos(top[0], top[1], d3(T[i], Bt[j]), d3(T[j], Bt[j]), nullptr, -1));
        for (int k = 1; k < m; ++k) {
            const int c = (i0 + k) % 4, nn = (c + 1) % 4;
            const Pt P = top[k], Q = bot[k], Rv = top[k - 1];
            top.push_back(circulos(P, Q, d3(T[c], T[nn]), d3(Bt[c], T[nn]), &Rv));
            bot.push_back(circulos(P, Q, d3(T[c], Bt[nn]), d3(Bt[c], Bt[nn]), &Rv));
        }
        Plana p;
        std::vector<Pt> pts = top;
        for (int k = int(bot.size()) - 1; k >= 0; --k) {
            if (k < int(bot.size()) - 1 && std::hypot(bot[k].x - bot[k + 1].x, bot[k].y - bot[k + 1].y) < 1e-6) continue;  // salida en punta
            pts.push_back(bot[k]);
        }
        p.cortes.push_back(poli(pts));
        for (int k = 1; k < m; ++k) p.marcas.push_back({top[k], bot[k]});
        return p;
    };
    std::vector<std::pair<int, int>> grupos;
    if (partes == 1) grupos = {{0, 4}};
    else if (partes == 2) grupos = {{0, 2}, {2, 2}};
    else grupos = {{0, 1}, {1, 1}, {2, 1}, {3, 1}};
    int k = 1;
    for (auto g : grupos) {
        Plana p = desarrollar(g.first, g.second);
        p.nombre = "Tolva piramidal " + std::to_string(k++) + "/" + std::to_string(grupos.size());
        p.nota = "Boca " + f0(A) + " x " + f0(B) + " · salida " + f0(a) + " x " + f0(b) + " · alto " + f0(H);
        r.piezas.push_back(p);
    }
    return r;
}

// ─── Tira de triángulos (cuadrado a redondo y reducción): rayos de arriba (r) a abajo (p) ───────────────
struct Rayo { P3 r, p; bool doblez; };
inline bool igual3(P3 a, P3 b) { return d3(a, b) < 1e-9; }
inline Plana tira(const std::vector<Rayo> &ry) {
    std::vector<Pt> R2{{0, 0}}, P2{{0, -d3(ry[0].r, ry[0].p)}};
    Pt tercero{-1e9, -d3(ry[0].r, ry[0].p) / 2};
    for (size_t k = 1; k < ry.size(); ++k) {
        const Rayo &A = ry[k - 1], &Bq = ry[k];
        const Pt a2 = R2[k - 1], b2 = P2[k - 1];
        if (igual3(A.r, Bq.r)) {
            const Pt np = circulos(a2, b2, d3(A.r, Bq.p), d3(A.p, Bq.p), &tercero);
            R2.push_back(a2); P2.push_back(np); tercero = b2;
        } else {
            const Pt nr = circulos(a2, b2, d3(A.r, Bq.r), d3(A.p, Bq.r), &tercero);
            R2.push_back(nr); P2.push_back(b2); tercero = a2;
        }
    }
    std::vector<Pt> recto, curvo;
    for (size_t k = 0; k < ry.size(); ++k) {
        if (recto.empty() || std::hypot(R2[k].x - recto.back().x, R2[k].y - recto.back().y) > 1e-6) recto.push_back(R2[k]);
        if (curvo.empty() || std::hypot(P2[k].x - curvo.back().x, P2[k].y - curvo.back().y) > 1e-6) curvo.push_back(P2[k]);
    }
    std::vector<Pt> pts = recto;
    for (int k = int(curvo.size()) - 1; k >= 0; --k) pts.push_back(curvo[k]);
    Plana p;
    p.cortes.push_back(poli(pts));
    for (size_t k = 1; k + 1 < ry.size(); ++k) p.marcas.push_back({R2[k], P2[k]});
    return p;
}

// ─── 4) Cuadrado a redondo (boca rectangular A x B arriba, redondo Ø D abajo, alto H) ───────────────────
// N = divisiones de cada cuarto del redondo (líneas de prensado); partes 1, 2 o 4.
inline Resultado cuadradoRedondo(double A, double B, double D, double H, int N, int partes) {
    Resultado r;
    if (!(A > 0 && B > 0 && D > 0 && H > 0) || N < 1) { r.error = "Revise las medidas: lados, diámetro y alto mayores que cero."; return r; }
    const int SG[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    const double ANG[4] = {270 * GRAD, 0, 90 * GRAD, 180 * GRAD};
    P3 C[4], M[4];
    for (int i = 0; i < 4; ++i) C[i] = {SG[i][0] * A / 2, SG[i][1] * B / 2, H};
    for (int i = 0; i < 4; ++i) M[i] = {(C[i].x + C[(i + 1) % 4].x) / 2, (C[i].y + C[(i + 1) % 4].y) / 2, H};
    const int n = 4 * N;
    const double Rp = D / 2 * (PI / n) / std::sin(PI / n);   // el borde hecho de cuerdas mide justo π·D
    auto Pp = [&](double a) { return P3{Rp * std::cos(a), Rp * std::sin(a), 0}; };
    auto rayos = [&](int s, int m) {
        std::vector<Rayo> out{{M[s % 4], Pp(ANG[s % 4]), false}};
        for (int k = 0; k < m; ++k) {
            const int lado = (s + k) % 4, esq = (lado + 1) % 4;
            const double a0 = ANG[lado];
            out.push_back({C[esq], Pp(a0), true});
            for (int t = 1; t <= N; ++t) out.push_back({C[esq], Pp(a0 + t * PI / 2 / N), t == N});
        }
        out.push_back({M[(s + m) % 4], Pp(ANG[(s + m) % 4]), false});
        return out;
    };
    std::vector<std::pair<int, int>> grupos;
    if (partes == 1) grupos = {{0, 4}};
    else if (partes == 2) grupos = {{0, 2}, {2, 2}};
    else grupos = {{0, 1}, {1, 1}, {2, 1}, {3, 1}};
    int k = 1;
    for (auto g : grupos) {
        Plana p = tira(rayos(g.first, g.second));
        p.nombre = "Cuadrado a redondo " + std::to_string(k++) + "/" + std::to_string(grupos.size());
        p.nota = f0(A) + " x " + f0(B) + " a Ø " + f0(D) + " · alto " + f0(H);
        r.piezas.push_back(p);
    }
    return r;
}

// ─── 5) Reducción excéntrica (Ø D1 arriba, Ø D2 abajo, alto H, corrido e del centro de abajo) ───────────
inline Resultado reduccion(double D1, double D2, double H, double e, int N, int partes) {
    Resultado r;
    if (!(D1 > 0 && D2 > 0 && H > 0) || N < 1) { r.error = "Revise las medidas: diámetros y alto mayores que cero."; return r; }
    const int n = 4 * N;
    const double k = (PI / n) / std::sin(PI / n), R1 = D1 / 2, R2 = D2 / 2;
    auto T = [&](int i) { const double a = 2 * PI * i / n; return P3{R1 * k * std::cos(a), R1 * k * std::sin(a), H}; };
    auto Bq = [&](int i) { const double a = 2 * PI * i / n; return P3{e + R2 * k * std::cos(a), R2 * k * std::sin(a), 0}; };
    auto rayos = [&](int s, int m) {
        std::vector<Rayo> out{{T(s), Bq(s), false}};
        for (int j = s; j < s + m; ++j) { out.push_back({T(j + 1), Bq(j), true}); out.push_back({T(j + 1), Bq(j + 1), true}); }
        return out;
    };
    std::vector<std::pair<int, int>> grupos;
    if (partes == 1) grupos = {{0, n}};
    else grupos = {{0, n / 2}, {n / 2, n - n / 2}};
    int c = 1;
    for (auto g : grupos) {
        Plana p = tira(rayos(g.first, g.second));
        p.nombre = "Reducción " + std::to_string(c++) + "/" + std::to_string(grupos.size());
        p.nota = "Ø " + f0(D1) + " a Ø " + f0(D2) + " · alto " + f0(H) + " · corrido " + f0(e);
        r.piezas.push_back(p);
    }
    return r;
}

// ─── 6) Codo de gajos (tubo Ø D, ángulo del codo, radio de la curva Rc, N piezas, tramo recto) ──────────
inline Resultado codoGajos(double D, double angulo, double Rc, int N, double recto) {
    Resultado r;
    const double rr = D / 2;
    if (!(D > 0) || !(angulo > 0 && angulo <= 180) || N < 2 || N > 30 || recto < 0) { r.error = "Revise: diámetro > 0, ángulo de 1° a 180°, de 2 a 30 piezas."; return r; }
    if (Rc <= rr) { r.error = "El radio de la curva debe ser mayor que medio diámetro."; return r; }
    const double alfa = angulo * GRAD / (N - 1), b = alfa / 2;
    const int PASOS = 96;
    for (int k = 0; k < N; ++k) {
        const bool ini = k == 0, fin = k == N - 1;
        const double bAb = ini ? 0 : b, bAr = fin ? 0 : b, rAb = ini ? recto : 0, rAr = fin ? recto : 0;
        const double f0_ = (k % 2 == 1) ? 0 : PI;    // costuras alternadas (garganta, dorso…)
        std::vector<Pt> arriba, abajo;
        for (int i = 0; i <= PASOS; ++i) {
            const double fi = 2 * PI * i / PASOS, x = rr * fi, rho = Rc + rr * std::cos(fi + f0_);
            arriba.push_back({x, rAr + rho * std::tan(bAr)});
            abajo.push_back({x, -(rAb + rho * std::tan(bAb))});
        }
        std::vector<Pt> pts = abajo;
        for (int i = PASOS; i >= 0; --i) pts.push_back(arriba[i]);
        Plana p;
        p.nombre = "Codo · gajo " + std::to_string(k + 1) + "/" + std::to_string(N);
        p.cortes.push_back(poli(pts));
        p.marcas.push_back({{PI * rr, abajo[PASOS / 2].y}, {PI * rr, arriba[PASOS / 2].y}});   // línea de centro
        p.nota = "Tubo Ø " + f0(D) + " · codo " + f0(angulo) + "° · radio " + f0(Rc);
        r.piezas.push_back(p);
    }
    return r;
}

// ─── 7) Boca de pescado: ramal Ø d que entra a un principal Ø D con ángulo g y corrido e ──────────────
// L = largo del ramal desde el eje del principal; Lp > 0 dibuja el principal con su agujero, si no la
// plantilla del agujero sola.
inline Resultado bocaPescado(double D, double d, double gGrados, double e, double L, double Lp) {
    Resultado r;
    const double R = D / 2, rr = d / 2, g = gGrados * GRAD;
    if (!(D > 0 && d > 0) || !(gGrados >= 15 && gGrados <= 90)) { r.error = "Revise: diámetros > 0 y ángulo de 15° a 90°."; return r; }
    if (rr + std::fabs(e) > R + 1e-6) { r.error = "El ramal corrido se sale del principal (radio del ramal + corrido > radio del principal)."; return r; }
    const int PASOS = 120;
    struct Q { double fi, t, X, s; };
    std::vector<Q> pts;
    for (int i = 0; i <= PASOS; ++i) {
        const double fi = 2 * PI * i / PASOS, y = rr * std::sin(fi) + e, z = std::sqrt(std::max(0.0, R * R - y * y));
        const double t = (z + rr * std::cos(fi) * std::cos(g)) / std::sin(g);
        const double X = t * std::cos(g) + rr * std::cos(fi) * std::sin(g);
        pts.push_back({fi, t, X, R * std::asin(std::max(-1.0, std::min(1.0, y / R)))});
    }
    double tMax = -1e300, tMin = 1e300;
    for (auto &q : pts) { tMax = std::max(tMax, q.t); tMin = std::min(tMin, q.t); }
    if (!(L > tMax + 1)) { r.error = "El ramal debe medir más de " + f0(tMax) + " mm desde el eje del principal."; return r; }
    Plana ra;
    ra.nombre = "Boca de pescado · ramal";
    std::vector<Pt> c{{0, 0}, {PI * d, 0}};
    for (int i = PASOS; i >= 0; --i) c.push_back({rr * pts[i].fi, L - pts[i].t});
    ra.cortes.push_back(poli(c));
    ra.marcas.push_back({{PI * d / 2, 0}, {PI * d / 2, L - pts[PASOS / 2].t}});
    ra.nota = "Ramal Ø " + f0(d) + " a " + f0(gGrados) + "° en principal Ø " + f0(D);
    r.piezas.push_back(ra);
    double x0 = 1e300, x1 = -1e300, s0 = 1e300, s1 = -1e300;
    for (auto &q : pts) { x0 = std::min(x0, q.X); x1 = std::max(x1, q.X); s0 = std::min(s0, q.s); s1 = std::max(s1, q.s); }
    Plana pa;
    if (Lp > 0) {
        if (Lp < (x1 - x0) + 20) { r.error = "El principal debe medir más que el agujero (" + f0(x1 - x0) + " mm a lo largo)."; return r; }
        const double w = PI * D, cx = w / 2, cy = Lp / 2 - (x0 + x1) / 2;
        pa.nombre = "Boca de pescado · principal";
        pa.cortes.push_back(poli({{0, 0}, {w, 0}, {w, Lp}, {0, Lp}}));
        std::vector<Pt> h;
        for (int i = 0; i < PASOS; ++i) h.push_back({cx + pts[i].s, cy + pts[i].X});
        pa.cortes.push_back(poli(h));
        pa.marcas.push_back({{cx, 0}, {cx, Lp}});
        pa.nota = "Principal Ø " + f0(D) + " x " + f0(Lp) + " con el agujero";
    } else {
        pa.nombre = "Boca de pescado · plantilla del agujero";
        std::vector<Pt> h;
        for (int i = 0; i < PASOS; ++i) h.push_back({pts[i].s - s0, pts[i].X - x0});
        pa.cortes.push_back(poli(h));
        pa.marcas.push_back({{-s0, 0}, {-s0, x1 - x0}});
        pa.nota = "Plantilla para marcar el agujero sobre el tubo";
    }
    r.piezas.push_back(pa);
    return r;
}

// ─── 8) Arandela de sinfín (helicoidal): anillo abierto para formar una vuelta ──────────────────────────
// D = exterior del sinfín, d = eje, P = paso; holgura = cuánto más grande se deja el hueco del eje.
inline Resultado arandelaSinfin(double D, double d, double P, double holgura, double largoTornillo) {
    Resultado r;
    const double de = d + holgura;
    if (!(D > de && de > 0 && P > 0)) { r.error = "El exterior debe ser mayor que el eje (más la holgura), y el paso mayor que cero."; return r; }
    const double w = (D - de) / 2, Lo = std::hypot(PI * D, P), Li = std::hypot(PI * de, P);
    const double ri = w * Li / (Lo - Li), Ro = ri + w, TH = Lo / Ro;
    Plana p;
    p.nombre = "Arandela de sinfín";
    Contorno c;
    if (TH >= 2 * PI - 1e-6) { r.error = "Con estas medidas la arandela se cierra sola: revise el paso."; return r; }
    const double bu = std::tan(TH / 4);
    c.push_back({Ro, 0, bu});                                               // arco de afuera, contra el reloj
    c.push_back({Ro * std::cos(TH), Ro * std::sin(TH), 0});
    c.push_back({ri * std::cos(TH), ri * std::sin(TH), -bu});              // arco de adentro, de vuelta
    c.push_back({ri, 0, 0});
    p.cortes.push_back(c);
    if (largoTornillo > 0) p.cantidad = int(std::ceil(largoTornillo / P - 1e-9));
    p.nota = "Ø ext " + f1(2 * Ro) + " · Ø int " + f1(2 * ri) + " · abertura " + f1(360 - TH / GRAD) + "°";
    if (largoTornillo > 0) p.nota += " · cortar " + std::to_string(p.cantidad);
    r.piezas.push_back(p);
    return r;
}

// Punto bien adentro de la pieza (lejos de los bordes y fuera de los huecos), para escribirle el nombre.
// Devuelve también la distancia al borde más cercano (sirve para elegir el tamaño de la letra).
inline Pt puntoAdentro(const Plana &p, double &holgura) {
    holgura = 0;
    std::vector<std::vector<Pt>> anillos;
    for (const auto &c : p.cortes) {
        std::vector<Pt> a;
        for (size_t i = 0; i < c.size(); ++i) {
            const V v = c[i], w = c[(i + 1) % c.size()];
            a.push_back({v.x, v.y});
            if (std::fabs(v.bulge) > 1e-12) {        // arco: unos puntos intermedios
                const double th = 4 * std::atan(v.bulge), ch = std::hypot(w.x - v.x, w.y - v.y);
                const double R = ch / (2 * std::sin(std::fabs(th) / 2)), h = std::sqrt(std::max(0.0, R * R - ch * ch / 4));
                const double lado = ((v.bulge > 0) == (std::fabs(th) < PI)) ? 1.0 : -1.0;
                const double cx = (v.x + w.x) / 2 - (w.y - v.y) / ch * h * lado, cy = (v.y + w.y) / 2 + (w.x - v.x) / ch * h * lado;
                const double a1 = std::atan2(v.y - cy, v.x - cx);
                for (int k = 1; k < 24; ++k) a.push_back({cx + R * std::cos(a1 + th * k / 24), cy + R * std::sin(a1 + th * k / 24)});
            }
        }
        anillos.push_back(a);
    }
    if (anillos.empty() || anillos[0].empty()) return {};
    double x0 = 1e300, y0 = 1e300, x1 = -1e300, y1 = -1e300;
    for (Pt q : anillos[0]) { x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y); }
    auto dentro = [&](Pt q) {
        bool c = false;
        for (const auto &a : anillos)
            for (size_t i = 0, j = a.size() - 1; i < a.size(); j = i++)
                if ((a[i].y > q.y) != (a[j].y > q.y) && q.x < (a[j].x - a[i].x) * (q.y - a[i].y) / (a[j].y - a[i].y) + a[i].x) c = !c;
        return c;
    };
    auto distBorde = [&](Pt q) {
        double d = 1e300;
        for (const auto &a : anillos)
            for (size_t i = 0, j = a.size() - 1; i < a.size(); j = i++) {
                const double dx = a[i].x - a[j].x, dy = a[i].y - a[j].y, L = dx * dx + dy * dy;
                double t = L > 0 ? ((q.x - a[j].x) * dx + (q.y - a[j].y) * dy) / L : 0;
                t = std::max(0.0, std::min(1.0, t));
                d = std::min(d, std::hypot(q.x - a[j].x - t * dx, q.y - a[j].y - t * dy));
            }
        return d;
    };
    Pt mejor{(x0 + x1) / 2, (y0 + y1) / 2};
    const int n = 48;
    for (int i = 0; i <= n; ++i)
        for (int j = 0; j <= n; ++j) {
            const Pt q{x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * j / n};
            if (!dentro(q)) continue;
            const double d = distBorde(q);
            if (d > holgura) { holgura = d; mejor = q; }
        }
    return mejor;
}

// Caja (para poner las piezas una al lado de la otra).
inline void caja(const Plana &p, double &x0, double &y0, double &x1, double &y1) {
    x0 = y0 = 1e300; x1 = y1 = -1e300;
    for (const auto &c : p.cortes)
        for (size_t i = 0; i < c.size(); ++i) {
            const V a = c[i], b = c[(i + 1) % c.size()];
            x0 = std::min(x0, a.x); y0 = std::min(y0, a.y); x1 = std::max(x1, a.x); y1 = std::max(y1, a.y);
            if (std::fabs(a.bulge) > 1e-12) {     // el arco puede salirse de la caja de sus extremos
                const double th = 4 * std::atan(a.bulge), ch = std::hypot(b.x - a.x, b.y - a.y);
                const double rad = ch / (2 * std::sin(std::fabs(th) / 2));
                for (int k = 1; k < 16; ++k) {   // muestrear el arco
                    const double t = k / 16.0;
                    const double mx = (a.x + b.x) / 2, my = (a.y + b.y) / 2;
                    const double nx = -(b.y - a.y) / ch, ny = (b.x - a.x) / ch;
                    const double h = std::sqrt(std::max(0.0, rad * rad - ch * ch / 4));
                    const double lado = ((a.bulge > 0) == (std::fabs(th) < PI)) ? 1.0 : -1.0;
                    const double cx = mx + nx * h * lado, cy = my + ny * h * lado;
                    const double a1 = std::atan2(a.y - cy, a.x - cx);
                    const double ang = a1 + th * t;
                    const double px = cx + rad * std::cos(ang), py = cy + rad * std::sin(ang);
                    x0 = std::min(x0, px); y0 = std::min(y0, py); x1 = std::max(x1, px); y1 = std::max(y1, py);
                }
            }
        }
}

} // namespace piezas
