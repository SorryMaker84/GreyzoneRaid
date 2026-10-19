"""Migrate saved map text and item names without rebuilding or replacing geometry."""
import json
import re
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
catalog = json.loads((root/'Scripts/r4_text_catalog.json').read_text(encoding='utf-8-sig'))
assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world_font = unreal.load_asset('/Game/UI/Fonts/F_RaidChineseWorld')
assert world_font, 'Import R4 fonts first'
report = {'maps': {}, 'defaults': [], 'unknown_latin_text': []}

# Refresh old Blueprint default strings; identifiers and gameplay values stay intact.
for path in ('/Game/Blueprints/BP_DepotSupply', '/Game/Blueprints/BP_RaidLoot'):
    if not assets.does_asset_exist(path): continue
    bp = unreal.load_asset(path)
    cls = bp.generated_class()
    default = unreal.get_default_object(cls)
    old = default.get_editor_property('display_name')
    new = catalog['items'].get(old, old)
    default.set_editor_property('display_name', new)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert assets.save_loaded_asset(bp, only_if_is_dirty=False)
    report['defaults'].append({'asset': path, 'old': old, 'new': new})

for map_path in ('/Game/Maps/Depot', '/Game/Maps/RaidYard'):
    assert levels.load_level(map_path)
    items, labels = [], []
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.RaidLoot):
            old = actor.get_editor_property('display_name')
            new = catalog['items'].get(old, old)
            actor.set_editor_property('display_name', new)
            # Calling an editor property change refreshes native OnConstruction.
            items.append({'actor': actor.get_actor_label(), 'old': old, 'new': new})
        for component in actor.get_components_by_class(unreal.TextRenderComponent):
            old = str(component.get_editor_property('text'))
            new = catalog['static_text'].get(old, old)
            # Transient interaction labels will be filled by gameplay only when focused.
            if component.get_name() == 'InteractionLabel': new = ''
            component.set_font(world_font)
            component.set_text(new)
            labels.append({'actor': actor.get_actor_label(), 'component': component.get_name(), 'text': new,
                           'font': component.get_editor_property('font').get_path_name()})
            # Text may contain only controls/sector IDs as Latin characters.
            residue = re.sub(r'E-07|7\.62|[E]', '', new)
            if re.search(r'[A-Za-z]{2,}', residue):
                report['unknown_latin_text'].append({'map':map_path,'actor':actor.get_actor_label(),'text':new})
    assert levels.save_current_level()
    report['maps'][map_path] = {'items':items, 'labels':labels}

# Return the editor's saved startup world to the playable Depot.
assert levels.load_level('/Game/Maps/Depot')
(root/'Evidence/R4_MapLocalization.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
assert not report['unknown_latin_text'], report['unknown_latin_text']
unreal.log('R4_MAP_LOCALIZATION_OK '+json.dumps({k:{'items':len(v['items']),'labels':len(v['labels'])} for k,v in report['maps'].items()}))
