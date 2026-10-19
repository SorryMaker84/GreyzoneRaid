"""Import the R2 licensed source assets. Run once using UnrealEditor-Cmd -ExecutePythonScript.
This script deliberately does not change maps, config, or C++ gameplay.
All source bytes and permissions are documented in ASSET_SOURCES_R2.md.
"""
from pathlib import Path
import json
import traceback
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
MAT = unreal.MaterialEditingLibrary
REPORT = {"imports": [], "warnings": [], "assets": {}}
# Keep the explicit FbxImportUI settings effective on UE 5.8 reimports too.
# This affects only this editor process; no project config is changed.
editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(editor_world, "Interchange.FeatureFlags.Import.FBX 0")

def safe_property(obj, name, value):
    try:
        obj.set_editor_property(name, value)
    except Exception as exc:
        REPORT["warnings"].append(str(obj.get_class().get_name()) + "." + name + ": " + str(exc))

def import_file(source, folder, name, options=None, asset_class=None, force=False):
    source = Path(source)
    if not source.is_file():
        raise RuntimeError("Required downloaded source does not exist: " + str(source))
    ASSETS.make_directory(folder)
    target = folder + "/" + name
    existing = ASSETS.load_asset(target) if not force and ASSETS.does_asset_exist(target) else None
    if existing:
        REPORT["imports"].append({"source": str(source), "assets": [target], "reused_existing": True})
        return existing, []
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", folder)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("replace_existing", force)
    task.set_editor_property("replace_existing_settings", force)
    if options is not None:
        task.set_editor_property("options", options)
        task.set_editor_property("factory", unreal.FbxFactory())
    TOOLS.import_asset_tasks([task])
    paths = [str(p) for p in task.get_editor_property("imported_object_paths")]
    results = [ASSETS.load_asset(p) for p in paths]
    results = [r for r in results if r]
    chosen = next((r for r in results if asset_class is None or isinstance(r, asset_class)), None)
    if chosen is None:
        chosen = ASSETS.load_asset(target)
    if chosen is None:
        raise RuntimeError("No imported asset produced for " + str(source))
    REPORT["imports"].append({"source": str(source), "assets": paths or [chosen.get_path_name()], "reused_existing": False})
    return chosen, results

def save_required(asset):
    if asset is None:
        raise RuntimeError("Cannot persist a missing imported dependency")
    if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Asset save failed: " + asset.get_path_name())
    package = asset.get_path_name().split(".", 1)[0]
    disk_file = ROOT / "Content" / (package.removeprefix("/Game/") + ".uasset")
    if not disk_file.is_file():
        raise RuntimeError("Saved package missing on disk: " + str(disk_file))
    REPORT.setdefault("persisted", []).append({"asset": asset.get_path_name(), "file": str(disk_file),
                                                "bytes": disk_file.stat().st_size})

def texture(source, folder, name, kind):
    item, _ = import_file(source, folder, name, asset_class=unreal.Texture2D)
    item.set_editor_property("srgb", kind == "color")
    if kind == "normal":
        item.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif kind == "mask":
        item.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    ASSETS.save_loaded_asset(item)
    return item

def make_material(folder, name, color, normal=None, packed=None, roughness=0.7):
    target = folder + "/" + name
    ASSETS.make_directory(folder)
    material = ASSETS.load_asset(target) if ASSETS.does_asset_exist(target) else None
    if material:
        return material
    material = TOOLS.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    base = MAT.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, -160)
    base.set_editor_property("texture", color)
    MAT.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    if normal:
        expression = MAT.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, 100)
        expression.set_editor_property("texture", normal)
        expression.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        MAT.connect_material_property(expression, "RGB", unreal.MaterialProperty.MP_NORMAL)
    if packed:
        expression = MAT.create_material_expression(material, unreal.MaterialExpressionTextureSample, -420, 360)
        expression.set_editor_property("texture", packed)
        expression.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        # Author calls this texture RMAO: roughness, metallic, ambient occlusion.
        MAT.connect_material_property(expression, "R", unreal.MaterialProperty.MP_ROUGHNESS)
        MAT.connect_material_property(expression, "G", unreal.MaterialProperty.MP_METALLIC)
        MAT.connect_material_property(expression, "B", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    else:
        expression = MAT.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 220)
        expression.set_editor_property("r", roughness)
        MAT.connect_material_property(expression, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MAT.recompile_material(material)
    ASSETS.save_loaded_asset(material)
    return material

def fbx_options(skeletal=False, animations=False):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH if skeletal else unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", skeletal)
    options.set_editor_property("import_animations", animations)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("override_full_name", True)
    if skeletal:
        data = options.get_editor_property("skeletal_mesh_import_data")
        safe_property(options, "create_physics_asset", False)
        safe_property(data, "import_morph_targets", False)
        safe_property(data, "convert_scene", True)
        safe_property(data, "convert_scene_unit", True)
        if animations:
            options.set_editor_property("override_animation_name", "A_FPSArms_Idle")
            safe_property(options.get_editor_property("anim_sequence_import_data"), "import_custom_attribute", False)
    else:
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("auto_generate_collision", False)
        safe_property(data, "convert_scene", True)
        safe_property(data, "convert_scene_unit", True)
        safe_property(data, "build_nanite", False)
    return options

def mesh_info(mesh):
    result = {"path": mesh.get_path_name(), "class": mesh.get_class().get_name()}
    try:
        bounds = mesh.get_bounds()
        result["bounds"] = {"origin_cm": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                            "half_extent_cm": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z],
                            "size_cm": [2 * bounds.box_extent.x, 2 * bounds.box_extent.y, 2 * bounds.box_extent.z]}
    except Exception as exc:
        REPORT["warnings"].append("Bounds: " + str(exc))
    if isinstance(mesh, unreal.SkeletalMesh):
        skeleton = mesh.get_editor_property("skeleton")
        result["skeleton"] = skeleton.get_path_name() if skeleton else None
        try:
            component = unreal.SkeletalMeshComponent()
            component.set_skeletal_mesh_asset(mesh)
            result["bone_names"] = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
        except Exception as exc:
            REPORT["warnings"].append("Bone inspection: " + str(exc))
    return result

