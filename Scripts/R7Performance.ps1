param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [int]$Width = 1600,
    [int]$Height = 900,
    [ValidateRange(1,10)][int]$Runs = 3,
    [ValidateRange(10,120)][int]$SampleSeconds = 20,
    [string]$PerformanceView = '',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
if (!$OutputDirectory) { $OutputDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) ('Evidence\R7Performance_' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmss_fffffffZ')) }
$r7PerfRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $r7PerfRoot) { throw 'Performance group directory must be new.' }
New-Item -ItemType Directory -Path $r7PerfRoot -Force | Out-Null
$r7PerfSummary = [ordered]@{schema_version=1;status='RUNNING';runs=@();comparison_baseline=$null;fps_improvement_claimed=$false;started_utc=[DateTime]::UtcNow.ToString('o');finished_utc=$null}
try {
    for ($r7Run = 1; $r7Run -le $Runs; ++$r7Run) {
        $r7RunDirectory = Join-Path $r7PerfRoot ('run{0:d2}' -f $r7Run)
        & (Join-Path $PSScriptRoot 'R7Test.ps1') -Mode Performance -EngineRoot $EngineRoot -GameExecutable $GameExecutable -Width $Width -Height $Height -OutputDirectory $r7RunDirectory -PerformanceView $PerformanceView -SampleSeconds $SampleSeconds -TimeoutSeconds 240
        $r7PerfSummary.runs += (Get-Content -Raw -LiteralPath (Join-Path $r7RunDirectory 'R7RunReceipt.json') | ConvertFrom-Json)
    }
    $r7PerfSummary.status = 'PASS'
} catch {
    $r7PerfSummary.status = 'FAIL'
    $r7PerfSummary.error = $_.Exception.Message
    throw
} finally {
    $r7PerfSummary.finished_utc = [DateTime]::UtcNow.ToString('o')
    $r7PerfSummary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $r7PerfRoot 'R7PerformanceManifest.json') -Encoding UTF8
}
Write-Output "R7_PERFORMANCE: $Runs independent processes complete; raw frame timings in $r7PerfRoot; no FPS improvement assertion."
