// Pruebas del acomodo de piezas (nesting.h). Compilar sin Qt:
//   py -3.10 -m ziglang c++ -std=c++17 -O2 pruebas_nesting.cpp -o pruebas_nesting.exe
// Revisa con la geometría REAL (no con la cuadrícula) que ninguna pieza se monte sobre otra, que respeten
// la separación y el margen, y que no se pierda ninguna.
#include "nesting.h"

#include <chrono>
#include <cstdio>

using namespace nesting;

static int fallos = 0, total = 0;
static void ok(bool c, const char *msg) {
    ++total;
    if (!c) { ++fallos; std::printf("  FALLA: %s\n", msg); }
}

static Poligono circulo(double cx, double cy, double r, int n = 72) {
    Poligono p;
    for (int i = 0; i < n; ++i) p.push_back({cx + r * std::cos(2 * PI * i / n), cy + r * std::sin(2 * PI * i / n)});
    return p;
}
static Poligono rect(double x0, double y0, double w, double h) { return {{x0, y0}, {x0 + w, y0}, {x0 + w, y0 + h}, {x0, y0 + h}}; }
static Poligono abanico(double R, double r, double th, int n = 40) {
    Poligono p;
    for (int i = 0; i <= n; ++i) { double a = -th / 2 + th * i / n; p.push_back({R * std::sin(a), R * std::cos(a)}); }
    for (int i = n; i >= 0; --i) { double a = -th / 2 + th * i / n; p.push_back({r * std::sin(a), r * std::cos(a)}); }
    return p;
}

static Poligono mover(const Poligono &p, const Colocacion &c) {
    Poligono q;
    for (Pt v : p) { Pt g = girar(v, c.angulo); q.push_back({g.x + c.dx, g.y + c.dy}); }
    return q;
}
static double distSeg(Pt p, Pt a, Pt b) {
    double dx = b.x - a.x, dy = b.y - a.y, L = dx * dx + dy * dy, t = L > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / L : 0;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(p.x - a.x - t * dx, p.y - a.y - t * dy);
}
static bool dentroPol(Pt p, const Poligono &q) {
    bool c = false;
    for (size_t i = 0, j = q.size() - 1; i < q.size(); j = i++)
        if ((q[i].y > p.y) != (q[j].y > p.y) && p.x < (q[j].x - q[i].x) * (p.y - q[i].y) / (q[j].y - q[i].y) + q[i].x) c = !c;
    return c;
}
// distancia entre bordes (0 si se cruzan)
static double distPol(const Poligono &a, const Poligono &b) {
    double d = 1e300;
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j) {
            d = std::min(d, distSeg(a[i], b[j], b[(j + 1) % b.size()]));
            d = std::min(d, distSeg(b[j], a[i], a[(i + 1) % a.size()]));
        }
    return d;
}
// ¿la pieza A (afuera) se monta sobre la pieza B (afuera + huecos)?
static bool montadas(const Pieza &A, const Colocacion &ca, const Pieza &B, const Colocacion &cb, double sep, double &dmin) {
    Poligono a = mover(A.afuera, ca), b = mover(B.afuera, cb);
    dmin = distPol(a, b);
    bool aEnB = dentroPol(a[0], b), bEnA = dentroPol(b[0], a);
    if (aEnB) {  // A está dentro del contorno de B: solo vale si cae en un hueco de B, con separación
        for (const auto &h : B.huecos) {
            Poligono hh = mover(h, cb);
            if (dentroPol(a[0], hh)) { dmin = distPol(a, hh); return dmin < sep - 1e-6; }
        }
        return true;
    }
    if (bEnA) {
        for (const auto &h : A.huecos) {
            Poligono hh = mover(h, ca);
            if (dentroPol(b[0], hh)) { dmin = distPol(b, hh); return dmin < sep - 1e-6; }
        }
        return true;
    }
    return dmin < sep - 1e-6;
}

