[CmdletBinding()]
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [Alias('GameExecutable')][string]$PackageExe = '',
    [ValidateRange(1,100)][int]$Runs = 3,
    [ValidateRange(10,7200)][int]$TimeoutSeconds = 300,
    [switch]$Stress
)

$ErrorActionPreference = 'Stop'
$benchmarkProjectRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$benchmarkProject = Join-Path $benchmarkProjectRoot 'GreyzoneRaid.uproject'
$benchmarkEngineRoot = [System.IO.Path]::GetFullPath($EngineRoot)
$benchmarkMode = if ($PackageExe) { 'Packaged' } else { 'Editor' }
$benchmarkExecutable = if ($PackageExe) {
    [System.IO.Path]::GetFullPath($PackageExe)
} else {
    Join-Path $benchmarkEngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
}
$benchmarkMap = '/Game/Maps/Depot'
if (!(Test-Path -LiteralPath $benchmarkExecutable -PathType Leaf)) { throw "UE executable not found: $benchmarkExecutable" }
if (!(Test-Path -LiteralPath $benchmarkProject -PathType Leaf)) { throw "Project not found: $benchmarkProject" }

function Get-BenchmarkFileDigest {
    param([string]$Path, [switch]$VersionInfo)
    $digest = [ordered]@{ path = $Path; exists = (Test-Path -LiteralPath $Path -PathType Leaf) }
    if ($digest.exists) {
        $item = Get-Item -LiteralPath $Path
        $digest.bytes = $item.Length
        $digest.last_write_utc = $item.LastWriteTimeUtc.ToString('o')
        $digest.sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
        if ($VersionInfo) {
            $version = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($Path)
            $digest.file_version = $version.FileVersion
            $digest.product_version = $version.ProductVersion
        }
    }
    return $digest
}

function Get-BenchmarkMachineInfo {
    $machine = [ordered]@{
        machine_name = [Environment]::MachineName
        os_version = [Environment]::OSVersion.VersionString
        logical_processors = [Environment]::ProcessorCount
        is_64_bit_process = [Environment]::Is64BitProcess
        powershell_version = $PSVersionTable.PSVersion.ToString()
        query_warnings = @()
    }
    # Keep only reproducibility metadata; do not collect serial numbers or account data.
    foreach ($query in @(
        @{ name = 'cpu'; class = 'Win32_Processor'; fields = @('Name','NumberOfCores','NumberOfLogicalProcessors','MaxClockSpeed') },
        @{ name = 'gpu'; class = 'Win32_VideoController'; fields = @('Name','DriverVersion','AdapterRAM') },
        @{ name = 'computer'; class = 'Win32_ComputerSystem'; fields = @('Manufacturer','Model','TotalPhysicalMemory') },
        @{ name = 'os'; class = 'Win32_OperatingSystem'; fields = @('Caption','Version','BuildNumber') }
    )) {
        try {
            $machine[$query.name] = @(Get-CimInstance -ClassName $query.class -ErrorAction Stop | Select-Object -Property $query.fields)
        } catch {
            $machine.query_warnings += "$($query.class): $($_.Exception.Message)"
        }
    }
    return $machine
}

function ConvertTo-BenchmarkCommandArgument {
    param([string]$Argument)
    # Start-Process joins ArgumentList with spaces. Quote values explicitly for UE FParse.
    if ($Argument.Contains('"') -or $Argument.Contains("`r") -or $Argument.Contains("`n")) {
        throw 'Benchmark arguments must not contain quotes or line breaks.'
    }
    if ($Argument.StartsWith('-') -and $Argument.Contains('=') -and
        ($Argument -match '\s' -or $Argument -match '^-(abslog|RaidBenchmarkOutputDir)=')) {
        $equals = $Argument.IndexOf('=')
        return $Argument.Substring(0, $equals + 1) + '"' + $Argument.Substring($equals + 1) + '"'
    }
    if ($Argument -match '\s') { return '"' + $Argument + '"' }
    return $Argument
}

