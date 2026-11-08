/*****************************************************************************/
/*  herramientas.cpp - Matriz y Arandela (estilo AutoCAD) de EDWIN CNC        */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC                                             */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#include "edwincnc.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <cmath>

namespace {

const double PI_ = 3.14159265358979323846;

QDoubleSpinBox *numero(double minimo, double maximo, double valor, int decimales = 2) {
    auto s = new QDoubleSpinBox;
    s->setRange(minimo, maximo);
    s->setDecimals(decimales);
    s->setValue(valor);
    s->setSuffix(" mm");
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// MATRIZ (AutoCAD: MATRIZ / ARRAY): copias en filas y columnas, o alrededor de un centro.
void EdwinCnc::matriz(Document_Interface *doc, QWidget *parent)
{
    QList<Plug_Entity *> sel;
    if (!doc->getSelect(&sel, QString::fromUtf8("Matriz: seleccione los objetos y presione Enter"))
        || sel.isEmpty()) {
        qDeleteAll(sel);
        return;
    }

    QSettings cfg;
    cfg.beginGroup("EdwinCnc/Matriz");

    QDialog dlg(parent);
    dlg.setWindowTitle(QString::fromUtf8("Matriz"));
    auto lay = new QVBoxLayout(&dlg);
    lay->addWidget(new QLabel(QString::fromUtf8("%1 objeto(s) seleccionado(s).").arg(sel.size())));

    auto tipo = new QComboBox;
    tipo->addItem(QString::fromUtf8("Matriz rectangular (filas y columnas)"));
    tipo->addItem(QString::fromUtf8("Matriz polar (alrededor de un centro)"));
    tipo->setCurrentIndex(cfg.value("tipo", 0).toInt());
    lay->addWidget(tipo);

    auto pila = new QStackedWidget;

    // Rectangular
    auto pRect = new QWidget;
    auto fRect = new QFormLayout(pRect);
    auto filas = new QSpinBox; filas->setRange(1, 500); filas->setValue(cfg.value("filas", 2).toInt());
    auto columnas = new QSpinBox; columnas->setRange(1, 500); columnas->setValue(cfg.value("columnas", 3).toInt());
    auto dFilas = numero(-100000, 100000, cfg.value("distFilas", 50.0).toDouble(), 3);
    auto dColumnas = numero(-100000, 100000, cfg.value("distColumnas", 50.0).toDouble(), 3);
    fRect->addRow(QString::fromUtf8("Filas:"), filas);
    fRect->addRow(QString::fromUtf8("Columnas:"), columnas);
    fRect->addRow(QString::fromUtf8("Distancia entre filas (hacia arriba):"), dFilas);
    fRect->addRow(QString::fromUtf8("Distancia entre columnas (a la derecha):"), dColumnas);
    auto ayudaRect = new QLabel(QString::fromUtf8(
        "La distancia va de un objeto al siguiente (de centro a centro).<br>"
        "Use números negativos para ir hacia abajo o a la izquierda."));
    ayudaRect->setWordWrap(true);
    fRect->addRow(ayudaRect);
    pila->addWidget(pRect);

    // Polar
    auto pPolar = new QWidget;
    auto fPolar = new QFormLayout(pPolar);
    auto total = new QSpinBox; total->setRange(2, 3600); total->setValue(cfg.value("total", 6).toInt());
    auto angulo = new QDoubleSpinBox; angulo->setRange(-360, 360); angulo->setDecimals(2);
    angulo->setSuffix(QString::fromUtf8(" °")); angulo->setValue(cfg.value("angulo", 360.0).toDouble());
    fPolar->addRow(QString::fromUtf8("Número total de elementos (con el original):"), total);
    fPolar->addRow(QString::fromUtf8("Ángulo para llenar:"), angulo);
    auto ayudaPolar = new QLabel(QString::fromUtf8(
        "Después de Aceptar, haga clic en el <b>centro</b> de la matriz.<br>"
        "Ángulo positivo = contra las agujas del reloj. Las copias se giran.<br>"
        "Ejemplo: brida con 8 huecos → seleccione un hueco, 8 elementos, 360°, "
        "y clic en el centro de la brida."));
    ayudaPolar->setWordWrap(true);
    fPolar->addRow(ayudaPolar);
    pila->addWidget(pPolar);

    pila->setCurrentIndex(tipo->currentIndex());
    QObject::connect(tipo, QOverload<int>::of(&QComboBox::currentIndexChanged), pila, &QStackedWidget::setCurrentIndex);
    lay->addWidget(pila);

    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Aceptar"));
    botones->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);

    if (dlg.exec() != QDialog::Accepted) {
        qDeleteAll(sel);
        return;
    }

    cfg.setValue("tipo", tipo->currentIndex());
    cfg.setValue("filas", filas->value());
    cfg.setValue("columnas", columnas->value());
    cfg.setValue("distFilas", dFilas->value());
    cfg.setValue("distColumnas", dColumnas->value());
    cfg.setValue("total", total->value());
    cfg.setValue("angulo", angulo->value());

    if (tipo->currentIndex() == 0) {
        const int nf = filas->value(), nc = columnas->value();
        const long copias = long(nf) * nc - 1;
        if (copias <= 0) { qDeleteAll(sel); return; }
        if (copias * sel.size() > 20000) {
            QMessageBox::warning(parent, QString::fromUtf8("Matriz"),
                QString::fromUtf8("Son demasiadas copias (%1). Use menos filas o columnas.").arg(copias * sel.size()));
            qDeleteAll(sel);
            return;
        }
        for (Plug_Entity *e : sel) {
            // Cada move() deja el anterior y apunta a la copia nueva: se avanza de copia en copia.
            QPointF actual(0, 0);
            for (int f = 0; f < nf; ++f) {
                for (int c = 0; c < nc; ++c) {
                    if (f == 0 && c == 0) continue;
                    const QPointF destino(c * dColumnas->value(), f * dFilas->value());
                    e->move(destino - actual, DPI::KEEP_ORIGINAL);
                    actual = destino;
                }
            }
        }
    } else {
        QPointF centro;
        if (!doc->getPoint(&centro, QString::fromUtf8("Matriz polar: haga clic en el centro"))) {
            qDeleteAll(sel);
            return;
        }
        const int n = total->value();
        const double grados = angulo->value();
        // 360° reparte en partes iguales sin repetir la primera; otro ángulo incluye los dos extremos.
        const double paso = (std::fabs(std::fabs(grados) - 360.0) < 1e-9) ? grados / n : grados / (n - 1);
        const double pasoRad = paso * PI_ / 180.0;
        for (Plug_Entity *e : sel) {
            for (int i = 1; i < n; ++i)
                e->rotate(centro, pasoRad, DPI::KEEP_ORIGINAL);
        }
    }

    qDeleteAll(sel);
    doc->updateView();
}

// ---------------------------------------------------------------------------
// ARANDELA (AutoCAD: ARANDELA / DONUT). Para plasma se dibuja como dos círculos:
// el contorno de afuera y el hueco. Diámetro interior 0 = disco sin hueco.
void EdwinCnc::arandela(Document_Interface *doc, QWidget *parent)
{
    QSettings cfg;
    cfg.beginGroup("EdwinCnc/Arandela");

    QDialog dlg(parent);
    dlg.setWindowTitle(QString::fromUtf8("Arandela"));
    auto lay = new QVBoxLayout(&dlg);
    auto form = new QFormLayout;
    auto interior = numero(0, 100000, cfg.value("interior", 10.0).toDouble(), 3);
    auto exterior = numero(0.01, 100000, cfg.value("exterior", 30.0).toDouble(), 3);
    form->addRow(QString::fromUtf8("Diámetro interior (hueco):"), interior);
    form->addRow(QString::fromUtf8("Diámetro exterior:"), exterior);
    lay->addLayout(form);
    auto ayuda = new QLabel(QString::fromUtf8(
        "Después de Aceptar, haga clic donde va el centro de cada arandela.<br>"
        "Para terminar presione Enter, Esc o clic derecho.<br>"
        "Diámetro interior 0 = disco sin hueco."));
    ayuda->setWordWrap(true);
    lay->addWidget(ayuda);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Aceptar"));
    botones->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);

    if (dlg.exec() != QDialog::Accepted) return;
    const double di = interior->value(), de = exterior->value();
    if (di >= de) {
        QMessageBox::warning(parent, QString::fromUtf8("Arandela"),
            QString::fromUtf8("El diámetro interior debe ser menor que el exterior."));
        return;
    }
    cfg.setValue("interior", di);
    cfg.setValue("exterior", de);

    QPointF centro;
    while (doc->getPoint(&centro, QString::fromUtf8("Arandela: centro (Enter para terminar)"))) {
        doc->addCircle(&centro, de / 2.0);
        if (di > 0) doc->addCircle(&centro, di / 2.0);
        doc->updateView();
    }
}
