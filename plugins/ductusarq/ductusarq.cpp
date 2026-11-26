/*****************************************************************************/
/*  ductusarq.cpp - Menú DuctusArq, sistema de medidas y capas                */
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

#include <QCoreApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QUrl>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

using da::u;

namespace {

const char *MENU = "DuctusArq";
// La barra «Arquitectura» y los comandos de teclado encuentran cada herramienta por el
// comienzo de estos textos ("Casa rápida", "Ejes", "Muro", ...): no cambiar ese comienzo.
const char *CMD_CASA = "Casa rápida (planta por medidas)...";
const char *CMD_EJES = "Ejes (rejilla con burbujas)...";
const char *CMD_COLUMNAS = "Columnas...";
const char *CMD_MURO = "Muro (por puntos, con espesor)...";
const char *CMD_PORTICO = "Pórtico (alzado: columnas, vigas y zapatas)...";
const char *CMD_CIMENTACION = "Cimentación (zapatas y vigas en planta)...";
const char *CMD_VANO = "Puerta / ventana en el muro...";
const char *CMD_CUADRO_VANOS = "Cuadro de puertas y ventanas";
const char *CMD_ESPACIO = "Área de un espacio (clic adentro)...";
const char *CMD_CUADRO_AREAS = "Cuadro de áreas e índices";
const char *CMD_MOBILIARIO = "Anexar mobiliario...";
const char *CMD_ELECTRO = "Anexar electrodomésticos (appliances)...";
const char *CMD_SANITARIOS = "Anexar aparatos sanitarios...";
const char *CMD_TUT_CO = "Tutorial: plano para Colombia (PDF)";
const char *CMD_TUT_MIA = "Tutorial: plano para Miami (PDF)";
const char *CMD_ROTULO = "Rótulo / cajetín por especialidad...";
const char *CMD_MEDIDAS = "Sistema de medidas y escala (metros o pies)...";
const char *CMD_CAPAS = "Capas de arquitectura e ingeniería";
const char *CMD_COMANDOS = "Comandos de DuctusArq (teclado)";
const char *CMD_ACERCA = "Acerca de DuctusArq 2D";

bool esDuctusArq() {
    return QCoreApplication::applicationName().contains("DuctusArq", Qt::CaseInsensitive);
}

struct EscalaOp { double valor; const char *texto; };

const EscalaOp ESCALAS_M[] = {
    {20, "1:20 (detalles)"}, {25, "1:25"}, {50, "1:50 (plantas de casas)"}, {75, "1:75"},
    {100, "1:100 (plantas de edificios)"}, {125, "1:125"}, {200, "1:200"}, {250, "1:250 (localización)"},
    {500, "1:500"}, {1000, "1:1000"},
};
const EscalaOp ESCALAS_I[] = {
    {16, "3/4\" = 1'-0\""}, {24, "1/2\" = 1'-0\" (detalles)"}, {32, "3/8\" = 1'-0\""},
    {48, "1/4\" = 1'-0\" (plantas de casas)"}, {64, "3/16\" = 1'-0\""}, {96, "1/8\" = 1'-0\" (edificios)"},
    {128, "3/32\" = 1'-0\""}, {192, "1/16\" = 1'-0\""}, {240, "1\" = 20' (sitio)"}, {480, "1\" = 40'"},
};

} // namespace

QString DuctusArq::name() const
{
    return QString("DuctusArq");
}

PluginCapabilities DuctusArq::getCapabilities() const
{
    PluginCapabilities pc;
    if (!esDuctusArq()) return pc;  // en EDWIN CNC CAD-CAM / CAD2CAM no se muestra
    for (const char *c : {CMD_CASA, CMD_EJES, CMD_COLUMNAS, CMD_MURO, CMD_VANO, CMD_ESPACIO, CMD_CUADRO_VANOS,
                          CMD_CUADRO_AREAS, CMD_MOBILIARIO, CMD_ELECTRO, CMD_SANITARIOS, CMD_PORTICO,
                          CMD_CIMENTACION, CMD_ROTULO, CMD_MEDIDAS, CMD_CAPAS, CMD_TUT_CO, CMD_TUT_MIA,
                          CMD_COMANDOS, CMD_ACERCA})
        pc.menuEntryPoints << PluginMenuLocation(MENU, u(c));
    return pc;
}

