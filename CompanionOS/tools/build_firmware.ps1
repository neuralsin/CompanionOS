param(
    [string]$Fqbn = "esp32:esp32:esp32:PartitionScheme=huge_app",
    [string]$BuildPath = "$env:TEMP/companion-ui-build",
    [string]$ArduinoData = "$env:LOCALAPPDATA/Arduino15",
    [string]$Libraries = "$env:USERPROFILE/OneDrive/Documents/Arduino/libraries"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $root "arduino-cli-bin/arduino-cli.exe"
$sketch = Join-Path $root "arduino/CompanionOS_Main"
if (!(Test-Path -LiteralPath $cli)) { throw "Arduino CLI missing: $cli" }
$cores = Join-Path $ArduinoData "packages/esp32/hardware/esp32"
$core = Get-ChildItem -LiteralPath $cores -Directory |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$core) { throw "ESP32 Arduino core missing" }
$dependencies = @(
    (Join-Path $core.FullName "libraries/BLE"),
    (Join-Path $Libraries "RF24"),
    (Join-Path $Libraries "TJpg_Decoder")
)
$arguments = @("compile", "--fqbn", $Fqbn, "--build-path", $BuildPath)
foreach ($dependency in $dependencies) {
    if (!(Test-Path -LiteralPath $dependency)) { throw "Library missing: $dependency" }
    $arguments += @("--library", $dependency)
}
$arguments += $sketch
New-Item -ItemType Directory -Force -Path $BuildPath | Out-Null
# The CLI may clean its build directory when the sketch's file set changes.
# Redirect outside it so Windows does not lock a file the CLI needs to remove.
$stdout = $BuildPath + ".stdout.log"
$stderr = $BuildPath + ".stderr.log"
$quoted = ($arguments | ForEach-Object { '"' + $_ + '"' }) -join ' '
$process = Start-Process -FilePath $cli -ArgumentList $quoted -WindowStyle Hidden -Wait -PassThru `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
Get-Content -LiteralPath $stdout
Get-Content -LiteralPath $stderr
exit $process.ExitCode
