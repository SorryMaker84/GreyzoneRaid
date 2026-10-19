"""Read-only validation of the completed R4 delivery; writes its evidence manifest."""
import hashlib, json, re, struct
from datetime import datetime, timezone
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT/'Evidence'
PACKAGE = ROOT/'Packaged_R4/Windows'
def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024), b''): h.update(block)
    return h.hexdigest()
def record(path):
    stat = path.stat()
    return dict(path=path.relative_to(ROOT).as_posix(), bytes=stat.st_size,
                modified_utc=datetime.fromtimestamp(stat.st_mtime,timezone.utc).isoformat(), sha256=sha(path))
def read(path): return path.read_text(encoding='utf-8-sig',errors='replace')
runs=[]
for folder,width,height in [('R4Flow_Editor',1600,900),('R4Flow_Packaged',1600,900),('R4Flow_Packaged720',1280,720)]:
    base=EVIDENCE/folder
    log=read(base/'FlowTest.log')
    assert 'RAID_FLOW_RESULT: PASS checks=198 failures=0' in log and 'RAID_FLOW_CHECK:FAIL' not in log
    receipt=json.loads(read(base/'FlowTestResult.json'))
    assert receipt['process_exit_code']==0 and receipt['real_frontend_quit_after_result']
    pngs=sorted(base.glob('*.png'))
    assert len(pngs)==9
    for path in pngs:
        header=path.read_bytes()[:24]
        assert header[:8]==b'\x89PNG\r\n\x1a\n'
        assert struct.unpack('>II',header[16:24])==(width,height),path
        assert path.stat().st_mtime>=datetime.fromisoformat(receipt['started_utc'].replace('Z','+00:00')).timestamp()
    marker=[line for line in log.splitlines() if 'RAID_FLOW_RESULT:' in line][-1]
    runs.append(dict(folder=folder,result=marker,checks=198,screenshots=9,width=width,height=height,exit_code=0))
for mode in ['Editor','Packaged']:
    log=read(EVIDENCE/f'SelfTest_R4_{mode}.log')
    assert 'RAID_SELFTEST_RESULT: PASS checks=67 failures=0' in log
    runs.append(dict(log=f'SelfTest_R4_{mode}.log',checks=67,result=[x for x in log.splitlines() if 'RAID_SELFTEST_RESULT:' in x][-1]))
    base=EVIDENCE/f'R4Visual_{mode}'
    log=read(base/'VisualReview.log')
    assert 'R4_VISUAL_REVIEW_COMPLETE: screenshots=13' in log and 'LogExit: Exiting.' in log
    pngs=sorted(base.glob('*.png'))
    assert len(pngs)==13
    for path in pngs: assert struct.unpack('>II',path.read_bytes()[16:24])==(1600,900)
    runs.append(dict(folder=base.name,screenshots=13,result='COMPLETE and normal exit'))
build=read(EVIDENCE/'EditorBuild_R4.log')
cook=read(EVIDENCE/'Package_R4.log')
assert 'Result: Succeeded' in build
assert 'BUILD SUCCESSFUL' in cook and 'AutomationTool exiting with ExitCode=0' in cook
map_report=json.loads(read(EVIDENCE/'R4_MapLocalization.json'))
assert not map_report['unknown_latin_text']
font_report=json.loads(read(EVIDENCE/'R4_FontValidation.json'))
assert font_report['status']=='R4_FONT_BAKED_OK'
assert not font_report['font_cmap_missing'] and not font_report['world_subset_missing']
for name in ['LICENSE.NotoSansCJK.txt','COPYRIGHT.NotoSansCJK.txt']:
    assert sha(ROOT/'Downloads/Fonts'/name)==sha(PACKAGE/'ThirdPartyLicenses'/name)
inputs=[ROOT/'GreyzoneRaid.uproject']
for folder in ['Source','Content','Config']:
    inputs.extend(p for p in (ROOT/folder).rglob('*') if p.is_file())
package_finished=(EVIDENCE/'Package_R4.log').stat().st_mtime
late=[p.relative_to(ROOT).as_posix() for p in inputs if p.stat().st_mtime>package_finished]
assert not late,late
payload=[p for p in PACKAGE.rglob('*') if p.is_file() and p.name!='START_HERE.txt' and not any(x in ['Evidence','Saved','Docs','Screenshots'] for x in p.relative_to(PACKAGE).parts)]
binaries=[p for p in payload if p.suffix.lower() in ['.exe','.pak','.utoc','.ucas']]
result=dict(status='PASS',checked_utc=datetime.now(timezone.utc).isoformat(),machine='DESKTOP-TR04BB0',
    tests=runs,hardware_input_tested=False,audio_listening_tested=False,
    original_r3_backup_bytes=(ROOT/'Backups/R3_20261007/GreyzoneRaid_R3_SourceContent.zip').stat().st_size,
    source_content_config_count=len(inputs),inputs_modified_after_package=late,
    package_payload_bytes=sum(p.stat().st_size for p in payload),
    source_content_config=[record(p) for p in sorted(inputs)],package_binaries=[record(p) for p in sorted(binaries)])
(EVIDENCE/'R4_DeliveryManifest.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k not in ['source_content_config','package_binaries','tests']},indent=2))
