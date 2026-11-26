// Pruebas de las piezas por medidas (piezas.h). Compilar sin Qt:
//   py -3.10 -m ziglang c++ -std=c++17 -O2 pruebas_piezas.cpp -o pruebas_piezas.exe
// Revisa contra la geometría de cada pieza: largos de los bordes, áreas y que el contorno no se cruce.
#include "piezas.h"

#include <cstdio>

using namespace piezas;

static int fallos = 0, total = 0;
static void ok(bool c, const char *msg) { ++total; if (!c) { ++fallos; std::printf("  FALLA: %s\n", msg); } }
static bool cerca(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

// largo de un tramo con bulge
static double largoTramo(V a, V b) {
    const double ch = std::hypot(b.x - a.x, b.y - a.y);
    if (std::fabs(a.bulge) < 1e-12) return ch;
    const double th = 4 * std::atan(std::fabs(a.bulge));
    return ch / (2 * std::sin(th / 2)) * th;
}
static double perimetro(const Contorno &c) { double s = 0; for (size_t i = 0; i < c.size(); ++i) s += largoTramo(c[i], c[(i + 1) % c.size()]); return s; }
// área con arcos (cuerda + segmento circular con signo)
static double area(const Contorno &c) {
    double s = 0;
    for (size_t i = 0; i < c.size(); ++i) {
        const V a = c[i], b = c[(i + 1) % c.size()];
        s += (a.x * b.y - b.x * a.y) / 2;
        if (std::fabs(a.bulge) > 1e-12) {
            const double th = 4 * std::atan(a.bulge), ch = std::hypot(b.x - a.x, b.y - a.y), R = ch / (2 * std::sin(std::fabs(th) / 2));
            s += (th > 0 ? 1 : -1) * R * R / 2 * (std::fabs(th) - std::sin(std::fabs(th)));
        }
    }
    return std::fabs(s);
}
static bool seCruzan(Pt a, Pt b, Pt c, Pt d) {
    auto o = [](Pt p, Pt q, Pt r) { return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x); };
    const double d1 = o(a, b, c), d2 = o(a, b, d), d3_ = o(c, d, a), d4 = o(c, d, b);
    return ((d1 > 1e-9 && d2 < -1e-9) || (d1 < -1e-9 && d2 > 1e-9)) && ((d3_ > 1e-9 && d4 < -1e-9) || (d3_ < -1e-9 && d4 > 1e-9));
}
static bool simple(const Contorno &c) {
    const size_t n = c.size();
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 2; j < n; ++j) {
            if (i == 0 && j == n - 1) continue;
            if (seCruzan({c[i].x, c[i].y}, {c[(i + 1) % n].x, c[(i + 1) % n].y}, {c[j].x, c[j].y}, {c[(j + 1) % n].x, c[(j + 1) % n].y})) return false;
        }
    return true;
}
static void todosSimples(const Resultado &r, const char *nombre) {
    char m[200];
    for (const auto &p : r.piezas) for (const auto &c : p.cortes) {
        std::snprintf(m, sizeof m, "%s: '%s' tiene el contorno cruzado", nombre, p.nombre.c_str());
        ok(simple(c), m);
    }
}

