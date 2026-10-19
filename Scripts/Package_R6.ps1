param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Packaged_R6','Packaged_R6_Baseline')][string]$OutputName = 'Packaged_R6'
)
$ErrorActionPreference = 'Stop'
$raidProjectRoot = Split-Path -Parent $PSScriptRoot
$raidUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$raidArchive = Join-Path $raidProjectRoot $OutputName
& $raidUat BuildCookRun "-project=$raidProjectRoot\GreyzoneRaid.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -AdditionalCookerOptions=-SkipZenStore -map=/Game/Maps/Depot -stage -pak -archive "-archivedirectory=$raidArchive" -unattended -utf8output
if ($LASTEXITCODE -ne 0) { throw "R6 packaging failed with exit code $LASTEXITCODE" }
$raidLicenseDestination = Join-Path $raidArchive 'Windows\ThirdPartyLicenses'
New-Item -ItemType Directory -Path $raidLicenseDestination -Force | Out-Null
foreach ($raidLicenseName in @('LICENSE.NotoSansCJK.txt', 'COPYRIGHT.NotoSansCJK.txt')) {
    Copy-Item -LiteralPath (Join-Path $raidProjectRoot ('Downloads\Fonts\' + $raidLicenseName)) -Destination $raidLicenseDestination -Force
}
$raidAdditionalLicenses = @{
    'Downloads\Models\SteinGames\Extracted\WeaponsPack\license.txt' = 'SteinGames_ClassicWeapons_LICENSE.txt'
    'Downloads\Audio\KenneyImpact\Extracted\License.txt' = 'Kenney_ImpactSounds_LICENSE.txt'
}
foreach ($raidSource in $raidAdditionalLicenses.Keys) {
    Copy-Item -LiteralPath (Join-Path $raidProjectRoot $raidSource) -Destination (Join-Path $raidLicenseDestination $raidAdditionalLicenses[$raidSource]) -Force
}
foreach ($raidSourceDoc in @('ASSET_SOURCES_R2.md','ASSET_SOURCES_R3.md','ASSET_SOURCES_R4.md','ASSET_FONT_R4.md','ASSET_SOURCES_R5.md','ASSET_SOURCES_R6.md')) {
    Copy-Item -LiteralPath (Join-Path $raidProjectRoot $raidSourceDoc) -Destination $raidLicenseDestination -Force
}
if ($OutputName -eq 'Packaged_R6') {
    foreach ($raidGuide in @('README_R6.md','R6_CODE_GUIDE.md','R6_ENGINE_EXPERIMENT.md','TEST_RESULTS_R6.md')) {
        $raidGuidePath = Join-Path $raidProjectRoot $raidGuide
        if (Test-Path -LiteralPath $raidGuidePath) {
            $raidGuideText = [IO.File]::ReadAllText($raidGuidePath)
            $raidGuideText = [regex]::Replace($raidGuideText, '\]\(([^)]+)\)', {
                param($raidMatch)
                $raidTarget = $raidMatch.Groups[1].Value
                $raidPlainTarget = $raidTarget.Trim('<','>')
                if ($raidPlainTarget -match '^(#|/|[A-Za-z][A-Za-z0-9+.-]*:)') { return $raidMatch.Value }
                return '](../../' + $raidTarget + ')'
            })
            [IO.File]::WriteAllText((Join-Path $raidArchive ('Windows\' + $raidGuide)), $raidGuideText, [Text.UTF8Encoding]::new($false))
        }
    }
}
Write-Output "$OutputName packaging and attribution staging completed."
