# Export OpenSCAD file to STL (Windows)
# Usage: .\export-stl.ps1 input.scad output.stl [-Defines @("var=value", ...)]

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$InputFile,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$OutputFile,

    [string[]]$Defines = @()
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$ScriptDir\common.ps1"

$openscad = Get-OpenSCAD

if (-not (Test-Path $InputFile)) {
    Write-Error "Error: File not found: $InputFile"
    exit 1
}

# Ensure output directory exists
$outDir = Split-Path -Parent $OutputFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

# Build -D args
$defineArgs = @()
foreach ($def in $Defines) {
    $defineArgs += "-D"; $defineArgs += $def
}

Write-Host "Exporting STL: $InputFile -> $OutputFile"
if ($Defines.Count -gt 0) {
    Write-Host "Parameters: $($Defines -join ', ')"
}

& $openscad @defineArgs -o $OutputFile $InputFile
$success = $?

if ($success -and (Test-Path $OutputFile)) {
    $sizeKB = [math]::Round((Get-Item $OutputFile).Length / 1KB, 1)
    Write-Host "STL exported: $OutputFile ($sizeKB KB)" -ForegroundColor Green
} elseif ($success -and -not (Test-Path $OutputFile)) {
    Write-Error "Export appeared to succeed but output file not found: $OutputFile"
    exit 1
} else {
    Write-Error "STL export failed."
    exit 1
}
