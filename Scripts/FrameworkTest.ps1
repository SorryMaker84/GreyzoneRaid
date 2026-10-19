[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = ''
)
$ErrorActionPreference = 'Stop'
$raidProjectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$raidMode = if ($GameExecutable) { 'Packaged' } else { 'Editor' }
$raidEvidence = Join-Path $raidProjectRoot ('Evidence\R6Framework_' + $raidMode)
New-Item -ItemType Directory -Path $raidEvidence -Force | Out-Null
$raidLog = Join-Path $raidEvidence 'FrameworkTest.log'
$raidStarted = [DateTime]::UtcNow
if ($GameExecutable) {
    $raidExecutable = [IO.Path]::GetFullPath($GameExecutable)
    $raidInner = Join-Path (Split-Path -Parent $raidExecutable) 'GreyzoneRaid\Binaries\Win64\GreyzoneRaid.exe'
    if (Test-Path -LiteralPath $raidInner -PathType Leaf) { $raidExecutable = $raidInner }
    $raidArguments = @('/Game/Maps/Depot')
} else {
    $raidExecutable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $raidArguments = @(('"' + (Join-Path $raidProjectRoot 'GreyzoneRaid.uproject') + '"'), '/Game/Maps/Depot', '-game')
}
if (!(Test-Path -LiteralPath $raidExecutable -PathType Leaf)) { throw "Missing executable: $raidExecutable" }
$raidArguments += @('-RaidFrameworkTest','-RenderOffscreen','-ForceRes','-windowed','-ResX=1600','-ResY=900','-nosplash','-unattended','-nosound',('"-abslog=' + $raidLog + '"'))
$raidProcess = Start-Process -FilePath $raidExecutable -ArgumentList $raidArguments -WindowStyle Hidden -PassThru
try {
    $raidDeadline = [DateTime]::UtcNow.AddSeconds(180)
    while (!$raidProcess.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -gt $raidDeadline) { throw 'R6 framework test timed out.' }
    }
    $raidProcess.Refresh()
    if ($raidProcess.ExitCode -ne 0) { throw "R6 framework process exited $($raidProcess.ExitCode)." }
    if (!(Test-Path -LiteralPath $raidLog) -or (Get-Item -LiteralPath $raidLog).LastWriteTimeUtc -lt $raidStarted) {
        throw 'Missing or stale framework log.'
    }
    $raidText = [IO.File]::ReadAllText($raidLog)
    foreach ($raidPhase in @('kernel','registered_components','interaction_lifecycle')) {
        if ($raidText -notmatch ('RAID_FRAMEWORK_PHASE:\s*' + [regex]::Escape($raidPhase))) {
            throw "Framework phase missing: $raidPhase"
        }
    }
    if ($raidText -match 'RAID_FRAMEWORK_CHECK:\s*FAIL' -or $raidText -match '(?i)Fatal error:|Assertion failed:|Unhandled Exception:|LogTemp: Error:') {
        throw 'Framework assertion or engine error detected; inspect the log.'
    }
    $raidResult = [regex]::Match($raidText, 'RAID_FRAMEWORK_RESULT:\s*PASS\s+checks=(\d+)\s+failures=0')
    if (!$raidResult.Success) { throw 'Framework PASS summary missing.' }
    $raidPassed = [regex]::Matches($raidText, 'RAID_FRAMEWORK_CHECK:\s*PASS').Count
    if ($raidPassed -ne [int]$raidResult.Groups[1].Value -or $raidPassed -lt 70) {
        throw "Framework check count mismatch/incomplete: $raidPassed"
    }
    $raidBeforeResult = $raidText.Substring(0, $raidResult.Index)
    foreach ($raidProof in @('production_catalog_asset_loaded_without_fallback','production_catalog_medkit_grant_and_deferred_use','production_catalog_ammo_grant_and_use')) {
        $raidProofPattern = 'RAID_FRAMEWORK_CHECK:\s*PASS\s+' + [regex]::Escape($raidProof) + '\s*\|'
        if ([regex]::Matches($raidBeforeResult, $raidProofPattern).Count -ne 1) {
            throw "Production catalog proof missing or duplicated: $raidProof"
        }
    }
    [ordered]@{
        status='PASS'; mode=$raidMode; checks=$raidPassed; started_utc=$raidStarted.ToString('o');
        finished_utc=[DateTime]::UtcNow.ToString('o'); executable=$raidExecutable;
        executable_sha256=(Get-FileHash -LiteralPath $raidExecutable -Algorithm SHA256).Hash;
        log=$raidLog; log_sha256=(Get-FileHash -LiteralPath $raidLog -Algorithm SHA256).Hash;
        limitations='Automated game-thread commands and real UE lifecycle; no hardware mouse/keyboard or audio verification.'
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $raidEvidence 'FrameworkTestResult.json') -Encoding UTF8
    Write-Output "R6 framework $raidMode PASS: $raidPassed checks. Log: $raidLog"
} finally {
    $raidProcess.Refresh()
    if (!$raidProcess.HasExited) {
        & taskkill.exe /PID $raidProcess.Id /T /F | Out-Null
        if ($LASTEXITCODE -ne 0) { Write-Warning 'Could not terminate the framework process tree.' }
    }
    $raidProcess.Dispose()
}
