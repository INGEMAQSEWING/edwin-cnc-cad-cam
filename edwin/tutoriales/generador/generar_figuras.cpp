// Genera las figuras de los tutoriales de DuctusArq 2D con el MISMO cálculo del programa
// (plugins/ductusarq/arq.h). Escribe primitivas de dibujo en texto; armar_pdf.py las dibuja.
//   py -3.10 -m ziglang c++ -std=c++17 -I C:/dev/edwin-cnc-cad-cam/plugins/ductusarq generar_figuras.cpp -o gen.exe
//   gen.exe colombia > colombia.txt   |   gen.exe miami > miami.txt
#include "arq.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace arq;

static Sistema SIS;
static Unidades UN;

static void fig(const char *n) { std::printf("FIG %s\n", n); }
static void L(const char *capa, P a, P b) { std::printf("L %s %.5f %.5f %.5f %.5f\n", capa, a.x, a.y, b.x, b.y); }
static void A(const char *capa, const Arco &a) { std::printf("A %s %.5f %.5f %.5f %.6f %.6f\n", capa, a.c.x, a.c.y, a.r, a.a1, a.a2); }
static void C(const char *capa, P c, double r) { std::printf("C %s %.5f %.5f %.5f\n", capa, c.x, c.y, r); }
static void S(const char *capa, const Rect &r) { std::printf("S %s %.5f %.5f %.5f %.5f\n", capa, r.x1, r.y1, r.x2, r.y2); }
static void T(const char *capa, P p, double h, const char *al, const std::string &t) {
    std::printf("T %s %.5f %.5f %.5f %s %s\n", capa, p.x, p.y, h, al, t.c_str());
}
static void D(P a, P b, P lin) {  // cota alineada horizontal o vertical
    std::printf("D %.5f %.5f %.5f %.5f %.5f %.5f %s\n", a.x, a.y, b.x, b.y, lin.x, lin.y,
                textoLongitud(largo(b - a), SIS).c_str());
}
static void R(const Rect &r, const char *capa) {
    L(capa, P(r.x1, r.y1), P(r.x2, r.y1)); L(capa, P(r.x2, r.y1), P(r.x2, r.y2));
    L(capa, P(r.x2, r.y2), P(r.x1, r.y2)); L(capa, P(r.x1, r.y2), P(r.x1, r.y1));
}

static void eje(P a, P b, const std::string &nombre) {
    const double r = UN.papel(4.5);
    const P d = b - a;
    const P dir = d * (1.0 / largo(d));
    L("A-EJES", a, b);
    const P c = b + dir * r;
    C("A-EJES", c, r);
    T("A-EJES", c, UN.papel(4), "cm", nombre);
}

struct VanoDef {
    TipoVano tipo;
    P o, dir;
    double ancho, alto, antepecho;
    int lado;
    bool bisagraInicio;
    std::string marca, aprob;
};

struct Ej {
    Casa casa;
    std::vector<VanoDef> vanos;
    struct Esp { const char *nombre; P punto; };
    std::vector<Esp> espacios;
    double lote;            // área del lote
    const char *titulo;
};

