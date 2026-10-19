"""Persist the existing fence's ISM usage flag; no graph, texture or map edits."""
import hashlib
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
path = root / 'Content/Materials/M_ChainLink.uasset'
before = hashlib.sha256(path.read_bytes()).hexdigest()
asset = unreal.load_asset('/Game/Materials/M_ChainLink')
assert asset is not None
usage = unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES
library = unreal.MaterialEditingLibrary
before_usage = library.has_material_usage(asset, usage)
library.set_base_material_usage(asset, usage, True)
library.recompile_material(asset)
assert library.has_material_usage(asset, usage)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
report = {'status': 'SAVED_REQUIRES_PACKAGED_RENDER_CHECK', 'asset': asset.get_path_name(),
          'before_instanced_usage': before_usage, 'after_instanced_usage': True,
          'sha256_before': before, 'sha256_after': hashlib.sha256(path.read_bytes()).hexdigest(),
          'graph_or_texture_changes': False, 'map_changes': False}
(root / 'Evidence/R7FenceUsageFix.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('R7_FENCE_USAGE_SAVED: packaged render and warning scan still required')
