# Arma los tutoriales en PDF de DuctusArq 2D (Colombia y Miami) con las figuras que calcula
# generar_figuras.cpp (el mismo cálculo del programa).
#   py -3.10 -I armar_pdf.py <carpeta_generador> <carpeta_salida> <carpeta_referencias>
import math
import os
import sys

import pymupdf

GEN, SALIDA, REF = sys.argv[1], sys.argv[2], sys.argv[3]

COLOR = {
    "A-MUROS": (0, 0, 0), "A-COLUMNAS": (0.15, 0.15, 0.15), "A-EJES": (0.78, 0, 0),
    "A-PUERTAS": (0.75, 0.5, 0), "A-VENTANAS": (0, 0.55, 0.75), "A-SIMBOLOS": (0.45, 0, 0.62),
    "A-TEXTOS": (0, 0, 0.55), "A-COTAS": (0, 0.5, 0),
}
GROSOR = {"A-MUROS": 1.1, "A-COLUMNAS": 1.0, "A-EJES": 0.35, "A-PUERTAS": 0.55, "A-VENTANAS": 0.55,
          "A-SIMBOLOS": 0.45, "A-TEXTOS": 0.5, "A-COTAS": 0.35}
AZUL = (0.06, 0.2, 0.42)
AMARILLO = (0.95, 0.72, 0.02)
GRIS = (0.35, 0.35, 0.35)


def leer(nombre):
    figs, datos = {}, {"AREA": [], "VANO": [], "CASILLA": []}
    actual = None
    with open(os.path.join(GEN, nombre), encoding="utf-8") as f:
        for linea in f:
            linea = linea.rstrip("\n")
            if not linea:
                continue
            tipo, _, resto = linea.partition(" ")
            if tipo == "FIG":
                actual = figs.setdefault(resto, [])
            elif tipo in ("L", "A", "C", "S", "T", "D"):
                actual.append((tipo, resto))
            elif tipo in datos:
                datos[tipo].append(resto)
            else:
                datos[tipo] = resto
    return figs, datos


def caja(prims):
    xs, ys = [], []
    for t, r in prims:
        p = r.split(" ")
        if t == "L":
            xs += [float(p[1]), float(p[3])]; ys += [float(p[2]), float(p[4])]
        elif t in ("C", "A"):
            c = (float(p[1]), float(p[2])); rad = float(p[3])
            xs += [c[0] - rad, c[0] + rad]; ys += [c[1] - rad, c[1] + rad]
        elif t == "D":
            xs += [float(p[0]), float(p[2]), float(p[4])]; ys += [float(p[1]), float(p[3]), float(p[5])]
    return min(xs), min(ys), max(xs), max(ys)


