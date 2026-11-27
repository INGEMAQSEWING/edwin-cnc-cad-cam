/*****************************************************************************/
/*  fachadas.cpp - Fachadas y cortes desde la planta (DuctusArq)             */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/
//
// FAC: dibuja las fachadas (frente, fondo, izquierda, derecha) con los muros, puertas y ventanas
// de la planta, la losa y los niveles. COR: dibuja un corte por una línea que se marca en la
// planta (muros y losas cortados en negro, ventanas con antepecho y dintel, nombres de espacios).

#include "ductusarq.h"
#include "comun.h"
#include "fachadas.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QVBoxLayout>

using da::u;

namespace da {
std::vector<arq::EspacioVentas> espaciosConForma(Document_Interface *doc, const arq::Unidades &un);  // espacios.cpp
}

namespace {

struct DatosPlanta {
    std::vector<std::vector<arq::P>> muros;
    std::vector<arq::VanoCantidad> vanos;
};

DatosPlanta leerPlanta(Document_Interface *doc) {
    DatosPlanta d;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) d.muros.push_back(pol);
    }
    for (const QString &t : da::leerDatos(doc, "DUCTUS-VANO|")) {
        arq::DatoVano dv;
        arq::VanoCantidad v;
        if (!arq::leerDatoVano(t.toStdString(), dv) || !arq::vanoDeDato(t.toStdString(), v.v)) continue;
        v.marca = dv.marca;
        v.tipo = dv.tipo;
        v.alto = dv.alto;
        v.antepecho = dv.antepecho;
        d.vanos.push_back(v);
    }
    return d;
}

// Alto de muro y espesor de losa (se recuerdan).
bool pedirAlturas(QWidget *parent, const arq::Unidades &un, const QString &titulo, double &alto, double &losa,
                  QLineEdit **extra = nullptr, const QString &etiquetaExtra = QString(), const QString &valorExtra = QString()) {
    const bool m = un.sistema == arq::Sistema::Metrico;
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(titulo);
    auto lay = new QVBoxLayout(&dlg);
    auto f = new QFormLayout;
    auto eAlto = new QLineEdit(cfg.value(m ? "DuctusArq/altoMuroM" : "DuctusArq/altoMuroIn", m ? u("2.40") : u("8'-0\"")).toString());
    auto eLosa = new QLineEdit(cfg.value(m ? "DuctusArq/losaM" : "DuctusArq/losaIn", m ? u("0.15") : u("6\"")).toString());
    f->addRow(u("Alto libre de muros (piso a cielo raso):"), eAlto);
    f->addRow(u("Espesor de losa o cubierta:"), eLosa);
    QLineEdit *eExtra = nullptr;
    if (extra) {
        eExtra = new QLineEdit(valorExtra);
        f->addRow(etiquetaExtra, eExtra);
        *extra = eExtra;
    }
    lay->addLayout(f);
    auto b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(b, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(b, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(b);
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return false;
        if (arq::leerLongitud(eAlto->text().toStdString(), un.sistema, alto) && alto > 0
            && arq::leerLongitud(eLosa->text().toStdString(), un.sistema, losa) && losa >= 0)
            break;
        QMessageBox::warning(parent, titulo, u("Revise las medidas."));
    }
    cfg.setValue(m ? "DuctusArq/altoMuroM" : "DuctusArq/altoMuroIn", eAlto->text());
    cfg.setValue(m ? "DuctusArq/losaM" : "DuctusArq/losaIn", eLosa->text());
    return true;
}

void dibujarAlzado(Document_Interface *doc, const QPointF &base, const arq::Alzado &a, const arq::Unidades &un) {
    const double metro = un.metros(1.0);
    da::capa(doc, "A-MUROS");
    for (const auto &c : a.cortado) {
        std::vector<QPointF> q;
        for (const arq::P &p : c) q.push_back(QPointF(base.x() + p.x, base.y() + p.y));
        doc->addSolid(q);
        for (size_t i = 0; i < c.size(); ++i) da::linea(doc, base, c[i], c[(i + 1) % c.size()]);
    }
    for (const arq::Seg &s : a.contorno) da::linea(doc, base, s.a, s.b);
    for (const arq::Seg &s : a.terreno) da::linea(doc, base, s.a, s.b);
    da::capa(doc, "A-PUERTAS");
    for (const arq::Seg &s : a.puertas) da::linea(doc, base, s.a, s.b);
    da::capa(doc, "A-VENTANAS");
    for (const arq::Seg &s : a.ventanas) da::linea(doc, base, s.a, s.b);
    for (const arq::Seg &s : a.vidrio) da::linea(doc, base, s.a, s.b);
    // Niveles: triángulo con la punta en el nivel y el texto al lado.
    da::capa(doc, "A-COTAS");
    const double t = un.papel(2.2);
    for (const auto &n : a.niveles) {
        const arq::P p = n.p;
        da::linea(doc, base, p, arq::P(p.x - t, p.y + t));
        da::linea(doc, base, p, arq::P(p.x + t, p.y + t));
        da::linea(doc, base, arq::P(p.x - t, p.y + t), arq::P(p.x + t, p.y + t));
        da::linea(doc, base, arq::P(p.x - 0.25 * metro, p.y), arq::P(p.x + t * 6, p.y));
        da::texto(doc, base, arq::P(p.x + t * 1.6, p.y + t * 1.6), QString::fromStdString(n.texto), un.papel(2.5),
                  DPI::HAlignLeft, DPI::VAlignBottom);
    }
    // Cota del alto de muro a la izquierda.
    da::cota(doc, base, arq::P(-0.5 * metro, 0), arq::P(-0.5 * metro, a.alto - (a.alto > 0 ? 0 : 0)),
             arq::P(-0.5 * metro - un.papel(8), a.alto));
    da::capa(doc, "A-TEXTOS");
    for (const auto &x : a.textos) da::texto(doc, base, x.first, QString::fromStdString(x.second), un.papel(3));
    da::texto(doc, base, arq::P(a.ancho / 2, -un.papel(10)), QString::fromStdString(a.titulo), un.papel(5));
    da::texto(doc, base, arq::P(a.ancho / 2, -un.papel(17)),
              u("ESCALA ") + QString::fromStdString(arq::textoEscala(un.escala, un.sistema)), un.papel(3));
}

} // namespace

