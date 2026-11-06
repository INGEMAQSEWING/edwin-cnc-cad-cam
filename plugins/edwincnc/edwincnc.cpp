/*****************************************************************************/
/*  edwincnc.cpp - Herramientas de EDWIN CNC para corte por plasma           */
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

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSettings>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

const char *MENU = "EDWIN CNC";
const char *CMD_REVISAR = "Revisar para corte";
const char *CMD_LAMINA = "Lámina de referencia...";
const char *CMD_CAPAS = "Crear capas de corte";
const char *CMD_ACERCA = "Acerca de las herramientas EDWIN CNC";

const QString CAPA_REVISION = "REVISION";

// Tolerancias en unidades del dibujo (mm).
const double TOL_UNION = 0.01;    // dos extremos a menos de esto se consideran unidos
const double HUECO_MAX = 2.0;     // extremo suelto con otro a menos de esto = hueco pequeño
const double TRAMO_MIN = 0.1;     // tramos más cortos que esto son "diminutos"

bool esCapaSinCorte(const QString &capa) {
    const QString c = capa.trimmed().toUpper();
    return c == "MARCADO" || c == "REFERENCIA" || c == CAPA_REVISION;
}

double dist(const QPointF &a, const QPointF &b) {
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

QString num(double v) {
    return QString::number(v, 'f', 2);
}

// Un elemento abierto (línea, arco, polilínea o arco de elipse abiertos) aporta dos extremos.
struct Extremo {
    QPointF p;
    int elemento;
};

// Unión de conjuntos simple (para agrupar extremos y elementos).
struct Union {
    std::vector<int> padre;
    explicit Union(int n) : padre(n) { for (int i = 0; i < n; ++i) padre[i] = i; }
    int raiz(int x) { while (padre[x] != x) { padre[x] = padre[padre[x]]; x = padre[x]; } return x; }
    void unir(int a, int b) { a = raiz(a); b = raiz(b); if (a != b) padre[b] = a; }
};

struct Marca {
    QPointF p;
};

} // namespace

QString EdwinCnc::name() const
{
    return QString("EDWIN CNC");
}

PluginCapabilities EdwinCnc::getCapabilities() const
{
    PluginCapabilities pc;
    pc.menuEntryPoints
        << PluginMenuLocation(MENU, QString::fromUtf8(CMD_REVISAR))
        << PluginMenuLocation(MENU, QString::fromUtf8(CMD_LAMINA))
        << PluginMenuLocation(MENU, QString::fromUtf8(CMD_CAPAS))
        << PluginMenuLocation(MENU, QString::fromUtf8(CMD_ACERCA));
    return pc;
}

void EdwinCnc::execComm(Document_Interface *doc, QWidget *parent, QString cmd)
{
    if (cmd == QString::fromUtf8(CMD_REVISAR))
        revisarParaCorte(doc, parent);
    else if (cmd == QString::fromUtf8(CMD_LAMINA))
        laminaDeReferencia(doc, parent);
    else if (cmd == QString::fromUtf8(CMD_CAPAS))
        crearCapas(doc, parent, true);
    else if (cmd == QString::fromUtf8(CMD_ACERCA))
        QMessageBox::information(parent, "EDWIN CNC",
            QString::fromUtf8(
                "<b>Herramientas EDWIN CNC para corte por plasma</b><br><br>"
                "<b>Revisar para corte:</b> busca contornos abiertos, huecos pequeños, "
                "líneas repetidas y tramos diminutos en las capas que se cortan, y los "
                "marca con círculos en la capa REVISION.<br><br>"
                "<b>Lámina de referencia:</b> dibuja el tamaño de la lámina en la capa "
                "REFERENCIA para ver si las piezas caben.<br><br>"
                "<b>Crear capas de corte:</b> agrega CORTE (rojo), MARCADO (verde) y "
                "REFERENCIA (gris) a un dibujo que no las tenga.<br><br>"
                "Las capas MARCADO, REFERENCIA y REVISION no se revisan como corte."));
}

