/*****************************************************************************/
/*  insertar_pieza.cpp - EDWIN CNC: insertar pieza de lámina por medidas     */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC                                             */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/
//
// Como las calculadoras de la web, pero dentro del programa: el usuario elige la pieza, escribe las medidas
// y el desarrollo plano aparece en el dibujo (CORTE para cortar, MARCADO para doblar o rolar, REFERENCIA
// para el nombre y las medidas). Los cálculos están en piezas.h (se prueba solo con pruebas_piezas.cpp).

#include "edwincnc.h"
#include "piezas.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

namespace {

QString u(const char *s) { return QString::fromUtf8(s); }
QString u(const std::string &s) { return QString::fromStdString(s); }

struct Campo {
    const char *clave, *texto;
    double minimo, maximo, porDefecto;
    int decimales;
    const char *sufijo;
    const char *ayuda;
};

struct Tipo {
    const char *nombre;
    const char *descripcion;
    std::vector<Campo> campos;
    std::function<piezas::Resultado(const std::vector<double> &)> calcular;
};

std::vector<Tipo> tipos() {
    using namespace piezas;
    return {
        {"Tolva cónica (valde / vagón)", "Cono truncado: boca redonda arriba y salida redonda abajo. Sale en abanicos.",
         {{"ds", "Diámetro de arriba", 1, 100000, 1200, 1, " mm", "Medida interna de la boca."},
          {"di", "Diámetro de abajo", 0, 100000, 600, 1, " mm", "0 = termina en punta."},
          {"h", "Alto", 1, 100000, 900, 1, " mm", "Distancia vertical entre las dos bocas."},
          {"n", "Partir en piezas iguales", 1, 24, 2, 0, "", "1 = una sola pieza; 2 = dos mitades (cuando no cabe en la lámina)."}},
         [](const std::vector<double> &v) { return tolvaConica(v[0], v[1], v[2], int(v[3])); }},
        {"Tolva en U (mezcladora horizontal)", "Canal con fondo de medio círculo: un cuerpo de una lámina y dos tapas en U.",
         {{"ancho", "Ancho", 10, 100000, 600, 1, " mm", "El fondo es medio círculo de este diámetro."},
          {"alto", "Alto total", 5, 100000, 550, 1, " mm", "Desde el fondo hasta el borde de arriba."},
          {"largo", "Largo", 10, 100000, 1200, 1, " mm", "Largo de la tolva (de tapa a tapa)."}},
         [](const std::vector<double> &v) { return tolvaU(v[0], v[1], v[2]); }},
        {"Tolva piramidal (boca cuadrada)", "Boca rectangular arriba y salida rectangular abajo, centradas.",
         {{"A", "Boca, largo", 1, 100000, 800, 1, " mm", ""},
          {"B", "Boca, ancho", 1, 100000, 600, 1, " mm", ""},
          {"a", "Salida, largo", 0, 100000, 200, 1, " mm", "0 = termina en punta."},
          {"b", "Salida, ancho", 0, 100000, 100, 1, " mm", ""},
          {"H", "Alto", 1, 100000, 500, 1, " mm", ""},
          {"p", "Piezas (1, 2 o 4)", 1, 4, 2, 0, "", "1 = una pieza con 3 dobleces; 2 = dos piezas en L; 4 = las cuatro caras."}},
         [](const std::vector<double> &v) { int p = int(v[5]); if (p == 3) p = 2; return tolvaPiramidal(v[0], v[1], v[2], v[3], v[4], p); }},
        {"Cuadrado a redondo", "Transición de boca rectangular arriba a redondo abajo.",
         {{"A", "Rectángulo, largo", 1, 100000, 600, 1, " mm", ""},
          {"B", "Rectángulo, ancho", 1, 100000, 400, 1, " mm", ""},
          {"D", "Diámetro del redondo", 1, 100000, 300, 1, " mm", ""},
          {"H", "Alto", 1, 100000, 400, 1, " mm", ""},
          {"N", "Divisiones por cuarto", 2, 36, 6, 0, "", "Líneas de prensado en cada esquina redonda (más = más suave)."},
          {"p", "Piezas (1, 2 o 4)", 1, 4, 2, 0, "", ""}},
         [](const std::vector<double> &v) { int p = int(v[5]); if (p == 3) p = 2; return cuadradoRedondo(v[0], v[1], v[2], v[3], int(v[4]), p); }},
        {"Reducción excéntrica", "De un tubo a otro más chico, con el de abajo corrido hacia un lado (0 = concéntrica).",
         {{"D1", "Diámetro de arriba", 1, 100000, 600, 1, " mm", ""},
          {"D2", "Diámetro de abajo", 1, 100000, 300, 1, " mm", ""},
          {"H", "Alto", 1, 100000, 500, 1, " mm", ""},
          {"e", "Corrido del centro de abajo", 0, 100000, 150, 1, " mm", "0 = concéntrica."},
          {"N", "Divisiones por cuarto", 2, 36, 6, 0, "", ""},
          {"p", "Piezas (1 o 2)", 1, 2, 1, 0, "", ""}},
         [](const std::vector<double> &v) { return reduccion(v[0], v[1], v[2], v[3], int(v[4]), int(v[5])); }},
        {"Codo de gajos (tubo)", "Codo de tubo hecho con gajos rectos soldados.",
         {{"D", "Diámetro del tubo", 1, 100000, 200, 1, " mm", ""},
          {"ang", "Ángulo del codo", 1, 180, 90, 1, "°", ""},
          {"R", "Radio de la curva", 1, 100000, 300, 1, " mm", "Del centro de la curva al eje del tubo."},
          {"N", "Número de gajos", 2, 30, 4, 0, "", "Los dos de las puntas son medios gajos."},
          {"recto", "Tramo recto en las puntas", 0, 100000, 0, 1, " mm", ""}},
         [](const std::vector<double> &v) { return codoGajos(v[0], v[1], v[2], int(v[3]), v[4]); }},
        {"Boca de pescado (tubo con tubo)", "Corte del ramal que entra a un tubo principal, y el agujero del principal.",
         {{"D", "Diámetro del principal", 1, 100000, 300, 1, " mm", ""},
          {"d", "Diámetro del ramal", 1, 100000, 150, 1, " mm", ""},
          {"g", "Ángulo entre los tubos", 15, 90, 90, 1, "°", "90° = en T."},
          {"e", "Corrido del ramal", -100000, 100000, 0, 1, " mm", "0 = centrado."},
          {"L", "Largo del ramal", 1, 100000, 400, 1, " mm", "Desde el eje del principal hasta la punta."},
          {"Lp", "Largo del principal", 0, 100000, 0, 1, " mm", "0 = solo la plantilla del agujero."}},
         [](const std::vector<double> &v) { return bocaPescado(v[0], v[1], v[2], v[3], v[4], v[5]); }},
        {"Arandela de sinfín (helicoidal)", "Anillo abierto que se estira para formar una vuelta del sinfín.",
         {{"D", "Diámetro exterior del sinfín", 1, 100000, 300, 1, " mm", ""},
          {"d", "Diámetro del eje", 1, 100000, 100, 1, " mm", ""},
          {"P", "Paso (de cresta a cresta)", 1, 100000, 300, 1, " mm", ""},
          {"hol", "Holgura del eje", 0, 50, 1, 1, " mm", "Cuánto más grande queda el hueco que el eje."},
          {"L", "Largo del sinfín", 0, 100000, 0, 1, " mm", "Opcional: calcula cuántas arandelas cortar."}},
         [](const std::vector<double> &v) { return arandelaSinfin(v[0], v[1], v[2], v[3], v[4]); }},
    };
}

} // namespace

