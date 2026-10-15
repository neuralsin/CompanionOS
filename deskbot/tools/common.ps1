# Common utilities for OpenSCAD tools (Windows PowerShell)

function Find-OpenSCAD {
    # Check if openscad is in PATH
    $inPath = Get-Command openscad -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    # Common Windows install locations
    $candidates = @(
        "D:\openscad\openscad.exe",                          # Local install
        "$env:ProgramFiles\OpenSCAD\openscad.exe",
        "${env:ProgramFiles(x86)}\OpenSCAD\openscad.exe",
        "$env:LocalAppData\Programs\OpenSCAD\openscad.exe",
        "C:\OpenSCAD\openscad.exe"
    )

    foreach ($path in $candidates) {
        if (Test-Path $path) { return $path }
    }

    return $null
}

function Get-OpenSCAD {
    $exe = Find-OpenSCAD
    if (-not $exe) {
        Write-Error "Error: OpenSCAD not found!`n`nInstall OpenSCAD using:`n  winget install --id OpenSCAD.OpenSCAD`n  Or download from https://openscad.org/downloads.html"
        exit 1
    }
    return $exe
}

function Get-OpenSCADVersion {
    $exe = Get-OpenSCAD
    & $exe --version 2>&1
}