void EdwinCnc::crearCapas(Document_Interface *doc, QWidget *parent, bool avisar)
{
    const QString anterior = doc->getCurrentLayer();
    const QStringList existentes = doc->getAllLayer();
    struct Capa { const char *nombre; int color; DPI::LineType tipo; };
    const Capa capas[] = {
        {"CORTE", 0xFF0000, DPI::SolidLine},
        {"MARCADO", 0x00C800, DPI::SolidLine},
        {"REFERENCIA", 0x808080, DPI::DashLine},
    };
    int nuevas = 0;
    for (const Capa &c : capas) {
        if (existentes.contains(c.nombre, Qt::CaseInsensitive)) continue;
        doc->setLayer(c.nombre);
        doc->setCurrentLayerProperties(c.color, DPI::Width00, c.tipo);
        ++nuevas;
    }
    // Si se acaban de crear, se deja CORTE como capa activa; si no, la que estaba.
    doc->setLayer(nuevas > 0 ? QString("CORTE") : anterior);
    doc->updateView();
    if (avisar)
        QMessageBox::information(parent, "EDWIN CNC",
            nuevas > 0 ? QString::fromUtf8("Listo: se crearon %1 capa(s). La capa activa es CORTE.").arg(nuevas)
                       : QString::fromUtf8("Este dibujo ya tiene las capas CORTE, MARCADO y REFERENCIA."));
}

void EdwinCnc::borrarRevision(Document_Interface *doc)
{
    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, true);
    for (Plug_Entity *e : todas) {
        QHash<int, QVariant> d;
        e->getData(&d);
        if (d.value(DPI::LAYER).toString() == CAPA_REVISION)
            doc->removeEntity(e);
        delete e;
    }
}

