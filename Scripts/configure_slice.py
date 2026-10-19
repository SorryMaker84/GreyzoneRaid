"""Run after the R2 Editor build and import_assets.py; no build/download side effects.

Depot uses clean Blueprint children so legacy component-template overrides cannot
keep the previous two-cube enemy, spinning loot cube, or large green platform.
Optional Scripts/player_pose_overrides.json overrides the editable pose defaults.
"""
import json
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
ASSETS = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
BEL = unreal.BlueprintEditorLibrary
LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MAP = "/Game/Maps/Depot"
BP_DIR = "/Game/Blueprints"
REPORT = {"blueprints": {}, "replaced": [], "notes": [], "bounds": {}}

# Override these properties with JSON arrays; all lengths are cm, rotation is
# [pitch,yaw,roll]. Values not listed here retain the current native defaults.
PLAYER_POSE = {
    "weapon_model_offset": [0, 0, 0],
    "weapon_model_rotation": [0, -90, 0],
    "weapon_model_scale": [1, 1, 1],
    "arms_offset": [0, 0, 0],
    "arms_rotation": [0, 0, 0],
    "arms_scale": [0.1, 0.1, 0.1],
}
PLAYER_ASSETS = {
    "weapon_model": "/Game/Weapons/SM_RifleBody_R3",
    "magazine_model": "/Game/Weapons/SM_RifleMagazine_R3",
    "arms_model": "/Game/Weapons/SK_FPSArms",
    "arms_idle_animation": "/Game/Weapons/A_FPSArms_Grip_R3",
    "arms_reload_animation": "/Game/Weapons/A_FPSArms_Reload_R3",
    "arms_fire_animation": "/Game/Weapons/A_FPSArms_Fire_R3",
    "fire_sound": "/Game/Audio/S_RifleShot",
    "reload_sound": "/Game/Audio/S_Reload",
    "empty_sound": "/Game/Audio/S_Empty",
    "hit_sound": "/Game/Audio/S_Hit",
    "footstep_sound": "/Game/Audio/S_Footstep",
}
override_path = ROOT / "Scripts" / "player_pose_overrides.json"
if override_path.is_file():
    overrides = json.loads(override_path.read_text(encoding="utf-8-sig"))
    PLAYER_POSE.update(overrides.get("pose", overrides))
    PLAYER_ASSETS.update(overrides.get("assets", {}))


def load(path, required=True):
    # Native loading also works when a commandlet has not scanned the Asset Registry.
    object_path = path if "." in path.rsplit("/", 1)[-1] else path + "." + path.rsplit("/", 1)[-1]
    asset = unreal.load_asset(object_path)
    assert asset or not required, "Required slice asset missing: " + object_path
    return asset


def vector(v):
    return unreal.Vector(float(v[0]), float(v[1]), float(v[2]))


def vec_json(v):
    return [float(v.x), float(v.y), float(v.z)]


def blueprint(name, parent):
    path = BP_DIR + "/" + name
    bp = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else None
    if bp is None:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = TOOLS.create_asset(name, BP_DIR, unreal.Blueprint, factory)
    assert bp, "Blueprint creation failed: " + name
    assert BEL.compile_blueprint(bp), "Blueprint compilation failed: " + name
    klass = bp.generated_class()
    assert klass, "Blueprint has no generated class: " + name
    REPORT["blueprints"][name] = {"class": klass.get_path_name(), "status": str(bp.get_editor_property("status"))}
    return bp, klass


def save_bp(bp):
    assert BEL.compile_blueprint(bp), "Configured Blueprint failed to compile"
    assert ASSETS.save_loaded_asset(bp, only_if_is_dirty=False)
    return bp.generated_class()


def component_report(actor, required=()):
    comps = actor.get_components_by_class(unreal.ActorComponent)
    names = sorted(str(c.get_name()) for c in comps)
    missing = [name for name in required if name not in names]
    assert not missing, actor.get_actor_label() + " missing new native components " + str(missing)
    return {"component_count": len(comps), "component_names": names}


