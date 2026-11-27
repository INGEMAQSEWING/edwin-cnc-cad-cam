/*****************************************************************************/
/*  herramientas.cpp - Casa rápida, ejes, columnas, muro, pórtico y          */
/*                     cimentación de DuctusArq 2D                           */
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
#include "circulacion.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>
#include <memory>

using da::u;

namespace {

// Ventana con casillas de medidas que recuerdan lo último escrito (por sistema de medidas).
struct Ventana {
    QDialog dlg;
    QVBoxLayout *lay;
    QFormLayout *form;
    std::vector<QLineEdit *> campos;
    std::vector<QCheckBox *> casillas;
    arq::Unidades un;

    Ventana(QWidget *parent, const QString &titulo, const QString &nota) : dlg(parent), un(da::unidades()) {
        dlg.setWindowTitle(titulo);
        lay = new QVBoxLayout(&dlg);
        if (!nota.isEmpty()) {
            auto l = new QLabel(nota);
            l->setWordWrap(true);
            lay->addWidget(l);
        }
        auto ayuda = new QLabel("<i>" + da::ayudaUnidades(un) + u(" Escala: ")
                                + QString::fromStdString(arq::textoEscala(un.escala, un.sistema)) + "</i>");
        ayuda->setWordWrap(true);
        lay->addWidget(ayuda);
        form = new QFormLayout;
        lay->addLayout(form);
    }
    QLineEdit *medida(const QString &etiqueta, const char *clave, const char *metrico, const char *imperial,
                      const QString &ayuda = QString()) {
        QLineEdit *e = da::campo(form, etiqueta, da::clave(un, clave), da::sug(un, metrico, imperial), ayuda);
        campos.push_back(e);
        return e;
    }
    QLineEdit *textoLibre(const QString &etiqueta, const char *clave, const QString &porDefecto) {
        QLineEdit *e = da::campo(form, etiqueta, u(clave), porDefecto);
        campos.push_back(e);
        return e;
    }
    QCheckBox *casilla(const QString &texto, const char *clave, bool porDefecto) {
        QSettings cfg;
        auto c = new QCheckBox(texto);
        c->setChecked(cfg.value(u("DuctusArq/") + u(clave), porDefecto).toBool());
        c->setProperty("clave", u(clave));
        lay->addWidget(c);
        casillas.push_back(c);
        return c;
    }
    bool abrir() {
        auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        botones->button(QDialogButtonBox::Ok)->setText(u("Dibujar"));
        QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        lay->addWidget(botones);
        return dlg.exec() == QDialog::Accepted;
    }
    void guardar() {
        da::guardar(campos);
        QSettings cfg;
        for (QCheckBox *c : casillas)
            cfg.setValue(u("DuctusArq/") + c->property("clave").toString(), c->isChecked());
    }
};

// Las ventanas se vuelven a abrir si una medida está mal escrita (no se pierde lo escrito).
template <typename Leer>
bool pedir(Ventana &v, Leer leer) {
    if (!v.abrir()) return false;
    while (!leer())
        if (v.dlg.exec() != QDialog::Accepted) return false;
    v.guardar();
    return true;
}

// Título del plano debajo del dibujo, con la escala.
void tituloPlano(Document_Interface *doc, const QPointF &base, double xCentro, double y, const QString &t,
                 const arq::Unidades &un, const QString &extra = QString()) {
    da::capa(doc, "A-TEXTOS");
    da::texto(doc, base, arq::P(xCentro, y), t, un.papel(5), DPI::HAlignCenter, DPI::VAlignTop);
    QString esc = u("ESCALA ") + QString::fromStdString(arq::textoEscala(un.escala, un.sistema));
    if (!extra.isEmpty()) esc += u("   ·   ") + extra;
    da::texto(doc, base, arq::P(xCentro, y - un.papel(8)), esc, un.papel(3), DPI::HAlignCenter, DPI::VAlignTop);
}

QString nombreEjeX(size_t i) { return QString::number(static_cast<int>(i) + 1); }
QString nombreEjeY(size_t i) { return QString::fromStdString(arq::letraEje(static_cast<int>(i))); }

// Rejilla de ejes: verticales (1, 2, 3...) con burbuja abajo y horizontales (A, B, C...) con
// burbuja a la izquierda. x0..x1 / y0..y1 = lo que ocupa el dibujo.
void rejilla(Document_Interface *doc, const QPointF &base, const std::vector<double> &xs, const std::vector<double> &ys,
             double x0, double x1, double y0, double y1, const arq::Unidades &un, double fuera) {
    const double sobra = un.papel(4);
    for (size_t i = 0; i < xs.size(); ++i)
        da::eje(doc, base, arq::P(xs[i], y1 + sobra), arq::P(xs[i], y0 - fuera), nombreEjeX(i), un);
    for (size_t j = 0; j < ys.size(); ++j)
        da::eje(doc, base, arq::P(x1 + sobra, ys[j]), arq::P(x0 - fuera, ys[j]), nombreEjeY(j), un);
}

void columnasEn(Document_Interface *doc, const QPointF &base, const std::vector<arq::Rect> &cols) {
    if (cols.empty()) return;
    da::capa(doc, "A-COLUMNAS");
    for (const arq::Rect &r : cols) {
        da::rectangulo(doc, base, r);
        da::relleno(doc, base, r);
    }
}

QString area(double a, const arq::Unidades &un) {
    if (un.sistema == arq::Sistema::Metrico) return u("%1 m²").arg(a, 0, 'f', 2);
    return u("%1 ft²").arg(a / 144.0, 0, 'f', 1);
}

} // namespace

