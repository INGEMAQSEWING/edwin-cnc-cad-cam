/*****************************************************************************/
/*  generador.cpp - EDWIN CNC: generar código G para Mach3 (plasma X/Y)      */
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
// Reemplaza a SheetCAM: lee las piezas del dibujo, aplica la separación de la silueta y las colitas, ordena el
// corte, dibuja el recorrido en la capa RECORRIDO y guarda el .tap con el formato del post "Mach3 plasma no Z".
// El cálculo está en cam.h (se prueba solo con pruebas_cam.cpp).

#include "edwincnc.h"
#include "cam.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <cmath>

namespace {

const QString CAPA_RECORRIDO = "RECORRIDO";
const QString CAPA_RAPIDO = "RECORRIDO RAPIDO";

// Capas que nunca se cortan (las mismas de "Revisar para corte" más las del recorrido).
bool capaSinCorte(const QString &capa) {
    const QString c = capa.trimmed().toUpper();
    return c == "MARCADO" || c == "REFERENCIA" || c == "REVISION" || c == CAPA_RECORRIDO || c == CAPA_RAPIDO;
}

QString u(const char *s) { return QString::fromUtf8(s); }

// Nombre sin tildes ni caracteres raros (Mach3 lee el .tap en ASCII).
std::string ascii(const QString &s) {
    QString n = s.normalized(QString::NormalizationForm_D);
    std::string r;
    for (QChar ch : n) {
        const ushort c = ch.unicode();
        if (c >= 32 && c < 127 && c != '(' && c != ')') r += char(c);
    }
    return r.empty() ? "Pieza" : r;
}

// Perfiles guardados (uno por material y espesor). Valores por defecto: los del .tap de SheetCAM del usuario.
void guardarPerfil(QSettings &s, const QString &nombre, const cam::Parametros &p) {
    s.beginGroup("perfiles/" + nombre);
    s.setValue("kerf", p.kerf);
    s.setValue("velocidad", p.velocidad);
    s.setValue("perforar", p.perforar);
    s.setValue("entradaTipo", p.entradaTipo);
    s.setValue("entrada", p.entrada);
    s.setValue("salidaTipo", p.salidaTipo);
    s.setValue("salida", p.salida);
    s.setValue("desacelerarMm", p.desacelerarMm);
    s.setValue("desacelerarPct", p.desacelerarPct);
    s.setValue("pausaFinal", p.pausaFinal);
    s.setValue("volverAlOrigen", p.volverAlOrigen);
    s.endGroup();
}
cam::Parametros leerPerfil(QSettings &s, const QString &nombre) {
    cam::Parametros p;
    s.beginGroup("perfiles/" + nombre);
    p.kerf = s.value("kerf", p.kerf).toDouble();
    p.velocidad = s.value("velocidad", p.velocidad).toDouble();
    p.perforar = s.value("perforar", p.perforar).toDouble();
    p.entradaTipo = s.value("entradaTipo", p.entradaTipo).toInt();
    p.entrada = s.value("entrada", p.entrada).toDouble();
    p.salidaTipo = s.value("salidaTipo", p.salidaTipo).toInt();
    p.salida = s.value("salida", p.salida).toDouble();
    p.desacelerarMm = s.value("desacelerarMm", p.desacelerarMm).toDouble();
    p.desacelerarPct = s.value("desacelerarPct", p.desacelerarPct).toDouble();
    p.pausaFinal = s.value("pausaFinal", p.pausaFinal).toDouble();
    p.volverAlOrigen = s.value("volverAlOrigen", p.volverAlOrigen).toBool();
    s.endGroup();
    return p;
}

// Dibuja un tramo en el dibujo (en la capa activa).
void dibujar(Document_Interface *doc, const cam::Seg &s) {
    if (!s.arco) {
        QPointF a(s.a.x, s.a.y), b(s.b.x, s.b.y);
        doc->addLine(&a, &b);
        return;
    }
    QPointF c(s.c.x, s.c.y);
    const double r = cam::radio(s);
    // addArc va siempre contra el reloj, en grados.
    double a1 = cam::angulo(s.c, s.ccw ? s.a : s.b) * 180 / cam::PI;
    double a2 = cam::angulo(s.c, s.ccw ? s.b : s.a) * 180 / cam::PI;
    if (std::fabs(cam::barrido(s) - 2 * cam::PI) < 1e-9) { doc->addCircle(&c, r); return; }
    doc->addArc(&c, r, a1, a2);
}

} // namespace

