/*****************************************************************************/
/*  ventas.cpp - Plano de ventas a color para ventas y mercadeo (DuctusArq)  */
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
// Arma la lámina (ventas.h) con los muros, puertas, ventanas, espacios y muebles guardados
// en el dibujo y la pinta en varios formatos listos para el equipo de ventas y mercadeo:
//   plano_ventas.pdf          folleto (Carta, vertical)
//   plano_ventas.png          imagen grande (2400 x 3000)
//   instagram_vertical.png    1080 x 1350
//   instagram_cuadrado.png    1080 x 1080
//   historia_9x16.png         1080 x 1920 (historias y reels)

#include "ductusarq.h"
#include "comun.h"
#include "ventas.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <QMessageBox>
#include <QSettings>

using da::u;

namespace da {
std::vector<arq::EspacioVentas> espaciosConForma(Document_Interface *doc, const arq::Unidades &un);  // espacios.cpp
}

namespace {

struct Encabezado {
    QString proyecto, titulo, nota;
    QImage logo;
};

QColor col(arq::Color c) { return QColor(c.r, c.g, c.b); }

// Pinta la lámina dentro de «pagina» (px del dispositivo): proyecto y logo arriba, plano al
// centro, título y nota abajo.
void pintar(QPainter &p, const QRectF &pagina, const arq::Lamina &L, const Encabezado &enc)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(pagina, QColor(255, 255, 255));
    const double lado = std::min(pagina.width(), pagina.height());
    const QColor tinta(70, 56, 44);
    const QString serif = u("Georgia");

    // Arriba: logo (izquierda) y nombre del proyecto (centro)
    const double altoCab = pagina.height() * 0.085;
    QRectF cab(pagina.left() + lado * 0.05, pagina.top() + lado * 0.03, pagina.width() - lado * 0.10, altoCab);
    if (!enc.logo.isNull()) {
        const double h = cab.height() * 0.9;
        const double w = h * enc.logo.width() / std::max(1, enc.logo.height());
        p.drawImage(QRectF(cab.left(), cab.top() + (cab.height() - h) / 2, w, h), enc.logo);
    }
    if (!enc.proyecto.isEmpty()) {
        QFont f(serif);
        f.setPixelSize(std::max(8, static_cast<int>(lado * 0.034)));
        f.setLetterSpacing(QFont::PercentageSpacing, 112);
        p.setFont(f);
        p.setPen(tinta);
        p.drawText(cab, Qt::AlignCenter, enc.proyecto.toUpper());
    }

    // Abajo: título grande y nota pequeña
    const double altoPie = pagina.height() * 0.12;
    QRectF pie(pagina.left(), pagina.bottom() - altoPie, pagina.width(), altoPie);
    if (!enc.titulo.isEmpty()) {
        QFont f(serif);
        f.setPixelSize(std::max(8, static_cast<int>(lado * 0.042)));
        f.setLetterSpacing(QFont::PercentageSpacing, 108);
        p.setFont(f);
        p.setPen(tinta);
        p.drawText(QRectF(pie.left(), pie.top(), pie.width(), pie.height() * 0.55), Qt::AlignCenter, enc.titulo.toUpper());
    }
    if (!enc.nota.isEmpty()) {
        QFont f(u("Arial"));
        f.setPixelSize(std::max(6, static_cast<int>(lado * 0.016)));
        p.setFont(f);
        p.setPen(QColor(120, 110, 100));
        p.drawText(QRectF(pie.left() + lado * 0.04, pie.top() + pie.height() * 0.55, pie.width() - lado * 0.08,
                          pie.height() * 0.35), Qt::AlignCenter | Qt::TextWordWrap, enc.nota);
    }

    // Plano al centro
    const QRectF zona(pagina.left() + lado * 0.05, cab.bottom() + lado * 0.02, pagina.width() - lado * 0.10,
                      pie.top() - cab.bottom() - lado * 0.03);
    const double cw = L.caja.x2 - L.caja.x1, ch = L.caja.y2 - L.caja.y1;
    if (cw <= 0 || ch <= 0 || zona.width() <= 0 || zona.height() <= 0) { p.restore(); return; }
    const double s = std::min(zona.width() / cw, zona.height() / ch);
    const double ox = zona.left() + (zona.width() - cw * s) / 2, oy = zona.top() + (zona.height() - ch * s) / 2;
    auto M = [&](arq::P q) { return QPointF(ox + (q.x - L.caja.x1) * s, oy + (L.caja.y2 - q.y) * s); };

