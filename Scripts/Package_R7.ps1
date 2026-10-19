param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Packaged_R7')][string]$OutputName = 'Packaged_R7'
)
$ErrorActionPreference = 'Stop'
$raidProjectRoot = Split-Path -Parent $PSScriptRoot
$raidUat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$raidArchive = Join-Path $raidProjectRoot $OutputName
$raidMetadata = Join-Path $raidProjectRoot 'Content\Validation\FrontierValidation.json'

# Fail before invoking UAT if the new map or its packaged validation input is absent.
foreach ($raidRequiredPath in @(
    $raidUat,
    (Join-Path $raidProjectRoot 'GreyzoneRaid.uproject'),
    (Join-Path $raidProjectRoot 'Content\Maps\Frontier.umap'),
    (Join-Path $raidProjectRoot 'Content\Maps\Depot.umap'),
    $raidMetadata
)) {
    if (-not (Test-Path -LiteralPath $raidRequiredPath -PathType Leaf)) {
        throw "R7 packaging input is missing: $raidRequiredPath"
    }
}
$null = [IO.File]::ReadAllText($raidMetadata) | ConvertFrom-Json -ErrorAction Stop
$raidMetadataHash = (Get-FileHash -LiteralPath $raidMetadata -Algorithm SHA256).Hash.ToLowerInvariant()

# Use a distinct archive and cook both the R7 map and the preserved Depot regression map.
# Validation/*.json is included through DirectoriesToAlwaysStageAsUFS in DefaultGame.ini.
& $raidUat BuildCookRun "-project=$raidProjectRoot\GreyzoneRaid.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -AdditionalCookerOptions=-SkipZenStore '-map=/Game/Maps/Frontier+/Game/Maps/Depot' -stage -pak -archive "-archivedirectory=$raidArchive" -unattended -utf8output
$raidUatExitCode = $LASTEXITCODE
if ($raidUatExitCode -ne 0) { throw "R7 packaging failed with exit code $raidUatExitCode" }

$raidWindows = Join-Path $raidArchive 'Windows'
$raidLicenseDestination = Join-Path $raidWindows 'ThirdPartyLicenses'
New-Item -ItemType Directory -Path $raidLicenseDestination -Force | Out-Null
$raidCopied = [System.Collections.Generic.List[object]]::new()
$raidSkipped = [System.Collections.Generic.List[string]]::new()

function Copy-R7DeliveryFile {
    param([string]$RelativeSource, [string]$Destination, [string]$Kind)
    $raidSourcePath = Join-Path $raidProjectRoot $RelativeSource
    if (-not (Test-Path -LiteralPath $raidSourcePath -PathType Leaf)) {
        $raidSkipped.Add($RelativeSource)
        Write-Output "R7 delivery document absent; skipped: $RelativeSource"
        return
    }
    Copy-Item -LiteralPath $raidSourcePath -Destination $Destination -Force
    $raidSourceHash = (Get-FileHash -LiteralPath $raidSourcePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $raidDestinationHash = (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($raidSourceHash -ne $raidDestinationHash) {
        throw "R7 delivery copy verification failed: $RelativeSource"
    }
    $raidCopied.Add([ordered]@{
        source = $RelativeSource.Replace('\', '/')
        destination = $Destination.Substring($raidWindows.Length + 1).Replace('\', '/')
        kind = $Kind
        sha256 = $raidDestinationHash
    })
}

$raidLicenseSources = [ordered]@{
    'Downloads\Fonts\LICENSE.NotoSansCJK.txt' = 'LICENSE.NotoSansCJK.txt'
    'Downloads\Fonts\COPYRIGHT.NotoSansCJK.txt' = 'COPYRIGHT.NotoSansCJK.txt'
    'Downloads\Models\SteinGames\Extracted\WeaponsPack\license.txt' = 'SteinGames_ClassicWeapons_LICENSE.txt'
    'Downloads\Audio\KenneyImpact\Extracted\License.txt' = 'Kenney_ImpactSounds_LICENSE.txt'
}
foreach ($raidSource in $raidLicenseSources.Keys) {
    Copy-R7DeliveryFile -RelativeSource $raidSource -Destination (Join-Path $raidLicenseDestination $raidLicenseSources[$raidSource]) -Kind 'license'
}
foreach ($raidAttribution in @(
    'ASSET_SOURCES.md', 'ASSET_SOURCES_R2.md', 'ASSET_SOURCES_R3.md',
    'ASSET_SOURCES_R4.md', 'ASSET_FONT_R4.md', 'ASSET_SOURCES_R5.md',
    'ASSET_SOURCES_R6.md', 'ASSET_SOURCES_R7.md'
)) {
    Copy-R7DeliveryFile -RelativeSource $raidAttribution -Destination (Join-Path $raidLicenseDestination $raidAttribution) -Kind 'attribution'
}
foreach ($raidGuide in @(
    'README_R7.md', 'R7_CODE_GUIDE.md', 'R7_BEGINNER_GUIDE.md',
    'R7_LEVEL_DESIGN.md', 'TEST_RESULTS_R7.md', 'R7_HANDS_NOTES.md'
)) {
    Copy-R7DeliveryFile -RelativeSource $raidGuide -Destination (Join-Path $raidWindows $raidGuide) -Kind 'guide'
}

$raidReceipt = [ordered]@{
    schema_version = 1
    created_utc = [DateTime]::UtcNow.ToString('o')
    archive_directory = $OutputName
    uat_exit_code = $raidUatExitCode
    requested_maps = @('/Game/Maps/Frontier', '/Game/Maps/Depot')
    validation_content_path = 'Validation/FrontierValidation.json'
    validation_source_sha256 = $raidMetadataHash
    validation_staging = 'DirectoriesToAlwaysStageAsUFS'
    copied_files = @($raidCopied.ToArray())
    skipped_missing_files = @($raidSkipped.ToArray())
    qualification = 'UAT completed successfully; packaged runtime verification is performed separately. Only copied_files were copied and hash-checked.'
}
$raidReceiptPath = Join-Path $raidWindows 'R7_PackagingReceipt.json'
[IO.File]::WriteAllText($raidReceiptPath, ($raidReceipt | ConvertTo-Json -Depth 6), [Text.UTF8Encoding]::new($false))
Write-Output "R7 UAT exited 0; copied and verified $($raidCopied.Count) delivery files; skipped $($raidSkipped.Count) absent files. Receipt: $raidReceiptPath"
