"""Read retained PCM headers/onsets only. This is not a listening test."""
import array,hashlib,json,pathlib,wave
root=pathlib.Path(__file__).resolve().parents[1]
rows=[]
for name in ['S_RifleShot.wav','S_Reload.wav','S_Hit.wav']:
 path=root/'Downloads/Audio/Prepared'/name
 with wave.open(str(path)) as w:
  assert w.getsampwidth()==2 and w.getnchannels()==1
  samples=array.array('h',w.readframes(w.getnframes()));rate=w.getframerate()
 peak=max(map(abs,samples));first=next((i for i,v in enumerate(samples) if abs(v)>peak*.05),0)/rate
 rows.append({'file':path.relative_to(root).as_posix(),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
  'duration_seconds':len(samples)/rate,'first_sample_above_5_percent_peak_seconds':first,'sample_rate':rate})
report={'audio_listened':False,'note':'Waveform threshold is not a perceived response-latency measurement. No audio files changed.','files':rows}
(root/'Evidence/R9_AudioAnalysis.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
