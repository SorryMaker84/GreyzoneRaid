"""Original R7 industrial surface palette, using the already licensed R2 textures.

Importing this module is safe outside Unreal: it only declares data/functions.
Call build_materials() from the Frontier builder inside the coordinated editor.
All writes are restricted to /Game/Materials/R7 and Evidence/R7_VisualAssets.json.
Existing /Game/Materials/Depot and /Game/Textures/Depot assets remain unchanged.
"""
import json
from pathlib import Path

MATERIAL_DIR = "/Game/Materials/R7"
TEXTURE_DIR = "/Game/Textures/Depot"

# Keys are shared with the Frontier map builder; all dimensions are centimetres.
MATERIAL_PATHS = {
    "concrete": MATERIAL_DIR + "/M_R7_Concrete",
    "light_concrete": MATERIAL_DIR + "/M_R7_LightConcrete",
    "asphalt": MATERIAL_DIR + "/M_R7_Asphalt",
    "dirt": MATERIAL_DIR + "/M_R7_Dirt",
    "blue": MATERIAL_DIR + "/M_R7_BlueSteel",
    "olive": MATERIAL_DIR + "/M_R7_OliveSteel",
    "rust": MATERIAL_DIR + "/M_R7_RustSteel",
    "steel": MATERIAL_DIR + "/M_R7_GalvanizedSteel",
    "dark": MATERIAL_DIR + "/M_R7_DarkSteel",
    "wood": MATERIAL_DIR + "/M_R7_Wood",
    "yellow": MATERIAL_DIR + "/M_R7_SafetyYellow",
    "white": MATERIAL_DIR + "/M_R7_RoadPaint",
    "rubber": MATERIAL_DIR + "/M_R7_Rubber",
    "green": MATERIAL_DIR + "/M_R7_ExtractionLight",
    "warm_lamp": MATERIAL_DIR + "/M_R7_WarmLamp",
    "plaster": MATERIAL_DIR + "/M_R7_InteriorPlaster",
    "roof": MATERIAL_DIR + "/M_R7_RoofCladding",
    "glass": MATERIAL_DIR + "/M_R7_OpaqueGlass",
    "wet": MATERIAL_DIR + "/M_R7_WetGround",
    "red_light": MATERIAL_DIR + "/M_R7_RedBeacon",
    "hazard": MATERIAL_DIR + "/M_R7_HazardStripe",
}


def _spec(rgb, family=None, tile=240, rough=0.7, rough_range=0.18,
          metal=0.0, texture_weight=0.88, normal_strength=0.55,
          desaturate=0.0, macro=False, glow=0.0):
    return dict(rgb=rgb, family=family, tile_cm=tile, roughness_floor=rough,
                roughness_range=rough_range, metallic=metal,
                texture_weight=texture_weight, normal_strength=normal_strength,
                desaturate=desaturate, macro=macro, emissive=glow)


