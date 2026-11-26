/*****************************************************************************/
/*  espacios.cpp - Puertas y ventanas en el muro, áreas por espacio y los     */
/*                 cuadros de puertas y ventanas y de áreas (DuctusArq 2D)    */
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
// Cada puerta, ventana y espacio deja un texto de datos en la capa A-DATOS (no se imprime):
// así los cuadros se arman leyendo el dibujo, aunque se haya guardado y vuelto a abrir.

#include "ductusarq.h"
#include "comun.h"
#include "ventas.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

using da::u;

namespace {

struct EntidadMuro {
    Plug_Entity *e;
    bool polilinea;
    bool cerrada;
    std::vector<arq::Seg> segs;
};

// Lee las líneas y polilíneas del dibujo. soloMuros = solo capas de muros (A-MUROS, *MURO*,
// *WALL*); las demás se borran de la lista.
std::vector<EntidadMuro> leerLineas(Document_Interface *doc, bool soloMuros) {
    std::vector<EntidadMuro> r;
    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, true);
    for (Plug_Entity *e : todas) {
        QHash<int, QVariant> d;
        e->getData(&d);
        const int tipo = d.value(DPI::ETYPE).toInt();
        const QString capa = d.value(DPI::LAYER).toString().toUpper();
        const bool esMuro = capa.contains("MURO") || capa.contains("WALL");
        if ((soloMuros && !esMuro) || (tipo != DPI::LINE && tipo != DPI::POLYLINE)) { delete e; continue; }
        EntidadMuro m{e, tipo == DPI::POLYLINE, false, {}};
        if (tipo == DPI::LINE) {
            m.segs.push_back({arq::P(d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble()),
                              arq::P(d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble())});
        } else {
            QList<Plug_VertexData> v;
            e->getPolylineData(&v);
            m.cerrada = d.value(DPI::CLOSEPOLY).toBool();
            for (int i = 0; i + 1 < v.size(); ++i)
                m.segs.push_back({arq::P(v[i].point.x(), v[i].point.y()), arq::P(v[i + 1].point.x(), v[i + 1].point.y())});
            if (m.cerrada && v.size() > 2)
                m.segs.push_back({arq::P(v.last().point.x(), v.last().point.y()), arq::P(v[0].point.x(), v[0].point.y())});
        }
        r.push_back(m);
    }
    return r;
}

void soltar(std::vector<EntidadMuro> &v) {
    for (EntidadMuro &m : v) delete m.e;
    v.clear();
}

std::vector<arq::Seg> todosLosSegmentos(const std::vector<EntidadMuro> &v) {
    std::vector<arq::Seg> s;
    for (const EntidadMuro &m : v) s.insert(s.end(), m.segs.begin(), m.segs.end());
    return s;
}

using da::dato;
using da::leerDatos;

void arco(Document_Interface *doc, const arq::Arco &a) {
    QPointF c(a.c.x, a.c.y);
    doc->addArc(&c, a.r, a.a1 * 57.29577951308232, a.a2 * 57.29577951308232);
}