// ---------------------------------------------------------------------------
void DuctusArq::fachadas(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const DatosPlanta d = leerPlanta(doc);
    if (d.muros.empty()) {
        QMessageBox::information(parent, u("Fachadas"),
                                 u("El dibujo no tiene muros de DuctusArq. Dibuje la planta con CASA o MU, y las "
                                   "puertas y ventanas con PUE (con su alto y antepecho)."));
        return;
    }
    double alto = 0, losa = 0;
    if (!pedirAlturas(parent, un, u("Fachadas"), alto, losa)) return;
    QPointF base;
    if (!doc->getPoint(&base, u("Fachadas: clic donde va la esquina de abajo a la izquierda de la primera fachada")))
        return;
    const QString antes = doc->getCurrentLayer();
    const bool ingles = un.sistema == arq::Sistema::Imperial;
    const double sep = un.papel(40);
    double x = 0, y = 0, altoFila = 0;
    int n = 0;
    for (arq::Lado l : {arq::Lado::Frente, arq::Lado::Fondo, arq::Lado::Izquierda, arq::Lado::Derecha}) {
        const arq::Alzado a = arq::fachada(d.muros, d.vanos, l, alto, losa, un.metros(1.0), ingles);
        dibujarAlzado(doc, QPointF(base.x() + x, base.y() + y), a, un);
        altoFila = std::max(altoFila, a.alto);
        x += a.ancho + sep;
        if (++n == 2) {  // dos por fila
            x = 0;
            y -= altoFila + sep;
            altoFila = 0;
        }
    }
    doc->setLayer(antes);
    doc->updateView();
}

// ---------------------------------------------------------------------------
void DuctusArq::cortes(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const DatosPlanta d = leerPlanta(doc);
    if (d.muros.empty()) {
        QMessageBox::information(parent, u("Corte"), u("El dibujo no tiene muros de DuctusArq. Dibuje la planta con CASA o MU."));
        return;
    }
    // Letra siguiente (A, B, C...) según los cortes que ya tiene el dibujo.
    const int hechos = da::leerDatos(doc, "DUCTUS-CORTE|").size();
    const QString letraSug = QString(QChar('A' + std::min(hechos, 25)));
    double alto = 0, losa = 0;
    QLineEdit *eLetra = nullptr;
    if (!pedirAlturas(parent, un, u("Corte"), alto, losa, &eLetra, u("Letra del corte:"), letraSug)) return;
    const QString letra = eLetra && !eLetra->text().trimmed().isEmpty() ? eLetra->text().trimmed().toUpper() : letraSug;
    QPointF p1, p2, base;
    if (!doc->getPoint(&p1, u("Corte %1: clic en la planta donde empieza la línea de corte (cruzando los muros)").arg(letra))) return;
    if (!doc->getPoint(&p2, u("Corte %1: clic donde termina la línea de corte").arg(letra), &p1)) return;
    const auto espacios = da::espaciosConForma(doc, un);
    const arq::Alzado a = arq::corte(d.muros, d.vanos, espacios, arq::P(p1.x(), p1.y()), arq::P(p2.x(), p2.y()),
                                     letra.toStdString(), alto, losa, un.metros(1.0));
    // Línea de corte en la planta, con la letra en cada punta y flechas hacia donde se mira.
    const QString antes = doc->getCurrentLayer();
    const bool aLoAncho = std::fabs(p2.x() - p1.x()) >= std::fabs(p2.y() - p1.y());
    const arq::P a1 = aLoAncho ? arq::P(p1.x(), (p1.y() + p2.y()) / 2) : arq::P((p1.x() + p2.x()) / 2, p1.y());
    const arq::P a2 = aLoAncho ? arq::P(p2.x(), a1.y) : arq::P(a1.x, p2.y());
    const arq::P mira = aLoAncho ? arq::P(0, 1) : arq::P(1, 0);
    da::capa(doc, "A-EJES");
    da::linea(doc, QPointF(0, 0), a1, a2);
    da::capa(doc, "A-TEXTOS");
    const double f = un.papel(6);
    for (const arq::P &e : {a1, a2}) {
        da::linea(doc, QPointF(0, 0), e, e + mira * f);
        da::linea(doc, QPointF(0, 0), e + mira * f, e + mira * (f * 0.6) + arq::P(mira.y, mira.x) * (f * 0.25));
        da::linea(doc, QPointF(0, 0), e + mira * f, e + mira * (f * 0.6) - arq::P(mira.y, mira.x) * (f * 0.25));
    }
    da::texto(doc, QPointF(0, 0), a1 - mira * un.papel(4), letra, un.papel(5));
    da::texto(doc, QPointF(0, 0), a2 - mira * un.papel(4), letra + u("'"), un.papel(5));
    da::dato(doc, QPointF(a1.x, a1.y), u("DUCTUS-CORTE|%1").arg(letra), un);
    if (doc->getPoint(&base, u("Corte %1: clic donde va la esquina de abajo a la izquierda del dibujo del corte").arg(letra)))
        dibujarAlzado(doc, base, a, un);
    doc->setLayer(antes);
    doc->updateView();
}
