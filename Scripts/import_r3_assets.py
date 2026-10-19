"""Import verified CC0 rifle parts and apply an original cloth glove material."""
from pathlib import Path
import json
import unreal
ROOT=Path(unreal.Paths.project_dir()).resolve()
# Read only reusable import functions, never execute R2's import entry point.
helpers=(ROOT/'Scripts/import_assets.py').read_text().split('# SoundWave assets')[0]
exec(compile(helpers,'R2_import_helpers','exec'))
material=unreal.load_asset('/Game/Weapons/M_ClassicRifle')
for source,name in [('RifleBody','SM_RifleBody_R3'),('RifleMagazine','SM_RifleMagazine_R3')]:
    mesh,_=import_file(ROOT/'Downloads/Models/R3RifleParts'/ (source+'.fbx'),'/Game/Weapons',name,fbx_options(False,False),unreal.StaticMesh)
    for slot in range(len(mesh.get_editor_property('static_materials'))): mesh.set_material(slot,material)
    assert ASSETS.save_loaded_asset(mesh)
    REPORT['assets'][name]=mesh_info(mesh)

path='/Game/Materials/M_R3GloveFabric'
fabric=unreal.load_asset(path) if ASSETS.does_asset_exist(path) else None
if fabric is None:
    fabric=TOOLS.create_asset('M_R3GloveFabric','/Game/Materials',unreal.Material,unreal.MaterialFactoryNew())
    fabric.set_editor_property('used_with_skeletal_mesh',True)
    def node(cls,**props):
        n=MAT.create_material_expression(fabric,cls)
        for key,value in props.items(): n.set_editor_property(key,value)
        return n
    def connect(a,b,pin='',output=''): assert MAT.connect_material_expressions(a,output,b,pin)
    def scalar(v): return node(unreal.MaterialExpressionConstant,r=v)
    def binary(cls,a,b):
        n=node(cls); connect(a,n,'A'); connect(b,n,'B'); return n
    uv=node(unreal.MaterialExpressionTextureCoordinate,u_tiling=170.,v_tiling=170.)
    bands=[]
    for red in (True,False):
        mask=node(unreal.MaterialExpressionComponentMask,r=red,g=not red,b=False,a=False); connect(uv,mask)
        wave=node(unreal.MaterialExpressionSine); connect(mask,wave); bands.append(wave)
    weave=binary(unreal.MaterialExpressionMultiply,*bands)
    variation=binary(unreal.MaterialExpressionAdd,scalar(.88),binary(unreal.MaterialExpressionMultiply,weave,scalar(.12)))
    color=node(unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(.067,.079,.046,1))
    base=binary(unreal.MaterialExpressionMultiply,color,variation)
    MAT.connect_material_property(base,'',unreal.MaterialProperty.MP_BASE_COLOR)
    MAT.connect_material_property(scalar(.92),'',unreal.MaterialProperty.MP_ROUGHNESS)
    MAT.recompile_material(fabric); assert ASSETS.save_loaded_asset(fabric)
arms=unreal.load_asset('/Game/Weapons/SK_FPSArms')
slots=arms.get_editor_property('materials')
for i in range(len(slots)):
    slot=slots[i]
    slot.set_editor_property('material_interface',fabric)
    slots[i]=slot
arms.set_editor_property('materials',slots); assert ASSETS.save_loaded_asset(arms,only_if_is_dirty=False)
REPORT['assets']['arms_material']=fabric.get_path_name()
REPORT['assets']['arms_material_slots']=[s.get_editor_property('material_interface').get_path_name() for s in arms.get_editor_property('materials')]
(ROOT/'Evidence/ImportedAssetsR3.json').write_text(json.dumps(REPORT,indent=2))
unreal.log('R3_ASSET_IMPORT_OK '+json.dumps(REPORT['assets']))
