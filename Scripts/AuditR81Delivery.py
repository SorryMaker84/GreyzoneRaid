"""Hash the tested R8.1 inputs, preserve R8, and synchronize only R8.1 docs."""
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
 write('Evidence/R81_CodeAtPackage.json',[record(p) for p in files(['Source','Config','Content'])])
 print('R81 compiled/cooked input hashes recorded')
 raise SystemExit(0)
receipts={n:read(p) for n,p in {
 'map':'Evidence/R81Map_Packaged/Receipt.json',
 'ux':'Evidence/R81UX_Packaged/Receipt.json',
 'flow':'Evidence/R81Flow_Packaged/R81FlowReceipt.json'}.items()}
for n,r in receipts.items():assert r['status']=='PASS',(n,r['status'])
for x in read('Evidence/R81_CodeAtPackage.json'):assert sha(ROOT/x['path'])==x['sha256'],x['path']
old=read('Evidence/R8_FinalDeliveryManifest.json')
for x in old['package_files']:assert sha(ROOT/x['path'])==x['sha256'],'R8 package changed: '+x['path']
for name in ['README','TEST_RESULTS']:shutil.copy2(ROOT/(name+'_R81.md'),ROOT/(name+'.md'))
package=ROOT/'Packaged_R81/Windows'
pr=read('Packaged_R81/Windows/R81_PackagingReceipt.json')
assert pr['uat_exit_code']==0 and not pr['skipped_missing_files']
for x in pr['copied_files']:
 source=ROOT/x['source'];dest=package/x['destination']
 shutil.copy2(source,dest);x['sha256']=sha(dest)
 assert x['sha256']==sha(source)
pr['documents_refreshed_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
write('Packaged_R81/Windows/R81_PackagingReceipt.json',pr)
inputs=files(['Source','Config','Content','Scripts'])+[ROOT/n for n in ['GreyzoneRaid.uproject','README_R81.md','TEST_RESULTS_R81.md','README.md','TEST_RESULTS.md']]
pack=sorted(p for p in package.rglob('*') if p.is_file() and not {'Saved','Evidence'}&set(p.relative_to(package).parts))
manifest={'status':'PASS_RECORDED_CHECKS','created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'receipts':receipts,'source_files':[record(p) for p in inputs],'package_files':[record(p) for p in pack],
 'compiled_inputs_unchanged':True,'r8_package_unchanged':True,'documents_synced':True,
 'movement_diagnosis':read('Evidence/R81MovementDiagnosis.json'),
 'r8_restore_zip':record(ROOT/'Backups/R8_BeforeR81_Source.zip'),
 'hardware_input_tested':False,'audio_listened':False,'new_full_map_route_or_balance_claim':False}
write('Evidence/R81_FinalDeliveryManifest.json',manifest)
print(json.dumps({'status':manifest['status'],'source_files':len(inputs),'package_files':len(pack),'package_bytes':sum(x['bytes'] for x in manifest['package_files']),'r8_preserved':True}))
