param(
    [string]$Compiler = "C:/mingw/bin/g++.exe",
    [string]$TftEsPi = "$env:USERPROFILE/OneDrive/Documents/Arduino/libraries/TFT_eSPI",
    [string]$Filter = "",
    [switch]$Render,
    [string]$OutputDir = "$PSScriptRoot/host-output"
)
$ErrorActionPreference = "Stop"
if (!(Test-Path -LiteralPath $Compiler)) { throw "Compiler not found: $Compiler" }
foreach ($font in @("glcdfont.c", "Font16.h", "Font16.c", "Font32rle.h", "Font32rle.c")) {
    if (!(Test-Path -LiteralPath "$TftEsPi/Fonts/$font")) { throw "Installed font missing: $TftEsPi/Fonts/$font" }
}
# MinGW's compiler subprocesses and executables need its DLL directory on PATH.
$oldPath = $env:PATH
$env:PATH = "$(Split-Path -Parent $Compiler);$oldPath"
$failed = $false
try {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    foreach ($resolution in @(@(160,128), @(320,240))) {
        $w, $h = $resolution
        $exe = Join-Path $OutputDir "ui-host-${w}x${h}.exe"
        $log = Join-Path $OutputDir "build-${w}x${h}.log"
        $arguments = @("-std=c++14", "-O1", "-g", "-Wall", "-Wextra", "-Wno-unused-function",
            "-DSCREEN_W=$w", "-DSCREEN_H=$h", "-I$TftEsPi",
            "-I$env:USERPROFILE/OneDrive/Documents/Arduino/libraries/ArduinoJson/src",
            "$PSScriptRoot/ui_host_regression.cpp", "-o", $exe)
        Write-Host "Building ${w}x${h} with $Compiler"
        # Start-Process preserves native diagnostics without PowerShell treating
        # compiler stderr as terminating NativeCommandError records.
        $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
        $stderr = Join-Path $OutputDir "build-${w}x${h}.stderr.log"
        $proc = Start-Process -FilePath $Compiler -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError $stderr
        Get-Content -LiteralPath $log
        Get-Content -LiteralPath $stderr
        if ($proc.ExitCode -ne 0) { Write-Host "Compile failed: $($proc.ExitCode)"; $failed = $true; continue }
        $runArgs = @()
        if ($Filter) { $runArgs += @("--filter", $Filter) }
        if ($Render) {
            $imageDir = Join-Path $OutputDir "${w}x${h}"
            New-Item -ItemType Directory -Force -Path $imageDir | Out-Null
            $runArgs += @("--render", $imageDir)
        }
        & $exe @runArgs
        if ($LASTEXITCODE -ne 0) { $failed = $true }
    }
} finally { $env:PATH = $oldPath }
if ($failed) { exit 1 }
exit 0
