/*****************************************************************************/
/*  superponer.cpp - Planos superpuestos y revisión de cruces (DuctusArq)    */
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
// SUP: trae el plano de otra especialidad (DWG o DXF) encima del arquitectónico, como un bloque
// en la capa SUP-<especialidad>. Sus capas entran con prefijo (EST-, ELE-, HID-...) y en el color
// de la especialidad: nunca se mezclan con las del arquitectónico. Se cuadra por una esquina
// común (por ejemplo la de arriba a la izquierda del pórtico). La misma ventana prende y apaga
// cada plano superpuesto.
//
// CRU: revisa las columnas del estructural contra los muros, puertas, ventanas y espacios.

#include "ductusarq.h"
#include "comun.h"
#include "cruces.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

using da::u;

namespace da {
std::vector<arq::EspacioVentas> espaciosConForma(Document_Interface *doc, const arq::Unidades &un);  // espacios.cpp
}

namespace {

struct Especialidad {
    const char *nombre;
    const char *codigo;
    int color;
};

const std::vector<Especialidad> &especialidades() {
    static const std::vector<Especialidad> e = {
        {"Estructural", "EST", 0xD01818},      {"Eléctrico", "ELE", 0x1E5AD2},
        {"Hidrosanitario", "HID", 0x00A050},   {"Gas", "GAS", 0xE08000},
        {"Aire acondicionado", "AA", 0x00A0B4}, {"Otro", "OTR", 0xA000A0},
    };
    return e;
}

QString nombreEspecialidad(const QString &capaSup) {
    for (const auto &e : especialidades())
        if (capaSup == u("SUP-") + u(e.codigo)) return u(e.nombre);
    return capaSup;
}

struct Superpuesto {
    QString capa;    // SUP-EST
    QString bloque;  // EST-estructural_casa
};

// Planos superpuestos que ya tiene el dibujo (inserciones en capas SUP-...).
std::vector<Superpuesto> superpuestos(Document_Interface *doc) {
    std::vector<Superpuesto> r;
    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, false);
    for (Plug_Entity *e : todas) {
        QHash<int, QVariant> d;
        e->getData(&d);
        const QString capa = d.value(DPI::LAYER).toString();
        if (d.value(DPI::ETYPE).toInt() == DPI::INSERT && capa.startsWith("SUP-") && capa != "SUP-CRUCES")
            r.push_back({capa, d.value(DPI::BLKNAME).toString()});
        delete e;
    }
    return r;
}

// Mueve la inserción del bloque (se busca de nuevo porque mover crea la entidad otra vez).
void moverInsercion(Document_Interface *doc, const QString &bloque, QPointF d) {
    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, false);
    bool hecho = false;
    for (Plug_Entity *e : todas) {
        QHash<int, QVariant> v;
        e->getData(&v);
        if (!hecho && v.value(DPI::ETYPE).toInt() == DPI::INSERT && v.value(DPI::BLKNAME).toString() == bloque) {
            e->move(d);
            hecho = true;
        }
        delete e;
    }
}

bool cajaInsercion(Document_Interface *doc, const QString &bloque, arq::Rect &caja) {
    QList<QPolygonF> polys;
    QStringList capas;
    QList<QPointF> centros;
    QList<double> radios;
    if (!doc->getInsertGeometry(bloque, &polys, &capas, &centros, &radios)) return false;
    std::vector<arq::P> pts;
    for (const QPolygonF &p : polys)
        for (const QPointF &q : p) pts.push_back(arq::P(q.x(), q.y()));
    for (const QPointF &c : centros) pts.push_back(arq::P(c.x(), c.y()));
    if (pts.empty()) return false;
    caja = arq::cajaPuntos(pts);
    return true;
}

// Unidades del archivo -> metros.
double aMetros(int i) {
    switch (i) {
    case 1: return 0.001;   // milímetros
    case 2: return 0.01;    // centímetros
    case 3: return 1.0;     // metros
    case 4: return 0.0254;  // pulgadas
    case 5: return 0.3048;  // pies
    default: return 0;      // igual que este dibujo
    }
}

} // namespace

