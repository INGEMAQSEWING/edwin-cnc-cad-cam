# DuctusArq 2D - Render de la planta amoblada con Blender (gratis).
# Lo llama el programa así:
#   blender -b --factory-startup -P ductusarq_render.py -- plano.json carpeta_salida [calidad]
# plano.json lo escribe DuctusArq: muros (polígonos), vanos (puertas y ventanas con sus 4
# esquinas, alto y antepecho) y muebles (id, centro, giro, ancho, fondo, alto), en unidades
# del dibujo; "unidad_m" las pasa a metros (1 si son metros, 0.0254 si son pulgadas).
# Sale: planta_superior.png (vista desde arriba, para ventas) y planta_3d.png (vista en 3D).
#
# Copyright (C) 2026 EDWIN CNC / OpenDuctus - GPLv2 o posterior.
import json
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
PLANO = argv[0]
SALIDA = argv[1]
CALIDAD = argv[2] if len(argv) > 2 else "normal"
os.makedirs(SALIDA, exist_ok=True)

with open(PLANO, encoding="utf-8") as f:
    datos = json.load(f)
K = float(datos.get("unidad_m", 1.0))
ALTO_MURO = float(datos.get("alto_muro", 2.40 / K)) * K

bpy.ops.wm.read_factory_settings(use_empty=True)
escena = bpy.context.scene


# --------------------------------------------------------------------------- materiales
def material(nombre, color, rugosidad=0.6, metal=0.0, transmision=0.0, alfa=1.0):
    m = bpy.data.materials.get(nombre)
    if m:
        return m
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rugosidad
    b.inputs["Metallic"].default_value = metal
    for nombre_t in ("Transmission Weight", "Transmission"):
        if nombre_t in b.inputs:
            b.inputs[nombre_t].default_value = transmision
            break
    b.inputs["Alpha"].default_value = alfa
    return m


def material_madera(nombre, claro, oscuro, escala=6.0, tablas=False):
    """Madera con vetas finas (coordenadas en metros del objeto). tablas = piso de tablones."""
    m = bpy.data.materials.get(nombre)
    if m:
        return m
    m = bpy.data.materials.new(nombre)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes.get("Principled BSDF")
    b.inputs["Roughness"].default_value = 0.45
    coord = nt.nodes.new("ShaderNodeTexCoord")
    veta = nt.nodes.new("ShaderNodeTexWave")
    veta.wave_type = "BANDS"
    veta.bands_direction = "Y"
    veta.inputs["Scale"].default_value = escala
    veta.inputs["Distortion"].default_value = 3.0
    veta.inputs["Detail"].default_value = 4.0
    nt.links.new(coord.outputs["Object"], veta.inputs["Vector"])
    rampa = nt.nodes.new("ShaderNodeValToRGB")
    rampa.color_ramp.elements[0].color = (*oscuro, 1)
    rampa.color_ramp.elements[1].color = (*claro, 1)
    nt.links.new(veta.outputs["Fac"], rampa.inputs["Fac"])
    if tablas:
        # Tablones de 1.20 x 0.18 m con juntas finas.
        ladrillo = nt.nodes.new("ShaderNodeTexBrick")
        ladrillo.inputs["Scale"].default_value = 1.0
        ladrillo.inputs["Mortar Size"].default_value = 0.004
        ladrillo.inputs["Brick Width"].default_value = 1.20
        ladrillo.inputs["Row Height"].default_value = 0.18
        ladrillo.inputs["Color1"].default_value = (1, 1, 1, 1)
        ladrillo.inputs["Color2"].default_value = (0.88, 0.88, 0.88, 1)
        ladrillo.inputs["Mortar"].default_value = (0.35, 0.35, 0.35, 1)
        nt.links.new(coord.outputs["Object"], ladrillo.inputs["Vector"])
        mezcla = nt.nodes.new("ShaderNodeMix")
        mezcla.data_type = "RGBA"
        mezcla.blend_type = "MULTIPLY"
        mezcla.inputs["Factor"].default_value = 1.0
        nt.links.new(rampa.outputs["Color"], mezcla.inputs["A"])
        nt.links.new(ladrillo.outputs["Color"], mezcla.inputs["B"])
        nt.links.new(mezcla.outputs["Result"], b.inputs["Base Color"])
    else:
        nt.links.new(rampa.outputs["Color"], b.inputs["Base Color"])
    return m


