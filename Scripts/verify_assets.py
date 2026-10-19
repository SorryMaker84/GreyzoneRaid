import unreal

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
assert levels.load_level("/Game/Maps/RaidYard"), "Saved map could not load"
all_actors = actors.get_all_level_actors()
counts = {}
for name in ["RaidEnemy", "RaidLoot", "RaidExtractionZone", "RaidNavBounds", "PlayerStart", "StaticMeshActor"]:
    cls = getattr(unreal, name)
    counts[name] = sum(isinstance(a, cls) for a in all_actors)
assert counts["RaidEnemy"] == 3, counts
assert counts["RaidLoot"] == 9, counts
assert counts["RaidExtractionZone"] == 1, counts
assert counts["RaidNavBounds"] == 1, counts
assert counts["PlayerStart"] == 1, counts
assert counts["StaticMeshActor"] >= 25, counts
for name in ["BP_RaidEnemy", "BP_RaidLoot", "BP_Extraction", "BP_RaidGameMode"]:
    assert assets.load_blueprint_class("/Game/Blueprints/" + name), name
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property("default_game_mode"), "Missing GameMode"
nav = next(a for a in all_actors if isinstance(a, unreal.RaidNavBounds))
assert nav.get_component_by_class(unreal.BoxComponent).get_unscaled_box_extent().x > 2000
assert levels.save_current_level()
assert assets.save_directory("/Game", only_if_is_dirty=False, recursive=True)
unreal.log("RAID_ASSET_VERIFY: PASS " + str(counts))
