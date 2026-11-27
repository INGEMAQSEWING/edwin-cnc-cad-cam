# Imágenes de DuctusArq 2D: pantalla de carga (splash) e imagen del lado del instalador.
# Mismo estilo de EDWIN CNC (fondo oscuro con cuadrícula, logo amarillo), con textos de
# arquitectura en vez de plasma. Uso (desde la raíz del repositorio):
#   py -3.10 edwin/marca_ductusarq.py
from PIL import Image, ImageDraw, ImageFont

LOGO = Image.open('edwin/logo-edwin-cnc.png').convert('RGBA')
F = 'C:/Windows/Fonts/'
FONDO = (13, 16, 20); FONDO2 = (30, 36, 44); AMARILLO = (255, 242, 0); NEGRO = (18, 18, 18)
BLANCO = (240, 243, 246); GRIS = (150, 160, 172); CIAN = (120, 210, 235)


def font(n, s):
    return ImageFont.truetype(F + n, s)


def logo_ancho(w):
    return LOGO.resize((w, round(LOGO.height * w / LOGO.width)), Image.LANCZOS)


def degradado(W, H):
    im = Image.new('RGBA', (W, H), FONDO)
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / H
        d.line([(0, y), (W, y)], fill=tuple(int(FONDO2[i] * (1 - t) + FONDO[i] * t) for i in range(3)))
    return im


def splash(W=544, H=338):
    im = degradado(W, H)
    d = ImageDraw.Draw(im)
    for x in range(0, W, 24):
        d.line([(x, 0), (x, H)], fill=(36, 43, 52))
    for y in range(0, H, 24):
        d.line([(0, y), (W, y)], fill=(36, 43, 52))
    lg = logo_ancho(330)
    im.alpha_composite(lg, ((W - lg.width) // 2, 18))
    y = 18 + lg.height + 8

    def centro(txt, f, color, dy):
        nonlocal y
        d.text(((W - d.textlength(txt, font=f)) / 2, y), txt, font=f, fill=color)
        y += dy

    centro('DuctusArq 2D', font('ariblk.ttf', 32), AMARILLO, 44)
    centro('Diseño arquitectónico: planos, planos de ventas y render 3D', font('arialbd.ttf', 15), BLANCO, 24)
    centro('Normas de Colombia (NSR-10, ICONTEC) y EE. UU. (Florida, Miami-Dade)', font('arial.ttf', 13), CIAN, 20)
    centro('En camino: COVENIN, NOM, INN y más de Latinoamérica', font('arial.ttf', 12), GRIS, 0)
    d.text((24, H - 30), 'Suite OpenDuctus  ·  Basado en LibreCAD  ·  Software libre GPLv2', font=font('arial.ttf', 12),
           fill=GRIS)
    return im


def lado(W=164, H=314):
    im = Image.new('RGB', (W, H), NEGRO)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, W - 1, 6], fill=AMARILLO)
    d.rectangle([0, H - 7, W - 1, H - 1], fill=AMARILLO)
    lg = logo_ancho(148)
    im.paste(lg, (8, 60), lg)

    def centro(y, txt, f, color):
        d.text(((W - d.textlength(txt, font=f)) / 2, y), txt, font=f, fill=color)

    y = 60 + lg.height + 16
    centro(y, 'DuctusArq 2D', font('arialbd.ttf', 15), AMARILLO)
    centro(y + 24, 'Diseño arquitectónico', font('arial.ttf', 11), BLANCO)
    centro(y + 39, 'planos, ventas y render 3D', font('arial.ttf', 11), BLANCO)
    centro(y + 62, 'Normas de Colombia', font('arial.ttf', 10), CIAN)
    centro(y + 75, 'y EE. UU. (Florida)', font('arial.ttf', 10), CIAN)
    centro(H - 32, 'EDWIN CNC · OpenDuctus', font('arial.ttf', 10), GRIS)
    return im


splash().convert('RGB').save('librecad/res/main/splash_ductusarq.png')
lado().save('scripts/postprocess-windows/instalador-lado-ductusarq.bmp')
print('ok')
