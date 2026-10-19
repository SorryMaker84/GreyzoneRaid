param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [string]$Map = '/Game/Maps/Depot',
    [ValidateRange(960,3840)][int]$Width = 1600,
    [ValidateRange(540,2160)][int]$Height = 900
)
$ErrorActionPreference = 'Stop'
$flowProjectRoot = Split-Path -Parent $PSScriptRoot
$flowEvidence = Join-Path $flowProjectRoot 'Evidence\R6Flow'
New-Item -ItemType Directory -Path $flowEvidence -Force | Out-Null
$flowLog = Join-Path $flowEvidence 'FlowTest.log'
$flowStartedUtc = [DateTime]::UtcNow
if ($GameExecutable) {
    $flowExecutable = [System.IO.Path]::GetFullPath($GameExecutable)
    $flowArguments = @($Map)
    $flowExecutableDirectory = Split-Path -Parent $flowExecutable
    $flowPackagedProject = Join-Path $flowExecutableDirectory 'GreyzoneRaid'
    if ((Split-Path -Leaf $flowExecutableDirectory) -eq 'Win64' -and
        (Split-Path -Leaf (Split-Path -Parent $flowExecutableDirectory)) -eq 'Binaries') {
        # Also accept the inner packaged executable under GreyzoneRaid/Binaries/Win64.
        $flowPackagedProject = Split-Path -Parent (Split-Path -Parent $flowExecutableDirectory)
    }
    $flowCaptureFolder = Join-Path $flowPackagedProject 'Evidence\R6Flow'
} else {
    $flowExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $flowArguments = @(('"' + (Join-Path $flowProjectRoot 'GreyzoneRaid.uproject') + '"'), $Map, '-game')
    $flowCaptureFolder = $flowEvidence
}
if (!(Test-Path -LiteralPath $flowExecutable)) { throw "UE executable not found: $flowExecutable" }
# Start without RaidStart: the ordinary frontend must be exercised first.
$flowArguments += @('-RaidFlowTest', '-RenderOffscreen','-ForceRes', '-windowed', "-ResX=$Width", "-ResY=$Height", '-nosplash', '-unattended', '-nosound', ('"-abslog=' + $flowLog + '"'))
$flowProcess = Start-Process -FilePath $flowExecutable -ArgumentList $flowArguments -WindowStyle Hidden -PassThru
Write-Output "UE menu-flow process: $($flowProcess.Id)"
$flowDeadline = [DateTime]::UtcNow.AddSeconds(240)
while (!$flowProcess.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -gt $flowDeadline) {
        # The captured process object identifies only this run. Never stop other UE processes.
        Stop-Process -InputObject $flowProcess
        throw 'Menu-flow run exceeded 240 seconds; only its own test process was stopped.'
    }
}
$flowProcess.Refresh()
if (!(Test-Path -LiteralPath $flowLog) -or (Get-Item -LiteralPath $flowLog).LastWriteTimeUtc -lt $flowStartedUtc) {
    throw 'No fresh menu-flow log was produced.'
}
$flowLines = Get-Content -LiteralPath $flowLog
$flowResult = $flowLines | Where-Object { $_ -match 'RAID_FLOW_RESULT:' } | Select-Object -Last 1
if (!$flowResult -or $flowResult -notmatch 'RAID_FLOW_RESULT: PASS checks=([0-9]+) failures=0') {
    throw "Menu-flow checks did not pass; inspect $flowLog"
}
$flowChecks = [int]$Matches[1]
# R9 removes two start-search clicks (hitbox and dispatch checks for each).
# Search progress/state/timing assertions remain and start at the actual E open.
if ($flowChecks -lt 248) { throw "Incomplete menu/font/container coverage: checks=$flowChecks" }
$flowResultIndex = [Array]::LastIndexOf($flowLines, $flowResult)
$flowQuit = $false
for ($flowLineIndex = $flowResultIndex + 1; $flowLineIndex -lt $flowLines.Count; $flowLineIndex++) {
    if ($flowLines[$flowLineIndex] -match 'RAID_QUIT: requested from frontend') { $flowQuit = $true; break }
}
if (!$flowQuit) { throw 'PASS marker was not followed by the real frontend Quit action.' }
if ($flowProcess.ExitCode -ne 0) { throw "Quit did not produce a normal process exit: $($flowProcess.ExitCode)" }
$flowRequiredScreens = @('01_main_menu.png', '02_instructions.png', '03_running.png', '03a_container_unknown.png', '03b_container_revealed.png', '03c_container_empty.png', '04_extraction_countdown.png', '05_extracted_success.png', '06_dead_failure.png', '07_restart_clean.png', '08_return_menu.png', '09_abandoned.png')
foreach ($flowScreenName in $flowRequiredScreens) {
    # FPaths::ProjectDir belongs to the packaged game when GameExecutable is supplied.
    $flowScreen = Join-Path $flowCaptureFolder $flowScreenName
    if (!(Test-Path -LiteralPath $flowScreen)) { throw "Missing flow screenshot: $flowScreenName" }
    $flowFile = Get-Item -LiteralPath $flowScreen
    if ($flowFile.LastWriteTimeUtc -lt $flowStartedUtc -or $flowFile.Length -lt 128) { throw "Stale or incomplete screenshot: $flowScreenName" }
    $flowBytes = [System.IO.File]::ReadAllBytes($flowScreen)
    $flowSignature = [byte[]]@(137,80,78,71,13,10,26,10)
    for ($flowByteIndex = 0; $flowByteIndex -lt 8; $flowByteIndex++) {
        if ($flowBytes[$flowByteIndex] -ne $flowSignature[$flowByteIndex]) { throw "Invalid PNG: $flowScreenName" }
    }
    $flowActualWidth = (([int]$flowBytes[16]) -shl 24) -bor (([int]$flowBytes[17]) -shl 16) -bor (([int]$flowBytes[18]) -shl 8) -bor ([int]$flowBytes[19])
    $flowActualHeight = (([int]$flowBytes[20]) -shl 24) -bor (([int]$flowBytes[21]) -shl 16) -bor (([int]$flowBytes[22]) -shl 8) -bor ([int]$flowBytes[23])
    if ($flowActualWidth -ne $Width -or $flowActualHeight -ne $Height) { throw "Unexpected PNG dimensions $flowActualWidth x $flowActualHeight for $flowScreenName; expected $Width x $Height" }
}
if ([System.IO.Path]::GetFullPath($flowCaptureFolder) -ne [System.IO.Path]::GetFullPath($flowEvidence)) {
    foreach ($flowScreenName in $flowRequiredScreens) {
        $flowSourceScreen = Join-Path $flowCaptureFolder $flowScreenName
        if (Test-Path -LiteralPath $flowSourceScreen) {
            $flowSourceFile = Get-Item -LiteralPath $flowSourceScreen
            if ($flowSourceFile.LastWriteTimeUtc -ge $flowStartedUtc -and $flowSourceFile.Length -ge 128) {
                Copy-Item -LiteralPath $flowSourceScreen -Destination (Join-Path $flowEvidence $flowScreenName) -Force
            }
        }
    }
}
$flowReceipt = [ordered]@{
    status = 'PASS'; checks = $flowChecks; failures = 0; process_exit_code = $flowProcess.ExitCode
    real_frontend_quit_after_result = $flowQuit; hardware_input_tested = $false
    width = $Width; height = $Height
    evidence_log = $flowLog; required_screenshots = $flowRequiredScreens
    capture_source_folder = $flowCaptureFolder; canonical_evidence_folder = $flowEvidence
    started_utc = $flowStartedUtc.ToString('o'); finished_utc = [DateTime]::UtcNow.ToString('o')
}
$flowReceipt | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $flowEvidence 'FlowTestResult.json') -Encoding UTF8
Write-Output $flowResult
Write-Output 'Real frontend Quit and normal process exit verified; twelve fresh PNG screenshots verified.'