MATERIAL_SPECS = {
    "concrete": _spec((0.64, 0.66, 0.63), "concrete", rough=0.72, macro=True),
    "light_concrete": _spec((0.79, 0.78, 0.70), "concrete", rough=0.76, macro=True),
    "asphalt": _spec((0.39, 0.43, 0.44), "asphalt", 300, rough=0.74, normal_strength=0.65, macro=True),
    # Original recolouring of the existing aggregate surface, not a new soil scan.
    "dirt": _spec((0.36, 0.30, 0.21), "asphalt", 230, rough=0.86, rough_range=0.1,
                  desaturate=0.8, normal_strength=0.7, macro=True),
    "blue": _spec((0.17, 0.29, 0.34), "metal", 150, rough=0.51, metal=0.25),
    "olive": _spec((0.31, 0.36, 0.24), "metal", 150, rough=0.57, metal=0.22),
    "rust": _spec((0.56, 0.32, 0.19), "rust", 180, rough=0.67, metal=0.35,
                  texture_weight=0.9, normal_strength=0.6, macro=True),
    # Equipment housings/tanks use subdued non-corrugated wear and smooth normals.
    "steel": _spec((0.49, 0.55, 0.56), "rust", 180, rough=0.35, rough_range=0.22,
                   metal=0.75, texture_weight=0.30, normal_strength=0.12, desaturate=1.0),
    "dark": _spec((0.095, 0.12, 0.13), "rust", 180, rough=0.49, metal=0.52,
                  texture_weight=0.24, normal_strength=0.08, desaturate=1.0),
    "wood": _spec((0.67, 0.51, 0.33), "wood", 120, rough=0.73, normal_strength=0.6),
    "yellow": _spec((0.86, 0.56, 0.075), rough=0.7, macro=True),
    "white": _spec((0.75, 0.78, 0.69), rough=0.88, macro=True),
    "rubber": _spec((0.032, 0.041, 0.044), rough=0.88),
    "green": _spec((0.06, 0.70, 0.27), rough=0.4, glow=2.3),
    "warm_lamp": _spec((1.0, 0.68, 0.34), rough=0.32, glow=3.8),
    "plaster": _spec((0.71, 0.75, 0.70), "concrete", 280, rough=0.78,
                     texture_weight=0.48, normal_strength=0.2, macro=True),
    "roof": _spec((0.20, 0.26, 0.27), "metal", 150, rough=0.59, metal=0.38),
    # Opaque tinted panels are deliberate; no translucent surfaces obscure enemies.
    "glass": _spec((0.065, 0.14, 0.17), rough=0.16, metal=0.38),
    "wet": _spec((0.13, 0.17, 0.18), "asphalt", 300, rough=0.10, rough_range=0.12,
                 normal_strength=0.16, texture_weight=0.55, macro=True),
    "red_light": _spec((0.95, 0.075, 0.022), rough=0.32, glow=3.0),
    "hazard": _spec((0.86, 0.56, 0.075), rough=0.73),
}


