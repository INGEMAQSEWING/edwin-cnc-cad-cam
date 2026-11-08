/*****************************************************************************/
/*  edwincnc.h - Herramientas de EDWIN CNC para corte por plasma             */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC                                             */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*  You should have received a copy of the GNU General Public License        */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*****************************************************************************/

#ifndef EDWINCNC_H
#define EDWINCNC_H

#include <QObject>
#include "qc_plugininterface.h"
#include "document_interface.h"

class EdwinCnc : public QObject, QC_PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(QC_PluginInterface)
    Q_PLUGIN_METADATA(IID LC_DocumentInterface_iid FILE "edwincnc.json")

public:
    PluginCapabilities getCapabilities() const Q_DECL_OVERRIDE;
    QString name() const Q_DECL_OVERRIDE;
    void execComm(Document_Interface *doc, QWidget *parent, QString cmd) Q_DECL_OVERRIDE;

private:
    void revisarParaCorte(Document_Interface *doc, QWidget *parent);
    void generarCodigoG(Document_Interface *doc, QWidget *parent);  // generador.cpp
    void laminaDeReferencia(Document_Interface *doc, QWidget *parent);
    void crearCapas(Document_Interface *doc, QWidget *parent, bool avisar);
    void borrarRevision(Document_Interface *doc);
    void matriz(Document_Interface *doc, QWidget *parent);    // herramientas.cpp
    void arandela(Document_Interface *doc, QWidget *parent);  // herramientas.cpp
};

#endif // EDWINCNC_H