function Stop-BenchmarkProcessTree {
    param([System.Diagnostics.Process]$Process, [string]$RunDirectory)
    $Process.Refresh()
    if ($Process.HasExited) { return }
    # Scope taskkill to this captured, still-running PID and its descendants only.
    $taskkill = Join-Path $env:SystemRoot 'System32\taskkill.exe'
    $cleanup = Start-Process -FilePath $taskkill -ArgumentList @('/PID', [string]$Process.Id, '/T', '/F') `
        -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $RunDirectory 'ProcessCleanup.stdout.log') `
        -RedirectStandardError (Join-Path $RunDirectory 'ProcessCleanup.stderr.log')
    if (!$cleanup.WaitForExit(15000)) { throw "PID tree cleanup did not finish for benchmark PID $($Process.Id)." }
    $cleanup.Refresh()
    if ($cleanup.ExitCode -ne 0) { throw "PID tree cleanup failed with exit code $($cleanup.ExitCode); inspect ProcessCleanup logs." }
    if (!$Process.WaitForExit(15000)) { throw "Benchmark PID $($Process.Id) is still running after PID tree cleanup." }
}

function Read-BenchmarkCsv {
    param([string]$Path, [string[]]$RequiredColumns, [string]$ExpectedRunId)
    if (!(Test-Path -LiteralPath $Path -PathType Leaf) -or (Get-Item -LiteralPath $Path).Length -eq 0) {
        throw "Missing or empty benchmark CSV: $Path"
    }
    $rows = @(Import-Csv -LiteralPath $Path)
    if ($rows.Count -eq 0) { throw "Benchmark CSV contains no data rows: $Path" }
    $columns = @($rows[0].PSObject.Properties.Name)
    foreach ($column in $RequiredColumns) {
        if ($columns -notcontains $column) { throw "Missing CSV column '$column': $Path" }
    }
    foreach ($row in $rows) {
        if ($row.run_id -cne $ExpectedRunId) { throw "CSV run_id does not match this independent process: $Path" }
    }
    return $rows
}

function Get-BenchmarkNumber {
    param([string]$Value)
    $number = 0.0
    if (![double]::TryParse($Value, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$number) -or
        [double]::IsNaN($number) -or [double]::IsInfinity($number) -or $number -lt 0) {
        throw "Invalid nonnegative benchmark number: '$Value'"
    }
    return $number
}

function Get-BenchmarkMedian {
    param([double[]]$Values)
    $orderedValues = @($Values | Sort-Object)
    $middle = [int][Math]::Floor($orderedValues.Count / 2)
    if ($orderedValues.Count % 2) { return $orderedValues[$middle] }
    return ($orderedValues[$middle - 1] + $orderedValues[$middle]) / 2.0
}