M = {
    "muro": material("Muro", (0.16, 0.16, 0.17), 0.8),
    "piso": material_madera("Piso madera", (0.62, 0.46, 0.31), (0.50, 0.36, 0.23), 18.0, True),
    "exterior": material("Exterior", (0.93, 0.93, 0.92), 0.9),
    "vidrio": material("Vidrio", (0.75, 0.88, 0.95), 0.05, 0, 0.9, 0.35),
    "madera": material_madera("Madera", (0.62, 0.47, 0.32), (0.50, 0.37, 0.24), 30.0),
    "madera_osc": material("Madera oscura", (0.20, 0.14, 0.10), 0.5),
    "tela": material("Tela", (0.78, 0.74, 0.68), 0.9),
    "tela_gris": material("Tela gris", (0.42, 0.43, 0.45), 0.9),
    "blanco": material("Blanco", (0.92, 0.92, 0.90), 0.5),
    "sabana": material("Sábana", (0.95, 0.95, 0.93), 0.85),
    "cobija": material("Cobija", (0.55, 0.47, 0.40), 0.9),
    "ceramica": material("Cerámica", (0.97, 0.97, 0.97), 0.15),
    "acero": material("Acero", (0.75, 0.76, 0.78), 0.3, 1.0),
    "negro": material("Negro", (0.04, 0.04, 0.04), 0.4),
    "piedra": material("Piedra", (0.82, 0.80, 0.77), 0.3),
    "planta": material("Planta", (0.18, 0.38, 0.16), 0.8),
}


# --------------------------------------------------------------------------- geometría
def prisma(nombre, puntos, z0, z1, mat):
    """Prisma de un polígono (lista de (x, y) en metros) entre las alturas z0 y z1."""
    me = bpy.data.meshes.new(nombre)
    bm = bmesh.new()
    abajo = [bm.verts.new((x, y, z0)) for x, y in puntos]
    cara = bm.faces.new(abajo)
    if cara.normal.z > 0:
        cara.normal_flip()
    r = bmesh.ops.extrude_face_region(bm, geom=[cara])
    arriba = [v for v in r["geom"] if isinstance(v, bmesh.types.BMVert)]
    bmesh.ops.translate(bm, verts=arriba, vec=(0, 0, z1 - z0))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(nombre, me)
    escena.collection.objects.link(ob)
    ob.data.materials.append(mat)
    return ob


def caja(nombre, cx, cy, w, f, z0, h, mat, padre=None):
    ob = prisma(nombre, [(cx - w / 2, cy - f / 2), (cx + w / 2, cy - f / 2), (cx + w / 2, cy + f / 2),
                         (cx - w / 2, cy + f / 2)], z0, z0 + h, mat)
    if padre:
        ob.parent = padre
    return ob


def cilindro(nombre, cx, cy, r, z0, h, mat, padre=None, rx=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=32, radius=r, depth=h, location=(cx, cy, z0 + h / 2))
    ob = bpy.context.active_object
    ob.name = nombre
    if rx:
        ob.scale.x = rx / r
    ob.data.materials.append(mat)
    if padre:
        ob.parent = padre
    return ob


def unir(objetos, nombre):
    objetos = [o for o in objetos if o]
    if not objetos:
        return None
    bpy.ops.object.select_all(action="DESELECT")
    for o in objetos:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objetos[0]
    if len(objetos) > 1:
        bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = nombre
    return ob


