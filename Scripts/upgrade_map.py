"""Build the task-owned Depot map in UE Editor (never changes RaidYard).

Geometry: installed Epic Engine/BasicShapes, assembled in centimetres.
Optional CC0 PBR sources: SourceArt/Depot/texture_manifest.json, schema:
{"asphalt": {"basecolor": "relative/or/absolute.png", "roughness": "...",
             "normal": "..."}, "concrete": {...}, "metal": {...}, "wood": {...}}
Paths are relative to the manifest. Missing sets use a world-space noise graph.
Re-running replaces only actors tagged DepotGenerated in /Game/Maps/Depot.
"""
import json
import math
import random
from pathlib import Path

import unreal

MAP = "/Game/Maps/Depot"
TAG = "DepotGenerated"
MATERIAL_DIR = "/Game/Materials/Depot"
TEXTURE_DIR = "/Game/Textures/Depot"
PROJECT = Path(unreal.Paths.project_dir()).resolve()
MANIFEST = PROJECT / "SourceArt" / "Depot" / "texture_manifest.json"
assets = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matlib = unreal.MaterialEditingLibrary
RNG = random.Random(72026)
COUNTS = {"solid": 0, "detail": 0, "gameplay": 0}

for directory in ["/Game/Maps", MATERIAL_DIR, TEXTURE_DIR]:
    assets.make_directory(directory)


def load(path):
    # Commandlets do not necessarily scan Engine assets into the Asset Registry.
    # Native object loading resolves the installed package on disk directly.
    if path.startswith("/Engine/"):
        object_path = path if "." in path.rsplit("/", 1)[-1] else path + "." + path.rsplit("/", 1)[-1]
        result = unreal.load_asset(object_path)
        assert result, "Installed Engine asset failed to load: " + object_path
        return result
    return assets.load_asset(path) if assets.does_asset_exist(path) else None


def texture_sets():
    """Import local sources without account, download or license side effects."""
    if not MANIFEST.is_file():
        unreal.log("DEPOT_TEXTURES: no manifest; world-space procedural fallback")
        return {}
    specs = json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
    result = {}
    for family, maps in specs.items():
        if not isinstance(maps, dict):
            continue
        target = TEXTURE_DIR + "/" + family
        assets.make_directory(target)
        result[family] = {}
        for channel in ("basecolor", "roughness", "normal"):
            filename = maps.get(channel)
            if not filename:
                continue
            source = Path(filename)
            if not source.is_absolute():
                source = MANIFEST.parent / source
            assert source.is_file(), "Missing texture source: " + str(source)
            name = "T_D_" + family + "_" + channel
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", str(source))
            task.set_editor_property("destination_path", target)
            task.set_editor_property("destination_name", name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            asset_tools.import_asset_tasks([task])
            tex = load(target + "/" + name)
            assert tex, "Texture import failed: " + name
            tex.set_editor_property("srgb", channel == "basecolor")
            if channel == "normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            elif channel == "roughness":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            assert assets.save_loaded_asset(tex), "Texture save failed: " + name
            result[family][channel] = tex
        unreal.log("DEPOT_TEXTURES: imported " + family + " " + str(list(result[family])))
    return result


TEXTURES = texture_sets()
WORLD_TEXTURE = load("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture")
WORLD_NORMAL = load("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal")
assert WORLD_TEXTURE and WORLD_NORMAL, "Installed WorldAligned material functions required"


def node(mat, cls, x, y, **properties):
    n = matlib.create_material_expression(mat, cls, x, y)
    assert n, "Material node creation failed: " + cls.__name__
    for key, val in properties.items():
        n.set_editor_property(key, val)
    return n


def scalar(mat, value, x=-500, y=400):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=value)


def color(mat, rgb, x=-800, y=0):
    return node(mat, unreal.MaterialExpressionConstant3Vector, x, y,
                constant=unreal.LinearColor(*rgb, 1.0))


def link(a, b, input_name, output=""):
    inputs = [str(value) for value in matlib.get_material_expression_input_names(b)]
    outputs = [str(value) for value in matlib.get_material_expression_output_names(a)]
    compact = lambda value: "".join(c for c in str(value).lower() if c.isalnum())
    actual_input = input_name
    if input_name:
        matches = [name for name in inputs if compact(name) == compact(input_name)]
        if matches:
            actual_input = matches[0]
        elif len(inputs) == 1:
            # ComponentMask and other single-input nodes often report an unnamed pin.
            # An empty name is the documented ConnectMaterialExpressions first-pin default.
            actual_input = inputs[0]
        else:
            unreal.log_error("DEPOT_MATERIAL_PINS: requested input=" + input_name +
                             " actual inputs=" + str(inputs) + " source outputs=" + str(outputs))
            raise AssertionError("Material input not found: " + input_name)
    actual_output = output
    if output:
        matches = [name for name in outputs if compact(name) == compact(output)]
        if matches:
            actual_output = matches[0]
    connected = matlib.connect_material_expressions(a, actual_output, b, actual_input)
    if not connected:
        unreal.log_error("DEPOT_MATERIAL_PINS: requested=" + str((input_name, output)) +
                         " resolved=" + str((actual_input, actual_output)) +
                         " destination inputs=" + str(inputs) + " source outputs=" + str(outputs))
    assert connected, "Material connection failed: " + actual_input + " / " + actual_output