// ---------------------------------------------------------------------------
// CASA RÁPIDA: «una casa de 6 x 15» -> muros exteriores, divisiones, ejes, columnas y cotas.
void DuctusArq::casaRapida(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Casa rápida (planta por medidas)"),
              u("Dibuja la planta de una casa rectangular: muros exteriores con su espesor, divisiones "
                "interiores, ejes con burbujas, columnas en los cruces y cotas. Después puede "
                "agregar o borrar muros con las demás herramientas."));
    auto ancho = v.medida(u("Ancho (frente) de la casa:"), "casaAncho", "6", "20'");
    auto largo = v.medida(u("Largo (fondo) de la casa:"), "casaLargo", "15", "50'");
    auto mExt = v.medida(u("Espesor muro exterior:"), "casaMuroExt", "0.15", "6\"");
    auto mInt = v.medida(u("Espesor muro interior:"), "casaMuroInt", "0.10", "4\"");
    auto divX = v.medida(u("Espacios a lo ancho (opcional):"), "casaDivX", "3", "10'",
                         u("Medidas de los espacios de izquierda a derecha, separadas por espacios. "
                           "Ejemplo en 6 m: «3» divide en 3 + 3. Vacío = sin muros interiores."));
    auto divY = v.medida(u("Espacios a lo largo (opcional):"), "casaDivY", "4 3.5 4", "13' 12' 13'",
                         u("Medidas de los espacios de abajo hacia arriba. Ejemplo en 15 m: «4 3.5 4» "
                           "(lo que sobra queda como último espacio)."));
    auto pas = v.medida(u("Pasillo (ancho libre, 0 = sin pasillo):"), "casaPasillo", "1.00", "3'-6\"",
                        u("Pasillo a lo largo, a la derecha del primer espacio a lo ancho, desde la sala hasta el "
                          "fondo: cada alcoba y el baño quedan con su puerta al pasillo. Con pasillo escriba UN "
                          "solo número en «Espacios a lo ancho» (el ancho de los cuartos de la izquierda)."));
    auto col = v.medida(u("Columnas (lado, 0 = sin columnas):"), "casaColumna", "0.25", "10\"");
    auto cEjes = v.casilla(u("Ejes con burbujas"), "casaEjes", true);
    auto cCotas = v.casilla(u("Cotas"), "casaCotas", true);
    auto cTitulo = v.casilla(u("Título «PLANTA ARQUITECTÓNICA» y área"), "casaTitulo", true);

    arq::Casa c;
    std::vector<double> ex, ey;
    if (!pedir(v, [&]() {
        double colLado = 0;
        if (!da::medida(parent, ancho, v.un, c.ancho, 0.0001, u("Ancho"))) return false;
        if (!da::medida(parent, largo, v.un, c.largo, 0.0001, u("Largo"))) return false;
        if (!da::medida(parent, mExt, v.un, c.muroExt, 0.0001, u("Muro exterior"))) return false;
        if (!da::medida(parent, mInt, v.un, c.muroInt, 0.0001, u("Muro interior"))) return false;
        if (!da::lista(parent, divX, v.un, ex, u("Espacios a lo ancho"), true)) return false;
        if (!da::lista(parent, divY, v.un, ey, u("Espacios a lo largo"), true)) return false;
        if (!da::medida(parent, col, v.un, colLado, 0, u("Columnas"))) return false;
        if (!da::medida(parent, pas, v.un, c.pasillo, 0, u("Pasillo"))) return false;
        if (2 * c.muroExt >= std::min(c.ancho, c.largo)) {
            QMessageBox::warning(parent, u("Revise las medidas"), u("Los muros exteriores son más gruesos que la casa."));
            return false;
        }
        c.columna = colLado;
        c.divX.clear();
        c.divY.clear();
        double acum = 0;
        for (double d : ex) { acum += d; c.divX.push_back(acum); }
        acum = 0;
        for (double d : ey) { acum += d; c.divY.push_back(acum); }
        if (c.pasillo > 0) {
            if (c.divX.size() != 1 || c.divY.empty()) {
                QMessageBox::warning(parent, u("Pasillo"),
                                     u("Con pasillo escriba UN solo número en «Espacios a lo ancho» (el ancho de los "
                                       "cuartos de la izquierda) y al menos uno en «Espacios a lo largo» (el fondo de "
                                       "la sala). O escriba 0 en «Pasillo» para una casa sin pasillo."));
                return false;
            }
            if (arq::casaGeom(c).bocaAncho <= 0) {
                QMessageBox::warning(parent, u("Pasillo"),
                                     u("El pasillo no cabe: los cuartos de la izquierda más el pasillo son más anchos "
                                       "que la casa. Reduzca el ancho de los cuartos o el del pasillo."));
                return false;
            }
        }
        return true;
    })) return;

    QPointF base;
    if (!doc->getPoint(&base, u("Casa rápida: haga clic donde va la esquina de abajo a la izquierda"))) return;

    const arq::CasaGeom g = arq::casaGeom(c);
    const arq::Unidades &un = v.un;
    // Muros: contorno limpio de muros + columnas (las uniones en L, T y X quedan sin rayas).
    std::vector<arq::Rect> todo = g.muros;
    todo.insert(todo.end(), g.columnas.begin(), g.columnas.end());
    da::capa(doc, "A-MUROS");
    da::segmentos(doc, base, arq::contornoUnion(todo));
    columnasEn(doc, base, g.columnas);
    {   // muros y columnas para el render 3D de la planta amoblada
        std::vector<std::vector<arq::P>> polis;
        for (const arq::Rect &r : todo) polis.push_back(arq::esquinas(r));
        da::datosMuros(doc, base, polis, v.un);
    }
    if (g.bocaAncho > 0) {
        // Boca abierta sala-pasillo: se guarda como vano (sin puerta) para que el área de la sala
        // no se escape al pasillo y la revisión de circulación sepa que están unidos.
        arq::Vano b = arq::bocaPasillo(g);
        b.o = b.o + arq::P(base.x(), base.y());
        da::dato(doc, QPointF(b.o.x + b.ancho / 2, b.o.y - un.papel(4)),
                 u("DUCTUS-VANO|VA-1|%1|%2|%3|0||%4").arg(u(arq::nombreTipoVano(b.tipo))).arg(b.ancho, 0, 'f', 4)
                     .arg(un.metros(2.10), 0, 'f', 4)
                     .arg(QString::fromStdString(arq::esquinasVano(b) + "|" + arq::datoLadoVano(b))), un);
    }

    const double dCota1 = un.papel(8), dCota2 = un.papel(15), dEje = un.papel(22);
    if (cEjes->isChecked())
        rejilla(doc, base, g.ejesX, g.ejesY, 0, c.ancho, 0, c.largo, un, cCotas->isChecked() ? dEje : un.papel(8));
    if (cCotas->isChecked()) {
        da::capa(doc, "A-COTAS");
        if (g.ejesX.size() >= 2) da::cadenaX(doc, base, g.ejesX, 0, -dCota1);
        da::cota(doc, base, arq::P(0, 0), arq::P(c.ancho, 0), arq::P(c.ancho, -dCota2));
        if (g.ejesY.size() >= 2) da::cadenaY(doc, base, g.ejesY, 0, -dCota1);
        da::cota(doc, base, arq::P(0, 0), arq::P(0, c.largo), arq::P(-dCota2, c.largo));
    }
    if (cTitulo->isChecked())
        tituloPlano(doc, base, c.ancho / 2, -(cEjes->isChecked() ? dEje + un.papel(14) : dCota2 + un.papel(8)),
                    u("PLANTA ARQUITECTÓNICA"), un,
                    u("ÁREA CONSTRUIDA ") + area(c.ancho * c.largo, un) + u("  (")
                        + da::textoLong(c.ancho, un) + u(" x ") + da::textoLong(c.largo, un) + u(")"));
    da::capa(doc, "A-MUROS");
    doc->updateView();
}

