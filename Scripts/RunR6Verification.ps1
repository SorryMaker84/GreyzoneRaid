[CmdletBinding()]
param(
    [ValidateSet('Both','Editor','Packaged')][string]$Mode = 'Both',
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)
$ErrorActionPreference = 'Stop'
$raidRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$raidPython = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
$raidPackage = Join-Path $raidRoot 'Packaged_R6\Windows\GreyzoneRaid.exe'
$raidWrapperRoot = Join-Path $raidRoot 'Evidence\R6FinalWrappers'
New-Item -ItemType Directory -Path $raidWrapperRoot -Force | Out-Null
$raidModes = if ($Mode -eq 'Both') { @('Packaged','Editor') } else { @($Mode) }
$raidSuites = @(
    @{ Name='Framework'; Script='FrameworkTest.ps1'; Width=1600; Height=900; Directory=''; Log='FrameworkTest.log' },
    @{ Name='Container'; Script='ContainerTest.ps1'; Width=1600; Height=900; Directory='R6Container'; Log='ContainerTest.log' },
    @{ Name='Flow'; Script='FlowTest.ps1'; Width=1600; Height=900; Directory='R6Flow'; Log='FlowTest.log' },
    @{ Name='Self'; Script='Test.ps1'; Width=1280; Height=720; Directory='R6SelfTest'; Log='SelfTest.log' },
    @{ Name='Visual'; Script='VisualReview.ps1'; Width=1600; Height=900; Directory='R6Visual'; Log='VisualReview.log' }
)
foreach ($raidMode in $raidModes) {
    $raidRuns = @($raidSuites)
    if ($raidMode -eq 'Packaged') {
        $raidRuns += @(
            @{ Name='Container'; Script='ContainerTest.ps1'; Width=1280; Height=720; Directory='R6Container'; Log='ContainerTest.log' },
            @{ Name='Flow'; Script='FlowTest.ps1'; Width=1280; Height=720; Directory='R6Flow'; Log='FlowTest.log' }
        )
    }
    foreach ($raidSuite in $raidRuns) {
        $raidRunId = '{0}_{1}_{2}x{3}' -f $raidMode,$raidSuite.Name,$raidSuite.Width,$raidSuite.Height
        $raidReceipt = Join-Path $raidRoot ('Evidence\R6Final\' + $raidRunId + '\receipt.json')
        if (Test-Path -LiteralPath $raidReceipt) { throw "Receipt already exists; audit it before intentionally starting another verification: $raidRunId" }
        $raidWrapperLog = Join-Path $raidWrapperRoot ($raidRunId + '.log')
        $raidStarted = [DateTime]::UtcNow
        Write-Output ("START {0} {1}" -f $raidRunId,$raidStarted.ToString('o'))
        $raidParameters = @{ EngineRoot=$EngineRoot }
        if ($raidMode -eq 'Packaged') { $raidParameters.GameExecutable = $raidPackage }
        if ($raidSuite.Name -in @('Container','Flow')) {
            $raidParameters.Width = $raidSuite.Width
            $raidParameters.Height = $raidSuite.Height
        }
        try {
            & (Join-Path $PSScriptRoot $raidSuite.Script) @raidParameters *> $raidWrapperLog
            # Each called wrapper independently checks the native UE process exit.
            Add-Content -LiteralPath $raidWrapperLog -Value "R6_RUN_EXIT: run_id=$raidRunId exit_code=0" -Encoding UTF8
        } catch {
            Add-Content -LiteralPath $raidWrapperLog -Value "R6_RUN_EXIT: run_id=$raidRunId exit_code=1" -Encoding UTF8
            throw
        }
        $raidFinished = [DateTime]::UtcNow
        $raidCaptureRoot = if ($raidMode -eq 'Packaged') {
            Join-Path $raidRoot 'Packaged_R6\Windows\GreyzoneRaid\Evidence'
        } else { Join-Path $raidRoot 'Evidence' }
        $raidCaptureDirectory = if ($raidSuite.Directory) { Join-Path $raidCaptureRoot $raidSuite.Directory } else { '' }
        $raidGameLog = switch ($raidSuite.Name) {
            'Framework' { Join-Path $raidRoot ("Evidence\R6Framework_$raidMode\FrameworkTest.log") }
            'Visual' { Join-Path $raidRoot 'Evidence\R6VisualReview.log' }
            default { Join-Path $raidRoot ('Evidence\' + $raidSuite.Directory + '\' + $raidSuite.Log) }
        }
        $raidCaptureArguments = @(
            (Join-Path $PSScriptRoot 'audit_r6_delivery.py'), 'capture', '--project-root', $raidRoot,
            '--mode', $raidMode, '--suite', $raidSuite.Name,
            '--width', [string]$raidSuite.Width, '--height', [string]$raidSuite.Height,
            '--log', $raidGameLog, '--wrapper-log', $raidWrapperLog, '--wrapper-exit-code', '0',
            '--started-utc', $raidStarted.ToString('o'), '--finished-utc', $raidFinished.ToString('o')
        )
        if ($raidCaptureDirectory) { $raidCaptureArguments += @('--capture-dir', $raidCaptureDirectory) }
        if ($raidMode -eq 'Packaged') { $raidCaptureArguments += @('--package-exe', $raidPackage) }
        & $raidPython @raidCaptureArguments
        if ($LASTEXITCODE -ne 0) { throw "Evidence capture rejected $raidRunId; inspect its output." }
        Write-Output ("PASS {0} {1}" -f $raidRunId,$raidFinished.ToString('o'))
    }
}
Write-Output 'R6 verification runs and their evidence captures completed. Run the delivery audit separately after benchmark capture.'