static Ej colombia() {
    Ej e;
    e.casa.ancho = 7.00; e.casa.largo = 10.00; e.casa.muroExt = 0.15; e.casa.muroInt = 0.10;
    e.casa.divX = {3.40};
    e.casa.divY = {3.00, 6.50};
    e.casa.columna = 0.25;
    auto v = [&](TipoVano t, P o, P d, double a, double h, double ap, int lado, bool bi, const char *m) {
        e.vanos.push_back({t, o, d, a, h, ap, lado, bi, m, ""});
    };
    const P X(1, 0), Y(0, 1);
    v(TipoVano::Batiente, P(0.60, 0), X, 0.90, 2.10, 0, 1, true, "P-1");     // entrada
    v(TipoVano::Batiente, P(3.35, 1.10), Y, 0.90, 2.10, 0, 1, false, "P-2");  // sala -> cocina
    v(TipoVano::Batiente, P(2.20, 2.95), X, 0.80, 2.10, 0, 1, true, "P-3");   // sala -> alcoba 1
    v(TipoVano::Batiente, P(1.20, 6.45), X, 0.80, 2.10, 0, 1, true, "P-3");   // alcoba 1 -> alcoba 2
    v(TipoVano::Batiente, P(4.20, 2.95), X, 0.80, 2.10, 0, 1, false, "P-3");  // cocina -> baño
    v(TipoVano::Batiente, P(5.40, 6.45), X, 0.80, 2.10, 0, 1, true, "P-3");   // baño -> alcoba ppal
    v(TipoVano::Ventana, P(1.80, 0), X, 1.20, 1.20, 1.00, 0, true, "V-1");   // sala
    v(TipoVano::Ventana, P(4.60, 0), X, 1.50, 1.00, 1.10, 0, true, "V-2");   // cocina
    v(TipoVano::Ventana, P(1.00, 10.0), X, 1.20, 1.20, 1.00, 0, true, "V-1"); // alcoba 2
    v(TipoVano::Ventana, P(4.80, 10.0), X, 1.20, 1.20, 1.00, 0, true, "V-1"); // alcoba principal
    v(TipoVano::Ventana, P(0, 4.10), Y, 1.20, 1.20, 1.00, 0, true, "V-1");   // alcoba 1
    v(TipoVano::Ventana, P(7.0, 4.40), Y, 0.60, 0.60, 1.60, 0, true, "V-3"); // baño
    e.espacios = {{"SALA", P(1.70, 1.60)}, {"COCINA - COMEDOR", P(5.20, 1.80)}, {"ALCOBA 1", P(1.70, 4.80)},
                  {"BAÑO Y ROPAS", P(5.20, 4.90)}, {"ALCOBA 2", P(1.70, 8.20)}, {"ALCOBA PRINCIPAL", P(5.20, 8.20)}};
    e.lote = 8.00 * 12.00;
    e.titulo = "PLANTA ARQUITECTÓNICA PRIMER PISO";
    return e;
}

static Ej miami() {
    Ej e;
    const double ft = 12;
    e.casa.ancho = 38 * ft; e.casa.largo = 48 * ft; e.casa.muroExt = 8; e.casa.muroInt = 4.5;
    e.casa.divX = {15 * ft};
    e.casa.divY = {12 * ft, 23 * ft, 34 * ft};
    e.casa.columna = 0;
    auto v = [&](TipoVano t, P o, P d, double a, double h, double ap, int lado, bool bi, const char *m, const char *noa) {
        e.vanos.push_back({t, o, d, a, h, ap, lado, bi, m, noa});
    };
    const P X(1, 0), Y(0, 1);
    const char *noaP = "NOA 00-0000.00 (EJ.)";
    const char *noaV = "NOA 00-0000.01 (EJ.)";
    // Puertas interiores de 2'-8" y 2'-6"
    v(TipoVano::Batiente, P(15 * ft - 2.25, 4 * ft), Y, 32, 80, 0, -1, true, "P-1", "");     // bedroom 3
    v(TipoVano::Batiente, P(15 * ft - 2.25, 14 * ft), Y, 30, 80, 0, -1, true, "P-2", "");    // bath
    v(TipoVano::Batiente, P(15 * ft - 2.25, 26 * ft), Y, 32, 80, 0, -1, true, "P-1", "");    // bedroom 2
    v(TipoVano::Doble, P(15 * ft - 2.25, 38 * ft), Y, 60, 80, 0, -1, true, "P-3", "");       // master (2 hojas)
    v(TipoVano::Batiente, P(26 * ft, 12 * ft - 2.25), X, 32, 80, 0, -1, true, "P-1", "");    // bedroom 4
    v(TipoVano::Batiente, P(28 * ft, 34 * ft - 2.25), X, 30, 80, 0, 1, true, "P-2", "");     // master bath
    // Ventanas de impacto (Miami-Dade) con NOA
    v(TipoVano::Ventana, P(4 * ft, 0), X, 36, 60, 36, 0, true, "V-1", noaV);
    v(TipoVano::Ventana, P(24 * ft, 0), X, 36, 60, 36, 0, true, "V-1", noaV);
    v(TipoVano::Ventana, P(0, 27 * ft), Y, 36, 60, 36, 0, true, "V-1", noaV);
    v(TipoVano::Ventana, P(4 * ft, 48 * ft), X, 72, 60, 36, 0, true, "V-2", noaV);
    v(TipoVano::Ventana, P(28 * ft, 48 * ft), X, 36, 48, 48, 0, true, "V-3", noaV);
    v(TipoVano::Ventana, P(38 * ft, 16 * ft), Y, 72, 72, 24, 0, true, "V-2", noaV);
    (void)noaP;
    e.espacios = {{"BEDROOM 3", P(7 * ft, 6 * ft)}, {"BATH", P(7 * ft, 17 * ft)}, {"BEDROOM 2", P(7 * ft, 28 * ft)},
                  {"MASTER SUITE", P(7 * ft, 41 * ft)}, {"BEDROOM 4", P(27 * ft, 6 * ft)},
                  {"LOFT", P(27 * ft, 18 * ft)}, {"OPEN TO BELOW", P(27 * ft, 29 * ft)},
                  {"BONUS ROOM", P(27 * ft, 41 * ft)}};
    e.lote = (50 * ft) * (100 * ft);
    e.titulo = "SECOND FLOOR PLAN / PLANTA SEGUNDO PISO";
    return e;
}

