"""Summarize the bounded probe without treating an injected stall as history."""
import csv,json,pathlib,re
ROOT=pathlib.Path(__file__).resolve().parents[1]
runs=[]
for name in ['Normal01','Normal02','Injected']:
    folder=ROOT/'Evidence'/('R81Movement_'+name)
    receipt=json.loads((folder/'Receipt.json').read_text(encoding='utf-8-sig'))
    assert receipt['status']=='DIAGNOSTIC_COMPLETE'
    log=(folder/'Runtime.log').read_text(encoding='utf-8-sig')
    rows=list(csv.DictReader((folder/'Movement.csv').open(encoding='utf-8-sig',newline='')))
    measurements=[]
    for phase,elapsed,applied,distance,dt in re.findall(r'R81_MEASUREMENT: phase=(\w+) elapsed=([\d.]+) physics_input_seconds=([\d.]+) distance=([\d.]+) current_dt=([\d.]+)',log):
        measurements.append({'phase':phase,'elapsed_seconds_including_unprocessed_frame':float(elapsed),'seconds_with_input_already_processed':float(applied),'sampled_distance_cm':float(distance),'unprocessed_frame_dt':float(dt),'mean_distance_per_processed_input_second':float(distance)/float(applied)})
    runs.append({'name':name,'injected':receipt['injected_hitch'],'result':receipt['result'],
        'measurements':measurements,'lane':[line.split('R81_LANE: ')[-1] for line in log.splitlines() if 'R81_LANE:' in line],
        'samples':len(rows),'max_dt':max(float(x['dt']) for x in rows),
        'input_ignored_values':sorted(set(x['input_ignored'] for x in rows)),
        'can_act_values':sorted(set(x['can_act'] for x in rows)),
        'movement_modes':sorted(set(x['movement_mode'] for x in rows)),
        'sprint_max_speeds':sorted(set(x['max_speed'] for x in rows if x['phase']=='sprint'))})
report={'runs':runs,'historical_root_cause_confirmed':False,
    'qualification':'Controlled injection tests the old pre-physics measurement vulnerability. It cannot establish the cause of the isolated R8 failure without historical frame samples. Normal runs and unchanged gameplay speed/input are reported separately.'}
(ROOT/'Evidence/R81MovementDiagnosis.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
