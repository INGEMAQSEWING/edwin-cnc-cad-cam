# Marca EDWIN CNC CAD/CAM con el logo oficial de EDWIN CNC (el mismo de CNC Control Phone).
# Uso: python marca2.py <logo-edwin-cnc.png> <carpeta-salida>
import sys
from PIL import Image, ImageDraw, ImageFont

LOGO = Image.open(sys.argv[1]).convert('RGBA')  # 2720x1000, fondo transparente
OUT = sys.argv[2]
F = 'C:/Windows/Fonts/'
def font(n, s): return ImageFont.truetype(F + n, s)
FONDO = (13, 16, 20); FONDO2 = (30, 36, 44); AMARILLO = (255, 242, 0); NEGRO = (18, 18, 18)
BLANCO = (240, 243, 246); GRIS = (150, 160, 172)

def logo_ancho(w):
    h = round(LOGO.height * w / LOGO.width)
    return LOGO.resize((w, h), Image.LANCZOS)

# Brazo robot del logo (pictograma), recortado del PNG original.
BRAZO = LOGO.crop((round(1600 * 1.36), round(110 * 1.36), round(1870 * 1.36), round(410 * 1.36)))

def icono(S):
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = S // 6
    d.rounded_rectangle([0, 0, S - 1, S - 1], r, fill=NEGRO)
    m = max(2, S // 22)
    d.rounded_rectangle([m, m, S - 1 - m, S - 1 - m], r - m, fill=AMARILLO)
    m2 = m * 2
    d.rounded_rectangle([m2, m2, S - 1 - m2, S - 1 - m2], r - m2, outline=NEGRO, width=max(1, S // 40))
    # brazo robot centrado arriba
    bw = int(S * 0.5)
    bh = round(BRAZO.height * bw / BRAZO.width)
    b = BRAZO.resize((bw, bh), Image.LANCZOS)
    im.alpha_composite(b, ((S - bw) // 2 + S // 30, int(S * 0.14)))
    # "CNC" abajo
    if S >= 48:
        f = font('ariblk.ttf', int(S * 0.2))
        tw = d.textlength('CNC', font=f)
        d.text(((S - tw) / 2, S * 0.68), 'CNC', font=f, fill=NEGRO)
    return im

def degradado(W, H):
    im = Image.new('RGBA', (W, H), FONDO)
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / H
        c = tuple(int(FONDO2[i] * (1 - t) + FONDO[i] * t) for i in range(3))
        d.line([(0, y), (W, y)], fill=c)
    return im

def splash(W=544, H=338):
    im = degradado(W, H)
    d = ImageDraw.Draw(im)
    for x in range(0, W, 24): d.line([(x, 0), (x, H)], fill=(36, 43, 52))
    for y in range(0, H, 24): d.line([(0, y), (W, y)], fill=(36, 43, 52))
    lg = logo_ancho(400)
    im.alpha_composite(lg, ((W - lg.width) // 2, 24))
    y = 24 + lg.height + 10
    f = font('ariblk.ttf', 34)
    tw = d.textlength('CAD/CAM', font=f)
    d.text(((W - tw) / 2, y), 'CAD/CAM', font=f, fill=AMARILLO)
    y += 50
    f2 = font('arialbd.ttf', 18)
    t2 = 'Dibujo 2D para corte CNC por plasma  ·  Versión 1.0'
    d.text(((W - d.textlength(t2, font=f2)) / 2, y), t2, font=f2, fill=BLANCO)
    d.text((24, H - 30), 'Basado en LibreCAD  ·  Software libre GPLv2', font=font('arial.ttf', 13), fill=GRIS)
    return im

def franja(W=73, H=335):
    im = degradado(W, H)
    lg = logo_ancho(round((W - 10) * LOGO.width / LOGO.height)).rotate(90, expand=True)
    im.alpha_composite(lg, ((W - lg.width) // 2, 20))
    return im

ic = icono(512)
ic.resize((128, 128), Image.LANCZOS).save(OUT + '/librecad.png')
tamanos = [16, 24, 32, 48, 64, 128, 256]
capas = [icono(s) if s >= 48 else icono(256).resize((s, s), Image.LANCZOS) for s in tamanos]
capas[-1].save(OUT + '/librecad.ico', sizes=[(s, s) for s in tamanos], append_images=capas[:-1])
ic.save(OUT + '/icono-512.png')
splash().convert('RGB').save(OUT + '/splash_librecad.png')
franja().convert('RGB').save(OUT + '/intro_librecad.png')
print('ok')
