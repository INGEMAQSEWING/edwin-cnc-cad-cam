#pragma once
// Revisión de circulación: ¿se llega a cada espacio desde la entrada sin atravesar una alcoba
// o un baño? ¿Los pasillos tienen el ancho mínimo? Sin Qt (se prueba en pruebas_arq.cpp).
//
// Cómo funciona: cada puerta une los dos espacios que tiene a lado y lado (o un espacio con
// la calle si da afuera). Desde afuera se recorre la casa pasando solo por espacios «de paso»
// (sala, comedor, cocina, pasillo, hall...). Las alcobas, baños, closets y estudios son
// «destino»: se llega a ellos pero no se pasa por ellos para ir a otro lado.

#include "ventas.h"

#include <queue>
#include <string>
#include <vector>

namespace arq {

enum class UsoEspacio { Paso, Privado, Servicio };

inline UsoEspacio usoEspacio(const std::string &nombre) {
    const std::string n = mayusculasAscii(nombre);
    auto tiene = [&](std::initializer_list<const char *> l) {
        for (const char *x : l) if (n.find(x) != std::string::npos) return true;
        return false;
    };
    const TipoPiso t = pisoPorNombre(nombre);
    if (t == TipoPiso::Bano || t == TipoPiso::Closet) return UsoEspacio::Servicio;
    if (tiene({"ALCOBA", "HABITACI", "DORMITORIO", "CUARTO", "RECAMARA", "REC\xC3\x81MARA", "BEDROOM", "MASTER",
               "SUITE", "ESTUDIO", "STUDY", "OFFICE", "OFICINA", "DEN", "NURSERY", "BONUS"}))
        return UsoEspacio::Privado;
    return UsoEspacio::Paso;
}

inline bool esPasillo(const std::string &nombre) {
    const std::string n = mayusculasAscii(nombre);
    for (const char *x : {"PASILLO", "CIRCULACI", "HALL", "CORREDOR", "CORRIDOR", "HALLWAY"})
        if (n.find(x) != std::string::npos) return true;
    return false;
}

// En un segundo piso se entra por la escalera.
inline bool esEscalera(const std::string &nombre) {
    const std::string n = mayusculasAscii(nombre);
    for (const char *x : {"ESCALERA", "STAIR", "LANDING", "DESCANSO"})
        if (n.find(x) != std::string::npos) return true;
    return false;
}

struct AvisoCirculacion {
    int nivel;          // 2 = problema, 1 = revisar, 0 = bien
    std::string texto;
};

struct UnionPuerta { int a, b; };  // índices de espacio; -1 = afuera

// Qué espacios une cada puerta (las ventanas no cuentan). `metro` = unidades del dibujo por metro.
inline std::vector<UnionPuerta> unionesPuertas(const std::vector<EspacioVentas> &esp, const std::vector<Vano> &vanos,
                                              double metro) {
    std::vector<UnionPuerta> r;
    auto donde = [&](P p) {
        for (size_t i = 0; i < esp.size(); ++i)
            if (dentroDe(esp[i].celdas, p)) return static_cast<int>(i);
        return -1;
    };
    for (const Vano &v : vanos) {
        if (v.tipo == TipoVano::Ventana) continue;
        const double sale = v.espesor / 2 + 0.20 * metro;
        const P m = local(v, v.ancho / 2, v.espesor / 2);
        const int a = donde(m - v.n * sale), b = donde(m + v.n * sale);
        if (a != b) r.push_back({a, b});
    }
    // Dos nombres dentro del mismo espacio (sin muro ni vano entre ellos): quedan unidos.
    for (size_t i = 0; i < esp.size(); ++i)
        for (size_t j = i + 1; j < esp.size(); ++j) {
            bool junto = false;
            for (const Rect &a : esp[i].celdas)
                for (const Rect &b : esp[j].celdas)
                    if (solape(a, b) > 1e-9 * metro * metro) junto = true;
            if (junto) r.push_back({static_cast<int>(i), static_cast<int>(j)});
        }
    return r;
}

// Vano abierto entre la sala y el pasillo que deja «Casa rápida».
inline Vano bocaPasillo(const CasaGeom &g) {
    Vano v;
    v.tipo = TipoVano::Vano;
    v.o = P(g.bocaX, g.bocaY);
    v.dir = P(1, 0);
    v.n = P(0, 1);
    v.ancho = g.bocaAncho;
    v.espesor = g.bocaEspesor;
    return v;
}

inline std::vector<AvisoCirculacion> revisarCirculacion(const std::vector<EspacioVentas> &esp,
                                                       const std::vector<Vano> &vanos, double metro,
                                                       double pasilloMin) {
    std::vector<AvisoCirculacion> av;
    const int n = static_cast<int>(esp.size());
    std::vector<UsoEspacio> uso(n);
    for (int i = 0; i < n; ++i) uso[i] = usoEspacio(esp[i].nombre);
    // Nodo n = afuera.
    const std::vector<UnionPuerta> un = unionesPuertas(esp, vanos, metro);
    std::vector<std::vector<int>> vec(n + 1);
    auto nodo = [&](int i) { return i < 0 ? n : i; };
    bool hayEntrada = false;
    for (const UnionPuerta &u : un) {
        vec[nodo(u.a)].push_back(nodo(u.b));
        vec[nodo(u.b)].push_back(nodo(u.a));
        if (u.a < 0 || u.b < 0) hayEntrada = true;
    }
    if (!hayEntrada) {
        // Piso alto: la escalera hace de entrada.
        for (int i = 0; i < n; ++i)
            if (esEscalera(esp[i].nombre)) {
                vec[n].push_back(i);
                vec[i].push_back(n);
                uso[i] = UsoEspacio::Paso;
                if (!hayEntrada) av.push_back({0, "Piso alto: la entrada es la escalera («" + esp[i].nombre + "»)."});
                hayEntrada = true;
            }
    }
    if (!hayEntrada) {
        av.push_back({2, "No hay puerta de entrada: ninguna puerta da a la calle (en un piso alto, nombre la "
                         "escalera como ESCALERA o STAIR)."});
        return av;
    }
    // 1) Recorrido «correcto»: solo se pasa por espacios de paso.
    std::vector<bool> bien(n + 1, false);
    std::queue<int> q;
    bien[n] = true;
    q.push(n);
    while (!q.empty()) {
        const int x = q.front(); q.pop();
        if (x != n && uso[x] != UsoEspacio::Paso) continue;  // se llega, pero no se pasa
        for (int y : vec[x])
            if (!bien[y]) { bien[y] = true; q.push(y); }
    }
    // 2) Recorrido libre (por cualquier puerta), para decir por dónde toca pasar.
    std::vector<int> padre(n + 1, -2);
    padre[n] = -1;
    q.push(n);
    while (!q.empty()) {
        const int x = q.front(); q.pop();
        for (int y : vec[x])
            if (padre[y] == -2) { padre[y] = x; q.push(y); }
    }
    int problemas = 0;
    for (int i = 0; i < n; ++i) {
        if (bien[i]) continue;
        if (padre[i] == -2) {
            av.push_back({2, "«" + esp[i].nombre + "» no tiene puerta: no se puede entrar."});
            ++problemas;
            continue;
        }
        // Espacio privado o de servicio más cercano en el camino.
        int por = padre[i];
        while (por >= 0 && por != n && uso[por] == UsoEspacio::Paso) por = padre[por];
        const std::string &pasa = esp[por >= 0 && por < n ? por : i].nombre;
        if (uso[i] == UsoEspacio::Servicio && por >= 0 && por < n && uso[por] == UsoEspacio::Privado) {
            av.push_back({0, "«" + esp[i].nombre + "» es privado de «" + pasa + "» (se entra por la alcoba)."});
        } else {
            av.push_back({2, "Para llegar a «" + esp[i].nombre + "» hay que pasar por «" + pasa + "»."});
            ++problemas;
        }
    }
    // Baño social: al menos un baño al que se llegue sin pasar por una alcoba.
    bool hayBano = false, banoSocial = false;
    for (int i = 0; i < n; ++i)
        if (pisoPorNombre(esp[i].nombre) == TipoPiso::Bano) {
            hayBano = true;
            if (bien[i]) banoSocial = true;
        }
    if (hayBano && !banoSocial)
        av.push_back({1, "No hay baño social: a todos los baños se entra por una alcoba o por otro baño."});
    // Ancho de pasillos (lo angosto de su caja).
    for (int i = 0; i < n; ++i) {
        if (!esPasillo(esp[i].nombre) || esp[i].celdas.empty()) continue;
        const Rect c = cajaCeldas(esp[i].celdas);
        const double ancho = std::min(c.x2 - c.x1, c.y2 - c.y1);
        if (ancho + 1e-6 < pasilloMin) {
            av.push_back({2, "«" + esp[i].nombre + "» es angosto: tiene " + textoLongitud(ancho, metro > 1.5 ? Sistema::Imperial : Sistema::Metrico)
                                 + " y el mínimo es " + textoLongitud(pasilloMin, metro > 1.5 ? Sistema::Imperial : Sistema::Metrico) + "."});
            ++problemas;
        }
    }
    if (problemas == 0)
        av.insert(av.begin(), {0, "Circulación bien: a cada espacio se llega desde la entrada sin atravesar alcobas ni baños."});
    return av;
}

} // namespace arq