def output(a, prop, pin=""):
    assert matlib.connect_material_property(a, pin, prop), "Material output connection failed"


def function_pin(n, requested, output_pin=False):
    names = (matlib.get_material_expression_output_names(n) if output_pin else
             matlib.get_material_expression_input_names(n))
    compact = lambda value: "".join(c for c in str(value).lower() if c.isalnum())
    for name in names:
        if compact(name) == compact(requested):
            return str(name)
    raise RuntimeError("Missing material function pin " + requested + " in " + str(names))


def world_map(mat, tex, normal=False, tile_cm=240, row=0):
    sampler = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
               unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if tex.get_editor_property("srgb") else
               unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    obj = node(mat, unreal.MaterialExpressionTextureObject, -1200, row,
               texture=tex, sampler_type=sampler)
    fn = node(mat, unreal.MaterialExpressionMaterialFunctionCall, -750, row)
    assert fn.set_material_function(WORLD_NORMAL if normal else WORLD_TEXTURE)
    link(obj, fn, function_pin(fn, "TextureObject"))
    size = color(mat, (tile_cm, tile_cm, tile_cm), -1200, row + 150)
    link(size, fn, function_pin(fn, "TextureSize"))
    return fn, function_pin(fn, "XYZ Texture", True)


def material(name, rgb, rough=0.8, metal=0.0, family=None, tile=240, glow=0.0):
    path = MATERIAL_DIR + "/" + name
    mat = load(path)
    if not mat:
        mat = asset_tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    assert mat
    # This namespace is generated by this script; rebuilding permits texture upgrades.
    matlib.delete_all_material_expressions(mat)
    maps = TEXTURES.get(family, {})
    tint = color(mat, rgb)
    if "basecolor" in maps:
        surface, pin = world_map(mat, maps["basecolor"], tile_cm=tile)
    else:
        surface = node(mat, unreal.MaterialExpressionNoise, -1050, -240,
                       scale=0.035, quality=1, levels=2, output_min=0.68, output_max=1.0)
        pin = ""
    tinting = node(mat, unreal.MaterialExpressionMultiply, -350, 0)
    link(surface, tinting, "A", pin)
    link(tint, tinting, "B")
    output(tinting, unreal.MaterialProperty.MP_BASE_COLOR)
    if "roughness" in maps:
        rnode, rpin = world_map(mat, maps["roughness"], tile_cm=tile, row=420)
        channel = node(mat, unreal.MaterialExpressionComponentMask, -300, 460,
                       r=True, g=False, b=False, a=False)
        link(rnode, channel, "Input", rpin)
        output(channel, unreal.MaterialProperty.MP_ROUGHNESS)
    else:
        output(scalar(mat, rough, -300, 450), unreal.MaterialProperty.MP_ROUGHNESS)
    output(scalar(mat, metal, -300, 610), unreal.MaterialProperty.MP_METALLIC)
    if "normal" in maps:
        normals, npin = world_map(mat, maps["normal"], True, tile, row=850)
        # Epic's WorldAlignedNormal wrapper converts World to Tangent internally.
        mat.set_editor_property("tangent_space_normal", True)
        output(normals, unreal.MaterialProperty.MP_NORMAL, npin)
    if glow:
        emit = node(mat, unreal.MaterialExpressionMultiply, -100, 180)
        link(tint, emit, "A")
        link(scalar(mat, glow, -350, 220), emit, "B")
        output(emit, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    errors = matlib.recompile_material(mat)
    assert not errors, name + " material compile errors: " + str(errors)
    assert assets.save_loaded_asset(mat), "Failed to save " + name
    return mat


MATS = {
    "asphalt": material("M_D_Asphalt", (0.32, 0.36, 0.38), 0.92, family="asphalt", tile=300),
    "concrete": material("M_D_Concrete", (0.62, 0.64, 0.62), 0.85, family="concrete", tile=210),
    "light_concrete": material("M_D_LightConcrete", (0.8, 0.79, 0.71), 0.9, family="concrete", tile=210),
    "blue": material("M_D_BlueSteel", (0.12, 0.31, 0.37), 0.63, 0.35, "metal", 150),
    "olive": material("M_D_OliveSteel", (0.29, 0.34, 0.21), 0.72, 0.25, "metal", 150),
    "red": material("M_D_RustSteel", (0.51, 0.19, 0.10), 0.73, 0.4, "metal", 150),
    "steel": material("M_D_Galvanized", (0.47, 0.53, 0.53), 0.45, 0.75, "metal", 180),
    "dark": material("M_D_DarkSteel", (0.08, 0.1, 0.105), 0.65, 0.55, "metal", 180),
    "wood": material("M_D_Wood", (0.73, 0.52, 0.30), 0.84, family="wood", tile=120),
    "yellow": material("M_D_SafetyYellow", (0.87, 0.57, 0.08), 0.78),
    "white": material("M_D_RoadPaint", (0.78, 0.80, 0.68), 0.92),
    "stain": material("M_D_OilWear", (0.10, 0.12, 0.13), 0.6),
    "rubber": material("M_D_Rubber", (0.045, 0.052, 0.055), 0.91),
    "green": material("M_D_ExtractionLight", (0.04, 0.75, 0.21), 0.45, glow=2.0),
    "lamp": material("M_D_WarmLamp", (1.0, 0.68, 0.32), 0.4, glow=3.0),
}

if assets.does_asset_exist(MAP):
    assert levels.load_level(MAP), "Could not load existing Depot"
    for actor in actors.get_all_level_actors():
        if TAG in [str(tag) for tag in actor.get_editor_property("tags")]:
            assert actors.destroy_actor(actor), "Could not replace generated actor"
else:
    assert levels.new_level(MAP), "Could not create Depot"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
game_class = assets.load_blueprint_class("/Game/Blueprints/BP_RaidGameMode")
assert game_class, "Run create_map.py first to create gameplay Blueprint subclasses"
world.get_world_settings().set_editor_property("default_game_mode", game_class)
MESHES = {name: load("/Engine/BasicShapes/" + name) for name in ("Cube", "Cylinder", "Sphere", "Cone")}
assert all(MESHES.values())


def tag_actor(actor, label, folder):
    assert actor, "Actor spawn failed: " + label
    actor.set_actor_label(label)
    actor.set_editor_property("tags", [unreal.Name(TAG)])
    actor.set_folder_path(unreal.Name("Depot/" + folder))
    return actor


def shape(label, xyz, size, mat="steel", mesh="Cube", yaw=0, pitch=0, roll=0,
          solid=False, folder="Details", shadow=True):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz),
                unreal.Rotator(pitch=pitch, yaw=yaw, roll=roll))
    tag_actor(actor, label, folder)
    comp = actor.static_mesh_component
    comp.set_static_mesh(MESHES[mesh])
    comp.set_material(0, MATS[mat])
    comp.set_mobility(unreal.ComponentMobility.STATIC)
    comp.set_collision_profile_name("BlockAll" if solid else "NoCollision")
    comp.set_editor_property("can_ever_affect_navigation", solid)
    comp.set_cast_shadow(shadow)
    actor.set_actor_scale3d(unreal.Vector(*(v / 100.0 for v in size)))
    COUNTS["solid" if solid else "detail"] += 1
    return actor


