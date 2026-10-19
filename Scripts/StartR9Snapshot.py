"""Preserve the verified R8.1 inputs and verify the retained playable package."""
import hashlib,json,pathlib,zipfile,datetime
root=pathlib.Path(__file__).resolve().parents[1]
sha=lambda b:hashlib.sha256(b).hexdigest()
manifest=json.loads((root/'Evidence/R81_FinalDeliveryManifest.json').read_text())
for group in ['source_files','package_files']:
 for row in manifest[group]:
  assert sha((root/row['path']).read_bytes())==row['sha256'],row['path']
target=root/'Backups/R81_BeforeR9_Source.zip'
assert not target.exists(), 'Do not replace the recovery snapshot'
with zipfile.ZipFile(target,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=1) as z:
 for row in manifest['source_files']:z.write(root/row['path'],row['path'])
with zipfile.ZipFile(target) as z:
 for row in manifest['source_files']:assert sha(z.read(row['path']))==row['sha256'],row['path']
result={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'source_count':len(manifest['source_files']),
 'package_count':len(manifest['package_files']),'r81_inputs_unchanged':True,'r81_package_unchanged':True,
 'zip_readback_verified':True,'snapshot':str(target),'bytes':target.stat().st_size,'sha256':sha(target.read_bytes())}
(root/'Evidence/R9_Preflight.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
