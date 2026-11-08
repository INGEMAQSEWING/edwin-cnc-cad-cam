## EDWIN CNC CAD/CAM 1.0.7 (prueba)

Programa de dibujo 2D y generador de código G para mesas CNC de plasma con Mach3, de **EDWIN CNC**.

### Novedades
- **Generar código G (Mach3)** — menú *EDWIN CNC → Generar código G (Mach3)…*. Reemplaza a SheetCAM para mesas X/Y sin eje Z:
  - Separación de la silueta (Kerf): la antorcha se corre la mitad del corte hacia el desperdicio, por fuera del contorno de afuera y por dentro de los huecos.
  - Colita de entrada (Lead in) y colita al final (Lead out): ninguna, recta o curva, siempre del lado del desperdicio.
  - Tiempo para perforar (Pierce delay), velocidad de corte, desaceleración al terminar y pausa antes de apagar.
  - Corta primero los huecos y después el contorno de afuera; entre piezas, la más cercana primero.
  - Dibuja el recorrido en la capa RECORRIDO (y los movimientos rápidos en RECORRIDO RAPIDO) para revisarlo antes de cortar.
  - Guarda el archivo .tap con el formato del post "Mach3 plasma no Z" (G21, G90 G91.1, M03/M05, G04 en segundos, arcos G02/G03 con I J).
  - Perfiles por material y espesor.
- **Instalador en español** con página de Bienvenida y Créditos de EDWIN CNC antes de la licencia.
- En Windows el programa ahora se registra como **EDWIN CNC CAD-CAM** (antes salía "LibreCAD (x64)").

### Antes de instalar
Desinstale la versión anterior (en *Configuración → Aplicaciones*, puede aparecer como "LibreCAD (x64)").

### Importante
Versión de prueba: corra el primer .tap **en el aire, con la antorcha apagada**, y revise las medidas de la primera pieza cortada.

---
EDWIN CNC CAD/CAM es software libre (GPL v2), basado en LibreCAD. © 2026 EDWIN CNC — funciones nuevas y mejoras para corte por plasma.
