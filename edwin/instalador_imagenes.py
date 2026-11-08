# Imágenes del instalador de EDWIN CNC CAD/CAM (NSIS): lado de Bienvenida/Final (164x314) y encabezado (150x57),
# con el logo oficial amarillo y negro. Uso: py -3.10 edwin/instalador_imagenes.py
from PIL import Image, ImageDraw, ImageFont
LOGO = Image.open('edwin/logo-edwin-cnc.png').convert('RGBA')
F = 'C:/Windows/Fonts/'
NEGRO = (18, 18, 18); AMARILLO = (255, 242, 0); BLANCO = (240, 243, 246); GRIS = (150, 160, 172)

def logo_ancho(w):
    return LOGO.resize((w, round(LOGO.height * w / LOGO.width)), Image.LANCZOS)

# Lado (Bienvenida y Final): fondo negro, franja amarilla, logo y textos.
lado = Image.new('RGB', (164, 314), NEGRO)
d = ImageDraw.Draw(lado)
d.rectangle([0, 0, 163, 6], fill=AMARILLO)
d.rectangle([0, 307, 163, 313], fill=AMARILLO)
l = logo_ancho(148)
lado.paste(l, (8, 70), l)
f1 = ImageFont.truetype(F + 'arialbd.ttf', 15)
f2 = ImageFont.truetype(F + 'arial.ttf', 11)
def centro(y, txt, f, color):
    w = d.textlength(txt, font=f)
    d.text(((164 - w) / 2, y), txt, font=f, fill=color)
centro(70 + l.height + 18, 'CAD/CAM', f1, AMARILLO)
centro(70 + l.height + 40, 'Dibujo y corte', f2, BLANCO)
centro(70 + l.height + 55, 'por plasma', f2, BLANCO)
centro(282, 'EDWIN CNC', f2, GRIS)
lado.save('edwin/instalador-lado.bmp')

# Encabezado de las demás páginas: fondo blanco (lo pide Windows) con el logo.
enc = Image.new('RGB', (150, 57), (255, 255, 255))
l2 = logo_ancho(140)
if l2.height > 51:
    l2 = LOGO.resize((round(LOGO.width * 51 / LOGO.height), 51), Image.LANCZOS)
fondo = Image.new('RGBA', l2.size, NEGRO + (255,))
fondo.alpha_composite(l2)
enc.paste(fondo.convert('RGB'), ((150 - l2.width) // 2, (57 - l2.height) // 2))
enc.save('edwin/instalador-encabezado.bmp')
print('ok', lado.size, enc.size)
