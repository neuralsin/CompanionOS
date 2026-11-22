param(
    [string]$Compiler = "g++",
    [string]$OutputDir = "$env:TEMP/companion-eyes-thought-host"
)
$ErrorActionPreference = "Stop"
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = "$(Split-Path -Parent $compilerPath);$oldPath"
$failed = $false
try {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    foreach ($resolution in @(@(160, 128), @(320, 240))) {
        $w, $h = $resolution
        foreach ($platform in @("esp8266", "esp32")) {
            $name = "eyes-thought-{0}x{1}-{2}" -f $w, $h, $platform
            $exe = Join-Path $OutputDir "$name.exe"
            $log = Join-Path $OutputDir "$name.build.log"
            $stderr = Join-Path $OutputDir "$name.build.stderr.log"
            $arguments = @("-std=c++11", "-O1", "-g", "-Wall", "-Wextra",
                "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
                "-UNDEBUG", "-DSCREEN_W=$w", "-DSCREEN_H=$h")
            if ($platform -eq "esp32") { $arguments += "-DESP32" }
            $arguments += @("$PSScriptRoot/eyes_thought_host_regression.cpp", "-o", $exe)
            Write-Host "Building $name"
            $quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
            $proc = Start-Process -FilePath $compilerPath -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $log -RedirectStandardError $stderr
            Get-Content -LiteralPath $log
            Get-Content -LiteralPath $stderr
            if ($proc.ExitCode -ne 0) {
                Write-Host "Compile failed: $($proc.ExitCode)"
                $failed = $true
                continue
            }
            & $exe
            if ($LASTEXITCODE -ne 0) { $failed = $true }
        }
    }
} finally {
    $env:PATH = $oldPath
}
if ($failed) { exit 1 }
exit 0
