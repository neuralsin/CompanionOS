# Validate an OpenSCAD file for syntax errors (Windows)
# Usage: .\validate.ps1 input.scad

param(
    [Parameter(Mandatory=$true)]
    [string]$InputFile
)

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$ScriptDir\common.ps1"

$openscad = Get-OpenSCAD

if (-not (Test-Path $InputFile)) {
    Write-Error "Error: File not found: $InputFile"
    exit 1
}

Write-Host "Validating: $InputFile"

# Create a temp echo file
$TempOutput = [System.IO.Path]::GetTempFileName() + ".echo"

try {
    # Run OpenSCAD with echo output (fastest way to check syntax)
    # Using --export-format=echo just parses and evaluates without rendering
    & $openscad -o $TempOutput --export-format=echo $InputFile
    $success = $?

    if ($success) {
        Write-Host "[OK] Syntax OK" -ForegroundColor Green
        if ((Test-Path $TempOutput) -and (Get-Item $TempOutput).Length -gt 0) {
            Write-Host ""
            Write-Host "Echo output:"
            Get-Content $TempOutput
        }
        exit 0
    } else {
        Write-Host "[FAIL] Validation failed" -ForegroundColor Red
        exit 1
    }
} finally {
    if (Test-Path $TempOutput) { Remove-Item $TempOutput -ErrorAction SilentlyContinue }
}