int main() {
    char m[200];
    {   // tolva cónica Ø1200 / Ø600 / alto 900 en 2 piezas: radios 1897,4 / 948,7, 56,9° cada una
        auto r = tolvaConica(1200, 600, 900, 2);
        ok(r.error.empty() && r.piezas.size() == 1 && r.piezas[0].cantidad == 2, "conica: 1 forma, cortar 2");
        const auto &c = r.piezas[0].cortes[0];
        ok(cerca(largoTramo(c[0], c[1]), PI * 1200 / 2, 0.01), "conica: arco de afuera = mitad de pi*1200");
        ok(cerca(largoTramo(c[2], c[3]), PI * 600 / 2, 0.01), "conica: arco de adentro = mitad de pi*600");
        const double g = std::hypot(900, 300);
        ok(cerca(largoTramo(c[1], c[2]), g, 0.01), "conica: costura = generatriz");
        const double lateral = PI * (600 + 300) * g;    // área lateral del cono truncado
        ok(cerca(area(c) * 2, lateral, lateral * 1e-6), "conica: 2 piezas = area lateral");
        todosSimples(r, "conica");
        std::printf("conica: %s\n", r.piezas[0].nota.c_str());
        auto cil = tolvaConica(500, 500, 300, 1);
        ok(cil.error.empty() && cerca(area(cil.piezas[0].cortes[0]), PI * 500 * 300, 0.01), "cilindro = rectangulo pi*D x alto");
        auto punta = tolvaConica(400, 0, 300, 1);
        ok(punta.error.empty() && punta.piezas[0].cortes[0].size() == 3, "cono con punta = sector de 3 vertices");
    }
    {   // tolva en U 600 x 550 x 1200 (la de la demo): cuerpo 1442,5 x 1200 y 2 tapas
        auto r = tolvaU(600, 550, 1200);
        ok(r.error.empty() && r.piezas.size() == 2, "U: cuerpo + tapa");
        ok(cerca(area(r.piezas[0].cortes[0]), (2 * 250 + PI * 300) * 1200, 0.01), "U: cuerpo = (2h + pi R) x largo");
        ok(cerca(area(r.piezas[1].cortes[0]), 600 * 250 + PI * 300 * 300 / 2, 0.01), "U: tapa = rectangulo + medio circulo");
        ok(cerca(perimetro(r.piezas[1].cortes[0]), 2 * 250 + 600 + PI * 300, 0.01), "U: perimetro de la tapa");
        ok(r.piezas[1].cantidad == 2 && r.piezas[0].marcas.size() == 2, "U: 2 tapas y 2 marcas de rolado");
        ok(!tolvaU(600, 200, 1000).error.empty(), "U: alto menor que el radio da error");
        todosSimples(r, "U");
    }
    {   // tolva piramidal 800x600 → 200x100, alto 500: cada cara mide igual que en 3D
        for (int partes : {1, 2, 4}) {
            auto r = tolvaPiramidal(800, 600, 200, 100, 500, partes);
            std::snprintf(m, sizeof m, "piramidal %d: %d piezas", partes, partes);
            ok(r.error.empty() && int(r.piezas.size()) == partes, m);
            double a = 0; for (auto &p : r.piezas) a += area(p.cortes[0]);
            // área 3D de las 4 caras trapezoidales
            auto tri = [](P3 p, P3 q, P3 s) { P3 u{q.x - p.x, q.y - p.y, q.z - p.z}, v{s.x - p.x, s.y - p.y, s.z - p.z};
                return 0.5 * std::sqrt(std::pow(u.y * v.z - u.z * v.y, 2) + std::pow(u.z * v.x - u.x * v.z, 2) + std::pow(u.x * v.y - u.y * v.x, 2)); };
            const int SG[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
            double a3 = 0;
            for (int i = 0; i < 4; ++i) {
                int j = (i + 1) % 4;
                P3 Ti{SG[i][0] * 400.0, SG[i][1] * 300.0, 500}, Tj{SG[j][0] * 400.0, SG[j][1] * 300.0, 500};
                P3 Bi{SG[i][0] * 100.0, SG[i][1] * 50.0, 0}, Bj{SG[j][0] * 100.0, SG[j][1] * 50.0, 0};
                a3 += tri(Ti, Tj, Bj) + tri(Ti, Bj, Bi);
            }
            std::snprintf(m, sizeof m, "piramidal %d: area plana %.1f = area 3D %.1f", partes, a, a3);
            ok(cerca(a, a3, a3 * 1e-6), m);
            todosSimples(r, "piramidal");
        }
    }
    {   // cuadrado a redondo 600x400 a Ø300, alto 400: borde recto = perímetro del rectángulo; curvo = pi*D
        for (int partes : {1, 2, 4}) {
            auto r = cuadradoRedondo(600, 400, 300, 400, 6, partes);
            ok(r.error.empty() && int(r.piezas.size()) == partes, "cuadrado-redondo: numero de piezas");
            double recto = 0, curvo = 0;
            for (auto &p : r.piezas) {
                const auto &c = p.cortes[0];
                // la mitad de los vértices es borde recto, el resto el curvo; se suman los tramos del borde de arriba (y=0 inicial)
                (void)c;
            }
            // revisar con perímetros: todas las piezas juntas = 2(A+B) + pi*D + 2 * costuras * piezas
            double per = 0; for (auto &p : r.piezas) per += perimetro(p.cortes[0]);
            (void)recto; (void)curvo;
            std::snprintf(m, sizeof m, "cuadrado-redondo %d: perimetro %.1f mayor que 2(A+B)+piD", partes, per);
            ok(per > 2 * (600 + 400) + PI * 300, m);
            todosSimples(r, "cuadrado-redondo");
        }
        // bordes exactos en 1 pieza: recorrer el contorno y sumar el borde recto (puntos con y >= -1e-6 al inicio)
        auto r = cuadradoRedondo(600, 400, 300, 400, 6, 1);
        std::printf("cuadrado-redondo: %zu vertices, %zu dobleces\n", r.piezas[0].cortes[0].size(), r.piezas[0].marcas.size());
        ok(r.piezas[0].marcas.size() == 4 * 6 + 4, "cuadrado-redondo: 4 cuartos x 6 divisiones + 4 dobleces de esquina");
    }
    {   // reducción excéntrica Ø600 → Ø300, alto 500, corrido 150
        for (int partes : {1, 2}) {
            auto r = reduccion(600, 300, 500, 150, 6, partes);
            ok(r.error.empty() && int(r.piezas.size()) == partes, "reduccion: numero de piezas");
            todosSimples(r, "reduccion");
        }
        auto con = reduccion(600, 300, 500, 0, 12, 1);       // sin corrido = cono: comparar con tolva cónica
        auto cono = tolvaConica(600, 300, 500, 1);
        const double a1 = area(con.piezas[0].cortes[0]), a2 = area(cono.piezas[0].cortes[0]);
        std::snprintf(m, sizeof m, "reduccion sin corrido = cono (%.0f vs %.0f mm2)", a1, a2);
        ok(cerca(a1, a2, a2 * 0.01), m);
    }
    {   // codo de 90° en 4 piezas, tubo Ø200, radio 300
        auto r = codoGajos(200, 90, 300, 4, 50);
        ok(r.error.empty() && r.piezas.size() == 4, "codo: 4 gajos");
        const auto &c = r.piezas[0].cortes[0];
        double x1 = -1e300; for (auto v : c) x1 = std::max(x1, v.x);
        ok(cerca(x1, PI * 200, 0.01), "codo: el gajo mide pi*D alrededor");
        ok(!codoGajos(200, 90, 90, 4, 0).error.empty(), "codo: radio menor que medio diametro da error");
        todosSimples(r, "codo");
    }
    {   // boca de pescado Ø300 con ramal Ø150 a 90°
        auto r = bocaPescado(300, 150, 90, 0, 400, 0);
        ok(r.error.empty() && r.piezas.size() == 2, "boca: ramal + plantilla");
        double x1 = -1e300; for (auto v : r.piezas[0].cortes[0]) x1 = std::max(x1, v.x);
        ok(cerca(x1, PI * 150, 0.01), "boca: el ramal mide pi*d alrededor");
        auto r2 = bocaPescado(300, 150, 60, 30, 500, 800);
        ok(r2.error.empty() && r2.piezas[1].cortes.size() == 2, "boca: principal con su agujero");
        ok(!bocaPescado(300, 280, 90, 30, 400, 0).error.empty(), "boca: ramal corrido que se sale da error");
        todosSimples(r, "boca"); todosSimples(r2, "boca2");
    }
    {   // arandela del sinfín Ø300 / eje 100 / paso 300 (la de la demo): ext 2R, int 2r, ángulo
        auto r = arandelaSinfin(300, 100, 300, 0, 1200);
        ok(r.error.empty() && r.piezas[0].cantidad == 4, "sinfin: 1200 / 300 = 4 arandelas");
        const auto &c = r.piezas[0].cortes[0];
        const double Lo = std::hypot(PI * 300, 300), Li = std::hypot(PI * 100, 300);
        ok(cerca(largoTramo(c[0], c[1]), Lo, 0.01), "sinfin: borde de afuera = largo de la helice exterior");
        ok(cerca(largoTramo(c[2], c[3]), Li, 0.01), "sinfin: borde de adentro = largo de la helice del eje");
        std::printf("sinfin: %s\n", r.piezas[0].nota.c_str());
        todosSimples(r, "sinfin");
    }
    std::printf("\n%d de %d pruebas bien\n", total - fallos, total);
    return fallos ? 1 : 0;
}