def box(label, xyz, size, mat="steel", **kwargs):
    return shape(label, xyz, size, mat, **kwargs)


def cylinder(label, xyz, diameter, height, mat="steel", **kwargs):
    return shape(label, xyz, (diameter, diameter, height), mat, mesh="Cylinder", **kwargs)


def beam(label, a, b, thickness, mat="steel", **kwargs):
    delta = tuple(b[i] - a[i] for i in range(3))
    length = math.sqrt(sum(value * value for value in delta))
    mid = tuple((a[i] + b[i]) * 0.5 for i in range(3))
    yaw = math.degrees(math.atan2(delta[1], delta[0]))
    pitch = math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1])))
    return box(label, mid, (length, thickness, thickness), mat,
               yaw=yaw, pitch=pitch, **kwargs)


def sign(label, message, xyz, yaw=0, size=38, color=(230, 233, 211, 255), folder="Signs", pitch=0):
    actor = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*xyz),
                unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))
    tag_actor(actor, label, folder)
    comp = actor.get_component_by_class(unreal.TextRenderComponent)
    comp.set_text(message)
    comp.set_world_size(size)
    comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    comp.set_text_render_color(unreal.Color(*color))
    return actor


def local(origin, yaw, p):
    angle = math.radians(yaw)
    return (origin[0] + p[0] * math.cos(angle) - p[1] * math.sin(angle),
            origin[1] + p[0] * math.sin(angle) + p[1] * math.cos(angle), origin[2] + p[2])


