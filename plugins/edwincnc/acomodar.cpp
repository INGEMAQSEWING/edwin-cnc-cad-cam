/*****************************************************************************/
/*  acomodar.cpp - EDWIN CNC: acomodar piezas en la lámina (nesting)         */
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
// El usuario selecciona las piezas; cada contorno cerrado de afuera es una pieza y se lleva consigo sus
// huecos y todo lo que tenga adentro (marcado, textos). nesting.h decide dónde va cada una; aquí se mueven
// y giran las entidades del dibujo (se deshace con Ctrl+Z) y se dibujan las láminas en REFERENCIA.

#include "edwincnc.h"
#include "cam.h"
#include "nesting.h"

#include <QApplication>
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
#include <QVBoxLayout>
#include <cmath>
#include <map>

namespace {

QString u(const char *s) { return QString::fromUtf8(s); }

bool capaQueNoSeMueve(const QString &capa) {
    const QString c = capa.trimmed().toUpper();
    return c == "REFERENCIA" || c == "REVISION" || c == "RECORRIDO" || c == "RECORRIDO RAPIDO";
}
bool capaSinCorte(const QString &capa) {
    const QString c = capa.trimmed().toUpper();
    return c == "MARCADO" || capaQueNoSeMueve(c);
}

// Raíz de unión simple.
struct Union {
    std::vector<int> p;
    explicit Union(int n) : p(n) { for (int i = 0; i < n; ++i) p[i] = i; }
    int r(int x) { while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; } return x; }
    void unir(int a, int b) { a = r(a); b = r(b); if (a != b) p[b] = a; }
};

// Tramos de una entidad del dibujo (para saber qué contornos forma).
bool tramos(Plug_Entity *ent, cam::Entrada &e) {
    QHash<int, QVariant> d;
    ent->getData(&d);
    const cam::P p1{d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble()};
    switch (d.value(DPI::ETYPE).toInt()) {
    case DPI::LINE:
        cam::agregarLinea(e, p1, {d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble()});
        return true;
    case DPI::ARC:
        cam::agregarArco(e, p1, d.value(DPI::RADIUS).toDouble(), d.value(DPI::STARTANGLE).toDouble(), d.value(DPI::ENDANGLE).toDouble());
        return true;
    case DPI::CIRCLE:
        cam::agregarCirculo(e, p1, d.value(DPI::RADIUS).toDouble());
        return true;
    case DPI::ELLIPSE:
        cam::agregarElipse(e, p1, {d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble()}, d.value(DPI::HEIGHT).toDouble(),
                           d.value(DPI::STARTANGLE).toDouble(), d.value(DPI::ENDANGLE).toDouble());
        return true;
    case DPI::POLYLINE: {
        QList<Plug_VertexData> v;
        ent->getPolylineData(&v);
        std::vector<cam::P> pts;
        std::vector<double> bul;
        for (const Plug_VertexData &x : v) { pts.push_back({x.point.x(), x.point.y()}); bul.push_back(x.bulge); }
        if (pts.empty()) return false;
        cam::agregarPolilinea(e, pts, bul, d.value(DPI::CLOSEPOLY).toBool());
        return true;
    }
    default:
        return false;
    }
}

QPointF puntoDeReferencia(Plug_Entity *ent) {
    QHash<int, QVariant> d;
    ent->getData(&d);
    return {d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble()};
}

nesting::Poligono aPoligono(const cam::Contorno &c) {
    nesting::Poligono p;
    for (const cam::P &q : cam::poligono(c, 24)) p.push_back({q.x, q.y});
    return p;
}

} // namespace

