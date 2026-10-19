"""Summarize measured cadence; FPS labels are limits, actual frame time is reported."""
import csv,json,pathlib,statistics,sys
root=pathlib.Path(__file__).resolve().parents[1]
results=[]
for name in sys.argv[1:]:
 p=root/'Evidence'/name
 frames=list(csv.DictReader((p/'Frames.csv').open(encoding='utf-8-sig')))
 shots=list(csv.DictReader((p/'Shots.csv').open(encoding='utf-8-sig')))
 phases={}
 for phase in ['hip_hold','ads_hold','tap_first','tap_early_hold']:
  times=[float(r['time']) for r in shots if r['phase']==phase]
  gaps=[b-a for a,b in zip(times,times[1:])]
  fs=[r for r in frames if r['phase']==phase]
  phases[phase]={'shots':len(times),'intervals_s':gaps,'mean_interval_s':statistics.mean(gaps) if gaps else None,
   'max_interval_s':max(gaps) if gaps else None,'mean_dt_s':statistics.mean(float(r['dt']) for r in fs),
   'max_recoil_pitch':max(float(r['recoil_pitch']) for r in fs),'max_kick_cm':max(float(r['kick']) for r in fs)}
 sync=[r for r in frames if 'arms_shot_age' in r and float(r['arms_shot_age'])>=0]
 results.append({'run':name,'phases':phases,'arm_phase_samples':len(sync),
  'max_arms_shot_age_error_s':max((abs(float(r['arms_shot_age'])-float(r['shot_age'])) for r in sync),default=None),
  'max_arms_reload_phase_error':max((abs(float(r['arms_reload'])-float(r['visual_reload'])) for r in sync),default=None),
  'max_wrist_gap_cm':max((max(float(r['left_wrist_gap_cm']),float(r['right_wrist_gap_cm'])) for r in sync if 'left_wrist_gap_cm' in r),default=None)})
target=root/'Evidence'/('R9_Shooting_'+('Baseline' if all('Baseline' in n for n in sys.argv[1:]) else 'After')+'.json')
target.write_text(json.dumps(results,indent=2))
print(json.dumps(results))
