param([string]$GameExecutable='', [switch]$Baseline, [switch]$MeasuresOnly, [int]$Fps=60,
 [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
if (-not $out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)) { throw 'Use a fresh project-local output folder.' }
New-Item -ItemType Directory -Path $out | Out-Null
if($GameExecutable){$exe=$GameExecutable;$argsList=@('/Game/Maps/Frontier')}
else{$exe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe';$argsList=@(('"'+$root+'\GreyzoneRaid.uproject"'),'/Game/Maps/Frontier','-game')}
$argsList+='-RaidR9Test'
if($Baseline){$argsList+='-R9Baseline'}
if($MeasuresOnly){$argsList+='-R9MeasuresOnly'}
$argsList+=@('-nosound','-unattended','-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900','-nosplash',('-R9Output="'+$out+'"'),('-abslog="'+$out+'\Runtime.log"'),('-ExecCmds="t.MaxFPS '+$Fps+'"'))
$started=[DateTime]::UtcNow
$process=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
Write-Output "R9 PID=$($process.Id) FPS limit=$Fps"
$deadline=$started.AddSeconds(280)
while(-not $process.WaitForExit(1000)){if([DateTime]::UtcNow -gt $deadline){Stop-Process -InputObject $process;throw 'R9 exceeded280s.'}}
$process.Refresh()
$result=Get-Content -LiteralPath ($out+'\Runtime.log') | Where-Object {$_ -match 'R9_RESULT:'} | Select-Object -Last 1
$ok=$process.ExitCode -eq 0 -and $result -match 'failures=0' -and (Test-Path -LiteralPath ($out+'\Frames.csv'))
$status=if($ok){if($Baseline){'BASELINE_OBSERVED'}else{'PASS'}}else{'FAIL'}
$screens=@(Get-ChildItem -LiteralPath $out -Filter '*.png' | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash;fresh=($_.LastWriteTimeUtc -gt $started);bytes=$_.Length}})
@{status=$status;executable=$exe;exit_code=$process.ExitCode;fps_limit=$Fps;baseline=[bool]$Baseline;measurements_only=[bool]$MeasuresOnly;result=$result;screenshots=$screens;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');hardware_input_tested=$false;audio_listened=$false} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath ($out+'\Receipt.json') -Encoding utf8
Write-Output $result
if(-not $ok){throw 'R9 probe failed; inspect receipt/log.'}