// ---------------------------------------------------------------------------
// EJES: rejilla con burbujas (1, 2, 3 ... y A, B, C ...) y cotas entre ejes.
void DuctusArq::ejes(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Ejes"),
              u("Escriba las distancias entre ejes. El primer clic es el cruce de los ejes 1 y A."));
    auto dx = v.medida(u("Distancias a lo ancho (ejes 1, 2, 3 ...):"), "ejesX", "3.50 3.00 3.50", "12' 10' 12'");
    auto dy = v.medida(u("Distancias a lo largo (ejes A, B, C ...):"), "ejesY", "4.00 3.00", "13' 10'");
    auto cCotas = v.casilla(u("Cotas entre ejes y total"), "ejesCotas", true);
    std::vector<double> lx, ly;
    if (!pedir(v, [&]() {
        return da::lista(parent, dx, v.un, lx, u("Distancias a lo ancho"), false)
            && da::lista(parent, dy, v.un, ly, u("Distancias a lo largo"), false);
    })) return;
    QPointF base;
    if (!doc->getPoint(&base, u("Ejes: haga clic en el cruce de los ejes 1 y A"))) return;
    const arq::Unidades &un = v.un;
    const auto xs = arq::posicionesEjes(lx), ys = arq::posicionesEjes(ly);
    const double fuera = un.papel(cCotas->isChecked() ? 22 : 10);
    rejilla(doc, base, xs, ys, xs.front(), xs.back(), ys.front(), ys.back(), un, fuera);
    if (cCotas->isChecked()) {
        da::capa(doc, "A-COTAS");
        da::cadenaX(doc, base, xs, ys.front(), ys.front() - un.papel(8));
        if (xs.size() > 2) da::cota(doc, base, arq::P(xs.front(), ys.front()), arq::P(xs.back(), ys.front()),
                                    arq::P(xs.back(), ys.front() - un.papel(15)));
        da::cadenaY(doc, base, ys, xs.front(), xs.front() - un.papel(8));
        if (ys.size() > 2) da::cota(doc, base, arq::P(xs.front(), ys.front()), arq::P(xs.front(), ys.back()),
                                    arq::P(xs.front() - un.papel(15), ys.back()));
    }
    doc->updateView();
}