def local_box(label, origin, yaw, p, size, mat, **kwargs):
    return box(label, local(origin, yaw, p), size, mat, yaw=yaw, **kwargs)


def container(label, x, y, length=950, width=320, height=320, yaw=0, mat="blue"):
    """Closed shipping container; all ribs/door hardware are non-blocking details."""
    origin = (x, y, 0)
    folder = "Cargo"
    local_box(label + "_Body", origin, yaw, (0, 0, height / 2), (length, width, height), mat,
              solid=True, folder=folder)
    for side in (-1, 1):
        for index in range(1, int(length / 42)):
            at = -length / 2 + index * 42
            local_box(label + "_Corrugation", origin, yaw, (at, side * (width / 2 + 3), height / 2),
                      (13, 7, height - 24), mat, folder=folder)
        for z in (12, height - 10):
            local_box(label + "_SideRail", origin, yaw, (0, side * (width / 2 + 5), z),
                      (length + 12, 12, 15), "dark", folder=folder)
    for index in range(1, int(length / 52)):
        local_box(label + "_RoofRib", origin, yaw, (-length / 2 + index * 52, 0, height + 3),
                  (8, width - 14, 6), mat, folder=folder)
    for end in (-1, 1):
        for side in (-1, 1):
            local_box(label + "_EndPost", origin, yaw, (end * length / 2, side * width / 2, height / 2),
                      (18, 18, height + 8), "dark", folder=folder)
            for z in (12, height - 12):
                local_box(label + "_CornerCasting", origin, yaw,
                          (end * length / 2, side * width / 2, z), (25, 25, 24), "steel", folder=folder)
        local_box(label + "_DoorFrame", origin, yaw, (end * (length / 2 + 5), 0, height - 10),
                  (15, width, 18), "dark", folder=folder)
    for side in (-1, 1):
        local_box(label + "_Door", origin, yaw, (length / 2 + 9, side * width / 4, height / 2),
                  (6, width / 2 - 12, height - 35), mat, folder=folder)
        for rod in (-1, 1):
            p = (length / 2 + 15, side * width / 4 + rod * 42, height / 2)
            cylinder(label + "_LockRod", local(origin, yaw, p), 5, height - 55, "steel", folder=folder)
            for z in (55, height - 55):
                local_box(label + "_Hinge", origin, yaw, (p[0] + 2, p[1], z),
                          (9, 20, 12), "steel", folder=folder)
    sign(label + "_Serial", label.upper() + " / 07", local(origin, yaw, (length / 2 + 20, 0, 255)),
         yaw=yaw, size=27, folder=folder)


def crate(label, x, y, bottom=15, size=110, height=110, yaw=0):
    origin = (x, y, bottom)
    box(label + "_Core", (x, y, bottom + height / 2), (size, size, height), "wood",
        yaw=yaw, solid=True, folder="Supplies")
    for side in (-1, 1):
        for z in (12, height - 12):
            local_box(label + "_Board", origin, yaw, (0, side * (size / 2 + 2), z),
                      (size + 4, 8, 14), "wood", folder="Supplies")
            local_box(label + "_Band", origin, yaw, (side * (size / 2 + 3), 0, z),
                      (7, size + 6, 8), "dark", folder="Supplies")
        local_box(label + "_Corner", origin, yaw, (side * (size / 2 - 10), -size / 2 - 5, height / 2),
                  (13, 6, height), "steel", folder="Supplies")
    for slat in range(5):
        local_box(label + "_PalletSlat", origin, yaw, (-size / 2 + 10 + slat * size / 5, 0, -8),
                  (size / 6, size + 16, 10), "wood", folder="Supplies")
    for side in (-1, 1):
        local_box(label + "_PalletRunner", origin, yaw, (0, side * size / 3, -12),
                  (size + 16, 12, 8), "dark", folder="Supplies")
    return bottom + height


def barrel(label, x, y, mat="red", bottom=0):
    cylinder(label + "_Drum", (x, y, bottom + 48), 64, 96, mat, solid=True, folder="Supplies")
    for z in (4, 27, 70, 94):
        cylinder(label + "_Hoop", (x, y, bottom + z), 67, 4, "dark", folder="Supplies")
    cylinder(label + "_Lid", (x, y, bottom + 97), 63, 3, "steel", folder="Supplies")
    cylinder(label + "_Cap", (x + 15, y, bottom + 100), 9, 4, "dark", folder="Supplies")
    cylinder(label + "_SafetyBand", (x, y, bottom + 47), 65, 17, "yellow", folder="Supplies")