def build_materials():
    """Build/rebuild only R7 materials and return {key: unreal.Material}.

    Uses the installed Editor Scripting/Python plugins and pre-existing textures.
    It neither loads/saves a level nor spawns actors, imports files or downloads.
    Recompilation errors and missing source textures fail the coordinated build.
    """
    import unreal

    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    ml = unreal.MaterialEditingLibrary
    assert set(MATERIAL_PATHS) == set(MATERIAL_SPECS)
    assets.make_directory(MATERIAL_DIR)

    def load(path):
        obj = unreal.load_asset(path)
        assert obj, "R7 required asset missing: " + path
        return obj

    world_texture = load("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture")
    world_normal = load("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal")
    textures = {}
    for family in sorted({s["family"] for s in MATERIAL_SPECS.values() if s["family"]}):
        textures[family] = {channel: load(TEXTURE_DIR + "/" + family + "/T_D_" + family + "_" + channel)
                            for channel in ("basecolor", "roughness", "normal")}

    def node(mat, cls, x=0, y=0, **properties):
        result = ml.create_material_expression(mat, cls, x, y)
        assert result, "R7 material node creation failed: " + cls.__name__
        for prop, value in properties.items():
            # UE5.8 forwards each normal expression notification to its material.
            # Defer that work until the explicit final recompile below.
            result.set_editor_property(prop, value, notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
        return result

    def scalar(mat, value, x=-600, y=0):
        return node(mat, unreal.MaterialExpressionConstant, x, y, r=float(value))

    def colour(mat, value, x=-600, y=0):
        return node(mat, unreal.MaterialExpressionConstant3Vector, x, y,
                    constant=unreal.LinearColor(*value, 1.0))

    def compact(value):
        return "".join(c for c in str(value).lower() if c.isalnum())

    def pin(expr, name, output=False):
        names = list(ml.get_material_expression_output_names(expr) if output else
                     ml.get_material_expression_input_names(expr))
        # UE's MaterialGraphNode shortens Input to NAME_None even on multi-input
        # nodes such as Desaturation (None, Fraction). FName None selects input 0.
        aliases = {"Input": ("Input", "None", ""),
                   "A > B": ("A > B", "A>B", "AGreaterThanB"),
                   "A == B": ("A == B", "A==B", "AEqualsB"),
                   "A < B": ("A < B", "A<B", "ALessThanB")}
        candidates = aliases.get(name, (name,))
        for candidate in candidates:
            for value in names:
                if str(value) == candidate:
                    return str(value)
        for candidate in candidates:
            matches = [str(value) for value in names if compact(value) == compact(candidate)]
            if len(matches) == 1:
                return matches[0]
        if not output and len(names) == 1:
            return str(names[0])
        raise RuntimeError("R7 missing pin %s on %s: %s" % (name, expr.get_class().get_name(), names))

    def link(src, dst, input_name, output_name=""):
        input_pin = pin(dst, input_name) if input_name else ""
        output_pin = pin(src, output_name, True) if output_name else ""
        assert ml.connect_material_expressions(src, output_pin, dst, input_pin), \
            "R7 connection failed: %s -> %s" % (output_pin, input_pin)

    def output(src, prop, source_pin=""):
        assert ml.connect_material_property(src, source_pin, prop), "R7 material output failed: " + str(prop)

    def world_map(mat, texture, tile, normal=False, row=0):
        sampler = (unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else
                   unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if texture.get_editor_property("srgb") else
                   unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        tex = node(mat, unreal.MaterialExpressionTextureObject, -1600, row,
                   texture=texture, sampler_type=sampler)
        fn = node(mat, unreal.MaterialExpressionMaterialFunctionCall, -1250, row)
        assert fn.set_material_function(world_normal if normal else world_texture)
        link(tex, fn, "TextureObject")
        link(colour(mat, (tile, tile, tile), -1600, row + 160), fn, "TextureSize")
        return fn, pin(fn, "XYZ Texture", True)

    def multiply(mat, left, right, row=0, left_pin="", right_pin=""):
        value = node(mat, unreal.MaterialExpressionMultiply, -300, row)
        link(left, value, "A", left_pin)
        link(right, value, "B", right_pin)
        return value

    def lerp(mat, left, right, alpha, row=0, right_pin=""):
        value = node(mat, unreal.MaterialExpressionLinearInterpolate, -700, row)
        link(left, value, "A")
        link(right, value, "B", right_pin)
        link(scalar(mat, alpha, -1050, row + 180), value, "Alpha")
        return value

    result = {}
    report = {"namespace": MATERIAL_DIR, "materials": [], "external_downloads": 0,
              "modified_existing_textures": False, "geometry": "Map builder owns geometry; basic shape material slot 0"}
    for key, spec in MATERIAL_SPECS.items():
        path = MATERIAL_PATHS[key]
        mat = unreal.load_asset(path) if assets.does_asset_exist(path) else None
        if mat is None:
            mat = tools.create_asset(path.rsplit("/", 1)[-1], MATERIAL_DIR,
                                     unreal.Material, unreal.MaterialFactoryNew())
        assert mat, "R7 material creation failed: " + path
        ml.delete_all_material_expressions(mat)
        tint = colour(mat, spec["rgb"], -700, -300)
        base = tint
        maps = textures.get(spec["family"])
        if maps:
            sample, sample_pin = world_map(mat, maps["basecolor"], spec["tile_cm"], row=0)
            if spec["desaturate"]:
                desat = node(mat, unreal.MaterialExpressionDesaturation, -1000, 0)
                link(sample, desat, "Input", sample_pin)
                link(scalar(mat, spec["desaturate"], -1200, 160), desat, "Fraction")
                sample, sample_pin = desat, ""
            texture_colour = lerp(mat, colour(mat, (1.0, 1.0, 1.0), -1100, -150), sample,
                                  spec["texture_weight"], right_pin=sample_pin)
            base = multiply(mat, texture_colour, tint)

        if key == "hazard":
            # World-space diagonal stripe, stable on all three main face orientations.
            position = node(mat, unreal.MaterialExpressionWorldPosition, -1450, -500)
            stripe_coord = node(mat, unreal.MaterialExpressionDotProduct, -1200, -500)
            link(position, stripe_coord, "A")
            link(colour(mat, (0.035, 0.035, 0.035), -1450, -340), stripe_coord, "B")
            fraction = node(mat, unreal.MaterialExpressionFrac, -950, -500)
            link(stripe_coord, fraction, "Input")
            threshold = node(mat, unreal.MaterialExpressionIf, -650, -500, equals_threshold=0.0001)
            link(fraction, threshold, "A")
            link(scalar(mat, 0.5, -1000, -220), threshold, "B")
            ink = colour(mat, (0.025, 0.032, 0.029), -900, -800)
            link(tint, threshold, "A > B")
            link(tint, threshold, "A == B")
            link(ink, threshold, "A < B")
            base = threshold
        if spec["macro"]:
            wear = node(mat, unreal.MaterialExpressionNoise, -300, -650, scale=0.0012,
                        quality=1, levels=1, output_min=0.87, output_max=1.0)
            base = multiply(mat, base, wear, row=-200)
        output(base, unreal.MaterialProperty.MP_BASE_COLOR)

        roughness = scalar(mat, spec["roughness_floor"], -450, 430)
        if maps:
            rough_sample, rough_pin = world_map(mat, maps["roughness"], spec["tile_cm"], row=500)
            channel = node(mat, unreal.MaterialExpressionComponentMask, -980, 500,
                           r=True, g=False, b=False, a=False)
            link(rough_sample, channel, "Input", rough_pin)
            scaled = multiply(mat, channel, scalar(mat, spec["roughness_range"], -800, 730), row=520)
            remap = node(mat, unreal.MaterialExpressionAdd, -100, 480)
            link(roughness, remap, "A")
            link(scaled, remap, "B")
            roughness = remap
        output(roughness, unreal.MaterialProperty.MP_ROUGHNESS)
        output(scalar(mat, spec["metallic"], -100, 760), unreal.MaterialProperty.MP_METALLIC)
        if maps and spec["normal_strength"]:
            normal, normal_pin = world_map(mat, maps["normal"], spec["tile_cm"], normal=True, row=1020)
            # Epic WorldAlignedNormal already transforms from world to tangent space.
            mat.set_editor_property("tangent_space_normal", True,
                                    notify_mode=unreal.PropertyAccessChangeNotifyMode.NEVER)
            normal_mix = lerp(mat, colour(mat, (0.0, 0.0, 1.0), -1100, 1270), normal,
                              spec["normal_strength"], row=1050, right_pin=normal_pin)
            normalise = node(mat, unreal.MaterialExpressionNormalize, -260, 1080)
            link(normal_mix, normalise, "VectorInput")
            output(normalise, unreal.MaterialProperty.MP_NORMAL)
        if spec["emissive"]:
            output(multiply(mat, tint, scalar(mat, spec["emissive"], -500, 1430), row=1410),
                   unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        errors = ml.recompile_material(mat)
        assert not errors, path + " material compilation failed: " + str(errors)
        assert assets.save_loaded_asset(mat, only_if_is_dirty=False), "Failed to save " + path
        result[key] = mat
        report["materials"].append({"key": key, "path": path, **spec,
                                    "texture_assets": {k: v.get_path_name() for k, v in (maps or {}).items()},
                                    "compile_errors": list(errors)})
        unreal.log("R7_MATERIAL_OK " + key + " " + path)
    root = Path(unreal.Paths.project_dir()).resolve()
    evidence = root / "Evidence"
    evidence.mkdir(exist_ok=True)
    (evidence / "R7_VisualAssets.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("R7_VISUAL_ASSETS_OK " + str(len(result)) + " materials; existing source textures unchanged")
    return result


if __name__ == "__main__":
    build_materials()
