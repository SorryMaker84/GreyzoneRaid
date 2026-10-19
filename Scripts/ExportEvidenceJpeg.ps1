param([Parameter(Mandatory=$true)][string]$Folder, [string[]]$Names = @())
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$resolvedFolder = (Resolve-Path -LiteralPath $Folder).Path
$encoder = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
$parameters = [System.Drawing.Imaging.EncoderParameters]::new(1)
$parameters.Param[0] = [System.Drawing.Imaging.EncoderParameter]::new([System.Drawing.Imaging.Encoder]::Quality, [long]78)
try {
    foreach ($file in (Get-ChildItem -LiteralPath $resolvedFolder -Filter '*.png')) {
        if ($Names.Count -gt 0 -and $file.BaseName -notin $Names) { continue }
        $destination = [System.IO.Path]::ChangeExtension($file.FullName, '.jpg')
        $sourceImage = [System.Drawing.Image]::FromFile($file.FullName)
        try { $sourceImage.Save($destination, $encoder, $parameters) }
        finally { $sourceImage.Dispose() }
        Get-Item -LiteralPath $destination | Select-Object FullName,Length,LastWriteTimeUtc
    }
} finally { $parameters.Dispose() }
