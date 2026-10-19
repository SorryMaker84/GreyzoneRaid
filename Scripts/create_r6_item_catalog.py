"""Run inside UE's Python commandlet after compiling the R6 item data types."""
import json
from pathlib import Path
import unreal

ASSET_PATH = '/Game/Data/DA_RaidItems'
SPECS = [
    ('Tech', '电子元件', 120, unreal.RaidItemUseEffect.NONE, 0),
    ('Medkit', '医疗包', 75, unreal.RaidItemUseEffect.HEAL, 45),
    ('Ammo', '备用弹药', 35, unreal.RaidItemUseEffect.AMMO, 24),
    ('Rations', '应急口粮', 40, unreal.RaidItemUseEffect.NONE, 0),
]

asset = (unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
         if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH) else None)
if asset is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.RaidItemCatalog)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'DA_RaidItems', '/Game/Data', unreal.RaidItemCatalog, factory)
if not isinstance(asset, unreal.RaidItemCatalog):
    raise RuntimeError('Existing catalog has the wrong class; refusing to replace it.')

definitions = []
for item_id, label, value, effect, amount in SPECS:
    definition = unreal.RaidItemDefinition()
    for key, field_value in [('item_id', unreal.Name(item_id)), ('display_name', label),
                             ('unit_value', value), ('use_effect', effect), ('use_amount', amount)]:
        definition.set_editor_property(key, field_value)
    definitions.append(definition)
asset.set_editor_property('definitions', definitions)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
    raise RuntimeError('Could not save the R6 item catalog.')

actual = asset.get_editor_property('definitions')
assert len(actual) == 4
for item, spec in zip(actual, SPECS):
    assert str(item.get_editor_property('item_id')) == spec[0]
    assert item.get_editor_property('display_name') == spec[1]
    assert item.get_editor_property('unit_value') == spec[2]
    assert item.get_editor_property('use_effect') == spec[3]
    assert item.get_editor_property('use_amount') == spec[4]

receipt = {'asset': ASSET_PATH, 'status': 'PASS', 'definitions': [
    {'id': s[0], 'label': s[1], 'value': s[2], 'effect': str(s[3]), 'amount': s[4]}
    for s in SPECS], 'source': 'Original project data; no external asset download.'}
evidence = Path(unreal.Paths.project_dir()) / 'Evidence/R6ItemCatalog.json'
evidence.write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('R6_ITEM_CATALOG: PASS four data-driven item definitions saved and verified.')
