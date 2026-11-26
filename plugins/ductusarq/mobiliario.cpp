/*****************************************************************************/
/*  mobiliario.cpp - Anexar mobiliario, electrodomésticos y sanitarios       */
/*                   (DuctusArq 2D)                                          */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#include "ductusarq.h"
#include "comun.h"
#include "muebles.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

using da::u;

void DuctusArq::anexarMueble(Document_Interface *doc, QWidget *parent, int grupoInicial)
{
    const arq::Unidades un = da::unidades();
    const bool imperial = un.sistema == arq::Sistema::Imperial;
    QSettings cfg;

    QDialog dlg(parent);
    dlg.setWindowTitle(u("Anexar mobiliario y electrodomésticos"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Escoja la pieza, revise la medida y dé clic en «Colocar». Después haga clic donde va "
                             "el CENTRO de la pieza; puede poner varias seguidas (Enter o Esc termina). "
                             "El frente de la pieza mira hacia abajo con giro 0°.<br><i>%1</i>").arg(da::ayudaUnidades(un)));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto grupo = new QComboBox;
    grupo->addItem(u("Mobiliario (camas, sofás, comedor, closet ...)"));
    grupo->addItem(u("Electrodomésticos / appliances (nevera, estufa, lavadora ...)"));
    grupo->addItem(u("Aparatos sanitarios (sanitario, lavamanos, ducha, tina ...)"));
    grupo->setCurrentIndex(grupoInicial >= 0 ? grupoInicial : cfg.value("DuctusArq/mueGrupo", 0).toInt());
    lay->addWidget(grupo);
    auto lista = new QListWidget;
    lista->setMinimumHeight(220);
    lay->addWidget(lista);
    auto f = new QFormLayout;
    lay->addLayout(f);
    auto ancho = new QLineEdit;
    auto fondo = new QLineEdit;
    auto giro = new QComboBox;
    for (const char *g : {"0°", "90°", "180°", "270°"}) giro->addItem(u(g));
    giro->setCurrentIndex(cfg.value("DuctusArq/mueGiro", 0).toInt());
    f->addRow(u("Ancho (X):"), ancho);
    f->addRow(u("Fondo (Y):"), fondo);
    f->addRow(u("Giro:"), giro);

    std::vector<const arq::Pieza *> visibles;
    auto llenar = [&]() {
        lista->clear();
        visibles.clear();
        for (const arq::Pieza &p : arq::catalogo())
            if (static_cast<int>(p.grupo) == grupo->currentIndex()) {
                visibles.push_back(&p);
                const QString medida = da::textoLong(imperial ? p.anchoIn : p.anchoM, un) + u(" x ")
                    + da::textoLong(imperial ? p.fondoIn : p.fondoM, un);
                lista->addItem(u(p.nombre) + u("  /  ") + u(p.ingles) + u("   (") + medida + u(")"));
            }
        lista->setCurrentRow(0);
    };
    QObject::connect(lista, &QListWidget::currentRowChanged, &dlg, [&](int fila) {
        if (fila < 0 || fila >= static_cast<int>(visibles.size())) return;
        const arq::Pieza &p = *visibles[static_cast<size_t>(fila)];
        ancho->setText(da::textoLong(imperial ? p.anchoIn : p.anchoM, un));
        fondo->setText(da::textoLong(imperial ? p.fondoIn : p.fondoM, un));
    });
    QObject::connect(grupo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int) { llenar(); });
    llenar();
    const QString ultima = cfg.value("DuctusArq/muePieza").toString();
    for (size_t i = 0; i < visibles.size(); ++i)
        if (ultima == u(visibles[i]->id)) lista->setCurrentRow(static_cast<int>(i));

    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Colocar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(lista, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    lay->addWidget(botones);

    double w = 0, fo = 0;
    const arq::Pieza *pz = nullptr;
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return;
        const int fila = lista->currentRow();
        if (fila < 0 || fila >= static_cast<int>(visibles.size())) continue;
        pz = visibles[static_cast<size_t>(fila)];
        if (!da::medida(parent, ancho, un, w, 0.0001, u("Ancho"))) continue;
        if (!da::medida(parent, fondo, un, fo, 0.0001, u("Fondo"))) continue;
        break;
    }
    cfg.setValue("DuctusArq/mueGrupo", grupo->currentIndex());
    cfg.setValue("DuctusArq/mueGiro", giro->currentIndex());
    cfg.setValue("DuctusArq/muePieza", u(pz->id));
    const double grados = 90.0 * giro->currentIndex();
    const double alto = imperial ? pz->altoIn : pz->altoM;
    const double detalle = un.metros(0.05);
    const arq::Dibujo local = arq::dibujarPieza(*pz, w, fo, detalle, imperial);

    const QString capaAntes = doc->getCurrentLayer();
    QPointF c;
    int n = 0;
    while (doc->getPoint(&c, u("%1: clic en el centro (Enter o Esc para terminar)").arg(u(pz->nombre)))) {
        const arq::Dibujo d = arq::colocar(local, arq::P(c.x(), c.y()), grados);
        da::capa(doc, arq::capaGrupo(pz->grupo));
        for (const arq::Seg &s : d.lineas) da::linea(doc, QPointF(0, 0), s.a, s.b);
        for (const auto &k : d.circulos) {
            QPointF cc(k.first.x, k.first.y);
            doc->addCircle(&cc, k.second);
        }
        for (const auto &t : d.textos)
            da::texto(doc, QPointF(0, 0), t.first, QString::fromStdString(t.second), d.altoTexto,
                      DPI::HAlignCenter, DPI::VAlignMiddle, grados == 180 ? 0 : grados == 270 ? 90 : grados);
        // Dato para el render 3D de la planta amoblada (capa A-DATOS, no se imprime).
        const bool existe = doc->getAllLayer().contains("A-DATOS", Qt::CaseInsensitive);
        doc->setLayer("A-DATOS");
        if (!existe) doc->setCurrentLayerProperties(0xC8C8C8, DPI::Width00, DPI::SolidLine);
        QPointF pd = c;
        doc->addText(QString::fromStdString(arq::datoMueble(*pz, arq::P(c.x(), c.y()), grados, w, fo, alto)),
                     "standard", &pd, un.papel(0.6), 0, DPI::HAlignLeft, DPI::VAlignBottom);
        ++n;
        doc->updateView();
    }
    doc->setLayer(capaAntes);
    doc->updateView();
}
