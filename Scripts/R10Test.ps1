param([string]$GameExecutable='', [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
if(-not $out.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $out)){throw 'Fresh project-local directory required.'}
New-Item -ItemType Directory -Path $out | Out-Null
if($GameExecutable){$exe=$GameExecutable;$argsList=@('/Game/Maps/Frontier')}
else{$exe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe';$argsList=@(('"'+$root+'\GreyzoneRaid.uproject"'),'/Game/Maps/Frontier','-game')}
$argsList+=@('-RaidR10Test','-nosound','-unattended','-RenderOffscreen','-windowed','-ForceRes','-ResX=1600','-ResY=900','-nosplash',('-R10Output="'+$out+'"'),('-abslog="'+$out+'\Runtime.log"'),'-ExecCmds="t.MaxFPS 60"')
$started=[DateTime]::UtcNow
$p=Start-Process -FilePath $exe -ArgumentList $argsList -WindowStyle Hidden -PassThru
Write-Output "R10 isolated probe PID=$($p.Id)"
while(-not $p.WaitForExit(1000)){if([DateTime]::UtcNow -gt $started.AddSeconds(160)){Stop-Process -InputObject $p;throw 'R10 probe timeout.'}}
$p.Refresh()
$result=Get-Content -LiteralPath ($out+'\Result.json') -Raw | ConvertFrom-Json
$ok=$p.ExitCode -eq 0 -and $result.status -eq 'PASS' -and $result.failures -eq 0
@{status=$(if($ok){'PASS'}else{'FAIL'});executable=$exe;result=$result;exit_code=$p.ExitCode;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');hardware_input_tested=$false;audio_listened=$false;scope='Isolated load/treatment fixtures; real full raid is separately recorded.'} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($out+'\Receipt.json') -Encoding utf8
Write-Output ($result | ConvertTo-Json -Compress)
if(-not $ok){throw 'R10 isolated probe failed.'}
