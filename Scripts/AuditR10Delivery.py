"""Record exactly what was built and verify the tested archive, retained R9 and allowed data change."""
import datetime,hashlib,json,pathlib,shutil,sys
R=pathlib.Path(__file__).resolve().parents[1]
def read(p):return json.loads((R/p).read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(4*1024*1024),b''):h.update(block)
 return h.hexdigest()
def files(names):return sorted(p for n in names for p in (R/n).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
def record(p):return {'path':p.relative_to(R).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)}
def write(p,obj):(R/p).write_text(json.dumps(obj,indent=2),encoding='utf-8')
if '--freeze' in sys.argv:
 write('Evidence/R10_CodeAtPackage.json',[record(p) for p in files(['Source','Config','Content'])]);print('Frozen R10 compiled/cooked inputs');raise SystemExit()
receipts={n:read(p) for n,p in {
 'r10':'Evidence/R10Packaged_Final/Receipt.json','r9':'Evidence/R10R9_Packaged/Receipt.json',
 'regression':'Evidence/R10Regression_Packaged/Receipt.json','ux':'Evidence/R10UX_Packaged/Receipt.json',
 'map':'Evidence/R10Map_Packaged/Receipt.json','journey':'Evidence/R10Journey_Packaged/R7RunReceipt.json'}.items()}
for n,r in receipts.items():assert r['status']=='PASS',(n,r['status'])
journey=read('Evidence/R10Journey_Packaged/R7DirectorReceipt.json')
for n in ['r10_authored_loot_load','r10_cover_treatment','r10_fired_after_treatment','south_success_without_mission','north_success_after_mission','normal_ai_caused_death']:assert journey[n],n
frozen=read('Evidence/R10_CodeAtPackage.json')
assert {x['path'] for x in frozen}=={p.relative_to(R).as_posix() for p in files(['Source','Config','Content'])}
for x in frozen:assert sha(R/x['path'])==x['sha256'],x['path']
retained=read('Evidence/R9_FinalDeliveryManifest.json')
for x in retained['package_files']:assert sha(R/x['path'])==x['sha256'],'R9 package changed: '+x['path']
allowed={'Content/Data/DA_RaidItems.uasset'};changed=[]
for x in retained['source_files']:
 if x['path'].startswith(('Content/','Config/')) and sha(R/x['path'])!=x['sha256']:
  assert x['path'] in allowed,'Unexpected content/config change: '+x['path']
  changed.append(x['path'])
assert set(changed)==allowed
for n in ['README','TEST_RESULTS']:shutil.copy2(R/(n+'_R10.md'),R/(n+'.md'))
pack=R/'Packaged_R10/Windows';pr=read('Packaged_R10/Windows/R10_PackagingReceipt.json')
assert pr['uat_exit_code']==0 and not pr['skipped_missing_files']
assert sha(R/'Binaries/Win64/GreyzoneRaid.exe')==sha(pack/'GreyzoneRaid/Binaries/Win64/GreyzoneRaid.exe')
staged=[];stage=R/'Saved/StagedBuilds/Windows'
for p in sorted(stage.rglob('*')):
 if not p.is_file():continue
 rel=p.relative_to(stage)
 if {'Saved','Evidence'}&set(rel.parts):continue
 assert (pack/rel).is_file() and sha(p)==sha(pack/rel),'Stage/archive mismatch: '+str(rel)
 staged.append(rel.as_posix())
assert any(p.endswith('.pak') for p in staged)
for x in pr['copied_files']:
 src=R/x['source'];dst=pack/x['destination'];shutil.copy2(src,dst);x['sha256']=sha(dst);assert sha(src)==x['sha256']
pr['documents_refreshed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();write('Packaged_R10/Windows/R10_PackagingReceipt.json',pr)
inputs=files(['Source','Config','Content','Scripts'])+[R/n for n in ['GreyzoneRaid.uproject','README_R10.md','TEST_RESULTS_R10.md','R10_CHANGE_GUIDE.md','README.md','TEST_RESULTS.md']]
package=sorted(p for p in pack.rglob('*') if p.is_file() and not {'Saved','Evidence'}&set(p.relative_to(pack).parts))
manifest={'status':'PASS_RECORDED_CHECKS','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'receipts':receipts,
 'source_files':[record(p) for p in inputs],'package_files':[record(p) for p in package],
 'compiled_inputs_unchanged':len(frozen),'staged_archive_matches':staged,'native_binary_matches_build':True,
 'retained_r9_package_files':len(retained['package_files']),'allowed_content_changes':changed,'documents_synced':True,
 'r9_restore_zip':record(R/'Backups/R9_BeforeR10_Source.zip'),'hardware_input_tested':False,'audio_listened':False}
assert manifest['r9_restore_zip']['sha256']==read('Evidence/R10_Preflight.json')['sha256']
write('Evidence/R10_FinalDeliveryManifest.json',manifest)
print(json.dumps({'status':manifest['status'],'source_files':len(inputs),'package_files':len(package),'compiled_inputs':len(frozen),'staged_files':len(staged),'retained_r9_files':manifest['retained_r9_package_files']}))