// Tabla dibujada: anchos de columna y altos en mm de papel; fila 0 = encabezado.
void tabla(Document_Interface *doc, const QPointF &base, const QString &titulo, const std::vector<double> &anchos,
           const std::vector<QStringList> &filas, const arq::Unidades &un) {
    const double altoFila = 7, altoTitulo = 9;
    double total = 0;
    for (double a : anchos) total += a;
    const double alto = altoTitulo + altoFila * filas.size();
    auto P = [&](double x, double y) { return arq::P(un.papel(x), un.papel(y)); };
    da::capa(doc, "A-TEXTOS");
    da::rectangulo(doc, base, arq::rect(0, 0, un.papel(total), -un.papel(alto)));
    da::linea(doc, base, P(0, -altoTitulo), P(total, -altoTitulo));
    da::texto(doc, base, P(total / 2, -altoTitulo / 2), titulo, un.papel(4));
    for (size_t f = 0; f < filas.size(); ++f) {
        const double y = -altoTitulo - altoFila * (f + 1);
        da::linea(doc, base, P(0, y), P(total, y));
        double x = 0;
        for (size_t c = 0; c < anchos.size(); ++c) {
            const QString t = c < static_cast<size_t>(filas[f].size()) ? filas[f][static_cast<int>(c)] : QString();
            const double h = std::max(1.2, std::min(f == 0 ? 2.2 : 2.5, (anchos[c] - 2) / std::max(1, t.size()) / 0.82));
            if (!t.isEmpty()) da::texto(doc, base, P(x + anchos[c] / 2, y + altoFila / 2), t, un.papel(h));
            x += anchos[c];
        }
    }
    double x = 0;
    for (size_t c = 0; c + 1 < anchos.size(); ++c) {
        x += anchos[c];
        da::linea(doc, base, P(x, -altoTitulo), P(x, -alto));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// PUERTA / VENTANA / VANO dentro de un muro: corta el muro, cierra las jambas, dibuja la hoja
// y su giro (o la ventana), pone la marca P-1 / V-1 y deja el dato para el cuadro.
void DuctusArq::puertaVentana(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Puerta / ventana en el muro"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("1) Clic sobre la cara del muro donde empieza el vano (use F3 para que se pegue a la "
                             "línea). 2) Clic hacia donde sigue el muro. 3) En puertas: clic del lado hacia donde "
                             "abre y cerca de la bisagra. El muro se corta solo.<br><i>%1</i>").arg(da::ayudaUnidades(un)));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    lay->addLayout(f);
    auto tipo = new QComboBox;
    for (int t = 0; t <= 4; ++t) tipo->addItem(u(arq::nombreTipoVano(static_cast<arq::TipoVano>(t))));
    tipo->setCurrentIndex(cfg.value("DuctusArq/vanoTipo", 0).toInt());
    f->addRow(u("Tipo:"), tipo);
    auto ancho = da::campo(f, u("Ancho del vano:"), da::clave(un, "vanoAncho"), da::sug(un, "0.90", "3'"));
    auto alto = da::campo(f, u("Alto:"), da::clave(un, "vanoAlto"), da::sug(un, "2.10", "6'-8\""));
    auto antepecho = da::campo(f, u("Antepecho (ventanas, altura desde el piso):"), da::clave(un, "vanoAntepecho"),
                               da::sug(un, "1.00", "3'"));
    auto marca = da::campo(f, u("Marca (vacío = automática):"), "vanoMarca", QString(),
                           u("Vacío: si ya hay una igual usa la misma marca; si no, la siguiente (P-1, P-2 / V-1, V-2)."));
    marca->setText(QString());
    auto aprob = da::campo(f, u("Aprobación de producto (opcional):"), "vanoAprob", QString(),
                           u("Miami-Dade: número NOA (ej. 21-0405.06) y/o Florida FL#. Colombia: fabricante o referencia."));
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Dibujar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    double vAncho = 0, vAlto = 0, vAnte = 0;
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return;
        if (!da::medida(parent, ancho, un, vAncho, 0.0001, u("Ancho"))) continue;
        if (!da::medida(parent, alto, un, vAlto, 0.0001, u("Alto"))) continue;
        if (!da::medida(parent, antepecho, un, vAnte, 0, u("Antepecho"))) continue;
        break;
    }
    da::guardar({ancho, alto, antepecho, aprob});
    cfg.setValue("DuctusArq/vanoTipo", tipo->currentIndex());
    const arq::TipoVano t = static_cast<arq::TipoVano>(tipo->currentIndex());
    const bool esVentana = t == arq::TipoVano::Ventana;
    const bool esPuerta = t == arq::TipoVano::Batiente || t == arq::TipoVano::Doble;

    QPointF p1, p2;
    if (!doc->getPoint(&p1, u("Vano: clic en la cara del muro donde empieza"))) return;
    if (!doc->getPoint(&p2, u("Vano: clic hacia donde sigue el muro"), &p1)) return;
    arq::Vano v;
    v.tipo = t;
    v.o = arq::P(p1.x(), p1.y());
    const arq::P d = arq::P(p2.x(), p2.y()) - v.o;
    if (arq::largo(d) < 1e-9) return;
    v.dir = d * (1.0 / arq::largo(d));
    v.ancho = vAncho;

    const double tol = un.metros(0.001);
    std::vector<EntidadMuro> lineas = leerLineas(doc, true);
    std::vector<arq::Seg> segs = todosLosSegmentos(lineas);
    double w = 0;
    bool hay = arq::buscarOtraCara(segs, v.o, v.dir, v.ancho, un.metros(0.80), tol, w);
    if (!hay) {  // muros en otras capas
        soltar(lineas);
        lineas = leerLineas(doc, false);
        segs = todosLosSegmentos(lineas);
        hay = arq::buscarOtraCara(segs, v.o, v.dir, v.ancho, un.metros(0.80), tol, w);
    }
    if (!hay) {
        soltar(lineas);
        QMessageBox::warning(parent, u("Puerta / ventana"),
                             u("No encontré la otra cara del muro.\n\nHaga el primer clic exactamente sobre la "
                               "línea de la cara del muro (F3 activa las referencias a objetos) y el segundo clic "
                               "a lo largo de esa misma línea."));
        return;
    }
    const arq::P perp(-v.dir.y, v.dir.x);
    v.n = w > 0 ? perp : perp * -1.0;
    v.espesor = std::fabs(w);

    if (esPuerta) {
        QPointF p3;
        if (!doc->getPoint(&p3, u("Puerta: clic del lado hacia donde abre, cerca de la bisagra"), &p1)) { soltar(lineas); return; }
        const arq::P q3(p3.x(), p3.y());
        v.lado = arq::punto(q3 - v.o, v.n) > v.espesor / 2 ? 1 : -1;
        v.bisagraInicio = arq::punto(q3 - v.o, v.dir) < v.ancho / 2;
    }

    // Cortar el muro: las líneas que pasan por el vano se cambian por sus pedazos.
    const QString capaAntes = doc->getCurrentLayer();
    for (EntidadMuro &m : lineas) {
        std::vector<arq::Seg> nuevos;
        bool cambio = false;
        for (const arq::Seg &s : m.segs) {
            const auto r = arq::cortarPorVano(s, v, tol);
            if (r.size() != 1 || arq::largo(r[0].a - s.a) > tol || arq::largo(r[0].b - s.b) > tol) cambio = true;
            nuevos.insert(nuevos.end(), r.begin(), r.end());
        }
        if (!cambio) continue;
        QHash<int, QVariant> d2;
        m.e->getData(&d2);
        doc->setLayer(d2.value(DPI::LAYER).toString());
        doc->removeEntity(m.e);
        for (const arq::Seg &s : nuevos) da::linea(doc, QPointF(0, 0), s.a, s.b);
    }
    soltar(lineas);

    // Símbolo
    const arq::SimboloVano s = arq::simboloVano(v, un.papel(7));
    da::capa(doc, esVentana ? "A-VENTANAS" : "A-PUERTAS");
    for (const arq::Seg &x : s.lineas) da::linea(doc, QPointF(0, 0), x.a, x.b);
    for (const arq::Arco &a : s.arcos) arco(doc, a);

    // Marca: la misma si ya hay una puerta o ventana igual; si no, la siguiente.
    const QString tipoTxt = u(arq::nombreTipoVano(t));
    const QString aprobTxt = aprob->text().trimmed().toUpper().replace('|', '/');
    QString m = marca->text().trimmed().toUpper().replace('|', '/');
    if (m.isEmpty()) {
        std::vector<std::string> usadas;
        for (const QString &x : leerDatos(doc, "DUCTUS-VANO|")) {
            arq::DatoVano dv;
            if (!arq::leerDatoVano(x.toStdString(), dv)) continue;
            usadas.push_back(dv.marca);
            if (m.isEmpty() && dv.tipo == tipoTxt.toStdString() && std::fabs(dv.ancho - vAncho) < tol
                && std::fabs(dv.alto - vAlto) < tol && (!esVentana || std::fabs(dv.antepecho - vAnte) < tol)
                && dv.aprobacion == aprobTxt.toStdString())
                m = QString::fromStdString(dv.marca);
        }
        if (m.isEmpty()) m = QString::fromStdString(arq::siguienteMarca(esVentana ? "V-" : "P-", usadas));
    }
    da::capa(doc, "A-SIMBOLOS");
    const double r = un.papel(3.5);
    QPointF cm(s.marca.x, s.marca.y);
    if (esVentana) da::rectangulo(doc, QPointF(0, 0), arq::rect(s.marca.x - r * 1.3, s.marca.y - r * 0.8,
                                                              s.marca.x + r * 1.3, s.marca.y + r * 0.8));
    else doc->addCircle(&cm, r);
    da::texto(doc, QPointF(0, 0), s.marca, m, un.papel(2.5));
    dato(doc, QPointF(s.marca.x, s.marca.y - r * 1.6),
         u("DUCTUS-VANO|%1|%2|%3|%4|%5|%6|%7").arg(m, tipoTxt).arg(vAncho, 0, 'f', 4).arg(vAlto, 0, 'f', 4)
             .arg(esVentana ? vAnte : 0.0, 0, 'f', 4).arg(aprobTxt, QString::fromStdString(arq::esquinasVano(v) + "|" + arq::datoLadoVano(v))), un);
    doc->setLayer(capaAntes);
    doc->updateView();
}

