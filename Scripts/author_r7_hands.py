"""Author grip/contact poses against the retained rifle, without changing textures.

Weapon-root centimetres; imported arm rig has 100x root and .1 component scale.
The original R3 animations remain untouched. Reload hand follows the SAME rigid
magazine transform used by RaidPlayer, including its pivot/rock rotation.
"""
import json
import math
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
helper = (ROOT/'Scripts/author_grip_pose.py').read_text().split('overrides={}')[0]
exec(compile(helper, 'retained_pose_coordinate_math', 'exec'))

def smooth(t):
    t = max(0., min(1., t))
    return t*t*(3.-2.*t)
def lerp(a,b,t): return add(mul(a,1-t),mul(b,t))

REST = [38.,-12.,0.]
GRAB = [12.,-4.,-8.]
PIVOT = [22.,0.,-8.]
TIMES = [0.,.20,.36,.54,.67,.79,1.]
OFFSETS = [[0,0,0],[0,0,0],[1.5,0,-6],[4,0,-10],[1.5,0,-5],[0,0,0],[0,0,0]]

def magazine(t):
    off = [0,0,0]
    for i in range(6):
        if TIMES[i] <= t <= TIMES[i+1]:
            off = lerp(OFFSETS[i],OFFSETS[i+1],smooth((t-TIMES[i])/(TIMES[i+1]-TIMES[i])))
            break
    # UE positive Pitch rotates +X toward +Z: negative mathematical Y rotation.
    q = axis([0,1,0], math.radians(off[2]*.45))
    return off,q

def pose(t=0., reloading=False, trigger=0.):
    grasp = min(smooth(t/.20),smooth((1-t)/.21)) if reloading else 0.
    off,rock = magazine(t) if reloading else ([0,0,0],[0,0,0,1])
    mag_wrist = add(add(PIVOT,off),qr(rock,sub(GRAB,PIVOT)))
    wrist_l = lerp(REST,mag_wrist,grasp)
    forward_l = unit(lerp([.20,.92,.34],qr(rock,[.80,.05,.60]),grasp))
    across_l = unit(lerp([-.98,.20,0],qr(rock,[.60,0,-.80]),grasp))
    overrides = {}
    def set_world(name,position,rotation):
        parent=L[name]['parent']; p=overrides.get(parent,W.get(parent))
        lt=[x/s for x,s in zip(qr(qi(p['rotation']),sub(position,p['translation'])),p['scale'])] if p else position
        lq=qm(qi(p['rotation']),rotation) if p else rotation
        overrides[name]={'translation':position,'rotation':rotation,'scale':W[name]['scale'],'lt':lt,'lq':lq}
    for side,shoulder,wrist,pole,forward,across in [
        ('R',[-24,20,-28],[-4,3,-5],[-14,36,-30],[.65,0,.76],[.76,0,-.65]),
        ('L',[3,-25,-22],wrist_l,[30,-43,-22],forward_l,across_l),
    ]:
        upper,fore,end='upper_arm_'+side,'forearm_'+side,'forearm_'+side+'_end'
        old_s,old_e,old_w=(mul(W[n]['translation'],.1) for n in (upper,fore,end))
        a,b=length(sub(old_e,old_s)),length(sub(old_w,old_e))
        d=length(sub(wrist,shoulder))
        assert abs(a-b)+.01 < d < a+b-.01, (side,'unreachable wrist',d,a+b)
        direction=unit(sub(wrist,shoulder))
        along=(a*a-b*b+d*d)/(2*d); height=math.sqrt(max(0,a*a-along*along))
        bend=unit(sub(sub(pole,shoulder),mul(direction,dot(sub(pole,shoulder),direction))))
        elbow=add(add(shoulder,mul(direction,along)),mul(bend,height))
        hand='hand_'+side
        src_forward=sub(W['f_middle_01_'+side]['translation'],W[hand]['translation'])
        src_across=sub(W['f_pinky_01_'+side]['translation'],W['f_index_01_'+side]['translation'])
        hand_delta=qm(frame_quat(forward,across),qi(frame_quat(src_forward,src_across)))
        hand_q=qm(hand_delta,W[hand]['rotation'])
        up_q=qm(between(sub(old_e,old_s),sub(elbow,shoulder)),W[upper]['rotation'])
        # Match forearm roll to the hand's anatomical frame before solving its axis.
        # A direction-only rotation preserves an unrelated old roll and twists the wrist.
        fore_delta=qm(between(qr(hand_delta,sub(old_w,old_e)),sub(wrist,elbow)),hand_delta)
        fore_q=qm(fore_delta,W[fore]['rotation'])
        delta=mul(sub(shoulder,old_s),10)
        for bone in ('clavicle_'+side,'deltoid_'+side): set_world(bone,add(W[bone]['translation'],delta),W[bone]['rotation'])
        set_world(upper,mul(shoulder,10),up_q)
        set_world(fore,mul(elbow,10),fore_q)
        set_world(end,mul(wrist,10),fore_q)
        control='hand_'+side+'_control'
        set_world(control,mul(wrist,10),W[control]['rotation'])
        set_world(hand,mul(wrist,10),hand_q)
    result={}
    for name in names:
        tr,q,sc=L[name]['translation'],L[name]['rotation'],L[name]['scale']
        if name in overrides: tr,q=overrides[name]['lt'],overrides[name]['lq']
        if name.startswith('f_') and not name.endswith('_end'):
            segment=int(name.split('_')[-2])
            # These are additional curls over the source's existing 10/17/5 degrees.
            bend={1:24.,2:37.,3:25.}[segment]
            if name.endswith('_L'): bend += grasp*{1:4.,2:4.,3:0.}[segment]
            if name.startswith('f_index_') and name.endswith('_R'):
                bend={1:5.,2:14.,3:12.}[segment]+trigger*5.
            q=qm(q,axis([1,0,0],math.radians(-bend)))
        if name.startswith('thumb_03_') and not name.endswith('_end'):
            q=qm(q,axis([1,0,0],math.radians(-12)))
        result[name]=(tr,q,sc)
    # Oppose the thumb onto the actual contact instead of preserving the source's
    # open, abducted thumb. Finger axes differ from the arm and are not Euler rolls.
    world={}
    remaining=set(names)
    while remaining:
        for n in list(remaining):
            parent=L[n]['parent']
            if parent and parent not in world: continue
            tr,q,sc=result[n]
            if parent:
                pt,pq,ps=world[parent]
                tr=add(pt,qr(pq,[a*b for a,b in zip(tr,ps)]));q=qm(pq,q);sc=[a*b for a,b in zip(sc,ps)]
            world[n]=(tr,q,sc);remaining.remove(n)
    target_mag=add(add(PIVOT,off),qr(rock,sub([22.,-2.,0.],PIVOT)))
    for side,target_cm in [('L',lerp([39.,-3.,8.],target_mag,grasp)),('R',[1.,-1.,5.])]:
        target=mul(target_cm,10)
        bone='thumb_01_'+side
        base=world[bone][0];tip=world['thumb_03_'+side+'_end'][0]
        delta=between(sub(tip,base),sub(target,base))
        parent_q=world['hand_'+side][1]
        tr,q,sc=result[bone]
        result[bone]=(tr,qm(qm(qm(qi(parent_q),delta),parent_q),q),sc)
    return result

