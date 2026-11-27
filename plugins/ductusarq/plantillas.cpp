/*****************************************************************************/
/*  plantillas.cpp - Plantilla de oficina (DuctusArq)                        */
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
// PLANT: guarda o carga la «plantilla de oficina» en un archivo .json que se puede pasar a los
// demás computadores del equipo, para que todos dibujen igual:
//   - capas del dibujo con su color, grosor y tipo de línea;
//   - sistema de medidas y escala;
//   - datos del rótulo (empresa, profesional, matrícula, formato de hoja...).

#include "ductusarq.h"
#include "comun.h"

#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

using da::u;

void DuctusArq::plantillaOficina(Document_Interface *doc, QWidget *parent)
{
    QMessageBox pregunta(QMessageBox::Question, u("Plantilla de oficina"),
                         u("La plantilla guarda las capas (color, grosor y tipo de línea), el sistema de medidas y la "
                           "escala, y los datos del rótulo. Pásela a los demás computadores del equipo para que todos "
                           "dibujen igual."),
                         QMessageBox::NoButton, parent);
    auto guardar = pregunta.addButton(u("Guardar la plantilla de este dibujo..."), QMessageBox::AcceptRole);
    auto cargar = pregunta.addButton(u("Cargar una plantilla..."), QMessageBox::ActionRole);
    pregunta.addButton(QMessageBox::Cancel);
    pregunta.exec();
    const QString carpeta = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + u("/DuctusArq/Plantillas");
    QDir().mkpath(carpeta);
    QSettings cfg;

    if (pregunta.clickedButton() == guardar) {
        const QString ruta = QFileDialog::getSaveFileName(parent, u("Guardar plantilla de oficina"),
                                                          carpeta + u("/plantilla-oficina.json"), u("Plantilla (*.json)"));
        if (ruta.isEmpty()) return;
        QJsonObject raiz;
        raiz["programa"] = u("DuctusArq 2D");
        raiz["version"] = 1;
        // Capas con sus propiedades (se leen activando cada una y se deja la de antes).
        const QString antes = doc->getCurrentLayer();
        QJsonArray capas;
        for (const QString &n : doc->getAllLayer()) {
            if (n.startsWith("SUP-") || n == "0") continue;  // los planos superpuestos no van en la plantilla
            doc->setLayer(n);
            int c = 0;
            DPI::LineWidth w;
            DPI::LineType t;
            doc->getCurrentLayerProperties(&c, &w, &t);
            QJsonObject o;
            o["nombre"] = n;
            o["color"] = c;
            o["grosor"] = static_cast<int>(w);
            o["tipo"] = static_cast<int>(t);
            capas.append(o);
        }
        doc->setLayer(antes);
        raiz["capas"] = capas;
        QJsonObject ajustes;
        for (const char *k : {"DuctusArq/Sistema", "DuctusArq/EscalaMetrica", "DuctusArq/EscalaImperial"})
            if (cfg.contains(k)) ajustes[u(k)] = QJsonValue::fromVariant(cfg.value(k));
        cfg.beginGroup("DuctusArq/Rotulo");
        for (const QString &k : cfg.allKeys()) ajustes[u("DuctusArq/Rotulo/") + k] = QJsonValue::fromVariant(cfg.value(k));
        cfg.endGroup();
        raiz["ajustes"] = ajustes;
        QFile f(ruta);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(parent, u("Plantilla de oficina"), u("No pude guardar «%1».").arg(ruta));
            return;
        }
        f.write(QJsonDocument(raiz).toJson());
        QMessageBox::information(parent, u("Plantilla de oficina"),
                                 u("Plantilla guardada con %1 capa(s):\n%2\n\nCópiela a los demás computadores y use "
                                   "PLANT > Cargar.").arg(capas.size()).arg(QDir::toNativeSeparators(ruta)));
        return;
    }
    if (pregunta.clickedButton() != cargar) return;

    const QString ruta = QFileDialog::getOpenFileName(parent, u("Cargar plantilla de oficina"), carpeta, u("Plantilla (*.json)"));
    if (ruta.isEmpty()) return;
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) return;
    const QJsonObject raiz = QJsonDocument::fromJson(f.read(2 * 1024 * 1024)).object();
    if (raiz.value("programa").toString() != u("DuctusArq 2D")) {
        QMessageBox::warning(parent, u("Plantilla de oficina"), u("Ese archivo no es una plantilla de DuctusArq 2D."));
        return;
    }
    const QString antes = doc->getCurrentLayer();
    const QStringList existentes = doc->getAllLayer();
    int nuevas = 0, cambiadas = 0;
    for (const QJsonValue &v : raiz.value("capas").toArray()) {
        const QJsonObject o = v.toObject();
        const QString n = o.value("nombre").toString().left(120);
        if (n.isEmpty()) continue;
        const int grosor = o.value("grosor").toInt(), tipo = o.value("tipo").toInt();
        if (grosor < -3 || grosor > 211 || tipo < -2 || tipo > 19) continue;  // valores fuera de LibreCAD
        existentes.contains(n) ? ++cambiadas : ++nuevas;
        doc->setLayer(n);
        doc->setCurrentLayerProperties(o.value("color").toInt() & 0xFFFFFF, static_cast<DPI::LineWidth>(grosor),
                                       static_cast<DPI::LineType>(tipo));
    }
    doc->setLayer(antes);
    // Solo se aceptan los ajustes conocidos (nada de otras claves del archivo).
    const QJsonObject ajustes = raiz.value("ajustes").toObject();
    for (auto it = ajustes.begin(); it != ajustes.end(); ++it)
        if (it.key() == u("DuctusArq/Sistema") || it.key() == u("DuctusArq/EscalaMetrica")
            || it.key() == u("DuctusArq/EscalaImperial") || it.key().startsWith(u("DuctusArq/Rotulo/")))
            cfg.setValue(it.key(), it.value().toVariant());
    doc->updateView();
    QMessageBox::information(parent, u("Plantilla de oficina"),
                             u("Plantilla cargada: %1 capa(s) nueva(s) y %2 actualizada(s); medidas, escala y datos del "
                               "rótulo listos.").arg(nuevas).arg(cambiadas));
}