$benchmarkStartedUtc = [DateTime]::UtcNow
$benchmarkStamp = $benchmarkStartedUtc.ToString('yyyyMMdd_HHmmss_fffffffZ')
$benchmarkEvidenceRoot = Join-Path $benchmarkProjectRoot 'Evidence'
if (!(Test-Path -LiteralPath $benchmarkEvidenceRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $benchmarkEvidenceRoot | Out-Null
}
$benchmarkEvidence = Join-Path $benchmarkEvidenceRoot "R6FrameworkBenchmark_${benchmarkMode}_$benchmarkStamp"
# No -Force: an existing experiment is never reused or overwritten.
New-Item -ItemType Directory -Path $benchmarkEvidence | Out-Null
$benchmarkManifestPath = Join-Path $benchmarkEvidence 'FrameworkBenchmarkManifest.json'
$benchmarkSourcePaths = @(
    'Source\GreyzoneRaid\RaidHUD.h', 'Source\GreyzoneRaid\RaidHUD.cpp',
    'Source\GreyzoneRaid\RaidPlayer.h', 'Source\GreyzoneRaid\RaidPlayer.cpp',
    'Source\GreyzoneRaid\R6FrameworkBenchmarkDirector.h', 'Source\GreyzoneRaid\R6FrameworkBenchmarkDirector.cpp'
)
$benchmarkEnginePaths = @(
    'Engine\Source\Runtime\Engine\Public\EngineUtils.h',
    'Engine\Source\Runtime\Engine\Private\EngineUtils.cpp',
    'Engine\Source\Runtime\CoreUObject\Public\UObject\UObjectHash.h',
    'Engine\Source\Runtime\CoreUObject\Private\UObject\UObjectHash.cpp'
)
$benchmarkBuildVersion = Join-Path $benchmarkEngineRoot 'Engine\Build\Build.version'
$benchmarkManifest = [ordered]@{
    schema_version = 1
    status = 'RUNNING'
    mode = $benchmarkMode
    started_utc = $benchmarkStartedUtc.ToString('o')
    finished_utc = $null
    evidence_directory = $benchmarkEvidence
    engine_root = $benchmarkEngineRoot
    requested_package_exe = $PackageExe
    executable = Get-BenchmarkFileDigest -Path $benchmarkExecutable -VersionInfo
    project = Get-BenchmarkFileDigest -Path $benchmarkProject
    script = Get-BenchmarkFileDigest -Path $PSCommandPath
    sources = @($benchmarkSourcePaths | ForEach-Object { Get-BenchmarkFileDigest -Path (Join-Path $benchmarkProjectRoot $_) })
    engine_sources = @($benchmarkEnginePaths | ForEach-Object { Get-BenchmarkFileDigest -Path (Join-Path $benchmarkEngineRoot $_) })
    engine_build_version_file = Get-BenchmarkFileDigest -Path $benchmarkBuildVersion
    engine_build_version = $null
    engine_build_version_warning = $null
    machine = Get-BenchmarkMachineInfo
    settings = [ordered]@{ runs = $Runs; timeout_seconds = $TimeoutSeconds; map = $benchmarkMap; width = 1280; height = 720; stress = [bool]$Stress }
    methodology = @(
        'Each run launches a fresh process and writes to its own empty output directory.',
        'One iteration is the named operation; HUD combinations include multiple framework calls.',
        'Director summary medians and p95 describe batch means in ns/iteration, not individual-call latency percentiles.',
        'Across-run statistics are min/median/max of successful process median_ns_per_iteration values, grouped by scenario and operation.',
        'The harness scenario is reported independently; no overhead is mechanically subtracted.',
        'Local source hashes describe the workspace and selected EngineRoot; packaged binary provenance must be checked against Director metadata.',
        'The runner does not build, cook, install, or use NullRHI.'
    )
    runs = @()
    exports = @()
    errors = @()
}
if (Test-Path -LiteralPath $benchmarkBuildVersion -PathType Leaf) {
    try { $benchmarkManifest.engine_build_version = Get-Content -LiteralPath $benchmarkBuildVersion -Raw | ConvertFrom-Json }
    catch { $benchmarkManifest.engine_build_version_warning = $_.Exception.Message }
}
$benchmarkManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $benchmarkManifestPath -Encoding UTF8
$benchmarkRunMedians = [System.Collections.Generic.List[object]]::new()
$benchmarkAbortRuns = $false

for ($benchmarkRunIndex = 1; $benchmarkRunIndex -le $Runs; $benchmarkRunIndex++) {
    $benchmarkRunName = 'run{0:D2}' -f $benchmarkRunIndex
    $benchmarkRunDirectory = Join-Path $benchmarkEvidence $benchmarkRunName
    New-Item -ItemType Directory -Path $benchmarkRunDirectory | Out-Null
    $benchmarkRunId = '{0}_{1}_{2}' -f $benchmarkMode, $benchmarkStamp, $benchmarkRunName
    $benchmarkLog = Join-Path $benchmarkRunDirectory 'FrameworkBenchmark.log'
    $benchmarkArguments = @()
    if (!$PackageExe) { $benchmarkArguments += @($benchmarkProject, $benchmarkMap, '-game') }
    else { $benchmarkArguments += @($benchmarkMap) }
    $benchmarkArguments += @(
        '-RaidFrameworkBenchmark', "-RaidBenchmarkOutputDir=$benchmarkRunDirectory", "-RaidBenchmarkRunId=$benchmarkRunId",
        '-RenderOffscreen', '-windowed', '-ForceRes', '-ResX=1280', '-ResY=720', '-NoVSync',
        '-unattended', '-nosplash', '-nosound', "-abslog=$benchmarkLog"
    )
    if ($Stress) { $benchmarkArguments += '-RaidBenchmarkStress' }
    $benchmarkCommandLine = ($benchmarkArguments | ForEach-Object { ConvertTo-BenchmarkCommandArgument -Argument $_ }) -join ' '
    $benchmarkWorkingDirectory = Split-Path -Parent $benchmarkExecutable
    $benchmarkRun = [ordered]@{
        run_index = $benchmarkRunIndex; run_id = $benchmarkRunId; status = 'RUNNING'
        directory = $benchmarkRunDirectory; log = $benchmarkLog
        executable = $benchmarkExecutable; working_directory = $benchmarkWorkingDirectory
        arguments = $benchmarkArguments; argument_line = $benchmarkCommandLine
        started_utc = [DateTime]::UtcNow.ToString('o'); finished_utc = $null
        pid = $null; exit_code = $null; timed_out = $false; result_marker = $null
        raw_csv_rows = 0; summary_csv_rows = 0; outputs = @(); error = $null
    }
    $benchmarkManifest.runs += $benchmarkRun
    $benchmarkManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $benchmarkManifestPath -Encoding UTF8
    $benchmarkProcess = $null
    try {
        $benchmarkProcess = Start-Process -FilePath $benchmarkExecutable -ArgumentList $benchmarkCommandLine `
            -WorkingDirectory $benchmarkWorkingDirectory -WindowStyle Hidden -PassThru
        $benchmarkRun.pid = $benchmarkProcess.Id
        $benchmarkManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $benchmarkManifestPath -Encoding UTF8
        Write-Output "[$benchmarkRunIndex/$Runs] $benchmarkMode benchmark PID $($benchmarkProcess.Id): $benchmarkRunDirectory"
        $benchmarkDeadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
        while (!$benchmarkProcess.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -ge $benchmarkDeadline) {
                $benchmarkRun.timed_out = $true
                Stop-BenchmarkProcessTree -Process $benchmarkProcess -RunDirectory $benchmarkRunDirectory
                throw "Benchmark exceeded $TimeoutSeconds seconds; its own PID tree was stopped."
            }
        }
        $benchmarkProcess.Refresh()
        $benchmarkRun.exit_code = $benchmarkProcess.ExitCode
        if (!(Test-Path -LiteralPath $benchmarkLog -PathType Leaf)) { throw "No benchmark log: $benchmarkLog" }
        $benchmarkRun.result_marker = Get-Content -LiteralPath $benchmarkLog | Where-Object { $_ -match 'RAID_FRAMEWORK_BENCHMARK_RESULT:' } | Select-Object -Last 1
        if (!$benchmarkRun.result_marker -or $benchmarkRun.result_marker -notmatch 'RAID_FRAMEWORK_BENCHMARK_RESULT:\s+PASS\b') {
            throw "Benchmark PASS marker absent; inspect $benchmarkLog"
        }
        if ($benchmarkProcess.ExitCode -ne 0) { throw "Benchmark exited abnormally: $($benchmarkProcess.ExitCode)" }
        $benchmarkRaw = @(Read-BenchmarkCsv -Path (Join-Path $benchmarkRunDirectory 'FrameworkBenchmark.csv') `
            -RequiredColumns @('run_id','scenario','operation','world_extractions','inventory_rows','batch_index','iterations','elapsed_ns','ns_per_iteration','checksum') -ExpectedRunId $benchmarkRunId)
        $benchmarkSummary = @(Read-BenchmarkCsv -Path (Join-Path $benchmarkRunDirectory 'FrameworkBenchmarkSummary.csv') `
            -RequiredColumns @('run_id','scenario','operation','world_extractions','inventory_rows','batches','iterations_per_batch','min_ns_per_iteration','median_ns_per_iteration','p95_ns_per_iteration','mean_ns_per_iteration','max_ns_per_iteration','checksum') -ExpectedRunId $benchmarkRunId)
        $benchmarkRun.raw_csv_rows = $benchmarkRaw.Count
        $benchmarkRun.summary_csv_rows = $benchmarkSummary.Count
        $benchmarkMetadataPath = Join-Path $benchmarkRunDirectory 'FrameworkBenchmarkMetadata.json'
        if (!(Test-Path -LiteralPath $benchmarkMetadataPath -PathType Leaf) -or (Get-Item -LiteralPath $benchmarkMetadataPath).Length -eq 0) {
            throw "Missing or empty Director metadata: $benchmarkMetadataPath"
        }
        $benchmarkMetadata = Get-Content -LiteralPath $benchmarkMetadataPath -Raw | ConvertFrom-Json
        if ($null -eq $benchmarkMetadata -or $benchmarkMetadata -isnot [pscustomobject]) { throw 'Director metadata must be a JSON object.' }
        $benchmarkKeys = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $benchmarkThisRunMedians = @($benchmarkSummary | ForEach-Object {
            $row = $_
            if (!$row.scenario -or !$row.operation) { throw 'Summary contains an empty scenario or operation.' }
            if (!$benchmarkKeys.Add($row.scenario + [char]0 + $row.operation)) { throw 'Duplicate scenario/operation in a process summary.' }
            [pscustomobject][ordered]@{
                run_index = $benchmarkRunIndex; run_id = $benchmarkRunId
                scenario = $row.scenario; operation = $row.operation
                world_extractions = $row.world_extractions; inventory_rows = $row.inventory_rows
                batches = $row.batches; iterations_per_batch = $row.iterations_per_batch
                median_ns_per_iteration = (Get-BenchmarkNumber -Value $row.median_ns_per_iteration).ToString('R', [Globalization.CultureInfo]::InvariantCulture)
            }
        })
        $benchmarkRun.status = 'PASS'
        Write-Output $benchmarkRun.result_marker
    } catch {
        $benchmarkRun.status = 'FAIL'
        $benchmarkRun.error = $_.Exception.Message
        $benchmarkManifest.errors += "${benchmarkRunName}: $($_.Exception.Message)"
        $benchmarkAbortRuns = $true
        Write-Warning "${benchmarkRunName}: $($_.Exception.Message)"
    } finally {
        if ($null -ne $benchmarkProcess) {
            try {
                $benchmarkProcess.Refresh()
                if (!$benchmarkProcess.HasExited) { Stop-BenchmarkProcessTree -Process $benchmarkProcess -RunDirectory $benchmarkRunDirectory }
                $benchmarkRun.exit_code = $benchmarkProcess.ExitCode
            } catch {
                $benchmarkRun.status = 'FAIL'
                $benchmarkManifest.errors += "${benchmarkRunName} cleanup: $($_.Exception.Message)"
                $benchmarkAbortRuns = $true
            }
        }
        $benchmarkRun.finished_utc = [DateTime]::UtcNow.ToString('o')
        $benchmarkRun.outputs = @(@('FrameworkBenchmark.csv','FrameworkBenchmarkSummary.csv','FrameworkBenchmarkMetadata.json','FrameworkBenchmark.log') | ForEach-Object {
            Get-BenchmarkFileDigest -Path (Join-Path $benchmarkRunDirectory $_)
        })
        $benchmarkManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $benchmarkManifestPath -Encoding UTF8
    }
    if ($benchmarkRun.status -eq 'PASS') {
        foreach ($medianRow in $benchmarkThisRunMedians) { $benchmarkRunMedians.Add($medianRow) }
    }
    # Stop after any failed run; abnormal exits can leave untracked helper processes.
    if ($benchmarkAbortRuns) { break }
}

try {
    if ($benchmarkRunMedians.Count -gt 0) {
        $benchmarkPerRunPath = Join-Path $benchmarkEvidence 'FrameworkBenchmarkRuns.csv'
        $benchmarkRunMedians | Export-Csv -LiteralPath $benchmarkPerRunPath -NoTypeInformation -Encoding UTF8
        $benchmarkManifest.exports += Get-BenchmarkFileDigest -Path $benchmarkPerRunPath
        $benchmarkAcrossRuns = @($benchmarkRunMedians | Group-Object -Property scenario,operation | ForEach-Object {
            $groupRows = @($_.Group)
            $first = $groupRows[0]
            # Pilot calibration can select different iteration counts in each process.
            $dimensions = @($groupRows | Select-Object world_extractions,inventory_rows,batches -Unique)
            if ($dimensions.Count -ne 1) { throw "Inconsistent cross-run inputs for $($first.scenario)/$($first.operation)." }
            $values = @($groupRows | ForEach-Object { Get-BenchmarkNumber -Value $_.median_ns_per_iteration } | Sort-Object)
            [pscustomobject][ordered]@{
                scenario = $first.scenario; operation = $first.operation
                world_extractions = $first.world_extractions; inventory_rows = $first.inventory_rows
                batches = $first.batches
                successful_runs = $values.Count; requested_runs = $Runs; all_runs_present = ($values.Count -eq $Runs)
                min_process_median_ns_per_iteration = $values[0].ToString('R', [Globalization.CultureInfo]::InvariantCulture)
                median_process_median_ns_per_iteration = (Get-BenchmarkMedian -Values $values).ToString('R', [Globalization.CultureInfo]::InvariantCulture)
                max_process_median_ns_per_iteration = $values[-1].ToString('R', [Globalization.CultureInfo]::InvariantCulture)
            }
        } | Sort-Object scenario,operation)
        $benchmarkAcrossRunPath = Join-Path $benchmarkEvidence 'FrameworkBenchmarkAcrossRuns.csv'
        $benchmarkAcrossRuns | Export-Csv -LiteralPath $benchmarkAcrossRunPath -NoTypeInformation -Encoding UTF8
        $benchmarkManifest.exports += Get-BenchmarkFileDigest -Path $benchmarkAcrossRunPath
        if (@($benchmarkAcrossRuns | Where-Object { !$_.all_runs_present }).Count -gt 0) {
            $benchmarkManifest.errors += 'At least one scenario/operation lacks a valid summary from every requested process.'
        }
    }
} catch {
    $benchmarkManifest.errors += "Cross-run export: $($_.Exception.Message)"
}
$benchmarkFailedRuns = @($benchmarkManifest.runs | Where-Object { $_.status -ne 'PASS' })
$benchmarkManifest.status = if ($benchmarkFailedRuns.Count -eq 0 -and $benchmarkManifest.errors.Count -eq 0) { 'PASS' } else { 'FAIL' }
$benchmarkManifest.finished_utc = [DateTime]::UtcNow.ToString('o')
$benchmarkManifest | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $benchmarkManifestPath -Encoding UTF8
Write-Output "Benchmark evidence: $benchmarkEvidence"
if ($benchmarkManifest.status -ne 'PASS') { throw "Framework benchmark experiment failed; inspect $benchmarkManifestPath" }
Write-Output "Framework benchmark PASS: $Runs independent processes; per-process and across-run iteration summaries exported."