// ---------------------------------------------------------------------------
// COLUMNAS: en todos los cruces de ejes o haciendo clic en cada punto. Van rellenas.
void DuctusArq::columnas(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Columnas"), u("Columnas rectangulares rellenas, centradas en el punto."));
    auto modo = new QComboBox;
    modo->addItem(u("Haciendo clic en cada punto"));
    modo->addItem(u("En todos los cruces de ejes (distancias de abajo)"));
    QSettings cfg;
    modo->setCurrentIndex(cfg.value("DuctusArq/colModo", 0).toInt());
    v.form->addRow(u("Dónde:"), modo);
    auto b = v.medida(u("Ancho (a lo ancho, X):"), "colB", "0.25", "10\"");
    auto h = v.medida(u("Alto (a lo largo, Y):"), "colH", "0.25", "10\"");
    auto dx = v.medida(u("Distancias entre ejes a lo ancho:"), "ejesX", "3.50 3.00 3.50", "12' 10' 12'");
    auto dy = v.medida(u("Distancias entre ejes a lo largo:"), "ejesY", "4.00 3.00", "13' 10'");
    double cb = 0, ch = 0;
    std::vector<double> lx, ly;
    if (!pedir(v, [&]() {
        if (!da::medida(parent, b, v.un, cb, 0.0001, u("Ancho"))) return false;
        if (!da::medida(parent, h, v.un, ch, 0.0001, u("Alto"))) return false;
        if (modo->currentIndex() == 1)
            return da::lista(parent, dx, v.un, lx, u("Distancias a lo ancho"), false)
                && da::lista(parent, dy, v.un, ly, u("Distancias a lo largo"), false);
        return true;
    })) return;
    cfg.setValue("DuctusArq/colModo", modo->currentIndex());

    auto columna = [&](double x, double y) { return arq::rect(x - cb / 2, y - ch / 2, x + cb / 2, y + ch / 2); };
    if (modo->currentIndex() == 1) {
        QPointF base;
        if (!doc->getPoint(&base, u("Columnas: haga clic en el cruce de los ejes 1 y A"))) return;
        std::vector<arq::Rect> cols;
        for (double x : arq::posicionesEjes(lx))
            for (double y : arq::posicionesEjes(ly)) cols.push_back(columna(x, y));
        columnasEn(doc, base, cols);
        std::vector<std::vector<arq::P>> polis;
        for (const arq::Rect &r : cols) polis.push_back(arq::esquinas(r));
        da::datosMuros(doc, base, polis, v.un);
    } else {
        QPointF p;
        int n = 0;
        while (doc->getPoint(&p, u("Columna %1: haga clic en el centro (Enter o Esc para terminar)").arg(n + 1))) {
            columnasEn(doc, p, {columna(0, 0)});
            da::datosMuros(doc, p, {arq::esquinas(columna(0, 0))}, v.un);
            ++n;
            doc->updateView();
        }
    }
    doc->updateView();
}

