param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [ValidateRange(960,3840)][int]$Width = 1600,
    [ValidateRange(540,2160)][int]$Height = 900
)
$ErrorActionPreference = 'Stop'
$containerProjectRoot = Split-Path -Parent $PSScriptRoot
$containerEvidence = Join-Path $containerProjectRoot 'Evidence\R6Container'
New-Item -ItemType Directory -Path $containerEvidence -Force | Out-Null
$containerLog = Join-Path $containerEvidence 'ContainerTest.log'
$containerStarted = [DateTime]::UtcNow
if ($GameExecutable) {
    $containerExecutable = [System.IO.Path]::GetFullPath($GameExecutable)
    $containerArguments = @('/Game/Maps/Depot')
    $containerCaptureFolder = Join-Path (Split-Path -Parent $containerExecutable) 'GreyzoneRaid\Evidence\R6Container'
} else {
    $containerExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $containerArguments = @(('"' + (Join-Path $containerProjectRoot 'GreyzoneRaid.uproject') + '"'), '/Game/Maps/Depot', '-game')
    $containerCaptureFolder = $containerEvidence
}
$containerArguments += @('-RaidContainerTest','-RenderOffscreen','-ForceRes','-windowed',"-ResX=$Width","-ResY=$Height",'-nosplash','-unattended','-nosound',('"-abslog=' + $containerLog + '"'))
$containerProcess = Start-Process -FilePath $containerExecutable -ArgumentList $containerArguments -WindowStyle Hidden -PassThru
Write-Output "R6 container integration process: $($containerProcess.Id)"
$containerDeadline = [DateTime]::UtcNow.AddSeconds(300)
while (!$containerProcess.WaitForExit(1000)) {
    if ([DateTime]::UtcNow -gt $containerDeadline) {
        Stop-Process -InputObject $containerProcess
        throw 'R6 container test timed out; only its own process was stopped.'
    }
}
$containerProcess.Refresh()
if (!(Test-Path -LiteralPath $containerLog) -or (Get-Item -LiteralPath $containerLog).LastWriteTimeUtc -lt $containerStarted) {
    throw 'No fresh container test log.'
}
$containerResult = Get-Content -LiteralPath $containerLog | Where-Object { $_ -match 'RAID_CONTAINER_RESULT:' } | Select-Object -Last 1
if (!$containerResult -or $containerResult -notmatch 'RAID_CONTAINER_RESULT: PASS checks=([0-9]+) failures=0') {
    throw "Container checks failed; inspect $containerLog"
}
$containerChecks = [int]$Matches[1]
if ($containerChecks -lt 152 -or $containerProcess.ExitCode -ne 0) { throw 'Incomplete test coverage or abnormal process exit.' }
$containerRequired = @('01_unknown.png','02_search_partial.png','03_first_revealed.png','04_chosen_first_item.png','05_full_backpack.png','06_inventory_choice.png','07_enemy_loot_unknown.png','08_enemy_loot_taken.png','09_extraction_countdown.png','10_extraction_success.png','11_death_during_search.png','12_live_combat_interrupt.png','13_live_combat_death.png')
$containerScreens = @($containerRequired | ForEach-Object {
    $requiredScreen = Get-Item -LiteralPath (Join-Path $containerCaptureFolder $_)
    if ($requiredScreen.LastWriteTimeUtc -lt $containerStarted -or $requiredScreen.Length -le 128) { throw "Stale or incomplete container screenshot: $_" }
    $requiredScreen
})
foreach ($screen in $containerScreens) {
    $bytes = [System.IO.File]::ReadAllBytes($screen.FullName)
    if ([System.BitConverter]::ToString($bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Invalid PNG $($screen.Name)" }
    $actualWidth = (([int]$bytes[16]) -shl 24) -bor (([int]$bytes[17]) -shl 16) -bor (([int]$bytes[18]) -shl 8) -bor ([int]$bytes[19])
    $actualHeight = (([int]$bytes[20]) -shl 24) -bor (([int]$bytes[21]) -shl 16) -bor (([int]$bytes[22]) -shl 8) -bor ([int]$bytes[23])
    if ($actualWidth -ne $Width -or $actualHeight -ne $Height) { throw "Unexpected PNG dimensions $actualWidth x $actualHeight for $($screen.Name); expected $Width x $Height" }
    if ($containerCaptureFolder -ne $containerEvidence) {
        Copy-Item -LiteralPath $screen.FullName -Destination (Join-Path $containerEvidence $screen.Name) -Force
    }
}
$containerReceipt = [ordered]@{
    status='PASS'; checks=$containerChecks; failures=0; process_exit_code=$containerProcess.ExitCode
    screenshots=$containerScreens.Name; width=$Width; height=$Height; hardware_input_tested=$false; audio_listening_tested=$false
    started_utc=$containerStarted.ToString('o'); finished_utc=[DateTime]::UtcNow.ToString('o')
    evidence_log=$containerLog; capture_source_folder=$containerCaptureFolder
}
$containerReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $containerEvidence 'ContainerTestResult.json') -Encoding UTF8
Write-Output $containerResult
Write-Output "Verified $($containerScreens.Count) fresh container screenshots and normal exit."
