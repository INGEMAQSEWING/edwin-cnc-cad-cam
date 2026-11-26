/*****************************************************************************/
/*  rotulo.cpp - Rótulo / cajetín por especialidad de DuctusArq 2D           */
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
// Hoja con marco y rótulo abajo a la derecha (180 x 60 mm; 180 x 100 mm en A1, A0, ARCH D y
// ARCH E, con tabla de revisiones y sello de la curaduría / permit stamp). Casillas según
// ISO 7200, lo que piden las curadurías en Colombia (Res. 462 de 2017: planos rotulados y
// firmados, matrícula profesional, escala) y el número de plancha del US National CAD
// Standard (A-101, S-201, E-301 ...). Ver C:\EDWIN DEVELOPMENTS\DuctusArq\investigacion-rotulos.md.

#include "ductusarq.h"
#include "comun.h"

#include <QComboBox>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>
#include <map>

using da::u;

namespace {

// Alto de texto (mm) para que el valor quepa en el ancho de la casilla.
double altoQueCabe(const QString &t, double alto, double ancho) {
    if (t.isEmpty()) return alto;
    const double porLetra = 0.82;  // ancho medio de una letra / alto, fuente "standard"
    return std::max(1.2, std::min(alto, ancho / (t.size() * porLetra)));
}

} // namespace

