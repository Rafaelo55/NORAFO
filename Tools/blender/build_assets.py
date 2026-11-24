"""
No Sleep Pogo - construccion procedural de modelos y animaciones en Blender.

Ejecutar (Blender 5.x):
  blender -b --factory-startup --python Tools/blender/build_assets.py -- <SourceArt> [--preview <dir>]

Genera:
  SourceArt/Models/*.fbx      (una malla por archivo, pivote en el origen)
  SourceArt/Models/NoSleepPogo_Assets.blend
  <preview>/*.png             (renders de revision)

Convenciones:
  - Metros, +X adelante, +Z arriba (Unreal recibe +X adelante, cm).
  - El nombre de cada material de Blender == sufijo de la textura T_<Nombre>.
  - Estilo PS1: pocos poligonos, sombreado facetado, UV por proyeccion de caja.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
ROOT = os.path.abspath(argv[0]) if argv else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "SourceArt"))
PREVIEW = None
if "--preview" in argv:
    PREVIEW = os.path.abspath(argv[argv.index("--preview") + 1])
    os.makedirs(PREVIEW, exist_ok=True)
TEX_DIR = os.path.join(ROOT, "Textures")
OUT_DIR = os.path.join(ROOT, "Models")
os.makedirs(OUT_DIR, exist_ok=True)

FPS = 30

# ----------------------------------------------------------------------------- escena
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
scene.render.fps = FPS

MATS = {}


def get_mat(name):
    if name in MATS:
        return MATS[name]
    m = bpy.data.materials.new(name)
    try:
        m.use_nodes = True
    except Exception:
        pass
    nt = m.node_tree
    bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
    path = os.path.join(TEX_DIR, "T_%s.png" % name)
    if bsdf and os.path.exists(path):
        img = bpy.data.images.load(path, check_existing=True)
        tn = nt.nodes.new("ShaderNodeTexImage")
        tn.image = img
        tn.interpolation = 'Closest'
        nt.links.new(tn.outputs["Color"], bsdf.inputs["Base Color"])
        nt.nodes.active = tn
        if "Roughness" in bsdf.inputs:
            bsdf.inputs["Roughness"].default_value = 1.0
    MATS[name] = m
    return m


class Builder:
    """Acumula geometria en un bmesh con material, grupo de huesos y UV por caja."""

    def __init__(self, name, density=1.0):
        self.name = name
        self.bm = bmesh.new()
        self.uv = self.bm.loops.layers.uv.verify()
        self.dl = self.bm.verts.layers.deform.verify()
        self.mats = []
        self.groups = []
        self.density = density

    def _mat_index(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def _group_index(self, g):
        if g not in self.groups:
            self.groups.append(g)
        return self.groups.index(g)

    def _finish(self, verts, mat, group=None, density=None, uv_mode="box", uv_rect=None):
        verts = list(verts)
        faces = {f for v in verts for f in v.link_faces}
        mi = self._mat_index(mat)
        d = density if density is not None else self.density
        for f in faces:
            f.material_index = mi
            f.smooth = False
        self.bm.normal_update()
        for f in faces:
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for lp in f.loops:
                c = lp.vert.co
                if uv_mode == "box":
                    if ax == 2:
                        u, v = c.x, c.y * (1 if n.z > 0 else -1)
                    elif ax == 0:
                        u, v = c.y * (1 if n.x > 0 else -1), c.z
                    else:
                        u, v = -c.x * (1 if n.y > 0 else -1), c.z
                    lp[self.uv].uv = (u * d, v * d)
        if group is not None:
            gi = self._group_index(group)
            for v in verts:
                v[self.dl][gi] = 1.0
        return faces

    # primitivas -------------------------------------------------------------
    def box(self, center, size, mat, group=None, rot=None, bevel=0.0, density=None):
        m = Matrix.Translation(Vector(center))
        if rot is not None:
            m = m @ rot.to_matrix().to_4x4() if isinstance(rot, Quaternion) else m @ rot
        m = m @ Matrix.Diagonal(Vector((size[0], size[1], size[2], 1.0)))
        r = bmesh.ops.create_cube(self.bm, size=1.0, matrix=m)
        verts = r["verts"]
        if bevel > 0:
            edges = list({e for v in verts for e in v.link_edges})
            res = bmesh.ops.bevel(self.bm, geom=verts + edges, offset=bevel, segments=1, affect='EDGES', profile=0.5)
            verts = list({v for f in res.get("faces", []) for v in f.verts} | set(v for v in verts if v.is_valid))
            verts = [v for v in verts if v.is_valid]
            # incluir todos los vertices conectados (la pieza entera)
            verts = self._island(verts)
        return self._finish(verts, mat, group, density)

    def _island(self, seed):
        seen = set(seed)
        stack = list(seed)
        while stack:
            v = stack.pop()
            for e in v.link_edges:
                o = e.other_vert(v)
                if o not in seen:
                    seen.add(o)
                    stack.append(o)
        return list(seen)

    def cyl(self, p0, p1, r0, mat, group=None, r1=None, sides=8, cap=True, density=None):
        p0, p1 = Vector(p0), Vector(p1)
        r1 = r0 if r1 is None else r1
        d = p1 - p0
        L = d.length
        q = Vector((0, 0, 1)).rotation_difference(d.normalized())
        m = Matrix.Translation((p0 + p1) / 2) @ q.to_matrix().to_4x4()
        r = bmesh.ops.create_cone(self.bm, cap_ends=cap, cap_tris=False, segments=sides, radius1=r0, radius2=r1, depth=L, matrix=m)
        return self._finish(r["verts"], mat, group, density)

    def sphere(self, center, radius, mat, group=None, seg=8, rings=5, scale=(1, 1, 1), density=None):
        m = Matrix.Translation(Vector(center)) @ Matrix.Diagonal(Vector((scale[0], scale[1], scale[2], 1)))
        r = bmesh.ops.create_uvsphere(self.bm, u_segments=seg, v_segments=rings, radius=radius, matrix=m)
        return self._finish(r["verts"], mat, group, density)

    def quad(self, pts, mat, uvs=None, group=None, density=None):
        vs = [self.bm.verts.new(Vector(p)) for p in pts]
        f = self.bm.faces.new(vs)
        faces = self._finish(vs, mat, group, density)
        if uvs:
            for lp, uv in zip(f.loops, uvs):
                lp[self.uv].uv = uv
        return f

    def polyline_tube(self, pts, radius, mat, group=None, sides=4, density=None):
        allv = []
        for a, b in zip(pts[:-1], pts[1:]):
            faces = self.cyl(a, b, radius, mat, group, sides=sides, cap=False, density=density)
            allv += [v for f in faces for v in f.verts]
        return allv

    def build(self, link=True):
        me = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(me)
        self.bm.free()
        for m in self.mats:
            me.materials.append(get_mat(m))
        ob = bpy.data.objects.new(self.name, me)
        for g in self.groups:
            ob.vertex_groups.new(name=g)
        if link:
            col = bpy.data.collections.get(self.name) or bpy.data.collections.new(self.name)
            if col.name not in scene.collection.children:
                scene.collection.children.link(col)
            col.objects.link(ob)
        return ob


def export_fbx(objs, path, anim=False):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.hide_set(False)
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, apply_scale_options='FBX_SCALE_UNITS',
        axis_forward='-Z', axis_up='Y', mesh_smooth_type='FACE', use_mesh_modifiers=True,
        add_leaf_bones=False, primary_bone_axis='Y', secondary_bone_axis='X',
        bake_anim=anim, bake_anim_use_all_actions=anim, bake_anim_use_nla_strips=False,
        bake_anim_use_all_bones=True, bake_anim_force_startend_keying=True, bake_anim_simplify_factor=0.0,
        use_armature_deform_only=False, armature_nodetype='NULL')


STATIC = []  # (objeto)


def static(builder):
    ob = builder.build()
    STATIC.append(ob)
    return ob


# ============================================================================ primitivas
def make_cube():
    b = Builder("SM_Cube", density=1.0)
    r = bmesh.ops.create_cube(b.bm, size=1.0)
    faces = b._finish(r["verts"], "Concrete")
    # UV 0..1 por cara (para decals / texturas completas)
    for f in faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for lp in f.loops:
            c = lp.vert.co
            if ax == 2:
                u, v = c.x + 0.5, (c.y if n.z > 0 else -c.y) + 0.5
            elif ax == 0:
                u, v = (c.y if n.x > 0 else -c.y) + 0.5, c.z + 0.5
            else:
                u, v = (-c.x if n.y > 0 else c.x) + 0.5, c.z + 0.5
            lp[b.uv].uv = (u, v)
    static(b)


def make_plane():
    b = Builder("SM_Plane")
    b.quad([(-0.5, -0.5, 0), (0.5, -0.5, 0), (0.5, 0.5, 0), (-0.5, 0.5, 0)], "EyeMosaic",
           uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    static(b)


def make_cylinder():
    b = Builder("SM_Cylinder")
    b.cyl((0, 0, 0), (0, 0, 1), 0.5, "MetalGray", sides=8)
    static(b)


def make_sky():
    b = Builder("SM_SkyDome")
    seg, rings = 16, 6
    rows = []
    for j in range(rings + 1):
        el = math.radians(-15 + j * (105 / rings))
        row = []
        for i in range(seg + 1):
            az = 2 * math.pi * i / seg
            row.append(Vector((math.cos(el) * math.cos(az), math.cos(el) * math.sin(az), math.sin(el))))
        rows.append(row)
    for j in range(rings):
        for i in range(seg):
            pts = [rows[j][i], rows[j + 1][i], rows[j + 1][i + 1], rows[j][i + 1]]  # normal hacia adentro
            v0 = 1 - j / rings
            v1 = 1 - (j + 1) / rings
            b.quad(pts, "Sky", uvs=[(i / seg * 4, v0), (i / seg * 4, v1), ((i + 1) / seg * 4, v1), ((i + 1) / seg * 4, v0)])
    static(b)


# ============================================================================ kit urbano
def make_frontage_bays():
    """Modulos de 3 m. El frente da a +Y y el origen coincide con el suelo del piso."""
    shop = Builder("SM_ShopBay")
    # Jambas profundas, zocalo de obra y un tapacanto oscuro bajo la marquesina.
    for x in (-1.435, 1.435):
        shop.box((x, 0.085, 1.28), (0.13, 0.17, 2.50), "PlasterBeige", bevel=0.014)
        shop.box((x, 0.100, 0.24), (0.15, 0.20, 0.48), "PlasterGray", bevel=0.014)
    shop.box((0, 0.11, 2.49), (2.99, 0.22, 0.115), "MetalGray", bevel=0.012)
    shop.box((0, 0.15, 3.015), (2.99, 0.30, 0.095), "PlasterGray", bevel=0.018)
    static(shop)

    window = Builder("SM_WindowBay")
    # Coincide con la abertura de 36 px en la textura de fachada de 64 px.
    for x in (-0.855, 0.855):
        window.box((x, 0.044, 1.55), (0.056, 0.088, 1.42), "MetalGray")
    window.box((0, 0.055, 2.255), (1.76, 0.11, 0.075), "MetalGray")
    window.box((0, 0.095, 0.83), (1.91, 0.19, 0.10), "PlasterGray", bevel=0.015)
    static(window)


def make_bollard():
    b = Builder("SM_Bollard", density=4)
    b.cyl((0, 0, 0), (0, 0, 0.72), 0.075, "Concrete", sides=8)
    b.cyl((0, 0, 0.72), (0, 0, 0.8), 0.075, "Concrete", r1=0.04, sides=8)
    b.cyl((0, 0, 0.52), (0, 0, 0.6), 0.078, "GuideTile", sides=8)
    static(b)


def make_curb():
    b = Builder("SM_Curb", density=2)
    b.box((0, 0, 0.1), (1.0, 0.25, 0.2), "Curb", bevel=0.02)
    static(b)


def make_awning():
    """Toldo jugable: ancho 1 m (X, se escala), profundidad 1 m (Y desde la pared, se escala),
    tapa plana en z=0 con franja marcada al frente; faldon delantero hasta z=-0.3."""
    b = Builder("SM_Awning", density=1.0)
    # tapa (tela) - dos paneles: atras plano con leve caida visual hacia la pared
    b.box((0, 0.47, -0.04), (1.0, 0.94, 0.08), "AwningRed")
    # franja de borde marcada
    b.box((0, 0.97, -0.035), (1.0, 0.06, 0.09), "EdgeStripe")
    # faldon con festones
    n = 6
    for i in range(n):
        x0 = -0.5 + i / n
        x1 = x0 + 1 / n
        b.quad([(x0, 1.0, -0.08), (x1, 1.0, -0.08), (x1, 1.0, -0.26), ((x0 + x1) / 2, 1.0, -0.33), (x0, 1.0, -0.26)][:4],
               "AwningRed")
        b.quad([(x0, 1.0, -0.26), (x1, 1.0, -0.26), ((x0 + x1) / 2, 1.0, -0.33)] + [], "AwningRed") if False else None
    b.box((0, 1.0, -0.17), (1.0, 0.02, 0.18), "AwningRed")
    # laterales triangulares
    for x in (-0.5, 0.5):
        b.quad([(x, 0.0, 0.0), (x, 1.0, 0.0), (x, 1.0, -0.26), (x, 0.0, -0.06)], "AwningRed")
    # estructura metalica
    for x in (-0.48, 0.48):
        b.cyl((x, 0.0, -0.5), (x, 0.9, -0.1), 0.015, "MetalGray", sides=4)
    static(b)


def make_awning_folded():
    b = Builder("SM_AwningFolded", density=2)
    b.cyl((-0.5, 0.12, 0), (0.5, 0.12, 0), 0.12, "AwningBlue", sides=8)
    b.box((0, 0.05, 0.0), (1.0, 0.1, 0.22), "MetalGray")
    static(b)


def make_railing():
    b = Builder("SM_Railing", density=4)
    for i in range(3):
        b.box((i * 1.0, 0, 0.5), (0.05, 0.05, 1.0), "MetalRust")
    b.box((1.0, 0, 1.0), (2.05, 0.06, 0.05), "MetalRust")
    b.box((1.0, 0, 0.55), (2.0, 0.03, 0.03), "MetalRust")
    for i in range(1, 10):
        b.box((i * 0.2, 0, 0.3), (0.02, 0.02, 0.5), "MetalRust")
    b.box((1.0, 0, 0.06), (2.0, 0.03, 0.03), "MetalRust")
    static(b)


def make_pole():
    b = Builder("SM_UtilityPole", density=2)
    b.cyl((0, 0, 0), (0, 0, 9.0), 0.16, "Pole", r1=0.12, sides=8)
    b.box((0, 0, 8.3), (0.12, 1.6, 0.12), "Wood")
    b.box((0, 0, 7.5), (0.1, 1.2, 0.1), "Wood")
    for y in (-0.7, -0.25, 0.25, 0.7):
        b.cyl((0, y, 8.36), (0, y, 8.5), 0.035, "White", sides=6)
    b.cyl((0.3, 0, 6.2), (0.3, 0, 6.9), 0.22, "MetalGray", sides=8)
    b.box((0.18, 0, 6.5), (0.1, 0.08, 0.3), "MetalGray")
    static(b)


def make_vending():
    b = Builder("SM_Vending", density=1)
    b.box((0, 0, 0.9), (0.75, 0.95, 1.8), "SignRed")
    # frente con textura completa
    b.quad([(0.376, 0.46, 0.02), (0.376, -0.46, 0.02), (0.376, -0.46, 1.78), (0.376, 0.46, 1.78)], "Vending",
           uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    b.box((0, 0, 1.83), (0.8, 1.0, 0.06), "MetalGray")
    b.box((0, 0, 0.9), (0.8, 1.0, 1.75), "EdgeStripe") if False else None
    static(b)


def make_ac():
    b = Builder("SM_ACUnit", density=2)
    b.box((0, 0, 0.3), (0.3, 0.8, 0.6), "PlasterBeige", bevel=0.015)
    b.quad([(0.151, 0.4, 0.01), (0.151, -0.4, 0.01), (0.151, -0.4, 0.59), (0.151, 0.4, 0.59)], "ACUnit",
           uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    static(b)


def make_crate():
    b = Builder("SM_CrateStack", density=1)
    for k in range(3):
        b.box((0, 0, 0.15 + k * 0.3), (0.45, 0.36, 0.29), "Crate")
    static(b)


def make_planter():
    b = Builder("SM_Planter", density=2)
    b.box((0, 0, 0.22), (1.2, 0.5, 0.45), "Concrete", bevel=0.02)
    for x in (-0.35, 0.05, 0.4):
        b.sphere((x, 0, 0.55), 0.28, "Leaves", seg=6, rings=4, scale=(1.1, 0.8, 0.7))
    static(b)


def make_lantern():
    b = Builder("SM_Lantern", density=2)
    prof = [(0.0, 0.07), (0.05, 0.13), (0.15, 0.16), (0.27, 0.15), (0.36, 0.11), (0.4, 0.07)]
    for (z0, r0), (z1, r1) in zip(prof[:-1], prof[1:]):
        b.cyl((0, 0, z0), (0, 0, z1), r0, "Lantern", r1=r1, sides=8, cap=False)
    b.cyl((0, 0, -0.02), (0, 0, 0.0), 0.07, "Black", sides=8)
    b.cyl((0, 0, 0.4), (0, 0, 0.43), 0.07, "Black", sides=8)
    b.cyl((0, 0, 0.43), (0, 0, 0.6), 0.008, "Black", sides=4)
    static(b)


def make_sign_vertical():
    b = Builder("SM_SignVertical", density=1)
    b.box((0, 0, 0.9), (0.4, 0.12, 1.8), "MetalGray")
    for s in (1, -1):
        y = 0.061 * s
        pts = [(-0.19, y, 0.02), (0.19, y, 0.02), (0.19, y, 1.78), (-0.19, y, 1.78)]
        if s < 0:
            pts = [pts[1], pts[0], pts[3], pts[2]]
        b.quad(pts, "SignRed", uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    b.box((0, -0.3, 1.6), (0.04, 0.5, 0.04), "MetalGray")
    b.box((0, -0.3, 0.2), (0.04, 0.5, 0.04), "MetalGray")
    static(b)


def make_road_sign():
    b = Builder("SM_RoadSign", density=4)
    b.cyl((0, 0, 0), (0, 0, 2.6), 0.035, "MetalGray", sides=6)
    b.cyl((0.04, 0, 2.3), (0.06, 0, 2.3), 0.3, "AwningBlue", sides=10)
    b.box((0.065, 0, 2.3), (0.01, 0.36, 0.06), "White")
    static(b)


def make_water_tank():
    b = Builder("SM_WaterTank", density=1)
    b.cyl((0, 0, 0.6), (0, 0, 2.0), 0.7, "WaterTank", sides=10)
    b.cyl((0, 0, 2.0), (0, 0, 2.2), 0.7, "WaterTank", r1=0.3, sides=10)
    for a in range(4):
        x, y = math.cos(a * math.pi / 2 + 0.78) * 0.55, math.sin(a * math.pi / 2 + 0.78) * 0.55
        b.box((x, y, 0.3), (0.08, 0.08, 0.6), "MetalRust")
    static(b)


def make_vent():
    b = Builder("SM_RoofVent", density=3)
    b.box((0, 0, 0.3), (0.5, 0.5, 0.6), "MetalGray")
    b.box((0, 0, 0.65), (0.6, 0.6, 0.1), "MetalGray")
    static(b)


def make_ring(name, mat):
    b = Builder(name)
    b.quad([(-0.5, -0.5, 0), (0.5, -0.5, 0), (0.5, 0.5, 0), (-0.5, 0.5, 0)], mat, uvs=[(0, 0), (1, 0), (1, 1), (0, 1)])
    static(b)


# ============================================================================ auto blanco
def make_car():
    b = Builder("SM_Car", density=1.0)
    L, W = 4.2, 1.7
    # carroceria baja (perfil lateral extruido en Y)
    prof = [(-2.1, 0.32), (-2.1, 0.75), (-1.95, 0.85), (1.6, 0.85), (2.1, 0.72), (2.1, 0.32)]
    # caja principal con bisel
    b.box((0, 0, 0.58), (4.2, 1.7, 0.52), "CarBody", bevel=0.08)
    # capot inclinado y cabina
    cab = [(-1.75, 0.84), (-1.45, 1.42), (0.55, 1.42), (1.15, 0.84)]
    hw = 0.78
    tw = 0.68
    vs_l = [b.bm.verts.new((x, -hw if z < 1 else -tw, z)) for x, z in cab]
    vs_r = [b.bm.verts.new((x, hw if z < 1 else tw, z)) for x, z in cab]
    fs = []
    fs.append(b.bm.faces.new(list(reversed(vs_l))))
    fs.append(b.bm.faces.new(vs_r))
    for i in range(4):
        j = (i + 1) % 4
        fs.append(b.bm.faces.new([vs_l[i], vs_l[j], vs_r[j], vs_r[i]]))
    b._finish(vs_l + vs_r, "CarGlass")
    # techo blanco
    b.box((-0.45, 0, 1.44), (1.95, 1.34, 0.05), "CarBody")
    # pilares
    for y in (-0.7, 0.7):
        b.box((-1.58, y, 1.13), (0.12, 0.06, 0.6), "CarBody", rot=Matrix.Rotation(math.radians(-27), 4, 'Y'))
        b.box((0.82, y, 1.13), (0.12, 0.06, 0.66), "CarBody", rot=Matrix.Rotation(math.radians(42), 4, 'Y'))
        b.box((-0.45, y * 1.02, 1.13), (0.08, 0.05, 0.56), "CarBody")
    # ruedas
    for x in (-1.35, 1.3):
        for y in (-0.78, 0.78):
            b.cyl((x, y - 0.1 * (1 if y > 0 else -1), 0.3), (x, y + 0.08 * (1 if y > 0 else -1), 0.3), 0.3, "Tire", sides=8)
    # faros, luces traseras, patente
    for y in (-0.6, 0.6):
        b.box((2.1, y, 0.68), (0.04, 0.32, 0.12), "CarLights")
        b.box((-2.1, y, 0.7), (0.04, 0.26, 0.14), "CarLights")
    b.box((2.11, 0, 0.45), (0.03, 0.4, 0.12), "White")
    b.box((-2.11, 0, 0.5), (0.03, 0.4, 0.12), "White")
    b.box((2.05, 0, 0.36), (0.1, 1.6, 0.1), "Black")
    b.box((-2.05, 0, 0.36), (0.1, 1.6, 0.1), "Black")
    # espejos
    for y in (-0.92, 0.92):
        b.box((0.9, y, 1.0), (0.12, 0.1, 0.09), "CarBody")
    static(b)


# ============================================================================ jinete + pogo (esqueletico)
# posiciones de reposo (m), +X adelante, +Y izquierda, origen = punta del pogo en el piso
P = {
    "grip_l": Vector((0.0, 0.23, 1.38)),
    "wrist_l": Vector((-0.07, 0.25, 1.34)),
    "elbow_l": Vector((-0.30, 0.35, 1.10)),
    "shoulder_l": Vector((-0.47, 0.25, 1.30)),
    "ankle_l": Vector((-0.06, 0.11, 0.44)),
    "toe_l": Vector((0.12, 0.11, 0.40)),
    "knee_l": Vector((0.07, 0.14, 0.80)),
    "hip_l": Vector((-0.22, 0.11, 0.96)),
}
for k in list(P.keys()):
    if k.endswith("_l"):
        v = P[k].copy()
        v.y = -v.y
        P[k[:-2] + "_r"] = v

BONES = [
    # nombre, cabeza, cola, padre
    ("root", (0, 0, 0), (0, 0, 0.05), None),
    ("frame", (0, 0, 0.30), (0, 0, 1.38), "root"),
    ("plunger", (0, 0, 0.0), (0, 0, 0.30), "frame"),
    ("spring", (0, 0, 0.07), (0, 0, 0.32), "frame"),
    ("body", (-0.22, 0, 0.92), (-0.22, 0, 1.10), "root"),
    ("spine", (-0.22, 0, 1.10), (-0.40, 0, 1.40), "body"),
    ("head", (-0.40, 0, 1.52), (-0.40, 0, 1.68), "spine"),
]
for s in ("l", "r"):
    BONES += [
        ("hand_" + s, P["wrist_" + s], P["grip_" + s], "root"),
        ("lowerarm_" + s, P["wrist_" + s], P["elbow_" + s], "hand_" + s),
        ("upperarm_" + s, P["elbow_" + s], P["shoulder_" + s], "lowerarm_" + s),
        ("foot_" + s, P["ankle_" + s], P["toe_" + s], "root"),
        ("calf_" + s, P["ankle_" + s], P["knee_" + s], "foot_" + s),
        ("thigh_" + s, P["knee_" + s], P["hip_" + s], "calf_" + s),
    ]


def make_gripping_hand(b, side, sign):
    """Una sola carcasa desde la muneca hasta los nudillos, plegada sobre el manillar."""
    grip = P["grip_" + side]
    group = "hand_" + side
    # x, ancho medio, cota inferior y superior, desplazamiento hacia el menique.
    sections = [(-0.095, 0.033, -0.064, 0.003, 0.017),
                (-0.069, 0.048, -0.049, 0.032, 0.012),
                (-0.025, 0.064, -0.024, 0.060, 0.004),
                (0.011, 0.065, -0.021, 0.061, 0.0),
                (0.040, 0.052, -0.044, 0.031, -0.003),
                (0.030, 0.043, -0.055, -0.023, -0.003)]
    # El dorso ancho se afina hacia los dedos. El lado del menique queda mas bajo.
    outline = [(-0.75, 0.0), (0.65, 0.0), (1.0, 0.24), (0.92, 0.69),
               (0.48, 0.90), (-0.30, 1.0), (-0.87, 0.84), (-1.0, 0.39)]
    rings = []
    for x, width, low, high, offset in sections:
        ring = []
        for across, height in outline:
            pos = grip + Vector((x, sign * (offset + across * width), low + (high - low) * height))
            ring.append(b.bm.verts.new(pos))
        rings.append(ring)
    faces = [b.bm.faces.new(rings[0][::-1]), b.bm.faces.new(rings[-1])]
    for previous, current in zip(rings, rings[1:]):
        for i in range(len(outline)):
            j = (i + 1) % len(outline)
            faces.append(b.bm.faces.new((previous[i], previous[j], current[j], current[i])))
    bmesh.ops.recalc_face_normals(b.bm, faces=faces)
    b._finish([v for ring in rings for v in ring], "Skin", group, density=2)
    base = grip + Vector((-0.049, -sign * 0.042, 0.021))
    joint = grip + Vector((-0.020, -sign * 0.068, 0.006))
    tip = grip + Vector((0.013, -sign * 0.058, -0.024))
    b.cyl(base, joint, 0.026, "Skin", group, r1=0.023, sides=6, density=2)
    b.cyl(joint, tip, 0.023, "Skin", group, r1=0.016, sides=6, density=2)


def make_rider():
    b = Builder("SK_PogoRider", density=6.0)
    # ---- pogo
    b.cyl((0, 0, 0.0), (0, 0, 0.06), 0.034, "Rubber", "plunger", sides=8)
    b.cyl((0, 0, 0.06), (0, 0, 0.36), 0.014, "SpringMetal", "plunger", sides=6)
    coil = []
    turns, steps = 6, 8
    for i in range(turns * steps + 1):
        a = 2 * math.pi * i / steps
        z = 0.07 + 0.25 * i / (turns * steps)
        coil.append((math.cos(a) * 0.04, math.sin(a) * 0.04, z))
    b.polyline_tube(coil, 0.007, "SpringMetal", "spring", sides=4)
    b.cyl((0, 0, 0.30), (0, 0, 1.38), 0.024, "PogoRed", "frame", sides=8)
    b.cyl((0, 0, 0.30), (0, 0, 0.36), 0.05, "PogoRed", "frame", sides=8)
    b.cyl((0, -0.17, 0.34), (0, 0.17, 0.34), 0.016, "PogoRed", "frame", sides=6)
    for y in (-0.11, 0.11):
        b.box((0.0, y, 0.355), (0.10, 0.12, 0.022), "Rubber", "frame")
    b.cyl((0, -0.30, 1.38), (0, 0.30, 1.38), 0.02, "PogoRed", "frame", sides=8)
    b.cyl((0, -0.035, 1.38), (0, 0.035, 1.38), 0.028, "PogoRed", "frame", sides=8)
    for y in (-1, 1):
        b.cyl((0, y * 0.165, 1.38), (0, y * 0.305, 1.38), 0.029, "Grip", "frame", sides=8)
        b.cyl((0, y * 0.305, 1.38), (0, y * 0.315, 1.38), 0.034, "Grip", "frame", sides=8)
    # ---- manos facetadas (mitones envolviendo el puno)
    for s, sg in (("l", 1), ("r", -1)):
        make_gripping_hand(b, s, sg)
        # antebrazo y brazo
        b.cyl(P["wrist_" + s], P["elbow_" + s], 0.038, "Skin", "lowerarm_" + s, r1=0.05, sides=6)
        mid = P["elbow_" + s].lerp(P["shoulder_" + s], 0.45)
        b.cyl(P["elbow_" + s], mid, 0.05, "Skin", "upperarm_" + s, r1=0.056, sides=6)
        b.cyl(mid, P["shoulder_" + s], 0.058, "White", "upperarm_" + s, r1=0.062, sides=6)
        # piernas (jean) y zapatillas
        b.cyl(P["ankle_" + s] + Vector((0, 0, 0.03)), P["knee_" + s], 0.05, "Denim", "calf_" + s, r1=0.058, sides=6)
        b.cyl(P["knee_" + s], P["hip_" + s], 0.062, "Denim", "thigh_" + s, r1=0.078, sides=6)
        b.sphere(P["knee_" + s], 0.06, "Denim", "thigh_" + s, seg=6, rings=4)
        a = P["ankle_" + s]
        b.box((a.x + 0.05, a.y, 0.405), (0.28, 0.105, 0.08), "Sneaker", "foot_" + s, bevel=0.015)
        b.box((a.x + 0.17, a.y, 0.395), (0.06, 0.1, 0.06), "Sneaker", "foot_" + s, bevel=0.01)
        b.box((a.x + 0.04, a.y, 0.37), (0.29, 0.11, 0.02), "Rubber", "foot_" + s)
    b.box((-0.30, 0, 0.99), (0.14, 0.26, 0.12), "Denim", "body", bevel=0.02)
    mesh_ob = b.build()

    # ---- armadura
    arm_data = bpy.data.armatures.new("Armature")
    arm = bpy.data.objects.new("Armature", arm_data)
    col = bpy.data.collections["SK_PogoRider"]
    col.objects.link(arm)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode='EDIT')
    for name, h, t, par in BONES:
        eb = arm_data.edit_bones.new(name)
        eb.head = Vector(h)
        eb.tail = Vector(t)
        eb.roll = 0.0
        if par:
            eb.parent = arm_data.edit_bones[par]
    bpy.ops.object.mode_set(mode='OBJECT')
    mesh_ob.parent = arm
    mod = mesh_ob.modifiers.new("Armature", 'ARMATURE')
    mod.object = arm
    for pb in arm.pose.bones:
        pb.rotation_mode = 'QUATERNION'
    make_rider_actions(arm)
    return arm, mesh_ob


# --- animacion ---------------------------------------------------------------------------
def bone_rest(arm):
    return {bn.name: bn.matrix_local.copy() for bn in arm.data.bones}


def seg_matrix(rest, p_old, q_old, p_new, q_new):
    """Matriz armadura para un hueso cuyo segmento va de p_old->q_old en reposo a p_new->q_new."""
    rq = (q_old - p_old).normalized().rotation_difference((q_new - p_new).normalized())
    rot = rq.to_matrix().to_4x4() @ rest.to_3x3().normalized().to_4x4()
    return Matrix.Translation(p_new) @ rot


def two_bone(a, c, l1, l2, hint):
    """IK de dos huesos: devuelve la articulacion media entre a y c."""
    d = (c - a)
    dist = min(d.length, l1 + l2 - 1e-4)
    dirv = d.normalized()
    x = (l1 * l1 - l2 * l2 + dist * dist) / (2 * dist)
    h = math.sqrt(max(l1 * l1 - x * x, 0.0))
    p = hint - dirv * hint.dot(dirv)
    if p.length < 1e-6:
        p = Vector((1, 0, 0))
    return a + dirv * x + p.normalized() * h


def bend_hint(a, mid, c):
    dirv = (c - a).normalized()
    v = mid - a
    return v - dirv * v.dot(dirv)


def apply_pose(arm, frame, targets):
    """targets: dict hueso -> matriz armadura deseada (los demas en reposo relativo)."""
    rest = bone_rest(arm)
    world = {}
    for bn in arm.data.bones:  # orden padre->hijo
        par = bn.parent
        M = targets.get(bn.name)
        if M is None:
            # sigue al padre rigidamente
            if par is None:
                M = rest[bn.name]
            else:
                M = world[par.name] @ (rest[par.name].inverted() @ rest[bn.name])
        world[bn.name] = M
        if par is None:
            basis = rest[bn.name].inverted() @ M
        else:
            rest_rel = rest[par.name].inverted() @ rest[bn.name]
            new_rel = world[par.name].inverted() @ M
            basis = rest_rel.inverted() @ new_rel
        pb = arm.pose.bones[bn.name]
        loc, rot, sca = basis.decompose()
        pb.location = loc
        pb.rotation_quaternion = rot
        pb.scale = sca
        pb.keyframe_insert("location", frame=frame)
        pb.keyframe_insert("rotation_quaternion", frame=frame)
        pb.keyframe_insert("scale", frame=frame)


def limb_targets(rest, s, wrist, shoulder, ankle, hip, hand_M, foot_M, out):
    """Resuelve brazos y piernas con IK y llena out con matrices de hueso."""
    W0, E0, S0 = P["wrist_" + s], P["elbow_" + s], P["shoulder_" + s]
    A0, K0, H0 = P["ankle_" + s], P["knee_" + s], P["hip_" + s]
    out["hand_" + s] = hand_M
    out["foot_" + s] = foot_M
    E = two_bone(wrist, shoulder, (E0 - W0).length, (S0 - E0).length, bend_hint(W0, E0, S0))
    out["lowerarm_" + s] = seg_matrix(rest["lowerarm_" + s], W0, E0, wrist, E)
    out["upperarm_" + s] = seg_matrix(rest["upperarm_" + s], E0, S0, E, shoulder)
    K = two_bone(ankle, hip, (K0 - A0).length, (H0 - K0).length, bend_hint(A0, K0, H0))
    out["calf_" + s] = seg_matrix(rest["calf_" + s], A0, K0, ankle, K)
    out["thigh_" + s] = seg_matrix(rest["thigh_" + s], K0, H0, K, hip)


def ease(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def new_action(arm, name):
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = act
    return act


FRAME_DROP = 0.20   # compresion del resorte (m)
BODY_DROP = 0.30    # caida total del cuerpo/camara a carga maxima (m)


def make_rider_actions(arm):
    rest = bone_rest(arm)
    T = Matrix.Translation

    # --- Compress: 0..30 (se recorre segun la carga 0..1)
    new_action(arm, "Compress")
    for f in range(0, 31):
        c = f / 30.0
        fd, bd = FRAME_DROP * c, BODY_DROP * c
        tg = {}
        tg["frame"] = T((0, 0, -fd)) @ rest["frame"]
        tg["plunger"] = rest["plunger"]
        base = Vector((0, 0, 0.07))
        L = 0.25
        s = (L - fd) / L
        tg["spring"] = T(base) @ Matrix.Diagonal(Vector((1 + 0.25 * c, 1 + 0.25 * c, s, 1))) @ T(-base) @ rest["spring"]
        tg["body"] = T((0, 0, -bd)) @ rest["body"]
        for sd in ("l", "r"):
            hm = T((0, 0, -fd)) @ rest["hand_" + sd]
            fm = T((0, 0, -fd)) @ rest["foot_" + sd]
            limb_targets(rest, sd, P["wrist_" + sd] + Vector((0, 0, -fd)), P["shoulder_" + sd] + Vector((0.02 * c, 0, -bd)),
                         P["ankle_" + sd] + Vector((0, 0, -fd)), P["hip_" + sd] + Vector((-0.03 * c, 0, -bd)), hm, fm, tg)
        apply_pose(arm, f, tg)

    # --- LetGo: 0..45 (suelta el pogo al llegar al ojo azul)
    new_action(arm, "LetGo")
    for f in range(0, 46):
        t = f / 30.0
        tg = {}
        # el pogo cae hacia adelante pivotando en la punta
        a = math.radians(84) * ease((t - 0.12) / 0.6) ** 1.6
        if t > 0.75:
            a -= math.radians(6) * math.sin(min(1, (t - 0.75) / 0.3) * math.pi)
        R = Matrix.Rotation(a, 4, 'Y')
        slide = T((0.08 * ease((t - 0.1) / 0.6), 0, 0))
        tg["frame"] = slide @ R @ rest["frame"]
        # el cuerpo baja de los apoyapies al piso
        drop = 0.36 * ease((t - 0.18) / 0.4)
        dip = 0.05 * math.sin(min(1, max(0, (t - 0.45) / 0.35)) * math.pi)
        tg["body"] = T((-0.04 * ease(t / 0.6), 0, -drop - dip)) @ rest["body"]
        for sd, sg in (("l", 1), ("r", -1)):
            open_t = ease(t / 0.22)
            down_t = ease((t - 0.25) / 0.6)
            hoff = Vector((-0.06 * open_t - 0.30 * down_t, sg * (0.05 * open_t + 0.08 * down_t), 0.05 * open_t - 0.45 * down_t))
            hrot = Matrix.Rotation(sg * math.radians(-35) * open_t, 4, 'X') @ Matrix.Rotation(math.radians(25) * open_t + math.radians(40) * down_t, 4, 'Y')
            w = P["wrist_" + sd]
            hm = T(w + hoff) @ hrot @ T(-w) @ rest["hand_" + sd]
            foff = Vector((-0.03 * ease((t - 0.18) / 0.35), sg * 0.05 * ease((t - 0.18) / 0.35), -0.36 * ease((t - 0.18) / 0.35)))
            fm = T(foff) @ rest["foot_" + sd]
            body_off = Vector((-0.04 * ease(t / 0.6), 0, -drop - dip))
            limb_targets(rest, sd, w + hoff, P["shoulder_" + sd] + body_off, P["ankle_" + sd] + foff, P["hip_" + sd] + body_off, hm, fm, tg)
        apply_pose(arm, f, tg)

    # --- Balance: 0..60 loop (parado sobre el pogo esperando el primer salto)
    new_action(arm, "Balance")
    for f in range(0, 61):
        ph = 2 * math.pi * f / 60.0
        R = Matrix.Rotation(math.radians(2.2) * math.sin(ph), 4, 'X') @ Matrix.Rotation(math.radians(1.2) * math.sin(2 * ph + 0.6), 4, 'Y')
        apply_pose(arm, f, {"root": R @ rest["root"]})

    arm.animation_data.action = None


# ============================================================================ render de revision
def setup_preview():
    scene.render.engine = 'BLENDER_WORKBENCH'
    sh = scene.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'TEXTURE'
    sh.show_shadows = True
    sh.show_cavity = False
    scene.render.resolution_x = 320
    scene.render.resolution_y = 320
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("W")
    scene.world = world
    cam_data = bpy.data.cameras.new("PreviewCam")
    cam = bpy.data.objects.new("PreviewCam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def frame_camera(cam, objs, direction=(1.0, -0.8, 0.6), lens=50):
    pts = []
    dg = bpy.context.evaluated_depsgraph_get()
    for o in objs:
        if o.type != 'MESH':
            continue
        oe = o.evaluated_get(dg)
        me = oe.to_mesh()
        pts += [oe.matrix_world @ v.co for v in me.vertices]
        oe.to_mesh_clear()
    if not pts:
        return
    mn = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    mx = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    c = (mn + mx) / 2
    r = (mx - mn).length / 2
    cam.data.lens = lens
    fov = cam.data.angle
    dist = r / math.sin(fov / 2) * 1.05
    d = Vector(direction).normalized()
    cam.location = c + d * dist
    cam.rotation_euler = (c - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.clip_end = dist * 4
    cam.data.clip_start = 0.01


def render_preview(cam, name, objs, **kw):
    if not PREVIEW:
        return
    vis = {o: o.hide_render for o in scene.objects}
    for o in scene.objects:
        if o.type == 'MESH':
            o.hide_render = o not in objs
    frame_camera(cam, objs, **kw)
    scene.render.filepath = os.path.join(PREVIEW, name + ".png")
    bpy.ops.render.render(write_still=True)
    for o, h in vis.items():
        o.hide_render = h


def render_fp(cam, arm, rider, action, frame, name, pitch=-10.0, fov=100):
    if not PREVIEW:
        return
    arm.animation_data.action = bpy.data.actions[action]
    scene.frame_set(frame)
    vis = {o: o.hide_render for o in scene.objects}
    for o in scene.objects:
        if o.type == 'MESH':
            o.hide_render = o is not rider and o.name not in ("FP_Ground",)
    head = arm.matrix_world @ arm.pose.bones["head"].head
    cam.location = head
    cam.rotation_euler = (math.radians(90 + pitch), 0, math.radians(-90))
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(fov)
    cam.data.clip_start = 0.02
    scene.render.resolution_x, scene.render.resolution_y = 480, 270
    scene.render.filepath = os.path.join(PREVIEW, name + ".png")
    bpy.ops.render.render(write_still=True)
    scene.render.resolution_x = scene.render.resolution_y = 320
    cam.data.sensor_fit = 'AUTO'
    for o, h in vis.items():
        o.hide_render = h
    arm.animation_data.action = None


# ============================================================================ main
def main():
    make_cube(); make_plane(); make_cylinder(); make_sky()
    make_bollard(); make_curb(); make_awning(); make_awning_folded(); make_railing(); make_pole()
    make_vending(); make_ac(); make_crate(); make_planter(); make_lantern(); make_sign_vertical()
    make_road_sign(); make_water_tank(); make_vent()
    make_frontage_bays()
    make_ring("SM_Ring", "Ring"); make_ring("SM_RingBroken", "RingBroken")
    make_car()
    arm, rider = make_rider()

    for ob in STATIC:
        export_fbx([ob], os.path.join(OUT_DIR, ob.name + ".fbx"))
    scene.frame_start, scene.frame_end = 0, 60
    export_fbx([arm, rider], os.path.join(OUT_DIR, "SK_PogoRider.fbx"), anim=True)

    if PREVIEW:
        cam = setup_preview()
        for ob in STATIC:
            if ob.name in ("SM_SkyDome",):
                continue
            render_preview(cam, ob.name, [ob])
        render_preview(cam, "SK_PogoRider_side", [rider], direction=(0.2, 1.0, 0.25))
        # suelo para las vistas en primera persona
        b = Builder("FP_Ground", density=0.5)
        b.box((2, 0, -0.05), (12, 12, 0.1), "Paving")
        gnd = b.build()
        render_fp(cam, arm, rider, "Compress", 0, "FP_neutral_fwd", pitch=-8)
        render_fp(cam, arm, rider, "Compress", 30, "FP_compress_fwd", pitch=-8)
        render_fp(cam, arm, rider, "Compress", 0, "FP_neutral_down", pitch=-70)
        render_fp(cam, arm, rider, "Compress", 30, "FP_compress_down", pitch=-70)
        render_fp(cam, arm, rider, "LetGo", 12, "FP_letgo_12", pitch=-30)
        render_fp(cam, arm, rider, "LetGo", 45, "FP_letgo_45", pitch=-45)
        arm.animation_data.action = bpy.data.actions["LetGo"]
        scene.frame_set(45)
        render_preview(cam, "SK_PogoRider_letgo_side", [rider], direction=(0.2, 1.0, 0.25))
        arm.animation_data.action = None
        bpy.data.objects.remove(gnd)

    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT_DIR, "NoSleepPogo_Assets.blend"), compress=True)
    print("ASSETS_DONE", sorted(os.listdir(OUT_DIR)))


main()
