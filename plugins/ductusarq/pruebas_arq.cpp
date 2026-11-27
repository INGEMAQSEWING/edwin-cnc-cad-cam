// Pruebas de arq.h (sin Qt). Compilar:
//   py -3.10 -m ziglang c++ -std=c++17 pruebas_arq.cpp -o pruebas_arq.exe && pruebas_arq.exe
#include "arq.h"
#include "muebles.h"
#include "ventas.h"
#include "circulacion.h"
#include "cruces.h"
#include "cantidades.h"
#include "fachadas.h"
#include "normas.h"

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

    // --- Rótulo en franja (Miami)
    {
        Rotulo f = rotuloFranja(609.6 - 20);  // ARCH D horizontal: 24" de alto menos márgenes
        double sum = 0;
        bool bien = true;
        for (const Casilla &k : f.casillas) {
            if (k.x < -1e-9 || k.x + k.w > f.ancho + 1e-9 || k.y < -1e-9 || k.y + k.h > f.alto + 1e-6) bien = false;
            sum += k.w * k.h;
        }
        ok(bien && cerca(sum, f.ancho * f.alto, 1e-3), "franja: casillas llenan el alto");
        ok(f.casillas.back().clave == "sello" && cerca(f.casillas.back().h, 114.3), "franja: hueco de la ciudad arriba de 4.5\"");
        ok(f.casillas.front().clave == "plancha", "franja: plancha abajo");
        ok(franjaCabe(437) && !franjaCabe(196), "franja solo en hojas grandes");
    }
    // --- Vanos
    {
        // Muro horizontal de 0.15: caras y = 0 y y = 0.15, de x = 0 a x = 5.
        std::vector<Seg> muro{{P(0, 0), P(5, 0)}, {P(0, 0.15), P(5, 0.15)}};
        double w = 0;
        ok(buscarOtraCara(muro, P(1, 0), P(1, 0), 0.9, 0.6, 1e-6, w) && cerca(w, 0.15), "otra cara arriba");
        ok(buscarOtraCara(muro, P(1, 0.15), P(1, 0), 0.9, 0.6, 1e-6, w) && cerca(w, -0.15), "otra cara abajo");
        ok(!buscarOtraCara(muro, P(1, 0), P(1, 0), 0.9, 0.1, 1e-6, w), "muro más grueso que el máximo");
        ok(!buscarOtraCara(muro, P(6, 0), P(1, 0), 0.9, 0.6, 1e-6, w), "vano fuera del muro");
        Vano v;
        v.o = P(1, 0); v.dir = P(1, 0); v.n = P(0, 1); v.ancho = 0.9; v.espesor = 0.15;
        auto c1 = cortarPorVano(muro[0], v, 1e-6);
        ok(c1.size() == 2 && cerca(c1[0].b.x, 1) && cerca(c1[1].a.x, 1.9) && cerca(c1[1].b.x, 5), "corta la cara de abajo");
        auto c2 = cortarPorVano(muro[1], v, 1e-6);
        ok(c2.size() == 2 && cerca(c2[0].a.y, 0.15), "corta la cara de arriba");
        auto c3 = cortarPorVano({P(0, 2), P(5, 2)}, v, 1e-6);
        ok(c3.size() == 1, "no corta líneas de otro muro");
        auto c4 = cortarPorVano({P(1.2, 0), P(1.5, 0)}, v, 1e-6);
        ok(c4.empty(), "borra el pedazo que queda dentro del vano");
        auto c5 = cortarPorVano({P(1, 0), P(1, 0.15)}, v, 1e-6);
        ok(c5.size() == 1, "no corta líneas perpendiculares");
        // Puerta de 1 hoja que abre hacia arriba (lado +1) con bisagra al inicio.
        v.tipo = TipoVano::Batiente; v.lado = 1; v.bisagraInicio = true;
        auto s = simboloVano(v, 0.3);
        ok(s.lineas.size() == 3 && s.arcos.size() == 1, "puerta: 2 jambas + hoja + arco");
        ok(cerca(s.lineas[2].a.x, 1) && cerca(s.lineas[2].a.y, 0.15) && cerca(s.lineas[2].b.y, 1.05), "hoja de 0.90 hacia arriba");
        ok(cerca(s.arcos[0].r, 0.9) && cerca(s.arcos[0].c.x, 1), "arco con centro en la bisagra");
        double barrido = std::fmod(s.arcos[0].a2 - s.arcos[0].a1 + 4 * 3.141592653589793, 2 * 3.141592653589793);
        ok(cerca(barrido, 3.141592653589793 / 2, 1e-9), "arco de 90°");
        ok(s.marca.y < 0, "marca al otro lado de donde abre");
        v.lado = -1; v.bisagraInicio = false;
        s = simboloVano(v, 0.3);
        ok(cerca(s.lineas[2].a.x, 1.9) && cerca(s.lineas[2].b.y, -0.9), "hoja hacia abajo con bisagra al final");
        v.tipo = TipoVano::Doble;
        s = simboloVano(v, 0.3);
        ok(s.arcos.size() == 2 && cerca(s.arcos[0].r, 0.45), "puerta de 2 hojas");
        v.tipo = TipoVano::Ventana;
        s = simboloVano(v, 0.3);
        ok(s.lineas.size() == 6 && s.arcos.empty(), "ventana: jambas + 4 líneas");
        v.tipo = TipoVano::Corrediza;
        s = simboloVano(v, 0.3);
        ok(s.lineas.size() == 10, "corrediza: jambas + 2 hojas");
        v.tipo = TipoVano::Vano;
        s = simboloVano(v, 0.3);
        ok(s.lineas.size() == 2, "vano: solo jambas");
    }
    // --- Área con un clic adentro
    {
        Casa c2;
        c2.ancho = 6; c2.largo = 15; c2.muroExt = 0.15; c2.muroInt = 0.10;
        c2.divX = {3.0};
        c2.divY = {5.0, 10.0};
        auto g2 = casaGeom(c2);
        auto lineas = contornoUnion(g2.muros);
        // Espacio de abajo a la izquierda: x 0.15..2.95, y 0.15..4.95
        auto a = areaPorPunto(lineas, P(1, 1), 50, 1e-6);
        ok(a.cerrado && cerca(a.area, 2.80 * 4.80, 1e-9), "área de la alcoba 2.80 x 4.80");
        a = areaPorPunto(lineas, P(4, 7), 50, 1e-6);
        ok(a.cerrado && cerca(a.area, (5.85 - 3.05) * (9.95 - 5.05), 1e-9), "área del espacio del centro derecha");
        a = areaPorPunto(lineas, P(-1, -1), 50, 1e-6);
        ok(!a.cerrado, "afuera de la casa: no cerrado");
        // Una puerta abierta deja escapar el área; la línea del vano la cierra.
        std::vector<Seg> sala{{P(0, 0), P(4, 0)}, {P(4, 0), P(4, 3)}, {P(4, 3), P(2, 3)}, {P(1, 3), P(0, 3)}, {P(0, 3), P(0, 0)}};
        ok(!areaPorPunto(sala, P(1, 1), 50, 1e-6).cerrado, "con vano abierto se escapa");
        sala.push_back({P(1, 3), P(2, 3)});
        a = areaPorPunto(sala, P(1, 1), 50, 1e-6);
        ok(a.cerrado && cerca(a.area, 12), "con la línea del vano: 12 m²");
        ok(cerca(areaPoligono({P(0, 0), P(4, 0), P(4, 3), P(0, 3)}), 12), "área de polígono");
        ok(cerca(areaPoligono({P(0, 0), P(4, 0), P(4, 3), P(2, 1.5), P(0, 3)}), 9), "polígono cóncavo");
    }
    // --- Datos y cuadros
    {
        DatoVano d;
        ok(leerDatoVano("DUCTUS-VANO|P-1|PUERTA BATIENTE 1 HOJA|0.9|2.1|0|NOA 21-0101.05", d)
               && d.marca == "P-1" && cerca(d.alto, 2.1) && d.aprobacion == "NOA 21-0101.05", "leer dato de vano");
        ok(!leerDatoVano("OTRA COSA", d), "texto que no es dato");
        {
            Vano v2;
            v2.o = P(1, 0); v2.dir = P(1, 0); v2.n = P(0, 1); v2.ancho = 1; v2.espesor = 0.15;
            const std::string t = "DUCTUS-VANO|P-1|PUERTA|1|2.1|0||" + esquinasVano(v2);
            std::vector<Seg> cierre;
            ok(leerDatoVano(t, d) && d.aprobacion.empty() && cierreVano(t, cierre) && cierre.size() == 2
                   && cerca(cierre[0].b.x, 2) && cerca(cierre[1].a.y, 0.15), "esquinas del vano en el dato");
            // El espacio con la puerta cortada se cierra con esas líneas.
            std::vector<Seg> sala{{P(0, 0), P(1, 0)}, {P(2, 0), P(4, 0)}, {P(4, 0), P(4, 3)}, {P(4, 3), P(0, 3)},
                                  {P(0, 3), P(0, 0)}, {P(1, 0), P(1, 0.15)}, {P(2, 0), P(2, 0.15)},
                                  {P(0, 0.15), P(1, 0.15)}, {P(2, 0.15), P(4, 0.15)}};
            ok(!areaPorPunto(sala, P(3, 2), 50, 1e-6).cerrado, "sin cierre se escapa por la puerta");
            sala.insert(sala.end(), cierre.begin(), cierre.end());
            ok(areaPorPunto(sala, P(3, 2), 50, 1e-6).cerrado, "con cierre del vano queda cerrado");
        }
        std::vector<DatoVano> vs;
        for (const char *t : {"DUCTUS-VANO|V-1|VENTANA|1.2|1.2|1|", "DUCTUS-VANO|P-10|PUERTA|0.9|2.1|0|",
                              "DUCTUS-VANO|P-2|PUERTA|0.8|2.1|0|", "DUCTUS-VANO|P-2|PUERTA|0.8|2.1|0|"}) {
            DatoVano x;
            if (leerDatoVano(t, x)) vs.push_back(x);
        }
        auto f = agruparVanos(vs);
        ok(f.size() == 3 && f[0].dato.marca == "P-2" && f[0].cantidad == 2 && f[1].dato.marca == "P-10"
               && f[2].dato.marca == "V-1", "cuadro de vanos: agrupa y ordena");
        ok(siguienteMarca("P-", {"P-1", "P-3", "V-7"}) == "P-4" && siguienteMarca("V-", {}) == "V-1", "siguiente marca");
        DatoEspacio e;
        ok(leerDatoEspacio("DUCTUS-ESPACIO|ALCOBA 1|1|13.44|espacio", e) && e.piso == 1 && cerca(e.area, 13.44), "leer dato de espacio");
        std::vector<DatoEspacio> es{{"ALCOBA", "espacio", 1, 13.44}, {"SALA", "espacio", 1, 20},
                                    {"PISO 1", "construida", 1, 90}, {"ALCOBA 2", "espacio", 2, 12},
                                    {"LOTE", "lote", 0, 120}};
        auto r = resumirAreas(es);
        ok(r.pisos.size() == 2 && cerca(r.construidaPorPiso[0], 90) && cerca(r.construidaPorPiso[1], 12), "áreas por piso");
        ok(cerca(r.construidaTotal, 102) && cerca(r.indiceOcupacion, 0.75) && cerca(r.indiceConstruccion, 0.85), "índices");
        es.pop_back();
        r = resumirAreas(es);
        ok(r.lote == 0 && r.indiceOcupacion == 0, "sin lote no hay índices");
    }

    // --- Dato de muro (render 3D)
    {
        std::vector<P> pol;
        const std::string t = datoMuro(esquinas(rect(0, 0, 6, 0.15)));
        ok(leerDatoMuro(t, pol) && pol.size() == 4 && cerca(pol[2].x, 6) && cerca(pol[2].y, 0.15), "dato de muro ida y vuelta");
        ok(!leerDatoMuro("DUCTUS-MURO|1,2;3", pol), "dato de muro mal escrito");
        ok(!leerDatoMuro("DUCTUS-VANO|x", pol), "no es muro");
    }
    // --- Muebles
    {
        bool todasDibujan = true, dentro = true, medidas = true;
        for (const Pieza &pz : catalogo()) {
            for (int sis = 0; sis < 2; ++sis) {
                const double w = sis ? pz.anchoIn : pz.anchoM, f = sis ? pz.fondoIn : pz.fondoM;
                const double uu = sis ? 2.0 : 0.05;
                if (w <= 0 || f <= 0) medidas = false;
                const Dibujo d = dibujarPieza(pz, w, f, uu, sis == 1);
                if (d.lineas.empty() && d.circulos.empty()) todasDibujan = false;
                // Las sillas del comedor y del escritorio salen de la mesa: margen de 1 silla.
                const std::string id = pz.id;
                const double m = (id.rfind("comedor", 0) == 0 || id == "escritorio") ? (sis ? 30.0 : 0.75) : 1e-9;
                for (const Seg &s : d.lineas)
                    for (P q : {s.a, s.b})
                        if (std::fabs(q.x) > w / 2 + m || std::fabs(q.y) > f / 2 + m) {
                            dentro = false;
                            std::printf("   %s se sale: %.3f %.3f\n", pz.id, q.x, q.y);
                        }
            }
            if (pz.altoM <= 0 || pz.altoIn <= 0) medidas = false;
        }
        ok(catalogo().size() >= 30, "catálogo con 30 piezas o más");
        ok(todasDibujan, "todas las piezas se dibujan");
        ok(dentro, "las piezas no se salen de su tamaño");
        ok(medidas, "todas tienen ancho, fondo y alto");
        ok(buscarPieza("cama_queen") && cerca(buscarPieza("cama_queen")->anchoIn, 60), "cama queen de 60\"");
        ok(!buscarPieza("no_existe"), "pieza que no existe");
        Dibujo d;
        d.lineas.push_back({P(1, 0), P(1, 1)});
        Dibujo r = colocar(d, P(10, 5), 90);
        ok(cerca(r.lineas[0].a.x, 10) && cerca(r.lineas[0].a.y, 6) && cerca(r.lineas[0].b.x, 9), "colocar y girar 90°");
        ok(datoMueble(*buscarPieza("nevera"), P(1, 2), 0, 0.7, 0.7, 1.75).rfind("DUCTUS-MUEBLE|nevera|1.0", 0) == 0, "dato de mueble");
    }


    // ---- Plano de ventas ----
    ok(pisoPorNombre("BA\xC3\x91O Y ROPAS") == TipoPiso::Bano, "ventas: baño");
    ok(pisoPorNombre("MASTER BATH") == TipoPiso::Bano, "ventas: bath");
    ok(pisoPorNombre("WALK-IN CLOSET") == TipoPiso::Closet, "ventas: closet antes que otros");
    ok(pisoPorNombre("COCINA - COMEDOR") == TipoPiso::Cocina, "ventas: cocina");
    ok(pisoPorNombre("OPEN TO BELOW") == TipoPiso::Vacio, "ventas: vacío");
    ok(pisoPorNombre("ALCOBA PRINCIPAL") == TipoPiso::Calido, "ventas: alcoba");
    ok(medidasEspacio(180, 168, Sistema::Imperial) == "15'-0\" X 14'-0\"", "ventas: medidas en pies");
    ok(medidasEspacio(3.4, 3.3, Sistema::Metrico) == "3.40 x 3.30 m", "ventas: medidas en metros");
    {
        Vano v; v.tipo = TipoVano::Batiente; v.o = P(1, 0); v.dir = P(1, 0); v.n = P(0, 1); v.ancho = 0.9; v.espesor = 0.15;
        v.lado = -1; v.bisagraInicio = false;
        const std::string d = "DUCTUS-VANO|P-1|" + std::string(nombreTipoVano(v.tipo)) + "|0.9|2.1|0||" + esquinasVano(v) + "|" + datoLadoVano(v);
        Vano r;
        ok(vanoDeDato(d, r) && r.tipo == TipoVano::Batiente && std::fabs(r.ancho - 0.9) < 1e-6 && std::fabs(r.espesor - 0.15) < 1e-6
           && r.lado == -1 && !r.bisagraInicio && std::fabs(r.n.y - 1) < 1e-9, "ventas: vano desde su dato");
        Vano viejo;  // datos de antes (sin lado ni bisagra)
        ok(vanoDeDato("DUCTUS-VANO|P-1|" + std::string(nombreTipoVano(v.tipo)) + "|0.9|2.1|0||" + esquinasVano(v), viejo)
           && viejo.lado == 1 && viejo.bisagraInicio, "ventas: vano viejo");
        DatoVano dv;
        ok(leerDatoVano(d, dv) && dv.marca == "P-1", "ventas: el cuadro de vanos sigue leyendo el dato nuevo");
        std::vector<Seg> cierre;
        ok(cierreVano(d, cierre) && cierre.size() == 2, "ventas: el área sigue cerrando con el dato nuevo");
    }
    {
        // Cuarto de 4 x 3 con una cama en el centro: el nombre no cae sobre la cama.
        EspacioVentas e{"ALCOBA", P(2, 1.5), {rect(0, 0, 4, 3)}, 12};
        std::vector<Rect> cama{rect(1.5, 0.8, 2.5, 2.2)};
        const P q = lugarLibre(e, 1.2, 0.4, cama);
        const Rect t{q.x - 0.6, q.y - 0.2, q.x + 0.6, q.y + 0.2};
        ok(solape(t, cama[0]) == 0 && q.x >= 0 && q.x <= 4, "ventas: nombre en lugar libre");
        std::vector<std::vector<P>> muros{esquinas(rect(-0.1, -0.1, 4.1, 0)), esquinas(rect(-0.1, 3, 4.1, 3.1)),
                                          esquinas(rect(-0.1, 0, 0, 3)), esquinas(rect(4, 0, 4.1, 3))};
        OpcionesVentas op;
        const Lamina L = planoVentas(muros, {}, {e}, {}, op, Sistema::Metrico);
        ok(!L.vacia() && L.textos.size() == 2 && L.textos[1].texto == "4.00 x 3.00 m", "ventas: lámina con nombre y medidas");
    }
    {
        // Casa con pasillo: 4 muros exteriores + 2 del pasillo + 2 tramos por cada división a lo largo.
        Casa c;
        c.ancho = 7; c.largo = 10; c.divX = {3.0}; c.divY = {3.0, 6.5}; c.pasillo = 1.0;
        auto g = casaGeom(c);
        ok(g.muros.size() == 10 && g.ejesX.size() == 4 && std::fabs(g.ejesX[2] - 4.1) < 1e-9, "pasillo: muros y ejes");
        c.pasillo = 9;  // no cabe: queda la casa normal
        g = casaGeom(c);
        ok(g.muros.size() == 7 && g.ejesX.size() == 3, "pasillo: si no cabe, casa sin pasillo");
    }
    {
        auto puerta = [](P o, P dir, P n, double ancho, double esp) {
            Vano v; v.tipo = TipoVano::Batiente; v.o = o; v.dir = dir; v.n = n; v.ancho = ancho; v.espesor = esp;
            return v;
        };
        const P X(1, 0), Y(0, 1);
        // Diseño encadenado (el error del ejemplo viejo): sala -> alcoba 1 -> alcoba 2.
        std::vector<EspacioVentas> e{{"SALA", P(), {rect(0.15, 0.15, 3.35, 2.95)}, 0},
                                     {"ALCOBA 1", P(), {rect(0.15, 3.05, 3.35, 6.45)}, 0},
                                     {"ALCOBA 2", P(), {rect(0.15, 6.55, 3.35, 9.85)}, 0}};
        std::vector<Vano> v{puerta(P(0.6, 0), X, Y, 0.9, 0.15), puerta(P(1.2, 2.95), X, Y, 0.8, 0.1),
                            puerta(P(1.2, 6.45), X, Y, 0.8, 0.1)};
        auto av = revisarCirculacion(e, v, 1.0, 0.9);
        bool encadenado = false;
        for (auto &a : av) if (a.nivel == 2 && a.texto.find("ALCOBA 2") != std::string::npos
                               && a.texto.find("pasar por \xC2\xAB" "ALCOBA 1") != std::string::npos) encadenado = true;
        ok(encadenado, "circulación: avisa que a la alcoba 2 se pasa por la alcoba 1");
        // Con pasillo: cada alcoba da al pasillo.
        e = {{"SALA", P(), {rect(0.15, 0.15, 4.45, 2.95)}, 0},
             {"PASILLO", P(), {rect(3.5, 2.95, 4.4, 9.85)}, 0},
             {"ALCOBA 1", P(), {rect(0.15, 3.05, 3.4, 6.45)}, 0},
             {"ALCOBA 2", P(), {rect(0.15, 6.55, 3.4, 9.85)}, 0},
             {"BA\xC3\x91O", P(), {rect(4.5, 3.05, 6.85, 6.45)}, 0}};
        v = {puerta(P(0.6, 0), X, Y, 0.9, 0.15), puerta(P(3.5, 4.0), Y, P(-1, 0), 0.8, 0.1),
             puerta(P(3.5, 7.0), Y, P(-1, 0), 0.8, 0.1), puerta(P(4.4, 4.0), Y, X, 0.8, 0.1)};
        av = revisarCirculacion(e, v, 1.0, 0.9);
        bool sinBoca = false;  // sin la boca abierta, al pasillo no se llega
        for (auto &a : av) if (a.nivel == 2 && a.texto.find("PASILLO") != std::string::npos) sinBoca = true;
        ok(sinBoca, "circulación: pasillo cerrado se detecta");
        Vano boca = puerta(P(3.5, 2.95), X, Y, 0.9, 0.1);
        boca.tipo = TipoVano::Vano;
        v.push_back(boca);
        av = revisarCirculacion(e, v, 1.0, 0.9);
        bool problema = false;
        for (auto &a : av) if (a.nivel > 0) problema = true;
        ok(!av.empty() && av[0].nivel == 0 && !problema, "circulación: con pasillo todo bien");
        av = revisarCirculacion(e, v, 1.0, 1.2);
        problema = false;
        for (auto &a : av) if (a.nivel == 2 && a.texto.find("angosto") != std::string::npos) problema = true;
        ok(problema, "circulación: pasillo angosto");
        av = revisarCirculacion(e, {v[1], v[2], v[3]}, 1.0, 0.9);
        ok(!av.empty() && av[0].texto.find("entrada") != std::string::npos, "circulación: sin puerta de entrada");
    }
    {
        // Cruces: columnas del estructural contra muros, vanos y espacios del arquitectónico.
        auto rectPoli = [](double x1, double y1, double x2, double y2, const char *capa) {
            return Poli{{P(x1, y1), P(x2, y1), P(x2, y2), P(x1, y2), P(x1, y1)}, capa};
        };
        std::vector<Poli> polis{rectPoli(0, 0, 0.25, 0.25, "EST-COLUMNAS"),        // en la esquina (muro)
                                rectPoli(0.9, -0.05, 1.15, 0.20, "EST-COLUMNAS"),   // en la puerta
                                rectPoli(2.0, 2.0, 2.25, 2.25, "EST-COLUMNAS"),     // suelta en la sala
                                rectPoli(0, 0, 0.25, 0.25, "EST-COLUMNAS"),         // repetida
                                rectPoli(0, 0, 5, 4, "EST-COLUMNAS"),               // muy grande: no es columna
                                rectPoli(3, 3, 3.25, 3.25, "EST-COTAS")};           // fuera de la capa de columnas
        auto cols = detectarColumnas(polis, {}, 0.15, 1.5);
        ok(cols.size() == 3, "cruces: detecta 3 columnas (sin repetida, grande ni de otra capa)");
        std::vector<std::vector<P>> muros{esquinas(rect(-0.1, -0.1, 4.1, 0.15)), esquinas(rect(-0.1, -0.1, 0.15, 3.1))};
        std::vector<VanoArq> vanos{{"P-1", rect(0.8, 0, 1.7, 0.15)}};
        std::vector<EspacioVentas> esp{{"SALA", P(2, 1.5), {rect(0.15, 0.15, 4, 3)}, 0}};
        auto cr = revisarCruces(cols, muros, vanos, esp, 0.02);
        bool puerta = false, suelta = false;
        for (auto &c : cr) {
            if (c.nivel == 2 && c.texto.find("P-1") != std::string::npos) puerta = true;
            if (c.nivel == 2 && c.texto.find("suelta") != std::string::npos && c.texto.find("SALA") != std::string::npos) suelta = true;
        }
        ok(puerta && suelta && cr.size() == 2, "cruces: columna en la puerta y columna suelta en la sala");
        cols = detectarColumnas({rectPoli(0, 0, 0.25, 0.25, "")}, {}, 0.15, 1.5);
        cr = revisarCruces(cols, muros, vanos, esp, 0.02);
        ok(cr.size() == 1 && cr[0].nivel == 0, "cruces: todo bien");
        cr = revisarCruces({}, muros, vanos, esp, 0.02);
        ok(cr.size() == 1 && cr[0].nivel == 1, "cruces: sin columnas avisa");
        cols = detectarColumnas({}, {{P(2, 2), 0.15}}, 0.15, 1.5);
        ok(cols.size() == 1, "cruces: columna redonda");
    }
    {
        // Cantidades: cuarto de 4 x 3 (adentro) con muros de 0.10, una puerta de 0.90 x 2.10 y una
        // ventana de 1.20 x 1.20; una columna de 0.25; muro de 2.40 de alto.
        std::vector<std::vector<P>> muros{esquinas(rect(-0.1, -0.1, 4.1, 0)), esquinas(rect(-0.1, 3, 4.1, 3.1)),
                                          esquinas(rect(-0.1, 0, 0, 3)), esquinas(rect(4, 0, 4.1, 3)),
                                          esquinas(rect(4.1, 3.1, 4.35, 3.35))};
        auto vano = [](const char *marca, const char *tipo, P o, double ancho, double alto, double ante) {
            VanoCantidad v;
            v.marca = marca; v.tipo = tipo; v.alto = alto; v.antepecho = ante;
            v.v.o = o; v.v.dir = P(1, 0); v.v.n = P(0, 1); v.v.ancho = ancho; v.v.espesor = 0.1;
            return v;
        };
        std::vector<VanoCantidad> vanos{vano("P-1", "BATIENTE", P(0.5, -0.1), 0.9, 2.1, 0),
                                        vano("V-1", "VENTANA", P(2.0, 3.0), 1.2, 1.2, 1.0)};
        std::vector<EspacioVentas> esp{{"ALCOBA", P(2, 1.5), {rect(0, 0, 4, 3)}, 12}};
        const Cantidades c = calcularCantidades(muros, vanos, esp, 2.4, 1.0);
        ok(c.columnas == 1 && std::fabs(c.largoMuros - (4.2 + 4.2 + 3 + 3)) < 1e-6, "cantidades: muros y columna");
        ok(std::fabs(c.areaVanosEnMuros - (0.9 * 2.1 + 1.2 * 1.2)) < 1e-6 && c.puertasPorMarca.at("P-1") == 1
               && c.ventanasPorMarca.at("V-1") == 1 && std::fabs(c.areaVidrio - 1.44) < 1e-6, "cantidades: puertas y ventanas");
        ok(std::fabs(c.areaPisos - 12) < 1e-6 && std::fabs(c.cieloRaso - 12) < 1e-6, "cantidades: piso y cielo raso");
        // Pintura: perímetro 14 x 2.4 = 33.6 menos puerta 1.89 y ventana 1.44 = 30.27; zócalo 14 - 0.9.
        ok(std::fabs(c.pintura - 30.27) < 1e-6 && std::fabs(c.zocalo - 13.1) < 1e-6, "cantidades: pintura y zócalo");
    }
    {
        // Fachadas y cortes: cuarto de 4 x 3 con puerta al frente y ventana atrás.
        std::vector<std::vector<P>> muros{esquinas(rect(-0.1, -0.1, 4.1, 0)), esquinas(rect(-0.1, 3, 4.1, 3.1)),
                                          esquinas(rect(-0.1, 0, 0, 3)), esquinas(rect(4, 0, 4.1, 3))};
        auto vano = [](const char *tipo, P o, double ancho, double alto, double ante) {
            VanoCantidad v;
            v.marca = "X"; v.tipo = tipo; v.alto = alto; v.antepecho = ante;
            v.v.o = o; v.v.dir = P(1, 0); v.v.n = P(0, 1); v.v.ancho = ancho; v.v.espesor = 0.1;
            return v;
        };
        std::vector<VanoCantidad> vanos{vano("BATIENTE", P(0.5, -0.1), 0.9, 2.1, 0), vano("VENTANA", P(2.0, 3.0), 1.2, 1.2, 1.0)};
        Alzado f = fachada(muros, vanos, Lado::Frente, 2.4, 0.15, 1.0);
        ok(std::fabs(f.ancho - 4.2) < 1e-9 && !f.puertas.empty() && f.ventanas.empty() && f.niveles.size() == 3,
           "fachada: frente con la puerta y sin la ventana de atrás");
        Alzado b = fachada(muros, vanos, Lado::Fondo, 2.4, 0.15, 1.0);
        bool ventanaBien = false;  // la ventana va de 2.0 a 3.2 en planta; vista desde atrás: de 0.9 a 2.1
        for (const Seg &s : b.ventanas) if (std::fabs(std::min(s.a.x, s.b.x) - 0.9) < 1e-9 && std::fabs(s.a.y - 1.0) < 1e-9) ventanaBien = true;
        ok(ventanaBien, "fachada: ventana de atrás en su lugar (vista desde atrás)");
        std::vector<EspacioVentas> esp{{"SALA", P(2, 1.5), {rect(0, 0, 4, 3)}, 12}};
        Alzado c = corte(muros, vanos, esp, P(-1, 1.5), P(5, 1.5), "A", 2.4, 0.15, 1.0);
        // Losa + contrapiso + 2 muros cortados (izquierda y derecha)
        ok(c.cortado.size() == 4 && c.textos.size() == 1 && c.textos[0].second == "SALA", "corte: muros, losas y nombre");
        Alzado d = corte(muros, vanos, esp, P(2.6, -1), P(2.6, 4), "B", 2.4, 0.15, 1.0);
        // Cruza el muro de atrás por la ventana (queda antepecho y dintel) y el de adelante entero.
        ok(d.cortado.size() == 2 + 1 + 2 && !d.vidrio.empty(), "corte: ventana cortada con antepecho y dintel");
    }
    {
        // Normas: alcoba de 3 x 3 sin ventana y con puerta de 0.60 (Colombia), y en EE. UU. alcoba pequeña.
        auto puerta = [](P o, double ancho) {
            VanoNorma v;
            v.c.marca = "P-9"; v.c.tipo = "BATIENTE"; v.c.alto = 2.1;
            v.c.v.o = o; v.c.v.dir = P(1, 0); v.c.v.n = P(0, 1); v.c.v.ancho = ancho; v.c.v.espesor = 0.1;
            return v;
        };
        std::vector<EspacioVentas> esp{{"ALCOBA 9", P(1.5, 1.5), {rect(0, 0, 3, 3)}, 9}};
        auto av = revisarNormas(esp, {puerta(P(1, -0.1), 0.60)}, 2.4, 1.0, Norma::Colombia, false);
        bool puertaMal = false, sinVentana = false;
        for (auto &a : av) {
            if (a.nivel == 2 && a.texto.find("P-9") != std::string::npos) puertaMal = true;
            if (a.nivel == 2 && a.texto.find("no tiene ventana") != std::string::npos) sinVentana = true;
        }
        ok(puertaMal && sinVentana, "normas: puerta angosta y alcoba sin ventana (Colombia)");
        av = revisarNormas({{"BEDROOM 9", P(1, 1), {rect(0, 0, 2, 2)}, 4}}, {}, 2.4, 1.0, Norma::Florida, false);
        bool chica = false;
        for (auto &a : av) if (a.nivel == 2 && a.fuente == "FBC-R R304.1") chica = true;
        ok(chica, "normas: alcoba de menos de 70 ft2 (Florida)");
        ok(!esAlcoba("MASTER BATH") && esAlcoba("MASTER SUITE") && esAlcoba("ALCOBA 1"), "normas: qué es alcoba");
    }
    std::printf("%d/%d pruebas bien\n", total - fallas, total);
    return fallas == 0 ? 0 : 1;
}