# --------------------------------------------------------------------------- muros y vanos
muros = []
for i, poli in enumerate(datos.get("muros", [])):
    pts = [(x * K, y * K) for x, y in poli]
    if len(pts) >= 3:
        muros.append(prisma(f"Muro{i}", pts, 0, ALTO_MURO, M["muro"]))

cortes, vidrios = [], []
for i, v in enumerate(datos.get("vanos", [])):
    e = [Vector((x * K, y * K, 0)) for x, y in v["esquinas"]]
    if len(e) != 4:
        continue
    alto = float(v.get("alto", 2.1 / K)) * K
    ante = float(v.get("antepecho", 0)) * K
    es_ventana = "VENTANA" in v.get("tipo", "")
    z0 = ante if es_ventana else -0.05
    z1 = (ante + alto) if es_ventana else alto
    # Se agranda un poco a través del muro para que el corte lo atraviese limpio.
    n = (e[3] - e[0])
    n = n.normalized() * 0.03 if n.length > 0 else Vector((0, 0, 0))
    d = (e[1] - e[0])
    d = d.normalized() * 0.002 if d.length > 0 else Vector((0, 0, 0))
    q = [e[0] - n + d, e[1] - n - d, e[2] + n - d, e[3] + n + d]
    cortes.append(prisma(f"Corte{i}", [(p.x, p.y) for p in q], z0, z1, M["muro"]))
    if es_ventana:
        m0, m1 = (e[0] + e[3]) / 2, (e[1] + e[2]) / 2
        t = (e[3] - e[0]).normalized() * 0.006
        vidrios.append(prisma(f"Vidrio{i}", [((m0 - t).x, (m0 - t).y), ((m1 - t).x, (m1 - t).y),
                                             ((m1 + t).x, (m1 + t).y), ((m0 + t).x, (m0 + t).y)],
                              z0, z1, M["vidrio"]))
# Cada muro se corta por separado (cada uno es un sólido cerrado; juntos se enciman y el
# corte falla). Solo se cortan los muros que tocan algún vano.
def caja2d(ob):
    xs_ = [v.co.x for v in ob.data.vertices]
    ys_ = [v.co.y for v in ob.data.vertices]
    return min(xs_), min(ys_), max(xs_), max(ys_)


cajas_cortes = [(c, caja2d(c)) for c in cortes]
for m_ in muros:
    a = caja2d(m_)
    for c, b in cajas_cortes:
        if a[0] > b[2] or b[0] > a[2] or a[1] > b[3] or b[1] > a[3]:
            continue
        mod = m_.modifiers.new("Vano", "BOOLEAN")
        mod.operation = "DIFFERENCE"
        mod.solver = "EXACT"
        mod.object = c
        bpy.context.view_layer.objects.active = m_
        bpy.ops.object.modifier_apply(modifier=mod.name)
for c in cortes:
    bpy.data.objects.remove(c, do_unlink=True)

# Se funden todos los muros en una sola pieza (sin caras encimadas en las esquinas, que
# salen como manchas negras).
if len(muros) > 1:
    base_muro = muros[0]
    for otro in muros[1:]:
        mod = base_muro.modifiers.new("Union", "BOOLEAN")
        mod.operation = "UNION"
        mod.solver = "EXACT"
        mod.object = otro
        bpy.context.view_layer.objects.active = base_muro
        bpy.ops.object.modifier_apply(modifier=mod.name)
        bpy.data.objects.remove(otro, do_unlink=True)
    base_muro.name = "Muros"
    muros = [base_muro]

# Piso y terreno: del tamaño de los muros.
xs = [x * K for poli in datos.get("muros", []) for x, y in poli] or [0, 1]
ys = [y * K for poli in datos.get("muros", []) for x, y in poli] or [0, 1]
x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
ancho, largo = x1 - x0, y1 - y0
caja("Piso", cx, cy, ancho, largo, -0.02, 0.02, M["piso"])
caja("Terreno", cx, cy, ancho * 6, largo * 6, -0.05, 0.02, M["exterior"])


