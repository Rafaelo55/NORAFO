"""
No Sleep Pogo - pipeline de importacion para Unreal Engine 5.8 (commandlet de Python).

  UnrealEditor-Cmd.exe NoSleepPogo.uproject -run=pythonscript -script="Tools/unreal/import_assets.py" -unattended

Reconstruye desde cero /Game/Pogo y /Game/Maps a partir de SourceArt/:
  - Texturas (filtro Nearest, sin compresion) -> /Game/Pogo/Textures
  - Audio (WAV) -> /Game/Pogo/Audio
  - Materiales maestros PS1 + instancias -> /Game/Pogo/Materials
  - Mallas estaticas / esqueletica + animaciones -> /Game/Pogo/Meshes, /Game/Pogo/Characters
  - Mapa vacio L_City (el GameMode construye cada circuito en C++)
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "SourceArt")
EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()

G_TEX = "/Game/Pogo/Textures"
G_AUD = "/Game/Pogo/Audio"
G_MAT = "/Game/Pogo/Materials"
G_MESH = "/Game/Pogo/Meshes"
G_CHAR = "/Game/Pogo/Characters"
G_MAP = "/Game/Maps"

LOOPING = {"AMB_City", "MUS_Loop", "SFX_CarLoop"}
UNLIT = {"Ring", "RingBroken", "Sky", "EyeGlow"}
NO_COLLISION = {"SM_Awning", "SM_Ring", "SM_RingBroken", "SM_Plane", "SM_SkyDome", "SM_Lantern", "SM_SignVertical",
                "SM_ShopBay", "SM_WindowBay"}


def log(msg):
    unreal.log("POGO_IMPORT: " + str(msg))


def run_tasks(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.automated = True
        t.save = False
        t.replace_existing = True
        tasks.append(t)
    AT.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        out += list(t.imported_object_paths)
    return out


def clean():
    for d in ("/Game/Pogo", G_MAP):
        if EAL.does_directory_exist(d):
            EAL.delete_directory(d)
            log("borrado " + d)


# --------------------------------------------------------------------------------------- texturas
def import_textures():
    d = os.path.join(SRC, "Textures")
    files = sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".png"))
    run_tasks(files, G_TEX)
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        tex = unreal.load_asset("%s/%s" % (G_TEX, name))
        if not tex:
            log("FALTA textura " + name)
            continue
        tex.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        tex.set_editor_property("never_stream", True)
        if name == "T_Bayer":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        if name in ("T_Ring", "T_RingBroken", "T_EyeMosaic", "T_EyeGlow"):
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        if name in ("T_Ring", "T_RingBroken"):
            tex.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
            tex.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    log("texturas: %d" % len(files))


# --------------------------------------------------------------------------------------- audio
def import_audio():
    d = os.path.join(SRC, "Audio")
    files = sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".wav"))
    run_tasks(files, G_AUD)
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        s = unreal.load_asset("%s/%s" % (G_AUD, name))
        if s and name in LOOPING:
            s.set_editor_property("looping", True)
    log("audio: %d" % len(files))


# --------------------------------------------------------------------------------------- materiales
def expr(mat, cls, x, y, **props):
    e = MEL.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, a_out, b, b_in):
    ok = MEL.connect_material_expressions(a, a_out, b, b_in)
    if not ok:
        log("WARN no se pudo conectar %s.%s -> %s.%s" % (a.get_name(), a_out, b.get_name(), b_in))


def new_material(name):
    return AT.create_asset(name, G_MAT, unreal.Material, unreal.MaterialFactoryNew())


def add_jitter(mat):
    """Temblor de vertices estilo PS1: ajusta la posicion a una grilla relativa a la camara
    cuyo paso crece con la distancia (aproxima el redondeo en espacio de pantalla)."""
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -1400, 600,
              world_position_shader_offset=unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    cam = expr(mat, unreal.MaterialExpressionCameraPositionWS, -1400, 760)
    jit = expr(mat, unreal.MaterialExpressionScalarParameter, -1400, 880, parameter_name="Jitter", default_value=0.0035)
    rel = expr(mat, unreal.MaterialExpressionSubtract, -1200, 640)
    link(wp, "", rel, "A"); link(cam, "", rel, "B")
    dist = expr(mat, unreal.MaterialExpressionDistance, -1200, 780)
    link(wp, "", dist, "A"); link(cam, "", dist, "B")
    grid = expr(mat, unreal.MaterialExpressionMultiply, -1050, 820)
    link(dist, "", grid, "A"); link(jit, "", grid, "B")
    gmax = expr(mat, unreal.MaterialExpressionMax, -900, 820, const_b=0.05)
    link(grid, "", gmax, "A")
    div = expr(mat, unreal.MaterialExpressionDivide, -900, 660)
    link(rel, "", div, "A"); link(gmax, "", div, "B")
    rnd = expr(mat, unreal.MaterialExpressionRound, -750, 660)
    link(div, "", rnd, "")
    back = expr(mat, unreal.MaterialExpressionMultiply, -600, 700)
    link(rnd, "", back, "A"); link(gmax, "", back, "B")
    off = expr(mat, unreal.MaterialExpressionSubtract, -450, 700)
    link(back, "", off, "A"); link(rel, "", off, "B")
    MEL.connect_material_property(off, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def surface_common(mat, color_node, x=-300):
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, x - 300, -150, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = expr(mat, unreal.MaterialExpressionMultiply, x, -50)
    link(color_node, "RGB", mul, "A"); link(tint, "", mul, "B")
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    em = expr(mat, unreal.MaterialExpressionScalarParameter, x - 300, 150, parameter_name="Emissive", default_value=0.0)
    emm = expr(mat, unreal.MaterialExpressionMultiply, x, 150)
    link(mul, "", emm, "A"); link(em, "", emm, "B")
    MEL.connect_material_property(emm, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = expr(mat, unreal.MaterialExpressionScalarParameter, x - 300, 300, parameter_name="Roughness", default_value=0.95)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionScalarParameter, x - 300, 400, parameter_name="Specular", default_value=0.15)
    MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)


def set_usage(mat):
    for p in ("used_with_skeletal_mesh", "used_with_instanced_static_meshes", "used_with_static_lighting"):
        try:
            mat.set_editor_property(p, True)
        except Exception as e:  # noqa
            log("WARN usage %s %s" % (p, e))


def make_materials():
    white = unreal.load_asset(G_TEX + "/T_White")
    # ---- M_PS1: iluminado, UV del modelo
    m = new_material("M_PS1")
    m.set_editor_property("two_sided", True)
    set_usage(m)
    uvs = expr(m, unreal.MaterialExpressionScalarParameter, -1200, -300, parameter_name="UVScale", default_value=1.0)
    tc = expr(m, unreal.MaterialExpressionTextureCoordinate, -1200, -420)
    tcm = expr(m, unreal.MaterialExpressionMultiply, -1000, -380)
    link(tc, "", tcm, "A"); link(uvs, "", tcm, "B")
    tex = expr(m, unreal.MaterialExpressionTextureSampleParameter2D, -800, -300, parameter_name="Tex", texture=white)
    link(tcm, "", tex, "UVs")
    surface_common(m, tex)
    add_jitter(m)
    MEL.recompile_material(m)

    # ---- M_PS1_World: iluminado, UV alineadas al mundo (para bloques de nivel escalados)
    w = new_material("M_PS1_World")
    w.set_editor_property("two_sided", False)
    set_usage(w)
    wp = expr(w, unreal.MaterialExpressionWorldPosition, -1900, -500)
    tile = expr(w, unreal.MaterialExpressionScalarParameter, -1900, -360, parameter_name="TileSize", default_value=200.0)
    wps = expr(w, unreal.MaterialExpressionDivide, -1700, -460)
    link(wp, "", wps, "A"); link(tile, "", wps, "B")
    nrm = expr(w, unreal.MaterialExpressionVertexNormalWS, -1900, -200)
    an = expr(w, unreal.MaterialExpressionAbs, -1700, -200)
    link(nrm, "", an, "")
    xy = expr(w, unreal.MaterialExpressionComponentMask, -1500, -560, r=True, g=True, b=False, a=False)
    yz = expr(w, unreal.MaterialExpressionComponentMask, -1500, -460, r=False, g=True, b=True, a=False)
    xz = expr(w, unreal.MaterialExpressionComponentMask, -1500, -360, r=True, g=False, b=True, a=False)
    for c in (xy, yz, xz):
        link(wps, "", c, "")
    flip = expr(w, unreal.MaterialExpressionConstant2Vector, -1500, -260, r=1.0, g=-1.0)
    yzf = expr(w, unreal.MaterialExpressionMultiply, -1300, -460)
    link(yz, "", yzf, "A"); link(flip, "", yzf, "B")
    xzf = expr(w, unreal.MaterialExpressionMultiply, -1300, -360)
    link(xz, "", xzf, "A"); link(flip, "", xzf, "B")
    mx = expr(w, unreal.MaterialExpressionComponentMask, -1500, -160, r=True, g=False, b=False, a=False)
    my = expr(w, unreal.MaterialExpressionComponentMask, -1500, -80, r=False, g=True, b=False, a=False)
    mz = expr(w, unreal.MaterialExpressionComponentMask, -1500, 0, r=False, g=False, b=True, a=False)
    for c in (mx, my, mz):
        link(an, "", c, "")
    # redondear la mascara para que solo una proyeccion gane
    def rnd(node, y):
        r = expr(w, unreal.MaterialExpressionRound, -1350, y)
        link(node, "", r, "")
        return r
    rx, ry, rz = rnd(mx, -160), rnd(my, -80), rnd(mz, 0)
    a1 = expr(w, unreal.MaterialExpressionMultiply, -1100, -560); link(xy, "", a1, "A"); link(rz, "", a1, "B")
    a2 = expr(w, unreal.MaterialExpressionMultiply, -1100, -460); link(yzf, "", a2, "A"); link(rx, "", a2, "B")
    a3 = expr(w, unreal.MaterialExpressionMultiply, -1100, -360); link(xzf, "", a3, "A"); link(ry, "", a3, "B")
    s1 = expr(w, unreal.MaterialExpressionAdd, -950, -500); link(a1, "", s1, "A"); link(a2, "", s1, "B")
    s2 = expr(w, unreal.MaterialExpressionAdd, -850, -450); link(s1, "", s2, "A"); link(a3, "", s2, "B")
    wtex = expr(w, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300, parameter_name="Tex", texture=white)
    link(s2, "", wtex, "UVs")
    surface_common(w, wtex)
    add_jitter(w)
    MEL.recompile_material(w)

    # ---- M_PS1_Unlit: sin iluminacion, enmascarado (anillo, cielo, brillos)
    u = new_material("M_PS1_Unlit")
    u.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    u.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    u.set_editor_property("two_sided", True)
    set_usage(u)
    utc = expr(u, unreal.MaterialExpressionTextureCoordinate, -1100, -300)
    uuv = expr(u, unreal.MaterialExpressionScalarParameter, -1100, -180, parameter_name="UVScale", default_value=1.0)
    um = expr(u, unreal.MaterialExpressionMultiply, -900, -260)
    link(utc, "", um, "A"); link(uuv, "", um, "B")
    utex = expr(u, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300, parameter_name="Tex", texture=white)
    link(um, "", utex, "UVs")
    ut = expr(u, unreal.MaterialExpressionVectorParameter, -700, -50, parameter_name="Tint", default_value=unreal.LinearColor(1, 1, 1, 1))
    ub = expr(u, unreal.MaterialExpressionScalarParameter, -700, 80, parameter_name="Brightness", default_value=1.0)
    m1 = expr(u, unreal.MaterialExpressionMultiply, -450, -200); link(utex, "RGB", m1, "A"); link(ut, "", m1, "B")
    m2 = expr(u, unreal.MaterialExpressionMultiply, -300, -150); link(m1, "", m2, "A"); link(ub, "", m2, "B")
    MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    op = expr(u, unreal.MaterialExpressionScalarParameter, -700, 200, parameter_name="Opacity", default_value=1.0)
    m3 = expr(u, unreal.MaterialExpressionMultiply, -450, 150); link(utex, "A", m3, "A"); link(op, "", m3, "B")
    MEL.connect_material_property(m3, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(u)

    # ---- M_PP_PS1: post-proceso, color de 15 bits con dithering ordenado
    pp = new_material("M_PP_PS1")
    pp.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    pp.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
    st = expr(pp, unreal.MaterialExpressionSceneTexture, -1000, -200, scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    stc = expr(pp, unreal.MaterialExpressionComponentMask, -800, -200, r=True, g=True, b=True, a=False)
    link(st, "Color", stc, "")
    spos = expr(pp, unreal.MaterialExpressionScreenPosition, -1000, 50)
    bdiv = expr(pp, unreal.MaterialExpressionDivide, -800, 50, const_b=4.0)
    link(spos, "PixelPosition", bdiv, "A")
    bay = expr(pp, unreal.MaterialExpressionTextureSample, -600, 50, texture=unreal.load_asset(G_TEX + "/T_Bayer"),
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(bdiv, "", bay, "UVs")
    lv = expr(pp, unreal.MaterialExpressionScalarParameter, -800, -380, parameter_name="Levels", default_value=31.0)
    sc = expr(pp, unreal.MaterialExpressionMultiply, -600, -250); link(stc, "", sc, "A"); link(lv, "", sc, "B")
    bsub = expr(pp, unreal.MaterialExpressionSubtract, -450, 50, const_b=0.5); link(bay, "R", bsub, "A")
    ds = expr(pp, unreal.MaterialExpressionScalarParameter, -450, 180, parameter_name="DitherStrength", default_value=0.85)
    bmul = expr(pp, unreal.MaterialExpressionMultiply, -300, 80); link(bsub, "", bmul, "A"); link(ds, "", bmul, "B")
    add = expr(pp, unreal.MaterialExpressionAdd, -300, -200); link(sc, "", add, "A"); link(bmul, "", add, "B")
    half = expr(pp, unreal.MaterialExpressionAdd, -200, -200, const_b=0.5); link(add, "", half, "A")
    fl = expr(pp, unreal.MaterialExpressionFloor, -100, -200); link(half, "", fl, "")
    dv = expr(pp, unreal.MaterialExpressionDivide, 50, -200); link(fl, "", dv, "A"); link(lv, "", dv, "B")
    MEL.connect_material_property(dv, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(pp)
    log("materiales maestros listos")
    return m, w, u


def make_instances(m_lit, m_unlit):
    tex_names = [a.asset_name for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(G_TEX)]
    count = 0
    for tn in sorted(str(n) for n in tex_names):
        if not tn.startswith("T_") or tn == "T_Bayer":
            continue
        name = tn[2:]
        parent = m_unlit if name in UNLIT else m_lit
        mi = AT.create_asset("MI_" + name, G_MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, parent)
        MEL.set_material_instance_texture_parameter_value(mi, "Tex", unreal.load_asset("%s/%s" % (G_TEX, tn)))
        if name == "EyeGlow":
            MEL.set_material_instance_scalar_parameter_value(mi, "Brightness", 3.0)
        if name == "Sky":
            MEL.set_material_instance_scalar_parameter_value(mi, "Brightness", 1.0)
        if name in ("Ring", "RingBroken"):
            MEL.set_material_instance_scalar_parameter_value(mi, "Brightness", 1.6)
        if name in ("PogoRed", "MetalGray", "SpringMetal", "CarBody"):
            MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.55)
            MEL.set_material_instance_scalar_parameter_value(mi, "Specular", 0.4)
        if name == "CarLights":
            MEL.set_material_instance_scalar_parameter_value(mi, "Emissive", 0.6)
        if name in ("Skin", "Grip", "Denim", "Sneaker", "PogoRed", "SpringMetal", "Rubber", "White"):
            # el arma en primera persona no tiembla (queda estable en pantalla)
            MEL.set_material_instance_scalar_parameter_value(mi, "Jitter", 0.0)
        count += 1
    log("instancias de material: %d" % count)


# --------------------------------------------------------------------------------------- mallas
def assign_static_materials(sm):
    mats = sm.get_editor_property("static_materials")
    for i, sl in enumerate(mats):
        slot = str(sl.material_slot_name)
        mi = unreal.load_asset("%s/MI_%s" % (G_MAT, slot))
        if mi:
            sm.set_material(i, mi)
        else:
            log("WARN sin MI para slot %s en %s" % (slot, sm.get_name()))


def import_meshes():
    d = os.path.join(SRC, "Models")
    static = sorted(os.path.join(d, f) for f in os.listdir(d) if f.startswith("SM_") and f.endswith(".fbx"))
    imported = run_tasks(static, G_MESH)
    sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    if sme is None:  # en commandlet no hay subsistemas de editor: usar la libreria clasica
        sme = unreal.EditorStaticMeshLibrary
    for f in static:
        name = os.path.splitext(os.path.basename(f))[0]
        sm = unreal.load_asset("%s/%s" % (G_MESH, name))
        if not sm:
            log("FALTA malla " + name)
            continue
        assign_static_materials(sm)
        try:
            ns = sm.get_editor_property("nanite_settings")
            ns.enabled = False
            sm.set_editor_property("nanite_settings", ns)
        except Exception as e:  # noqa
            log("WARN nanite %s" % e)
        sme.remove_collisions(sm)
        if name not in NO_COLLISION:
            sme.add_simple_collisions(sm, unreal.ScriptCollisionShapeType.BOX)
        b = sm.get_bounding_box()
        log("malla %s bounds min=(%.0f,%.0f,%.0f) max=(%.0f,%.0f,%.0f)" % (name, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z))
    # borrar materiales creados automaticamente por el importador
    for p in imported:
        a = unreal.load_asset(p)
        if isinstance(a, (unreal.MaterialInstanceConstant, unreal.Material)) and p.startswith(G_MESH):
            EAL.delete_asset(p.split(".")[0])

    # ---- esqueletica
    imported = run_tasks([os.path.join(d, "SK_PogoRider.fbx")], G_CHAR)
    log("esqueletica importada: %s" % imported)
    skm = unreal.load_asset(G_CHAR + "/SK_PogoRider")
    mats = skm.get_editor_property("materials")
    new = []
    for sl in mats:
        slot = str(sl.get_editor_property("material_slot_name"))
        mi = unreal.load_asset("%s/MI_%s" % (G_MAT, slot))
        if mi:
            sl.set_editor_property("material_interface", mi)
        new.append(sl)
    skm.set_editor_property("materials", new)
    for p in imported:
        a = unreal.load_asset(p)
        if isinstance(a, unreal.AnimSequence):
            short = p.split(".")[0]
            action = short.split("_")[-1]
            target = "%s/A_Rider_%s" % (G_CHAR, action)
            EAL.rename_asset(short, target)
            log("anim %s -> %s" % (short, target))
        elif isinstance(a, (unreal.MaterialInstanceConstant, unreal.Material)):
            EAL.delete_asset(p.split(".")[0])
    skel = unreal.load_asset(G_CHAR + "/SK_PogoRider_Skeleton")
    if skel:
        try:
            names = [str(n) for n in unreal.AnimationLibrary.get_animation_track_names(unreal.load_asset(G_CHAR + "/A_Rider_Compress"))]
            log("huesos animados: %s" % names)
        except Exception as e:  # noqa
            log("WARN huesos %s" % e)
    b = skm.get_bounds()
    log("rider bounds origin=%s extent=%s" % (b.origin, b.box_extent))


def purge_importer_leftovers():
    """El importador crea copias de texturas/materiales junto a las mallas: se eliminan."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in (G_MESH, G_CHAR):
        for a in reg.get_assets_by_path(folder, recursive=True):
            cls = str(a.asset_class_path.asset_name)
            if cls in ("Texture2D", "MaterialInstanceConstant", "Material"):
                EAL.delete_asset(str(a.package_name))
                log("borrado sobrante %s (%s)" % (a.package_name, cls))


# --------------------------------------------------------------------------------------- mapa
def make_map():
    world = AT.create_asset("L_City", G_MAP, unreal.World, unreal.WorldFactory())
    log("mapa %s" % world)


def main():
    clean()
    import_textures()
    import_audio()
    m, w, u = make_materials()
    make_instances(m, u)
    import_meshes()
    purge_importer_leftovers()
    make_map()
    EAL.save_directory("/Game/Pogo", only_if_is_dirty=False, recursive=True)
    EAL.save_directory(G_MAP, only_if_is_dirty=False, recursive=True)
    log("IMPORT_DONE")


main()
