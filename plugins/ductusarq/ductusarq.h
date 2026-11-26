/*****************************************************************************/
/*  ductusarq.h - Herramientas de arquitectura de DuctusArq 2D                */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC / OpenDuctus                                */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#ifndef DUCTUSARQ_H
#define DUCTUSARQ_H

#include <QObject>
#include "qc_plugininterface.h"
#include "document_interface.h"

// El menú y la barra «Arquitectura» solo aparecen en DuctusArq 2D (DuctusArq-2D.exe); en
// EDWIN CNC CAD-CAM y CAD2CAM este plugin no muestra nada.
class DuctusArq : public QObject, QC_PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(QC_PluginInterface)
    Q_PLUGIN_METADATA(IID LC_DocumentInterface_iid FILE "ductusarq.json")

public:
    PluginCapabilities getCapabilities() const Q_DECL_OVERRIDE;
    QString name() const Q_DECL_OVERRIDE;
    void execComm(Document_Interface *doc, QWidget *parent, QString cmd) Q_DECL_OVERRIDE;

private:
    // herramientas.cpp
    void casaRapida(Document_Interface *doc, QWidget *parent);
    void ejes(Document_Interface *doc, QWidget *parent);
    void columnas(Document_Interface *doc, QWidget *parent);
    void muro(Document_Interface *doc, QWidget *parent);
    void portico(Document_Interface *doc, QWidget *parent);
    void cimentacion(Document_Interface *doc, QWidget *parent);
    // espacios.cpp
    void puertaVentana(Document_Interface *doc, QWidget *parent);
    void cuadroVanos(Document_Interface *doc, QWidget *parent);
    void espacio(Document_Interface *doc, QWidget *parent);
    void cuadroAreas(Document_Interface *doc, QWidget *parent);
    // rotulo.cpp
    void rotulo(Document_Interface *doc, QWidget *parent);
    // ductusarq.cpp
    void sistemaDeMedidas(Document_Interface *doc, QWidget *parent);
};

#endif // DUCTUSARQ_H
