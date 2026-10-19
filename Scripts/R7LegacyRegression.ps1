param([string]$GameExecutable = '', [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$r7LegacyRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$r7LegacyOutput = [IO.Path]::GetFullPath($OutputDirectory)
$r7LegacyRuntime = if ($GameExecutable) { 'Packaged' } else { 'Editor' }
function Assert-R7LocalPath([string]$Path) {
    $r7ResolvedPath = [IO.Path]::GetFullPath($Path)
    if (!$r7ResolvedPath.StartsWith($r7LegacyRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Regression path must stay within project: $r7ResolvedPath"
    }
    return $r7ResolvedPath
}
$null = Assert-R7LocalPath $r7LegacyOutput
if (Test-Path -LiteralPath $r7LegacyOutput) { throw 'Regression output must be new.' }
New-Item -ItemType Directory -Path $r7LegacyOutput | Out-Null
$r7LegacyReceipt = [ordered]@{runtime=$r7LegacyRuntime;status='RUNNING';started_utc=[DateTime]::UtcNow.ToString('o');suites=@()}
try {
    foreach ($r7Suite in @(
        @{name='Framework';script='FrameworkTest.ps1';folder="R6Framework_$r7LegacyRuntime"},
        @{name='SelfTest';script='Test.ps1';folder='R6SelfTest'},
        @{name='Flow';script='FlowTest.ps1';folder='R6Flow'}
    )) {
        $r7Fixed = Assert-R7LocalPath (Join-Path $r7LegacyRoot ('Evidence\' + $r7Suite.folder))
        $r7Preserved = Assert-R7LocalPath (Join-Path $r7LegacyOutput ('Original_' + $r7Suite.folder))
        $r7Fresh = Assert-R7LocalPath (Join-Path $r7LegacyOutput $r7Suite.name)
        $r7HadOriginal = Test-Path -LiteralPath $r7Fixed
        if ($r7HadOriginal) { Move-Item -LiteralPath $r7Fixed -Destination $r7Preserved }
        $r7SuiteReceipt = [ordered]@{suite=$r7Suite.name;status='RUNNING';started_utc=[DateTime]::UtcNow.ToString('o')}
        try {
            & (Join-Path $PSScriptRoot $r7Suite.script) -GameExecutable $GameExecutable
            $r7SuiteReceipt.status = 'PASS'
        } catch {
            $r7SuiteReceipt.status = 'FAIL'
            $r7SuiteReceipt.error = $_.Exception.Message
            throw
        } finally {
            if (Test-Path -LiteralPath $r7Fixed) { Move-Item -LiteralPath $r7Fixed -Destination $r7Fresh }
            if ($r7HadOriginal) { Move-Item -LiteralPath $r7Preserved -Destination $r7Fixed }
            $r7SuiteReceipt.finished_utc = [DateTime]::UtcNow.ToString('o')
            $r7LegacyReceipt.suites += $r7SuiteReceipt
        }
    }
    $r7LegacyReceipt.status = 'PASS'
} catch {
    $r7LegacyReceipt.status = 'FAIL'
    $r7LegacyReceipt.error = $_.Exception.Message
    throw
} finally {
    $r7LegacyReceipt.finished_utc = [DateTime]::UtcNow.ToString('o')
    $r7LegacyReceipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $r7LegacyOutput 'R7LegacyReceipt.json') -Encoding UTF8
}