def barrier(label, x, y, yaw=0):
    box(label + "_JerseyBase", (x, y, 16), (250, 80, 32), "concrete", yaw=yaw,
        solid=True, folder="Cover")
    box(label + "_JerseyWall", (x, y, 69), (250, 42, 108), "light_concrete", yaw=yaw,
        solid=True, folder="Cover")
    for offset in (-90, -30, 30, 90):
        local_box(label + "_WarningStripe", (x, y, 0), yaw, (offset, -23, 88),
                  (27, 2, 38), "yellow", folder="Cover")


def fence(label, a, b, base=290, height=180):
    length = math.dist(a, b)
    sections = max(1, round(length / 280))
    at = lambda fraction, z: (a[0] + (b[0] - a[0]) * fraction,
                              a[1] + (b[1] - a[1]) * fraction, z)
    for index in range(sections + 1):
        cylinder(label + "_Post", at(index / sections, base + height / 2), 7, height + 25,
                 "steel", folder="Perimeter")
    for z in (base + 15, base + height - 15):
        beam(label + "_Rail", at(0, z), at(1, z), 3, "steel", folder="Perimeter")
    for index in range(sections):
        left, right = index / sections, (index + 1) / sections
        for offset in range(0, height, 45):
            beam(label + "_Wire", at(left, base + offset), at(right, base + min(height, offset + 75)),
                 1.4, "steel", folder="Perimeter", shadow=False)
            beam(label + "_Wire", at(left, base + min(height, offset + 75)), at(right, base + offset),
                 1.4, "steel", folder="Perimeter", shadow=False)


def pipe(label, a, b, diameter=16, mat="steel"):
    delta = tuple(b[i] - a[i] for i in range(3))
    yaw = math.degrees(math.atan2(delta[1], delta[0]))
    pitch = math.degrees(math.atan2(delta[2], math.hypot(delta[0], delta[1])))
    length = math.dist(a, b)
    # Cylinder axis is local Z. A pitch of forward_pitch-90 maps it to the line.
    cylinder(label, tuple((a[i] + b[i]) / 2 for i in range(3)), diameter, length, mat,
             yaw=yaw, pitch=pitch - 90, folder="Infrastructure")
    for p in (a, b):
        shape(label + "_Joint", p, (diameter * 1.3,) * 3, mat,
              mesh="Sphere", folder="Infrastructure")


def lamp(label, x, y, height=520):
    cylinder(label + "_Foot", (x, y, 12), 46, 24, "concrete", solid=True, folder="Lighting")
    cylinder(label + "_Pole", (x, y, height / 2), 13, height, "dark", solid=True, folder="Lighting")
    beam(label + "_Arm", (x, y, height), (x + 105, y, height + 22), 9, "dark", folder="Lighting")
    box(label + "_Housing", (x + 95, y, height + 13), (80, 42, 18), "dark", folder="Lighting")
    box(label + "_Bulb", (x + 95, y, height + 2), (58, 28, 3), "lamp", folder="Lighting", shadow=False)
    light = tag_actor(actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x + 95, y, height - 12)),
                      label + "_Light", "Lighting")
    comp = light.light_component
    comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    comp.set_intensity(2300)
    comp.set_light_color(unreal.LinearColor(1.0, 0.71, 0.43, 1))
    comp.set_editor_property("attenuation_radius", 850)
    comp.set_editor_property("cast_shadows", False)


# Yard interior remains exactly 48 x 38 m. The south 8 m corridor is unobstructed.
box("Depot_Yard_Asphalt", (0, 0, -50), (4800, 3800, 100), "asphalt", solid=True, folder="Ground")
for label, p, s in [
    ("North", (0, 1950, 145), (5000, 100, 290)),
    ("South", (0, -1950, 145), (5000, 100, 290)),
    ("East", (2450, 0, 145), (100, 4000, 290)),
    ("West", (-2450, 0, 145), (100, 4000, 290)),
]:
    box("Perimeter_" + label, p, s, "concrete", solid=True, folder="Perimeter")
for a, b in [((-2400, -1900), (2400, -1900)), ((-2400, 1900), (2400, 1900)),
             ((-2400, -1900), (-2400, 1900)), ((2400, -1900), (2400, 1900))]:
    fence("PerimeterMesh", a, b)

# Texture detail in the road is supported by physical cracks, stains and repairs.
for i in range(26):
    x, y = RNG.uniform(-2100, 2100), RNG.uniform(-1750, 1750)
    cylinder("Ground_OilPatch", (x, y, 0.3), RNG.uniform(50, 130), 0.4, "stain",
             folder="Ground", shadow=False)
for i in range(48):
    box("Ground_Crack", (RNG.uniform(-2200, 2200), RNG.uniform(-1750, 1750), 0.7),
        (RNG.uniform(35, 160), RNG.uniform(1.4, 2.4), 0.3), "rubber", yaw=RNG.uniform(-180, 180),
        folder="Ground", shadow=False)