def dibujar(page, prims, rect, extra_caja=None):
    x0, y0, x1, y1 = caja(prims)
    if extra_caja:
        x0, y0, x1, y1 = min(x0, extra_caja[0]), min(y0, extra_caja[1]), max(x1, extra_caja[2]), max(y1, extra_caja[3])
    esc = min(rect.width / (x1 - x0), rect.height / (y1 - y0))
    ox = rect.x0 + (rect.width - (x1 - x0) * esc) / 2
    oy = rect.y0 + (rect.height - (y1 - y0) * esc) / 2

    def T(x, y):
        return pymupdf.Point(ox + (x - x0) * esc, oy + (y1 - y) * esc)

    sh = page.new_shape()

    def linea(a, b, color, ancho, guiones=None):
        sh.draw_line(a, b)
        sh.finish(color=color, width=ancho, dashes=guiones)

    for t, r in prims:
        p = r.split(" ")
        if t == "L":
            capa = p[0]
            guiones = "[4 1.5 1 1.5] 0" if capa == "A-EJES" else None
            linea(T(float(p[1]), float(p[2])), T(float(p[3]), float(p[4])), COLOR.get(capa, (0, 0, 0)),
                  GROSOR.get(capa, 0.4), guiones)
        elif t == "S":
            a, b = T(float(p[1]), float(p[2])), T(float(p[3]), float(p[4]))
            sh.draw_rect(pymupdf.Rect(min(a.x, b.x), min(a.y, b.y), max(a.x, b.x), max(a.y, b.y)))
            sh.finish(color=None, fill=COLOR["A-COLUMNAS"])
        elif t == "C":
            sh.draw_circle(T(float(p[1]), float(p[2])), float(p[3]) * esc)
            sh.finish(color=COLOR.get(p[0], (0, 0, 0)), width=GROSOR.get(p[0], 0.4), fill=(1, 1, 1))
        elif t == "A":
            cx, cy, rad, a1, a2 = map(float, p[1:6])
            barrido = (a2 - a1) % (2 * math.pi)
            pts = [T(cx + rad * math.cos(a1 + barrido * k / 24), cy + rad * math.sin(a1 + barrido * k / 24)) for k in range(25)]
            sh.draw_polyline(pts)
            sh.finish(color=COLOR[p[0]], width=0.35, dashes="[2 1] 0")
        elif t == "D":
            ax, ay, bx, by, lx, ly = map(float, p[:6])
            texto = " ".join(p[6:])
            horizontal = abs(by - ay) < abs(bx - ax)
            if horizontal:
                a2, b2 = (ax, ly), (bx, ly)
            else:
                a2, b2 = (lx, ay), (lx, by)
            verde = COLOR["A-COTAS"]
            linea(T(ax, ay), T(*a2), verde, 0.25)
            linea(T(bx, by), T(*b2), verde, 0.25)
            pa, pb = T(*a2), T(*b2)
            linea(pa, pb, verde, 0.35)
            for q in (pa, pb):
                linea(pymupdf.Point(q.x - 2, q.y + 2), pymupdf.Point(q.x + 2, q.y - 2), verde, 0.7)
            m = pymupdf.Point((pa.x + pb.x) / 2, (pa.y + pb.y) / 2)
            fs = 5.5
            ancho_txt = pymupdf.get_text_length(texto, fontname="helv", fontsize=fs)
            if horizontal:
                page.insert_text(pymupdf.Point(m.x - ancho_txt / 2, m.y - 1.5), texto, fontsize=fs, color=verde)
            else:
                page.insert_text(pymupdf.Point(m.x - 1.5, m.y + ancho_txt / 2), texto, fontsize=fs, color=verde, rotate=90)
    sh.commit()
    # Textos al final, encima de las líneas
    for t, r in prims:
        if t != "T":
            continue
        p = r.split(" ")
        capa, x, y, h, al = p[0], float(p[1]), float(p[2]), float(p[3]), p[4]
        texto = " ".join(p[5:])
        fs = max(4.0, h * esc * 1.15)
        w = pymupdf.get_text_length(texto, fontname="helv", fontsize=fs)
        q = T(x, y)
        dy = {"m": fs * 0.35, "b": 0, "t": fs * 0.8}[al[1]]
        page.insert_text(pymupdf.Point(q.x - w / 2, q.y + dy), texto, fontsize=fs, color=COLOR.get(capa, (0, 0, 0)))
    return esc


def tabla(page, x, y, anchos, filas, titulo, fs=7.5):
    alto = 13
    total = sum(anchos)
    page.draw_rect(pymupdf.Rect(x, y, x + total, y + 15), color=AZUL, fill=AZUL)
    page.insert_text(pymupdf.Point(x + 4, y + 10.5), titulo, fontsize=8.5, color=(1, 1, 1), fontname="hebo")
    yy = y + 15
    for i, fila in enumerate(filas):
        fondo = (0.88, 0.92, 0.97) if i == 0 else ((0.97, 0.97, 0.97) if i % 2 else (1, 1, 1))
        page.draw_rect(pymupdf.Rect(x, yy, x + total, yy + alto), color=(0.7, 0.7, 0.7), fill=fondo, width=0.4)
        xx = x
        for c, w in enumerate(anchos):
            t = fila[c] if c < len(fila) else ""
            f = fs
            while f > 4 and pymupdf.get_text_length(t, fontname="helv", fontsize=f) > w - 4:
                f -= 0.25
            page.insert_text(pymupdf.Point(xx + 2, yy + 9), t, fontsize=f, fontname="hebo" if i == 0 else "helv")
            xx += w
        yy += alto
    return yy


