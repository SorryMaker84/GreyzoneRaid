param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'GreyzoneRaid.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path -LiteralPath $buildTool)) { throw "UE build tool not found: $buildTool" }
& $buildTool GreyzoneRaidEditor Win64 Development "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE "-Log=$projectRoot\Evidence\EditorBuild.log"
if ($LASTEXITCODE -ne 0) { throw "UE build failed with exit code $LASTEXITCODE" }
