param([string]$Compiler="C:/mingw/bin/g++.exe")
$ErrorActionPreference="Stop"
$oldPath=$env:PATH
try {
    $env:PATH="$(Split-Path -Parent $Compiler);$oldPath"
    $output=Join-Path $PSScriptRoot "host-output"
    New-Item -ItemType Directory -Force -Path $output | Out-Null
    $exe=Join-Path $output "boot-health.exe"
    & $Compiler -std=c++11 -Wall -Wextra -Werror "-I$PSScriptRoot/boot_host" "$PSScriptRoot/boot_health_regression.cpp" -o $exe
    if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $exe
    if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally { $env:PATH=$oldPath }
