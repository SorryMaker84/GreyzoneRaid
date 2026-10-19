"""Run once in UE Editor's Python commandlet to make editable assets and a map.
All geometry comes from Unreal's installed Engine/BasicShapes assets.
"""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
matlib = unreal.MaterialEditingLibrary

for directory in ["/Game/Maps", "/Game/Materials", "/Game/Blueprints"]:
    assets.make_directory(directory)

def material(name, rgb, glow=False):
    path = "/Game/Materials/" + name
    mat = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not mat:
        mat = asset_tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
        color = matlib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -350, 0)
        color.set_editor_property("constant", unreal.LinearColor(*rgb, 1.0))
        matlib.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        roughness = matlib.create_material_expression(mat, unreal.MaterialExpressionConstant, -350, 180)
        roughness.set_editor_property("r", 0.75)
        matlib.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
        if glow:
            matlib.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        matlib.recompile_material(mat)
        assets.save_asset(path)
    return mat

mats = {
    "ground": material("M_Ground", (0.09, 0.13, 0.17)),
    "wall": material("M_Wall", (0.28, 0.35, 0.40)),
    "container": material("M_Container", (0.10, 0.34, 0.42)),
    "orange": material("M_Orange", (0.75, 0.29, 0.06)),
    "mark": material("M_Marking", (0.64, 0.70, 0.64)),
    "green": material("M_Extraction", (0.04, 0.8, 0.22), True),
}

def blueprint(name, parent):
    path = "/Game/Blueprints/" + name
    bp = assets.load_asset(path) if assets.does_asset_exist(path) else None
    if not bp:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        bp = asset_tools.create_asset(name, "/Game/Blueprints", unreal.Blueprint, factory)
        assets.save_asset(path)
    return assets.load_blueprint_class(path)

enemy_class = blueprint("BP_RaidEnemy", unreal.RaidEnemy)
loot_class = blueprint("BP_RaidLoot", unreal.RaidLoot)
extract_class = blueprint("BP_Extraction", unreal.RaidExtractionZone)
game_class = blueprint("BP_RaidGameMode", unreal.RaidGameMode)

assert levels.new_level("/Game/Maps/RaidYard"), "RaidYard already exists or could not be created; existing map preserved."
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", game_class)

cube_mesh = assets.load_asset("/Engine/BasicShapes/Cube")
def block(name, xyz, size, mat="wall", yaw=0):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*xyz), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    actor.set_actor_label(name)
    component = actor.static_mesh_component
    component.set_static_mesh(cube_mesh)
    component.set_material(0, mats[mat])
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name("BlockAll")
    actor.set_actor_scale3d(unreal.Vector(*(s/100.0 for s in size)))
    return actor

# Centimetres: a 48 x 38 metre yard, with cover and three routes.
block("Yard_Floor", (0, 0, -50), (4800, 3800, 100), "ground")
block("North_Wall", (0, 1950, 220), (5000, 100, 540))
block("South_Wall", (0, -1950, 220), (5000, 100, 540))
block("East_Wall", (2450, 0, 220), (100, 4000, 540))
block("West_Wall", (-2450, 0, 220), (100, 4000, 540))
for x, y, sx, sy in [(-750, -500, 800, 300), (300, 650, 800, 300), (1200, -450, 300, 700)]:
    block("Cargo_Container", (x, y, 150), (sx, sy, 300), "container")
    block("Container_Top_Rail", (x, y, 310), (sx+20, sy+20, 20), "mark")
for x, y in [(-1450, 350), (-300, -1000), (650, -1100), (1400, 1100), (100, 0)]:
    block("Low_Cover", (x, y, 65), (180, 180, 130), "orange")
for x in range(-1900, 2100, 400):
    block("Lane_Marking", (x, -1400, 1), (150, 12, 2), "mark")
for x, y in [(-1500, 350), (650, -1100)]:
    block("Stacked_Crate", (x, y, 155), (125, 125, 50), "orange")

start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-2000, -1400, 100), unreal.Rotator(pitch=0, yaw=15, roll=0))
start.set_actor_label("Insertion_West")

def text(label, message, xyz, yaw=180, color=(220,240,245,255), size=80):
    a = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*xyz), unreal.Rotator(pitch=0,yaw=yaw,roll=0))
    a.set_actor_label(label)
    c = a.get_component_by_class(unreal.TextRenderComponent)
    c.set_text(message)
    c.set_world_size(size)
    c.set_text_render_color(unreal.Color(*color))
    c.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return a

text("Sign_Extraction", "EXTRACTION  /  08 SECONDS", (2310,1200,300), 180, (90,255,130,255), 52)
text("Sign_Cargo", "CARGO  07", (-750,-660,230), -90, size=65)
text("Sign_Greyzone", "GREYZONE  /  STORAGE YARD", (2350,-950,310), 180, size=60)

for idx, (x,y,z,item,label,value) in enumerate([
    (-1550,-1250,48,"Tech","Electronics",120),
    (-1380,700,48,"Medkit","Medkit",75),
    (-290,-1000,154,"Rations","Field rations",40),
    (320,900,48,"Tech","Electronics",120),
    (700,-1100,206,"Medkit","Medkit",75),
    (1660,-950,48,"Tech","Data drive",180),
    (1650,1150,48,"Rations","Field rations",40),
    (1750,900,48,"Tech","Electronics",120),
    (650,1550,48,"Tech","Data drive",180),
]):
    a = actors.spawn_actor_from_class(loot_class, unreal.Vector(x,y,z))
    a.set_actor_label("Loot_%02d_%s" % (idx,item))
    for key,val in [("item_id",item),("display_name",label),("value",value)]:
        a.set_editor_property(key,val)

for idx, (x,y,points) in enumerate([
    (-250,1000,[(-700,1200,95),(-250,1000,95),(250,1400,95)]),
    (900,-800,[(900,-800,95),(1800,-1250,95),(1800,0,95)]),
    (1550,1300,[(1550,1300,95),(1800,600,95),(900,1450,95)]),
]):
    a = actors.spawn_actor_from_class(enemy_class, unreal.Vector(x,y,96))
    a.set_actor_label("Guard_%02d" % (idx+1))
    a.set_editor_property("patrol_points", [unreal.Vector(*p) for p in points])

extraction = actors.spawn_actor_from_class(extract_class, unreal.Vector(2010,1400,10))
extraction.set_actor_label("Extraction_East")
nav = actors.spawn_actor_from_class(unreal.RaidNavBounds, unreal.Vector(0,0,250))
nav.set_actor_label("Navigation_Bounds")

sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,1000), unreal.Rotator(pitch=-48,yaw=-25,roll=0))
sun.light_component.set_editor_property("intensity", 3.5)
sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0,0,500))
sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("intensity", 0.8)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0,0,0))
fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0,0,-50))
fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property("fog_density", 0.008)

assert levels.save_current_level(), "Failed to save RaidYard"
assert assets.save_directory("/Game", only_if_is_dirty=False, recursive=True), "Failed to save project assets"
unreal.log("RAID_ASSET_BUILD_COMPLETE: /Game/Maps/RaidYard with native gameplay and editable Blueprint subclasses")
