# Renders installer/smp.ico: an equaliser mark in the project palette.
# Multi-size ICO (16/32/48/64 as BMP, 256 as PNG) so every Windows surface
# has a crisp variant.
[CmdletBinding()]
param(
    [string]$OutputPath
)

if (-not $OutputPath) {
    $scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $OutputPath = Join-Path $scriptRoot 'smp.ico'
}

Add-Type -AssemblyName System.Drawing

function New-IconBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.Clear([System.Drawing.Color]::FromArgb(255, 16, 16, 22))

    $bars = 7
    $gap = [Math]::Max(1, [int]($size * 0.04))
    $barWidth = [Math]::Max(2, [int](($size * 0.72 - ($bars - 1) * $gap) / $bars))
    $totalWidth = $bars * $barWidth + ($bars - 1) * $gap
    $startX = [int](($size - $totalWidth) / 2)
    $heights = @(0.22, 0.40, 0.55, 0.70, 0.52, 0.36, 0.26)

    $white = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 245, 245, 245))
    for ($i = 0; $i -lt $bars; $i++) {
        $h = [int]($size * $heights[$i])
        $x = $startX + $i * ($barWidth + $gap)
        $y = $size - [int]($size * 0.16) - $h
        $g.FillRectangle($white, $x, $y, $barWidth, $h)
    }
    $white.Dispose()

    $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 90, 90, 105)), ([Math]::Max(1, $size / 64))
    $inset = [int]($size * 0.03)
    $g.DrawRectangle($pen, $inset, $inset, $size - 2 * $inset - 1, $size - 2 * $inset - 1)
    $pen.Dispose()
    $g.Dispose()
    return $bmp
}

function Get-BmpEntry([System.Drawing.Bitmap]$bmp) {
    $w = $bmp.Width
    $h = $bmp.Height
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter $ms

    # BITMAPINFOHEADER
    $bw.Write([UInt32]40)
    $bw.Write([Int32]$w)
    $bw.Write([Int32]($h * 2))
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]0)
    $bw.Write([UInt32]($w * $h * 4))
    $bw.Write([Int32]0)
    $bw.Write([Int32]0)
    $bw.Write([UInt32]0)
    $bw.Write([UInt32]0)

    for ($y = $h - 1; $y -ge 0; $y--) {
        for ($x = 0; $x -lt $w; $x++) {
            $c = $bmp.GetPixel($x, $y)
            $bw.Write([Byte]$c.B)
            $bw.Write([Byte]$c.G)
            $bw.Write([Byte]$c.R)
            $bw.Write([Byte]$c.A)
        }
    }

    # AND mask: one bit per pixel, rows padded to four bytes, all opaque.
    $maskRow = [Math]::Ceiling($w / 32.0) * 4
    $mask = New-Object byte[] ($maskRow * $h)
    $bw.Write($mask)

    $bw.Flush()
    $data = $ms.ToArray()
    $bw.Dispose()
    $ms.Dispose()
    return , $data
}

function Get-PngEntry([System.Drawing.Bitmap]$bmp) {
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $data = $ms.ToArray()
    $ms.Dispose()
    return , $data
}

$sizes = @(16, 32, 48, 64, 256)
$entries = @()
foreach ($size in $sizes) {
    $bmp = New-IconBitmap $size
    $data = if ($size -ge 256) { Get-PngEntry $bmp } else { Get-BmpEntry $bmp }
    $entries += [pscustomobject]@{ Size = $size; Data = $data }
    $bmp.Dispose()
}

$dir = Split-Path -Parent $OutputPath
if ($dir -and -not (Test-Path $dir)) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
}

$fs = [System.IO.File]::Create($OutputPath)
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0)
$bw.Write([UInt16]1)
$bw.Write([UInt16]$entries.Count)
$offset = 6 + 16 * $entries.Count
foreach ($entry in $entries) {
    $dim = if ($entry.Size -ge 256) { 0 } else { $entry.Size }
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]$dim)
    $bw.Write([Byte]0)
    $bw.Write([Byte]0)
    $bw.Write([UInt16]1)
    $bw.Write([UInt16]32)
    $bw.Write([UInt32]$entry.Data.Length)
    $bw.Write([UInt32]$offset)
    $offset += $entry.Data.Length
}
foreach ($entry in $entries) {
    $bw.Write($entry.Data)
}
$bw.Flush()
$bw.Dispose()
$fs.Dispose()

Write-Host "wrote $OutputPath ($((Get-Item $OutputPath).Length) bytes, $($entries.Count) sizes)"
