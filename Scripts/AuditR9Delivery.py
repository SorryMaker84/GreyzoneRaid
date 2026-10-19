"""Freeze compiled/cooked inputs, then audit the tested R9 package and retained releases."""
import datetime,hashlib,json,pathlib,shutil,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def read(p):return json.loads((ROOT/p).read_text(encoding='utf-8-sig'))
def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(4*1024*1024),b''):h.update(b)
 return h.hexdigest()
def files(folders):return sorted(p for d in folders for p in (ROOT/d).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
def record(p):return {'path':p.relative_to(ROOT).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p)}
def write(p,x):(ROOT/p).write_text(json.dumps(x,indent=2),encoding='utf-8')
if '--freeze' in sys.argv:
 write('Evidence/R9_CodeAtPackage.json',[record(p) for p in files(['Source','Config','Content'])])
 print('R9 compiled/cooked input hashes recorded');raise SystemExit(0)
receipts={n:read(p) for n,p in {
 'r9':'Evidence/R9Packaged_Final/Receipt.json','regression':'Evidence/R9Regression_Packaged/Receipt.json',
 'ux':'Evidence/R9UX_Packaged/Receipt.json','map':'Evidence/R9Map_Packaged/Receipt.json',
 'frontier':'Evidence/R9Frontier_Packaged/R7RunReceipt.json'}.items()}
for n,r in receipts.items():assert r['status']=='PASS',(n,r['status'])
for x in read('Evidence/R9_CodeAtPackage.json'):assert sha(ROOT/x['path'])==x['sha256'],x['path']
retained={}
for version in ['R81','R8']:
 old=read('Evidence/'+version+'_FinalDeliveryManifest.json')
 for x in old['package_files']:assert sha(ROOT/x['path'])==x['sha256'],'Retained package changed: '+x['path']
 retained[version]=len(old['package_files'])
for x in read('Evidence/R81_FinalDeliveryManifest.json')['source_files']:
 if x['path'].startswith(('Content/','Config/')):assert sha(ROOT/x['path'])==x['sha256'],'Unintended map/asset/config change: '+x['path']
for name in ['README','TEST_RESULTS']:shutil.copy2(ROOT/(name+'_R9.md'),ROOT/(name+'.md'))
package=ROOT/'Packaged_R9/Windows';pr=read('Packaged_R9/Windows/R9_PackagingReceipt.json')
assert pr['uat_exit_code']==0 and not pr['skipped_missing_files']
native_build=ROOT/'Binaries/Win64/GreyzoneRaid.exe'
native_package=package/'GreyzoneRaid/Binaries/Win64/GreyzoneRaid.exe'
assert sha(native_build)==sha(native_package),'Packaged executable differs from the native build'
stage=ROOT/'Saved/StagedBuilds/Windows'
staged_matches=[]
for staged in sorted(p for p in stage.rglob('*') if p.is_file()):
 relative=staged.relative_to(stage)
 if {'Saved','Evidence'}&set(relative.parts):continue
 archived=package/relative
 assert archived.is_file(),'Staged file missing from package: '+str(relative)
 assert sha(staged)==sha(archived),'Staged/archive mismatch: '+str(relative)
 staged_matches.append(relative.as_posix())
assert staged_matches and any(p.endswith('.pak') for p in staged_matches),'No staged cooked package compared'
for x in pr['copied_files']:
 source=ROOT/x['source'];dest=package/x['destination'];shutil.copy2(source,dest);x['sha256']=sha(dest);assert x['sha256']==sha(source)
pr['documents_refreshed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();write('Packaged_R9/Windows/R9_PackagingReceipt.json',pr)
inputs=files(['Source','Config','Content','Scripts'])+[ROOT/n for n in ['GreyzoneRaid.uproject','README_R9.md','TEST_RESULTS_R9.md','R9_CHANGE_GUIDE.md','README.md','TEST_RESULTS.md']]
pack=sorted(p for p in package.rglob('*') if p.is_file() and not {'Saved','Evidence'}&set(p.relative_to(package).parts))
manifest={'status':'PASS_RECORDED_CHECKS','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'receipts':receipts,'source_files':[record(p) for p in inputs],'package_files':[record(p) for p in pack],
 'compiled_inputs_unchanged':True,'retained_packages_unchanged':retained,'content_and_config_unchanged_from_r81':True,'documents_synced':True,
 'native_binary_matches_build':True,'staged_archive_matches':staged_matches,
 'r81_restore_zip':record(ROOT/'Backups/R81_BeforeR9_Source.zip'),
 'hardware_input_tested':False,'audio_listened':False,'subjective_gunfeel_approved':False}
write('Evidence/R9_FinalDeliveryManifest.json',manifest)
print(json.dumps({'status':manifest['status'],'source_files':len(inputs),'package_files':len(pack),'package_bytes':sum(x['bytes'] for x in manifest['package_files']),'retained_packages':retained}))
