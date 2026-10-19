param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [string]$GameExecutable = '', [string]$Map = '/Game/Maps/Depot')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$selfEvidence = Join-Path $projectRoot 'Evidence\R6SelfTest'
New-Item -ItemType Directory -Path $selfEvidence -Force | Out-Null
$testLog = Join-Path $selfEvidence 'SelfTest.log'
$testStarted = Get-Date
if ($GameExecutable) {
    $testExecutable = $GameExecutable
    $testArguments = @($Map)
} else {
    $testExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $testArguments = @(('"' + (Join-Path $projectRoot 'GreyzoneRaid.uproject') + '"'), $Map, '-game')
}
$testArguments += @('-RaidSelfTest', '-RenderOffscreen','-ForceRes', '-windowed', '-ResX=1280', '-ResY=720', '-nosplash', '-unattended', '-nosound', ('"-abslog=' + $testLog + '"'))
$testProcess = Start-Process -FilePath $testExecutable -ArgumentList $testArguments -WindowStyle Hidden -PassThru
Write-Output "UE integration process: $($testProcess.Id)"
if (!$testProcess.WaitForExit(240000)) {
    Stop-Process -Id $testProcess.Id
    throw 'Integration run exceeded 240 seconds. Only its own test process was stopped.'
}
$testProcess.Refresh()
if ($testProcess.ExitCode -ne 0) { throw "R6 UE process exited $($testProcess.ExitCode)." }
Write-Output "R6_PROCESS_EXIT_CODE: $($testProcess.ExitCode)"
if (!(Test-Path -LiteralPath $testLog) -or (Get-Item -LiteralPath $testLog).LastWriteTime -lt $testStarted) {
    throw 'No fresh integration log was produced.'
}
$testResult = Select-String -LiteralPath $testLog -Pattern 'RAID_SELFTEST_RESULT:' | Select-Object -Last 1
if (!$testResult) { throw "Integration did not finish; inspect $testLog" }
Write-Output $testResult.Line
# UE may return zero after a graceful RequestExitWithStatus; the result marker is authoritative.
if ($testResult.Line -notmatch 'RAID_SELFTEST_RESULT: PASS') { throw "Integration checks failed; inspect $testLog" }
