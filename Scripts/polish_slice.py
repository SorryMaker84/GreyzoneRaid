"""Small, repeatable visual correction pass on the existing Depot level."""
import math
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
assert levels.load_level('/Game/Maps/Depot')
tag = unreal.Name('DepotPolish')
for actor in actors.get_all_level_actors():
    if tag in actor.tags:
        actors.destroy_actor(actor)

cube = unreal.load_asset('/Engine/BasicShapes/Cube.Cube')
matroot = '/Game/Materials/Depot/'
def material(name):
    obj = unreal.load_asset(matroot + name + '.' + name)
    assert obj, name
    return obj

def box(name, location, dimensions, mat, collision=False):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.tags = [tag]
    actor.set_folder_path('Polish')
    mesh = actor.static_mesh_component
    mesh.set_static_mesh(cube)
    mesh.set_material(0, mat)
    mesh.set_collision_enabled(unreal.CollisionEnabled.QUERY_AND_PHYSICS if collision else unreal.CollisionEnabled.NO_COLLISION)
    actor.set_actor_scale3d(unreal.Vector(*(v / 100 for v in dimensions)))
    return actor

# A calibrated sky cubemap gives readable diffuse fill independently of sun angle.
sky_cube = unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap')
assert sky_cube
for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.DirectionalLight):
        actor.light_component.set_intensity(2.3)
        actor.light_component.set_light_color(unreal.LinearColor(1.0, 0.96, 0.88, 1))
        actor.light_component.set_editor_property('light_source_angle', 1.8)
    elif isinstance(actor, unreal.SkyLight):
        sky = actor.light_component
        sky.set_editor_property('real_time_capture', False)
        sky.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sky.set_cubemap(sky_cube)
        sky.set_intensity(0.7)
        sky.set_editor_property('lower_hemisphere_is_black', False)
    elif isinstance(actor, unreal.PointLight):
        actor.light_component.set_intensity(90.0)
    # Round, bright discs made the original oil spots look like temporary markers.
    if 'OilSpot' in actor.get_actor_label() or 'OilPatch' in actor.get_actor_label():
        actors.destroy_actor(actor)

# Replace sparse crossing bars with a continuous, fine diamond wire pattern.
fence_path = '/Game/Materials/M_ChainLink'
fence_mat = assets.load_asset(fence_path) if assets.does_asset_exist(fence_path) else None
if fence_mat is None:
    fence_mat = tools.create_asset('M_ChainLink', '/Game/Materials', unreal.Material, unreal.MaterialFactoryNew())
    fence_mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    fence_mat.set_editor_property('two_sided', True)
    def node(cls, **props):
        obj = mel.create_material_expression(fence_mat, cls)
        for key, value in props.items(): obj.set_editor_property(key, value)
        return obj
    def scalar(v): return node(unreal.MaterialExpressionConstant, r=v)
    def link(a, b, pin=''): assert mel.connect_material_expressions(a, '', b, pin)
    def binary(cls, a, b):
        out = node(cls); link(a, out, 'A'); link(b, out, 'B'); return out
    position = node(unreal.MaterialExpressionWorldPosition)
    horizontal = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
    # Use separate masks so both X-oriented and Y-oriented walls share this graph.
    x = node(unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
    y = node(unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    z = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
    for axis in (x, y, z): link(position, axis)
    along = binary(unreal.MaterialExpressionAdd, x, y)
    stripes = []
    for cls in (unreal.MaterialExpressionAdd, unreal.MaterialExpressionSubtract):
        diagonal = binary(cls, along, z)
        tiled = binary(unreal.MaterialExpressionMultiply, diagonal, scalar(1.0 / 28.0))
        fract = node(unreal.MaterialExpressionFrac); link(tiled, fract)
        center = binary(unreal.MaterialExpressionSubtract, fract, scalar(0.5))
        absolute = node(unreal.MaterialExpressionAbs); link(center, absolute)
        edge = binary(unreal.MaterialExpressionSubtract, absolute, scalar(0.46))
        scaled = binary(unreal.MaterialExpressionMultiply, edge, scalar(30.0))
        clamp = node(unreal.MaterialExpressionSaturate); link(scaled, clamp)
        stripes.append(clamp)
    opacity = binary(unreal.MaterialExpressionMax, *stripes)
    color = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.12,.15,.16,1))
    assert mel.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    assert mel.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    assert mel.connect_material_property(scalar(.65), '', unreal.MaterialProperty.MP_METALLIC)
    assert mel.connect_material_property(scalar(.58), '', unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(fence_mat)
    assert assets.save_loaded_asset(fence_mat)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label().startswith('PerimeterMesh_Wire'):
        actors.destroy_actor(actor)
for name, location, dimensions in [
    ('North', (0,1900,380), (4800,1,180)), ('South', (0,-1900,380), (4800,1,180)),
    ('East', (2400,0,380), (1,3800,180)), ('West', (-2400,0,380), (1,3800,180))]:
    panel = box('FineFence_' + name, location, dimensions, fence_mat)
    panel.static_mesh_component.set_cast_shadow(False)

# Extraction is a sheltered checkpoint: a roof, gate frame and green identification.
steel, blue = material('M_D_DarkSteel'), material('M_D_BlueSteel')
yellow, green = material('M_D_SafetyYellow'), material('M_D_ExtractionLight')
for y in (1075,1725):
    box('Extraction_PortalColumn', (2290,y,195), (32,32,390), steel)
box('Extraction_PortalRoof', (2160,1400,407), (330,700,28), blue)
box('Extraction_PortalFascia', (1982,1400,383), (18,700,55), steel)
box('Extraction_GreenIdentifier', (1970,1400,393), (4,600,8), green)
for y in (1170,1240,1310,1380,1450,1520,1590,1660):
    box('Extraction_GateSlat', (2370,y,130), (15,38,260), blue)
box('Extraction_Threshold', (2320,1400,4), (75,630,8), yellow)

# Break up the very long service road without blocking the test lane at y=-1400.
concrete = material('M_D_Concrete')
for x in (-1250,200,1500):
    box('RoadEdge_Bollard', (x,-1820,49), (30,30,98), concrete)
    box('RoadEdge_Reflector', (x,-1820,86), (32,32,14), yellow)

assert levels.save_current_level()
(root/'Evidence'/'VisualPolish.json').write_text(json.dumps({
    'sky_cubemap': sky_cube.get_path_name(), 'sun_intensity': 2.3, 'sky_intensity': .7,
    'lamp_intensity': 90, 'fine_chain_link': True, 'extraction_checkpoint_roof': True
}, indent=2), encoding='utf-8')
unreal.log('VISUAL_POLISH_COMPLETE: calibrated lighting, diamond fence, extraction checkpoint')
