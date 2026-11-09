// Pruebas del generador de código G (cam.h), sin Qt. Compilar y correr:
//   py -3.10 -m ziglang c++ -std=c++17 -O1 plugins/edwincnc/pruebas_cam.cpp -o pruebas_cam.exe && pruebas_cam.exe
// Revisa que el código G sea continuo (cada movimiento arranca donde terminó el anterior), que los arcos sean
// arcos de verdad (inicio y fin a la misma distancia del centro), y que el recorrido quede a la mitad del corte
// de la pieza (por fuera en el contorno de afuera, por dentro en los huecos).
#include "cam.h"

#include <cstdio>
#include <sstream>

using namespace cam;

static int oks = 0, males = 0;
static void ver(bool c, const std::string &t) {
    std::printf("  %s %s\n", c ? "OK " : "MAL", t.c_str());
    (c ? oks : males)++;
}

// Lee el código G y revisa continuidad y arcos. Devuelve los puntos de corte (muestreados) para medirlos.
struct Lectura {
    bool continuo = true, arcosBien = true;
    int m03 = 0, m05 = 0, g04 = 0;
    std::vector<P> corte;
    std::vector<std::string> lineas;
};
static double valor(const std::string &l, char k, double def) {
    const auto p = l.find(std::string(" ") + k);
    if (p == std::string::npos) return def;
    return std::stod(l.substr(p + 2));
}
static Lectura leer(const std::string &g) {
    Lectura r;
    std::istringstream in(g);
    std::string l;
    P pos{0, 0};
    bool antorcha = false;
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        r.lineas.push_back(l);
        if (l.find(" M03") != std::string::npos) { ++r.m03; antorcha = true; }
        if (l.find(" M05") != std::string::npos) { ++r.m05; antorcha = false; }
        if (l.find(" G04") != std::string::npos) ++r.g04;
        const bool g0 = l.find(" G00") != std::string::npos, g1 = l.find(" G01") != std::string::npos;
        const bool g2 = l.find(" G02") != std::string::npos, g3 = l.find(" G03") != std::string::npos;
        if (!(g0 || g1 || g2 || g3)) continue;
        const P fin{valor(l, 'X', pos.x), valor(l, 'Y', pos.y)};
        if (g2 || g3) {
            const P c{pos.x + valor(l, 'I', 0), pos.y + valor(l, 'J', 0)};
            if (std::fabs(dist(pos, c) - dist(fin, c)) > 0.002) r.arcosBien = false;
            Seg s{true, pos, fin, c, g3};
            if (antorcha) for (int k = 0; k <= 20; ++k) r.corte.push_back(puntoEn(s, k / 20.0));
        } else if (antorcha && g1) {
            for (int k = 0; k <= 20; ++k) r.corte.push_back(pos + (fin - pos) * (k / 20.0));
        }
        pos = fin;
    }
    return r;
}