// ---------------------------------------------------------------------------
// CUADRO DE PUERTAS Y VENTANAS: una fila por marca con la cantidad.
void DuctusArq::cuadroVanos(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    std::vector<arq::DatoVano> vanos;
    for (const QString &x : leerDatos(doc, "DUCTUS-VANO|")) {
        arq::DatoVano d;
        if (arq::leerDatoVano(x.toStdString(), d)) vanos.push_back(d);
    }
    if (vanos.empty()) {
        QMessageBox::information(parent, u("Cuadro de puertas y ventanas"),
                                 u("El dibujo no tiene puertas ni ventanas hechas con «Puerta / ventana»."));
        return;
    }
    QPointF base;
    if (!doc->getPoint(&base, u("Cuadro de puertas y ventanas: clic en la esquina de arriba a la izquierda"))) return;
    const bool imperial = un.sistema == arq::Sistema::Imperial;
    std::vector<QStringList> filas;
    filas.push_back({u("MARCA / MARK"), u("TIPO / TYPE"), u("ANCHO / WIDTH"), u("ALTO / HEIGHT"),
                     u("ANTEPECHO / SILL"), u("CANT. / QTY"),
                     imperial ? u("APROBACIÓN NOA / FL#") : u("OBSERVACIONES")});
    for (const arq::FilaVano &f : arq::agruparVanos(vanos)) {
        const bool ventana = f.dato.tipo == "VENTANA";
        filas.push_back({QString::fromStdString(f.dato.marca), QString::fromStdString(f.dato.tipo),
                         da::textoLong(f.dato.ancho, un), da::textoLong(f.dato.alto, un),
                         ventana ? da::textoLong(f.dato.antepecho, un) : u("-"), QString::number(f.cantidad),
                         QString::fromStdString(f.dato.aprobacion)});
    }
    tabla(doc, base, u("CUADRO DE PUERTAS Y VENTANAS / DOOR AND WINDOW SCHEDULE"), {18, 52, 22, 22, 24, 16, 46}, filas, un);
    doc->updateView();
}

