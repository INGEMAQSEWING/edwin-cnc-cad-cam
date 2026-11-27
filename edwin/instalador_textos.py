# Créditos del instalador (RTF con acentos escapados: lo muestra igual cualquier Windows).
# Uso: py -3.10 edwin/instalador_textos.py
P = [
    ('b', 'EDWIN CNC CAD/CAM'),
    ('', '© 2026 EDWIN CNC — Edwin Xabier Segura Valencia, Cúcuta, Colombia.'),
    ('', ''),
    ('b', 'Propietario de la marca y desarrollador'),
    ('', 'EDWIN CNC es el propietario de la marca EDWIN CNC y el autor de todas las funciones nuevas y mejoras que este programa agrega para el corte por plasma, y las sigue ampliando con cada versión:'),
    ('', '• Menú EDWIN CNC: revisar el dibujo antes de cortar (contornos abiertos, piezas sueltas), lámina de referencia y capas de trabajo.'),
    ('', '• Capas CORTE, MARCADO y REFERENCIA listas en cada dibujo nuevo.'),
    ('', '• Programa en español, en milímetros y con la imagen de EDWIN CNC.'),
    ('', '• En camino: generador de código G para Mach3 (colita de entrada y de salida, separación de la silueta, tiempo para perforar y más) y versión para Windows de 32 bits.'),
    ('', ''),
    ('b', 'Construido sobre software libre (código abierto)'),
    ('', 'La base de dibujo viene de proyectos libres, a los que damos el crédito que les corresponde:'),
    ('', '• LibreCAD: programa de dibujo 2D creado por la comunidad LibreCAD (librecad.org). Licencia GPL versión 2.'),
    ('', '• Qt: librería de ventanas de The Qt Company (licencia LGPL).'),
    ('', '• Boost, muParser y otras librerías de código abierto.'),
    ('', 'Gracias a sus autores y a toda la comunidad de código abierto.'),
    ('', ''),
    ('', 'Como lo pide la licencia GPL, EDWIN CNC CAD/CAM también es software libre: su código fuente, con las mejoras de EDWIN CNC, está en github.com/INGEMAQSEWING/edwin-cnc-cad-cam.'),
]

BARRA = chr(92)


def rtf(s):
    salida = []
    for c in s:
        if c in (BARRA, '{', '}'):
            salida.append(BARRA + c)
        elif ord(c) < 128:
            salida.append(c)
        else:
            n = ord(c)
            salida.append(f'{BARRA}u{n if n < 32768 else n - 65536}?')
    return ''.join(salida)


# Créditos de DuctusArq 2D (mismo programa, otro público: arquitectura, no plasma).
P_DUCTUSARQ = [
    ('b', 'DuctusArq 2D · Suite OpenDuctus'),
    ('', '© 2026 EDWIN CNC, Cúcuta, Colombia.'),
    ('', ''),
    ('b', 'Qué hace'),
    ('', 'Programa de diseño arquitectónico en 2D: planos para licencia o permiso, planos de ventas a color para folletos y redes, y render 3D de la planta amoblada.'),
    ('', '• Casa rápida con pasillo, ejes, columnas, muros, puertas y ventanas, áreas, cuadros de áreas y de puertas, mobiliario, rótulo por especialidad, pórticos y cimentación.'),
    ('', '• Revisión de circulación: avisa si a un cuarto solo se llega atravesando otro o si el pasillo queda angosto.'),
    ('', '• Normas técnicas de Colombia (NSR-10, ICONTEC, curadurías) y de EE. UU. (Florida Building Code, Miami-Dade), en metros o en pies y pulgadas.'),
    ('', '• En camino: normas de otros países de Latinoamérica, como COVENIN (Venezuela), NOM (México) e INN (Chile).'),
    ('', ''),
    ('b', 'Construido sobre software libre (código abierto)'),
    ('', 'La base de dibujo viene de proyectos libres, a los que damos el crédito que les corresponde:'),
    ('', '• LibreCAD: programa de dibujo 2D creado por la comunidad LibreCAD (librecad.org). Licencia GPL versión 2.'),
    ('', '• Qt: librería de ventanas de The Qt Company (licencia LGPL).'),
    ('', '• Blender (opcional, para el render 3D): programa libre de la Blender Foundation.'),
    ('', 'Gracias a sus autores y a toda la comunidad de código abierto.'),
    ('', ''),
    ('', 'Como lo pide la licencia GPL, DuctusArq 2D también es software libre: su código fuente está en github.com/INGEMAQSEWING/edwin-cnc-cad-cam.'),
]


def escribir(parrafos, ruta):
    lineas = []
    for tipo, texto in parrafos:
        t = rtf(texto)
        if tipo == 'b':
            t = '{' + BARRA + 'b ' + t + '}'
        lineas.append(t + BARRA + 'par\n')
    cabecera = '{' + BARRA + 'rtf1' + BARRA + 'ansi' + BARRA + 'deff0{' + BARRA + 'fonttbl{' + BARRA + 'f0 Segoe UI;}}' + BARRA + 'fs18\n'
    with open(ruta, 'w', encoding='ascii') as f:
        f.write(cabecera + ''.join(lineas) + '}')


escribir(P, 'scripts/postprocess-windows/creditos.rtf')
escribir(P_DUCTUSARQ, 'scripts/postprocess-windows/creditos-ductusarq.rtf')
print('ok')
