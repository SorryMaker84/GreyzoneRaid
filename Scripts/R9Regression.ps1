param([string]$GameExecutable='', [ValidateSet('Framework','SelfTest','Flow','Container')][string[]]$Suites=@('Framework','SelfTest','Flow','Container'), [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
function LocalPath([string]$path){$resolved=[IO.Path]::GetFullPath($path);if(!$resolved.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){throw "Outside project: $resolved"};return $resolved}
$out=LocalPath $OutputDirectory
if(Test-Path -LiteralPath $out){throw 'Use a fresh output folder'}
New-Item -ItemType Directory -Path $out | Out-Null
$runtime=if($GameExecutable){'Packaged'}else{'Editor'}
$specs=@{Framework=@{script='FrameworkTest.ps1';folder="R6Framework_$runtime"};SelfTest=@{script='Test.ps1';folder='R6SelfTest'};Flow=@{script='FlowTest.ps1';folder='R6Flow'};Container=@{script='ContainerTest.ps1';folder='R6Container'}}
$receipt=[ordered]@{status='RUNNING';runtime=$runtime;started_utc=[DateTime]::UtcNow.ToString('o');suites=@()}
try{
 foreach($name in $Suites){
  $spec=$specs[$name];$fixed=LocalPath (Join-Path $root ('Evidence\'+$spec.folder));$saved=LocalPath (Join-Path $out ('Original_'+$name));$fresh=LocalPath (Join-Path $out $name)
  $had=Test-Path -LiteralPath $fixed
  if($had){Move-Item -LiteralPath $fixed -Destination $saved}
  $item=[ordered]@{name=$name;status='RUNNING';started_utc=[DateTime]::UtcNow.ToString('o')}
  try{& (Join-Path $PSScriptRoot $spec.script) -GameExecutable $GameExecutable;$item.status='PASS'}
  catch{$item.status='FAIL';$item.error=$_.Exception.Message;throw}
  finally{
   if(Test-Path -LiteralPath $fixed){Move-Item -LiteralPath $fixed -Destination $fresh}
   if($had){Move-Item -LiteralPath $saved -Destination $fixed}
   $item.finished_utc=[DateTime]::UtcNow.ToString('o');$receipt.suites+=$item
  }
 }
 $receipt.status='PASS'
}catch{$receipt.status='FAIL';$receipt.error=$_.Exception.Message;throw}
finally{$receipt.finished_utc=[DateTime]::UtcNow.ToString('o');$receipt|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $out 'Receipt.json') -Encoding utf8}
