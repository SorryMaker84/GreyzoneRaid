param([Parameter(Mandatory=$true)][string]$OutputDirectory, [string]$GameExecutable = '')
$ErrorActionPreference = 'Stop'
$r7HandsRoot = Split-Path -Parent $PSScriptRoot
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Hands review needs a new evidence directory.' }
New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
$r7HandsLog = Join-Path $OutputDirectory 'Hands.log'
if ($GameExecutable) { $r7HandsExe = $GameExecutable; $r7HandsArgs = @('/Game/Maps/Depot') }
else {
    $r7HandsExe = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    $r7HandsArgs = @(('"' + (Join-Path $r7HandsRoot 'GreyzoneRaid.uproject') + '"'),'/Game/Maps/Depot','-game')
}
$r7HandsArgs += @('-RaidVisualReview','-RaidHandsReview',('-RaidVisualOutput="' + $OutputDirectory + '"'),'-RenderOffscreen','-ForceRes','-windowed','-ResX=1280','-ResY=720','-nosound','-unattended','-nosplash',('-abslog="' + $r7HandsLog + '"'))
$r7HandsStart = [DateTime]::UtcNow
$r7HandsProcess = Start-Process -FilePath $r7HandsExe -ArgumentList $r7HandsArgs -WindowStyle Hidden -PassThru
Write-Output "R7_HANDS_START: pid=$($r7HandsProcess.Id) output=$OutputDirectory"
if (!$r7HandsProcess.WaitForExit(240000)) {
    & "$env:SystemRoot\System32\taskkill.exe" /PID $r7HandsProcess.Id /T /F | Out-Null
    throw 'Owned hands review timed out.'
}
$r7HandsText = [IO.File]::ReadAllText($r7HandsLog)
if ($r7HandsText -match 'R6_VISUAL_REVIEW_FAILED|Log\w+: (Error|Fatal):' -or $r7HandsText -notmatch 'R7_HANDS_REVIEW_COMPLETE: screenshots=19') { throw 'Incomplete hands capture; inspect log.' }
$r7HandsImages = @(Get-ChildItem -LiteralPath $OutputDirectory -Filter '*.png')
if ($r7HandsImages.Count -ne 19) { throw 'Hands capture count mismatch.' }
$r7HandsReceipt = [ordered]@{status='CAPTURED_REQUIRES_VISUAL_REVIEW';started_utc=$r7HandsStart.ToString('o');finished_utc=[DateTime]::UtcNow.ToString('o');screenshots=@();ai_suppressed=$true;hardware_input=$false;audio_listened=$false;gameplay_proof=$false}
foreach ($r7HandsImage in $r7HandsImages) {
    if ($r7HandsImage.LastWriteTimeUtc -lt $r7HandsStart -or $r7HandsImage.Length -lt 1024) { throw 'Stale or incomplete image.' }
    $r7HandsReceipt.screenshots += @{name=$r7HandsImage.Name;bytes=$r7HandsImage.Length;sha256=(Get-FileHash -LiteralPath $r7HandsImage.FullName).Hash.ToLowerInvariant()}
}
$r7HandsReceipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'HandsReceipt.json') -Encoding UTF8
Write-Output 'R7_HANDS_CAPTURED: 19 real-renderer key frames; visual judgment still required.'
