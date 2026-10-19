"""Read the saved Depot's visible text before the R4 language migration."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Maps/Depot')
report = {'world_text': [], 'items': []}
for actor in actors.get_all_level_actors():
    for component in actor.get_components_by_class(unreal.TextRenderComponent):
        report['world_text'].append({
            'actor': actor.get_actor_label(), 'class': actor.get_class().get_path_name(),
            'component': component.get_name(), 'text': str(component.get_editor_property('text')),
            'world_size': component.get_editor_property('world_size'),
            'font': component.get_editor_property('font').get_path_name() if component.get_editor_property('font') else None,
        })
    if isinstance(actor, unreal.RaidLoot):
        report['items'].append({'actor': actor.get_actor_label(),
            'id': str(actor.get_editor_property('item_id')), 'display_name': actor.get_editor_property('display_name'),
            'quantity': actor.get_editor_property('quantity'), 'value': actor.get_editor_property('value')})
(root/'Evidence/R4_TextBefore.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
unreal.log('R4_TEXT_AUDIT_OK world_text=%d items=%d' % (len(report['world_text']),len(report['items'])))
