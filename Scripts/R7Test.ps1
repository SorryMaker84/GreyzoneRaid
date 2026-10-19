param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [ValidateSet('Test','Preview','Performance')][string]$Mode = 'Test',
    [ValidateRange(960,3840)][int]$Width = 1600,
    [ValidateRange(540,2160)][int]$Height = 900,
    [ValidateRange(60,1800)][int]$TimeoutSeconds = 1200,
    [string]$OutputDirectory = '',
    [string]$Manifest = '',
    [string]$PerformanceView = '',
    [ValidateRange(10,120)][int]$SampleSeconds = 20,
    [switch]$FinaleOnly,
    [switch]$R10Journey
)
$ErrorActionPreference = 'Stop'
$r7ProjectRoot = Split-Path -Parent $PSScriptRoot
$r7Runtime = if ($GameExecutable) { 'Packaged' } else { 'Editor' }
$r7RunId = '{0}_{1}_{2}' -f $r7Runtime,$Mode,([DateTime]::UtcNow.ToString('yyyyMMddTHHmmss_fffffffZ'))
if (!$OutputDirectory) { $OutputDirectory = Join-Path $r7ProjectRoot ("Evidence\R7${Mode}_$r7RunId") }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) {
    if (@(Get-ChildItem -LiteralPath $OutputDirectory -Force).Count -ne 0) { throw 'R7 output must be empty; historical evidence is never overwritten.' }
} else { New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null }
$r7Log = Join-Path $OutputDirectory 'R7.log'
if ($GameExecutable) {
    $r7Executable = [IO.Path]::GetFullPath($GameExecutable)
    $r7WorkingDirectory = Split-Path -Parent $r7Executable
    $r7Arguments = @('/Game/Maps/Frontier')
} else {
    $r7Executable = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
    $r7WorkingDirectory = $r7ProjectRoot
    $r7Arguments = @((Join-Path $r7ProjectRoot 'GreyzoneRaid.uproject'),'/Game/Maps/Frontier','-game')
}
if (!(Test-Path -LiteralPath $r7Executable -PathType Leaf)) { throw "Missing executable: $r7Executable" }
$r7Arguments += @("-RaidR7$Mode","-RaidR7RunId=$r7RunId","-RaidR7OutputDir=$OutputDirectory",'-RenderOffscreen','-ForceRes','-windowed',"-ResX=$Width","-ResY=$Height",'-NoVSync','-unattended','-nosplash','-nosound',"-abslog=$r7Log")
if ($Manifest) { $r7Arguments += "-RaidR7Manifest=$([IO.Path]::GetFullPath($Manifest))" }
if ($PerformanceView) { $r7Arguments += "-RaidR7PerformanceView=$PerformanceView" }
if ($R10Journey) { $r7Arguments += '-RaidR10Journey' }
if ($Mode -eq 'Performance') { $r7Arguments += "-RaidR7SampleSeconds=$SampleSeconds" }
if ($FinaleOnly) {
    if ($Mode -ne 'Test') { throw 'FinaleOnly applies only to gameplay Test.' }
    $r7Arguments += '-RaidR7FinaleOnly'
}
function ConvertTo-R7Argument([string]$Value) {
    if ($Value -match '["\r\n]') { throw 'Quotes and newlines are not allowed in R7 arguments.' }
    if ($Value -match '\s') {
        # FParse::Value expects a quoted value after '=', not a quoted whole switch.
        $r7Equals = $Value.IndexOf('=')
        if ($Value.StartsWith('-') -and $r7Equals -gt 0) { return $Value.Substring(0,$r7Equals + 1) + '"' + $Value.Substring($r7Equals + 1) + '"' }
        return '"' + $Value + '"'
    }
    return $Value
}
$r7ArgumentLine = ($r7Arguments | ForEach-Object { ConvertTo-R7Argument $_ }) -join ' '
$r7Started = [DateTime]::UtcNow
$r7Receipt = [ordered]@{
    schema_version=1; status='RUNNING'; run_id=$r7RunId; runtime=$r7Runtime; mode=$Mode
    map='/Game/Maps/Frontier'; executable=$r7Executable; arguments=$r7Arguments
    width=$Width; height=$Height; started_utc=$r7Started.ToString('o'); finished_utc=$null
    process_id=$null; native_exit_code=$null; wrapper_exit_code=$null; log=$r7Log
    screenshots=@(); hardware_input_tested=$false; audio_listened=$false; error=$null
    test_scope=$(if ($FinaleOnly) { 'finale_only_no_full_route_coverage' } else { 'full' })
}
$r7ReceiptPath = Join-Path $OutputDirectory 'R7RunReceipt.json'
$r7Process = $null
try {
    $r7Process = Start-Process -FilePath $r7Executable -ArgumentList $r7ArgumentLine -WorkingDirectory $r7WorkingDirectory -WindowStyle Hidden -PassThru
    $r7Receipt.process_id = $r7Process.Id
    $r7Receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $r7ReceiptPath -Encoding UTF8
    Write-Output "R7_RUN_START: run_id=$r7RunId mode=$Mode runtime=$r7Runtime pid=$($r7Process.Id) output=$OutputDirectory"
    $r7Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (!$r7Process.WaitForExit(1000)) {
        if ([DateTime]::UtcNow -ge $r7Deadline) { throw "R7 $Mode timed out after $TimeoutSeconds seconds." }
    }
    $r7Process.Refresh()
    $r7Receipt.native_exit_code = $r7Process.ExitCode
    Write-Output "R7_PROCESS_EXIT_CODE: $($r7Process.ExitCode)"
    if ($r7Process.ExitCode -ne 0) { throw "R7 process exited $($r7Process.ExitCode)." }
    if (!(Test-Path -LiteralPath $r7Log -PathType Leaf) -or (Get-Item -LiteralPath $r7Log).LastWriteTimeUtc -lt $r7Started) { throw 'No fresh R7 engine log.' }
    $r7LogText = [IO.File]::ReadAllText($r7Log)
    if ($r7LogText -match '(?im)\bLog\w+:\s*(?:Error|Fatal):|R7_CHECK:FAIL|R7_RESULT: FAIL|Assertion failed:|Unhandled Exception|Fatal error:') { throw 'R7 engine log contains a failure/error; inspect the preserved log.' }
    $r7Result = [regex]::Matches($r7LogText, "R7_RESULT: PASS mode=$Mode checks=(\d+) failures=0 captures=(\d+) frames=(\d+) run_id=$([regex]::Escape($r7RunId))")
    if ($r7Result.Count -ne 1) { throw 'Expected exactly one matching R7 success result.' }
    if ($r7LogText -notmatch 'LogExit: Exiting\.') { throw 'R7 engine normal-exit marker missing.' }
    $r7DirectorPath = Join-Path $OutputDirectory 'R7DirectorReceipt.json'
    if (!(Test-Path -LiteralPath $r7DirectorPath)) { throw 'Director receipt missing.' }
    $r7Director = Get-Content -Raw -LiteralPath $r7DirectorPath | ConvertFrom-Json
    if ($r7Director.status -ne 'PASS' -or $r7Director.run_id -ne $r7RunId -or $r7Director.mode -ne $Mode -or $r7Director.map -ne '/Game/Maps/Frontier') { throw 'Director receipt identity/status mismatch.' }
    if ($r7Director.width -ne $Width -or $r7Director.height -ne $Height) { throw 'Actual viewport dimensions differ from the requested resolution.' }
    $r7Screens = @(Get-ChildItem -LiteralPath $OutputDirectory -File -Filter '*.png')
    if ($r7Screens.Count -ne [int]$r7Director.screenshots -or $r7Screens.Count -ne [int]$r7Result[0].Groups[2].Value) { throw 'Screenshot count differs between receipt, log and files.' }
    foreach ($r7Screen in $r7Screens) {
        $r7Bytes = [IO.File]::ReadAllBytes($r7Screen.FullName)
        if ($r7Screen.LastWriteTimeUtc -lt $r7Started -or $r7Bytes.Length -le 128 -or [BitConverter]::ToString($r7Bytes[0..7]) -ne '89-50-4E-47-0D-0A-1A-0A') { throw "Stale/incomplete PNG: $($r7Screen.Name)" }
        $r7PngWidth = ([int]$r7Bytes[16] -shl 24) -bor ([int]$r7Bytes[17] -shl 16) -bor ([int]$r7Bytes[18] -shl 8) -bor [int]$r7Bytes[19]
        $r7PngHeight = ([int]$r7Bytes[20] -shl 24) -bor ([int]$r7Bytes[21] -shl 16) -bor ([int]$r7Bytes[22] -shl 8) -bor [int]$r7Bytes[23]
        if ($r7PngWidth -ne $Width -or $r7PngHeight -ne $Height) { throw "PNG resolution mismatch: $($r7Screen.Name)" }
        $r7Receipt.screenshots += [ordered]@{file=$r7Screen.Name;bytes=$r7Screen.Length;width=$r7PngWidth;height=$r7PngHeight;sha256=(Get-FileHash -LiteralPath $r7Screen.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
    if ($Mode -eq 'Preview' -and ($r7Screens.Count -lt 5 -or $r7Screens.Count -gt 12)) { throw 'Preview requires five to twelve authored views.' }
    if ($Mode -eq 'Test') {
        $r7RequiredChecks = @('technical_navigation_audit_is_not_player_traversal_proof','real_M_and_escape_bindings_preserve_running_raid','release_cancels_mission_and_resets_progress','lost_view_cancels_mission_and_resets_progress','out_of_range_cancels_mission_and_resets_progress','backpack_cancels_mission_and_resets_progress','production_damage_event_cancels_mission_and_resets_progress','six_second_real_hold_completes_optional_mission_and_unlocks_north','locked_north_cannot_extract_even_with_loot_and_full_wait','unpossessed_player_resets_partial_extraction','same_frame_replacement_pawn_cannot_inherit_partial_countdown','south_extracts_without_completing_optional_mission','north_extracts_after_real_mission_completion','normal_map_ai_causes_real_player_death','two_successful_raids_and_death_restart_remain_isolated','eight_authored_routes_and_twenty_four_containers_actually_exercised','production_guards_patrolled_pursued_and_damaged_player')
        foreach ($r7Check in $r7RequiredChecks) {
            if ($FinaleOnly -and $r7Check -eq 'eight_authored_routes_and_twenty_four_containers_actually_exercised') { continue }
            if ($r7LogText -notmatch ('R7_CHECK:PASS ' + [regex]::Escape($r7Check) + ' \|')) { throw "Required gameplay proof missing: $r7Check" }
        }
        if ($r7Director.test_scope -ne $r7Receipt.test_scope -or $r7Director.round -ne 4) { throw 'Incomplete or mismatched gameplay scope.' }
        if (!$FinaleOnly -and (@($r7Director.authored_routes_completed).Count -lt 8 -or $r7Director.container_interactions -lt 25)) { throw 'Incomplete route/container/restart evidence.' }
        if ($FinaleOnly -and $r7Director.container_interactions -lt 2) { throw 'Finale probe did not acquire real loot in each successful raid.' }
        foreach ($r7Proof in @('normal_ai_patrol_moved','normal_ai_chase_moved','normal_ai_damaged_player','south_success_without_mission','north_success_after_mission','normal_ai_caused_death')) {
            if ($r7Director.$r7Proof -ne $true) { throw "Director gameplay proof is false: $r7Proof" }
        }
        if (!(Test-Path -LiteralPath (Join-Path $OutputDirectory 'PlayerRoute.csv'))) { throw 'Actual player route telemetry missing.' }
    }
    if ($Mode -eq 'Performance') {
        $r7Frames = @(Import-Csv -LiteralPath (Join-Path $OutputDirectory 'FrameTimes.csv'))
        if ($r7Frames.Count -lt 120 -or $r7Frames.Count -ne [int]$r7Director.frame_samples) { throw 'Incomplete performance CSV.' }
        foreach ($r7Frame in $r7Frames) {
            if ($r7Frame.run_id -ne $r7RunId -or [double]$r7Frame.wall_frame_ms -le 0) { throw 'Invalid raw frame identity/timing.' }
        }
        $r7Receipt.frame_samples = $r7Frames.Count
        $r7Receipt.gpu_samples_available = @($r7Frames | Where-Object gpu_available -eq '1').Count
    }
    $r7Receipt.status = 'PASS'
    $r7Receipt.wrapper_exit_code = 0
    $r7Receipt.checks = [int]$r7Director.checks
    $r7Receipt.log_sha256 = (Get-FileHash -LiteralPath $r7Log -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Output $r7Result[0].Value
    Write-Output "R7_RUN_EXIT: run_id=$r7RunId native_exit_code=0 wrapper_exit_code=0"
} catch {
    $r7Receipt.status = 'FAIL'
    $r7Receipt.wrapper_exit_code = 1
    $r7Receipt.error = $_.Exception.Message
    throw
} finally {
    if ($r7Process) {
        $r7Process.Refresh()
        if (!$r7Process.HasExited) {
            # Restrict cleanup to the process tree created by this invocation.
            & "$env:SystemRoot\System32\taskkill.exe" /PID $r7Process.Id /T /F | Out-Null
            if ($LASTEXITCODE -ne 0) { Write-Warning 'Could not stop the owned R7 process tree.' }
        }
    }
    $r7Receipt.finished_utc = [DateTime]::UtcNow.ToString('o')
    $r7Receipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $r7ReceiptPath -Encoding UTF8
}
