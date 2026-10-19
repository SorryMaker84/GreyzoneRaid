"""Stage original PNG evidence unchanged; never synthesize or retouch gameplay screenshots."""
import hashlib,json,pathlib,shutil
R=pathlib.Path(__file__).resolve().parents[1];E=R/'Evidence'
receipt=json.loads((E/'R10Journey_Packaged/R7RunReceipt.json').read_text(encoding='utf-8-sig'))
assert receipt['status']=='PASS'
cover=sorted((E/'R10Journey_Packaged').glob('r1_r10_healing_at_cover_*.png'))[-1]
items=[
 ('R10_WeightedBackpack.png',E/'R10Journey_Packaged/r1_r10_weighted_backpack.png','Actual map loot, 6.3 kg cargo'),
 ('R10_CoverTreatment.png',cover,'Actual AI injury followed by walking to cover and treatment'),
 ('R10_Healed.png',E/'R10Journey_Packaged/r1_r10_healed_ready_to_fight.png','Treatment complete, one medical item consumed'),
 ('R10_Extraction.png',E/'R10Journey_Packaged/r1_08_south_success.png','Successful same-raid extraction after the new mechanic chain'),
 ('R10_WeightLimit.png',E/'R10Packaged_Final/02_capacity_rejected.png','Isolated capacity fixture: real backend refuses excess weight')]
out=E/'R10Screens';out.mkdir(exist_ok=True);results=[]
for name,src,scope in items:
 assert src.is_file()
 dest=out/name
 if dest.exists():assert dest.read_bytes()==src.read_bytes(),'Do not silently replace an existing staged deliverable'
 else:shutil.copy2(src,dest)
 b=dest.read_bytes();assert b==src.read_bytes() and b[:8]==b'\x89PNG\r\n\x1a\n'
 results.append({'local_path':str(dest),'source':src.relative_to(R).as_posix(),'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest(),'scope':scope})
(E/'R10_Screens.json').write_text(json.dumps(results,indent=2))
(E/'R10LibraryTools/upload_request.json').write_text(json.dumps({'uploads':[{'local_path':x['local_path'],'purpose':'create_library_file','library_artifact_type':'image'} for x in results]},indent=2))
print(json.dumps({'screens':len(results),'bytes':sum(x['bytes'] for x in results),'unaltered':True}))