int main(int argc, char **argv) {
    const bool esMiami = argc > 1 && std::strcmp(argv[1], "miami") == 0;
    SIS = esMiami ? Sistema::Imperial : Sistema::Metrico;
    UN = Unidades{SIS, esMiami ? 48.0 : 50.0};
    const Ej e = esMiami ? miami() : colombia();
    const CasaGeom g = casaGeom(e.casa);
    std::vector<Rect> todo = g.muros;
    todo.insert(todo.end(), g.columnas.begin(), g.columnas.end());
    std::vector<Seg> muros = contornoUnion(todo);
    const double W = e.casa.ancho, Lg = e.casa.largo;
    const double dC1 = UN.papel(8), dC2 = UN.papel(15), dEje = UN.papel(22);

    auto dibujarBase = [&](const std::vector<Seg> &ms) {
        for (const Seg &s : ms) L("A-MUROS", s.a, s.b);
        for (const Rect &c : g.columnas) { S("A-COLUMNAS", c); R(c, "A-COLUMNAS"); }
        for (size_t i = 0; i < g.ejesX.size(); ++i)
            eje(P(g.ejesX[i], Lg + UN.papel(4)), P(g.ejesX[i], -dEje), std::to_string(i + 1));
        for (size_t j = 0; j < g.ejesY.size(); ++j)
            eje(P(W + UN.papel(4), g.ejesY[j]), P(-dEje, g.ejesY[j]), letraEje(static_cast<int>(j)));
        for (size_t i = 1; i < g.ejesX.size(); ++i) D(P(g.ejesX[i - 1], 0), P(g.ejesX[i], 0), P(g.ejesX[i], -dC1));
        D(P(0, 0), P(W, 0), P(W, -dC2));
        for (size_t j = 1; j < g.ejesY.size(); ++j) D(P(0, g.ejesY[j - 1]), P(0, g.ejesY[j]), P(-dC1, g.ejesY[j]));
        D(P(0, 0), P(0, Lg), P(-dC2, Lg));
    };

    // Figura 1: casa rápida
    fig("casa");
    dibujarBase(muros);

    // Figura 2: puertas y ventanas (cortan el muro)
    std::vector<Seg> cortados = muros;
    struct Hecho { Vano v; SimboloVano s; VanoDef d; };
    std::vector<Hecho> hechos;
    const double tol = UN.metros(0.001);
    for (const VanoDef &d : e.vanos) {
        Vano v;
        v.tipo = d.tipo; v.o = d.o; v.dir = d.dir; v.ancho = d.ancho;
        double w = 0;
        if (!buscarOtraCara(cortados, v.o, v.dir, v.ancho, UN.metros(0.8), tol, w)) {
            std::fprintf(stderr, "no encontré la otra cara para %s en %.2f %.2f\n", d.marca.c_str(), d.o.x, d.o.y);
            return 1;
        }
        const P perp(-v.dir.y, v.dir.x);
        v.n = w > 0 ? perp : perp * -1.0;
        v.espesor = std::fabs(w);
        v.lado = d.lado == 0 ? 1 : d.lado;
        v.bisagraInicio = d.bisagraInicio;
        std::vector<Seg> nuevos;
        for (const Seg &s : cortados) { auto r = cortarPorVano(s, v, tol); nuevos.insert(nuevos.end(), r.begin(), r.end()); }
        cortados.swap(nuevos);
        hechos.push_back({v, simboloVano(v, UN.papel(7)), d});
    }
    auto dibujarVanos = [&]() {
        for (const Hecho &h : hechos) {
            const char *capa = h.v.tipo == TipoVano::Ventana ? "A-VENTANAS" : "A-PUERTAS";
            for (const Seg &s : h.s.lineas) L(capa, s.a, s.b);
            for (const Arco &a : h.s.arcos) A(capa, a);
            const double r = UN.papel(3.5);
            if (h.v.tipo == TipoVano::Ventana)
                R(rect(h.s.marca.x - r * 1.3, h.s.marca.y - r * 0.8, h.s.marca.x + r * 1.3, h.s.marca.y + r * 0.8), "A-SIMBOLOS");
            else C("A-SIMBOLOS", h.s.marca, r);
            T("A-SIMBOLOS", h.s.marca, UN.papel(2.5), "cm", h.d.marca);
        }
    };
    fig("vanos");
    dibujarBase(cortados);
    dibujarVanos();

    // Figura 3: áreas con clic adentro
    std::vector<Seg> cierre = cortados;
    for (const Hecho &h : hechos) {
        const std::string t = "DUCTUS-VANO|" + h.d.marca + "|X|1|1|0||" + esquinasVano(h.v);
        cierreVano(t, cierre);
    }
    fig("areas");
    dibujarBase(cortados);
    dibujarVanos();
    std::vector<DatoEspacio> datos;
    for (const auto &es : e.espacios) {
        const AreaEspacio a = areaPorPunto(cierre, es.punto, UN.metros(200), tol);
        if (!a.cerrado) { std::fprintf(stderr, "espacio abierto: %s\n", es.nombre); return 1; }
        char buf[64];
        if (SIS == Sistema::Metrico) std::snprintf(buf, sizeof buf, "%.2f m²", a.area);
        else std::snprintf(buf, sizeof buf, "%.1f ft²", a.area / 144.0);
        T("A-TEXTOS", P(es.punto.x, es.punto.y + UN.papel(2)), UN.papel(3), "cb", es.nombre);
        T("A-TEXTOS", P(es.punto.x, es.punto.y - UN.papel(1)), UN.papel(2.5), "ct", buf);
        datos.push_back({es.nombre, "espacio", 1, a.area});
        std::printf("AREA %s|%.6f\n", es.nombre, a.area);
    }
    datos.push_back({"CONSTRUIDA", "construida", 1, W * Lg});
    datos.push_back({"LOTE", "lote", 0, e.lote});
    const ResumenAreas r = resumirAreas(datos);
    std::printf("RESUMEN %.6f|%.6f|%.6f|%.6f|%.6f\n", r.utilPorPiso[0], r.construidaTotal, r.lote,
                r.indiceOcupacion, r.indiceConstruccion);
    // Cuadro de vanos
    std::vector<DatoVano> dv;
    for (const Hecho &h : hechos) {
        DatoVano d;
        d.marca = h.d.marca; d.tipo = nombreTipoVano(h.d.tipo); d.ancho = h.d.ancho; d.alto = h.d.alto;
        d.antepecho = h.d.antepecho; d.aprobacion = h.d.aprob;
        dv.push_back(d);
    }
    for (const FilaVano &f : agruparVanos(dv))
        std::printf("VANO %s|%s|%s|%s|%s|%d|%s\n", f.dato.marca.c_str(), f.dato.tipo.c_str(),
                    textoLongitud(f.dato.ancho, SIS).c_str(), textoLongitud(f.dato.alto, SIS).c_str(),
                    f.dato.tipo == std::string("VENTANA") ? textoLongitud(f.dato.antepecho, SIS).c_str() : "-",
                    f.cantidad, f.dato.aprobacion.c_str());
    // Título del plano
    std::printf("TITULO %s\n", e.titulo);
    std::printf("ESCALA %s\n", textoEscala(UN.escala, SIS).c_str());
    std::printf("CASA %s|%s\n", textoLongitud(W, SIS).c_str(), textoLongitud(Lg, SIS).c_str());
    // Rótulo
    if (esMiami) {
        const Rotulo ro = rotuloFranja(609.6 - 2 * 10.16);
        for (const Casilla &k : ro.casillas)
            std::printf("CASILLA %.3f %.3f %.3f %.3f %.2f %s|%s\n", k.x, k.y, k.w, k.h, k.alto, k.etiqueta.c_str(), k.clave.c_str());
        std::printf("ROTULO %.3f %.3f\n", ro.ancho, ro.alto);
    } else {
        const Rotulo ro = rotulo(false);
        for (const Casilla &k : ro.casillas)
            std::printf("CASILLA %.3f %.3f %.3f %.3f %.2f %s|%s\n", k.x, k.y, k.w, k.h, k.alto, k.etiqueta.c_str(), k.clave.c_str());
        std::printf("ROTULO %.3f %.3f\n", ro.ancho, ro.alto);
    }
    return 0;
}