// ---------------------------------------------------------------------------
// MURO: por puntos, con espesor y esquinas a inglete (como MLINE / MURO de AutoCAD Architecture).
void DuctusArq::muro(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Muro"),
              u("Haga clic en los puntos del recorrido del muro. Enter termina; si el último clic cae "
                "sobre el primero, el muro se cierra. Use F8 (orto) para muros rectos."));
    auto esp = v.medida(u("Espesor:"), "muroEsp", "0.15", "6\"");
    auto just = new QComboBox;
    just->addItem(u("Los puntos son el eje (centro del muro)"));
    just->addItem(u("Los puntos son la cara izquierda"));
    just->addItem(u("Los puntos son la cara derecha"));
    QSettings cfg;
    just->setCurrentIndex(cfg.value("DuctusArq/muroJust", 0).toInt());
    v.form->addRow(u("Puntos:"), just);
    double e = 0;
    if (!pedir(v, [&]() { return da::medida(parent, esp, v.un, e, 0.0001, u("Espesor")); })) return;
    cfg.setValue("DuctusArq/muroJust", just->currentIndex());
    const int justificacion = just->currentIndex() == 0 ? 0 : (just->currentIndex() == 1 ? 1 : -1);

    std::vector<arq::P> pts;
    QPointF p, anterior;
    if (!doc->getPoint(&p, u("Muro: primer punto"))) return;
    pts.emplace_back(p.x(), p.y());
    anterior = p;
    bool cerrado = false;
    const double tol = e * 0.05;
    while (doc->getPoint(&p, u("Muro: siguiente punto (Enter para terminar)"), &anterior)) {
        arq::P n(p.x(), p.y());
        if (arq::largo(n - pts.back()) < tol) continue;
        if (pts.size() >= 3 && arq::largo(n - pts.front()) < tol) { cerrado = true; break; }
        pts.push_back(n);
        anterior = p;
    }
    if (pts.size() < 2) return;
    const arq::Muro m = arq::muroPorPuntos(pts, e, justificacion, cerrado);
    const QPointF cero(0, 0);
    da::capa(doc, "A-MUROS");
    da::polilinea(doc, cero, m.izquierda, cerrado);
    da::polilinea(doc, cero, m.derecha, cerrado);
    {   // un cuadrilátero por tramo, para el render 3D
        std::vector<std::vector<arq::P>> polis;
        const size_t n = m.izquierda.size();
        for (size_t i = 0; i + 1 < n || (cerrado && i < n); ++i) {
            const size_t j = (i + 1) % n;
            polis.push_back({m.izquierda[i], m.izquierda[j], m.derecha[j], m.derecha[i]});
        }
        da::datosMuros(doc, cero, polis, v.un);
    }
    if (!cerrado) {  // tapas en los extremos
        da::linea(doc, cero, m.izquierda.front(), m.derecha.front());
        da::linea(doc, cero, m.izquierda.back(), m.derecha.back());
    }
    doc->updateView();
}

