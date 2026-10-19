param([string]$GameExecutable='', [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$out=[IO.Path]::GetFullPath($OutputDirectory)
if (-not $out.StartsWith($project+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Output must be inside this project.' }
if (Test-Path -LiteralPath $out) { throw 'Use a fresh output directory.' }
New-Item -ItemType Directory -Path $out | Out-Null
if ($GameExecutable) { $exe=$GameExecutable; $arguments=@('/Game/Maps/Frontier') }
else { $exe='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'; $arguments=@(('"'+$project+'\GreyzoneRaid.uproject"'),'/Game/Maps/Frontier','-game') }
$arguments+=@('-RaidR8Test',('-RaidR8Output="'+$out+'"'),'-RenderOffscreen','-ForceRes','-windowed','-ResX=1600','-ResY=900','-nosplash','-unattended','-nosound',('-abslog="'+$out+'\Runtime.log"'))
$started=[DateTime]::UtcNow
$process=Start-Process -FilePath $exe -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "R8 UX runtime PID=$($process.Id)"
$deadline=$started.AddSeconds(140)
while (-not $process.WaitForExit(1000)) { if ([DateTime]::UtcNow -gt $deadline) { Stop-Process -InputObject $process; throw 'R8 UX runtime timed out.' } }
$process.Refresh()
$result=Get-Content -LiteralPath ($out+'\Runtime.log') | Where-Object { $_ -match 'R8_UX_RESULT:' } | Select-Object -Last 1
$screens=@(Get-ChildItem -LiteralPath $out -Filter '*.png' | ForEach-Object { @{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash;fresh=($_.LastWriteTimeUtc -gt $started)} })
$ok=$process.ExitCode -eq 0 -and $result -match 'PASS checks=64 failures=0 raids=3' -and $screens.Count -eq 9 -and -not ($screens | Where-Object {-not $_.fresh -or $_.bytes -lt 10000})
@{status=$(if($ok){'PASS'}else{'FAIL'});executable=$exe;started_utc=$started.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');exit_code=$process.ExitCode;result=$result;screenshots=$screens;hardware_input_tested=$false;input='UE simulated key events through PlayerController and Canvas hitbox dispatch';ai='unchanged';fixtures='grant and teleport for extraction pause; controlled damage for death/restart'} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath ($out+'\Receipt.json') -Encoding utf8
Write-Output $result
if (-not $ok) { throw 'R8 UX checks failed; inspect Runtime.log and Receipt.json.' }