void EdwinCnc::acomodarPiezas(Document_Interface *doc, QWidget *parent)
{
    const QString titulo = u("Acomodar piezas en la lámina (nesting)");
    QList<Plug_Entity *> sel;
    if (!doc->getSelect(&sel, u("Acomodar: seleccione las piezas (con sus huecos y marcas) y presione Enter")) || sel.isEmpty()) {
        qDeleteAll(sel);
        return;
    }

    // ── 1) contornos cerrados que forman las entidades seleccionadas ──
    struct Ent { Plug_Entity *e; QString capa; cam::Entrada tr; bool geom; bool cerrada; };
    std::vector<Ent> ents;
    for (Plug_Entity *e : sel) {
        QHash<int, QVariant> d;
        e->getData(&d);
        Ent x{e, d.value(DPI::LAYER).toString(), {}, false, false};
        // De REFERENCIA solo se llevan los textos (el nombre de la pieza); las láminas viejas se quedan.
        const int tipoEnt = d.value(DPI::ETYPE).toInt();
        const bool texto = tipoEnt == DPI::TEXT || tipoEnt == DPI::MTEXT;
        if (capaQueNoSeMueve(x.capa) && !(texto && x.capa.trimmed().toUpper() == "REFERENCIA")) continue;
        if (!capaSinCorte(x.capa)) {
            x.geom = tramos(e, x.tr);
            x.cerrada = !x.tr.cerrados.empty() && x.tr.sueltos.empty();
        }
        ents.push_back(x);
    }
    const int n = int(ents.size());
    // entidades abiertas que se tocan por los extremos = un mismo contorno
    Union un(n);
    {
        struct Ext { cam::P p; int i; };
        std::vector<Ext> ext;
        for (int i = 0; i < n; ++i)
            if (ents[i].geom && !ents[i].cerrada)
                for (const cam::Seg &s : ents[i].tr.sueltos) { ext.push_back({s.a, i}); ext.push_back({s.b, i}); }
        std::sort(ext.begin(), ext.end(), [](const Ext &a, const Ext &b) { return a.p.x < b.p.x; });
        for (size_t a = 0; a < ext.size(); ++a)
            for (size_t b = a + 1; b < ext.size() && ext[b].p.x - ext[a].p.x <= 0.05; ++b)
                if (cam::dist(ext[a].p, ext[b].p) <= 0.05) un.unir(ext[a].i, ext[b].i);
    }
    struct Cont { cam::Contorno c; nesting::Poligono pol; double area; std::vector<int> ents; };
    std::vector<Cont> conts;
    std::map<int, std::vector<int>> grupos;
    for (int i = 0; i < n; ++i)
        if (ents[i].geom && !ents[i].cerrada) grupos[un.r(i)].push_back(i);
    int abiertos = 0;
    for (auto &g : grupos) {
        std::vector<cam::Seg> segs;
        for (int i : g.second) for (const cam::Seg &s : ents[i].tr.sueltos) segs.push_back(s);
        int ab = 0;
        auto cs = cam::encadenar(segs, 0.05, ab);
        if (cs.empty()) { abiertos += 1; continue; }
        // el más grande representa al grupo (con sus entidades)
        size_t mejor = 0;
        for (size_t k = 1; k < cs.size(); ++k) if (std::fabs(cam::area(cs[k])) > std::fabs(cam::area(cs[mejor]))) mejor = k;
        Cont c{cs[mejor], aPoligono(cs[mejor]), std::fabs(cam::area(cs[mejor])), g.second};
        conts.push_back(c);
        if (ab > 0) ++abiertos;
    }
    for (int i = 0; i < n; ++i)
        if (ents[i].geom && ents[i].cerrada)
            for (const cam::Contorno &c : ents[i].tr.cerrados)
                conts.push_back({c, aPoligono(c), std::fabs(cam::area(c)), {i}});
    if (conts.empty()) {
        QMessageBox::warning(parent, titulo, u("No encontré piezas cerradas en lo que seleccionó.\n\n"
                                               "Use «Revisar para corte» para encontrar contornos abiertos."));
        qDeleteAll(sel);
        return;
    }

    // ── 2) piezas: contornos de afuera (profundidad par) con sus huecos y lo que tienen adentro ──
    const int nc = int(conts.size());
    std::vector<int> prof(nc, 0), padre(nc, -1);
    for (int i = 0; i < nc; ++i)
        for (int j = 0; j < nc; ++j) {
            if (i == j || conts[j].area <= conts[i].area) continue;
            std::vector<cam::P> pj;
            for (auto q : conts[j].pol) pj.push_back({q.x, q.y});
            if (cam::dentro({conts[i].pol[0].x, conts[i].pol[0].y}, pj)) {
                ++prof[i];
                if (padre[i] < 0 || conts[j].area < conts[padre[i]].area) padre[i] = j;
            }
        }
    std::vector<int> piezaDe(nc, -1);
    std::vector<nesting::Pieza> piezas;
    std::vector<std::vector<int>> entsDePieza;
    for (int i = 0; i < nc; ++i)
        if (prof[i] % 2 == 0) {
            piezaDe[i] = int(piezas.size());
            nesting::Pieza p; p.afuera = conts[i].pol;
            piezas.push_back(p);
            entsDePieza.push_back(conts[i].ents);
        }
    for (int i = 0; i < nc; ++i)
        if (prof[i] % 2 == 1 && padre[i] >= 0 && piezaDe[padre[i]] >= 0) {
            const int k = piezaDe[padre[i]];
            piezas[k].huecos.push_back(conts[i].pol);
            for (int e : conts[i].ents) entsDePieza[k].push_back(e);
        }
    // lo demás (marcas, textos, líneas sueltas) va con la pieza más chica que lo contiene
    std::vector<bool> asignada(n, false);
    for (auto &v : entsDePieza) for (int e : v) asignada[e] = true;
    int sueltas = 0;
    for (int i = 0; i < n; ++i) {
        if (asignada[i]) continue;
        const QPointF r = puntoDeReferencia(ents[i].e);
        int mejor = -1;
        for (int c = 0; c < nc; ++c) {
            if (piezaDe[c] < 0) continue;
            std::vector<cam::P> pc;
            for (auto q : conts[c].pol) pc.push_back({q.x, q.y});
            if (cam::dentro({r.x(), r.y()}, pc) && (mejor < 0 || conts[c].area < conts[mejor].area)) mejor = c;
        }
        if (mejor >= 0) entsDePieza[piezaDe[mejor]].push_back(i);
        else ++sueltas;
    }

    // ── 3) ventana de opciones ──
    QSettings cfg;
    cfg.beginGroup("EdwinCnc/Acomodar");
    QDialog dlg(parent);
    dlg.setWindowTitle(titulo);
    auto lay = new QVBoxLayout(&dlg);
    auto info = new QLabel(u("Encontré <b>%1 pieza(s)</b>. Se acomodan de la más grande a la más chica, lo más a la "
                             "izquierda posible, para que lo que sobre quede junto como retal.").arg(piezas.size()));
    info->setWordWrap(true);
    lay->addWidget(info);
    auto form = new QFormLayout;
    auto num = [](double a, double b, double v, int dec, const char *suf) {
        auto s = new QDoubleSpinBox; s->setRange(a, b); s->setDecimals(dec); s->setValue(v); s->setSuffix(QString::fromUtf8(suf)); return s;
    };
    auto ancho = num(50, 100000, cfg.value("ancho", 2440).toDouble(), 1, " mm");
    auto alto = num(50, 100000, cfg.value("alto", 1220).toDouble(), 1, " mm");
    auto margen = num(0, 500, cfg.value("margen", 10).toDouble(), 1, " mm");
    auto separacion = num(0, 200, cfg.value("separacion", 6).toDouble(), 1, " mm");
    auto giros = new QComboBox;
    giros->addItems({u("No girar (respetar la veta)"), u("Solo voltear (0° y 180°)"), u("Cada 90°"), u("Cada 45°"), u("Cada 15° (más lento)")});
    giros->setCurrentIndex(cfg.value("giros", 2).toInt());
    auto copias = new QSpinBox; copias->setRange(1, 500); copias->setValue(1);
    auto precision = new QComboBox;
    precision->addItems({u("Normal (rápido)"), u("Fina (aprovecha más, más lento)")});
    precision->setCurrentIndex(cfg.value("precision", 0).toInt());
    form->addRow(u("Lámina, largo:"), ancho);
    form->addRow(u("Lámina, ancho:"), alto);
    form->addRow(u("Margen al borde:"), margen);
    form->addRow(u("Separación entre piezas:"), separacion);
    form->addRow(u("Girar piezas:"), giros);
    form->addRow(u("Copias de cada pieza:"), copias);
    form->addRow(u("Precisión:"), precision);
    separacion->setToolTip(u("Ancho del corte del plasma más lo que quiera dejar entre piezas (por ejemplo 2 + 4 mm)."));
    lay->addLayout(form);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Acomodar"));
    botones->button(QDialogButtonBox::Cancel)->setText(u("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) { qDeleteAll(sel); return; }
    cfg.setValue("ancho", ancho->value()); cfg.setValue("alto", alto->value());
    cfg.setValue("margen", margen->value()); cfg.setValue("separacion", separacion->value());
    cfg.setValue("giros", giros->currentIndex()); cfg.setValue("precision", precision->currentIndex());

    nesting::Opciones op;
    op.ancho = ancho->value(); op.alto = alto->value();
    op.margen = margen->value(); op.separacion = separacion->value();
    const int girosN[] = {1, 2, 4, 8, 24};
    op.giros = girosN[giros->currentIndex()];
    const double grande = std::max(op.ancho, op.alto) - 2 * op.margen;
    op.celda = precision->currentIndex() == 0 ? std::max(1.0, grande / 600.0) : std::max(0.5, grande / 1400.0);
    for (auto &p : piezas) p.cantidad = copias->value();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const nesting::Resultado res = nesting::acomodar(piezas, op);
    QApplication::restoreOverrideCursor();

    // ── 4) mover las piezas: las láminas van a la derecha de todo el dibujo ──
    double maxX = -1e300, minY = 1e300;
    {
        QList<Plug_Entity *> todas;
        doc->getAllEntities(&todas, true);
        for (Plug_Entity *e : todas) {
            QHash<int, QVariant> d; e->getData(&d);
            const double x1 = d.value(DPI::STARTX).toDouble(), x2 = d.value(DPI::ENDX, x1).toDouble();
            const double rr = d.value(DPI::RADIUS, 0).toDouble();
            maxX = std::max({maxX, x1 + rr, x2});
            minY = std::min({minY, d.value(DPI::STARTY).toDouble() - rr, d.value(DPI::ENDY, d.value(DPI::STARTY)).toDouble()});
            if (d.value(DPI::ETYPE).toInt() == DPI::POLYLINE) {
                QList<Plug_VertexData> v;
                e->getPolylineData(&v);
                for (const Plug_VertexData &q : v) { maxX = std::max(maxX, q.point.x()); minY = std::min(minY, q.point.y()); }
            }
        }
        qDeleteAll(todas);
    }
    if (maxX < -1e299) { maxX = 0; minY = 0; }
    const double ox = std::ceil((maxX + 300) / 10) * 10, oy = std::floor(minY / 10) * 10;
    const double paso = op.ancho + 200;

    // colocaciones por pieza, en orden de copia: la primera mueve el original y las demás dejan copia
    std::vector<std::vector<nesting::Colocacion>> porPieza(piezas.size());
    for (const auto &c : res.colocadas) porPieza[c.pieza].push_back(c);
    for (size_t k = 0; k < piezas.size(); ++k) {
        auto &v = porPieza[k];
        std::sort(v.begin(), v.end(), [](const nesting::Colocacion &a, const nesting::Colocacion &b) { return a.copia < b.copia; });
        for (int ie : entsDePieza[k]) {
            Plug_Entity *e = ents[ie].e;
            double angAct = 0, txAct = 0, tyAct = 0;
            bool primera = true;
            for (const auto &c : v) {
                const double tx = ox + c.lamina * paso + c.dx, ty = oy + c.dy;
                // del estado actual (ángulo angAct, corrimiento txAct,tyAct) al nuevo: girar la diferencia
                e->moveRotate(QPointF(tx - txAct, ty - tyAct), QPointF(tx, ty), c.angulo - angAct,
                              primera ? DPI::DELETE_ORIGINAL : DPI::KEEP_ORIGINAL);
                angAct = c.angulo; txAct = tx; tyAct = ty;
                primera = false;
            }
        }
    }

    // ── 5) láminas en REFERENCIA, con su número y cuánto se aprovecha ──
    crearCapas(doc, parent, false);
    const QString antes = doc->getCurrentLayer();
    doc->setLayer("REFERENCIA");
    double usoTotal = 0;
    for (int L = 0; L < res.laminas; ++L) {
        const double x0 = ox + L * paso, y0 = oy;
        QPointF a(x0, y0), b(x0 + op.ancho, y0), c(x0 + op.ancho, y0 + op.alto), d(x0, y0 + op.alto);
        doc->addLine(&a, &b); doc->addLine(&b, &c); doc->addLine(&c, &d); doc->addLine(&d, &a);
        const double uso = res.usoPorLamina[L] * 100;
        usoTotal += uso;
        QPointF t(x0, y0 + op.alto + op.alto * 0.02);
        doc->addText(u("Lámina %1 de %2 · %3 x %4 mm · aprovechada %5 %")
                         .arg(L + 1).arg(res.laminas).arg(op.ancho, 0, 'f', 0).arg(op.alto, 0, 'f', 0).arg(uso, 0, 'f', 1),
                     "standard", &t, std::max(10.0, op.alto * 0.03), 0, DPI::HAlignLeft, DPI::VAlignBottom);
    }
    doc->setLayer(antes);
    qDeleteAll(sel);
    doc->updateView();

    QString msg = u("<b>%1 pieza(s) acomodadas en %2 lámina(s)</b> de %3 x %4 mm.<br>Aprovechamiento medio: <b>%5 %</b>.")
                      .arg(res.colocadas.size()).arg(res.laminas).arg(op.ancho, 0, 'f', 0).arg(op.alto, 0, 'f', 0)
                      .arg(res.laminas ? usoTotal / res.laminas : 0, 0, 'f', 1);
    if (!res.noCaben.empty())
        msg += u("<br><br><b>%1 pieza(s) no caben</b> ni en una lámina vacía: se dejaron donde estaban.").arg(res.noCaben.size());
    if (abiertos)
        msg += u("<br><br>%1 grupo(s) de líneas no cierran y no se acomodaron (use «Revisar para corte»).").arg(abiertos);
    if (sueltas)
        msg += u("<br><br>%1 elemento(s) no están dentro de ninguna pieza y se dejaron en su sitio.").arg(sueltas);
    msg += u("<br><br>Para volver atrás: Ctrl+Z (una vez por cada elemento movido) o cierre sin guardar.");
    QMessageBox::information(parent, titulo, msg);
}