// ---------------------------------------------------------------------------
void DuctusArq::superponerPlano(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Superponer plano de otra especialidad"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Trae el plano <b>estructural</b>, <b>eléctrico</b>, <b>hidrosanitario</b> u otro (DWG o DXF) "
                             "encima del arquitectónico, en su propia capa y su propio color. Sus capas entran con un "
                             "prefijo (EST-, ELE-, HID-...): nunca se mezclan con las suyas. Después escriba <b>CRU</b> "
                             "para revisar los cruces de las columnas con muros, puertas y ventanas."));
    nota->setWordWrap(true);
    nota->setMinimumWidth(520);
    lay->addWidget(nota);

    // Planos que ya están: prender y apagar.
    const std::vector<Superpuesto> ya = superpuestos(doc);
    std::vector<std::pair<QString, QCheckBox *>> casillas;
    if (!ya.empty()) {
        auto grupo = new QGroupBox(u("Planos superpuestos (marque para verlos, desmarque para apagarlos)"));
        auto g = new QVBoxLayout(grupo);
        for (const Superpuesto &s : ya) {
            bool visible = true, bloqueado = true;
            doc->getLayerState(s.capa, &visible, &bloqueado);
            auto c = new QCheckBox(nombreEspecialidad(s.capa) + u("   (") + s.bloque + u(")"));
            c->setChecked(visible);
            g->addWidget(c);
            casillas.push_back({s.capa, c});
        }
        auto ver = new QHBoxLayout;
        auto todos = new QPushButton(u("Ver todos"));
        auto ninguno = new QPushButton(u("Solo el arquitectónico"));
        QObject::connect(todos, &QPushButton::clicked, [&]() { for (auto &c : casillas) c.second->setChecked(true); });
        QObject::connect(ninguno, &QPushButton::clicked, [&]() { for (auto &c : casillas) c.second->setChecked(false); });
        ver->addWidget(todos);
        ver->addWidget(ninguno);
        ver->addStretch();
        g->addLayout(ver);
        lay->addWidget(grupo);
    }

    // Agregar un plano.
    auto grupo = new QGroupBox(u("Agregar un plano"));
    auto f = new QFormLayout(grupo);
    auto esp = new QComboBox;
    for (const auto &e : especialidades()) esp->addItem(u(e.nombre));
    esp->setCurrentIndex(cfg.value("DuctusArq/supEsp", 0).toInt());
    auto archivo = new QLineEdit;
    archivo->setPlaceholderText(u("Archivo DWG o DXF del ingeniero"));
    auto buscar = new QPushButton(u("Buscar..."));
    QObject::connect(buscar, &QPushButton::clicked, [&]() {
        const QString r = QFileDialog::getOpenFileName(&dlg, u("Plano para superponer"),
                                                       cfg.value("DuctusArq/supCarpeta").toString(),
                                                       u("Planos (*.dwg *.dxf);;Todos los archivos (*)"));
        if (!r.isEmpty()) archivo->setText(r);
    });
    auto filaArchivo = new QHBoxLayout;
    filaArchivo->addWidget(archivo, 1);
    filaArchivo->addWidget(buscar);
    auto unidades = new QComboBox;
    unidades->addItems({u("Igual que este dibujo"), u("Milímetros"), u("Centímetros"), u("Metros"), u("Pulgadas"),
                        u("Pies")});
    unidades->setCurrentIndex(cfg.value("DuctusArq/supUnidades", 0).toInt());
    auto cuadre = new QComboBox;
    cuadre->addItems({u("Por una esquina común (clic en el plano y luego en el arquitectónico)"),
                      u("Mismo punto 0,0 (los dos planos tienen el mismo origen)")});
    cuadre->setCurrentIndex(cfg.value("DuctusArq/supCuadre", 0).toInt());
    f->addRow(u("Especialidad:"), esp);
    f->addRow(u("Archivo:"), filaArchivo);
    f->addRow(u("Unidades del archivo:"), unidades);
    f->addRow(u("Cuadre:"), cuadre);
    auto ayuda = new QLabel(u("<i>Si el plano sale muy grande o muy pequeño, cambie las unidades (los ingenieros "
                              "suelen dibujar en milímetros). La esquina común puede ser la de arriba a la izquierda "
                              "del pórtico: el cruce de los ejes del borde.</i>"));
    ayuda->setWordWrap(true);
    f->addRow(ayuda);
    lay->addWidget(grupo);

    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Aplicar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;

    // 1) Prender / apagar los que ya están (quedan bloqueados para no moverlos sin querer).
    for (auto &c : casillas) doc->setLayerState(c.first, c.second->isChecked(), true);

    // 2) Agregar el nuevo.
    const QString ruta = archivo->text().trimmed();
    if (ruta.isEmpty()) { doc->updateView(); return; }
    cfg.setValue("DuctusArq/supEsp", esp->currentIndex());
    cfg.setValue("DuctusArq/supUnidades", unidades->currentIndex());
    cfg.setValue("DuctusArq/supCuadre", cuadre->currentIndex());
    cfg.setValue("DuctusArq/supCarpeta", QFileInfo(ruta).absolutePath());
    const Especialidad &e = especialidades()[esp->currentIndex()];
    const QString codigo = u(e.codigo);
    const QString bloque = doc->addBlockSuperpuesto(ruta, codigo + "-", e.color);
    if (bloque.isEmpty()) {
        QMessageBox::warning(parent, u("Superponer plano"),
                             u("No pude abrir «%1».\n\nSi es un DWG muy nuevo, pida al ingeniero que lo guarde como "
                               "DWG 2018 o como DXF, o ábralo y guárdelo como DXF.").arg(QFileInfo(ruta).fileName()));
        return;
    }
    const QString antes = doc->getCurrentLayer();
    const QString capaSup = u("SUP-") + codigo;
    const bool existia = doc->getAllLayer().contains(capaSup);
    if (existia) doc->setLayerState(capaSup, true, false);  // para poder agregar y mover
    doc->setLayer(capaSup);
    if (!existia) doc->setCurrentLayerProperties(e.color, DPI::Width00, DPI::SolidLine);
    const double m = aMetros(unidades->currentIndex());
    const double s = m <= 0 ? 1.0 : (un.sistema == arq::Sistema::Metrico ? m : m / 0.0254);
    doc->addInsert(bloque, QPointF(0, 0), QPointF(s, s), 0);

    // Primera ubicación: esquina de arriba a la izquierda del plano sobre la del arquitectónico
    // (para que quede a la vista); después se cuadra con los clics.
    std::vector<arq::P> pts;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) pts.insert(pts.end(), pol.begin(), pol.end());
    }
    arq::Rect cajaSup;
    if (!pts.empty() && cajaInsercion(doc, bloque, cajaSup)) {
        const arq::Rect a = arq::cajaPuntos(pts);
        moverInsercion(doc, bloque, QPointF(a.x1 - cajaSup.x1, a.y2 - cajaSup.y2));
    }
    doc->updateView();

    if (cuadre->currentIndex() == 0) {
        QPointF p1, p2;
        const QString nom = u(e.nombre).toLower();
        if (doc->getPoint(&p1, u("Superponer: clic en la esquina común EN EL PLANO %1 (p. ej. arriba a la izquierda "
                                  "del pórtico; la rueda del ratón aleja)").arg(nom))
            && doc->getPoint(&p2, u("Superponer: clic en la MISMA esquina EN EL ARQUITECTÓNICO"), &p1)) {
            moverInsercion(doc, bloque, p2 - p1);
        }
    }
    doc->setLayerState(capaSup, true, true);
    doc->setLayer(antes);
    doc->updateView();
    QMessageBox::information(parent, u("Superponer plano"),
                             u("Listo: el plano %1 quedó en la capa %2, en su color y bloqueado.\n\n"
                               "• Para prenderlo o apagarlo, escriba SUP otra vez (o use el ojo de la capa %2).\n"
                               "• Sus capas internas empiezan por %3-.\n"
                               "• Escriba CRU para revisar los cruces de las columnas con el arquitectónico.")
                                 .arg(u(e.nombre).toLower(), capaSup, codigo));
}

