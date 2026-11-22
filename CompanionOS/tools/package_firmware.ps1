param(
    [string]$BuildPath = "$env:TEMP/companion-ui-build",
    [string]$Name = "CompanionOS-ESP32-LYRICS-UI-v3-private"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "arduino/CompanionOS_Main"
$output = Join-Path $root "OUTPUT/firmware"
$options = Get-Content -LiteralPath (Join-Path $BuildPath 'build.options.json') -Raw | ConvertFrom-Json
if ($options.fqbn -ne 'esp32:esp32:esp32:PartitionScheme=huge_app' -or
    $options.hardwareFolders -notmatch '[/\\]esp32[/\\]3\.3\.8(?:,|$)') {
    throw 'This package profile requires ESP32 core 3.3.8 and the huge_app partition'
}
$log = (Get-Content -LiteralPath ($BuildPath + '.stdout.log') -Raw) +
       (Get-Content -LiteralPath ($BuildPath + '.stderr.log') -Raw)
$flash = [regex]::Match($log, 'Sketch uses ([\d,]+) bytes')
$ram = [regex]::Match($log, 'Global variables use ([\d,]+) bytes')
if (!$flash.Success -or !$ram.Success) { throw "A successful build log is required" }

# Refuse stale artifacts, including mismatched local account seeds. Never log contents.
function Normalize-Header([string]$path) {
    $text = [IO.File]::ReadAllText($path).Replace("`r`n", "`n")
    return ([regex]::Replace($text, '(?m)^#line[^\n]*\n?', '')).TrimEnd("`r", "`n")
}
foreach ($header in Get-ChildItem -LiteralPath $source -Filter '*.h') {
    $copied = Join-Path $BuildPath ("sketch/" + $header.Name)
    if (!(Test-Path -LiteralPath $copied) -or
        (Normalize-Header $header.FullName) -cne (Normalize-Header $copied)) {
        throw "Build source mismatch: $($header.Name). Rebuild before packaging."
    }
}
$original = [IO.File]::ReadAllLines((Join-Path $source 'CompanionOS_Main.ino'))
$generated = [IO.File]::ReadAllLines((Join-Path $BuildPath 'sketch/CompanionOS_Main.ino.cpp'))
$mapped = @{}
$line = 0
$inSketch = $false
foreach ($text in $generated) {
    $directive = [regex]::Match($text, '^#line (\d+) "(.*)"$')
    if ($directive.Success) {
        $line = [int]$directive.Groups[1].Value
        $inSketch = $directive.Groups[2].Value.EndsWith('CompanionOS_Main.ino')
    } elseif ($inSketch) {
        $mapped[$line] = $text
        $line++
    }
}
for ($i = 0; $i -lt $original.Length; $i++) {
    if (!$mapped.ContainsKey($i + 1) -or $mapped[$i + 1] -cne $original[$i]) {
        throw "Generated sketch mismatch at source line $($i + 1). Rebuild before packaging."
    }
}

$esptool = Join-Path $env:LOCALAPPDATA 'Arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool.exe'
$validationOut = Join-Path $BuildPath 'image-validation.stdout.log'
$validationErr = Join-Path $BuildPath 'image-validation.stderr.log'
$app = Join-Path $BuildPath 'CompanionOS_Main.ino.bin'
$process = Start-Process -FilePath $esptool -ArgumentList "--chip esp32 image-info `"$app`"" `
    -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $validationOut -RedirectStandardError $validationErr
$validation = Get-Content -LiteralPath $validationOut -Raw
if ($process.ExitCode -ne 0 -or $validation -notmatch 'Checksum:.*valid' -or
    $validation -notmatch 'Validation Hash:.*valid') { throw "esptool image validation failed" }

New-Item -ItemType Directory -Force -Path $output | Out-Null
$names = @('CompanionOS_Main.ino.bin', 'CompanionOS_Main.ino.bootloader.bin',
    'CompanionOS_Main.ino.partitions.bin', 'CompanionOS_Main.ino.merged.bin', 'boot_app0.bin')
foreach ($file in $names + @('flash_args', 'partitions.csv')) {
    $inputFile = Join-Path $BuildPath $file
    if ($file -eq 'boot_app0.bin' -and !(Test-Path -LiteralPath $inputFile)) {
        $inputFile = Join-Path $env:LOCALAPPDATA 'Arduino15/packages/esp32/hardware/esp32/3.3.8/tools/partitions/boot_app0.bin'
    }
    Copy-Item -LiteralPath $inputFile -Destination (Join-Path $output $file) -Force
}
Copy-Item -LiteralPath (Join-Path $root 'OUTPUT/FLASHING.md') -Destination (Join-Path $output 'README.md') -Force
Copy-Item -LiteralPath (Join-Path $root 'OUTPUT/apple_design_audit_2026-10-18.md') -Destination (Join-Path $output 'UI-AUDIT.md') -Force
$images = @($names | ForEach-Object {
    $file = Get-Item -LiteralPath (Join-Path $output $_)
    [ordered]@{ file = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower() }
})
$manifest = [ordered]@{
    builtAtUtc = [DateTime]::UtcNow.ToString('o')
    revision = 'LYRICS-UI-v3'
    fqbn = 'esp32:esp32:esp32:PartitionScheme=huge_app'
    chip = 'ESP32'; flashBytes = 4194304; appPartitionBytes = 3145728
    arduinoCore = '3.3.8'; panel = 'ST7735 BLACKTAB, 160x128 landscape'
    buildFlashBytes = [int]$flash.Groups[1].Value.Replace(',', '')
    staticRamBytes = [int]$ram.Groups[1].Value.Replace(',', '')
    graphicsTlsReserveBytes = 81920; graphicsTlsLargestBlockBytes = 24576
    tlsHeapAdmissionGate = $false
    bluetoothMemory = 'Released in normal boot; explicit radio workspace restarts to reserve it'
    optionalServices = 'RuView allocated on tile entry, PC/game feeds use renewable view leases, weather on demand'
    lyrics = 'LRCLIB, bounded 24 KiB response / 16 KiB JSON parse / 8 KiB retained cache, page-scoped'
    spotifyUi = 'Silent routine polling, content-driven redraws, Apple Design skill audit'
    personalized = $true; physicalDeviceFlashed = $false; physicalTlsVerified = $false
    sourceSnapshotVerified = $true
    imageValidation = 'esptool 5.2.0: valid checksum and validation hash'
    images = $images
}
[IO.File]::WriteAllText((Join-Path $output 'manifest.json'), ($manifest | ConvertTo-Json -Depth 5))
$sums = @($images | ForEach-Object { $_.sha256 + '  ' + $_.file })
[IO.File]::WriteAllLines((Join-Path $output 'SHA256SUMS.txt'), $sums)
$members = @($names + @('flash_args', 'partitions.csv', 'README.md', 'UI-AUDIT.md', 'manifest.json', 'SHA256SUMS.txt') |
    ForEach-Object { Join-Path $output $_ })
$archive = Join-Path $output ($Name + '.zip')
Compress-Archive -LiteralPath $members -DestinationPath $archive -CompressionLevel Optimal -Force

Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    if ($zip.Entries.Count -ne $members.Count) { throw 'Unexpected archive members' }
    foreach ($entry in $images) {
        $zipped = $zip.GetEntry($entry.file)
        if (!$zipped -or $zipped.Length -ne $entry.bytes) { throw 'Archive image size mismatch' }
        $stream = $zipped.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '').ToLower() }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ($hash -ne $entry.sha256) { throw 'Archive image hash mismatch' }
    }
} finally { $zip.Dispose() }
Write-Output "Private firmware package verified: $archive"
Write-Output "Flash used: $($manifest.buildFlashBytes); static RAM: $($manifest.staticRamBytes) bytes"