ASSETS.make_directory(BP_DIR)
ASSETS.make_directory("/Game/Materials")
fx_path = "/Game/Materials/M_WeaponFX"
fx = ASSETS.load_asset(fx_path) if ASSETS.does_asset_exist(fx_path) else None
fx_created = fx is None
if fx_created:
    fx = TOOLS.create_asset("M_WeaponFX", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
assert fx
if fx_created:
    param = MEL.create_material_expression(fx, unreal.MaterialExpressionVectorParameter, -320, 0)
    param.set_editor_property("parameter_name", unreal.Name("Color"))
    param.set_editor_property("default_value", unreal.LinearColor(1.0, 0.5, 0.08, 1.0))
    assert MEL.connect_material_property(param, "", unreal.MaterialProperty.MP_BASE_COLOR)
    assert MEL.connect_material_property(param, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    roughness = MEL.create_material_expression(fx, unreal.MaterialExpressionConstant, -320, 180)
    roughness.set_editor_property("r", 0.4)
    assert MEL.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    errors = MEL.recompile_material(fx)
    assert not errors, "WeaponFX compile failed: " + str(errors)
    assert ASSETS.save_loaded_asset(fx, only_if_is_dirty=False)

# A project-owned parameterized cloth material supports skeletal permutations.
# Engine BasicShapeMaterial is intended for static graybox meshes and must not be
# used as the Manny uniform's MID parent.
cloth_path = "/Game/Materials/M_GuardCloth"
cloth = ASSETS.load_asset(cloth_path) if ASSETS.does_asset_exist(cloth_path) else None
cloth_created = cloth is None
if cloth_created:
    cloth = TOOLS.create_asset("M_GuardCloth", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
assert cloth
cloth.set_editor_property("used_with_skeletal_mesh", True)
if cloth_created:
    cloth_color = MEL.create_material_expression(cloth, unreal.MaterialExpressionVectorParameter, -320, 0)
    cloth_color.set_editor_property("parameter_name", unreal.Name("Color"))
    cloth_color.set_editor_property("default_value", unreal.LinearColor(0.18, 0.23, 0.115, 1.0))
    assert MEL.connect_material_property(cloth_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    cloth_roughness = MEL.create_material_expression(cloth, unreal.MaterialExpressionConstant, -320, 160)
    cloth_roughness.set_editor_property("r", 0.88)
    assert MEL.connect_material_property(cloth_roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    errors = MEL.recompile_material(cloth)
    assert not errors, "GuardCloth compile failed: " + str(errors)
    assert ASSETS.save_loaded_asset(cloth, only_if_is_dirty=False)

REPORT["guard_cloth"] = {"path": cloth.get_path_name(), "used_with_skeletal_mesh":
                         bool(cloth.get_editor_property("used_with_skeletal_mesh")), "roughness": 0.88}


def guard_materials(actor):
    mesh = actor.get_editor_property("mesh")
    count = mesh.get_num_materials()
    assert count > 0, "Guard skeletal mesh has no material slots"
    for slot in range(count):
        mesh.set_material(slot, cloth)
    return [mesh.get_material(slot).get_path_name() for slot in range(count)]


player_bp, player_class = blueprint("BP_RaidPlayer", unreal.RaidPlayer)
player_cdo = unreal.get_default_object(player_class)
for prop, path in PLAYER_ASSETS.items():
    player_cdo.set_editor_property(prop, load(path))
for prop, values in PLAYER_POSE.items():
    value = (unreal.Rotator(pitch=values[0], yaw=values[1], roll=values[2])
             if "rotation" in prop else vector(values))
    player_cdo.set_editor_property(prop, value)
try:
    player_cdo.set_editor_property("effect_material", fx)
except Exception as exc:
    # The constructor reloads this saved hard-reference path in the next game process.
    REPORT["notes"].append("Private EffectMaterial assignment skipped; saved material loads in a fresh game: " + str(exc))
player_class = save_bp(player_bp)
player_cdo = unreal.get_default_object(player_class)
REPORT["player_assets"] = {prop: player_cdo.get_editor_property(prop).get_path_name() for prop in PLAYER_ASSETS}
REPORT["player_pose"] = {prop: str(player_cdo.get_editor_property(prop)) for prop in PLAYER_POSE}

for kind, prop in [("weapon", "weapon_model"), ("arms", "arms_model")]:
    mesh = player_cdo.get_editor_property(prop)
    bounds = mesh.get_bounds()
    origin, extent = bounds.origin, bounds.box_extent
    REPORT["bounds"][kind] = {
        "asset": mesh.get_path_name(), "origin_cm": vec_json(origin),
        "half_extent_cm": vec_json(extent), "size_cm": [2 * v for v in vec_json(extent)],
        "min_cm": [a - b for a, b in zip(vec_json(origin), vec_json(extent))],
        "max_cm": [a + b for a, b in zip(vec_json(origin), vec_json(extent))],
    }
(ROOT / "Evidence" / "PlayerPoseBounds.json").write_text(json.dumps(REPORT["bounds"], indent=2), encoding="utf-8")

enemy_bp, enemy_class = blueprint("BP_DepotGuard", unreal.RaidEnemy)
guard_materials(unreal.get_default_object(enemy_class))
enemy_class = save_bp(enemy_bp)
REPORT["guard_cdo_materials"] = guard_materials(unreal.get_default_object(enemy_class))
loot_bp, loot_class = blueprint("BP_DepotSupply", unreal.RaidLoot)
extract_bp, extract_class = blueprint("BP_DepotExtraction", unreal.RaidExtractionZone)
game_bp, game_class = blueprint("BP_DepotGameMode", unreal.RaidGameMode)
game_cdo = unreal.get_default_object(game_class)
REPORT["native_game_mode_default_pawn"] = unreal.get_default_object(unreal.RaidGameMode).get_editor_property("default_pawn_class").get_path_name()
game_cdo.set_editor_property("default_pawn_class", player_class)
game_class = save_bp(game_bp)
assert unreal.get_default_object(game_class).get_editor_property("default_pawn_class") == player_class
for bp in (enemy_bp, loot_bp, extract_bp):
    assert ASSETS.save_loaded_asset(bp, only_if_is_dirty=False)

assert LEVELS.load_level(MAP), "Depot must already exist"
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
settings = world.get_world_settings()
old_game = settings.get_editor_property("default_game_mode")
REPORT["previous_map_game_mode"] = old_game.get_path_name() if old_game else None
settings.set_editor_property("default_game_mode", game_class)

rules = [
    (unreal.RaidEnemy, enemy_class,
     ("patrol_points", "health", "max_health", "detection_radius", "attack_range", "attack_damage", "attack_interval"),
     ("GuardVisuals", "CombatHelmet", "RifleStock", "EnemyWeaponAudio")),
    (unreal.RaidLoot, loot_class,
     ("item_id", "display_name", "quantity", "value", "search_duration"),
     ("CaseVisualRoot", "CaseLid", "HandleGrip", "FrontStencil")),
    (unreal.RaidExtractionZone, extract_class,
     ("extract_seconds",), ("StationVisualRoot", "SignalMast", "StationStatus", "BoundaryWest")),
]
snapshots = []
for actor in ACTORS.get_all_level_actors():
    if isinstance(actor, unreal.PlayerStart):
        # Frame the nearby corrugated cargo stack rather than the long empty road.
        actor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=30, roll=0), False)
        REPORT["spawn"] = {"location": vec_json(actor.get_actor_location()), "yaw": 30}
    for native, child, props, required in rules:
        if isinstance(actor, native):
            snapshots.append({"old": actor, "class": child, "label": actor.get_actor_label(),
                              "location": actor.get_actor_location(), "rotation": actor.get_actor_rotation(),
                              "scale": actor.get_actor_scale3d(), "tags": list(actor.get_editor_property("tags")),
                              "folder": actor.get_folder_path(), "properties": {p: actor.get_editor_property(p) for p in props},
                              "required": required})
            break

# Create replacements before deleting old instances; a spawn failure leaves the old map intact.
for item in snapshots:
    replacement = ACTORS.spawn_actor_from_class(item["class"], item["location"], item["rotation"])
    assert replacement, "Fresh native components could not be instantiated: " + item["label"]
    replacement.set_actor_scale3d(item["scale"])
    replacement.set_actor_label(item["label"])
    replacement.set_editor_property("tags", item["tags"])
    replacement.set_folder_path(item["folder"])
    for prop, value in item["properties"].items():
        replacement.set_editor_property(prop, value)
    audit = component_report(replacement, item["required"])
    if isinstance(replacement, unreal.RaidEnemy):
        audit["skeletal_materials"] = guard_materials(replacement)
    audit.update({"label": item["label"], "class": replacement.get_class().get_path_name()})
    REPORT["replaced"].append(audit)
    assert ACTORS.destroy_actor(item["old"]), "Could not remove superseded instance"

# Audit the actual spawned player's component tree, then remove this editor-only probe.
probe = ACTORS.spawn_actor_from_class(player_class, unreal.Vector(-4000, -4000, 200))
assert probe
probe.set_actor_label("Player_ComponentAudit_Probe")
REPORT["player_components"] = component_report(probe,
    ("WeaponPose", "OptionalWeaponModel", "OptionalFirstPersonArms", "MuzzleFlame", "RemovableMagazine"))
assert ACTORS.destroy_actor(probe)
assert len(REPORT["replaced"]) == 13, "Expected 3 guards, 9 supply cases and 1 extraction zone"
assert LEVELS.save_current_level(), "Could not save configured Depot"
for bp in (player_bp, enemy_bp, loot_bp, extract_bp, game_bp):
    assert ASSETS.save_loaded_asset(bp, only_if_is_dirty=False)
REPORT["final_map_game_mode"] = game_class.get_path_name()
REPORT["final_default_pawn"] = player_class.get_path_name()
out = ROOT / "Evidence" / "SliceConfiguration.json"
out.write_text(json.dumps(REPORT, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("SLICE_CONFIG_COMPLETE: Depot uses BP_RaidPlayer; " + str(len(REPORT["replaced"])) +
           " gameplay instances refreshed; " + str(out))