// ---------------------------------------------------------------------------
// ESPACIO: clic dentro (muros rectos) o esquinas a mano; escribe nombre y área y deja el dato.
void DuctusArq::espacio(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Área de un espacio"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Haga clic DENTRO del espacio: el área se mide hasta la cara de los muros "
                             "(las puertas hechas con «Puerta / ventana» cierran el espacio). Para formas con "
                             "muros inclinados, marque las esquinas a mano."));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    lay->addLayout(f);
    auto nombre = new QLineEdit(cfg.value("DuctusArq/espNombre", u("ALCOBA")).toString());
    f->addRow(u("Nombre:"), nombre);
    auto piso = new QSpinBox;
    piso->setRange(0, 200);
    piso->setValue(cfg.value("DuctusArq/espPiso", 1).toInt());
    f->addRow(u("Piso:"), piso);
    auto clase = new QComboBox;
    clase->addItem(u("Espacio (alcoba, sala, baño ...)"));
    clase->addItem(u("Área construida del piso (contorno exterior)"));
    clase->addItem(u("Lote / terreno"));
    clase->setCurrentIndex(cfg.value("DuctusArq/espClase", 0).toInt());
    f->addRow(u("Qué es:"), clase);
    auto modo = new QComboBox;
    modo->addItem(u("Clic dentro (muros rectos)"));
    modo->addItem(u("Marcar las esquinas a mano"));
    modo->setCurrentIndex(cfg.value("DuctusArq/espModo", 0).toInt());
    f->addRow(u("Cómo:"), modo);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;
    cfg.setValue("DuctusArq/espNombre", nombre->text());
    cfg.setValue("DuctusArq/espPiso", piso->value());
    cfg.setValue("DuctusArq/espClase", clase->currentIndex());
    cfg.setValue("DuctusArq/espModo", modo->currentIndex());
    static const char *clases[] = {"espacio", "construida", "lote"};
    const QString claseTxt = u(clases[clase->currentIndex()]);
    const double tol = un.metros(0.001);

    while (true) {
        double area = 0;
        QPointF etiqueta;
        if (modo->currentIndex() == 0) {
            if (!doc->getPoint(&etiqueta, u("Espacio «%1»: clic dentro (Enter para terminar)").arg(nombre->text()))) break;
            std::vector<EntidadMuro> lineas = leerLineas(doc, false);
            std::vector<arq::Seg> segs = todosLosSegmentos(lineas);
            soltar(lineas);
            // Las puertas y ventanas cierran el espacio por las dos caras del muro.
            for (const QString &x : leerDatos(doc, "DUCTUS-VANO|")) arq::cierreVano(x.toStdString(), segs);
            const arq::AreaEspacio a = arq::areaPorPunto(segs, arq::P(etiqueta.x(), etiqueta.y()), un.metros(200), tol);
            if (!a.cerrado) {
                QMessageBox::warning(parent, u("Área"),
                                     u("Ese espacio no está cerrado (o tiene muros inclinados).\n\nRevise que los "
                                       "muros se toquen, o use «Marcar las esquinas a mano»."));
                continue;
            }
            area = a.area;
        } else {
            std::vector<arq::P> pts;
            QPointF p, ant;
            if (!doc->getPoint(&p, u("Espacio «%1»: primera esquina (Enter para terminar)").arg(nombre->text()))) break;
            pts.emplace_back(p.x(), p.y());
            ant = p;
            while (doc->getPoint(&p, u("Siguiente esquina (Enter cierra el contorno)"), &ant)) {
                pts.emplace_back(p.x(), p.y());
                ant = p;
            }
            if (pts.size() < 3) continue;
            area = arq::areaPoligono(pts);
            da::capa(doc, clase->currentIndex() == 2 ? "A-SIMBOLOS" : "A-TEXTOS");
            da::polilinea(doc, QPointF(0, 0), pts, true);
            double cx = 0, cy = 0;
            for (const arq::P &q : pts) { cx += q.x; cy += q.y; }
            etiqueta = QPointF(cx / pts.size(), cy / pts.size());
        }
        // Nombre y área
        const QString n = nombre->text().trimmed().toUpper().replace('|', '/');
        const QString areaTxt = un.sistema == arq::Sistema::Metrico ? u("%1 m²").arg(area, 0, 'f', 2)
                                                                     : u("%1 ft²").arg(area / 144.0, 0, 'f', 1);
        da::capa(doc, "A-TEXTOS");
        da::texto(doc, QPointF(0, 0), arq::P(etiqueta.x(), etiqueta.y() + un.papel(2)), n, un.papel(3),
                  DPI::HAlignCenter, DPI::VAlignBottom);
        da::texto(doc, QPointF(0, 0), arq::P(etiqueta.x(), etiqueta.y() - un.papel(1)), areaTxt, un.papel(2.5),
                  DPI::HAlignCenter, DPI::VAlignTop);
        dato(doc, QPointF(etiqueta.x(), etiqueta.y() - un.papel(6)),
             u("DUCTUS-ESPACIO|%1|%2|%3|%4").arg(n).arg(clase->currentIndex() == 2 ? 0 : piso->value())
                 .arg(area, 0, 'f', 6).arg(claseTxt), un);
        doc->updateView();
        // Para el siguiente espacio se pide el nombre otra vez.
        bool okNombre = false;
        const QString otro = QInputDialog::getText(parent, u("Siguiente espacio"),
                                                   u("Nombre del siguiente espacio (Cancelar para terminar):"),
                                                   QLineEdit::Normal, nombre->text(), &okNombre);
        if (!okNombre || otro.trimmed().isEmpty()) break;
        nombre->setText(otro);
        cfg.setValue("DuctusArq/espNombre", otro);
    }
    da::capa(doc, "A-MUROS");
}

