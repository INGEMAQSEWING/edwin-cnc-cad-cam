/*****************************************************************************/
/*  render.cpp - Render de la planta amoblada con Blender (DuctusArq 2D)     */
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
// Junta los muros, puertas, ventanas y muebles guardados en el dibujo (capa A-DATOS), los
// escribe en plano.json y llama a Blender (gratis) con render/ductusarq_render.py, que saca
// dos imágenes: planta_superior.png (desde arriba, para ventas) y planta_3d.png (para redes).

#include "ductusarq.h"
#include "comun.h"
#include "muebles.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

using da::u;

namespace {

// Blender: el que el usuario escogió antes, o el más nuevo instalado (normal o portátil).
QString buscarBlender() {
    QSettings cfg;
    const QString guardado = cfg.value("DuctusArq/blender").toString();
    if (!guardado.isEmpty() && QFileInfo::exists(guardado)) return guardado;
    QStringList raices = {u("C:/Program Files/Blender Foundation"), u("C:/Program Files (x86)/Blender Foundation"),
                          qEnvironmentVariable("LOCALAPPDATA") + u("/Blender Foundation"),
                          qEnvironmentVariable("ProgramFiles") + u("/Blender Foundation")};
    QString mejor;
    for (const QString &raiz : raices) {
        QDir d(raiz);
        if (!d.exists()) continue;
        QStringList versiones = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (int i = versiones.size() - 1; i >= 0; --i) {
            const QString exe = d.filePath(versiones[i] + u("/blender.exe"));
            if (QFileInfo::exists(exe)) {
                if (mejor.isEmpty() || versiones[i] > QFileInfo(mejor).dir().dirName()) mejor = exe;
                break;
            }
        }
    }
    return mejor;
}

QJsonArray puntos(const std::vector<arq::P> &pol) {
    QJsonArray a;
    for (const arq::P &p : pol) a.append(QJsonArray{p.x, p.y});
    return a;
}

} // namespace