for x in range(-1900, 2250, 340):
    box("ServiceLane_Dash", (x, -1400, 1.1), (140, 13, 1), "yellow", folder="Ground", shadow=False)
for y in (-1810, -990):
    box("ServiceLane_Edge", (0, y, 1.1), (4550, 8, 1), "white", folder="Ground", shadow=False)
for x in (-1700, -200, 1350):
    box("ServiceLane_ArrowStem", (x, -1400, 1.7), (110, 14, 1), "white", folder="Ground", shadow=False)
    for yaw in (-35, 35):
        box("ServiceLane_ArrowHead", (x + 55, -1400 + (20 if yaw < 0 else -20), 1.7),
            (65, 12, 1), "white", yaw=yaw, folder="Ground", shadow=False)

# Two swung-open wire gate leaves sit beside the service corridor, never across it.
for y in (-1825, -975):
    cylinder("EntryGate_HingePost", (-2350, y, 145), 18, 290, "dark",
             solid=True, folder="Perimeter")
    fence("EntryGate_OpenLeaf", (-2350, y), (-2050, y), base=25, height=210)
    beam("EntryGate_Diagonal", (-2345, y, 30), (-2055, y, 230), 7,
         "dark", folder="Perimeter")
    box("EntryGate_Wheel", (-2060, y, 16), (24, 16, 32), "rubber", folder="Perimeter")
beam("EntryGate_Header", (-2350, -1825, 302), (-2350, -975, 302), 16,
     "dark", folder="Perimeter")
sign("EntryGate_Sign", "DEPOT  /  07", (-2338, -1400, 355), yaw=0, size=40)

container("Cargo_A", -800, -520, length=1000, mat="blue")
container("Cargo_B", 450, 580, length=950, yaw=180, mat="olive")
container("Cargo_C", 1250, -360, length=820, yaw=-90, mat="red")
for x, y, yaw in [(-1470, 340, 25), (-180, -800, 0), (670, -870, -10), (1320, 1100, 90), (100, 40, 0)]:
    barrier("JerseyCover", x, y, yaw)
for index, (x, y, yaw) in enumerate([(-1480, 620, 10), (-1550, -880, 0), (180, 960, -12),
                                     (1540, 740, 0), (520, 1720, 5)]):
    crate("SupplyCrate_%02d" % index, x, y, yaw=yaw)
for x, y in [(-1600, 460), (-1580, 550), (920, 300), (990, 300), (1560, -820), (1640, -820), (400, 1220)]:
    barrel("FuelDrum", x, y)

# North warehouse: closed industrial facade, route stays open in front of y=1300.
box("Warehouse_Structure", (-1150, 1580, 320), (1600, 560, 640), "light_concrete",
    solid=True, folder="Warehouse")
box("Warehouse_Roof", (-1150, 1580, 655), (1660, 620, 30), "dark", folder="Warehouse")
box("Warehouse_UpperCladding", (-1150, 1289, 530), (1600, 18, 195), "olive", folder="Warehouse")
for x in range(-1940, -350, 100):
    box("Warehouse_CladdingRib", (x, 1275, 530), (6, 8, 192), "steel", folder="Warehouse")
for x in (-1880, -1340, -800, -410):
    box("Warehouse_Pilaster", (x, 1280, 280), (42, 40, 560), "concrete", folder="Warehouse")
for index, x in enumerate((-1640, -1090, -580)):
    box("Warehouse_Shutter", (x, 1274, 185), (360, 10, 340), "steel", folder="Warehouse")
    for z in range(40, 350, 25):
        box("Warehouse_ShutterSlat", (x, 1266, z), (350, 8, 5), "dark", folder="Warehouse")
    for side in (-1, 1):
        box("Warehouse_ShutterRail", (x + side * 187, 1263, 185), (15, 18, 370), "dark", folder="Warehouse")
    box("Warehouse_ShutterHood", (x, 1250, 370), (402, 68, 42), "dark", folder="Warehouse")
    sign("Warehouse_BayNumber", "BAY 0%d" % (index + 1), (x, 1258, 416), yaw=-90, size=39)
    for side in (-1, 1):
        cylinder("Warehouse_Bollard", (x + side * 195, 1180, 47), 18, 94, "yellow",
                 solid=True, folder="Warehouse")
    box("Warehouse_WarningBoard", (x, 1260, 92), (80, 4, 42), "yellow", folder="Warehouse")
    sign("Warehouse_NoEntry", "SECURED", (x, 1255, 87), yaw=-90, size=15, color=(30, 35, 28, 255))
