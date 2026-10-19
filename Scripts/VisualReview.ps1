param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$SceneOnly, [string]$GameExecutable = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$logPath = Join-Path $projectRoot 'Evidence\R6VisualReview.log'
$started = Get-Date
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$arguments = @(('"' + (Join-Path $projectRoot 'GreyzoneRaid.uproject') + '"'), '/Game/Maps/Depot', '-game', '-RaidVisualReview', '-RenderOffscreen','-ForceRes', '-windowed', '-ResX=1600', '-ResY=900', '-unattended', '-nosplash', '-nosound', ('"-abslog=' + $logPath + '"'))
$screenshotFolder = Join-Path $projectRoot 'Evidence\R6Visual'
if ($GameExecutable) {
    $editorPath = $GameExecutable
    $arguments = @('/Game/Maps/Depot', '-RaidVisualReview', '-RenderOffscreen','-ForceRes', '-windowed', '-ResX=1600', '-ResY=900', '-unattended', '-nosplash', '-nosound', ('"-abslog=' + $logPath + '"'))
    $screenshotFolder = Join-Path (Split-Path -Parent $GameExecutable) 'GreyzoneRaid\Evidence\R6Visual'
}
if ($SceneOnly) { $arguments += '-RaidScenePreview' }
$reviewProcess = Start-Process -FilePath $editorPath -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "Visual review process: $($reviewProcess.Id)"
if (!$reviewProcess.WaitForExit(240000)) {
    Stop-Process -Id $reviewProcess.Id
    throw 'Visual review timed out; only its own process was stopped.'
}
$reviewProcess.Refresh()
if ($reviewProcess.ExitCode -ne 0) { throw "R6 UE process exited $($reviewProcess.ExitCode)." }
Write-Output "R6_PROCESS_EXIT_CODE: $($reviewProcess.ExitCode)"
if (!(Test-Path -LiteralPath $logPath) -or (Get-Item -LiteralPath $logPath).LastWriteTime -lt $started) { throw 'No fresh visual review log.' }
$result = Select-String -LiteralPath $logPath -Pattern 'R6_VISUAL_REVIEW_COMPLETE:' | Select-Object -Last 1
if (!$result) { throw "Visual review incomplete; inspect $logPath" }
$reviewScreens = @(Get-ChildItem -LiteralPath $screenshotFolder -Filter '*.png' | Where-Object LastWriteTime -ge $started)
foreach ($reviewScreen in $reviewScreens) {
    $reviewBytes = [System.IO.File]::ReadAllBytes($reviewScreen.FullName)
    if ($reviewBytes.Length -le 128 -or [System.BitConverter]::ToString($reviewBytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Invalid visual PNG: $($reviewScreen.Name)" }
    $reviewWidth = (([int]$reviewBytes[16]) -shl 24) -bor (([int]$reviewBytes[17]) -shl 16) -bor (([int]$reviewBytes[18]) -shl 8) -bor ([int]$reviewBytes[19])
    $reviewHeight = (([int]$reviewBytes[20]) -shl 24) -bor (([int]$reviewBytes[21]) -shl 16) -bor (([int]$reviewBytes[22]) -shl 8) -bor ([int]$reviewBytes[23])
    if ($reviewWidth -ne 1600 -or $reviewHeight -ne 900) { throw "Unexpected visual PNG dimensions $reviewWidth x $reviewHeight for $($reviewScreen.Name)" }
}
if (!$SceneOnly -and $reviewScreens.Count -ne 15) { throw 'Expected fifteen fresh visual screenshots.' }
Write-Output $result.Line
Get-ChildItem -LiteralPath $screenshotFolder -Filter '*.png' | Select-Object Name,Length,LastWriteTime
