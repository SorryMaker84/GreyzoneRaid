"""Author a constant rifle grip from the imported CC0 rig, in weapon-root cm.

Keeps the source animation unchanged. Arms use identity placement and scale .1.
Explicit two-bone arm geometry and hand frames avoid exporter-axis assumptions.
"""
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DATA = json.loads((ROOT / 'Evidence/ArmsPoseSample.json').read_text())
L, W = DATA['LOCAL'], DATA['WORLD']
names = DATA['bone_names']

def add(a,b): return [x+y for x,y in zip(a,b)]
def sub(a,b): return [x-y for x,y in zip(a,b)]
def mul(a,s): return [x*s for x in a]
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def length(a): return math.sqrt(dot(a,a))
def unit(a): return mul(a,1/max(length(a),1e-9))
def qm(a,b):
    av,bv=a[:3],b[:3]
    return add(add(mul(bv,a[3]),mul(av,b[3])),cross(av,bv))+[a[3]*b[3]-dot(av,bv)]
def qi(q): return [-q[0],-q[1],-q[2],q[3]]
def qr(q,v): return qm(qm(q,v+[0]),qi(q))[:3]
def axis(a,angle): return mul(unit(a),math.sin(angle/2))+[math.cos(angle/2)]
def between(a,b):
    a,b=unit(a),unit(b)
    q=cross(a,b)+[1+dot(a,b)]
    return mul(q,1/length(q))
def frame_quat(forward,across):
    x=unit(forward); y=unit(sub(across,mul(x,dot(across,x)))); z=cross(x,y)
    # Matrix columns are the orthonormal frame axes.
    m=[[x[i],y[i],z[i]] for i in range(3)]
    t=m[0][0]+m[1][1]+m[2][2]
    if t>0:
        s=math.sqrt(t+1)*2
        return [(m[2][1]-m[1][2])/s,(m[0][2]-m[2][0])/s,(m[1][0]-m[0][1])/s,s/4]
    i=max(range(3),key=lambda j:m[j][j]); j=(i+1)%3; k=(i+2)%3
    s=math.sqrt(1+m[i][i]-m[j][j]-m[k][k])*2
    q=[0,0,0,0]; q[i]=s/4; q[j]=(m[i][j]+m[j][i])/s; q[k]=(m[i][k]+m[k][i])/s; q[3]=(m[k][j]-m[j][k])/s
    return q

overrides={}
def set_world(name,position,rotation):
    parent=L[name]['parent']
    p=overrides.get(parent,W.get(parent))
    if p:
        local_position=[x/s for x,s in zip(qr(qi(p['rotation']),sub(position,p['translation'])),p['scale'])]
        local_rotation=qm(qi(p['rotation']),rotation)
    else: local_position,local_rotation=position,rotation
    overrides[name]={'translation':position,'rotation':rotation,'scale':W[name]['scale'],
                     'local_translation':local_position,'local_rotation':local_rotation}

for side,shoulder,wrist,pole,forward,across in [
    ('R',[-24,20,-28],[3,3.5,-12],[-14,39,-35],[1,0,.2],[.2,0,-1]),
    ('L',[2,-18,-22],[42,2,-3],[10,-38,-30],[.15,0,1],[-1,0,.15]),
]:
    upper,fore,end='upper_arm_'+side,'forearm_'+side,'forearm_'+side+'_end'
    old_s,old_e,old_w=(mul(W[n]['translation'],.1) for n in (upper,fore,end))
    a,b=length(sub(old_e,old_s)),length(sub(old_w,old_e))
    direction=unit(sub(wrist,shoulder)); d=min(length(sub(wrist,shoulder)),a+b-.05)
    along=(a*a-b*b+d*d)/(2*d)
    height=math.sqrt(max(0,a*a-along*along))
    bend=unit(sub(sub(pole,shoulder),mul(direction,dot(sub(pole,shoulder),direction))))
    elbow=add(add(shoulder,mul(direction,along)),mul(bend,height))
    up_q=qm(between(sub(old_e,old_s),sub(elbow,shoulder)),W[upper]['rotation'])
    fore_q=qm(between(sub(old_w,old_e),sub(wrist,elbow)),W[fore]['rotation'])
    # Move the shoulder's parent with it so the shoulder skin stays connected.
    delta=mul(sub(shoulder,old_s),10)
    for bone in ('clavicle_'+side,'deltoid_'+side):
        set_world(bone,add(W[bone]['translation'],delta),W[bone]['rotation'])
    set_world(upper,mul(shoulder,10),up_q)
    set_world(fore,mul(elbow,10),fore_q)
    set_world(end,mul(wrist,10),fore_q)
    hand='hand_'+side
    src_forward=sub(W['f_middle_01_'+side]['translation'],W[hand]['translation'])
    src_across=sub(W['f_pinky_01_'+side]['translation'],W['f_index_01_'+side]['translation'])
    delta_q=qm(frame_quat(forward,across),qi(frame_quat(src_forward,src_across)))
    hand_q=qm(delta_q,W[hand]['rotation'])
    control='hand_'+side+'_control'
    set_world(control,mul(wrist,10),W[control]['rotation'])
    set_world(hand,mul(wrist,10),hand_q)

source=unreal.load_asset('/Game/Weapons/A_FPSArms_Idle')
path='/Game/Weapons/A_FPSArms_Grip'
dest=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Weapons/A_FPSArms_Idle',path)
controller=dest.get_editor_property('controller')
if controller is None: controller=unreal.AnimDataController(outer=dest)
controller.set_model(dest.get_editor_property('data_model_interface'))
model=dest.get_editor_property('data_model_interface')
controller.open_bracket('Author rifle hold',False)
try:
    controller.set_number_of_frames(unreal.FrameNumber(1),False)
    for name in names:
        p=L[name]
        t=p['translation']; q=p['rotation']; s=p['scale']
        if name in overrides:
            t=overrides[name]['local_translation']; q=overrides[name]['local_rotation']
        if name.startswith('f_') and not name.endswith('_end'):
            segment=int(name.split('_')[-2])
            bend={1:45,2:70,3:55}[segment]
            if name.startswith('f_index_') and name.endswith('_R'): bend={1:10,2:30,3:25}[segment]
            q=qm(q,axis([1,0,0],math.radians(-bend)))
        if name.startswith('thumb_03_') and not name.endswith('_end'):
            q=qm(q,axis([1,0,0],math.radians(-25)))
        if not model.is_valid_bone_track_name(name): assert controller.add_bone_curve(name,False),name
        tv,qv,sv=unreal.Vector(*t),unreal.Quat(*q),unreal.Vector(*s)
        assert controller.set_bone_track_keys(name,[tv,tv],[qv,qv],[sv,sv],False),name
finally: controller.close_bracket(False)
assert unreal.EditorAssetLibrary.save_loaded_asset(dest)
(ROOT/'Evidence/AuthoredGrip.json').write_text(json.dumps({'asset':path,'overrides':overrides},indent=2))
unreal.log('R2_AUTHORED_GRIP_SAVED '+path)
