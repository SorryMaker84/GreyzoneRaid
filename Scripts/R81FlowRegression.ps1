param([Parameter(Mandatory=$true)][string]$GameExecutable,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$out=[IO.Path]::GetFullPath($OutputDirectory)
$fixed=[IO.Path]::GetFullPath((Join-Path $root 'Evidence\R6Flow'))
$old=[IO.Path]::GetFullPath((Join-Path $out 'Original_R6Flow'))
$fresh=[IO.Path]::GetFullPath((Join-Path $out 'Flow'))
foreach($p in @($out,$fixed,$old,$fresh)){if(-not $p.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Path outside project'}}
if(Test-Path -LiteralPath $out){throw 'Use a fresh evidence directory'}
New-Item -ItemType Directory -Path $out|Out-Null
$had=Test-Path -LiteralPath $fixed
$receipt=@{status='RUNNING';started_utc=[DateTime]::UtcNow.ToString('o');scope='Retained menu/container/manual-transfer/death/restart flow only; does not rerun the movement SelfTest or full map routes.'}
if($had){Move-Item -LiteralPath $fixed -Destination $old}
try{
 & (Join-Path $PSScriptRoot 'FlowTest.ps1') -GameExecutable $GameExecutable
 $receipt.status='PASS'
}catch{$receipt.status='FAIL';$receipt.error=$_.Exception.Message;throw}
finally{
 if(Test-Path -LiteralPath $fixed){Move-Item -LiteralPath $fixed -Destination $fresh}
 if($had){Move-Item -LiteralPath $old -Destination $fixed}
 $receipt.finished_utc=[DateTime]::UtcNow.ToString('o')
 $receipt|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'R81FlowReceipt.json') -Encoding utf8
}