// ---------------------------------------------------------------------------
// PÓRTICO en alzado: columnas, vigas por piso, zapatas, ejes, niveles y cotas.
void DuctusArq::portico(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Pórtico (alzado)"),
              u("Dibuja un pórtico visto de frente: columnas, una viga en cada piso, zapatas bajo el "
                "terreno, ejes, niveles y cotas. El clic es el pie del primer eje en el nivel ±0.00."));
    auto nombre = v.textoLibre(u("Nombre:"), "porNombre", u("PÓRTICO EJE A"));
    auto vanos = v.medida(u("Vanos (distancias entre ejes):"), "porVanos", "4.00 4.50", "13' 15'");
    auto alturas = v.medida(u("Altura de cada piso (de nivel a nivel):"), "porAlturas", "2.70 2.70", "10' 10'");
    auto col = v.medida(u("Ancho de columna:"), "porCol", "0.30", "12\"");
    auto viga = v.medida(u("Alto de viga:"), "porViga", "0.40", "16\"");
    auto zA = v.medida(u("Ancho de zapata (0 = sin zapatas):"), "porZapA", "1.20", "4'");
    auto zH = v.medida(u("Alto de zapata:"), "porZapH", "0.40", "16\"");
    auto prof = v.medida(u("Profundidad de cimentación (fondo de zapata):"), "porProf", "1.20", "4'");
    arq::Portico p;
    if (!pedir(v, [&]() {
        if (!da::lista(parent, vanos, v.un, p.vanos, u("Vanos"), false)) return false;
        if (!da::lista(parent, alturas, v.un, p.alturas, u("Alturas"), false)) return false;
        if (!da::medida(parent, col, v.un, p.columna, 0.0001, u("Columna"))) return false;
        if (!da::medida(parent, viga, v.un, p.viga, 0.0001, u("Viga"))) return false;
        if (!da::medida(parent, zA, v.un, p.zapataAncho, 0, u("Ancho de zapata"))) return false;
        if (p.zapataAncho > 0) {
            if (!da::medida(parent, zH, v.un, p.zapataAlto, 0.0001, u("Alto de zapata"))) return false;
            if (!da::medida(parent, prof, v.un, p.profundidad, 0.0001, u("Profundidad"))) return false;
            if (p.zapataAlto >= p.profundidad) {
                QMessageBox::warning(parent, u("Revise las medidas"), u("La zapata es más alta que la profundidad."));
                return false;
            }
        }
        for (double h : p.alturas)
            if (h <= p.viga) {
                QMessageBox::warning(parent, u("Revise las medidas"), u("Cada piso debe ser más alto que la viga."));
                return false;
            }
        return true;
    })) return;
    QPointF base;
    if (!doc->getPoint(&base, u("Pórtico: haga clic en el pie del primer eje (nivel ±0.00)"))) return;

    const arq::Unidades &un = v.un;
    const arq::PorticoGeom g = arq::porticoGeom(p);
    std::vector<arq::Rect> todo = g.piezas;
    todo.insert(todo.end(), g.zapatas.begin(), g.zapatas.end());
    da::capa(doc, "S-PORTICOS");
    da::segmentos(doc, base, arq::contornoUnion(todo));

    const double x0 = g.ejes.front(), x1 = g.ejes.back(), arriba = g.niveles.back();
    const double fondo = p.zapataAncho > 0 ? -p.profundidad : 0;
    // Terreno (nivel ±0.00) a los lados.
    da::capa(doc, "A-SIMBOLOS");
    const double sale = un.papel(15);
    da::linea(doc, base, arq::P(x0 - sale, 0), arq::P(x1 + sale, 0));
    // Ejes con burbuja arriba.
    for (size_t i = 0; i < g.ejes.size(); ++i)
        da::eje(doc, base, arq::P(g.ejes[i], fondo - un.papel(3)), arq::P(g.ejes[i], arriba + un.papel(22)),
                nombreEjeX(i), un);
    // Niveles a la derecha.
    da::capa(doc, "A-TEXTOS");
    const double xn = x1 + p.columna / 2 + un.papel(20);
    for (double n : g.niveles) {
        const QString t = (n == 0 ? u("N ±") : u("N +")) + da::textoLong(n, un);
        da::capa(doc, "A-SIMBOLOS");
        da::linea(doc, base, arq::P(xn - un.papel(2), n), arq::P(xn + un.papel(16), n));
        da::linea(doc, base, arq::P(xn, n), arq::P(xn - un.papel(1.5), n + un.papel(2.5)));
        da::linea(doc, base, arq::P(xn, n), arq::P(xn + un.papel(1.5), n + un.papel(2.5)));
        da::linea(doc, base, arq::P(xn - un.papel(1.5), n + un.papel(2.5)), arq::P(xn + un.papel(1.5), n + un.papel(2.5)));
        da::capa(doc, "A-TEXTOS");
        da::texto(doc, base, arq::P(xn + un.papel(3), n + un.papel(1)), t, un.papel(2.5), DPI::HAlignLeft, DPI::VAlignBottom);
    }
    // Cotas: vanos arriba y alturas a la derecha.
    da::capa(doc, "A-COTAS");
    da::cadenaX(doc, base, g.ejes, arriba, arriba + un.papel(10));
    std::vector<double> ys = g.niveles;
    if (p.zapataAncho > 0) ys.insert(ys.begin(), fondo);
    da::cadenaY(doc, base, ys, x1 + p.columna / 2, x1 + p.columna / 2 + un.papel(10));
    tituloPlano(doc, base, (x0 + x1) / 2, fondo - un.papel(10), nombre->text().trimmed().toUpper(), un);
    da::capa(doc, "S-PORTICOS");
    doc->updateView();
}