int main() {
    // ─── 1. La elipse del .tap de SheetCAM (centro 25,15; ejes 25 y 15; sin separación ni colitas) ───
    std::printf("Elipse (comparada con edwin/referencia/ELIPSE-sheetcam.TAP)\n");
    {
        Entrada e;
        agregarElipse(e, {25, 15}, {25, 0}, 0.6, 0, 2 * PI);
        Parametros prm;
        prm.kerf = 0;
        prm.entradaTipo = prm.salidaTipo = 0;
        prm.nombreArchivo = "ELIPSE.TAP";
        prm.nombrePieza = "Elipse";
        const Resultado r = preparar(e, prm);
        const std::string g = codigoG(r, prm);
        std::printf("%s", g.c_str());
        const Lectura L = leer(g);
        ver(r.cortes.size() == 1 && !r.cortes[0].hueco, "un solo contorno, de afuera");
        ver(L.lineas[0] == "N0010 (Filename: ELIPSE.TAP)" && L.lineas[2] == "N0030 G21 (Units: Metric)" && L.lineas[3] == "N0040 G90 G91.1" && L.lineas[4] == "N0050 F1",
            "encabezado igual al de SheetCAM (N0010, G21, G90 G91.1, F1)");
        ver(L.lineas[7] == "N0080 G00 X25.0000 Y30.0000", "arranca arriba, en X25 Y30, como SheetCAM: " + L.lineas[7]);
        ver(L.lineas[8] == "N0090 M03" && L.lineas[9] == "N0100 G04 P0.5", "enciende (M03) y espera 0,5 s para perforar (G04 P0.5)");
        ver(L.lineas[10].find(" G02 ") != std::string::npos && L.lineas[10].find("F2500.0") != std::string::npos, "corta a favor del reloj (G02) a F2500.0: " + L.lineas[10]);
        ver(L.lineas[L.lineas.size() - 2].find("G00 X0.0000 Y0.0000") != std::string::npos && L.lineas.back().find("M05 M30") != std::string::npos, "termina como SheetCAM: G00 X0 Y0 y M05 M30");
        ver(L.continuo && L.arcosBien, "arcos bien formados");
        double peor = 0;
        for (P p : L.corte) {
            // Distancia aproximada a la elipse: (x-25)²/25² + (y-15)²/15² = 1.
            const double v = std::pow((p.x - 25) / 25, 2) + std::pow((p.y - 15) / 15, 2);
            peor = std::max(peor, std::fabs(std::sqrt(v) - 1) * 15);
        }
        ver(peor < 0.02, "el recorrido sigue la elipse (desvío máximo " + std::to_string(peor) + " mm)");
    }

    // ─── 2. Lámina 100×100 con hueco redondo r10, separación 2 mm (el recorrido se corre 1 mm) ───
    std::printf("\nLámina cuadrada con hueco redondo, separación de la silueta 2 mm, colitas rectas\n");
    {
        Entrada e;
        agregarPolilinea(e, {{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {0, 0, 0, 0}, true);
        agregarCirculo(e, {50, 50}, 10);
        Parametros prm;
        prm.kerf = 2;
        const Resultado r = preparar(e, prm);
        const std::string g = codigoG(r, prm);
        std::printf("%s", g.c_str());
        const Lectura L = leer(g);
        ver(r.cortes.size() == 2, "dos contornos");
        ver(r.cortes.size() == 2 && r.cortes[0].hueco && !r.cortes[1].hueco, "primero el hueco, después el contorno de afuera");
        ver(L.m03 == 2 && L.m05 == 3 && L.g04 == 2, "enciende y apaga una vez por contorno");
        ver(L.arcosBien, "arcos bien formados");
        // El hueco: recorrido a radio 9 (por dentro); contorno de afuera: a 1 mm por fuera del cuadrado.
        const Contorno &h = r.cortes[0].camino;
        double peorH = 0;
        for (const Seg &s : h) for (int k = 0; k <= 10; ++k) peorH = std::max(peorH, std::fabs(dist(puntoEn(s, k / 10.0), {50, 50}) - 9));
        ver(peorH < 1e-6, "el hueco se corta a radio 9 (1 mm por dentro)");
        double peorA = 0;
        for (const Seg &s : r.cortes[1].camino)
            for (int k = 0; k <= 10; ++k) {
                const P p = puntoEn(s, k / 10.0);
                const double dx = std::max({0.0 - p.x, 0.0, p.x - 100}), dy = std::max({0.0 - p.y, 0.0, p.y - 100});
                peorA = std::max(peorA, std::fabs(std::hypot(dx, dy) - 1));
            }
        ver(peorA < 1e-6, "el contorno de afuera va a 1 mm por fuera, con esquinas redondas");
        ver(h.size() >= 2 && h.front().ccw, "el hueco va contra el reloj (G03)");
        bool g02afuera = false;
        for (const Seg &s : r.cortes[1].camino) if (s.arco && !s.ccw) g02afuera = true;
        ver(g02afuera, "el contorno de afuera va a favor del reloj (G02 en las esquinas)");
        // Colita del hueco: arranca adentro del hueco (lado del desperdicio).
        const P ent = r.cortes[0].entrada.front().a;
        ver(dist(ent, {50, 50}) < 9, "la colita de entrada del hueco arranca dentro del hueco");
        const P entA = r.cortes[1].entrada.front().a;
        ver(entA.x < -1 || entA.x > 101 || entA.y < -1 || entA.y > 101, "la colita del contorno de afuera arranca por fuera de la pieza");
    }

    // ─── 3. Rectángulo hecho con 4 líneas sueltas (así llega muchas veces el DXF) + colita curva ───
    std::printf("\nRectángulo de 4 líneas sueltas, colita curva, desaceleración en los últimos 5 mm\n");
    {
        Entrada e;
        agregarLinea(e, {0, 0}, {80, 0});
        agregarLinea(e, {80, 40}, {80, 0});  // una al revés
        agregarLinea(e, {80, 40}, {0, 40});
        agregarLinea(e, {0, 0.02}, {0, 40});  // con un hueco diminuto de 0,02 mm
        agregarLinea(e, {200, 200}, {210, 200});  // una suelta: no cierra
        Parametros prm;
        prm.kerf = 1.5;
        prm.entradaTipo = prm.salidaTipo = 2;
        prm.desacelerarMm = 5;
        prm.desacelerarPct = 60;
        const Resultado r = preparar(e, prm);
        const std::string g = codigoG(r, prm);
        std::printf("%s", g.c_str());
        const Lectura L = leer(g);
        ver(r.cortes.size() == 1 && r.abiertos == 1, "une las 4 líneas en un contorno y avisa la línea suelta");
        ver(L.arcosBien, "arcos bien formados");
        ver(g.find("F1500.0") != std::string::npos, "baja a 1500 mm/min (60 %) al final");
        double peor = 0;
        for (const Seg &s : r.cortes[0].camino)
            for (int k = 0; k <= 10; ++k) {
                const P p = puntoEn(s, k / 10.0);
                const double dx = std::max({0.0 - p.x, 0.0, p.x - 80}), dy = std::max({0.0 - p.y, 0.0, p.y - 40});
                peor = std::max(peor, std::fabs(std::hypot(dx, dy) - 0.75));
            }
        ver(peor < 0.03, "recorrido a 0,75 mm por fuera (desvío " + std::to_string(peor) + ")");
    }

    // ─── 4. Pieza con hueco cuadrado (esquinas que se cierran) y una pieza adentro del hueco ───
    std::printf("\nMarco con hueco cuadrado y una arandela dentro del hueco\n");
    {
        Entrada e;
        agregarPolilinea(e, {{0, 0}, {120, 0}, {120, 120}, {0, 120}}, {0, 0, 0, 0}, true);
        agregarPolilinea(e, {{20, 20}, {100, 20}, {100, 100}, {20, 100}}, {0, 0, 0, 0}, true);
        agregarCirculo(e, {60, 60}, 15);  // pieza dentro del hueco
        agregarCirculo(e, {60, 60}, 5);   // su hueco
        Parametros prm;
        prm.kerf = 2;
        const Resultado r = preparar(e, prm);
        const std::string g = codigoG(r, prm);
        const Lectura L = leer(g);
        ver(r.cortes.size() == 4, "cuatro contornos");
        std::vector<int> prof;
        for (const Corte &k : r.cortes) prof.push_back(k.profundidad);
        ver(prof == std::vector<int>({3, 2, 1, 0}), "orden de adentro hacia afuera: hueco de la arandela, arandela, hueco del marco, marco");
        ver(L.arcosBien, "arcos bien formados");
        double peor = 0;
        for (const Seg &s : r.cortes[2].camino)
            for (int k = 0; k <= 10; ++k) {
                const P p = puntoEn(s, k / 10.0);
                const double dentro = std::min({p.x - 20, 100 - p.x, p.y - 20, 100 - p.y});
                peor = std::max(peor, std::fabs(dentro - 1));
            }
        ver(peor < 1e-6, "el hueco cuadrado se corta 1 mm por dentro, con esquinas vivas");
    }

    // ─── 5. Lado del corte elegido a mano (lámina 100×100 con hueco r10, separación 2 mm) ───
    std::printf("\nLado del corte: todo por fuera, todo por dentro, sobre la línea\n");
    {
        auto medir = [](int lado, double &cuadrado, double &radioHueco, bool &continuo) {
            Entrada e;
            agregarPolilinea(e, {{0, 0}, {100, 0}, {100, 100}, {0, 100}}, {0, 0, 0, 0}, true);
            agregarCirculo(e, {50, 50}, 10);
            Parametros prm;
            prm.kerf = 2;
            prm.lado = lado;
            const Resultado r = preparar(e, prm);
            continuo = leer(codigoG(r, prm)).continuo && r.cortes.size() == 2;
            cuadrado = radioHueco = 0;
            if (r.cortes.size() != 2) return;
            // Distancia (con signo) del recorrido del cuadrado al borde: + afuera, - adentro (en el medio del lado de abajo).
            for (const Seg &s : r.cortes[1].camino)
                for (int k = 0; k <= 10; ++k) {
                    const P p = puntoEn(s, k / 10.0);
                    if (std::fabs(p.x - 50) < 5 && p.y < 50) cuadrado = -p.y;
                }
            radioHueco = dist(r.cortes[0].camino.front().a, {50, 50});
        };
        double q, h;
        bool c;
        medir(1, q, h, c);
        ver(c && std::fabs(q - 1) < 1e-6 && std::fabs(h - 11) < 1e-6, "todo por fuera: cuadrado 1 mm afuera y hueco a radio 11");
        medir(2, q, h, c);
        ver(c && std::fabs(q + 1) < 1e-6 && std::fabs(h - 9) < 1e-6, "todo por dentro: cuadrado 1 mm adentro y hueco a radio 9");
        medir(3, q, h, c);
        ver(c && std::fabs(q) < 1e-6 && std::fabs(h - 10) < 1e-6, "sobre la línea: sin separación");
        medir(0, q, h, c);
        ver(c && std::fabs(q - 1) < 1e-6 && std::fabs(h - 9) < 1e-6, "automático: igual que antes");
    }

    std::printf("\n%d OK, %d MAL\n", oks, males);
    return males ? 1 : 0;
}