for x in (-1660, -960, -480):
    box("Warehouse_WindowFrame", (x, 1262, 546), (230, 12, 82), "dark", folder="Warehouse")
    box("Warehouse_WindowGlass", (x, 1255, 546), (210, 4, 65), "blue", folder="Warehouse")
    for offset in (-65, 0, 65):
        box("Warehouse_WindowMullion", (x + offset, 1249, 546), (5, 5, 68), "steel", folder="Warehouse")
pipe("Warehouse_WaterPipe", (-1960, 1200, 25), (-1960, 1200, 480), 18)
pipe("Warehouse_FacadePipe", (-1960, 1200, 480), (-360, 1200, 480), 18)
for x in (-1750, -1200, -650):
    box("Warehouse_PipeBracket", (x, 1240, 480), (22, 70, 10), "dark", folder="Infrastructure")
for x in (-1580, -810):
    box("Warehouse_RooftopHVAC", (x, 1520, 755), (240, 180, 180), "steel", folder="Infrastructure")
    for z in range(700, 820, 20):
        box("Warehouse_HVACLouvre", (x, 1428, z), (220, 4, 8), "dark", folder="Infrastructure")
    cylinder("Warehouse_VentOutlet", (x, 1590, 925), 85, 160, "steel", folder="Infrastructure")
    cylinder("Warehouse_VentCap", (x, 1590, 1008), 120, 8, "dark", folder="Infrastructure")

# High-value utility shelter: open sides and roofed equipment, no fake doorway collider.
for x in (420, 1050):
    for y in (1430, 1810):
        box("UtilityShelter_Post", (x, y, 220), (16, 16, 440), "steel", solid=True, folder="Utilities")
box("UtilityShelter_Roof", (735, 1620, 450), (710, 470, 22), "olive", folder="Utilities")
for x in range(410, 1090, 65):
    box("UtilityShelter_RoofRib", (x, 1620, 465), (8, 465, 7), "steel", folder="Utilities")
box("Generator_Base", (925, 1680, 25), (160, 230, 50), "dark", solid=True, folder="Utilities")
box("Generator_Housing", (925, 1680, 105), (145, 210, 140), "olive", solid=True, folder="Utilities")
for z in range(55, 165, 15):
    box("Generator_Louvre", (925, 1569, z), (116, 4, 5), "dark", folder="Utilities")
pipe("Generator_Exhaust", (968, 1690, 160), (968, 1690, 310), 12, "dark")
pipe("Generator_Cable", (925, 1800, 12), (1650, 1800, 12), 8, "rubber")
box("Utility_FuseCabinet", (2200, 340, 130), (100, 75, 260), "steel", solid=True, folder="Utilities")
box("Utility_FuseDoor", (2144, 340, 130), (5, 66, 245), "olive", folder="Utilities")
sign("Utility_Danger", "DANGER", (2139, 340, 174), yaw=180, size=19, color=(255, 195, 60, 255))
pipe("Utility_OverheadPipe", (2310, -850, 460), (2310, 950, 460), 22)
for y in (-850, 50, 950):
    box("Utility_PipeSupport", (2310, y, 220), (16, 16, 440), "steel", solid=True, folder="Utilities")
    box("Utility_PipeSaddle", (2310, y, 444), (75, 36, 10), "dark", folder="Utilities")

# Supply/search staging: reachable tabletop and floor pickups, distinct ground identity.
box("Worktable_Top", (650, 1550, 108), (160, 110, 14), "wood", solid=True, folder="Supplies")
for x in (590, 710):
    for y in (1512, 1588):
        box("Worktable_Leg", (x, y, 50), (8, 8, 100), "steel", solid=True, folder="Supplies")
for x in (550, 670, 790):
    box("Utility_ParkingLine", (x, 1390, 1.7), (80, 7, 1), "yellow", folder="Ground", shadow=False)
sign("Utility_HighValue", "UTILITY  /  RESTRICTED", (735, 1424, 360), yaw=-90, size=34,
     color=(255, 197, 85, 255))
sign("Cargo_Zone", "CARGO STACK  /  07", (-900, -697, 235), yaw=-90, size=40)
sign("North_Route", "02  /  LOADING BAYS", (-1880, 1170, 305), yaw=-90, size=31)
sign("Service_Route", "01  /  SERVICE ROAD  >", (-2375, -1350, 240), yaw=0, size=30)
sign("Mid_Route", "03  /  CARGO CROSSING", (1450, 245, 245), yaw=180, size=29)

# Extraction stays at the original coordinates for repeatable gameplay testing.
extract_class = assets.load_blueprint_class("/Game/Blueprints/BP_Extraction")
extraction = tag_actor(actors.spawn_actor_from_class(extract_class, unreal.Vector(2010, 1400, 10)),
                       "Extraction_East", "Gameplay")