// ---------------------------------------------------------------------------
// CUADRO DE ÁREAS: espacios por piso, área construida, lote e índices.
void DuctusArq::cuadroAreas(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    std::vector<arq::DatoEspacio> es;
    for (const QString &x : leerDatos(doc, "DUCTUS-ESPACIO|")) {
        arq::DatoEspacio d;
        if (arq::leerDatoEspacio(x.toStdString(), d)) es.push_back(d);
    }
    if (es.empty()) {
        QMessageBox::information(parent, u("Cuadro de áreas"),
                                 u("El dibujo no tiene espacios medidos. Use primero «Área de un espacio»."));
        return;
    }
    QPointF base;
    if (!doc->getPoint(&base, u("Cuadro de áreas: clic en la esquina de arriba a la izquierda"))) return;
    const bool m = un.sistema == arq::Sistema::Metrico;
    auto area = [&](double a) { return m ? QString::number(a, 'f', 2) : QString::number(a / 144.0, 'f', 1); };
    const QString unidad = m ? u("m²") : u("ft²");
    const arq::ResumenAreas r = arq::resumirAreas(es);
    std::vector<QStringList> filas;
    filas.push_back({u("PISO / LEVEL"), u("ESPACIO / ROOM"), u("ÁREA (%1)").arg(unidad)});
    for (size_t k = 0; k < r.pisos.size(); ++k) {
        for (const arq::DatoEspacio &e : es)
            if (e.clase == "espacio" && e.piso == r.pisos[k])
                filas.push_back({QString::number(e.piso), QString::fromStdString(e.nombre), area(e.area)});
        filas.push_back({QString::number(r.pisos[k]), u("ÁREA ÚTIL PISO %1").arg(r.pisos[k]), area(r.utilPorPiso[k])});
        filas.push_back({QString::number(r.pisos[k]), u("ÁREA CONSTRUIDA PISO %1").arg(r.pisos[k]), area(r.construidaPorPiso[k])});
    }
    filas.push_back({QString(), u("ÁREA CONSTRUIDA TOTAL"), area(r.construidaTotal)});
    if (r.lote > 0) {
        filas.push_back({QString(), u("ÁREA DEL LOTE / LOT AREA"), area(r.lote)});
        filas.push_back({QString(), u("ÍNDICE DE OCUPACIÓN / LOT COVERAGE"), QString::number(r.indiceOcupacion * 100, 'f', 1) + " %"});
        filas.push_back({QString(), u("ÍNDICE DE CONSTRUCCIÓN / FAR"), QString::number(r.indiceConstruccion, 'f', 2)});
    }
    tabla(doc, base, u("CUADRO DE ÁREAS / AREA TABLE"), {24, 80, 30}, filas, un);
    // Avisos de la norma (no reemplazan la revisión del profesional).
    QStringList avisos;
    const double m2 = m ? r.construidaTotal : r.construidaTotal * 0.00064516;
    if (m2 >= 2000)
        avisos << u("Colombia: más de 2.000 m² construidos exigen revisión independiente de diseños estructurales "
                    "y supervisión técnica (Ley 1796 de 2016).");
    if (!m && r.lote > 0 && r.indiceOcupacion > 0.5)
        avisos << u("Miami 21, zona T3: la ocupación del primer piso no puede pasar del 50 % del lote.");
    if (!avisos.isEmpty())
        QMessageBox::information(parent, u("Cuadro de áreas: revisar"), avisos.join("\n\n"));
    doc->updateView();
}

