# Generate preview images from multiple angles (Windows)
# Usage: .\multi-preview.ps1 input.scad output_dir\ [-Defines @("var=value", ...)]

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$InputFile,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$OutputDir,

    [string[]]$Defines = @()
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$ScriptDir\common.ps1"

$openscad = Get-OpenSCAD

if (-not (Test-Path $InputFile)) {
    Write-Error "Error: File not found: $InputFile"
    exit 1
}

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$baseName = [System.IO.Path]::GetFileNameWithoutExtension($InputFile)

# Build -D args
$defineArgs = @()
foreach ($def in $Defines) {
    $defineArgs += "-D"; $defineArgs += $def
}

Write-Host "Generating multi-angle previews for: $InputFile"
Write-Host "Output directory: $OutputDir"
Write-Host ""

# Angles: name -> camera string
$angles = [ordered]@{
    iso   = "0,0,0,55,0,25,0"
    front = "0,0,0,90,0,0,0"
    back  = "0,0,0,90,0,180,0"
    left  = "0,0,0,90,0,90,0"
    right = "0,0,0,90,0,-90,0"
    top   = "0,0,0,0,0,0,0"
}

foreach ($angle in $angles.GetEnumerator()) {
    $outFile = Join-Path $OutputDir "${baseName}_$($angle.Key).png"
    Write-Host "  Rendering $($angle.Key) view..."

    & $openscad `
        --camera=$($angle.Value) `
        --imgsize=800,600 `
        --colorscheme="Tomorrow Night" `
        --autocenter `
        --viewall `
        @defineArgs `
        -o $outFile `
        $InputFile 2>$null
    $angleOk = $?

    if ($angleOk) {
        Write-Host "    -> $outFile" -ForegroundColor Green
    } else {
        Write-Host "    [FAIL] $($angle.Key)" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Generated previews:"
Get-ChildItem (Join-Path $OutputDir "${baseName}_*.png") | Select-Object Name, Length