void EdwinCnc::revisarParaCorte(Document_Interface *doc, QWidget *parent)
{
    const QString capaAnterior = doc->getCurrentLayer();
    borrarRevision(doc);

    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, true);

    std::vector<Extremo> extremos;
    int numElementosAbiertos = 0;
    int cerradosSolos = 0;          // círculos, polilíneas cerradas, elipses completas
    int splines = 0, textos = 0, bloques = 0;
    int duplicados = 0, diminutos = 0;
    std::vector<Marca> marcasDuplicado, marcasDiminuto, marcasTexto;
    QSet<QString> firmas;
    double minX = 1e300, minY = 1e300, maxX = -1e300, maxY = -1e300;
    auto ampliar = [&](const QPointF &p) {
        minX = std::min(minX, p.x()); minY = std::min(minY, p.y());
        maxX = std::max(maxX, p.x()); maxY = std::max(maxY, p.y());
    };
    auto r2 = [](double v) { return QString::number(std::round(v * 100.0) / 100.0, 'f', 2); };
    auto firmaPuntos = [&](QPointF a, QPointF b) {
        if (a.x() > b.x() || (a.x() == b.x() && a.y() > b.y())) std::swap(a, b);
        return r2(a.x()) + "," + r2(a.y()) + "," + r2(b.x()) + "," + r2(b.y());
    };
    auto agregarAbierto = [&](const QPointF &a, const QPointF &b) {
        extremos.push_back({a, numElementosAbiertos});
        extremos.push_back({b, numElementosAbiertos});
        ampliar(a); ampliar(b);
        ++numElementosAbiertos;
    };
    auto repetido = [&](const QString &firma, const QPointF &donde) {
        if (firmas.contains(firma)) { ++duplicados; marcasDuplicado.push_back({donde}); return true; }
        firmas.insert(firma);
        return false;
    };

    for (Plug_Entity *e : todas) {
        QHash<int, QVariant> d;
        e->getData(&d);
        const QString capa = d.value(DPI::LAYER).toString();
        if (esCapaSinCorte(capa)) { delete e; continue; }
        const int tipo = d.value(DPI::ETYPE).toInt();
        const QPointF p1(d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble());
        switch (tipo) {
        case DPI::LINE: {
            const QPointF p2(d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble());
            const QPointF medio = (p1 + p2) / 2.0;
            if (repetido("L" + firmaPuntos(p1, p2), medio)) break;
            if (dist(p1, p2) < TRAMO_MIN) { ++diminutos; marcasDiminuto.push_back({medio}); }
            agregarAbierto(p1, p2);
            break;
        }
        case DPI::ARC: {
            const double r = d.value(DPI::RADIUS).toDouble();
            const double a1 = d.value(DPI::STARTANGLE).toDouble();
            const double a2 = d.value(DPI::ENDANGLE).toDouble();
            const QPointF pa(p1.x() + r * std::cos(a1), p1.y() + r * std::sin(a1));
            const QPointF pb(p1.x() + r * std::cos(a2), p1.y() + r * std::sin(a2));
            double barrido = a2 - a1;
            while (barrido < 0) barrido += 2 * M_PI;
            const double am = a1 + barrido / 2;
            const QPointF medio(p1.x() + r * std::cos(am), p1.y() + r * std::sin(am));
            if (repetido("A" + r2(p1.x()) + "," + r2(p1.y()) + "," + r2(r) + "," + firmaPuntos(pa, pb), medio)) break;
            if (r * barrido < TRAMO_MIN) { ++diminutos; marcasDiminuto.push_back({medio}); }
            agregarAbierto(pa, pb);
            break;
        }
        case DPI::CIRCLE: {
            const double r = d.value(DPI::RADIUS).toDouble();
            ampliar(QPointF(p1.x() - r, p1.y() - r));
            ampliar(QPointF(p1.x() + r, p1.y() + r));
            if (repetido("C" + r2(p1.x()) + "," + r2(p1.y()) + "," + r2(r), QPointF(p1.x() + r, p1.y()))) break;
            ++cerradosSolos;
            break;
        }
        case DPI::ELLIPSE: {
            const QPointF mayor(d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble());
            const double ratio = d.value(DPI::HEIGHT).toDouble();
            const double a1 = d.value(DPI::STARTANGLE).toDouble();
            const double a2 = d.value(DPI::ENDANGLE).toDouble();
            const QPointF menor(-mayor.y() * ratio, mayor.x() * ratio);
            auto punto = [&](double t) {
                return QPointF(p1.x() + mayor.x() * std::cos(t) + menor.x() * std::sin(t),
                               p1.y() + mayor.y() * std::cos(t) + menor.y() * std::sin(t));
            };
            double barrido = std::fabs(a2 - a1);
            if (barrido < 1e-9 || std::fabs(barrido - 2 * M_PI) < 1e-6) {
                ampliar(punto(0)); ampliar(punto(M_PI)); ampliar(punto(M_PI / 2)); ampliar(punto(-M_PI / 2));
                ++cerradosSolos;
            } else {
                agregarAbierto(punto(a1), punto(a2));
            }
            break;
        }
        case DPI::POLYLINE: {
            QList<Plug_VertexData> v;
            e->getPolylineData(&v);
            if (v.isEmpty()) break;
            for (const Plug_VertexData &x : v) ampliar(x.point);
            // Tramos diminutos dentro de la polilínea.
            for (int i = 1; i < v.size(); ++i) {
                if (dist(v[i - 1].point, v[i].point) < TRAMO_MIN) {
                    ++diminutos;
                    marcasDiminuto.push_back({v[i].point});
                }
            }
            if (d.value(DPI::CLOSEPOLY).toBool())
                ++cerradosSolos;
            else if (dist(v.first().point, v.last().point) <= TOL_UNION && v.size() > 2)
                ++cerradosSolos;  // abierta pero termina donde empieza: corta igual
            else
                agregarAbierto(v.first().point, v.last().point);
            break;
        }
        case DPI::SPLINE:
        case DPI::SPLINEPOINTS:
            ++splines;
            break;
        case DPI::TEXT:
        case DPI::MTEXT:
            ++textos;
            marcasTexto.push_back({p1});
            ampliar(p1);
            break;
        case DPI::INSERT:
            ++bloques;
            break;
        default:
            break;  // puntos, cotas, sombreados, imágenes: no se cortan
        }
        delete e;
    }

    // ---- Agrupar extremos que coinciden (barrido ordenado por X) ----
    const int n = static_cast<int>(extremos.size());
    std::vector<int> orden(n);
    for (int i = 0; i < n; ++i) orden[i] = i;
    std::sort(orden.begin(), orden.end(), [&](int a, int b) { return extremos[a].p.x() < extremos[b].p.x(); });
    Union nodos(n);
    for (int i = 0; i < n; ++i) {
        const QPointF &pi = extremos[orden[i]].p;
        for (int j = i + 1; j < n; ++j) {
            const QPointF &pj = extremos[orden[j]].p;
            if (pj.x() - pi.x() > TOL_UNION) break;
            if (dist(pi, pj) <= TOL_UNION) nodos.unir(orden[i], orden[j]);
        }
    }
    QHash<int, int> tamanoNodo;
    for (int i = 0; i < n; ++i) tamanoNodo[nodos.raiz(i)] += 1;

    // ---- Contornos: elementos unidos por sus extremos ----
    Union contornos(numElementosAbiertos);
    QHash<int, int> primerElementoDelNodo;
    for (int i = 0; i < n; ++i) {
        const int r = nodos.raiz(i);
        if (primerElementoDelNodo.contains(r)) contornos.unir(primerElementoDelNodo[r], extremos[i].elemento);
        else primerElementoDelNodo[r] = extremos[i].elemento;
    }
    QHash<int, bool> contornoCerrado;  // raíz del contorno -> ¿todos sus nodos tienen exactamente 2 extremos?
    for (int i = 0; i < n; ++i) {
        const int c = contornos.raiz(extremos[i].elemento);
        if (!contornoCerrado.contains(c)) contornoCerrado[c] = true;
        if (tamanoNodo[nodos.raiz(i)] != 2) contornoCerrado[c] = false;
    }
    int cerrados = cerradosSolos, abiertos = 0;
    for (auto it = contornoCerrado.cbegin(); it != contornoCerrado.cend(); ++it)
        (it.value() ? cerrados : abiertos) += 1;

    // ---- Extremos sueltos, huecos pequeños y cruces ----
    std::vector<QPointF> sueltos, cruces;
    QSet<int> nodoContado;
    for (int i = 0; i < n; ++i) {
        const int r = nodos.raiz(i);
        const int t = tamanoNodo[r];
        if (t == 1) sueltos.push_back(extremos[i].p);
        else if (t >= 3 && !nodoContado.contains(r)) { nodoContado.insert(r); cruces.push_back(extremos[i].p); }
    }
    int huecos = 0;
    double huecoMayor = 0;
    if (sueltos.size() <= 4000) {
        for (size_t i = 0; i < sueltos.size(); ++i) {
            double mejor = 1e300;
            for (size_t j = 0; j < sueltos.size(); ++j)
                if (i != j) mejor = std::min(mejor, dist(sueltos[i], sueltos[j]));
            if (mejor < HUECO_MAX && i < sueltos.size()) {
                ++huecos;
                huecoMayor = std::max(huecoMayor, mejor);
            }
        }
    }
    huecos /= 2;  // cada hueco tiene dos extremos

    // ---- Marcas en la capa REVISION ----
    const int problemas = static_cast<int>(sueltos.size() + cruces.size() + marcasDuplicado.size()
                                           + marcasDiminuto.size() + marcasTexto.size());
    if (problemas > 0) {
        double tam = 1.0;
        if (maxX > minX) tam = std::max(1.0, std::max(maxX - minX, maxY - minY) * 0.012);
        doc->setLayer(CAPA_REVISION);
        doc->setCurrentLayerProperties(0xFF00FF, DPI::Width00, DPI::SolidLine);
        auto marcar = [&](const QPointF &p, double factor) {
            QPointF c = p;
            doc->addCircle(&c, tam * factor);
        };
        for (const QPointF &p : sueltos) marcar(p, 1.0);
        for (const QPointF &p : cruces) marcar(p, 1.4);
        for (const Marca &m : marcasDuplicado) marcar(m.p, 0.8);
        for (const Marca &m : marcasDiminuto) marcar(m.p, 0.6);
        for (const Marca &m : marcasTexto) marcar(m.p, 1.2);
        doc->setLayer(capaAnterior);
    }
    doc->updateView();

    // ---- Resumen ----
    QString t;
    t += QString::fromUtf8("<b>Contornos cerrados (listos para cortar):</b> %1<br>").arg(cerrados);
    if (abiertos == 0 && problemas == 0 && splines == 0 && bloques == 0) {
        t += QString::fromUtf8("<br><span style='color:#008000'><b>✔ Todo listo para cortar.</b></span><br>"
                               "No se encontraron contornos abiertos, líneas repetidas ni tramos diminutos.");
    } else {
        t += QString::fromUtf8("<br><b>Para revisar</b> (marcados con círculos morados en la capa REVISION):<ul>");
        if (abiertos > 0)
            t += QString::fromUtf8("<li><b>%1 contorno(s) abierto(s)</b> con %2 extremo(s) suelto(s). "
                                   "La antorcha no cierra la pieza.</li>").arg(abiertos).arg(sueltos.size());
        if (huecos > 0)
            t += QString::fromUtf8("<li>%1 hueco(s) pequeño(s) entre extremos (el mayor de %2 mm): "
                                   "probablemente faltó unir las líneas.</li>").arg(huecos).arg(num(huecoMayor));
        if (!cruces.empty())
            t += QString::fromUtf8("<li>%1 punto(s) donde se juntan 3 o más líneas (cruces o líneas sobrantes).</li>")
                     .arg(cruces.size());
        if (duplicados > 0)
            t += QString::fromUtf8("<li>%1 línea(s) o círculo(s) repetido(s) encima de otro: la antorcha "
                                   "pasaría dos veces. Bórrelos.</li>").arg(duplicados);
        if (diminutos > 0)
            t += QString::fromUtf8("<li>%1 tramo(s) diminuto(s) de menos de %2 mm.</li>").arg(diminutos).arg(num(TRAMO_MIN));
        if (textos > 0)
            t += QString::fromUtf8("<li>%1 texto(s) en capas de corte: para cortarlos hay que explotarlos "
                                   "(Modificar → Explotar texto en letras), o pasarlos a MARCADO.</li>").arg(textos);
        if (splines > 0)
            t += QString::fromUtf8("<li>%1 curva(s) spline: no se pueden revisar aquí; verifique que "
                                   "cierren con las piezas vecinas.</li>").arg(splines);
        if (bloques > 0)
            t += QString::fromUtf8("<li>%1 bloque(s): explótelos (Modificar → Explotar) para revisarlos.</li>")
                     .arg(bloques);
        t += "</ul>";
        t += QString::fromUtf8("Cuando corrija, vuelva a usar <b>Revisar para corte</b>: las marcas viejas se borran solas.");
    }
    t += QString::fromUtf8("<br><br><small>Se revisan todas las capas visibles menos MARCADO, REFERENCIA y REVISION.</small>");

    QMessageBox caja(parent);
    caja.setWindowTitle(QString::fromUtf8("EDWIN CNC — Revisar para corte"));
    caja.setTextFormat(Qt::RichText);
    caja.setText(t);
    caja.setIcon(abiertos == 0 && problemas == 0 ? QMessageBox::Information : QMessageBox::Warning);
    caja.exec();
}

