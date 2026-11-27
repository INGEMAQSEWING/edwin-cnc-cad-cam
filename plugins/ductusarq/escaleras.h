#pragma once
// Escalera recta en planta y su cálculo (sin Qt; se prueba en pruebas_arq.cpp).
// Colombia (NSR-10 K.3.8.3.4): contrahuella de 100 a 180 mm, huella de 280 mm o más y
// 2CH + H entre 620 y 640 mm. EE. UU. (IRC R311.7.5): contrahuella de 7 3/4" o menos y
// huella de 10" o más. Ancho mínimo: 0,90 m (NSR-10, uso general) / 36" (IRC R311.7.1).

#include "arq.h"

#include <cmath>
#include <string>
#include <vector>

namespace arq {

struct CalculoEscalera {
    int contrahuellas = 0;   // número de escalones que se suben
    double contrahuella = 0; // alto de cada escalón
    double huella = 0;       // fondo de cada escalón
    double largo = 0;        // largo en planta: (contrahuellas - 1) huellas
    bool necesitaDescanso = false;  // más de 18 seguidos (NTC 4145)
    std::vector<std::string> avisos;
};

// altoEntrepiso y ancho en unidades del dibujo; metro = unidades por metro.
inline CalculoEscalera calcularEscalera(double altoEntrepiso, double ancho, double metro, Sistema s) {
    CalculoEscalera c;
    const bool imp = s == Sistema::Imperial;
    const double chMax = imp ? 7.75 * 0.0254 * metro : 0.175 * metro;  // un poco por debajo de 180 mm
    c.contrahuellas = std::max(2, static_cast<int>(std::ceil(altoEntrepiso / chMax - 1e-9)));
    c.contrahuella = altoEntrepiso / c.contrahuellas;
    if (imp) {
        c.huella = std::max(10.0 * 0.0254 * metro, 25.0 * 0.0254 * metro - 2 * c.contrahuella);
    } else {
        c.huella = std::max(0.28 * metro, 0.63 * metro - 2 * c.contrahuella);  // paso de 63 cm
    }
    c.largo = (c.contrahuellas - 1) * c.huella;
    c.necesitaDescanso = c.contrahuellas > 18;
    const double anchoMin = imp ? 36 * 0.0254 * metro : 0.90 * metro;
    if (ancho + 1e-9 < anchoMin)
        c.avisos.push_back("El ancho es menor que el mínimo (" + textoLongitud(anchoMin, s) + ").");
    if (c.necesitaDescanso)
        c.avisos.push_back("Más de 18 escalones seguidos: hace falta un descanso a la mitad (NTC 4145).");
    return c;
}

struct PlantaEscalera {
    std::vector<Seg> contorno, huellas, flecha, corte;
    P textoSube;
};

// Escalera en planta: arranca en «inicio» y sube hacia «hacia»; el ancho queda a la derecha
// mirando hacia donde sube. Se corta (línea en zigzag) a la mitad, como se dibuja en planta.
inline PlantaEscalera plantaEscalera(P inicio, P hacia, double ancho, const CalculoEscalera &c) {
    PlantaEscalera e;
    P d = hacia - inicio;
    const double l = largo(d);
    if (l <= 0) d = P(0, 1); else d = d * (1.0 / l);
    const P r(d.y, -d.x);  // a la derecha
    auto Q = [&](double a, double b) { return inicio + d * a + r * b; };
    const double L = c.largo;
    e.contorno = {{Q(0, 0), Q(L, 0)}, {Q(L, 0), Q(L, ancho)}, {Q(L, ancho), Q(0, ancho)}, {Q(0, ancho), Q(0, 0)}};
    for (int i = 1; i < c.contrahuellas - 1; ++i) e.huellas.push_back({Q(i * c.huella, 0), Q(i * c.huella, ancho)});
    // Flecha por el centro: del primer escalón al último.
    const double m = ancho / 2;
    e.flecha.push_back({Q(c.huella * 0.3, m), Q(L - c.huella * 0.3, m)});
    e.flecha.push_back({Q(L - c.huella * 0.3, m), Q(L - c.huella * 0.9, m - ancho * 0.12)});
    e.flecha.push_back({Q(L - c.huella * 0.3, m), Q(L - c.huella * 0.9, m + ancho * 0.12)});
    // Línea de corte (a media altura) en zigzag
    const double x = L * 0.55;
    e.corte.push_back({Q(x - c.huella * 0.4, 0), Q(x, ancho * 0.45)});
    e.corte.push_back({Q(x, ancho * 0.45), Q(x - c.huella * 0.3, ancho * 0.55)});
    e.corte.push_back({Q(x - c.huella * 0.3, ancho * 0.55), Q(x + c.huella * 0.1, ancho)});
    e.textoSube = Q(c.huella * 0.9, m + ancho * 0.2);
    return e;
}

} // namespace arq