// ---------------------------------------------------------------------------
void DuctusArq::revisarCruces(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    std::vector<Superpuesto> est;
    for (const Superpuesto &s : superpuestos(doc))
        if (s.capa == "SUP-EST") est.push_back(s);
    if (est.empty()) {
        QMessageBox::information(parent, u("Revisar cruces"),
                                 u("Primero traiga el plano estructural con SUP (Superponer plano), especialidad "
                                   "«Estructural»."));
        return;
    }
    std::vector<arq::Poli> polis;
    std::vector<std::pair<arq::P, double>> circulos;
    for (const Superpuesto &s : est) {
        QList<QPolygonF> ps;
        QStringList capas;
        QList<QPointF> centros;
        QList<double> radios;
        doc->getInsertGeometry(s.bloque, &ps, &capas, &centros, &radios);
        for (int i = 0; i < ps.size(); ++i) {
            arq::Poli p;
            for (const QPointF &q : ps[i]) p.pts.push_back(arq::P(q.x(), q.y()));
            p.capa = capas.value(i).toStdString();
            polis.push_back(p);
        }
        for (int i = 0; i < centros.size(); ++i)
            circulos.push_back({arq::P(centros[i].x(), centros[i].y()), radios.value(i)});
    }
    const auto cols = arq::detectarColumnas(polis, circulos, un.metros(0.15), un.metros(1.50));

    std::vector<std::vector<arq::P>> muros;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) muros.push_back(pol);
    }
    std::vector<arq::VanoArq> vanos;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-VANO|")) {
        arq::Vano v;
        if (!arq::vanoDeDato(t.toStdString(), v)) continue;
        const std::vector<arq::P> esq{arq::local(v, 0, 0), arq::local(v, v.ancho, 0), arq::local(v, v.ancho, v.espesor),
                                      arq::local(v, 0, v.espesor)};
        vanos.push_back({t.section('|', 1, 1).toStdString(), arq::cajaPuntos(esq)});
    }
    if (muros.empty()) {
        QMessageBox::information(parent, u("Revisar cruces"),
                                 u("El arquitectónico no tiene muros de DuctusArq. La revisión usa los muros, puertas "
                                   "y ventanas dibujados con CASA, MU y PUE, y los espacios con ESP."));
        return;
    }
    const auto espacios = da::espaciosConForma(doc, un);
    const auto cruces = arq::revisarCruces(cols, muros, vanos, espacios, un.metros(0.02));

    // Marcas en rojo (se borran las de la revisión anterior).
    const QString antes = doc->getCurrentLayer();
    doc->deleteLayer("SUP-CRUCES");
    doc->setLayer("SUP-CRUCES");
    doc->setCurrentLayerProperties(0xFF0000, DPI::Width09, DPI::SolidLine);
    int n = 0;
    for (const arq::Cruce &c : cruces) {
        if (c.nivel == 0 || (c.donde.x == 0 && c.donde.y == 0)) continue;
        QPointF centro(c.donde.x, c.donde.y);
        doc->addCircle(&centro, un.papel(6));
        da::texto(doc, QPointF(0, 0), arq::P(c.donde.x + un.papel(8), c.donde.y + un.papel(6)),
                  QString::number(++n), un.papel(4), DPI::HAlignLeft);
    }
    doc->setLayer(antes);
    doc->updateView();

    QString html = u("<p>Revisé %1 columna(s) del estructural contra %2 muro(s), %3 puerta(s)/ventana(s) y %4 "
                     "espacio(s) del arquitectónico. Los problemas quedan marcados con un círculo rojo numerado en la "
                     "capa SUP-CRUCES.</p><ol>")
                       .arg(cols.size()).arg(muros.size()).arg(vanos.size()).arg(espacios.size());
    int malos = 0, k = 0;
    QString lista, bien;
    for (const arq::Cruce &c : cruces) {
        const QString t = QString::fromStdString(c.texto).toHtmlEscaped();
        if (c.nivel == 0) { bien += u("<p><span style='color:#1b7a3a'><b>BIEN:</b></span> %1</p>").arg(t); continue; }
        if (c.nivel == 2) ++malos;
        const bool marcado = !(c.donde.x == 0 && c.donde.y == 0);
        lista += u("<li%1><span style='color:%2'><b>%3:</b></span> %4</li>")
                     .arg(marcado ? u(" value='%1'").arg(++k) : QString(), c.nivel == 2 ? u("#b00020") : u("#a06000"),
                          c.nivel == 2 ? u("PROBLEMA") : u("REVISAR"), t);
    }
    html = bien + html + lista + u("</ol>");
    if (cols.empty())
        html += u("<p>Si el ingeniero dibujó las columnas con líneas sueltas, pídale que las haga con polilínea "
                  "cerrada, o revise en qué capa están (las capas del estructural empiezan por EST-).</p>");
    QMessageBox box(malos > 0 ? QMessageBox::Warning : QMessageBox::Information, u("Revisar cruces con el estructural"),
                    html, QMessageBox::Ok, parent);
    box.setTextFormat(Qt::RichText);
    box.exec();
}
