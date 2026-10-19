"""Verify final tested inputs and refresh R8 delivery docs without touching R7."""
from pathlib import Path
import datetime, hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[1]
def read(path): return json.loads((ROOT/path).read_text(encoding='utf-8-sig'))
def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda:f.read(4*1024*1024),b''):h.update(chunk)
    return h.hexdigest()
def record(path):return {'path':path.relative_to(ROOT).as_posix(),'bytes':path.stat().st_size,'sha256':sha(path)}

receipts={name:read(path) for name,path in {
    'ux':'Evidence/R8UX_Packaged_Release/Receipt.json',
    'frontier_finale':'Evidence/R8Finale_Packaged_Final/R7RunReceipt.json',
    'legacy':'Evidence/R8Legacy_Packaged_Release/R7LegacyReceipt.json'}.items()}
for name,r in receipts.items():assert r['status']=='PASS',(name,r['status'])
assert receipts['frontier_finale']['test_scope']=='finale_only_no_full_route_coverage'
# The 257-check Frontier run belongs to preserved checkpoint 2. Its tested
# non-pause paths are unchanged; final pause behavior is covered separately.
prior=read('Evidence/R8_Checkpoint2CodeAtPackage.json')
changes=[e['path'] for e in prior if sha(ROOT/e['path'])!=e['sha256']]
assert set(changes)=={'Source/GreyzoneRaid/RaidPlayer.cpp','Source/GreyzoneRaid/RaidR8TestDirector.cpp','Source/GreyzoneRaid/RaidR8TestDirector.h'}
player=(ROOT/'Source/GreyzoneRaid/RaidPlayer.cpp').read_bytes()
new=b'    // Engine pause already freezes physics. Keep velocity so a mid-air pause\n    // resumes the same jump, rather than cancelling its upward momentum.\n'
newline=b'\r\n' if b'\r\n' in player else b'\n'
new=new.replace(b'\n',newline)
assert player.count(new)==1
reconstructed=player.replace(new,b'    GetCharacterMovement()->StopMovementImmediately();'+newline)
old_player=next(e for e in prior if e['path']=='Source/GreyzoneRaid/RaidPlayer.cpp')
assert hashlib.sha256(reconstructed).hexdigest()==old_player['sha256'], 'Unreviewed production delta since checkpoint 2'
for item in read('Evidence/R8_FinalCodeAtPackage.json'):
    assert sha(ROOT/item['path'])==item['sha256'], 'Compiled input changed: '+item['path']
for item in read('Evidence/R7_FinalDeliveryManifest.json')['package_files']:
    assert sha(ROOT/item['path'])==item['sha256'], 'R7 package changed: '+item['path']
for path,digest in read('Evidence/R8_PreflightReceipt.json')['archives'][1]['entries'].items():
    assert sha(ROOT/path)==digest, 'Preserved full R7 package changed: '+path
for old,new in [('README_R8.md','README.md'),('TEST_RESULTS_R8.md','TEST_RESULTS.md')]:
    shutil.copy2(ROOT/old,ROOT/new)
package=ROOT/'Packaged_R8/Windows'
receipt=read('Packaged_R8/Windows/R8_PackagingReceipt.json')
for item in receipt['copied_files']:
    source=ROOT/item['source'];dest=package/item['destination']
    shutil.copy2(source,dest)
    item['sha256']=sha(dest)
    assert item['sha256']==sha(source)
receipt['documents_refreshed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
(package/'R8_PackagingReceipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
inputs=sorted(p for folder in ['Source','Config','Content','Scripts'] for p in (ROOT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
inputs += [ROOT/n for n in ['GreyzoneRaid.uproject','README_R8.md','TEST_RESULTS_R8.md','README.md','TEST_RESULTS.md']]
files=sorted(p for p in package.rglob('*') if p.is_file() and 'Saved' not in p.relative_to(package).parts and 'Evidence' not in p.relative_to(package).parts)
manifest={'status':'PASS_RECORDED_CHECKS','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'receipts':receipts,'source_files':[record(p) for p in inputs],'package_files':[record(p) for p in files],
          'compiled_inputs_unchanged':True,'r7_package_unchanged':True,'documents_synced':True,
          'frontier_receipt_package':'Packaged_R8_Checkpoint2',
          'production_delta_since_frontier_run':'Only remove StopMovementImmediately from PreparePauseInput; exact prior file reconstructed and SHA256 checked. New jump regression in test director.',
          'hardware_input_tested':False,'audio_listened':False,'full_frontier_routes_rerun':False,
          'scope':'Targeted real UE simulated-input UX, final dual-extraction mission/AI finale, retained Depot interaction regression. No new performance or subjective-handling claim.'}
(ROOT/'Evidence/R8_FinalDeliveryManifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
print(json.dumps({'status':manifest['status'],'source_files':len(inputs),'package_files':len(files),'package_bytes':sum(x['bytes'] for x in manifest['package_files']),'r7_preserved':True}))
