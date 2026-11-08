/*****************************************************************************/
/*  edwin_variante.h - Variante EDWIN CNC CAD2CAM (estilo AutoCAD 2010)      */
/*                                                                           */
/*  Copyright (C) 2026 EDWIN CNC                                             */
/*                                                                           */
/*  This library is free software, licensed under the terms of the GNU       */
/*  General Public License as published by the Free Software Foundation,     */
/*  either version 2 of the License, or (at your option) any later version.  */
/*****************************************************************************/
//
// El mismo programa sirve para las dos variantes:
//   - EDWIN-CNC.exe          -> EDWIN CNC CAD-CAM (estilo normal)
//   - EDWIN-CNC-CAD2CAM.exe  -> EDWIN CNC CAD2CAM (barras Dibujo y Modificar como el
//                               AutoCAD 2010 clásico, comandos de AutoCAD en español,
//                               Enter/espacio repite, F3/F7/F8)
// La variante se decide por el nombre del .exe (main.cpp). Cada variante guarda su
// configuración aparte (nombre de aplicación distinto).

#ifndef EDWIN_VARIANTE_H
#define EDWIN_VARIANTE_H

#include <functional>

namespace edwin {

inline bool &cad2camRef() { static bool v = false; return v; }
inline bool esCad2cam() { return cad2camRef(); }

// Desplazar / Copiar sin ventana en CAD2CAM (como en AutoCAD).
//   moverModo: -1 = ventana normal de LibreCAD, 0 = desplazar, 1 = copiar.
//   copiarPendiente: la próxima herramienta Mover arranca como Copiar.
inline int &moverModo() { static int v = -1; return v; }
inline bool &copiarPendiente() { static bool v = false; return v; }

// Enter o espacio sin texto en el dibujo (lo conecta la ventana principal con
// QG_ActionHandler::command({})). enviandoTecla evita que las teclas que el propio
// programa manda a la selección vuelvan a entrar aquí.
inline std::function<void()> &enterVacio() { static std::function<void()> f; return f; }
inline bool &enviandoTecla() { static bool v = false; return v; }

} // namespace edwin

#endif // EDWIN_VARIANTE_H
