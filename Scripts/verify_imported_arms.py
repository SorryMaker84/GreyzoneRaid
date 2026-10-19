"""Run in a separate fresh UE process to verify persisted R2 Skeleton dependencies."""
from pathlib import Path
import json
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
assets = unreal.EditorAssetLibrary
paths = {"mesh": "/Game/Weapons/SK_FPSArms", "skeleton": "/Game/Weapons/SK_FPSArms_Skeleton",
         "animation": "/Game/Weapons/A_FPSArms_Idle"}
loaded = {key: assets.load_asset(path) for key, path in paths.items()}
assert isinstance(loaded["mesh"], unreal.SkeletalMesh), "Persisted skeletal mesh not loaded"
assert isinstance(loaded["skeleton"], unreal.Skeleton), "Persisted USkeleton not loaded"
assert isinstance(loaded["animation"], unreal.AnimSequence), "Persisted animation not loaded"
mesh_skeleton = loaded["mesh"].get_editor_property("skeleton")
anim_skeleton = loaded["animation"].get_editor_property("skeleton")
assert mesh_skeleton, "Reloaded mesh Skeleton is null"
assert anim_skeleton, "Reloaded animation Skeleton is null"
expected = loaded["skeleton"].get_path_name()
assert mesh_skeleton.get_path_name() == expected == anim_skeleton.get_path_name(), "Skeleton dependency identity mismatch"
files = {}
for key, path in paths.items():
    file = root / "Content" / (path.removeprefix("/Game/") + ".uasset")
    assert file.is_file(), "Missing persisted package: " + str(file)
    files[key] = {"path": str(file), "bytes": file.stat().st_size}
component = unreal.SkeletalMeshComponent()
component.set_skeletal_mesh_asset(loaded["mesh"])
report = {"status": "PASS", "independent_fresh_process": True,
          "assets": {k: v.get_path_name() for k, v in loaded.items()},
          "mesh_skeleton": mesh_skeleton.get_path_name(), "animation_skeleton": anim_skeleton.get_path_name(),
          "bone_count": component.get_num_bones(),
          "root_bone": str(component.get_bone_name(0)),
          "animation_length_seconds": loaded["animation"].get_play_length(), "disk_files": files}
(root / "Evidence/ArmsPersistenceVerify.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
unreal.log("R2_ARMS_PERSISTENCE_VERIFY_PASS " + json.dumps(report))