void DuctusArq::rotulo(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    QSettings cfg;
    cfg.beginGroup("DuctusArq/Rotulo");

    QDialog dlg(parent);
    dlg.setWindowTitle(u("Rótulo / cajetín"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Dibuja la hoja con su marco y el rótulo abajo a la derecha, a la escala "
                             "del dibujo (%1). Después de dibujar, mueva su planta dentro del marco.")
                               .arg(QString::fromStdString(arq::textoEscala(un.escala, un.sistema))));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto grid = new QGridLayout;
    lay->addLayout(grid);
    int fila = 0;
    auto agregar = [&](const QString &etiqueta, QWidget *w, int col = 0) {
        grid->addWidget(new QLabel(etiqueta), fila, col);
        grid->addWidget(w, fila, col + 1);
        if (col == 2) ++fila;
    };

    auto hoja = new QComboBox;
    for (const arq::Hoja &h : arq::hojas()) hoja->addItem(u(h.nombre));
    hoja->setCurrentIndex(cfg.value("hoja", un.sistema == arq::Sistema::Imperial ? 8 : 1).toInt());
    auto orient = new QComboBox;
    orient->addItem(u("Horizontal"));
    orient->addItem(u("Vertical"));
    orient->setCurrentIndex(cfg.value("orientacion", 0).toInt());
    agregar(u("Hoja:"), hoja, 0);
    agregar(u("Orientación:"), orient, 2);

    auto perfil = new QComboBox;
    perfil->addItem(u("Códigos de EE. UU. / NCS (A, S, E, P ...)"));
    perfil->addItem(u("Códigos locales Colombia / México (ARQ, EST, IE, HS ...)"));
    perfil->setCurrentIndex(cfg.value("perfil", 0).toInt());
    auto disc = new QComboBox;
    for (const arq::Disciplina &d : arq::disciplinas()) disc->addItem(u(d.espanol));
    disc->setCurrentIndex(cfg.value("disciplina", 0).toInt());
    agregar(u("Códigos:"), perfil, 0);
    agregar(u("Especialidad:"), disc, 2);

    auto numero = new QSpinBox;
    numero->setRange(0, 9999);
    numero->setValue(cfg.value("numero", 101).toInt());
    numero->setToolTip(u("EE. UU.: 101 = primer plano en planta (A-101). Local: 1, 2, 3 ..."));
    auto plancha = new QLabel;
    auto actualizarPlancha = [&]() {
        const arq::Disciplina &d = arq::disciplinas()[static_cast<size_t>(std::max(0, disc->currentIndex()))];
        plancha->setText("<b>" + QString::fromStdString(arq::numeroPlancha(perfil->currentIndex() == 0 ? d.letra : d.local,
                                                                           numero->value())) + "</b>");
    };
    actualizarPlancha();
    QObject::connect(numero, QOverload<int>::of(&QSpinBox::valueChanged), &dlg, actualizarPlancha);
    QObject::connect(disc, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, actualizarPlancha);
    QObject::connect(perfil, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, actualizarPlancha);
    agregar(u("Número de plancha:"), numero, 0);
    agregar(u("Queda:"), plancha, 2);

    auto hojaN = new QSpinBox; hojaN->setRange(1, 999); hojaN->setValue(cfg.value("hojaN", 1).toInt());
    auto hojaT = new QSpinBox; hojaT->setRange(1, 999); hojaT->setValue(cfg.value("hojaT", 1).toInt());
    agregar(u("Hoja número:"), hojaN, 0);
    agregar(u("De (total):"), hojaT, 2);

    std::map<std::string, QLineEdit *> campos;
    auto texto = [&](const char *clave, const QString &etiqueta, const QString &porDefecto, int col) {
        auto e = new QLineEdit(cfg.value(clave, porDefecto).toString());
        campos[clave] = e;
        agregar(etiqueta, e, col);
    };
    texto("proyecto", u("Proyecto:"), u("CASA"), 0);
    texto("contenido", u("Contenido de la plancha:"), u("PLANTA ARQUITECTÓNICA PRIMER PISO"), 2);
    texto("direccion", u("Dirección del predio:"), QString(), 0);
    texto("propietario", u("Propietario:"), QString(), 2);
    texto("profesional", u("Profesional responsable:"), QString(), 0);
    texto("matricula", u("Matrícula profesional / License:"), QString(), 2);
    texto("empresa", u("Empresa:"), QString(), 0);
    texto("fase", u("Fase:"), u("LICENCIA DE CONSTRUCCIÓN"), 2);
    texto("elaboro", u("Elaboró:"), QString(), 0);
    texto("reviso", u("Revisó / aprobó:"), QString(), 2);
    texto("revision", u("Revisión:"), u("0"), 0);
    auto fecha = new QLineEdit(QDate::currentDate().toString("yyyy-MM-dd"));
    agregar(u("Fecha:"), fecha, 2);

    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Dibujar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;

    cfg.setValue("hoja", hoja->currentIndex());
    cfg.setValue("orientacion", orient->currentIndex());
    cfg.setValue("perfil", perfil->currentIndex());
    cfg.setValue("disciplina", disc->currentIndex());
    cfg.setValue("numero", numero->value());
    cfg.setValue("hojaN", hojaN->value());
    cfg.setValue("hojaT", hojaT->value());
    for (const auto &c : campos) cfg.setValue(c.first.c_str(), c.second->text());
    cfg.endGroup();

    QPointF base;
    if (!doc->getPoint(&base, u("Rótulo: haga clic donde va la esquina de abajo a la izquierda de la hoja"))) return;

    const arq::Hoja &h = arq::hojas()[static_cast<size_t>(hoja->currentIndex())];
    const bool vertical = orient->currentIndex() == 1;
    const double W = vertical ? h.alto : h.ancho, H = vertical ? h.ancho : h.alto;  // mm
    double mIzq, mOtros;
    arq::margenes(h, mIzq, mOtros);
    const arq::Rotulo r = arq::rotulo(h.grande);
    auto P = [&](double xmm, double ymm) { return arq::P(un.papel(xmm), un.papel(ymm)); };
    auto R = [&](double x1, double y1, double x2, double y2) {
        return arq::rect(un.papel(x1), un.papel(y1), un.papel(x2), un.papel(y2));
    };

    da::capa(doc, "G-ROTULO");
    da::rectangulo(doc, base, R(0, 0, W, H));                              // borde de la hoja
    da::rectangulo(doc, base, R(mIzq, mOtros, W - mOtros, H - mOtros));    // marco
    const double ox = W - mOtros - r.ancho, oy = mOtros;                   // rótulo abajo a la derecha

    const arq::Disciplina &d = arq::disciplinas()[static_cast<size_t>(disc->currentIndex())];
    const QString codigo = QString::fromStdString(arq::numeroPlancha(perfil->currentIndex() == 0 ? d.letra : d.local,
                                                                      numero->value()));
    auto valor = [&](const std::string &clave) -> QString {
        if (clave == "plancha") return codigo;
        if (clave == "hoja") return u("%1 de %2").arg(hojaN->value()).arg(hojaT->value());
        if (clave == "escala") return QString::fromStdString(arq::textoEscala(un.escala, un.sistema));
        if (clave == "fecha") return fecha->text();
        if (clave == "disciplina") return u(d.espanol).toUpper() + u(" / ") + u(d.ingles).toUpper();
        if (clave == "revisiones" || clave == "sello") return QString();
        auto it = campos.find(clave);
        return it == campos.end() ? QString() : it->second->text().trimmed().toUpper();
    };

    const double etiqueta = 1.6;  // mm
    for (const arq::Casilla &c : r.casillas) {
        da::capa(doc, "G-ROTULO");
        da::rectangulo(doc, base, R(ox + c.x, oy + c.y, ox + c.x + c.w, oy + c.y + c.h));
        da::capa(doc, "A-TEXTOS");
        da::texto(doc, base, P(ox + c.x + 1, oy + c.y + c.h - 1), QString::fromStdString(c.etiqueta),
                  un.papel(altoQueCabe(QString::fromStdString(c.etiqueta), etiqueta, c.w - 2)),
                  DPI::HAlignLeft, DPI::VAlignTop);
        const double cx = ox + c.x + c.w / 2;
        const double cy = oy + c.y + (c.h - etiqueta - 1) / 2;  // centro del espacio bajo la etiqueta
        if (c.clave == "profesional") {
            const QString nom = valor("profesional"), mat = campos["matricula"]->text().trimmed().toUpper();
            if (!nom.isEmpty())
                da::texto(doc, base, P(cx, oy + c.y + 9), nom, un.papel(altoQueCabe(nom, c.alto, c.w - 3)));
            if (!mat.isEmpty())
                da::texto(doc, base, P(cx, oy + c.y + 4), u("MP ") + mat, un.papel(altoQueCabe(u("MP ") + mat, 2.0, c.w - 3)));
            continue;
        }
        if (c.clave == "revisiones") {
            da::capa(doc, "G-ROTULO");
            // Renglones de la tabla de revisiones: 4 de 7 mm bajo la etiqueta.
            for (int k = 1; k <= 4; ++k)
                da::linea(doc, base, P(ox + c.x, oy + c.y + c.h - 4 - 7.0 * k), P(ox + c.x + c.w, oy + c.y + c.h - 4 - 7.0 * k));
            for (double col : {12.0, 37.0, 105.0})
                da::linea(doc, base, P(ox + c.x + col, oy + c.y), P(ox + c.x + col, oy + c.y + c.h - 4));
            da::capa(doc, "A-TEXTOS");
            const QString rev = campos["revision"]->text().trimmed().toUpper();
            if (!rev.isEmpty()) {
                const double y = oy + c.y + c.h - 4 - 3.5;
                da::texto(doc, base, P(ox + c.x + 6, y), rev, un.papel(2.0));
                da::texto(doc, base, P(ox + c.x + 24.5, y), fecha->text(), un.papel(2.0));
                da::texto(doc, base, P(ox + c.x + 39, y), u("EMISIÓN INICIAL"), un.papel(2.0), DPI::HAlignLeft);
            }
            continue;
        }
        const QString t = valor(c.clave);
        if (!t.isEmpty())
            da::texto(doc, base, P(cx, cy), t, un.papel(altoQueCabe(t, c.alto, c.w - 3)));
    }
    da::capa(doc, "G-ROTULO");
    doc->updateView();
}
