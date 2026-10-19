"""Sample frame 0; optionally bake an explicitly supplied FPS arms grip.

Run with UE Editor Python. By default this only writes Evidence/ArmsPoseSample.json.
If Scripts/grip_finger_overrides.json exists, also save A_FPSArms_Grip. The source
animation is never edited or saved. Override JSON is either {bone: [x,y,z,w]} or
{"rotations": {bone: [x,y,z,w]}}; quaternions are absolute parent-local rotations.
WORLD below means skeletal component space, as defined by UE AnimPoseSpaces.WORLD.
"""

import json
import math
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SOURCE_PATH = "/Game/Weapons/A_FPSArms_Idle"
MESH_PATH = "/Game/Weapons/SK_FPSArms"
GRIP_PATH = "/Game/Weapons/A_FPSArms_Grip"
OVERRIDES_PATH = ROOT / "Scripts" / "grip_finger_overrides.json"
REPORT_PATH = ROOT / "Evidence" / "ArmsPoseSample.json"
ASSETS = unreal.EditorAssetLibrary
POSES = unreal.AnimPoseExtensions


def load_asset(path):
    # Native object loading also works before a commandlet's registry scan.
    name = path.rsplit("/", 1)[-1]
    return unreal.load_asset(path + "." + name)


def vector_json(value):
    return [float(value.x), float(value.y), float(value.z)]


def quat_json(value):
    return [float(value.x), float(value.y), float(value.z), float(value.w)]


def transform_json(transform, parent):
    return {
        "translation": vector_json(transform.get_editor_property("translation")),
        "rotation": quat_json(transform.get_editor_property("rotation")),
        "scale": vector_json(transform.get_editor_property("scale3d")),
        "parent": parent,
    }


def sample_animation(animation, mesh):
    options = unreal.AnimPoseEvaluationOptions()
    options.set_editor_property("evaluation_type", unreal.AnimDataEvalType.SOURCE)
    options.set_editor_property("should_retarget", False)
    options.set_editor_property("extract_root_motion", False)
    options.set_editor_property("optional_skeletal_mesh", mesh)
    pose = POSES.get_anim_pose_at_frame(animation, 0, options)
    assert POSES.is_valid(pose), "Frame 0 did not produce a valid animation pose"

    # This component is only used to read the reference hierarchy, never spawned.
    component = unreal.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    names = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    pose_names = {str(name) for name in POSES.get_bone_names(pose)}
    missing = set(names) - pose_names
    assert not missing, "Pose is missing mesh bones: " + ", ".join(sorted(missing))

    local = {}
    world = {}
    transforms = {}
    for name in names:
        parent = str(component.get_parent_bone(name))
        parent = None if parent.lower() == "none" else parent
        transform = POSES.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)
        transforms[name] = transform
        local[name] = transform_json(transform, parent)
        world[name] = transform_json(
            POSES.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD), parent
        )
    return names, transforms, local, world


def read_overrides(names):
    data = json.loads(OVERRIDES_PATH.read_text(encoding="utf-8-sig"))
    assert isinstance(data, dict), "Grip overrides must be a JSON object"
    data = data.get("rotations", data)
    assert isinstance(data, dict), "rotations must be a bone-to-quaternion object"
    assert data, "Grip override file exists but contains no rotations"
    unknown = set(data) - set(names)
    assert not unknown, "Unknown override bones: " + ", ".join(sorted(unknown))
    rotations = {}
    for name, values in data.items():
        assert isinstance(values, list) and len(values) == 4, name + " needs [x,y,z,w]"
        assert all(isinstance(v, (int, float)) and not isinstance(v, bool) for v in values), name + " quaternion must be numeric"
        values = [float(value) for value in values]
        assert all(math.isfinite(value) for value in values), name + " quaternion must be finite"
        length = math.sqrt(sum(value * value for value in values))
        assert length > 1e-8, name + " quaternion has zero length"
        rotations[name] = unreal.Quat(*(value / length for value in values))
    return rotations