void DuctusArq::execComm(Document_Interface *doc, QWidget *parent, QString cmd)
{
    if (cmd == u(CMD_CASA)) casaRapida(doc, parent);
    else if (cmd == u(CMD_EJES)) ejes(doc, parent);
    else if (cmd == u(CMD_COLUMNAS)) columnas(doc, parent);
    else if (cmd == u(CMD_MURO)) muro(doc, parent);
    else if (cmd == u(CMD_PORTICO)) portico(doc, parent);
    else if (cmd == u(CMD_CIMENTACION)) cimentacion(doc, parent);
    else if (cmd == u(CMD_VANO)) puertaVentana(doc, parent);
    else if (cmd == u(CMD_CUADRO_VANOS)) cuadroVanos(doc, parent);
    else if (cmd == u(CMD_ESPACIO)) espacio(doc, parent);
    else if (cmd == u(CMD_CUADRO_AREAS)) cuadroAreas(doc, parent);
    else if (cmd == u(CMD_MOBILIARIO)) anexarMueble(doc, parent, 0);
    else if (cmd == u(CMD_ELECTRO)) anexarMueble(doc, parent, 1);
    else if (cmd == u(CMD_SANITARIOS)) anexarMueble(doc, parent, 2);
    else if (cmd == u(CMD_TUT_CO) || cmd == u(CMD_TUT_MIA)) {
        // Los tutoriales se instalan junto al programa, en la carpeta «tutoriales».
        const QString archivo = QCoreApplication::applicationDirPath() + u("/tutoriales/")
            + (cmd == u(CMD_TUT_CO) ? u("Tutorial DuctusArq 2D - Plano para Colombia.pdf")
                                    : u("Tutorial DuctusArq 2D - Plano para Miami.pdf"));
        if (!QFileInfo::exists(archivo) || !QDesktopServices::openUrl(QUrl::fromLocalFile(archivo)))
            QMessageBox::warning(parent, u("Tutorial"), u("No encontré el tutorial:\n%1").arg(archivo));
    }
    else if (cmd == u(CMD_ROTULO)) rotulo(doc, parent);
    else if (cmd == u(CMD_MEDIDAS)) sistemaDeMedidas(doc, parent);
    else if (cmd == u(CMD_CAPAS)) {
        const int n = da::crearCapasArq(doc);
        QMessageBox::information(parent, u("Capas"), n == 0
            ? u("El dibujo ya tiene todas las capas de arquitectura e ingeniería.")
            : u("Se crearon %1 capa(s): ejes, muros, columnas, puertas, ventanas, escaleras, cotas, "
                "textos, símbolos, mobiliario, cimentación, pórticos y rótulo.").arg(n));
    }
    else if (cmd == u(CMD_COMANDOS))
        QMessageBox::information(parent, u("Comandos de DuctusArq"), u(
            "Escriba el comando (no hace falta hacer clic en la línea de comandos) y presione "
            "<b>Enter</b> o la <b>barra espaciadora</b>.<br><br>"
            "<table cellspacing='0' cellpadding='3' border='1'>"
            "<tr><th>Herramienta</th><th>Escriba</th></tr>"
            "<tr><td>Casa rápida (planta por medidas)</td><td>CASA</td></tr>"
            "<tr><td>Ejes con burbujas</td><td>EJ · EJES</td></tr>"
            "<tr><td>Columnas</td><td>COL · COLUMNA</td></tr>"
            "<tr><td>Muro con espesor</td><td>MU · MURO</td></tr>"
            "<tr><td>Puerta o ventana en el muro</td><td>PUE · PUERTA · VEN · VENTANA · VANO</td></tr>"
            "<tr><td>Área de un espacio (clic adentro)</td><td>ESP · ESPACIO</td></tr>"
            "<tr><td>Cuadro de puertas y ventanas</td><td>CV · CUADROVANOS</td></tr>"
            "<tr><td>Cuadro de áreas e índices</td><td>CA · AREAS · CUADROAREAS</td></tr>"
            "<tr><td>Anexar mobiliario</td><td>MOB · MUEBLE</td></tr>"
            "<tr><td>Anexar electrodomésticos</td><td>ELEC · APP · APPLIANCE</td></tr>"
            "<tr><td>Anexar aparatos sanitarios</td><td>SAN · SANITARIO</td></tr>"
            "<tr><td>Pórtico (alzado)</td><td>POR · PORTICO</td></tr>"
            "<tr><td>Cimentación</td><td>CIM · ZAPATAS</td></tr>"
            "<tr><td>Rótulo / cajetín</td><td>ROT · ROTULO · CAJETIN</td></tr>"
            "<tr><td>Metros o pies y escala</td><td>MEDIDAS</td></tr>"
            "<tr><td>Capas de arquitectura</td><td>CAPASARQ</td></tr>"
            "</table><br>Los comandos de dibujo de AutoCAD en español también funcionan: "
            "L, PL, REC, C, A, B, CP, SI, E, D, GI, ES, RR, AL, MA, ...<br>"
            "F3 referencias a objetos · F7 rejilla · F8 orto."));
    else if (cmd == u(CMD_ACERCA))
        QMessageBox::about(parent, u("DuctusArq 2D"), u(
            "<b>DuctusArq 2D</b> — planos de arquitectura e ingeniería, de la suite OpenDuctus.<br><br>"
            "Sin licencias ni suscripciones. Código abierto (GPLv2), basado en LibreCAD.<br>"
            "Metros (Latinoamérica) o pies y pulgadas (EE. UU.).<br><br>"
            "Hecho por EDWIN CNC — ingemaqsewing.github.io/openductus"));
}

