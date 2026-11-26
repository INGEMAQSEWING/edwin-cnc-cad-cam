// Pruebas de arq.h (sin Qt). Compilar:
//   py -3.10 -m ziglang c++ -std=c++17 pruebas_arq.cpp -o pruebas_arq.exe && pruebas_arq.exe
#include "arq.h"

#include <cstdio>

using namespace arq;

static int fallas = 0, total = 0;

static void ok(bool c, const char *que) {
    ++total;
    if (!c) { ++fallas; std::printf("FALLA: %s\n", que); }
}

static bool cerca(double a, double b, double tol = 1e-6) { return std::fabs(a - b) < tol; }

static double largoTotal(const std::vector<Seg> &s) {
    double t = 0;
    for (const Seg &x : s) t += largo(x.b - x.a);
    return t;
}

int main() {
    double v;
    // --- Métrico
    ok(leerLongitud("3.50", Sistema::Metrico, v) && cerca(v, 3.5), "3.50 m");
    ok(leerLongitud("3,5", Sistema::Metrico, v) && cerca(v, 3.5), "coma decimal");
    ok(leerLongitud("15cm", Sistema::Metrico, v) && cerca(v, 0.15), "15cm");
    ok(leerLongitud("150mm", Sistema::Metrico, v) && cerca(v, 0.15), "150mm");
    ok(leerLongitud("6m", Sistema::Metrico, v) && cerca(v, 6), "6m");
    ok(!leerLongitud("tres", Sistema::Metrico, v), "texto inválido");
    ok(!leerLongitud("", Sistema::Metrico, v), "vacío");
    // --- Imperial (resultado en pulgadas)
    ok(leerLongitud("12'6\"", Sistema::Imperial, v) && cerca(v, 150), "12'6\"");
    ok(leerLongitud("12'-6\"", Sistema::Imperial, v) && cerca(v, 150), "12'-6\"");
    ok(leerLongitud("12' 6\"", Sistema::Imperial, v) && cerca(v, 150), "12' 6\"");
    ok(leerLongitud("12'", Sistema::Imperial, v) && cerca(v, 144), "12'");
    ok(leerLongitud("6\"", Sistema::Imperial, v) && cerca(v, 6), "6\"");
    ok(leerLongitud("12.5", Sistema::Imperial, v) && cerca(v, 150), "número solo = pies");
    ok(leerLongitud("5 1/2\"", Sistema::Imperial, v) && cerca(v, 5.5), "5 1/2\"");
    ok(leerLongitud("3/4\"", Sistema::Imperial, v) && cerca(v, 0.75), "3/4\"");
    ok(leerLongitud("10'6", Sistema::Imperial, v) && cerca(v, 126), "10'6 sin comillas");
    ok(leerLongitud("12\xE2\x80\x99 6\xE2\x80\x9D", Sistema::Imperial, v) && cerca(v, 150), "comillas tipográficas");
    ok(!leerLongitud("12'x", Sistema::Imperial, v), "imperial inválido");
    // --- Listas
    std::vector<double> l;
    ok(leerLista("3.50 3.00 3,50", Sistema::Metrico, l) && l.size() == 3 && cerca(l[2], 3.5), "lista métrica");
    ok(leerLista("0; 4  3", Sistema::Metrico, l) && l.size() == 3, "lista con ; y espacios dobles");
    ok(leerLista("12'-6\" 10'", Sistema::Imperial, l) && l.size() == 2 && cerca(l[0], 150) && cerca(l[1], 120), "lista imperial");
    ok(leerLista("12' 6\" 10'", Sistema::Imperial, l) && l.size() == 2 && cerca(l[0], 150), "lista imperial con espacio tras '");
    ok(!leerLista("3.5 x", Sistema::Metrico, l), "lista inválida");
    ok(leerLista("", Sistema::Metrico, l) && l.empty(), "lista vacía");
    // --- Texto
    ok(textoLongitud(3.5, Sistema::Metrico) == "3.50", "texto 3.50");
    ok(textoLongitud(150, Sistema::Imperial) == "12'-6\"", "texto 12'-6\"");
    ok(textoLongitud(6, Sistema::Imperial) == "6\"", "texto 6\"");
    ok(textoLongitud(5.5, Sistema::Imperial) == "5 1/2\"", "texto 5 1/2\"");
    ok(textoLongitud(143.999, Sistema::Imperial) == "12'-0\"", "redondeo a 12'-0\"");
    ok(textoEscala(50, Sistema::Metrico) == "1:50", "escala 1:50");
    ok(textoEscala(48, Sistema::Imperial) == "1/4\" = 1'-0\"", "escala 1/4");
    ok(textoEscala(96, Sistema::Imperial) == "1/8\" = 1'-0\"", "escala 1/8");
    // --- Unidades de papel
    Unidades um{Sistema::Metrico, 50};
    ok(cerca(um.papel(2.5), 0.125), "2.5 mm a 1:50 = 0.125 m");
    Unidades ui{Sistema::Imperial, 48};
    ok(cerca(ui.papel(25.4), 48), "1 in a 1/4\" = 48 in");
    // --- Ejes
    ok(letraEje(0) == "A" && letraEje(7) == "H" && letraEje(8) == "J", "sin I");
    ok(letraEje(12) == "N" && letraEje(13) == "P", "sin O");
    ok(letraEje(24) == "AA" && letraEje(25) == "AB", "AA, AB");
    auto pe = posicionesEjes({0, 3.5, 3.0, 3.5});
    ok(pe.size() == 4 && cerca(pe[3], 10), "posiciones con 0 inicial");
    pe = posicionesEjes({3.5, 3.0});
    ok(pe.size() == 3 && cerca(pe[2], 6.5), "posiciones sin 0");
    // --- Unión de rectángulos
    // Un rectángulo: perímetro.
    auto s = contornoUnion({rect(0, 0, 2, 1)});
    ok(s.size() == 4 && cerca(largoTotal(s), 6), "un rectángulo");
    // Dos que se tocan formando L: contorno sin la línea interior.
    s = contornoUnion({rect(0, 0, 3, 1), rect(0, 0, 1, 3)});
    ok(cerca(largoTotal(s), 12) && s.size() == 6, "unión en L limpia");
    // Cruz (X): 12 lados.
    s = contornoUnion({rect(-3, -0.5, 3, 0.5), rect(-0.5, -3, 0.5, 3)});
    ok(s.size() == 12 && cerca(largoTotal(s), 24), "unión en X limpia");
    // T
    s = contornoUnion({rect(0, 0, 6, 1), rect(2.5, 1, 3.5, 4)});
    ok(s.size() == 8 && cerca(largoTotal(s), 20), "unión en T limpia");
    // Rectángulos degenerados se ignoran.
    s = contornoUnion({rect(0, 0, 0, 5)});
    ok(s.empty(), "rectángulo sin área");
    // Anillo (muros exteriores): contorno exterior + interior.
    Casa c;
    c.ancho = 6; c.largo = 15; c.muroExt = 0.15; c.muroInt = 0.10;
    auto g = casaGeom(c);
    s = contornoUnion(g.muros);
    ok(s.size() == 8, "casa 6x15: 8 líneas (afuera y adentro)");
    ok(cerca(largoTotal(s), 2 * (6 + 15) + 2 * (5.7 + 14.7)), "casa 6x15: largo de líneas");
    ok(g.ejesX.size() == 2 && cerca(g.ejesX[0], 0.075) && cerca(g.ejesX[1], 5.925), "ejes X en centro de muro");
    // Con divisiones interiores: T limpias.
    c.divX = {3.0};
    c.divY = {5.0, 10.0};
    c.columna = 0.25;
    g = casaGeom(c);
    s = contornoUnion(g.muros);
    ok(g.ejesX.size() == 3 && g.ejesY.size() == 4, "ejes con divisiones");
    ok(g.columnas.size() == 12, "columnas en 12 cruces");
    // Columnas de borde dentro de la casa.
    bool dentro = true;
    for (const Rect &r : g.columnas) if (r.x1 < -1e-9 || r.y1 < -1e-9 || r.x2 > 6 + 1e-9 || r.y2 > 15 + 1e-9) dentro = false;
    ok(dentro, "columnas no se salen");
    // Contorno sin cruces internos: 6 espacios -> 1 exterior + 6 interiores = 4 + 6*4 = 28 lados.
    ok(s.size() == 28, "casa con 6 espacios: 28 lados");
    // División fuera de la casa se ignora.
    c.divX = {7.0};
    c.divY.clear();
    g = casaGeom(c);
    ok(g.ejesX.size() == 2, "división fuera ignorada");
    // --- Muro por puntos
    Muro m = muroPorPuntos({P(0, 0), P(4, 0), P(4, 3)}, 0.2, 0, false);
    ok(m.izquierda.size() == 3 && cerca(m.izquierda[0].y, 0.1) && cerca(m.derecha[0].y, -0.1), "muro centrado");
    // Esquina a inglete: la cara de afuera (derecha al girar a la izquierda) pasa por (4.1, -0.1).
    ok(cerca(m.derecha[1].x, 4.1) && cerca(m.derecha[1].y, -0.1), "inglete exterior");
    ok(cerca(m.izquierda[1].x, 3.9) && cerca(m.izquierda[1].y, 0.1), "inglete interior");
    m = muroPorPuntos({P(0, 0), P(4, 0)}, 0.15, 1, false);
    ok(cerca(m.izquierda[0].y, 0) && cerca(m.derecha[0].y, -0.15), "muro con cara izquierda en los puntos");
    m = muroPorPuntos({P(0, 0), P(4, 0), P(4, 3), P(0, 3)}, 0.2, 0, true);
    ok(m.izquierda.size() == 4 && cerca(m.izquierda[0].x, 0.1) && cerca(m.izquierda[0].y, 0.1), "muro cerrado");
    // --- Pórtico
    Portico p;
    p.vanos = {4.0, 5.0};
    p.alturas = {3.0, 2.8};
    auto pg = porticoGeom(p);
    ok(pg.ejes.size() == 3 && cerca(pg.ejes[2], 9), "pórtico: ejes");
    ok(pg.niveles.size() == 3 && cerca(pg.niveles[2], 5.8), "pórtico: niveles");
    ok(pg.piezas.size() == 5 && pg.zapatas.size() == 3, "pórtico: 3 columnas + 2 vigas + 3 zapatas");
    ok(cerca(pg.zapatas[0].y1, -1.2) && cerca(pg.piezas[0].y1, -0.8), "pórtico: zapata y arranque");
    s = contornoUnion(pg.piezas);
    ok(!s.empty(), "pórtico: contorno");
    // --- Cimentación
    Cimentacion ci;
    ci.ejesX = {0, 4, 8};
    ci.ejesY = {0, 5};
    auto cg = cimentacionGeom(ci);
    ok(cg.zapatas.size() == 6 && cg.columnas.size() == 6, "cimentación: zapatas");
    ok(cg.vigas.size() == 2 * 2 + 3 * 1, "cimentación: 7 vigas");
    // --- Rótulo
    Rotulo r = rotulo(false);
    bool adentro = true;
    for (const Casilla &k : r.casillas)
        if (k.x < 0 || k.y < 0 || k.x + k.w > r.ancho + 1e-9 || k.y + k.h > r.alto + 1e-9) adentro = false;
    ok(adentro, "rótulo base: casillas dentro");
    double area = 0;
    for (const Casilla &k : r.casillas) area += k.w * k.h;
    ok(cerca(area, 180 * 60), "rótulo base: casillas cubren todo sin encimarse");
    r = rotulo(true);
    area = 0;
    for (const Casilla &k : r.casillas) area += k.w * k.h;
    ok(cerca(r.alto, 100) && cerca(area, 180 * 100), "rótulo grande");
    ok(numeroPlancha("A", 101) == "A-101", "número de plancha");
    ok(hojas().size() == 10 && disciplinas().size() >= 14, "listas de hojas y disciplinas");

    std::printf("%d/%d pruebas bien\n", total - fallas, total);
    return fallas == 0 ? 0 : 1;
}