static void revisar(const char *nombre, const std::vector<Pieza> &ps, const Opciones &op, const Resultado &r) {
    int esperadas = 0;
    for (size_t i = 0; i < ps.size(); ++i) {
        bool nc = std::find(r.noCaben.begin(), r.noCaben.end(), int(i)) != r.noCaben.end();
        if (!nc) esperadas += ps[i].cantidad;
    }
    char m[256];
    std::snprintf(m, sizeof m, "%s: se colocaron %zu de %d", nombre, r.colocadas.size(), esperadas);
    ok(int(r.colocadas.size()) == esperadas, m);
    double peor = 1e300;
    for (size_t i = 0; i < r.colocadas.size(); ++i) {
        const auto &ci = r.colocadas[i];
        Poligono a = mover(ps[ci.pieza].afuera, ci);
        for (Pt v : a) {
            if (v.x < op.margen - 1e-6 || v.y < op.margen - 1e-6 || v.x > op.ancho - op.margen + 1e-6 || v.y > op.alto - op.margen + 1e-6) {
                std::snprintf(m, sizeof m, "%s: pieza %d fuera del margen (%.2f, %.2f)", nombre, ci.pieza, v.x, v.y);
                ok(false, m); break;
            }
        }
        for (size_t j = i + 1; j < r.colocadas.size(); ++j) {
            const auto &cj = r.colocadas[j];
            if (ci.lamina != cj.lamina) continue;
            double d;
            if (montadas(ps[ci.pieza], ci, ps[cj.pieza], cj, op.separacion, d)) {
                std::snprintf(m, sizeof m, "%s: piezas %zu y %zu muy juntas o montadas (%.2f mm)", nombre, i, j, d);
                ok(false, m);
            }
            peor = std::min(peor, d);
        }
    }
    double uso = 0; for (double u : r.usoPorLamina) uso += u;
    std::printf("%s: %zu piezas en %d lamina(s), celda %.2f mm, uso medio %.1f %%, menor separacion %.2f mm\n", nombre,
                r.colocadas.size(), r.laminas, r.celda, r.laminas ? 100 * uso / r.laminas : 0.0, peor > 1e200 ? 0.0 : peor);
}

int main() {
    Opciones op;   // 2440 x 1220, margen 10, separación 6

    {   // 1) diez rectángulos de 500 x 300: caben en una sola lámina
        std::vector<Pieza> ps(1);
        ps[0].afuera = rect(0, 0, 500, 300); ps[0].cantidad = 10;
        auto r = acomodar(ps, op);
        revisar("rectangulos", ps, op, r);
        ok(r.laminas == 1, "10 rectangulos 500x300 deben caber en 1 lamina");
    }
    {   // 2) un aro con hueco grande y un disco que solo cabe en el hueco (lámina baja y angosta)
        Opciones o = op; o.ancho = 700; o.alto = 640;
        std::vector<Pieza> ps(2);
        ps[0].afuera = circulo(0, 0, 300); ps[0].huecos.push_back(circulo(0, 0, 200));
        ps[1].afuera = circulo(0, 0, 120);
        auto r = acomodar(ps, o);
        revisar("aro con disco en el hueco", ps, o, r);
        ok(r.laminas == 1 && r.colocadas.size() == 2, "el disco debe caer dentro del hueco del aro (1 lamina)");
    }
    {   // 3) medias piezas de la tolva cónica (abanicos) — 2 por lámina como en la calculadora
        std::vector<Pieza> ps(1);
        ps[0].afuera = abanico(1897.4, 948.7, 0.9933); ps[0].cantidad = 4;
        auto r = acomodar(ps, op);
        revisar("abanicos", ps, op, r);
        ok(r.laminas <= 4, "4 medias piezas de tolva no deben pasar de 4 laminas");
    }
    {   // 4) pieza que no cabe
        std::vector<Pieza> ps(2);
        ps[0].afuera = rect(0, 0, 3000, 1500);
        ps[1].afuera = rect(0, 0, 100, 100); ps[1].cantidad = 3;
        auto r = acomodar(ps, op);
        revisar("no cabe", ps, op, r);
        ok(r.noCaben.size() == 1 && r.noCaben[0] == 0, "la pieza de 3000x1500 debe salir en 'no caben'");
    }
    {   // 5) mezcla grande (velocidad): 30 discos, 20 rectángulos, 10 bridas con huecos, 8 abanicos chicos
        std::vector<Pieza> ps(4);
        ps[0].afuera = circulo(0, 0, 90); ps[0].cantidad = 30;
        ps[1].afuera = rect(0, 0, 260, 140); ps[1].cantidad = 20;
        ps[2].afuera = circulo(0, 0, 160); ps[2].huecos.push_back(circulo(0, 0, 70));
        for (int k = 0; k < 4; ++k) ps[2].huecos.push_back(circulo(115 * std::cos(k * PI / 2), 115 * std::sin(k * PI / 2), 9, 24));
        ps[2].cantidad = 10;
        ps[3].afuera = abanico(700, 350, 1.0); ps[3].cantidad = 8;
        auto t0 = std::chrono::steady_clock::now();
        auto r = acomodar(ps, op);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        revisar("mezcla", ps, op, r);
        std::printf("  tiempo: %.2f s\n", s);
        ok(s < 20, "la mezcla debe acomodarse en menos de 20 s");
    }
    {   // 6) sin girar: piezas largas de 2300 x 100 caben acostadas
        Opciones o = op; o.giros = 1;
        std::vector<Pieza> ps(1);
        ps[0].afuera = rect(0, 0, 2300, 100); ps[0].cantidad = 5;
        auto r = acomodar(ps, o);
        revisar("largas sin girar", ps, o, r);
        ok(r.laminas == 1, "5 piezas de 2300x100 caben en 1 lamina sin girar");
    }
    std::printf("\n%d de %d pruebas bien\n", total - fallos, total);
    return fallos ? 1 : 0;
}