extraction.get_component_by_class(unreal.StaticMeshComponent).set_material(0, MATS["green"])
for x in (1750, 2270):
    box("Extraction_Edge", (x, 1400, 2.2), (6, 520, 2), "green", folder="Extraction", shadow=False)
for y in (1140, 1660):
    box("Extraction_Edge", (2010, y, 2.2), (520, 6, 2), "green", folder="Extraction", shadow=False)
for y in (1140, 1660):
    cylinder("Extraction_Beacon", (2295, y, 125), 12, 250, "dark", folder="Extraction")
    cylinder("Extraction_BeaconBulb", (2295, y, 251), 22, 24, "green", folder="Extraction")
box("Extraction_SignBoard", (2350, 1400, 295), (12, 860, 115), "dark", folder="Extraction")
sign("Sign_Extraction", "EXTRACTION  /  08 SEC", (2339, 1400, 310), yaw=180,
     size=44, color=(85, 255, 134, 255))
sign("Sign_ExtractionSub", "SECURE AT LEAST ONE SUPPLY", (2337, 1400, 265), yaw=180,
     size=19, color=(211, 236, 211, 255))
for x, y in [(-2200, -850), (-1880, 1000), (1580, -925), (2020, 1800)]:
    lamp("Yard_Lamp", x, y)

start = tag_actor(actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-2000, -1400, 100),
                    unreal.Rotator(pitch=0, yaw=15, roll=0)), "Insertion_West", "Gameplay")
loot_class = assets.load_blueprint_class("/Game/Blueprints/BP_RaidLoot")
for index, (x, y, z, item, label, value) in enumerate([
    (-1550, -1250, 48, "Tech", "Electronics", 120),
    (-1380, 700, 48, "Medkit", "Medkit", 75),
    (-290, -1000, 48, "Rations", "Field rations", 40),
    (320, 900, 48, "Tech", "Electronics", 120),
    (700, -1100, 48, "Medkit", "Medkit", 75),
    (1660, -950, 48, "Tech", "Data drive", 180),
    (1650, 1150, 48, "Rations", "Field rations", 40),
    (1750, 900, 48, "Tech", "Electronics", 120),
    (650, 1550, 141, "Tech", "Encrypted data drive", 300),
]):
    actor = tag_actor(actors.spawn_actor_from_class(loot_class, unreal.Vector(x, y, z)),
                      "Loot_%02d_%s" % (index, item), "Gameplay")
    for key, val in [("item_id", item), ("display_name", label), ("value", value)]:
        actor.set_editor_property(key, val)
    COUNTS["gameplay"] += 1
enemy_class = assets.load_blueprint_class("/Game/Blueprints/BP_RaidEnemy")
for index, (x, y, points) in enumerate([
    (-250, 1000, [(-700, 1070, 95), (-250, 1000, 95), (250, 1100, 95)]),
    (900, -800, [(900, -800, 95), (1800, -1250, 95), (1800, 0, 95)]),
    (1550, 1300, [(1550, 1300, 95), (1800, 600, 95), (1180, 1450, 95)]),
]):
    actor = tag_actor(actors.spawn_actor_from_class(enemy_class, unreal.Vector(x, y, 96)),
                      "Guard_%02d" % (index + 1), "Gameplay")
    actor.set_editor_property("patrol_points", [unreal.Vector(*point) for point in points])
    COUNTS["gameplay"] += 1
tag_actor(actors.spawn_actor_from_class(unreal.RaidNavBounds, unreal.Vector(0, 0, 250)),
          "Navigation_Bounds", "Gameplay")

sun = tag_actor(actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 1400),
                unreal.Rotator(pitch=-38, yaw=-32, roll=0)), "Depot_Sun", "Lighting")
sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sun.light_component.set_intensity(4.0)
sun.light_component.set_light_color(unreal.LinearColor(1.0, 0.91, 0.78, 1))
tag_actor(actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)), "Depot_Atmosphere", "Lighting")
sky = tag_actor(actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600)), "Depot_Sky", "Lighting")
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_intensity(1.2)
sky.light_component.set_editor_property("real_time_capture", True)
fog = tag_actor(actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -50)),
                "Depot_Air", "Lighting")
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.004)

assert levels.save_current_level(), "Failed to save Depot map"
assert assets.save_directory(MATERIAL_DIR, only_if_is_dirty=False, recursive=True)
assert assets.save_directory(TEXTURE_DIR, only_if_is_dirty=False, recursive=True)
unreal.log("DEPOT_MAP_BUILD_COMPLETE: " + MAP + " " + str(COUNTS) + "; south service lane 800cm clear")
