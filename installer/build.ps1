# Builds the Windows installer for SMP.
#
#   1. builds the Release binary with CMake,
#   2. stages the files the installer needs,
#   3. runs Inno Setup and writes dist\SMP-Setup-<version>.exe.
#
# Requires Inno Setup 6 (ISCC.exe). Install it with:
#   winget install --id JRSoftware.InnoSetup -e
[CmdletBinding()]
param(
    [string]$BuildDir = '.build',
    [string]$Iscc
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$installerDir = $PSScriptRoot

if (-not $Iscc) {
    $candidates = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe'
    )
    $Iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) {
    throw 'ISCC.exe not found. Install Inno Setup 6: winget install --id JRSoftware.InnoSetup -e'
}

Push-Location $root
try {
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw 'the build failed' }

    $stage = Join-Path $installerDir 'stage'
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $stage | Out-Null

    Copy-Item (Join-Path $root "$BuildDir\smp.exe") $stage
    Copy-Item (Join-Path $root 'README.md') $stage
    Copy-Item (Join-Path $root 'LICENSE') $stage

    & $Iscc (Join-Path $installerDir 'smp.iss')
    if ($LASTEXITCODE -ne 0) { throw 'ISCC failed' }
}
finally {
    Pop-Location
}

$setup = Get-ChildItem (Join-Path $root 'dist') -Filter 'SMP-Setup-*.exe' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
Write-Host ''
Write-Host "installer: $($setup.FullName) ($([math]::Round($setup.Length / 1MB, 2)) MB)"
