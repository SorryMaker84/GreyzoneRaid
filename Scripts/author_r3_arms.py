"""Bake rifle grip, trigger action and a timed two-bone reload from retained CC0 rig.
Animations are independent assets; the R2 and original author animations remain intact.
"""
from pathlib import Path
import json
import math
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve()
# Reuse only the pure quaternion/coordinate helpers, not the R2 authoring entry point.
helper=(ROOT/'Scripts/author_grip_pose.py').read_text().split('overrides={}')[0]
exec(compile(helper,'R2_pose_math','exec'))

def smooth(t):
    t=max(0,min(1,t)); return t*t*(3-2*t)
def lerp(a,b,t): return add(mul(a,1-t),mul(b,t))

def pose(left_wrist,grasp=0,trigger=0):
    overrides={}
    def set_world(name,position,rotation):
        parent=L[name]['parent']; p=overrides.get(parent,W.get(parent))
        t=[x/s for x,s in zip(qr(qi(p['rotation']),sub(position,p['translation'])),p['scale'])] if p else position
        q=qm(qi(p['rotation']),rotation) if p else rotation
        overrides[name]={'translation':position,'rotation':rotation,'scale':W[name]['scale'],'lt':t,'lq':q}
    for side,shoulder,wrist,pole,forward,across in [
        ('R',[-24,20,-28],[3,3.5,-12],[-14,39,-35],[1,0,.2],[.2,0,-1]),
        ('L',[2,-18,-22],left_wrist,[10,-38,-30],lerp([.15,0,1],[.4,0,.9],grasp),[-1,0,.15]),
    ]:
        upper,fore,end='upper_arm_'+side,'forearm_'+side,'forearm_'+side+'_end'
        old_s,old_e,old_w=(mul(W[n]['translation'],.1) for n in (upper,fore,end))
        a,b=length(sub(old_e,old_s)),length(sub(old_w,old_e))
        direction=unit(sub(wrist,shoulder)); d=min(length(sub(wrist,shoulder)),a+b-.05)
        along=(a*a-b*b+d*d)/(2*d); height=math.sqrt(max(0,a*a-along*along))
        bend=unit(sub(sub(pole,shoulder),mul(direction,dot(sub(pole,shoulder),direction))))
        elbow=add(add(shoulder,mul(direction,along)),mul(bend,height))
        up_q=qm(between(sub(old_e,old_s),sub(elbow,shoulder)),W[upper]['rotation'])
        fore_q=qm(between(sub(old_w,old_e),sub(wrist,elbow)),W[fore]['rotation'])
        delta=mul(sub(shoulder,old_s),10)
        for bone in ('clavicle_'+side,'deltoid_'+side): set_world(bone,add(W[bone]['translation'],delta),W[bone]['rotation'])
        set_world(upper,mul(shoulder,10),up_q); set_world(fore,mul(elbow,10),fore_q); set_world(end,mul(wrist,10),fore_q)
        hand='hand_'+side
        src_forward=sub(W['f_middle_01_'+side]['translation'],W[hand]['translation'])
        src_across=sub(W['f_pinky_01_'+side]['translation'],W['f_index_01_'+side]['translation'])
        hand_q=qm(qm(frame_quat(forward,across),qi(frame_quat(src_forward,src_across))),W[hand]['rotation'])
        control='hand_'+side+'_control'; set_world(control,mul(wrist,10),W[control]['rotation']); set_world(hand,mul(wrist,10),hand_q)
    result={}
    for name in names:
        t=L[name]['translation']; q=L[name]['rotation']; s=L[name]['scale']
        if name in overrides: t,q=overrides[name]['lt'],overrides[name]['lq']
        if name.startswith('f_') and not name.endswith('_end'):
            segment=int(name.split('_')[-2]); bend={1:45,2:70,3:55}[segment]
            if name.startswith('f_index_') and name.endswith('_R'): bend={1:10,2:30,3:25}[segment]+trigger*12
            q=qm(q,axis([1,0,0],math.radians(-bend)))
        if name.startswith('thumb_03_') and not name.endswith('_end'): q=qm(q,axis([1,0,0],math.radians(-25)))
        result[name]=(t,q,s)
    return result

REST=[43,1,-3]
KEYS=[(0,REST),(.20,[22,-4,-12]),(.36,[20,-4,-20]),(.54,[14,-4,-27]),(.67,[20,-4,-18]),(.79,[22,-4,-12]),(1,REST)]
def reload_pose(t):
    for i in range(len(KEYS)-1):
        ta,a=KEYS[i]; tb,b=KEYS[i+1]
        if ta <= t <= tb:
            wrist=lerp(a,b,smooth((t-ta)/(tb-ta))); break
    else: wrist=REST
    grasp=min(smooth(t/.20),smooth((1-t)/.21))
    return pose(wrist,grasp)

report={}
for name,frames,rate,eval_pose in [
    ('A_FPSArms_Grip_R3',1,24,lambda t:pose(REST)),
    ('A_FPSArms_Reload_R3',36,24,reload_pose),
    ('A_FPSArms_Fire_R3',5,24,lambda t:pose(REST,0,math.sin(t*math.pi))),
]:
    path='/Game/Weapons/'+name
    dest=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Weapons/A_FPSArms_Idle',path)
    controller=dest.get_editor_property('controller')
    if controller is None: controller=unreal.AnimDataController(outer=dest)
    model=dest.get_editor_property('data_model_interface'); controller.set_model(model)
    samples=[eval_pose(i/frames) for i in range(frames+1)]
    controller.open_bracket('Author R3 rifle animation',False)
    try:
        controller.set_frame_rate(unreal.FrameRate(rate,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(frames),False)
        for bone in names:
            if not model.is_valid_bone_track_name(bone): assert controller.add_bone_curve(bone,False),bone
            positions=[unreal.Vector(*p[bone][0]) for p in samples]
            rotations=[unreal.Quat(*p[bone][1]) for p in samples]
            scales=[unreal.Vector(*p[bone][2]) for p in samples]
            assert controller.set_bone_track_keys(bone,positions,rotations,scales,False),bone
    finally: controller.close_bracket(False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(dest)
    report[name]={'asset':path,'seconds':dest.get_play_length(),'frames':frames,'bones':len(names)}
(ROOT/'Evidence/R3ArmsAnimations.json').write_text(json.dumps(report,indent=2))
unreal.log('R3_ARMS_ANIMATIONS_SAVED '+json.dumps(report))