class Doc:
    def __init__(self, ancho, alto, marca):
        self.doc = pymupdf.open()
        self.W, self.H, self.marca = ancho, alto, marca
        self.n = 0

    def pagina(self, titulo=None):
        self.page = self.doc.new_page(width=self.W, height=self.H)
        self.n += 1
        p = self.page
        p.draw_rect(pymupdf.Rect(0, 0, self.W, 6), color=AMARILLO, fill=AMARILLO)
        if titulo:
            p.insert_text(pymupdf.Point(40, 48), titulo, fontsize=17, fontname="hebo", color=AZUL)
            p.draw_line(pymupdf.Point(40, 56), pymupdf.Point(self.W - 40, 56), color=AMARILLO, width=1.5)
        p.insert_text(pymupdf.Point(40, self.H - 22), f"DuctusArq 2D  -  {self.marca}", fontsize=7.5, color=GRIS)
        p.insert_text(pymupdf.Point(self.W - 60, self.H - 22), f"Pág. {self.n}", fontsize=7.5, color=GRIS)
        self.y = 72
        return p

    def parrafo(self, texto, fs=9.5, negrita=False, color=(0.1, 0.1, 0.1), sangria=0, alto=None):
        r = pymupdf.Rect(40 + sangria, self.y, self.W - 40, self.y + (alto or 400))
        sobra = self.page.insert_textbox(r, texto, fontsize=fs, fontname="hebo" if negrita else "helv",
                                         color=color, lineheight=1.25)
        usado = (alto or 400) - sobra if sobra >= 0 else (alto or 400)
        self.y += usado + 4
        return usado

    def pasos(self, items, fs=9.5):
        for i, t in enumerate(items, 1):
            self.page.insert_text(pymupdf.Point(42, self.y + fs), f"{i}.", fontsize=fs, fontname="hebo", color=AZUL)
            self.parrafo(t, fs=fs, sangria=14)

    def comando(self, cmd, que):
        p = self.page
        w = pymupdf.get_text_length(cmd, fontname="cobo", fontsize=10) + 12
        p.draw_rect(pymupdf.Rect(40, self.y, 40 + w, self.y + 16), color=(0.1, 0.1, 0.1), fill=(0.1, 0.1, 0.1))
        p.insert_text(pymupdf.Point(46, self.y + 11.5), cmd, fontsize=10, fontname="cobo", color=AMARILLO)
        p.insert_text(pymupdf.Point(50 + w, self.y + 11.5), que, fontsize=9.5, color=(0.1, 0.1, 0.1))
        self.y += 22

    def figura(self, prims, alto, pie):
        r = pymupdf.Rect(40, self.y, self.W - 40, self.y + alto)
        self.page.draw_rect(r, color=(0.8, 0.8, 0.8), width=0.5)
        dibujar(self.page, prims, pymupdf.Rect(r.x0 + 8, r.y0 + 8, r.x1 - 8, r.y1 - 8))
        self.y += alto + 4
        self.parrafo(pie, fs=8, color=GRIS)

    def imagen(self, ruta, alto, pie=None):
        r = pymupdf.Rect(40, self.y, self.W - 40, self.y + alto)
        self.page.insert_image(r, filename=ruta, keep_proportion=True)
        self.y += alto + 4
        if pie:
            self.parrafo(pie, fs=8, color=GRIS)

    def guardar(self, ruta):
        self.doc.set_metadata({"title": os.path.basename(ruta), "author": "EDWIN CNC / OpenDuctus",
                               "subject": "Tutorial DuctusArq 2D"})
        self.doc.save(ruta, deflate=True, garbage=3)


def hoja_completa(page, rect, figs, datos, miami, valores):
    """Hoja con marco, plano y rótulo (dibujo esquemático a escala de la hoja)."""
    if miami:
        hw, hh = 914.4, 609.6   # ARCH D horizontal, mm
        mi, mo = 0.75 * 25.4, 0.4 * 25.4
    else:
        hw, hh = 297.0, 420.0   # A3 vertical
        mi, mo = 20.0, 10.0
    k = min(rect.width / hw, rect.height / hh)
    ox = rect.x0 + (rect.width - hw * k) / 2
    oy = rect.y0 + (rect.height - hh * k) / 2

    def P(x, y):  # mm de la hoja (origen abajo a la izquierda)
        return pymupdf.Point(ox + x * k, oy + (hh - y) * k)

    def R(x1, y1, x2, y2):
        a, b = P(x1, y2), P(x2, y1)
        return pymupdf.Rect(a.x, a.y, b.x, b.y)

    page.draw_rect(R(0, 0, hw, hh), color=(0.2, 0.2, 0.2), width=0.6, fill=(1, 1, 1))
    page.draw_rect(R(mi, mo, hw - mo, hh - mo), color=(0, 0, 0), width=0.9)
    rw, rh = map(float, datos["ROTULO"].split(" "))
    rx, ry = hw - mo - rw, mo
    for c in datos["CASILLA"]:
        p = c.split(" ", 5)
        x, y, w, h, alto = map(float, p[:5])
        etiqueta, clave = p[5].rsplit("|", 1)
        page.draw_rect(R(rx + x, ry + y, rx + x + w, ry + y + h), color=(0, 0, 0), width=0.4)
        fs_et = max(2.2, 1.6 * k * 1.2)
        page.insert_text(P(rx + x + 1, ry + y + h - 1 - 1.6), etiqueta[:60], fontsize=fs_et, color=GRIS)
        v = valores.get(clave, "")
        if v:
            fs = max(2.5, alto * k * 1.1)
            while fs > 2 and pymupdf.get_text_length(v, fontname="helv", fontsize=fs) > (w - 3) * k:
                fs -= 0.2
            tw = pymupdf.get_text_length(v, fontname="helv", fontsize=fs)
            q = P(rx + x + w / 2, ry + y + (h - 3) / 2)
            page.insert_text(pymupdf.Point(q.x - tw / 2, q.y + fs * 0.35), v, fontsize=fs, fontname="hebo" if clave == "plancha" else "helv")
    # Plano dentro del marco, al lado del rótulo
    if miami:
        zona = R(mi + 15, mo + 25, rx - 15, hh - mo - 15)
    else:
        zona = R(mi + 10, mo + rh + 15, hw - mo - 10, hh - mo - 10)
    dibujar(page, figs["areas"], zona)


