"""Update only authored item metadata; maps, textures and models stay unchanged."""
import json
from pathlib import Path
import unreal
asset=unreal.EditorAssetLibrary.load_asset('/Game/Data/DA_RaidItems')
assert isinstance(asset,unreal.RaidItemCatalog)
weights={'Tech':3000,'Medkit':600,'Ammo':1200,'Rations':1500}
definitions=asset.get_editor_property('definitions')
assert {str(d.get_editor_property('item_id')) for d in definitions}==set(weights)
updated=[]
for d in definitions:
 d.set_editor_property('unit_weight_grams',weights[str(d.get_editor_property('item_id'))])
 updated.append(d)
asset.set_editor_property('definitions',updated)
assert unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
actual={str(d.get_editor_property('item_id')):d.get_editor_property('unit_weight_grams') for d in asset.get_editor_property('definitions')}
assert actual==weights
(Path(unreal.Paths.project_dir())/'Evidence/R10_ItemWeights.json').write_text(json.dumps({'status':'PASS','grams':actual,'source':'Original project balancing, not Delta Force values.'},indent=2))
unreal.log('R10_ITEM_WEIGHTS PASS')