# SoundWave assets use short PCM .wav files derived from retained CC0 recordings.
audio_manifest = ROOT / "Downloads" / "Audio" / "Prepared" / "audio_manifest.json"
for entry in json.loads(audio_manifest.read_text(encoding="utf-8"))["audio"]:
    sound, _ = import_file(entry["file"], "/Game/Audio", entry["asset_name"], asset_class=unreal.SoundWave)
    safe_property(sound, "volume", 1.0)
    ASSETS.save_loaded_asset(sound)
    REPORT["assets"][entry["asset_name"]] = sound.get_path_name()

weapon_source = ROOT / "Downloads" / "Models" / "SteinGames" / "Extracted" / "WeaponsPack" / "AK47"
base = texture(weapon_source / "T_AK47_C.png", "/Game/Weapons/Textures", "T_ClassicRifle_Color", "color")
normal = texture(weapon_source / "T_AK47_N.png", "/Game/Weapons/Textures", "T_ClassicRifle_Normal", "normal")
packed = texture(weapon_source / "T_AK47_RMAO.png", "/Game/Weapons/Textures", "T_ClassicRifle_RMAO", "mask")
weapon_material = make_material("/Game/Weapons", "M_ClassicRifle", base, normal, packed)
weapon, _ = import_file(weapon_source / "SKM_AK47.fbx", "/Game/Weapons", "SM_ClassicRifle",
                        fbx_options(False, False), unreal.StaticMesh)
# The source has multiple model objects. Every material slot uses the same atlas.
slots = weapon.get_editor_property("static_materials")
for i in range(len(slots)):
    weapon.set_material(i, weapon_material)
ASSETS.save_loaded_asset(weapon)
REPORT["assets"]["weapon"] = mesh_info(weapon)
REPORT["assets"]["weapon_material"] = weapon_material.get_path_name()

arms_source = ROOT / "Downloads" / "Models" / "FPSArms" / "Extracted"
skin_texture = texture(arms_source / "new_diff.png", "/Game/Weapons/Textures", "T_FPSArms_Color", "color")
skin_material = make_material("/Game/Weapons", "M_FPSArms", skin_texture, roughness=0.76)
arms, related = import_file(arms_source / "FPSArms_SingleRoot.fbx", "/Game/Weapons", "SK_FPSArms",
                            fbx_options(True, True), unreal.SkeletalMesh,
                            force=not (ROOT / "Content/Weapons/SK_FPSArms_Skeleton.uasset").is_file())
skeleton = arms.get_editor_property("skeleton")
if not isinstance(skeleton, unreal.Skeleton):
    raise RuntimeError("Imported arms have no USkeleton")
# FBX import's save flag does not necessarily persist an auto-created Skeleton.
# Save the actual dependency before mesh and animation packages that reference it.
save_required(skeleton)
slots = arms.get_editor_property("materials")
for slot in slots:
    slot.set_editor_property("material_interface", skin_material)
arms.set_editor_property("materials", slots)
save_required(arms)
REPORT["assets"]["arms"] = mesh_info(arms)
animations = [a for a in related if isinstance(a, unreal.AnimSequence)]
if not animations:
    animations = [ASSETS.load_asset(p) for p in ASSETS.list_assets("/Game/Weapons", True, False)]
    animations = [a for a in animations if isinstance(a, unreal.AnimSequence)]
REPORT["assets"]["arms_animations"] = []
for anim in animations:
    anim_skeleton = anim.get_editor_property("skeleton")
    if anim_skeleton is None:
        raise RuntimeError("Imported animation has no Skeleton; it must be recreated through FBX import")
    if anim_skeleton is None or anim_skeleton.get_path_name() != skeleton.get_path_name():
        raise RuntimeError("Arms animation references a different or missing Skeleton: " + anim.get_path_name())
    save_required(anim_skeleton)
    save_required(anim)
    REPORT["assets"]["arms_animations"].append({"path": anim.get_path_name(), "length_seconds": anim.get_play_length(),
                                                "skeleton": anim_skeleton.get_path_name()})
REPORT["assets"]["arms_material"] = skin_material.get_path_name()
out = ROOT / "Evidence" / "ImportedAssetsR2.json"
out.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("R2_ASSET_IMPORT_REPORT " + json.dumps(REPORT, ensure_ascii=False))
print("R2_ASSET_IMPORT_OK " + str(out))
