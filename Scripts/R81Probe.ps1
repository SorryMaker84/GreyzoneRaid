param([ValidateSet('Movement','Map')][string]$Mode='Map',[string]$GameExecutable='',
 [switch]$InjectHitch,[switch]$ExpectFixed,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
if (-not $out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)) { throw 'Use a fresh project-local output folder.' }
New-Item -ItemType Directory -Path $out | Out-Null
$map=if($Mode -eq 'Movement'){'/Game/Maps/Depot'}else{'/Game/Maps/Frontier'}
if($GameExecutable){$exe=$GameExecutable;$argsList=@($map)}
else{$exe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe';$argsList=@(('"'+$root+'\GreyzoneRaid.uproject"'),$map,'-game')}
if($Mode -eq 'Movement'){$argsList+=@('-RaidSelfTest','-R81MovementProbe')}else{$argsList+='-RaidR81MapProbe'}
if($InjectHitch){$argsList+='-R81InjectHitch'}
if($ExpectFixed){$argsList+='-R81ExpectFixed'}
$argsList+=@('-nosound','-unattended','-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900','-nosplash',('-R81Output="'+$out+'"'),('-abslog="'+$out+'\Runtime.log"'))
$started=[DateTime]::UtcNow
$process=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
Write-Output "R81 $Mode PID=$($process.Id)"
$deadline=$started.AddSeconds(90)
while(-not $process.WaitForExit(1000)){if([DateTime]::UtcNow -gt $deadline){Stop-Process -InputObject $process;throw 'Probe exceeded 90 seconds.'}}
$process.Refresh()
$lines=Get-Content -LiteralPath ($out+'\Runtime.log')
$screens=@(Get-ChildItem -LiteralPath $out -Filter '*.png' | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash;fresh=($_.LastWriteTimeUtc -gt $started);bytes=$_.Length}})
if($Mode -eq 'Movement'){
 $result=$lines | Where-Object {$_ -match 'R81_MOVEMENT_DIAGNOSTIC_COMPLETE:'} | Select-Object -Last 1
 $ok=$result -and (Test-Path -LiteralPath ($out+'\Movement.csv'))
 $status=if($ok){'DIAGNOSTIC_COMPLETE'}else{'FAIL'}
}else{
 $result=$lines | Where-Object {$_ -match 'R81_MAP_RESULT:'} | Select-Object -Last 1
 $ok=$process.ExitCode -eq 0 -and $result -match 'failures=0 raids=2' -and $screens.Count -eq 5 -and -not ($screens|Where-Object {-not $_.fresh -or $_.bytes -lt 10000})
 if($ExpectFixed){$ok=$ok -and $result -match 'RESULT: PASS checks=20 failures=0 raids=2'}
 $status=if($ok){if($ExpectFixed){'PASS'}else{'BASELINE_OBSERVED'}}else{'FAIL'}
}
@{status=$status;mode=$Mode;executable=$exe;native_exit_code=$process.ExitCode;result=$result;injected_hitch=[bool]$InjectHitch;expect_fixed=[bool]$ExpectFixed;screenshots=$screens;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');hardware_input_tested=$false} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath ($out+'\Receipt.json') -Encoding utf8
Write-Output $result
if(-not $ok){throw 'Probe incomplete or regression failed; inspect receipt/log.'}
