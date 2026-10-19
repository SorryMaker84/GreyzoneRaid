"""Readable shadow fill, two useful standing covers and clear medical/exfil landmarks."""
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert levels.load_level('/Game/Maps/Depot')
tag=unreal.Name('DepotR3')
for actor in actors.get_all_level_actors():
    if tag in actor.tags: actors.destroy_actor(actor)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
mats={k:unreal.load_asset('/Game/Materials/Depot/'+v) for k,v in {
    'wood':'M_D_Wood','dark':'M_D_DarkSteel','yellow':'M_D_SafetyYellow',
    'white':'M_D_RoadPaint','green':'M_D_ExtractionLight','steel':'M_D_Galvanized'}.items()}
report={'standing_cover':[],'light_changes':[]}
def box(name,pos,size,mat,collision=False,yaw=0):
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*pos),unreal.Rotator(0,yaw,0))
    actor.set_actor_label(name); actor.tags=[tag]; actor.set_folder_path('R3_Readability')
    mesh=actor.static_mesh_component; mesh.set_static_mesh(cube); mesh.set_material(0,mats[mat])
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*(s/100 for s in size))); return actor
def sign(name,text,pos,yaw,size=24,color=(220,228,210,255)):
    actor=actors.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(*pos),unreal.Rotator(0,yaw,0))
    actor.set_actor_label(name); actor.tags=[tag]; actor.set_folder_path('R3_Readability')
    actor.text_render.set_text(text); actor.text_render.set_world_size(size)
    actor.text_render.set_text_render_color(unreal.Color(*color))
    actor.text_render.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return actor
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.SkyLight):
        actor.light_component.set_intensity(1.35); report['light_changes'].append('sky fill 0.7 -> 1.35')
    if isinstance(actor,unreal.DirectionalLight):
        actor.light_component.set_intensity(2.6); report['light_changes'].append('sun 2.3 -> 2.6')

# Covers interrupt exposed approaches to the north and eastern exit. The original
# south service lane, loot locations, guard routes and extraction trigger stay open.
for index,(x,y) in enumerate([(-590,760),(1660,590)]):
    box('R3_StandingCover_'+str(index),(x,y,100),(150,130,200),'wood',True)
    for z in (15,100,190): box('R3_CrateBinding',(x,y-67,z),(154,5,12),'dark')
    for dx in (-67,67): box('R3_CrateCorner',(x+dx,y-67,100),(8,5,190),'steel')
    box('R3_CoverWarning',(x,y-71,165),(76,3,26),'yellow')
    sign('R3_CoverCargoLabel','CARGO',(x,y-74,161),-90,17,(40,45,33,255))
    report['standing_cover'].append([x,y,200])

box('R3_MedicalBoard',(-1380,800,163),(190,14,112),'dark',True)
for x in (-1460,-1300): box('R3_MedicalBoardPost',(x,800,70),(10,10,140),'steel',True)
box('R3_MedicalCrossV',(-1423,790,173),(12,5,50),'green')
box('R3_MedicalCrossH',(-1423,790,173),(50,5,12),'green')
sign('R3_MedicalLabel','AID',(-1345,790,173),-90,29)
sign('R3_MedicalSub','FIELD SUPPLIES',(-1380,790,137),-90,15)
for x,y in [(1410,1000),(1660,1130)]:
    box('R3_ExfilRouteLine',(x,y,1.8),(110,12,1),'green',yaw=28)
    for angle,dy in [(-7,-13),(63,13)]:
        box('R3_ExfilRouteArrow',(x+54,y+26+dy,1.9),(56,9,1),'green',yaw=angle)
assert levels.save_current_level()
(ROOT/'Evidence/R3ScenePolish.json').write_text(json.dumps(report,indent=2))
unreal.log('R3_SCENE_POLISH_OK '+json.dumps(report))