# --------------------------------------------------------------------------- muebles
def mueble(p):
    pid = p["id"]
    w, f, h = float(p["ancho"]) * K, float(p["fondo"]) * K, float(p["alto"]) * K
    raiz = bpy.data.objects.new(pid, None)
    escena.collection.objects.link(raiz)
    raiz.location = (float(p["x"]) * K, float(p["y"]) * K, 0)
    raiz.rotation_euler.z = math.radians(float(p.get("grados", 0)))
    B = lambda n, x, y, ww, ff, z, hh, m: caja(n, x, y, ww, ff, z, hh, m, raiz)
    C = lambda n, x, y, r, z, hh, m, rx=None: cilindro(n, x, y, r, z, hh, m, raiz, rx)
    if pid.startswith("cama"):
        B("base", 0, 0, w, f, 0, 0.30, M["madera_osc"])
        B("colchon", 0, -0.02, w - 0.04, f - 0.08, 0.30, 0.22, M["sabana"])
        B("cabecera", 0, f / 2 - 0.03, w + 0.04, 0.06, 0, 1.05, M["madera_osc"])
        B("cobija", 0, -f * 0.18, w - 0.02, f * 0.60, 0.50, 0.05, M["cobija"])
        n = 1 if pid == "cama_sencilla" else 2
        for i in range(n):
            x = 0 if n == 1 else (-w / 4 if i == 0 else w / 4)
            B(f"almohada{i}", x, f / 2 - 0.25, (w - 0.2) / n - 0.06, 0.32, 0.52, 0.12, M["blanco"])
    elif pid == "mesa_noche":
        B("cajon", 0, 0, w, f, 0, h, M["madera"])
        C("lampara", 0, 0, min(w, f) * 0.22, h, 0.35, M["tela"])
    elif pid in ("closet", "comoda", "mueble_tv"):
        B("cuerpo", 0, 0, w, f, 0, h, M["madera"])
        if pid == "mueble_tv":
            B("pantalla", 0, f / 2 - 0.05, w * 0.75, 0.04, h, 0.70, M["negro"])
    elif pid in ("sofa3", "sofa2", "poltrona"):
        tela = M["tela_gris"]
        B("asiento", 0, -0.05, w - 0.36, f - 0.2, 0, 0.44, tela)
        B("espaldar", 0, f / 2 - 0.1, w, 0.2, 0, h, tela)
        B("brazo_i", -w / 2 + 0.09, 0, 0.18, f, 0, 0.62, tela)
        B("brazo_d", w / 2 - 0.09, 0, 0.18, f, 0, 0.62, tela)
        n = 3 if pid == "sofa3" else (2 if pid == "sofa2" else 1)
        cw = (w - 0.36) / n
        for i in range(n):
            B(f"cojin{i}", -w / 2 + 0.18 + cw * (i + 0.5), -0.07, cw - 0.03, f - 0.28, 0.44, 0.10, M["tela"])
    elif pid == "mesa_centro":
        B("tapa", 0, 0, w, f, h - 0.04, 0.04, M["madera"])
        B("base", 0, 0, w * 0.8, f * 0.7, 0, h - 0.04, M["madera_osc"])
    elif pid.startswith("comedor") or pid == "escritorio":
        B("tapa", 0, 0, w, f, h - 0.04, 0.04, M["madera"])
        for sx in (-1, 1):
            for sy in (-1, 1):
                B("pata", sx * (w / 2 - 0.05), sy * (f / 2 - 0.05), 0.05, 0.05, 0, h - 0.04, M["madera_osc"])
        s = min(f * 0.55, 0.45)
        sillas = []
        if pid == "escritorio":
            sillas = [(0, -f / 2 - s * 0.2, -90)]
        else:
            xs_ = [0] if pid == "comedor4" else [-w / 4, w / 4]
            sillas = [(x, -f / 2 - s * 0.15, -90) for x in xs_] + [(x, f / 2 + s * 0.15, 90) for x in xs_]
            sillas += [(-w / 2 - s * 0.15, 0, 180), (w / 2 + s * 0.15, 0, 0)]
        for i, (x, y, a) in enumerate(sillas):
            ex, ey = math.cos(math.radians(a)), math.sin(math.radians(a))
            B(f"silla{i}", x, y, s, s, 0.42, 0.05, M["tela"])
            bw, bf = (0.05, s) if abs(ex) > 0.5 else (s, 0.05)
            B(f"espaldar{i}", x + ex * s * 0.45, y + ey * s * 0.45, bw, bf, 0.42, 0.45, M["madera_osc"])
    elif pid == "meson":
        B("mueble", 0, 0.02, w, f - 0.04, 0, h - 0.04, M["madera"])
        B("cubierta", 0, 0, w, f, h - 0.04, 0.04, M["piedra"])
    elif pid in ("nevera", "lavavajillas", "microondas", "lavadora", "secadora"):
        B("cuerpo", 0, 0, w, f, 0 if pid != "microondas" else 0.92, h, M["acero"] if pid in ("nevera", "microondas") else M["blanco"])
        if pid in ("lavadora", "secadora"):
            C("tapa", 0, -0.02, min(w, f) * 0.3, h, 0.01, M["negro"])
    elif pid == "estufa":
        B("cuerpo", 0, 0, w, f, 0, h - 0.02, M["acero"])
        B("vidrio", 0, 0, w, f, h - 0.02, 0.02, M["negro"])
        for sx in (-1, 1):
            for sy in (-1, 1):
                C("quemador", sx * w * 0.22, sy * f * 0.2, min(w, f) * 0.11, h, 0.015, M["acero"])
    elif pid in ("lavaplatos", "lavadero"):
        B("mueble", 0, 0, w, f, 0, h - 0.03, M["blanco"])
        B("poceta", 0, -0.02, w - 0.1, f - 0.15, h - 0.03, 0.03, M["acero"])
    elif pid == "calentador":
        C("tanque", 0, 0, min(w, f) / 2, 0, h, M["blanco"])
    elif pid == "aire":
        B("unidad", 0, 0, w, f, 2.05, h, M["blanco"])
    elif pid == "tv":
        B("pantalla", 0, 0, w, f, 0.9, h, M["negro"])
    elif pid == "sanitario":
        B("tanque", 0, f / 2 - 0.09, w, 0.18, 0.40, 0.35, M["ceramica"])
        C("taza", 0, -f * 0.12, w * 0.45, 0, 0.42, M["ceramica"], rx=w * 0.45)
        bpy.context.active_object.scale.y = (f * 0.38) / (w * 0.45)
    elif pid in ("lavamanos", "lavamanos_doble"):
        B("mueble", 0, 0, w, f, 0, h - 0.04, M["madera"])
        B("cubierta", 0, 0, w, f, h - 0.04, 0.04, M["piedra"])
        n = 2 if pid == "lavamanos_doble" else 1
        for i in range(n):
            x = 0 if n == 1 else (-w / 4 if i == 0 else w / 4)
            C(f"poceta{i}", x, -0.02, min(w / n, f) * 0.32, h, 0.012, M["ceramica"])
    elif pid == "ducha":
        B("plato", 0, 0, w, f, 0, 0.05, M["ceramica"])
        B("vidrio", 0, -f / 2 + 0.01, w, 0.01, 0.05, 1.9, M["vidrio"])
    elif pid == "tina":
        B("tina", 0, 0, w, f, 0, h, M["ceramica"])
        B("agua", 0, 0, w - 0.12, f - 0.12, h - 0.02, 0.025, M["vidrio"])
    else:
        B("caja", 0, 0, w, f, 0, h, M["madera"])


