# Generate a preview PNG from an OpenSCAD file (Windows)
# Usage: .\preview.ps1 input.scad output.png [options]
#
# Options:
#   -Camera   "x,y,z,rx,ry,rz,dist"   Camera position
#   -Size     "WxH"                    Image size (default: 800x600)
#   -Defines  @("var=value", ...)      Parameter overrides

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$InputFile,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$OutputFile,

    [string]$Camera = "0,0,0,55,0,25,0",
    [string]$Size   = "800x600",
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

# Convert WxH -> W,H
$imgSize = $Size -replace 'x', ','

# Build -D args
$defineArgs = @()
foreach ($def in $Defines) {
    $defineArgs += "-D"; $defineArgs += $def
}

Write-Host "Rendering preview: $InputFile -> $OutputFile"

& $openscad `
    --camera=$Camera `
    --imgsize=$imgSize `
    --colorscheme="Tomorrow Night" `
    --autocenter `
    --viewall `
    @defineArgs `
    -o $OutputFile `
    $InputFile
$success = $?

if ($success) {
    Write-Host "Preview saved to: $OutputFile" -ForegroundColor Green
} else {
    Write-Error "Rendering failed."
    exit 1
}