void DuctusArq::renderPlanta(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades un = da::unidades();
    const bool imperial = un.sistema == arq::Sistema::Imperial;

    // 1) Datos del dibujo
    QJsonArray muros, vanos, muebles;
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MURO|")) {
        std::vector<arq::P> pol;
        if (arq::leerDatoMuro(t.toStdString(), pol)) muros.append(puntos(pol));
    }
    for (const QString &t : da::leerDatos(doc, "DUCTUS-VANO|")) {
        arq::DatoVano dv;
        std::vector<arq::Seg> cierre;
        if (!arq::leerDatoVano(t.toStdString(), dv) || !arq::cierreVano(t.toStdString(), cierre)) continue;
        // cierre = [cara 1: e0 -> e1], [cara 2: e3 -> e2]
        QJsonObject o;
        o["tipo"] = QString::fromStdString(dv.tipo);
        o["alto"] = dv.alto;
        o["antepecho"] = dv.antepecho;
        o["esquinas"] = puntos({cierre[0].a, cierre[0].b, cierre[1].b, cierre[1].a});
        vanos.append(o);
    }
    for (const QString &t : da::leerDatos(doc, "DUCTUS-MUEBLE|")) {
        const QStringList p = t.split('|');
        if (p.size() < 8) continue;
        QJsonObject o;
        o["id"] = p[1];
        o["x"] = p[2].toDouble();
        o["y"] = p[3].toDouble();
        o["grados"] = p[4].toDouble();
        o["ancho"] = p[5].toDouble();
        o["fondo"] = p[6].toDouble();
        o["alto"] = p[7].toDouble();
        muebles.append(o);
    }
    if (muros.isEmpty()) {
        QMessageBox::information(parent, u("Render de la planta"),
                                 u("El dibujo no tiene muros guardados para el 3D.\n\nDibuje los muros con «Casa rápida» "
                                   "o «Muro» (los muros hechos con líneas sueltas no se pueden levantar), ponga puertas "
                                   "y ventanas con «Puerta / ventana» y los muebles con «Anexar mobiliario»."));
        return;
    }

    // 2) Ventana de opciones
    QSettings cfg;
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Render de la planta amoblada"));
    auto lay = new QVBoxLayout(&dlg);
    auto nota = new QLabel(u("Se levantan %1 muro(s) con %2 puerta(s)/ventana(s) y %3 mueble(s) y Blender (gratis) saca "
                             "dos fotos: <b>desde arriba</b> (para ventas, como plano amoblado) y <b>en 3D</b> (para "
                             "redes).<br>Tarda de 1 a 5 minutos según la calidad.")
                               .arg(muros.size()).arg(vanos.size()).arg(muebles.size()));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto f = new QFormLayout;
    lay->addLayout(f);
    auto calidad = new QComboBox;
    calidad->addItem(u("Rápida (para revisar)"), "rapida");
    calidad->addItem(u("Normal (redes sociales)"), "normal");
    calidad->addItem(u("Alta (impresión, tarda más)"), "alta");
    calidad->setCurrentIndex(cfg.value("DuctusArq/renderCalidad", 1).toInt());
    f->addRow(u("Calidad:"), calidad);
    auto alto = da::campo(f, u("Alto de los muros:"), da::clave(un, "renderAlto"), da::sug(un, "2.40", "8'"));
    QString blender = buscarBlender();
    auto lBlender = new QLabel(blender.isEmpty() ? u("<b>No encontré Blender.</b>") : blender);
    lBlender->setWordWrap(true);
    auto bBlender = new QPushButton(u("Buscar blender.exe..."));
    QObject::connect(bBlender, &QPushButton::clicked, &dlg, [&]() {
        const QString e = QFileDialog::getOpenFileName(&dlg, u("¿Dónde está blender.exe?"), u("C:/Program Files"),
                                                       u("Blender (blender.exe)"));
        if (!e.isEmpty()) { blender = e; lBlender->setText(e); cfg.setValue("DuctusArq/blender", e); }
    });
    f->addRow(u("Blender:"), lBlender);
    f->addRow(QString(), bBlender);
    auto ayuda = new QLabel(u("<i>Si no tiene Blender: descárguelo gratis de blender.org (no necesita licencia).</i>"));
    ayuda->setWordWrap(true);
    lay->addWidget(ayuda);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    botones->button(QDialogButtonBox::Ok)->setText(u("Renderizar"));
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    double altoMuro = 0;
    while (true) {
        if (dlg.exec() != QDialog::Accepted) return;
        if (!da::medida(parent, alto, un, altoMuro, 0.0001, u("Alto de los muros"))) continue;
        if (blender.isEmpty() || !QFileInfo::exists(blender)) {
            QMessageBox::warning(parent, u("Render"), u("Falta Blender: instálelo (gratis, blender.org) o búsquelo con el botón."));
            continue;
        }
        break;
    }
    da::guardar({alto});
    cfg.setValue("DuctusArq/renderCalidad", calidad->currentIndex());

    const QString guion = QCoreApplication::applicationDirPath() + u("/render/ductusarq_render.py");
    if (!QFileInfo::exists(guion)) {
        QMessageBox::warning(parent, u("Render"), u("No encontré el guion de render:\n%1\n\nReinstale DuctusArq 2D.").arg(guion));
        return;
    }

    // 3) plano.json en Documentos/DuctusArq/Render/<fecha-hora>
    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + u("/DuctusArq/Render/") + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QDir().mkpath(carpeta);
    QJsonObject raiz;
    raiz["unidad_m"] = imperial ? 0.0254 : 1.0;
    raiz["alto_muro"] = altoMuro;
    raiz["muros"] = muros;
    raiz["vanos"] = vanos;
    raiz["muebles"] = muebles;
    const QString plano = carpeta + u("/plano.json");
    QFile fj(plano);
    if (!fj.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(parent, u("Render"), u("No pude escribir %1").arg(plano));
        return;
    }
    fj.write(QJsonDocument(raiz).toJson(QJsonDocument::Indented));
    fj.close();

    // 4) Blender en segundo plano, con barra de espera
    QProcess blenderProc;
    blenderProc.setProcessChannelMode(QProcess::MergedChannels);
    blenderProc.start(blender, {u("-b"), u("--factory-startup"), u("-P"), guion, u("--"), plano, carpeta,
                                calidad->currentData().toString()});
    QProgressDialog espera(u("Blender está armando la planta en 3D y sacando las fotos..."), u("Cancelar"), 0, 0, parent);
    espera.setWindowTitle(u("Render de la planta"));
    espera.setWindowModality(Qt::WindowModal);
    espera.setMinimumDuration(0);
    espera.show();
    QString salida;
    while (!blenderProc.waitForFinished(200)) {
        QCoreApplication::processEvents();
        salida += QString::fromLocal8Bit(blenderProc.readAll());
        if (salida.contains("planta_3d.png")) espera.setLabelText(u("Foto en 3D lista. Falta la foto desde arriba..."));
        if (espera.wasCanceled()) {
            blenderProc.kill();
            blenderProc.waitForFinished(3000);
            return;
        }
        if (blenderProc.state() == QProcess::NotRunning) break;
    }
    salida += QString::fromLocal8Bit(blenderProc.readAll());
    espera.close();
    QFile log(carpeta + u("/blender.log"));
    if (log.open(QIODevice::WriteOnly)) log.write(salida.toUtf8());

    if (!salida.contains("DUCTUSARQ-LISTO")) {
        QMessageBox::warning(parent, u("Render"),
                             u("Blender no terminó el render. Revise el archivo blender.log en:\n%1").arg(carpeta));
        QDesktopServices::openUrl(QUrl::fromLocalFile(carpeta));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(carpeta + u("/planta_superior.png")));
    QDesktopServices::openUrl(QUrl::fromLocalFile(carpeta + u("/planta_3d.png")));
    QMessageBox::information(parent, u("Render de la planta"),
                             u("Listo. Las dos fotos quedaron en:\n%1\n\nplanta_superior.png — desde arriba (ventas)\n"
                               "planta_3d.png — en 3D (redes sociales)").arg(QDir::toNativeSeparators(carpeta)));
}