def tutorial(miami):
    figs, d = leer("miami.txt" if miami else "colombia.txt")
    casa_w, casa_l = d["CASA"].split("|")
    if miami:
        doc = Doc(612, 792, "Tutorial Miami (Florida)")
        u = lambda a: f"{float(a) / 144:.1f} ft²"
    else:
        doc = Doc(595, 842, "Tutorial Colombia (curaduría)")
        u = lambda a: f"{float(a):.2f} m²"
    vanos = [v.split("|") for v in d["VANO"]]
    areas = [a.split("|") for a in d["AREA"]]
    util, constr, lote, io, ic = map(float, d["RESUMEN"].split("|"))

    # ---------------------------------------------------------------- Portada
    p = doc.pagina()
    p.draw_rect(pymupdf.Rect(0, 0, doc.W, 230), color=AZUL, fill=AZUL)
    p.draw_rect(pymupdf.Rect(0, 230, doc.W, 236), color=AMARILLO, fill=AMARILLO)
    p.insert_text(pymupdf.Point(40, 70), "DuctusArq 2D", fontsize=14, color=AMARILLO, fontname="hebo")
    if miami:
        p.insert_text(pymupdf.Point(40, 112), "Cómo hacer un plano", fontsize=28, color=(1, 1, 1), fontname="hebo")
        p.insert_text(pymupdf.Point(40, 146), "para permiso en Miami", fontsize=28, color=(1, 1, 1), fontname="hebo")
        p.insert_text(pymupdf.Point(40, 180), "Florida Building Code · Miami-Dade · pies y pulgadas", fontsize=11, color=(0.85, 0.9, 1))
    else:
        p.insert_text(pymupdf.Point(40, 112), "Cómo hacer un plano", fontsize=28, color=(1, 1, 1), fontname="hebo")
        p.insert_text(pymupdf.Point(40, 146), "para licencia en Colombia", fontsize=28, color=(1, 1, 1), fontname="hebo")
        p.insert_text(pymupdf.Point(40, 180), "Curaduría urbana · Res. 462/2017 y 1025/2021 · metros", fontsize=11, color=(0.85, 0.9, 1))
    p.insert_text(pymupdf.Point(40, 210), "Tutorial paso a paso · versión 1.0.28 · noviembre de 2026", fontsize=9, color=(0.85, 0.9, 1))
    doc.y = 256
    doc.parrafo("La muestra que vamos a dibujar", fs=13, negrita=True, color=AZUL)
    ref = os.path.join(REF, "ventas.jpeg" if miami else "planta.jpeg")
    doc.imagen(ref, 300, "Muestra de referencia entregada por el usuario. En el tutorial se dibuja una casa "
               "parecida, con medidas redondas, para aprender las herramientas.")
    doc.parrafo("Lo que se aprende: medidas y escala, casa rápida (muros, ejes, columnas y cotas), puertas y "
                "ventanas dentro del muro, áreas con un clic, cuadro de áreas, cuadro de puertas y ventanas, "
                "rótulo y la lista para radicar.", fs=10)
    doc.parrafo("Las figuras de este tutorial salen del mismo cálculo que usa DuctusArq 2D: lo que ve aquí es lo "
                "que el programa dibuja con esas medidas.", fs=8.5, color=GRIS)

    # ---------------------------------------------------------------- Paso 1 y 2
    p = doc.pagina("1. Medidas y escala")
    doc.parrafo("Cómo se escriben los comandos: ponga el ratón sobre el dibujo y escriba la palabra; aparece abajo, "
                "en la línea de comandos. Presione Enter. Si no ve la línea de comandos, presione Ctrl + M. "
                "También están los botones de la barra Arquitectura (arriba) y el menú DuctusArq.", fs=9.5)
    doc.comando("MEDIDAS", "Sistema de medidas y escala")
    if miami:
        doc.pasos(["Sistema: «Imperial: pies y pulgadas (EE. UU.)».",
                   "Escala: «1/4\" = 1'-0\" (plantas de casas)». Las cotas salen en pies y pulgadas (12'-6\").",
                   "Cree un dibujo nuevo (Archivo > Nuevo): ya trae las capas A-MUROS, A-PUERTAS, A-VENTANAS, "
                   "A-COTAS, A-TEXTOS, S-CIMENTACION, G-ROTULO, etc., y la unidad en pulgadas como el AutoCAD de EE. UU."])
        doc.parrafo("Cómo escribir medidas: 12'-6\", 12' 6\", 6\" o 12.5 (un número solo son pies).", fs=9, color=GRIS)
    else:
        doc.pasos(["Sistema: «Métrico: metros (Colombia y Latinoamérica)».",
                   "Escala: «1:50 (plantas de casas)». Textos, cotas y burbujas salen del tamaño correcto al imprimir.",
                   "Cree un dibujo nuevo (Archivo > Nuevo): ya trae las capas A-MUROS, A-PUERTAS, A-VENTANAS, "
                   "A-COTAS, A-TEXTOS, S-CIMENTACION, G-ROTULO, etc., en metros."])
        doc.parrafo("Cómo escribir medidas: 3.50 o 3,50 (metros). También 15cm.", fs=9, color=GRIS)
    doc.y += 6
    doc.parrafo("2. Casa rápida: muros, ejes, columnas y cotas", fs=17, negrita=True, color=AZUL)
    doc.comando("CASA", "Casa rápida (planta por medidas)")
    if miami:
        filas = [["Casilla", "Qué escribir"], ["Ancho (frente)", "38'"], ["Largo (fondo)", "48'"],
                 ["Muro exterior", "8\"  (bloque de concreto CMU de 8\")"], ["Muro interior", "4 1/2\""],
                 ["Espacios a lo ancho", "13'  (cuartos de la izquierda)"], ["Espacios a lo largo", "12' 11' 11'"],
                 ["Pasillo", "4'  (hall de 4'-0\" libre; mínimo 3'-0\")"],
                 ["Columnas", "0  (sin columnas: muros de bloque)"]]
    else:
        filas = [["Casilla", "Qué escribir"], ["Ancho (frente)", "7"], ["Largo (fondo)", "10"],
                 ["Muro exterior", "0.15"], ["Muro interior", "0.10"], ["Espacios a lo ancho", "3.00  (cuartos de la izquierda)"],
                 ["Espacios a lo largo", "3.00 3.50"], ["Pasillo", "1.00  (ancho libre; mínimo 0.90)"], ["Columnas", "0.25"]]
    doc.y = tabla(p, 40, doc.y, [140, doc.W - 80 - 140], filas, "Ventana «Casa rápida»") + 8
    doc.parrafo("Deje marcadas «Ejes con burbujas», «Cotas» y «Título». Dé clic en «Dibujar» y después un clic en "
                "la pantalla donde va la esquina de abajo a la izquierda de la casa.", fs=9.5)
    doc.parrafo("Con «Pasillo» la casa queda con un pasillo a lo largo: cada alcoba y el baño abren al pasillo y "
                "nunca hay que atravesar un cuarto para llegar a otro. Entre la sala y el pasillo queda un vano "
                "abierto (sin puerta).", fs=9, color=GRIS)
    doc.figura(figs["casa"], doc.H - doc.y - 70,
               f"Resultado: casa de {casa_w} x {casa_l}. Las uniones de muros en L, T y cruz quedan limpias; "
               "ejes 1, 2, 3 y A, B, C... con burbujas; cotas entre ejes y totales.")

    # ---------------------------------------------------------------- Paso 3
    p = doc.pagina("3. Puertas y ventanas dentro del muro")
    doc.comando("PUE", "Puerta / ventana en el muro   (también VEN o VANO)")
    doc.pasos(["En la ventana escoja el tipo (puerta de 1 hoja, 2 hojas, corrediza, ventana o vano), el ancho, el "
               "alto y, en ventanas, el antepecho." + (" En Miami escriba la aprobación NOA / FL# del producto." if miami else ""),
               "Clic exactamente sobre la cara del muro donde empieza el vano (F3 activa las referencias para que se pegue a la línea).",
               "Clic hacia donde sigue el muro. El programa encuentra solo la otra cara, corta el muro y cierra las jambas.",
               "En puertas: clic del lado hacia donde abre y cerca de la bisagra.",
               "La marca (P-1, V-1...) se pone sola; si la puerta es igual a otra ya hecha, repite la misma marca."], fs=9)
    filas = [["Marca", "Tipo", "Ancho", "Alto", "Antepecho", "Cant."] + (["NOA / FL#"] if miami else [])]
    for v in vanos:
        filas.append([v[0], v[1].title(), v[2], v[3], v[4], v[5]] + ([v[6]] if miami else []))
    anchos = [34, 140, 46, 46, 50, 30] + ([doc.W - 80 - 346] if miami else [])
    if not miami:
        anchos[1] = doc.W - 80 - 206
    doc.y = tabla(p, 40, doc.y + 2, anchos, filas, "Puertas y ventanas de este ejemplo") + 8
    doc.figura(figs["vanos"], doc.H - doc.y - 60, "Resultado: el muro queda cortado, con la hoja y el giro de cada puerta "
               "y la doble línea del vidrio en las ventanas.")

    # ---------------------------------------------------------------- Paso 4
    p = doc.pagina("4. Áreas con un clic adentro")
    doc.comando("ESP", "Área de un espacio (clic adentro)")
    doc.pasos(["Escriba el nombre del espacio (por ejemplo " + ("BEDROOM 2" if miami else "ALCOBA 1") + ") y el piso.",
               "Clic DENTRO del espacio: el área se mide hasta la cara de los muros. Las puertas cierran el espacio, "
               "así el área no se escapa al cuarto de al lado.",
               "El programa pregunta el nombre del siguiente espacio. Cancelar termina.",
               "Para el lote y el área construida del piso use «Qué es: Lote» o «Área construida» y marque las esquinas a mano.",
               "Nombre también el " + ("HALL y la escalera (STAIR)" if miami else "PASILLO") + ". Después escriba CIR "
               "(Revisar circulación): el programa revisa que a cada cuarto se llegue desde la entrada sin atravesar "
               "otro cuarto, y que el pasillo tenga el ancho mínimo."], fs=9)
    doc.figura(figs["areas"], doc.H - doc.y - 60, "Resultado: cada espacio con su nombre y su área; el dato queda guardado "
               "en la capa A-DATOS (no se imprime) para el cuadro de áreas.")

    # ---------------------------------------------------------------- Paso 5
    p = doc.pagina("5. Cuadro de áreas y cuadro de puertas y ventanas")
    doc.comando("CA", "Cuadro de áreas e índices")
    filas = [["Espacio", "Área"]] + [[a[0], u(a[1])] for a in areas]
    filas += [["ÁREA ÚTIL PISO", u(util)], ["ÁREA CONSTRUIDA", u(constr)], ["ÁREA DEL LOTE (ejemplo)", u(lote)],
              ["ÍNDICE DE OCUPACIÓN" + (" / LOT COVERAGE" if miami else ""), f"{io * 100:.1f} %"],
              ["ÍNDICE DE CONSTRUCCIÓN" + (" / FAR" if miami else ""), f"{ic:.2f}"]]
    doc.y = tabla(p, 40, doc.y, [doc.W - 80 - 110, 110], filas, "CUADRO DE ÁREAS / AREA TABLE") + 6
    if miami:
        doc.parrafo("Miami 21, zona T3 (casas): la ocupación del primer piso no puede pasar del 50 % del lote y en el segundo piso "
                    "del 30 %. El programa avisa si se pasa. Los retiros y alturas los fija la zona de cada lote.", fs=8.5, color=GRIS)
    else:
        doc.parrafo("Si el proyecto pasa de 2.000 m² construidos, el programa avisa: la Ley 1796 de 2016 exige revisión "
                    "independiente de diseños y supervisión técnica. Los índices máximos los fija el POT de cada municipio.",
                    fs=8.5, color=GRIS)
    doc.y += 6
    doc.comando("CV", "Cuadro de puertas y ventanas")
    filas = [["Marca", "Tipo", "Ancho", "Alto", "Antep.", "Cant.", "NOA / FL#" if miami else "Observaciones"]]
    for v in vanos:
        filas.append([v[0], v[1], v[2], v[3], v[4], v[5], v[6]])
    doc.y = tabla(p, 40, doc.y, [32, 150, 42, 42, 42, 30, doc.W - 80 - 338], filas,
                  "CUADRO DE PUERTAS Y VENTANAS / DOOR AND WINDOW SCHEDULE") + 6
    if miami:
        doc.parrafo("En Miami-Dade (zona de huracanes, HVHZ) cada ventana, puerta exterior, persiana y techo debe tener "
                    "aprobación de producto (NOA de Miami-Dade y/o FL#) y resistir impacto o llevar persiana aprobada. "
                    "Los números de este ejemplo son ficticios: use los del fabricante.", fs=8.5, color=GRIS)

    # ---------------------------------------------------------------- Paso 6
    p = doc.pagina("6. Rótulo (cajetín) y hoja")
    doc.comando("ROT", "Rótulo / cajetín por especialidad")
    if miami:
        valores = {"proyecto": "RESIDENCE - SECOND FLOOR", "direccion": "0000 NW 00 ST, MIAMI, FL",
                   "propietario": "OWNER NAME", "profesional": "ARCHITECT NAME, AR00000",
                   "empresa": "FIRM NAME", "contenido": "SECOND FLOOR PLAN", "disciplina": "ARCHITECTURAL",
                   "escala": d["ESCALA"], "fase": "PERMIT SET", "plancha": "A-102", "hoja": "3 de 12",
                   "fecha": "2026-11-26", "revision": "0", "elaboro": "DRAWN", "reviso": "CHECKED"}
        doc.pasos(["Hoja: «ARCH D (36 x 24 in)», horizontal. Códigos: «EE. UU. / NCS». Especialidad: Arquitectónico (A).",
                   "Formato: «Franja vertical a la derecha (EE. UU. / Miami)»: deja arriba el cuadro de 4.5\" x 4.5\" "
                   "en blanco para el sello de la Ciudad de Miami y el cuadro de 3\" x 3\" para el sello del profesional.",
                   "Número de plancha 102: queda A-102. Llene proyecto, dirección, dueño, profesional (licencia AR / PE) y fase.",
                   "Clic en la esquina de abajo a la izquierda de la hoja y mueva su planta dentro del marco."], fs=9)
    else:
        valores = {"proyecto": "VIVIENDA UNIFAMILIAR", "direccion": "CALLE 00 N 00-00, CÚCUTA",
                   "propietario": "NOMBRE DEL PROPIETARIO", "profesional": "ARQ. NOMBRE",
                   "empresa": "EMPRESA", "contenido": "PLANTA ARQUITECTÓNICA PRIMER PISO",
                   "disciplina": "ARQUITECTÓNICO / ARCHITECTURAL", "escala": d["ESCALA"], "fase": "LICENCIA DE CONSTRUCCIÓN",
                   "plancha": "ARQ-1", "hoja": "1 de 6", "fecha": "2026-11-26", "revision": "0",
                   "elaboro": "ELABORÓ", "reviso": "REVISÓ"}
        doc.pasos(["Hoja: «A3», vertical. Códigos: «Locales Colombia / México» (ARQ, EST, IE, HS...). Especialidad: Arquitectónico.",
                   "Formato: «Abajo a la derecha (ISO / Latinoamérica)» (180 x 60 mm).",
                   "Llene proyecto, dirección del predio, propietario, profesional y su matrícula (CPNAA para arquitectos; "
                   "COPNIA para ingenieros), contenido de la plancha y fase «Licencia de construcción».",
                   "Clic en la esquina de abajo a la izquierda de la hoja y mueva su planta dentro del marco."], fs=9)
    hoja_completa(p, pymupdf.Rect(40, doc.y, doc.W - 40, doc.H - 50), figs, d, miami, valores)

    # ---------------------------------------------------------------- Paso 7: amoblar y render
    p = doc.pagina("7. Amoblar y render para ventas")
    doc.comando("MOB", "Anexar mobiliario   (ELEC o APP: electrodomésticos · SAN: sanitarios)")
    doc.pasos(["Escoja la pieza de la lista (" + ("king, queen, full o twin; sofá, comedor, closet..." if miami else
               "cama sencilla, doble, queen o king; sofá, comedor, closet...") + "). La medida sale sola "
               + ("en pulgadas (tallas de EE. UU.)" if miami else "en metros (tallas de Colombia)") + " y se puede cambiar.",
               "Escoja el giro (0°, 90°, 180° o 270°) y haga clic donde va el CENTRO de la pieza; puede poner varias seguidas.",
               "Cada pieza guarda su altura (nevera 1.75 m, cama 0.55 m...) para el render."], fs=9)
    doc.comando("REN", "Render de la planta amoblada (3D con Blender, gratis)")
    doc.pasos(["Escoja la calidad (rápida, normal para redes o alta para imprimir) y el alto de los muros.",
               "Blender trabaja sin abrirse y en 1 a 5 minutos deja dos fotos en Documentos\\DuctusArq\\Render: "
               "desde arriba (para ventas) y en 3D (para redes sociales).",
               "Solo se levantan los muros hechos con «Casa rápida» o «Muro»; los huecos de puertas y ventanas salen solos."], fs=9)
    carpeta_r = os.path.join(GEN, "render_miami" if miami else "render_colombia")
    alto_img = (doc.H - doc.y - 80) / 2
    r1 = pymupdf.Rect(40, doc.y, doc.W / 2 - 6, doc.y + alto_img * 2)
    r2 = pymupdf.Rect(doc.W / 2 + 6, doc.y, doc.W - 40, doc.y + alto_img * 2)
    p.insert_image(r1, filename=os.path.join(carpeta_r, "planta_superior.png"), keep_proportion=True)
    p.insert_image(r2, filename=os.path.join(carpeta_r, "planta_3d.png"), keep_proportion=True)
    doc.y += alto_img * 2 + 6
    doc.parrafo("Izquierda: planta amoblada desde arriba (los muros se cortan a 1.30 m para que se vean los huecos de "
                "las puertas). Derecha: la misma planta en 3D. Fotos sacadas por DuctusArq 2D con este mismo ejemplo.",
                fs=8, color=GRIS)

    # ---------------------------------------------------------------- Paso 8
    p = doc.pagina("8. Imprimir y radicar")
    doc.pasos(["Archivo > Vista previa de impresión: revise que la escala sea la del rótulo (" + d["ESCALA"] + ").",
               "Archivo > Exportar como PDF" + (": en Miami cada hoja va en un PDF aparte, con nombre en MAYÚSCULAS "
                                                "(A-102.PDF), vectorial y sin capas." if miami else
                                                ": un PDF por plancha o todo el juego, según pida la curaduría."),
               "Guarde también el DXF (Archivo > Guardar como)."], fs=9.5)
    doc.y += 4
    if miami:
        lista = ["Portada con índice de planos, datos del código (FBC 8.ª edición 2023; la 9.ª entra el 31-dic-2026), "
                 "ocupación y tipo de construcción.",
                 "Datos de viento en los planos: velocidad de diseño (Miami-Dade 175 mph para casas, riesgo II), "
                 "exposición C, GCpi y tabla de presiones de componentes y revestimiento (C&C).",
                 "Zona de inundación: BFE y cota del piso más bajo (NAVD 88), con survey de menos de 1 año.",
                 "Site plan con norte, linderos y retiros (Miami 21 T3: 20' / 10' / 5' / 20').",
                 "Plantas, todas las fachadas, cortes, cimentación, estructura del techo y corte de muro con conectores.",
                 "Cuadro de puertas y ventanas con NOA / FL# y protección de impacto.",
                 "Detectores de humo y CO, ventana de escape en alcobas, formulario de energía R402 o R405.",
                 "Sello y firma DIGITAL del profesional en cada PDF (Miami-Dade no acepta firmas escaneadas).",
                 "Cuadro de 4.5\" x 4.5\" libre arriba a la derecha para el sello de la ciudad.",
                 "Hojas horizontales; una hoja por archivo PDF."]
        titulo = "Lista para radicar en la Ciudad de Miami / Miami-Dade"
    else:
        lista = ["Formulario Único Nacional (FUN) y copia de la matrícula de cada profesional.",
                 "Plano de localización, plantas, cortes (mínimo dos, con la vía y la pendiente real), fachadas y planta de cubiertas.",
                 "Cuadro de áreas en el plano (índices de ocupación y construcción según el POT).",
                 "Rótulo en cada plancha con dirección, escala, contenido, firma y matrícula del profesional.",
                 "Norte, linderos, ejes, niveles y usos; plantas, cortes y fachadas a la misma escala.",
                 "Planos estructurales y memoria firmados por ingeniero civil (NSR-10 A.1.5): materiales, cargas, grupo de uso, DES/DMO/DMI.",
                 "Estudio de suelos (NSR-10 Título H) cuando aplique.",
                 "Más de 2.000 m² o 5 o más viviendas para vender: revisión independiente y supervisión técnica (Ley 1796).",
                 "Entrega en papel, digital o electrónica, con firma a mano o digital (Res. 1025 de 2021).",
                 "Valla amarilla de 1.00 x 0.70 m al radicar (Decreto 1077 de 2015)."]
        titulo = "Lista para radicar en la curaduría"
    doc.parrafo(titulo, fs=13, negrita=True, color=AZUL)
    for t in lista:
        p.draw_rect(pymupdf.Rect(42, doc.y + 2, 51, doc.y + 11), color=AZUL, width=0.8)
        doc.parrafo(t, fs=9, sangria=16)
    doc.y += 6
    doc.parrafo("Importante: DuctusArq 2D ayuda a dibujar y a no olvidar nada, pero no reemplaza la revisión del "
                "profesional que firma ni la lista de chequeo vigente de la " + ("Ciudad de Miami o de Miami-Dade." if miami
                                                                                  else "curaduría de su ciudad."),
                fs=8.5, color=GRIS)
    doc.parrafo("Fuentes: C:\\EDWIN DEVELOPMENTS\\DuctusArq\\normativa\\" + ("01-eeuu-licencias-planos.md y 06-florida-miami.md"
                                                                            if miami else "03-colombia-licencias-planos.md y 04-colombia-normas-dibujo-por-especialidad.md"),
                fs=7.5, color=GRIS)
    nombre = "Tutorial DuctusArq 2D - Plano para Miami.pdf" if miami else "Tutorial DuctusArq 2D - Plano para Colombia.pdf"
    doc.guardar(os.path.join(SALIDA, nombre))
    return nombre


for m in (False, True):
    print(tutorial(m))