report={'coordinates':'WeaponPose local centimetres','texture_changes':False,'rest_left_wrist':REST,
        'magazine_grab_wrist':GRAB,'magazine_pivot':PIVOT,'magazine_times':TIMES,'magazine_offsets':OFFSETS,'animations':{}}
for name,frames,rate,evaluate in [
    ('A_FPSArms_Grip_R7',1,24,lambda t:pose()),
    ('A_FPSArms_Reload_R7',72,48,lambda t:pose(t,True)),
    ('A_FPSArms_Fire_R7',10,48,lambda t:pose(trigger=math.sin(t*math.pi))),
]:
    path='/Game/Weapons/'+name
    dest=unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset('/Game/Weapons/A_FPSArms_Idle',path)
    controller=dest.get_editor_property('controller') or unreal.AnimDataController(outer=dest)
    model=dest.get_editor_property('data_model_interface'); controller.set_model(model)
    samples=[evaluate(i/frames) for i in range(frames+1)]
    controller.open_bracket('R7 anatomical hand contacts',False)
    try:
        controller.set_frame_rate(unreal.FrameRate(rate,1),False)
        controller.set_number_of_frames(unreal.FrameNumber(frames),False)
        for bone in names:
            if not model.is_valid_bone_track_name(bone): assert controller.add_bone_curve(bone,False)
            assert controller.set_bone_track_keys(bone,[unreal.Vector(*s[bone][0]) for s in samples],
                [unreal.Quat(*s[bone][1]) for s in samples],[unreal.Vector(*s[bone][2]) for s in samples],False)
    finally: controller.close_bracket(False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(dest)
    report['animations'][name]={'asset':path,'seconds':dest.get_play_length(),'frames':frames,'bones':len(names)}

# Change only animation references on the existing player Blueprint; no map rebuild.
bp=unreal.load_asset('/Game/Blueprints/BP_RaidPlayer')
assert bp is not None
cdo=unreal.get_default_object(bp.generated_class())
for prop,suffix in [('arms_idle_animation','Grip'),('arms_reload_animation','Reload'),('arms_fire_animation','Fire')]:
    cdo.set_editor_property(prop,unreal.load_asset('/Game/Weapons/A_FPSArms_'+suffix+'_R7'))
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
(ROOT/'Evidence/R7HandsAuthoring.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
unreal.log('R7_HANDS_AUTHORED: original R3 assets retained; visual review required')
