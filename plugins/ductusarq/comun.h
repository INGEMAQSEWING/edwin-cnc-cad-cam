/*****************************************************************************/
/*  comun.h - Ayudas de dibujo y de ventanas para DuctusArq 2D                */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*****************************************************************************/

#ifndef DUCTUSARQ_COMUN_H
#define DUCTUSARQ_COMUN_H

#include "arq.h"
#include "document_interface.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointF>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <vector>

namespace da {

inline QString u(const char *s) { return QString::fromUtf8(s); }

// ---------------------------------------------------------------------------
// Sistema de medidas y escala guardados (los mismos que usa el programa al crear un dibujo).
inline arq::Unidades unidades() {
    QSettings cfg;
    arq::Unidades un;
    const bool imperial = cfg.value("DuctusArq/Sistema", "metrico").toString() == "imperial";
    un.sistema = imperial ? arq::Sistema::Imperial : arq::Sistema::Metrico;
    un.escala = cfg.value(imperial ? "DuctusArq/EscalaImperial" : "DuctusArq/EscalaMetrica",
                          imperial ? 48.0 : 50.0).toDouble();
    if (un.escala <= 0) un.escala = imperial ? 48 : 50;
    return un;
}

inline QString textoLong(double v, const arq::Unidades &un) {
    return QString::fromStdString(arq::textoLongitud(v, un.sistema));
}

inline QString ayudaUnidades(const arq::Unidades &un) {
    return un.sistema == arq::Sistema::Metrico
        ? u("Medidas en metros: 3.50 o 3,50 (también 15cm).")
        : u("Medidas en pies y pulgadas: 12'-6\", 12' 6\", 6\" o 12.5 (un número solo son pies).");
}

// ---------------------------------------------------------------------------
// Capas de arquitectura e ingeniería (las mismas que trae un dibujo nuevo de DuctusArq 2D).
struct CapaArq {
    const char *nombre;
    int color;
    DPI::LineWidth ancho;
    DPI::LineType tipo;
};

inline const std::vector<CapaArq> &capasArq() {
    static const std::vector<CapaArq> c = {
        {"A-EJES", 0xC80000, DPI::Width05, DPI::DashDotLine},
        {"A-MUROS", 0x000000, DPI::Width11, DPI::SolidLine},
        {"A-COLUMNAS", 0x000000, DPI::Width11, DPI::SolidLine},
        {"A-PUERTAS", 0xBE8C00, DPI::Width07, DPI::SolidLine},
        {"A-VENTANAS", 0x0096BE, DPI::Width07, DPI::SolidLine},
        {"A-ESCALERAS", 0x5A5A5A, DPI::Width07, DPI::SolidLine},
        {"A-COTAS", 0x008C00, DPI::Width05, DPI::SolidLine},
        {"A-TEXTOS", 0x0000A0, DPI::Width07, DPI::SolidLine},
        {"A-SIMBOLOS", 0x7800A0, DPI::Width05, DPI::SolidLine},
        {"A-MOBILIARIO", 0x8C8C8C, DPI::Width03, DPI::SolidLine},
        {"S-CIMENTACION", 0xAA00AA, DPI::Width09, DPI::DashLine},
        {"S-PORTICOS", 0x0050C8, DPI::Width09, DPI::SolidLine},
        {"G-ROTULO", 0x000000, DPI::Width09, DPI::SolidLine},
    };
    return c;
}

// Activa la capa; si el dibujo no la tiene, la crea con su color, grosor y tipo de línea.
inline void capa(Document_Interface *doc, const char *nombre) {
    const QString n = QString::fromUtf8(nombre);
    const bool existe = doc->getAllLayer().contains(n, Qt::CaseInsensitive);
    doc->setLayer(n);
    if (existe) return;
    for (const CapaArq &c : capasArq())
        if (n == QString::fromUtf8(c.nombre)) {
            doc->setCurrentLayerProperties(c.color, c.ancho, c.tipo);
            return;
        }
}

inline int crearCapasArq(Document_Interface *doc) {
    const QString antes = doc->getCurrentLayer();
    const QStringList existentes = doc->getAllLayer();
    int nuevas = 0;
    for (const CapaArq &c : capasArq())
        if (!existentes.contains(QString::fromUtf8(c.nombre), Qt::CaseInsensitive)) {
            capa(doc, c.nombre);
            ++nuevas;
        }
    doc->setLayer(antes);
    return nuevas;
}

// ---------------------------------------------------------------------------
// Dibujo
inline QPointF q(const arq::P &p, const QPointF &base) { return QPointF(base.x() + p.x, base.y() + p.y); }

inline void linea(Document_Interface *doc, const QPointF &base, arq::P a, arq::P b) {
    QPointF p1 = q(a, base), p2 = q(b, base);
    doc->addLine(&p1, &p2);
}

inline void segmentos(Document_Interface *doc, const QPointF &base, const std::vector<arq::Seg> &s) {
    for (const arq::Seg &x : s) linea(doc, base, x.a, x.b);
}

inline void polilinea(Document_Interface *doc, const QPointF &base, const std::vector<arq::P> &pts, bool cerrada) {
    if (pts.size() < 2) return;
    std::vector<Plug_VertexData> v;
    for (const arq::P &p : pts) v.emplace_back(q(p, base), 0.0);
    doc->addPolyline(v, cerrada);
}

inline void rectangulo(Document_Interface *doc, const QPointF &base, const arq::Rect &r) {
    polilinea(doc, base, {arq::P(r.x1, r.y1), arq::P(r.x2, r.y1), arq::P(r.x2, r.y2), arq::P(r.x1, r.y2)}, true);
}

inline void relleno(Document_Interface *doc, const QPointF &base, const arq::Rect &r) {
    doc->addSolid({q(arq::P(r.x1, r.y1), base), q(arq::P(r.x2, r.y1), base),
                   q(arq::P(r.x2, r.y2), base), q(arq::P(r.x1, r.y2), base)});
}

inline void texto(Document_Interface *doc, const QPointF &base, arq::P p, const QString &t, double alto,
                  DPI::HAlign h = DPI::HAlignCenter, DPI::VAlign v = DPI::VAlignMiddle, double angulo = 0) {
    QPointF pt = q(p, base);
    doc->addText(t, "standard", &pt, alto, angulo, h, v);
}

inline void cota(Document_Interface *doc, const QPointF &base, arq::P a, arq::P b, arq::P linea) {
    doc->addDimAligned(q(a, base), q(b, base), q(linea, base));
}

// Eje: línea de a hasta b y burbuja con el nombre después de b (en la misma dirección).
inline void eje(Document_Interface *doc, const QPointF &base, arq::P a, arq::P b, const QString &nombre,
                const arq::Unidades &un) {
    const double r = un.papel(4.5);
    const arq::P d = b - a;
    const double l = arq::largo(d);
    if (l <= 0) return;
    const arq::P dir = d * (1.0 / l);
    capa(doc, "A-EJES");
    linea(doc, base, a, b);
    const arq::P centro = b + dir * r;
    QPointF c = q(centro, base);
    doc->addCircle(&c, r);
    texto(doc, base, centro, nombre, un.papel(4.0));
}

// Cadena de cotas a lo largo de X (horizontal) o de Y (vertical) en las posiciones dadas.
inline void cadenaX(Document_Interface *doc, const QPointF &base, const std::vector<double> &xs, double yRef, double yLinea) {
    for (size_t i = 1; i < xs.size(); ++i)
        cota(doc, base, arq::P(xs[i - 1], yRef), arq::P(xs[i], yRef), arq::P(xs[i], yLinea));
}

inline void cadenaY(Document_Interface *doc, const QPointF &base, const std::vector<double> &ys, double xRef, double xLinea) {
    for (size_t i = 1; i < ys.size(); ++i)
        cota(doc, base, arq::P(xRef, ys[i - 1]), arq::P(xRef, ys[i]), arq::P(xLinea, ys[i]));
}

// ---------------------------------------------------------------------------
// Casillas de las ventanas
// Casilla de una medida; recuerda lo escrito la última vez (clave en QSettings).
inline QLineEdit *campo(QFormLayout *f, const QString &etiqueta, const QString &clave, const QString &porDefecto,
                        const QString &ayuda = QString()) {
    QSettings cfg;
    auto e = new QLineEdit(cfg.value("DuctusArq/" + clave, porDefecto).toString());
    e->setProperty("clave", clave);
    if (!ayuda.isEmpty()) e->setToolTip(ayuda);
    f->addRow(etiqueta, e);
    return e;
}

inline void guardar(const std::vector<QLineEdit *> &campos) {
    QSettings cfg;
    for (QLineEdit *e : campos)
        cfg.setValue("DuctusArq/" + e->property("clave").toString(), e->text());
}

// Lee una medida; si está mal escrita avisa y devuelve false. minimo > 0 obliga a que sea mayor.
inline bool medida(QWidget *parent, QLineEdit *e, const arq::Unidades &un, double &v, double minimo,
                   const QString &nombre) {
    if (!arq::leerLongitud(e->text().toStdString(), un.sistema, v) || v < minimo || (minimo > 0 && v <= 0)) {
        QMessageBox::warning(parent, u("Revise la medida"),
                             u("«%1» no es una medida válida: %2\n\n%3").arg(nombre, e->text(), ayudaUnidades(un)));
        e->setFocus();
        e->selectAll();
        return false;
    }
    return true;
}

inline bool lista(QWidget *parent, QLineEdit *e, const arq::Unidades &un, std::vector<double> &v,
                  const QString &nombre, bool puedeVacia) {
    bool bien = arq::leerLista(e->text().toStdString(), un.sistema, v);
    for (double x : v) if (x < 0) bien = false;
    if (!puedeVacia && v.empty()) bien = false;
    if (!bien) {
        QMessageBox::warning(parent, u("Revise la lista de medidas"),
                             u("«%1» no se entiende: %2\n\nEscriba las medidas separadas por espacios, "
                               "por ejemplo 3.50 3.00 3.50.\n%3").arg(nombre, e->text(), ayudaUnidades(un)));
        e->setFocus();
        e->selectAll();
        return false;
    }
    return true;
}

// Valores sugeridos según el sistema (metros o pies-pulgadas).
inline QString sug(const arq::Unidades &un, const char *metrico, const char *imperial) {
    return QString::fromUtf8(un.sistema == arq::Sistema::Metrico ? metrico : imperial);
}

// Clave distinta para cada sistema, así no se mezclan metros con pies.
inline QString clave(const arq::Unidades &un, const char *nombre) {
    return QString::fromUtf8(nombre) + (un.sistema == arq::Sistema::Metrico ? "_m" : "_ft");
}

} // namespace da

#endif // DUCTUSARQ_COMUN_H