def bake_grip(source, transforms, rotations):
    destination = load_asset(GRIP_PATH)
    if destination is None:
        destination = ASSETS.duplicate_asset(SOURCE_PATH, GRIP_PATH)
    assert isinstance(destination, unreal.AnimSequence), "Could not create/load Grip animation"
    assert destination.get_path_name() != source.get_path_name(), "Grip must be a separate asset"
    assert destination.get_editor_property("skeleton") == source.get_editor_property("skeleton"), "Grip Skeleton differs from source"

    # GetController() is C++ only. These properties and interface methods are
    # reflected in UE 5.8; DuplicateTransient can leave Controller unset.
    controller = destination.get_editor_property("controller")
    if controller is None:
        controller = unreal.AnimDataController(outer=destination)
    controller.set_model(destination.get_editor_property("data_model_interface"))
    existing = {
        str(name) for name in
        destination.get_editor_property("data_model_interface").get_bone_track_names()
    }
    controller.open_bracket("Bake constant FPS arms grip", False)
    try:
        controller.set_number_of_frames(unreal.FrameNumber(1), False)
        for name, transform in transforms.items():
            if not destination.get_editor_property("data_model_interface").is_valid_bone_track_name(name):
                assert controller.add_bone_curve(name, False), "Could not add track: " + name
            translation = transform.get_editor_property("translation")
            rotation = rotations.get(name, transform.get_editor_property("rotation"))
            scale = transform.get_editor_property("scale3d")
            # One frame interval has two endpoints. Equal endpoint keys yield
            # the same pose at any playback time and work with UE compression.
            assert controller.set_bone_track_keys(
                name, [translation, translation], [rotation, rotation],
                [scale, scale], False
            ), "Could not write track: " + name
    finally:
        controller.close_bracket(False)
    assert ASSETS.save_loaded_asset(destination), "Grip animation could not be saved"
    return destination


def main():
    source = load_asset(SOURCE_PATH)
    mesh = load_asset(MESH_PATH)
    assert isinstance(source, unreal.AnimSequence), "Source animation missing"
    assert isinstance(mesh, unreal.SkeletalMesh), "FPS arms skeletal mesh missing"
    assert source.get_editor_property("skeleton") == mesh.get_editor_property("skeleton"), "Mesh and source Skeleton differ"
    names, transforms, local, world = sample_animation(source, mesh)
    report = {
        "status": "PASS",
        "mode": "sample",
        "source_animation": source.get_path_name(),
        "skeletal_mesh": mesh.get_path_name(),
        "frame": 0,
        "evaluation_type": "SOURCE",
        "should_retarget": False,
        "extract_root_motion": False,
        "rotation_order": "xyzw",
        "translation_unit": "cm",
        "world_space_definition": "skeletal component space; no actor/world placement",
        "bone_count": len(names),
        "bone_names": names,
        "LOCAL": local,
        "WORLD": world,
    }
    REPORT_PATH.parent.mkdir(parents=True, exist_ok=True)
    REPORT_PATH.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

    if OVERRIDES_PATH.is_file():
        rotations = read_overrides(names)
        destination = bake_grip(source, transforms, rotations)
        report.update({
            "mode": "sample_and_bake",
            "grip_animation": destination.get_path_name(),
            "grip_saved": True,
            "grip_length_seconds": destination.get_play_length(),
            "grip_frame_intervals": 1,
            "grip_keys_per_bone": 2,
            "overridden_bones": sorted(rotations),
            "normalized_local_rotation_overrides": {
                name: quat_json(rotation) for name, rotation in rotations.items()
            },
        })
        REPORT_PATH.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    unreal.log("R2_ARMS_POSE_SAMPLE_PASS " + json.dumps({
        "mode": report["mode"], "bone_count": report["bone_count"],
        "report": str(REPORT_PATH), "grip_animation": report.get("grip_animation")
    }))


main()
