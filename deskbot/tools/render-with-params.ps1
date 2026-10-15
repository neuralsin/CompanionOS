# Render OpenSCAD with parameters from a JSON file (Windows)
# Usage: .\render-with-params.ps1 input.scad params.json output.[stl|png]
#
# params.json format: {"width": 60, "height": 40, "include_lid": true}

param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$InputFile,

    [Parameter(Mandatory=$true, Position=1)]
    [string]$ParamsFile,

    [Parameter(Mandatory=$true, Position=2)]
    [string]$OutputFile
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$ScriptDir\common.ps1"

$openscad = Get-OpenSCAD

if (-not (Test-Path $InputFile)) {
    Write-Error "Error: Input file not found: $InputFile"
    exit 1
}

if (-not (Test-Path $ParamsFile)) {
    Write-Error "Error: Params file not found: $ParamsFile"
    exit 1
}

# Parse JSON params -> -D args using Python
$pythonScript = @"
import json, sys
with open(sys.argv[1]) as f:
    params = json.load(f)
for k, v in params.items():
    if isinstance(v, bool):
        print(f'{k}={str(v).lower()}')
    elif isinstance(v, str):
        print(f'{k}=\"{v}\"')
    else:
        print(f'{k}={v}')
"@

$tmpScript = [System.IO.Path]::GetTempFileName() + ".py"
try {
    Set-Content -Path $tmpScript -Value $pythonScript -Encoding UTF8
    $paramLines = python $tmpScript $ParamsFile
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to parse JSON params (is Python installed?)."
        exit 1
    }
} finally {
    if (Test-Path $tmpScript) { Remove-Item $tmpScript -ErrorAction SilentlyContinue }
}

$defineArgs = @()
foreach ($line in $paramLines) {
    $line = $line.Trim()
    if ($line) {
        $defineArgs += "-D"; $defineArgs += $line
    }
}

Write-Host "Rendering with parameters from: $ParamsFile"
Write-Host "Parameters: $($paramLines -join ', ')"

# Ensure output dir exists
$outDir = Split-Path -Parent $OutputFile
if ($outDir -and -not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Path $outDir -Force | Out-Null
}

$ext = [System.IO.Path]::GetExtension($OutputFile).TrimStart('.').ToLower()

switch ($ext) {
    "stl" {
        & $openscad @defineArgs -o $OutputFile $InputFile
    }
    "png" {
        & $openscad @defineArgs `
            --camera="0,0,0,55,0,25,0" `
            --imgsize="800,600" `
            --colorscheme="Tomorrow Night" `
            --autocenter --viewall `
            -o $OutputFile $InputFile
    }
    default {
        Write-Error "Unsupported output format: $ext. Supported: stl, png"
        exit 1
    }
}
$renderOk = $?

if ($renderOk) {
    Write-Host "Output saved: $OutputFile" -ForegroundColor Green
} else {
    Write-Error "Render failed."
    exit 1
}
