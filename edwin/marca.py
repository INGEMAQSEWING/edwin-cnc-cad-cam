# Logo, ícono, pantalla de inicio y franja de bienvenida de EDWIN CNC CAD/CAM.
import sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
OUT = sys.argv[1]
F = 'C:/Windows/Fonts/'
def font(n, s): return ImageFont.truetype(F + n, s)
FONDO = (24, 29, 36); FONDO2 = (36, 44, 54); NARANJA = (255, 122, 26); AMARILLO = (255, 200, 80); BLANCO = (240, 243, 246); GRIS = (150, 160, 172)

def icono(S):
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    r = S // 6
    d.rounded_rectangle([0, 0, S - 1, S - 1], r, fill=FONDO)
    # rejilla CAD tenue
    paso = S // 8
    for i in range(paso, S, paso):
        d.line([(i, S // 12), (i, S - S // 12)], fill=FONDO2, width=max(1, S // 128))
        d.line([(S // 12, i), (S - S // 12, i)], fill=FONDO2, width=max(1, S // 128))
    # "E" de placa cortada (3 barras + columna)
    x0, y0, w, h = S * 0.22, S * 0.2, S * 0.5, S * 0.6
    g = h * 0.2
    d.rectangle([x0, y0, x0 + g, y0 + h], fill=BLANCO)
    for k, largo in ((0, w), (0.4, w * 0.8), (0.8, w)):
        yy = y0 + h * k
        d.rectangle([x0, yy, x0 + largo, yy + g], fill=BLANCO)
    # corte de plasma: línea naranja en curso con chispa al final
    ancho = max(2, S // 40)
    ys = y0 + h * 0.4 + g / 2
    d.line([(x0 + w * 0.8 + S * 0.04, ys), (S * 0.86, ys)], fill=NARANJA, width=ancho)
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    cx, cy, rr = S * 0.86, ys, S * 0.07
    gd.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=NARANJA + (200,))
    glow = glow.filter(ImageFilter.GaussianBlur(S / 40))
    im = Image.alpha_composite(im, glow)
    d = ImageDraw.Draw(im)
    rr = S * 0.03
    d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], fill=AMARILLO)
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
    for x in range(0, W, 24): d.line([(x, 0), (x, H)], fill=(44, 52, 63))
    for y in range(0, H, 24): d.line([(0, y), (W, y)], fill=(44, 52, 63))
    ic = icono(120); im.alpha_composite(ic, (36, 40))
    d.text((176, 46), 'EDWIN CNC', font=font('arialbd.ttf', 52), fill=BLANCO)
    d.text((178, 106), 'CAD/CAM', font=font('arialbd.ttf', 34), fill=NARANJA)
    d.text((180, 152), 'Versión 1.0', font=font('arial.ttf', 18), fill=GRIS)
    d.line([(36, 196), (W - 36, 196)], fill=NARANJA, width=2)
    d.text((36, 212), 'Dibujo 2D para corte CNC por plasma', font=font('arialbd.ttf', 20), fill=BLANCO)
    d.text((36, 242), 'Archivos DXF  ·  Mach3', font=font('arial.ttf', 16), fill=GRIS)
    d.text((36, H - 34), 'Basado en LibreCAD  ·  Software libre GPLv2', font=font('arial.ttf', 13), fill=GRIS)
    return im

def franja(W=73, H=335):
    im = degradado(W, H)
    ic = icono(57); im.alpha_composite(ic, (8, 10))
    t = Image.new('RGBA', (H - 90, W), (0, 0, 0, 0))
    td = ImageDraw.Draw(t)
    td.text((0, 8), 'EDWIN CNC', font=font('arialbd.ttf', 30), fill=BLANCO)
    td.text((0, 42), 'CAD/CAM', font=font('arialbd.ttf', 20), fill=NARANJA)
    t = t.rotate(90, expand=True)
    im.alpha_composite(t, (0, 80))
    return im

ic = icono(512)
ic.resize((128, 128), Image.LANCZOS).save(OUT + '/librecad.png')
ic.save(OUT + '/librecad.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
ic.save(OUT + '/logo-512.png')
splash().convert('RGB').save(OUT + '/splash_librecad.png')
franja().convert('RGB').save(OUT + '/intro_librecad.png')
print('ok')
