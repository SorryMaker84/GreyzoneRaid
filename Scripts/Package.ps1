param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
# The default packaging entry point advances to R7 and preserves Packaged_R6.
& (Join-Path $PSScriptRoot 'Package_R7.ps1') -EngineRoot $EngineRoot -OutputName Packaged_R7
