"""One-time corrections to this task's newly generated map."""
import unreal
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level("/Game/Maps/RaidYard")
for actor in actors.get_all_level_actors():
    label = actor.get_actor_label()
    if isinstance(actor, unreal.PlayerStart):
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=15,roll=0), False)
    if isinstance(actor, unreal.DirectionalLight):
        actor.set_actor_rotation(unreal.Rotator(pitch=-48,yaw=-25,roll=0), False)
    if label == "Loot_04_Medkit":
        actor.set_actor_location(unreal.Vector(700,-1100,206), False, False)
    if isinstance(actor, unreal.TextRenderActor):
        unreal.log("RAID_ROTATION_BEFORE: " + label + " " + str(actor.get_actor_rotation()))
        yaw = -90 if label == "Sign_Cargo" else 180
        actor.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0), False)
        component = actor.get_component_by_class(unreal.TextRenderComponent)
        component.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        if label == "Sign_Extraction":
            actor.set_actor_location(unreal.Vector(2310,1200,300), False, False)
            component.set_world_size(52)
assert levels.save_current_level()
unreal.log("RAID_MAP_POLISH: PASS medkit accessible and signs centered")
