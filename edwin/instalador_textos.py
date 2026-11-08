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


lineas = []
for tipo, texto in P:
    t = rtf(texto)
    if tipo == 'b':
        t = '{' + BARRA + 'b ' + t + '}'
    lineas.append(t + BARRA + 'par\n')
cabecera = '{' + BARRA + 'rtf1' + BARRA + 'ansi' + BARRA + 'deff0{' + BARRA + 'fonttbl{' + BARRA + 'f0 Segoe UI;}}' + BARRA + 'fs18\n'
with open('scripts/postprocess-windows/creditos.rtf', 'w', encoding='ascii') as f:
    f.write(cabecera + ''.join(lineas) + '}')
print('ok')