for p in datos.get("muebles", []):
    mueble(p)
for o in vidrios:
    pass

# --------------------------------------------------------------------------- luz y cámaras
mundo = bpy.data.worlds.new("Mundo")
mundo.use_nodes = True
mundo.node_tree.nodes["Background"].inputs["Color"].default_value = (0.95, 0.95, 0.95, 1)
mundo.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.9
escena.world = mundo
sol = bpy.data.lights.new("Sol", "SUN")
sol.energy = 3.2
sol.angle = math.radians(8)
ob_sol = bpy.data.objects.new("Sol", sol)
escena.collection.objects.link(ob_sol)
ob_sol.rotation_euler = (math.radians(28), math.radians(-18), math.radians(35))

escena.render.engine = "CYCLES"
escena.cycles.device = "CPU"
escena.cycles.samples = {"rapida": 24, "normal": 64, "alta": 160}.get(CALIDAD, 64)
escena.cycles.use_denoising = True
escena.view_settings.view_transform = "AgX" if "AgX" in [i.identifier for i in escena.view_settings.bl_rna.properties["view_transform"].enum_items] else "Filmic"
escena.render.film_transparent = False
lado_px = {"rapida": 1000, "normal": 1600, "alta": 2400}.get(CALIDAD, 1600)
margen = 1.12