    for (int capa = 0; capa <= 6; ++capa) {
        // Rellenos de la capa (los pisos de un mismo color se juntan para que no se vean costuras)
        if (capa == 0) {
            std::vector<std::pair<QRgb, QPainterPath>> grupos;
            for (const arq::Relleno &r : L.rellenos) {
                if (r.capa != 0) continue;
                const QRgb clave = col(r.c).rgb();
                QPolygonF pol;
                for (const arq::P &q : r.pol) pol << M(q);
                auto it = std::find_if(grupos.begin(), grupos.end(), [&](const auto &g) { return g.first == clave; });
                if (it == grupos.end()) { grupos.push_back({clave, QPainterPath()}); it = grupos.end() - 1; }
                it->second.addPolygon(pol);
                it->second.closeSubpath();
            }
            for (auto &g : grupos) {
                QPainterPath camino = g.second.simplified();
                p.setPen(QPen(QColor(g.first), 1.0));
                p.setBrush(QColor(g.first));
                p.drawPath(camino);
            }
        } else {
            for (const arq::Relleno &r : L.rellenos) {
                if (r.capa != capa) continue;
                QPolygonF pol;
                for (const arq::P &q : r.pol) pol << M(q);
                p.setPen(QPen(col(r.c), 0.8));
                p.setBrush(col(r.c));
                p.drawPolygon(pol);
            }
        }
        p.setBrush(Qt::NoBrush);
        for (const arq::Trazo &t : L.trazos) {
            if (t.capa != capa) continue;
            QPen pen(col(t.c), std::max(0.6, t.grosor * s));
            pen.setCapStyle(Qt::FlatCap);
            p.setPen(pen);
            p.drawLine(M(t.a), M(t.b));
        }
        for (const arq::TrazoArco &a : L.arcos) {
            if (a.capa != capa) continue;
            double a1 = a.a.a1, a2 = a.a.a2;
            if (a2 < a1) a2 += 6.283185307179586;
            QPolygonF pol;
            const int n = 48;
            for (int i = 0; i <= n; ++i) {
                const double t = a1 + (a2 - a1) * i / n;
                pol << M(arq::P(a.a.c.x + a.a.r * std::cos(t), a.a.c.y + a.a.r * std::sin(t)));
            }
            p.setPen(QPen(col(a.c), std::max(0.6, a.grosor * s)));
            p.drawPolyline(pol);
        }
    }
    // Textos: se achican si no caben en el ancho del espacio.
    for (const arq::TextoLamina &t : L.textos) {
        QFont f(serif);
        f.setBold(t.negrita);
        double px = std::max(4.0, t.alto * s);
        f.setPixelSize(static_cast<int>(px));
        const QString texto = QString::fromStdString(t.texto);
        QFontMetricsF fm(f);
        const double ancho = fm.horizontalAdvance(texto), maximo = t.anchoMax * s;
        if (ancho > maximo && ancho > 0) {
            px = std::max(4.0, px * maximo / ancho);
            f.setPixelSize(static_cast<int>(px));
        }
        p.setFont(f);
        p.setPen(col(t.c));
        const QPointF c = M(t.pos);
        p.drawText(QRectF(c.x() - maximo, c.y() - px, maximo * 2, px * 2), Qt::AlignCenter, texto);
    }
    p.restore();
}

bool guardarPng(const QString &archivo, int w, int h, const arq::Lamina &L, const Encabezado &enc)
{
    QImage img(w, h, QImage::Format_RGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    pintar(p, QRectF(0, 0, w, h), L, enc);
    p.end();
    return img.save(archivo);
}

bool guardarPdf(const QString &archivo, bool carta, const arq::Lamina &L, const Encabezado &enc)
{
    QPdfWriter pdf(archivo);
    pdf.setPageSize(QPageSize(carta ? QPageSize::Letter : QPageSize::A4));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    pdf.setResolution(300);
    pdf.setTitle(enc.proyecto.isEmpty() ? enc.titulo : enc.proyecto + u(" - ") + enc.titulo);
    pdf.setCreator(u("DuctusArq 2D"));
    QPainter p;
    if (!p.begin(&pdf)) return false;
    pintar(p, QRectF(0, 0, pdf.width(), pdf.height()), L, enc);
    p.end();
    return true;
}

} // namespace

