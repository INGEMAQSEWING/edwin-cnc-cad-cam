/*****************************************************************************/
/*  pisos.cpp - Escalera y planta del piso siguiente (DuctusArq)             */
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
// ESC: escalera recta calculada con la norma (contrahuella, huella, 2CH + H) y dibujada en planta.
// PISO: arranca la planta del piso siguiente al lado: copia los muros de fachada, las columnas
// (la estructura sigue) y el hueco de la escalera; después se dibujan los muros interiores.

#include "ductusarq.h"
#include "comun.h"
#include "escaleras.h"
#include "fachadas.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

using da::u;

namespace {

bool leerMedida(QLineEdit *e, const arq::Unidades &un, double &v) {
    return arq::leerLongitud(e->text().toStdString(), un.sistema, v) && v > 0;
}

} // namespace

// ---------------------------------------------------------------------------
void DuctusArq::escalera(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const bool m = un.sistema == arq::Sistema::Metrico;
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Escalera recta"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u(m ? "Calcula los escalones con la NSR-10 (contrahuella de 18 cm o menos, huella de 28 cm o "
                                 "más y 2CH + H = 63 cm) y la dibuja en planta. Primer clic: donde arranca (esquina); "
                                 "segundo clic: hacia dónde sube. El ancho queda a la derecha."
                               : "Calcula los escalones con el IRC (contrahuella de 7 3/4\" o menos y huella de 10\" o "
                                 "más) y la dibuja en planta. Primer clic: donde arranca (esquina); segundo clic: hacia "
                                 "dónde sube. El ancho queda a la derecha."));
    nota->setWordWrap(true);
    nota->setMinimumWidth(460);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    auto eAlto = new QLineEdit(cfg.value(m ? "DuctusArq/escAltoM" : "DuctusArq/escAltoIn", m ? u("2.70") : u("9'-1\"")).toString());
    auto eAncho = new QLineEdit(cfg.value(m ? "DuctusArq/escAnchoM" : "DuctusArq/escAnchoIn", m ? u("0.90") : u("3'-0\"")).toString());
    f->addRow(u("Alto de piso a piso (entrepiso):"), eAlto);
    f->addRow(u("Ancho de la escalera:"), eAncho);
    lay->addLayout(f);
    auto b = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(b, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(b, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(b);
    double alto = 0, ancho = 0;
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return;
        if (leerMedida(eAlto, un, alto) && leerMedida(eAncho, un, ancho)) break;
        QMessageBox::warning(parent, u("Escalera"), u("Revise las medidas."));
    }
    cfg.setValue(m ? "DuctusArq/escAltoM" : "DuctusArq/escAltoIn", eAlto->text());
    cfg.setValue(m ? "DuctusArq/escAnchoM" : "DuctusArq/escAnchoIn", eAncho->text());
    const arq::CalculoEscalera c = arq::calcularEscalera(alto, ancho, un.metros(1.0), un.sistema);
    const QString resumen = u("%1 contrahuellas de %2 · huella de %3 · largo en planta %4")
                                .arg(c.contrahuellas)
                                .arg(QString::fromStdString(arq::textoLongitud(c.contrahuella, un.sistema)),
                                     QString::fromStdString(arq::textoLongitud(c.huella, un.sistema)),
                                     QString::fromStdString(arq::textoLongitud(c.largo, un.sistema)));
    QPointF p1, p2;
    if (!doc->getPoint(&p1, u("Escalera (%1): clic donde arranca").arg(resumen))) return;
    if (!doc->getPoint(&p2, u("Escalera: clic hacia dónde sube"), &p1)) return;
    const arq::PlantaEscalera e = arq::plantaEscalera(arq::P(p1.x(), p1.y()), arq::P(p2.x(), p2.y()), ancho, c);
    const QString antes = doc->getCurrentLayer();
    da::capa(doc, "A-ESCALERAS");
    for (const arq::Seg &s : e.contorno) da::linea(doc, QPointF(0, 0), s.a, s.b);
    for (const arq::Seg &s : e.huellas) da::linea(doc, QPointF(0, 0), s.a, s.b);
    for (const arq::Seg &s : e.flecha) da::linea(doc, QPointF(0, 0), s.a, s.b);
    for (const arq::Seg &s : e.corte) da::linea(doc, QPointF(0, 0), s.a, s.b);
    da::capa(doc, "A-TEXTOS");
    da::texto(doc, QPointF(0, 0), e.textoSube, m ? u("SUBE") : u("UP"), un.papel(2.5), DPI::HAlignLeft);
    // Dato para revisar y para el piso siguiente: contrahuellas, contrahuella, huella, ancho y contorno.
    QStringList pts;
    for (const arq::Seg &s : e.contorno) pts << QString::number(s.a.x, 'f', 4) << QString::number(s.a.y, 'f', 4);
    da::dato(doc, p1, u("DUCTUS-ESCALERA|%1|%2|%3|%4|%5").arg(c.contrahuellas).arg(c.contrahuella, 0, 'f', 4)
                          .arg(c.huella, 0, 'f', 4).arg(ancho, 0, 'f', 4).arg(pts.join(',')), un);
    doc->setLayer(antes);
    doc->updateView();
    QString t = u("Escalera dibujada:\n\n• ") + resumen;
    for (const std::string &a : c.avisos) t += u("\n• Ojo: ") + QString::fromStdString(a);
    t += u("\n\nRevise que arriba quede un descanso libre del ancho de la escalera.");
    QMessageBox::information(parent, u("Escalera"), t);
}