void EdwinCnc::generarCodigoG(Document_Interface *doc, QWidget *parent)
{
    QSettings s;
    s.beginGroup("EdwinCnc");
    QStringList perfiles = s.value("listaPerfiles").toStringList();
    if (perfiles.isEmpty()) {
        perfiles << u("Mi máquina");
        guardarPerfil(s, perfiles[0], cam::Parametros());
        s.setValue("listaPerfiles", perfiles);
    }

    // ─── Ventana ───
    QDialog dlg(parent);
    dlg.setWindowTitle(u("EDWIN CNC — Generar código G (Mach3 plasma)"));
    auto lay = new QVBoxLayout(&dlg);
    auto intro = new QLabel(u(
        "Corta todas las piezas de las capas visibles (menos MARCADO, REFERENCIA, REVISION y RECORRIDO).\n"
        "Primero los huecos, después el contorno de afuera de cada pieza, y la más cercana primero."));
    intro->setWordWrap(true);
    lay->addWidget(intro);

    auto filaPerfil = new QHBoxLayout;
    auto perfil = new QComboBox;
    perfil->addItems(perfiles);
    perfil->setCurrentText(s.value("perfil", perfiles[0]).toString());
    auto guardarComo = new QPushButton(u("Guardar como…"));
    auto borrar = new QPushButton(u("Borrar"));
    filaPerfil->addWidget(new QLabel(u("Perfil (material y espesor):")));
    filaPerfil->addWidget(perfil, 1);
    filaPerfil->addWidget(guardarComo);
    filaPerfil->addWidget(borrar);
    lay->addLayout(filaPerfil);

    auto form = new QFormLayout;
    auto numero = [](double min, double max, int dec, const QString &suf) {
        auto sp = new QDoubleSpinBox;
        sp->setRange(min, max);
        sp->setDecimals(dec);
        sp->setSuffix(suf);
        return sp;
    };
    auto kerf = numero(0, 20, 2, " mm");
    kerf->setToolTip(u("Ancho de la ranura que deja el plasma. La antorcha se corre la mitad hacia el desperdicio,\n"
                       "por fuera del contorno de afuera y por dentro de los huecos, para que la pieza salga a la medida."));
    auto velocidad = numero(1, 50000, 0, " mm/min");
    auto perforar = numero(0, 30, 2, " s");
    perforar->setToolTip(u("Pausa con la antorcha encendida antes de avanzar, para que perfore la lámina (G04)."));
    auto tipoEntrada = new QComboBox;
    auto tipoSalida = new QComboBox;
    for (auto c : {tipoEntrada, tipoSalida}) c->addItems({u("Ninguna"), u("Recta"), u("Curva")});
    auto entrada = numero(0, 100, 1, " mm");
    auto salida = numero(0, 100, 1, " mm");
    tipoEntrada->setToolTip(u("Dónde perfora: fuera de la pieza, del lado del desperdicio, para que el hueco de la perforación no la dañe."));
    tipoSalida->setToolTip(u("Cómo sale del corte al terminar, también del lado del desperdicio."));
    auto filaEntrada = new QHBoxLayout;
    filaEntrada->addWidget(tipoEntrada);
    filaEntrada->addWidget(entrada);
    auto filaSalida = new QHBoxLayout;
    filaSalida->addWidget(tipoSalida);
    filaSalida->addWidget(salida);
    auto desMm = numero(0, 100, 1, " mm");
    auto desPct = numero(5, 100, 0, " %");
    desMm->setToolTip(u("En los últimos milímetros del contorno baja la velocidad (0 = no baja)."));
    auto filaDes = new QHBoxLayout;
    filaDes->addWidget(new QLabel(u("últimos")));
    filaDes->addWidget(desMm);
    filaDes->addWidget(new QLabel(u("a")));
    filaDes->addWidget(desPct);
    auto pausa = numero(0, 30, 2, " s");
    pausa->setToolTip(u("Pausa al terminar cada contorno, antes de apagar la antorcha (0 = sin pausa)."));
    auto origen = new QCheckBox(u("Al terminar, volver al origen (G00 X0 Y0)"));
    auto lado = new QComboBox;
    lado->addItems({u("Automático: la pieza por fuera y los huecos por dentro"),
                    u("Todo por fuera de la línea"),
                    u("Todo por dentro de la línea (por ejemplo, recortar algo dentro de una lámina)"),
                    u("Sobre la línea (sin separación)")});
    lado->setToolTip(u("De qué lado de la línea pasa la antorcha. Siempre empieza en Automático."));
    form->addRow(u("Lado del corte:"), lado);
    form->addRow(u("Separación de la silueta (Kerf):"), kerf);
    form->addRow(u("Velocidad de corte (Feed rate):"), velocidad);
    form->addRow(u("Tiempo para perforar (Pierce delay):"), perforar);
    form->addRow(u("Colita de entrada (Lead in):"), filaEntrada);
    form->addRow(u("Colita al final (Lead out):"), filaSalida);
    form->addRow(u("Desaceleración al terminar:"), filaDes);
    form->addRow(u("Pausa antes de apagar:"), pausa);
    form->addRow(QString(), origen);
    lay->addLayout(form);
    auto dibujarRecorrido = new QCheckBox(u("Dibujar el recorrido en la capa RECORRIDO para revisarlo"));
    dibujarRecorrido->setChecked(s.value("dibujarRecorrido", true).toBool());
    lay->addWidget(dibujarRecorrido);

    auto ponerValores = [&](const cam::Parametros &p) {
        kerf->setValue(p.kerf);
        velocidad->setValue(p.velocidad);
        perforar->setValue(p.perforar);
        tipoEntrada->setCurrentIndex(p.entradaTipo);
        entrada->setValue(p.entrada);
        tipoSalida->setCurrentIndex(p.salidaTipo);
        salida->setValue(p.salida);
        desMm->setValue(p.desacelerarMm);
        desPct->setValue(p.desacelerarPct);
        pausa->setValue(p.pausaFinal);
        origen->setChecked(p.volverAlOrigen);
    };
    auto leerValores = [&]() {
        cam::Parametros p;
        p.kerf = kerf->value();
        p.velocidad = velocidad->value();
        p.perforar = perforar->value();
        p.entradaTipo = tipoEntrada->currentIndex();
        p.entrada = entrada->value();
        p.salidaTipo = tipoSalida->currentIndex();
        p.salida = salida->value();
        p.desacelerarMm = desMm->value();
        p.desacelerarPct = desPct->value();
        p.pausaFinal = pausa->value();
        p.volverAlOrigen = origen->isChecked();
        p.lado = lado->currentIndex();
        return p;
    };
    ponerValores(leerPerfil(s, perfil->currentText()));
    QObject::connect(perfil, &QComboBox::currentTextChanged, &dlg, [&](const QString &n) { ponerValores(leerPerfil(s, n)); });
    QObject::connect(guardarComo, &QPushButton::clicked, &dlg, [&]() {
        bool ok = false;
        const QString n = QInputDialog::getText(&dlg, u("Guardar perfil"), u("Nombre (por ejemplo: Lámina 1/8 o Acero 3 mm):"),
                                                QLineEdit::Normal, perfil->currentText(), &ok).trimmed();
        if (!ok || n.isEmpty()) return;
        guardarPerfil(s, n, leerValores());
        if (!perfiles.contains(n)) {
            perfiles << n;
            s.setValue("listaPerfiles", perfiles);
            perfil->addItem(n);
        }
        perfil->setCurrentText(n);
    });
    QObject::connect(borrar, &QPushButton::clicked, &dlg, [&]() {
        if (perfiles.size() <= 1) return;
        const QString n = perfil->currentText();
        s.remove("perfiles/" + n);
        perfiles.removeAll(n);
        s.setValue("listaPerfiles", perfiles);
        perfil->removeItem(perfil->currentIndex());
    });

    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Generar código G…"));
    botones->button(QDialogButtonBox::Cancel)->setText(u("Cancelar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;

    cam::Parametros prm = leerValores();
    guardarPerfil(s, perfil->currentText(), prm);  // el perfil recuerda los últimos valores usados
    s.setValue("perfil", perfil->currentText());
    s.setValue("dibujarRecorrido", dibujarRecorrido->isChecked());

    // ─── Leer el dibujo (y borrar el recorrido anterior) ───
    cam::Entrada e;
    int splines = 0, textos = 0, bloques = 0;
    QList<Plug_Entity *> todas;
    doc->getAllEntities(&todas, true);
    for (Plug_Entity *ent : todas) {
        QHash<int, QVariant> d;
        ent->getData(&d);
        const QString capa = d.value(DPI::LAYER).toString();
        const QString cu = capa.trimmed().toUpper();
        if (cu == CAPA_RECORRIDO || cu == CAPA_RAPIDO) { doc->removeEntity(ent); delete ent; continue; }
        if (capaSinCorte(capa)) { delete ent; continue; }
        const cam::P p1{d.value(DPI::STARTX).toDouble(), d.value(DPI::STARTY).toDouble()};
        switch (d.value(DPI::ETYPE).toInt()) {
        case DPI::LINE:
            cam::agregarLinea(e, p1, {d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble()});
            break;
        case DPI::ARC:
            cam::agregarArco(e, p1, d.value(DPI::RADIUS).toDouble(), d.value(DPI::STARTANGLE).toDouble(), d.value(DPI::ENDANGLE).toDouble());
            break;
        case DPI::CIRCLE:
            cam::agregarCirculo(e, p1, d.value(DPI::RADIUS).toDouble());
            break;
        case DPI::ELLIPSE:
            cam::agregarElipse(e, p1, {d.value(DPI::ENDX).toDouble(), d.value(DPI::ENDY).toDouble()}, d.value(DPI::HEIGHT).toDouble(),
                               d.value(DPI::STARTANGLE).toDouble(), d.value(DPI::ENDANGLE).toDouble());
            break;
        case DPI::POLYLINE: {
            QList<Plug_VertexData> v;
            ent->getPolylineData(&v);
            std::vector<cam::P> pts;
            std::vector<double> bul;
            for (const Plug_VertexData &x : v) { pts.push_back({x.point.x(), x.point.y()}); bul.push_back(x.bulge); }
            if (!pts.empty()) cam::agregarPolilinea(e, pts, bul, d.value(DPI::CLOSEPOLY).toBool());
            break;
        }
        case DPI::SPLINE:
        case DPI::SPLINEPOINTS: ++splines; break;
        case DPI::TEXT:
        case DPI::MTEXT: ++textos; break;
        case DPI::INSERT: ++bloques; break;
        default: break;
        }
        delete ent;
    }

    const cam::Resultado r = cam::preparar(e, prm);
    if (r.cortes.empty()) {
        doc->updateView();
        QMessageBox::warning(parent, u("EDWIN CNC — Generar código G"),
                             u("No hay contornos cerrados para cortar en las capas visibles.\n\n"
                               "Use EDWIN CNC → Revisar para corte para encontrar los contornos abiertos."));
        return;
    }

    // ─── Recorrido en el dibujo: corte en azul, movimientos rápidos a trazos y un círculo en cada perforación ───
    if (dibujarRecorrido->isChecked()) {
        const QString anterior = doc->getCurrentLayer();
        doc->setLayer(CAPA_RAPIDO);
        doc->setCurrentLayerProperties(0x9A9A9A, DPI::Width00, DPI::DashLine);
        cam::P donde{0, 0};
        for (const cam::Corte &k : r.cortes) {
            const cam::P ini = !k.entrada.empty() ? k.entrada.front().a : k.camino.front().a;
            dibujar(doc, {false, donde, ini, {}, true});
            donde = !k.salida.empty() ? k.salida.back().b : k.camino.back().b;
        }
        if (prm.volverAlOrigen) dibujar(doc, {false, donde, {0, 0}, {}, true});
        doc->setLayer(CAPA_RECORRIDO);
        doc->setCurrentLayerProperties(0x0080FF, DPI::Width00, DPI::SolidLine);
        for (const cam::Corte &k : r.cortes) {
            const cam::P ini = !k.entrada.empty() ? k.entrada.front().a : k.camino.front().a;
            QPointF c(ini.x, ini.y);
            doc->addCircle(&c, std::max(0.3, prm.kerf / 2));
            for (const cam::Seg &t : k.entrada) dibujar(doc, t);
            for (const cam::Seg &t : k.camino) dibujar(doc, t);
            for (const cam::Seg &t : k.salida) dibujar(doc, t);
        }
        doc->setLayer(anterior);
    }
    doc->updateView();

    // ─── Guardar el .tap ───
    const QString carpeta = s.value("carpetaTap", QDir::homePath()).toString();
    const QString ultimo = s.value("ultimoTap", "PIEZA.TAP").toString();
    QString archivo = QFileDialog::getSaveFileName(parent, u("Guardar código G para Mach3"), QDir(carpeta).filePath(ultimo),
                                                   u("Código G de Mach3 (*.tap);;Código G (*.nc *.ngc);;Todos (*.*)"));
    if (archivo.isEmpty()) return;
    const QFileInfo info(archivo);
    if (info.suffix().isEmpty()) archivo += ".tap";
    s.setValue("carpetaTap", QFileInfo(archivo).absolutePath());
    s.setValue("ultimoTap", QFileInfo(archivo).fileName());
    prm.nombreArchivo = ascii(QFileInfo(archivo).fileName().toUpper());
    prm.nombrePieza = ascii(QFileInfo(archivo).completeBaseName());
    const std::string g = cam::codigoG(r, prm);
    QFile f(archivo);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(parent, u("EDWIN CNC"), u("No se pudo guardar el archivo:\n%1").arg(archivo));
        return;
    }
    f.write(g.data(), qint64(g.size()));
    f.close();

    // ─── Resumen ───
    int huecos = 0;
    double largoCorte = 0;
    for (const cam::Corte &k : r.cortes) {
        if (k.hueco) ++huecos;
        for (const cam::Seg &t : k.entrada) largoCorte += cam::largo(t);
        for (const cam::Seg &t : k.camino) largoCorte += cam::largo(t);
        for (const cam::Seg &t : k.salida) largoCorte += cam::largo(t);
    }
    const double minutos = largoCorte / std::max(1.0, prm.velocidad) + r.cortes.size() * prm.perforar / 60.0;
    QString t = u("<b>Código G guardado:</b><br>%1<br><br>").arg(archivo.toHtmlEscaped());
    t += u("<b>%1</b> contorno(s): %2 de afuera y %3 hueco(s).<br>").arg(r.cortes.size()).arg(int(r.cortes.size()) - huecos).arg(huecos);
    if (prm.lado != 0) t += u("Lado del corte: <b>%1</b>.<br>").arg(lado->currentText());
    t += u("Corte: %1 mm · tiempo aproximado: %2 min (sin contar los movimientos rápidos).<br>")
             .arg(QString::number(largoCorte, 'f', 0)).arg(QString::number(minutos, 'f', 1));
    QStringList avisos;
    if (r.abiertos > 0) avisos << u("%1 contorno(s) abierto(s) NO se cortan (use Revisar para corte).").arg(r.abiertos);
    if (r.problemas > 0) avisos << u("%1 contorno(s) tienen detalles más chicos que el corte: revíselos en la capa RECORRIDO.").arg(r.problemas);
    if (splines > 0) avisos << u("%1 curva(s) spline no se cortan: conviértalas en polilínea.").arg(splines);
    if (textos > 0) avisos << u("%1 texto(s) no se cortan: explótelos en letras o páselos a MARCADO.").arg(textos);
    if (bloques > 0) avisos << u("%1 bloque(s) no se cortan: explótelos (Modificar → Explotar).").arg(bloques);
    if (!avisos.isEmpty()) t += u("<br><b>Para revisar:</b><ul><li>") + avisos.join("</li><li>") + "</li></ul>";
    if (dibujarRecorrido->isChecked())
        t += u("<br><small>El recorrido quedó dibujado en la capa RECORRIDO (azul) y los movimientos rápidos en "
               "RECORRIDO RAPIDO (gris a trazos). Se borran y se vuelven a dibujar cada vez que genere el código.</small>");
    QMessageBox caja(parent);
    caja.setWindowTitle(u("EDWIN CNC — Código G listo"));
    caja.setTextFormat(Qt::RichText);
    caja.setText(t);
    caja.setIcon(avisos.isEmpty() ? QMessageBox::Information : QMessageBox::Warning);
    caja.exec();
}
