/*****************************************************************************/
/*  normas.cpp - Revisar normas que se miden en la planta (DuctusArq)       */
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
#include "normas.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

using da::u;

namespace da {
std::vector<arq::EspacioVentas> espaciosConForma(Document_Interface *doc, const arq::Unidades &un);  // espacios.cpp
}

void DuctusArq::revisarNormas(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const bool m = un.sistema == arq::Sistema::Metrico;
    std::vector<arq::VanoNorma> vanos;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-VANO|")) {
        arq::DatoVano d;
        arq::VanoNorma v;
        if (!arq::leerDatoVano(t.toStdString(), d) || !arq::vanoDeDato(t.toStdString(), v.c.v)) continue;
        v.c.marca = d.marca;
        v.c.tipo = d.tipo;
        v.c.alto = d.alto;
        v.c.antepecho = d.antepecho;
        v.aprobacion = d.aprobacion;
        vanos.push_back(v);
    }
    const auto espacios = da::espaciosConForma(doc, un);
    if (espacios.empty()) {
        QMessageBox::information(parent, u("Revisar normas"),
                                 u("Para revisar las normas hacen falta los espacios con su nombre (ESP) y las puertas y "
                                   "ventanas (PUE)."));
        return;
    }
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Revisar normas"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Revisa lo que se puede medir en la planta: ancho de puertas y pasillos, ventanas en alcobas y "
                             "sala, áreas y lados mínimos, alto libre, ventana de escape (EE. UU.) y más. Son avisos de "
                             "ayuda: no reemplazan la revisión del profesional ni la versión vigente de cada norma."));
    nota->setWordWrap(true);
    nota->setMinimumWidth(480);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    auto norma = new QComboBox;
    norma->addItems({u("Colombia (NSR-10, NTC)"), u("Colombia · Bogotá (NSR-10 + Decreto 555)"),
                     u("EE. UU. · Florida (FBC Residential, IRC)"), u("EE. UU. · Miami-Dade (FBC + HVHZ)")});
    norma->setCurrentIndex(cfg.value("DuctusArq/norNorma", m ? 0 : 3).toInt());
    auto tercera = new QCheckBox(u("Modo 3.ª edad y silla de ruedas (puertas de 0,80 m y círculo de giro de 1,50 m)"));
    tercera->setChecked(cfg.value("DuctusArq/norTercera", false).toBool());
    auto alto = new QLineEdit(cfg.value(m ? "DuctusArq/altoMuroM" : "DuctusArq/altoMuroIn", m ? u("2.40") : u("8'-0\"")).toString());
    f->addRow(u("Norma:"), norma);
    f->addRow(u("Alto libre (piso a cielo raso):"), alto);
    f->addRow(tercera);
    lay->addLayout(f);
    auto b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    b->button(QDialogButtonBox::Ok)->setText(u("Revisar"));
    QObject::connect(b, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(b, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(b);
    double h = 0;
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return;
        if (arq::leerLongitud(alto->text().toStdString(), un.sistema, h) && h > 0) break;
        QMessageBox::warning(parent, u("Revisar normas"), u("No entendí el alto «%1».").arg(alto->text()));
    }
    cfg.setValue("DuctusArq/norNorma", norma->currentIndex());
    cfg.setValue("DuctusArq/norTercera", tercera->isChecked());
    cfg.setValue(m ? "DuctusArq/altoMuroM" : "DuctusArq/altoMuroIn", alto->text());

    const auto avisos = arq::revisarNormas(espacios, vanos, h, un.metros(1.0), static_cast<arq::Norma>(norma->currentIndex()),
                                           tercera->isChecked());
    QString html = u("<p><b>%1</b>%2 · %3 espacio(s), %4 puerta(s)/ventana(s).</p><ul>")
                       .arg(norma->currentText(), tercera->isChecked() ? u(" · modo 3.ª edad") : QString())
                       .arg(espacios.size()).arg(vanos.size());
    int malos = 0;
    for (const auto &a : avisos) {
        const char *color = a.nivel == 2 ? "#b00020" : a.nivel == 1 ? "#a06000" : "#1b7a3a";
        const QString que = a.nivel == 2 ? u("NO CUMPLE") : a.nivel == 1 ? u("REVISAR") : u("BIEN");
        if (a.nivel == 2) ++malos;
        html += u("<li><span style='color:%1'><b>%2:</b></span> %3%4</li>")
                    .arg(color, que, QString::fromStdString(a.texto).toHtmlEscaped(),
                         a.fuente.empty() ? QString() : u(" <i>(%1)</i>").arg(QString::fromStdString(a.fuente).toHtmlEscaped()));
    }
    html += u("</ul><p><i>Fuentes y detalle: carpeta «Investigacion normas» de DuctusArq. Confirme siempre la versión "
              "vigente de la norma y lo que pida la curaduría o el municipio.</i></p>");
    QMessageBox box(malos > 0 ? QMessageBox::Warning : QMessageBox::Information, u("Revisar normas"), html,
                    QMessageBox::Ok, parent);
    box.setTextFormat(Qt::RichText);
    box.exec();
}
