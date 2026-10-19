[CmdletBinding()]
param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$raidRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
function Get-RaidBuildBinding {
    $raidPaths = @(Get-ChildItem -LiteralPath (Join-Path $raidRoot 'Source') -File -Recurse | Sort-Object FullName | ForEach-Object FullName)
    $raidPaths += @(
        (Join-Path $raidRoot 'Binaries\Win64\UnrealEditor-GreyzoneRaid.dll'),
        (Join-Path $raidRoot 'Packaged_R6\Windows\GreyzoneRaid\Binaries\Win64\GreyzoneRaid.exe')
    )
    @($raidPaths | ForEach-Object {
        [ordered]@{ path=$_; sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash }
    })
}
$raidReceipt = [ordered]@{ status='RUNNING'; started_utc=[DateTime]::UtcNow.ToString('o'); before=(Get-RaidBuildBinding); groups=@() }
try {
    foreach ($raidMode in @('Editor','Packaged')) {
        $raidExisting = @(Get-ChildItem -LiteralPath (Join-Path $raidRoot 'Evidence') -Directory -Filter ('R6FrameworkBenchmark_' + $raidMode + '_*') | ForEach-Object FullName)
        $raidParameters = @{ EngineRoot=$EngineRoot; Runs=3 }
        if ($raidMode -eq 'Packaged') { $raidParameters.PackageExe = Join-Path $raidRoot 'Packaged_R6\Windows\GreyzoneRaid.exe' }
        & (Join-Path $PSScriptRoot 'FrameworkBenchmark.ps1') @raidParameters
        $raidCreated = @(Get-ChildItem -LiteralPath (Join-Path $raidRoot 'Evidence') -Directory -Filter ('R6FrameworkBenchmark_' + $raidMode + '_*') | Where-Object { $_.FullName -notin $raidExisting })
        if ($raidCreated.Count -ne 1) { throw "Expected exactly one new $raidMode benchmark directory" }
        $raidReceipt.groups += @{mode=$raidMode; directory=$raidCreated[0].FullName}
    }
    $raidReceipt.after = Get-RaidBuildBinding
    if (($raidReceipt.before | ConvertTo-Json -Depth 5 -Compress) -cne ($raidReceipt.after | ConvertTo-Json -Depth 5 -Compress)) { throw 'Source or final game module changed during post benchmarks' }
    $raidReceipt.status = 'PASS'
} catch {
    $raidReceipt.status = 'FAIL'
    $raidReceipt.error = $_.Exception.Message
    throw
} finally {
    $raidReceipt.finished_utc = [DateTime]::UtcNow.ToString('o')
    $raidReceipt | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $raidRoot 'Evidence\R6PostBenchmarkBuildBinding.json') -Encoding UTF8
}
Write-Output 'R6_POST_BENCHMARKS: PASS six independent processes; Source and final game modules unchanged.'
