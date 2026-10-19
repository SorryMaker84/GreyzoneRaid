"""Read completed R7 receipts and create a local delivery manifest (no UE or network)."""
import csv
import hashlib
import json
import re
import statistics
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EV = ROOT / 'Evidence'

def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))

def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

def percentile(values, fraction):
    ordered = sorted(values)
    pos = (len(ordered) - 1) * fraction
    lower = int(pos)
    return ordered[lower] + (ordered[min(lower + 1, len(ordered) - 1)] - ordered[lower]) * (pos - lower)

required = {
    'editor_depot': 'R7Legacy_Editor_01/R7LegacyReceipt.json',
    'packaged_gameplay': 'R7Test_Packaged_04/R7RunReceipt.json',
    'packaged_preview': 'R7Preview_Packaged_Final/R7RunReceipt.json',
    'packaged_performance': 'R7Performance_Packaged_Final/R7PerformanceManifest.json',
    'packaged_depot': 'R7Legacy_Packaged_Final/R7LegacyReceipt.json',
}
receipts = {name: read(EV / path) for name, path in required.items()}
for name, receipt in receipts.items():
    assert receipt['status'] == 'PASS', (name, receipt['status'])
director = read(EV / 'R7Test_Packaged_04/R7DirectorReceipt.json')
assert director['status'] == 'PASS' and director['failures'] == 0
assert director['test_scope'] != 'finale_only_no_full_route_coverage'
assert director['round'] == 4 and len(director['authored_routes_completed']) >= 8
assert director['container_interactions'] >= 25
hands = read(EV / 'R7Hands_Packaged_Final/HandsReceipt.json')
assert hands['status'] == 'CAPTURED_REQUIRES_VISUAL_REVIEW' and len(hands['screenshots']) == 19

performance = []
for folder in sorted((EV / 'R7Performance_Packaged_Final').glob('run*')):
    run = read(folder / 'R7RunReceipt.json')
    with (folder / 'FrameTimes.csv').open(encoding='utf-8-sig', newline='') as stream:
        rows = list(csv.DictReader(stream))
    timings = [float(row['wall_frame_ms']) for row in rows]
    assert len(timings) == run['frame_samples'] and min(timings) > 0
    performance.append({
        'run': folder.name, 'run_id': run['run_id'], 'frames': len(timings),
        'fps_from_mean_wall_ms': 1000 / statistics.mean(timings),
        'wall_mean_ms': statistics.mean(timings),
        'wall_p50_ms': percentile(timings, .5),
        'wall_p95_ms': percentile(timings, .95),
        'wall_p99_ms': percentile(timings, .99),
        'gpu_available_frames': sum(row['gpu_available'] == '1' for row in rows),
        'csv_sha256': digest(folder / 'FrameTimes.csv'),
    })
assert len(performance) == 3

# Final documents are copied separately from the cooked game. Refresh only those
# receipt entries after this explicit copy; do not suggest the binary was rebuilt.
package = ROOT / 'Packaged_R7/Windows'
package_receipt_path = package / 'R7_PackagingReceipt.json'
package_receipt = read(package_receipt_path)
for entry in package_receipt['copied_files']:
    source, target = ROOT / entry['source'], package / entry['destination']
    target.write_bytes(source.read_bytes())
    entry['sha256'] = digest(target)
    assert entry['sha256'] == digest(source)
package_receipt['documents_refreshed_utc'] = datetime.now(timezone.utc).isoformat()
package_receipt_path.write_text(json.dumps(package_receipt, indent=2), encoding='utf-8')

source_paths = [ROOT / 'GreyzoneRaid.uproject']
for folder in ('Source', 'Config', 'Content'):
    source_paths.extend(p for p in (ROOT / folder).rglob('*') if p.is_file())
source_paths.extend(p for p in (ROOT / 'Scripts').rglob('*')
                    if p.is_file() and p.suffix.lower() in ('.py', '.ps1', '.json', '.cmd', '.bat'))
package_paths = [p for p in package.rglob('*') if p.is_file()
                 and 'Saved' not in p.relative_to(package).parts
                 and 'Evidence' not in p.relative_to(package).parts]

def inventory(paths):
    return [{'path': p.relative_to(ROOT).as_posix(), 'bytes': p.stat().st_size,
             'sha256': digest(p)} for p in sorted(paths)]

broken_links = []
for name in ('README_R7.md', 'R7_BEGINNER_GUIDE.md', 'R7_CODE_GUIDE.md',
             'R7_LEVEL_DESIGN.md', 'R7_HANDS_NOTES.md', 'TEST_RESULTS_R7.md'):
    for link in re.findall(r'\]\(([^)]+)\)', (ROOT / name).read_text(encoding='utf-8-sig')):
        if '://' not in link and not link.startswith('#') and not (ROOT / link.split('#')[0]).exists():
            broken_links.append({'document': name, 'target': link})
assert not broken_links, broken_links

manifest = {
    'created_utc': datetime.now(timezone.utc).isoformat(),
    'status': 'PASS_RECORDED_AUTOMATED_CHECKS',
    'receipt_files': required, 'packaged_gameplay': director,
    'performance': performance, 'performance_baseline': None,
    'fps_improvement_claimed': False,
    'hands_capture_status': hands['status'],
    'hands_visual_review': 'Separate human/model image inspection; no automated aesthetic pass claim.',
    'hardware_input_tested': False, 'audio_listened': False,
    'source_files': inventory(source_paths), 'package_files': inventory(package_paths),
    'document_links_missing': broken_links,
    'preserved_r6_archive_receipt': 'Evidence/R7_PreflightReceipt.json',
}
output = EV / 'R7_FinalDeliveryManifest.json'
output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({'manifest': str(output), 'checks': director['checks'],
                  'routes': len(director['authored_routes_completed']),
                  'interactions': director['container_interactions'],
                  'performance': performance, 'source_files': len(source_paths),
                  'package_files': len(package_paths)}, ensure_ascii=True))