// ---------------------------------------------------------------------------
// Espacios con su forma (para el plano de ventas y el render): la forma se recalcula con los
// muros actuales desde el punto donde se hizo clic, así sigue bien aunque se muevan muros.
// Solo se usan las líneas de muros (y los muros guardados): los muebles no cierran espacios.
namespace da {
std::vector<arq::EspacioVentas> espaciosConForma(Document_Interface *doc, const arq::Unidades &un)
{
    std::vector<arq::EspacioVentas> r;
    struct Etiqueta { QString texto; QPointF punto; };
    std::vector<Etiqueta> datos;
    {
        QList<Plug_Entity *> todas;
        doc->getAllEntities(&todas, false);
        for (Plug_Entity *e : todas) {
            QHash<int, QVariant> d;
            e->getData(&d);
            const int tipo = d.value(DPI::ETYPE).toInt();
            if (tipo == DPI::TEXT || tipo == DPI::MTEXT) {
                const QString t = d.value(DPI::TEXTCONTENT).toString();
                if (t.startsWith(u("DUCTUS-ESPACIO|")))
                    datos.push_back({t, QPointF(d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble())});
            }
            delete e;
        }
    }
    if (datos.empty()) return r;
    std::vector<EntidadMuro> lineas = leerLineas(doc, true);
    std::vector<arq::Seg> segs = todosLosSegmentos(lineas);
    soltar(lineas);
    for (const QString &x : leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (!arq::leerDatoMuro(x.toStdString(), pol)) continue;
        for (size_t i = 0; i < pol.size(); ++i) segs.push_back({pol[i], pol[(i + 1) % pol.size()]});
    }
    for (const QString &x : leerDatos(doc, "DUCTUS-VANO|")) arq::cierreVano(x.toStdString(), segs);
    const double tol = un.metros(0.001);
    for (const Etiqueta &et : datos) {
        arq::DatoEspacio de;
        if (!arq::leerDatoEspacio(et.texto.toStdString(), de) || de.clase != "espacio") continue;
        // El dato se escribe 6 mm (de papel) debajo del punto del clic.
        const arq::P clic(et.punto.x(), et.punto.y() + un.papel(6));
        arq::EspacioVentas ev;
        ev.nombre = de.nombre;
        ev.etiqueta = clic;
        ev.area = de.area;
        const arq::AreaEspacio a = arq::areaPorPunto(segs, clic, un.metros(200), tol);
        if (a.cerrado) ev.celdas = a.celdas;
        r.push_back(ev);
    }
    return r;
}
} // namespace da