void EdwinCnc::laminaDeReferencia(Document_Interface *doc, QWidget *parent)
{
    QSettings s;
    s.beginGroup("EdwinCnc");

    QDialog dlg(parent);
    dlg.setWindowTitle(QString::fromUtf8("EDWIN CNC — Lámina de referencia"));
    auto lay = new QVBoxLayout(&dlg);
    auto ayuda = new QLabel(QString::fromUtf8(
        "Dibuja el borde de la lámina en la capa REFERENCIA (gris, a trazos).\n"
        "Esa capa nunca se corta: sirve para ver si las piezas caben."));
    lay->addWidget(ayuda);
    auto form = new QFormLayout;
    auto medidas = new QComboBox;
    struct Medida { const char *nombre; double ancho, largo; };
    const Medida lista[] = {
        {"1220 × 2440 mm (4 × 8 pies)", 1220, 2440},
        {"1500 × 3000 mm", 1500, 3000},
        {"1000 × 2000 mm", 1000, 2000},
        {"1220 × 1220 mm (4 × 4 pies)", 1220, 1220},
        {"610 × 1220 mm (2 × 4 pies)", 610, 1220},
        {"Otra medida", 0, 0},
    };
    for (const Medida &m : lista) medidas->addItem(QString::fromUtf8(m.nombre));
    auto ancho = new QDoubleSpinBox;
    auto largo = new QDoubleSpinBox;
    for (auto sp : {ancho, largo}) {
        sp->setRange(1, 100000);
        sp->setDecimals(1);
        sp->setSuffix(" mm");
    }
    ancho->setValue(s.value("ancho", 1220.0).toDouble());
    largo->setValue(s.value("largo", 2440.0).toDouble());
    medidas->setCurrentIndex(medidas->count() - 1);
    for (int i = 0; i < medidas->count() - 1; ++i)
        if (lista[i].ancho == ancho->value() && lista[i].largo == largo->value()) medidas->setCurrentIndex(i);
    QObject::connect(medidas, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&](int i) {
        if (lista[i].ancho > 0) { ancho->setValue(lista[i].ancho); largo->setValue(lista[i].largo); }
    });
    auto horizontal = new QCheckBox(QString::fromUtf8("Lámina acostada (el lado largo en X)"));
    horizontal->setChecked(s.value("horizontal", true).toBool());
    form->addRow(QString::fromUtf8("Medida:"), medidas);
    form->addRow(QString::fromUtf8("Ancho:"), ancho);
    form->addRow(QString::fromUtf8("Largo:"), largo);
    form->addRow(QString(), horizontal);
    lay->addLayout(form);
    auto nota = new QLabel(QString::fromUtf8("La esquina inferior izquierda queda en 0,0 (el cero de la máquina)."));
    nota->setStyleSheet("color: gray;");
    lay->addWidget(nota);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("Dibujar lámina"));
    botones->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;

    s.setValue("ancho", ancho->value());
    s.setValue("largo", largo->value());
    s.setValue("horizontal", horizontal->isChecked());

    double x = std::min(ancho->value(), largo->value());
    double y = std::max(ancho->value(), largo->value());
    if (horizontal->isChecked()) std::swap(x, y);

    const QString anterior = doc->getCurrentLayer();
    if (!doc->getAllLayer().contains("REFERENCIA", Qt::CaseInsensitive)) {
        doc->setLayer("REFERENCIA");
        doc->setCurrentLayerProperties(0x808080, DPI::Width00, DPI::DashLine);
    } else {
        doc->setLayer("REFERENCIA");
    }
    std::vector<QPointF> rect = {QPointF(0, 0), QPointF(x, 0), QPointF(x, y), QPointF(0, y)};
    doc->addLines(rect, true);
    doc->setLayer(anterior);
    doc->updateView();
}