def render(nombre, camara, rx, ry):
    escena.camera = camara
    escena.render.resolution_x, escena.render.resolution_y = rx, ry
    escena.render.filepath = os.path.join(SALIDA, nombre)
    bpy.ops.render.render(write_still=True)
    print("DUCTUSARQ-RENDER", escena.render.filepath)


# 1) En 3D (perspectiva desde una esquina, de arriba), para redes: muros blancos.
muro_blanco = material("Muro blanco", (0.90, 0.89, 0.87), 0.8)
for m_ in muros:
    m_.data.materials[0] = muro_blanco
cam3 = bpy.data.cameras.new("Perspectiva")
cam3.lens = 35
ob3 = bpy.data.objects.new("Cam3D", cam3)
escena.collection.objects.link(ob3)
dist = max(ancho, largo) * 1.35
ob3.location = (cx - dist * 0.55, cy - dist * 0.75, dist * 0.95)
direccion = Vector((cx, cy, 0.5)) - ob3.location
ob3.rotation_euler = direccion.to_track_quat("-Z", "Y").to_euler()
render("planta_3d.png", ob3, lado_px, int(lado_px * 0.75))

# 2) Desde arriba (ortográfica), como plano de ventas: todo se corta a 1.30 m de altura
# (así se ven los huecos de las puertas), muros oscuros y sol casi vertical.
CORTE = 1.30
for m_ in muros:
    m_.data.materials[0] = M["muro"]
# Muebles más altos que el corte (closets, nevera): se bajan al corte para que se vean llenos
# y no huecos; lo que queda entero por encima (aire acondicionado) no se ve.
nombres_muro = {m_.name for m_ in muros}
for ob in list(escena.objects):
    if ob.type != "MESH" or ob.name in nombres_muro or ob.name in ("Piso", "Terreno"):
        continue
    zs = [(ob.matrix_world @ v.co).z for v in ob.data.vertices]
    if not zs or max(zs) <= CORTE:
        continue
    if min(zs) >= CORTE - 0.02:
        ob.hide_render = True
        continue
    tope = CORTE - 0.02 - ob.matrix_world.translation.z
    for v in ob.data.vertices:
        if v.co.z > tope:
            v.co.z = tope
ob_sol.rotation_euler = (math.radians(12), math.radians(-8), math.radians(35))
cam = bpy.data.cameras.new("Arriba")
cam.type = "ORTHO"
cam.ortho_scale = max(ancho, largo) * margen
ob_cam = bpy.data.objects.new("CamArriba", cam)
escena.collection.objects.link(ob_cam)
ob_cam.location = (cx, cy, CORTE + 20)
cam.clip_start = 20.0
cam.clip_end = 40.0
if ancho >= largo:
    rx, ry = lado_px, max(200, int(lado_px * largo / ancho))
else:
    rx, ry = max(200, int(lado_px * ancho / largo)), lado_px
render("planta_superior.png", ob_cam, rx, ry)
print("DUCTUSARQ-LISTO")
