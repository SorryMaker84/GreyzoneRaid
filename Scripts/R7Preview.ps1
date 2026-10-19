param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [string]$GameExecutable = '',
    [int]$Width = 1600,
    [int]$Height = 900,
    [string]$OutputDirectory = '',
    [string]$Manifest = ''
)
$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'R7Test.ps1') -Mode Preview -EngineRoot $EngineRoot -GameExecutable $GameExecutable -Width $Width -Height $Height -OutputDirectory $OutputDirectory -Manifest $Manifest -TimeoutSeconds 240