// ---------------------------------------------------------------------------
// Sistema de medidas y escala: se guarda para los dibujos nuevos y se aplica al dibujo abierto.
void DuctusArq::sistemaDeMedidas(Document_Interface *doc, QWidget *parent)
{
    const arq::Unidades actual = da::unidades();
    QDialog dlg(parent);
    dlg.setWindowTitle(u("Sistema de medidas y escala"));
    auto lay = new QVBoxLayout(&dlg);
    auto f = new QFormLayout;
    auto sistema = new QComboBox;
    sistema->addItem(u("Métrico: metros (Colombia y Latinoamérica)"));
    sistema->addItem(u("Imperial: pies y pulgadas (EE. UU.)"));
    sistema->setCurrentIndex(actual.sistema == arq::Sistema::Imperial ? 1 : 0);
    auto escala = new QComboBox;
    f->addRow(u("Sistema:"), sistema);
    f->addRow(u("Escala de impresión:"), escala);
    lay->addLayout(f);
    auto nota = new QLabel(u("La escala define el tamaño de textos, cotas y burbujas en el papel "
                             "(por ejemplo, texto de 2.5 mm a 1:50 mide 0.125 m en el dibujo).<br>"
                             "Cambiar el sistema NO convierte lo que ya está dibujado: úselo al empezar."));
    nota->setWordWrap(true);
    lay->addWidget(nota);
    auto llenar = [&](int s) {
        escala->clear();
        QSettings cfg;
        const bool imp = s == 1;
        const double guardada = cfg.value(imp ? "DuctusArq/EscalaImperial" : "DuctusArq/EscalaMetrica",
                                          imp ? 48.0 : 50.0).toDouble();
        int i = 0;
        if (imp) for (const auto &e : ESCALAS_I) { escala->addItem(u(e.texto), e.valor); if (e.valor == guardada) escala->setCurrentIndex(i); ++i; }
        else for (const auto &e : ESCALAS_M) { escala->addItem(u(e.texto), e.valor); if (e.valor == guardada) escala->setCurrentIndex(i); ++i; }
    };
    llenar(sistema->currentIndex());
    QObject::connect(sistema, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, llenar);
    auto botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(botones);
    if (dlg.exec() != QDialog::Accepted) return;

    const bool imperial = sistema->currentIndex() == 1;
    const double esc = escala->currentData().toDouble();
    QSettings cfg;
    cfg.setValue("DuctusArq/Sistema", imperial ? "imperial" : "metrico");
    cfg.setValue(imperial ? "DuctusArq/EscalaImperial" : "DuctusArq/EscalaMetrica", esc);

    // Dibujo abierto: unidades ($INSUNITS 6 = metros, 1 = pulgadas) y tamaños de cota.
    doc->addVariable("$INSUNITS", imperial ? 1 : 6, 70);
    doc->addVariable("$DIMSCALE", imperial ? esc / 25.4 : esc / 1000.0, 40);
    doc->addVariable("$DIMTXT", 2.5, 40);
    doc->addVariable("$DIMASZ", 2.5, 40);
    doc->addVariable("$DIMTSZ", 1.5, 40);
    doc->addVariable("$DIMEXE", 1.5, 40);
    doc->addVariable("$DIMEXO", 1.5, 40);
    doc->addVariable("$DIMGAP", 1.0, 40);
    doc->addVariable("$DIMLUNIT", imperial ? 4 : 2, 70);
    doc->addVariable("$DIMDEC", imperial ? 4 : 2, 70);
    doc->addVariable("$DIMZIN", imperial ? 0 : 8, 70);
    doc->addVariable("$LUNITS", imperial ? 4 : 2, 70);
    doc->addVariable("$LUPREC", imperial ? 4 : 2, 70);
    da::crearCapasArq(doc);
    doc->updateView();
}