// ---------------------------------------------------------------------------
void DuctusArq::pisoSiguiente(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    std::vector<std::vector<arq::P>> muros;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) muros.push_back(pol);
    }
    if (muros.empty()) {
        QMessageBox::information(parent, u("Piso siguiente"), u("Primero dibuje la planta del primer piso con CASA o MU."));
        return;
    }
    QSettings cfg;
    bool ok = false;
    const int piso = QInputDialog::getInt(parent, u("Piso siguiente"),
                                          u("Número del piso nuevo (copia los muros de fachada, las columnas y el hueco de "
                                            "la escalera; los muros interiores se dibujan después con MU):"),
                                          cfg.value("DuctusArq/pisoNuevo", 2).toInt(), 2, 99, 1, &ok);
    if (!ok) return;
    // Planta de origen: si hay varias, la que se escoja con un clic.
    const auto plantas = arq::plantasSeparadas(muros, un.metros(0.02));
    std::vector<size_t> grupo = plantas.front();
    if (plantas.size() > 1) {
        QPointF dentro;
        if (!doc->getPoint(&dentro, u("Piso siguiente: clic DENTRO de la planta que se copia"))) return;
        for (const auto &g : plantas) {
            std::vector<std::vector<arq::P>> mm;
            for (size_t i : g) mm.push_back(muros[i]);
            const arq::Rect B = arq::cajaDe(mm);
            if (dentro.x() >= B.x1 && dentro.x() <= B.x2 && dentro.y() >= B.y1 && dentro.y() <= B.y2) grupo = g;
        }
    }
    std::vector<std::vector<arq::P>> origen;
    for (size_t i : grupo) origen.push_back(muros[i]);
    const arq::Rect B = arq::cajaDe(origen);
    const double tol = un.metros(0.02), ladoColumna = un.metros(0.80);
    std::vector<arq::Rect> copia, columnas;
    for (const auto &w : origen) {
        const arq::Rect r = arq::cajaDe({w});
        const double a = r.x2 - r.x1, h = r.y2 - r.y1;
        const bool esColumna = std::max(a, h) <= ladoColumna && std::min(a, h) >= 0.6 * std::max(a, h);
        const bool fachada = r.x1 <= B.x1 + tol || r.x2 >= B.x2 - tol || r.y1 <= B.y1 + tol || r.y2 >= B.y2 - tol;
        if (esColumna) columnas.push_back(r);
        else if (fachada) copia.push_back(r);
    }
    QPointF base;
    if (!doc->getPoint(&base, u("Piso %1: clic donde va la esquina de abajo a la izquierda de la planta nueva "
                                "(por ejemplo a la derecha del piso de abajo)").arg(piso)))
        return;
    cfg.setValue("DuctusArq/pisoNuevo", piso + 1);
    cfg.setValue("DuctusArq/espPiso", piso);  // ESP propone este piso
    const QPointF d(base.x() - B.x1, base.y() - B.y1);
    const QString antes = doc->getCurrentLayer();
    std::vector<arq::Rect> todo = copia;
    todo.insert(todo.end(), columnas.begin(), columnas.end());
    da::capa(doc, "A-MUROS");
    da::segmentos(doc, d, arq::contornoUnion(todo));
    if (!columnas.empty()) {
        da::capa(doc, "A-COLUMNAS");
        for (const arq::Rect &r : columnas) {
            da::rectangulo(doc, d, r);
            da::relleno(doc, d, r);
        }
    }
    {
        std::vector<std::vector<arq::P>> polis;
        for (const arq::Rect &r : todo) polis.push_back(arq::esquinas(r));
        da::datosMuros(doc, d, polis, un);
    }
    // Hueco de la escalera (vacío) arriba de cada escalera de esta planta.
    int escaleras = 0;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-ESCALERA|")) {
        const QStringList p = t.split('|');
        if (p.size() < 6) continue;
        const QStringList c = p[5].split(',');
        if (c.size() < 8) continue;
        std::vector<arq::P> q;
        for (int i = 0; i + 1 < c.size(); i += 2) q.emplace_back(c[i].toDouble(), c[i + 1].toDouble());
        const arq::Rect r = arq::cajaDe({q});
        if (r.x1 < B.x1 - tol || r.x2 > B.x2 + tol || r.y1 < B.y1 - tol || r.y2 > B.y2 + tol) continue;
        da::capa(doc, "A-ESCALERAS");
        for (size_t i = 0; i < q.size(); ++i) da::linea(doc, d, q[i], q[(i + 1) % q.size()]);
        da::linea(doc, d, arq::P(r.x1, r.y1), arq::P(r.x2, r.y2));
        da::linea(doc, d, arq::P(r.x1, r.y2), arq::P(r.x2, r.y1));
        da::capa(doc, "A-TEXTOS");
        da::texto(doc, d, arq::P((r.x1 + r.x2) / 2, (r.y1 + r.y2) / 2), u("VACÍO ESCALERA"), un.papel(2.5));
        ++escaleras;
    }
    da::capa(doc, "A-TEXTOS");
    da::texto(doc, d, arq::P((B.x1 + B.x2) / 2, B.y1 - un.papel(14)),
              u("PLANTA ARQUITECTÓNICA PISO %1").arg(piso), un.papel(5), DPI::HAlignCenter, DPI::VAlignTop);
    doc->setLayer(antes);
    doc->updateView();
    QMessageBox::information(parent, u("Piso siguiente"),
                             u("Listo: piso %1 con %2 muro(s) de fachada, %3 columna(s)%4.\n\n"
                               "Ahora dibuje los muros interiores (MU), las puertas y ventanas (PUE) y los espacios "
                               "(ESP ya propone el piso %1). Las fachadas y cortes de este piso salen con FAC y COR.")
                                 .arg(piso).arg(copia.size()).arg(columnas.size())
                                 .arg(escaleras ? u(" y el hueco de la escalera") : QString()));
}