// ---------------------------------------------------------------------------
// CIMENTACIÓN en planta: zapatas en los cruces de ejes, vigas de amarre y columnas.
void DuctusArq::cimentacion(Document_Interface *doc, QWidget *parent)
{
    Ventana v(parent, u("Cimentación (planta)"),
              u("Zapatas cuadradas en cada cruce de ejes, unidas con vigas de cimentación. "
                "El clic es el cruce de los ejes 1 y A. Las medidas de diseño las da el ingeniero "
                "estructural (NSR-10 / ACI 318): aquí solo se dibujan."));
    auto dx = v.medida(u("Distancias entre ejes a lo ancho:"), "ejesX", "3.50 3.00 3.50", "12' 10' 12'");
    auto dy = v.medida(u("Distancias entre ejes a lo largo:"), "ejesY", "4.00 3.00", "13' 10'");
    auto zap = v.medida(u("Lado de zapata:"), "cimZapata", "1.20", "4'");
    auto viga = v.medida(u("Ancho de viga de cimentación (0 = sin vigas):"), "cimViga", "0.30", "12\"");
    auto col = v.medida(u("Columna (lado, 0 = sin columnas):"), "cimCol", "0.30", "12\"");
    auto cCotas = v.casilla(u("Ejes y cotas"), "cimCotas", true);
    arq::Cimentacion c;
    std::vector<double> lx, ly;
    if (!pedir(v, [&]() {
        if (!da::lista(parent, dx, v.un, lx, u("Distancias a lo ancho"), false)) return false;
        if (!da::lista(parent, dy, v.un, ly, u("Distancias a lo largo"), false)) return false;
        if (!da::medida(parent, zap, v.un, c.zapata, 0.0001, u("Zapata"))) return false;
        if (!da::medida(parent, viga, v.un, c.viga, 0, u("Viga"))) return false;
        return da::medida(parent, col, v.un, c.columna, 0, u("Columna"));
    })) return;
    QPointF base;
    if (!doc->getPoint(&base, u("Cimentación: haga clic en el cruce de los ejes 1 y A"))) return;

    const arq::Unidades &un = v.un;
    c.ejesX = arq::posicionesEjes(lx);
    c.ejesY = arq::posicionesEjes(ly);
    const arq::CimentacionGeom g = arq::cimentacionGeom(c);
    std::vector<arq::Rect> todo = g.zapatas;
    todo.insert(todo.end(), g.vigas.begin(), g.vigas.end());
    da::capa(doc, "S-CIMENTACION");
    da::segmentos(doc, base, arq::contornoUnion(todo));
    columnasEn(doc, base, g.columnas);

    const double x0 = c.ejesX.front() - c.zapata / 2, x1 = c.ejesX.back() + c.zapata / 2;
    const double y0 = c.ejesY.front() - c.zapata / 2, y1 = c.ejesY.back() + c.zapata / 2;
    double abajo = y0 - un.papel(8);
    if (cCotas->isChecked()) {
        rejilla(doc, base, c.ejesX, c.ejesY, x0, x1, y0, y1, un, un.papel(22));
        da::capa(doc, "A-COTAS");
        da::cadenaX(doc, base, c.ejesX, y0, y0 - un.papel(8));
        da::cadenaY(doc, base, c.ejesY, x0, x0 - un.papel(8));
        abajo = y0 - un.papel(22) - un.papel(14);
    }
    tituloPlano(doc, base, (x0 + x1) / 2, abajo, u("PLANTA DE CIMENTACIÓN"), un,
                u("ZAPATAS ") + da::textoLong(c.zapata, un) + u(" x ") + da::textoLong(c.zapata, un));
    da::capa(doc, "S-CIMENTACION");
    doc->updateView();
}