void EdwinCnc::insertarPieza(Document_Interface *doc, QWidget *parent)
{
    const QString titulo = u("Insertar pieza por medidas");
    const std::vector<Tipo> lista = tipos();
    QSettings cfg;
    cfg.beginGroup("EdwinCnc/Pieza");

    QDialog dlg(parent);
    dlg.setWindowTitle(titulo);
    auto lay = new QVBoxLayout(&dlg);
    auto elegir = new QComboBox;
    for (const Tipo &t : lista) elegir->addItem(u(t.nombre));
    lay->addWidget(new QLabel(u("<b>¿Qué pieza?</b>")));
    lay->addWidget(elegir);
    auto desc = new QLabel;
    desc->setWordWrap(true);
    lay->addWidget(desc);
    auto pila = new QStackedWidget;
    std::vector<std::vector<QDoubleSpinBox *>> cajas(lista.size());
    for (size_t i = 0; i < lista.size(); ++i) {
        auto pag = new QWidget;
        auto form = new QFormLayout(pag);
        for (const Campo &c : lista[i].campos) {
            auto s = new QDoubleSpinBox;
            s->setRange(c.minimo, c.maximo);
            s->setDecimals(c.decimales);
            s->setSuffix(u(c.sufijo));
            s->setValue(cfg.value(QString::number(i) + "/" + c.clave, c.porDefecto).toDouble());
            if (*c.ayuda) s->setToolTip(u(c.ayuda));
            form->addRow(u(c.texto) + ":", s);
            cajas[i].push_back(s);
        }
        pila->addWidget(pag);
    }
    lay->addWidget(pila);
    auto copias = new QCheckBox(u("Dibujar todas las copias que hay que cortar (para acomodarlas después)"));
    copias->setChecked(cfg.value("copias", true).toBool());
    lay->addWidget(copias);
    auto despues = new QLabel(u("Después de insertarla puede usar <b>EDWIN CNC ▸ Acomodar piezas en la lámina</b> "
                                "y <b>Generar código G</b>."));
    despues->setWordWrap(true);
    lay->addWidget(despues);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Insertar"));
    botones->button(QDialogButtonBox::Cancel)->setText(u("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    auto cambiar = [&](int i) { pila->setCurrentIndex(i); desc->setText(u(lista[size_t(i)].descripcion)); };
    QObject::connect(elegir, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, cambiar);
    const int ultimo = std::max(0, std::min(int(lista.size()) - 1, cfg.value("tipo", 0).toInt()));
    elegir->setCurrentIndex(ultimo);
    cambiar(ultimo);

    piezas::Resultado res;
    int tipo = 0;
    for (;;) {
        if (dlg.exec() != QDialog::Accepted) return;
        tipo = elegir->currentIndex();
        std::vector<double> v;
        for (size_t k = 0; k < cajas[tipo].size(); ++k) {
            v.push_back(cajas[tipo][k]->value());
            cfg.setValue(QString::number(tipo) + "/" + lista[tipo].campos[k].clave, cajas[tipo][k]->value());
        }
        cfg.setValue("tipo", tipo);
        cfg.setValue("copias", copias->isChecked());
        res = lista[tipo].calcular(v);
        if (res.error.empty() && !res.piezas.empty()) break;
        QMessageBox::warning(parent, titulo, u(res.error.empty() ? std::string("No se pudo calcular la pieza.") : res.error));
    }

    QPointF base;
    if (!doc->getPoint(&base, u("Pieza: haga clic donde va la esquina de abajo a la izquierda"))) return;

    crearCapas(doc, parent, false);
    const QString antes = doc->getCurrentLayer();
    double x = base.x();
    const double sep = 50;
    int dibujadas = 0;
    for (const piezas::Plana &p : res.piezas) {
        double x0, y0, x1, y1;
        piezas::caja(p, x0, y0, x1, y1);
        const int n = copias->isChecked() ? std::max(1, p.cantidad) : 1;
        const double alto = y1 - y0;
        for (int k = 0; k < n; ++k) {
            const double dx = x - x0, dy = base.y() - y0;
            doc->setLayer("CORTE");
            for (const piezas::Contorno &c : p.cortes) {
                std::vector<Plug_VertexData> vs;
                for (const piezas::V &q : c) vs.emplace_back(QPointF(q.x + dx, q.y + dy), q.bulge);
                doc->addPolyline(vs, true);
            }
            if (!p.marcas.empty()) {
                doc->setLayer("MARCADO");
                for (const piezas::Linea &l : p.marcas) {
                    QPointF a(l.a.x + dx, l.a.y + dy), b(l.b.x + dx, l.b.y + dy);
                    doc->addLine(&a, &b);
                }
            }
            doc->setLayer("REFERENCIA");
            const double th = std::max(8.0, std::min(40.0, alto * 0.04));
            QPointF t(x, base.y() - th * 1.8);
            QString nombre = u(p.nombre);
            if (p.cantidad > 1) nombre += u(n > 1 ? " (%1 de %2)" : " (cortar %2)").arg(k + 1).arg(p.cantidad);
            doc->addText(nombre, "standard", &t, th, 0, DPI::HAlignLeft, DPI::VAlignTop);
            QPointF t2(x, base.y() - th * 3.4);
            doc->addText(u(p.nota), "standard", &t2, th * 0.8, 0, DPI::HAlignLeft, DPI::VAlignTop);
            x += (x1 - x0) + sep;
            ++dibujadas;
        }
    }
    doc->setLayer(antes);
    doc->updateView();
    (void)dibujadas;
}