void DuctusArq::planoVentas(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const bool imperial = un.sistema == arq::Sistema::Imperial;

    // 1) Datos del dibujo
    std::vector<std::vector<arq::P>> muros;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) muros.push_back(pol);
    }
    if (muros.empty()) {
        QMessageBox::information(parent, u("Plano de ventas"),
                                 u("El dibujo no tiene muros guardados.\n\nDibuje los muros con «Casa rápida» o «Muro», "
                                   "las puertas y ventanas con «Puerta / ventana», los nombres de los espacios con "
                                   "«Área de un espacio» y los muebles con «Mobiliario»."));
        return;
    }
    std::vector<arq::Vano> vanos;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-VANO|")) {
        arq::Vano v;
        if (arq::vanoDeDato(t.toStdString(), v)) vanos.push_back(v);
    }
    std::vector<arq::MuebleVentas> muebles;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MUEBLE|")) {
        const QStringList p = t.split('|');
        if (p.size() < 8) continue;
        const arq::Pieza *pz = arq::buscarPieza(p[1].toStdString());
        if (!pz) continue;
        const arq::P c(p[2].toDouble(), p[3].toDouble());
        const double grados = p[4].toDouble(), w = p[5].toDouble(), f = p[6].toDouble();
        arq::Dibujo d = arq::colocar(arq::dibujarPieza(*pz, w, f, un.metros(0.05), imperial), c, grados);
        d.textos.clear();
        muebles.push_back({d, arq::contornoMueble(c, w, f, grados)});
    }
    const std::vector<arq::EspacioVentas> espacios = da::espaciosConForma(doc, un);

    // 2) Opciones
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Plano de ventas a color"));
    auto lay = new QVBoxLayout(&dlg);
    int sinForma = 0;
    for (const auto &e : espacios) if (e.celdas.empty()) ++sinForma;
    QString resumen = u("Lámina a color para el equipo de <b>ventas y mercadeo</b>: muros llenos, pisos de color según el "
                        "espacio, puertas, ventanas, muebles y el nombre de cada espacio con sus medidas. Sin ejes, "
                        "cotas ni rótulo.<br><br>Encontré %1 muro(s), %2 puerta(s)/ventana(s), %3 espacio(s) con nombre "
                        "y %4 mueble(s).").arg(muros.size()).arg(vanos.size()).arg(espacios.size()).arg(muebles.size());
    if (espacios.empty())
        resumen += u("<br><b>Consejo:</b> ponga el nombre de cada espacio con «Área de un espacio» (ESP): así cada uno "
                     "sale con su color de piso y sus medidas.");
    else if (sinForma > 0)
        resumen += u("<br>%1 espacio(s) no están cerrados por muros: salen con el nombre pero sin color de piso.").arg(sinForma);
    auto nota = new QLabel(resumen);
    nota->setWordWrap(true);
    nota->setMinimumWidth(460);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    lay->addLayout(f);
    auto proyecto = new QLineEdit(cfg.value("DuctusArq/venProyecto").toString());
    proyecto->setPlaceholderText(u("Ej.: Conjunto Los Almendros · Casa tipo A"));
    f->addRow(u("Proyecto:"), proyecto);
    auto titulo = new QLineEdit(cfg.value(imperial ? "DuctusArq/venTituloFt" : "DuctusArq/venTituloM",
                                          imperial ? u("FIRST FLOOR") : u("PLANTA PRIMER PISO")).toString());
    f->addRow(u("Título:"), titulo);
    auto pie = new QLineEdit(cfg.value(imperial ? "DuctusArq/venNotaFt" : "DuctusArq/venNotaM",
                                       imperial ? u("Dimensions are approximate. Furniture is for illustration only.")
                                                : u("Medidas aproximadas. El mobiliario es ilustrativo y no está incluido."))
                                 .toString());
    f->addRow(u("Nota al pie:"), pie);
    auto filaLogo = new QHBoxLayout;
    auto logo = new QLineEdit(cfg.value("DuctusArq/venLogo").toString());
    logo->setPlaceholderText(u("(opcional) PNG o JPG de la constructora"));
    auto buscar = new QPushButton(u("Buscar..."));
    filaLogo->addWidget(logo, 1);
    filaLogo->addWidget(buscar);
    f->addRow(u("Logo:"), filaLogo);
    QObject::connect(buscar, &QPushButton::clicked, [&]() {
        const QString a = QFileDialog::getOpenFileName(&dlg, u("Logo"), logo->text(), u("Imágenes (*.png *.jpg *.jpeg)"));
        if (!a.isEmpty()) logo->setText(a);
    });
    auto paleta = new QComboBox;
    paleta->addItem(u("Cálida (beige, como los folletos de ventas)"));
    paleta->addItem(u("Gris claro (moderna)"));
    paleta->setCurrentIndex(cfg.value("DuctusArq/venPaleta", 0).toInt());
    f->addRow(u("Colores:"), paleta);
    auto medidas = new QCheckBox(u("Medidas de cada espacio (ancho x fondo)"));
    medidas->setChecked(cfg.value("DuctusArq/venMedidas", true).toBool());
    auto area = new QCheckBox(u("Área de cada espacio"));
    area->setChecked(cfg.value("DuctusArq/venArea", false).toBool());
    auto conMuebles = new QCheckBox(u("Muebles"));
    conMuebles->setChecked(cfg.value("DuctusArq/venMuebles", true).toBool());
    f->addRow(u("Mostrar:"), medidas);
    f->addRow(QString(), area);
    f->addRow(QString(), conMuebles);
    auto salidas = new QLabel(u("Se guardan juntos: <b>PDF</b> para folleto, <b>PNG</b> grande, <b>Instagram</b> vertical "
                                "y cuadrado, e <b>historia</b> 9:16."));
    salidas->setWordWrap(true);
    lay->addWidget(salidas);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Crear plano de ventas"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;
    cfg.setValue("DuctusArq/venProyecto", proyecto->text());
    cfg.setValue(imperial ? "DuctusArq/venTituloFt" : "DuctusArq/venTituloM", titulo->text());
    cfg.setValue(imperial ? "DuctusArq/venNotaFt" : "DuctusArq/venNotaM", pie->text());
    cfg.setValue("DuctusArq/venLogo", logo->text());
    cfg.setValue("DuctusArq/venPaleta", paleta->currentIndex());
    cfg.setValue("DuctusArq/venMedidas", medidas->isChecked());
    cfg.setValue("DuctusArq/venArea", area->isChecked());
    cfg.setValue("DuctusArq/venMuebles", conMuebles->isChecked());

    // 3) Lámina y archivos
    arq::OpcionesVentas op;
    op.medidas = medidas->isChecked();
    op.area = area->isChecked();
    op.muebles = conMuebles->isChecked();
    op.paleta = paleta->currentIndex();
    const arq::Lamina L = arq::planoVentas(muros, vanos, espacios, muebles, op, un.sistema);
    Encabezado enc;
    enc.proyecto = proyecto->text().trimmed();
    enc.titulo = titulo->text().trimmed();
    enc.nota = pie->text().trimmed();
    if (!logo->text().trimmed().isEmpty()) {
        enc.logo.load(logo->text().trimmed());
        if (enc.logo.isNull())
            QMessageBox::warning(parent, u("Plano de ventas"), u("No pude abrir el logo:\n%1\n\nSigo sin logo.").arg(logo->text()));
    }

    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                            + u("/DuctusArq/Ventas/") + QDateTime::currentDateTime().toString(u("yyyy-MM-dd_HH-mm-ss"));
    QDir().mkpath(carpeta);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = guardarPdf(carpeta + u("/plano_ventas.pdf"), imperial, L, enc);
    ok = guardarPng(carpeta + u("/plano_ventas.png"), 2400, 3000, L, enc) && ok;
    ok = guardarPng(carpeta + u("/instagram_vertical.png"), 1080, 1350, L, enc) && ok;
    ok = guardarPng(carpeta + u("/instagram_cuadrado.png"), 1080, 1080, L, enc) && ok;
    ok = guardarPng(carpeta + u("/historia_9x16.png"), 1080, 1920, L, enc) && ok;
    QApplication::restoreOverrideCursor();
    if (!ok) {
        QMessageBox::warning(parent, u("Plano de ventas"), u("No pude guardar todos los archivos en:\n%1").arg(carpeta));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(carpeta));
    QDesktopServices::openUrl(QUrl::fromLocalFile(carpeta + u("/plano_ventas.png")));
}
