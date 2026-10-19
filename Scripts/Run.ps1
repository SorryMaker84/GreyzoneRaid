param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$SelfTest)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$arguments = @(('"' + (Join-Path $projectRoot 'GreyzoneRaid.uproject') + '"'), '-game', '-windowed', '-ResX=1600', '-ResY=900', '-NoSplash', '-unattended', ('"-abslog=' + (Join-Path $projectRoot 'Evidence\GameRun.log') + '"'))
if ($SelfTest) {
    # The legacy self-test validates Depot; R7's default map is Frontier.
    $arguments = @($arguments[0], '/Game/Maps/Depot') + $arguments[1..($arguments.Count - 1)]
    $arguments += @('-RaidSelfTest', '-nosound')
}
$gameWindowStyle = if ($SelfTest) { 'Hidden' } else { 'Normal' }
Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle $gameWindowStyle
