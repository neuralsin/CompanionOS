param(
    [string]$Compiler = "g++",
    [string]$OutputDir = "$env:TEMP/companion-eyes-thought-context"
)
$ErrorActionPreference = "Stop"
$compilerPath = (Get-Command $Compiler -ErrorAction Stop).Source
$oldPath = $env:PATH
$env:PATH = "$(Split-Path -Parent $compilerPath);$oldPath"
$failed = $false
try {
    New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
    # Compile selected .ino bodies verbatim, not a second copy of their logic.
    $inoPath = (Resolve-Path "$PSScriptRoot/../arduino/CompanionOS_Main/CompanionOS_Main.ino").Path
    $source = [IO.File]::ReadAllText($inoPath)
    $init = [regex]::Match($source, "(?ms)^void initThoughtScheduler\(\)\s*\{.*?^\}")
    $ambient = [regex]::Match($source, "(?ms)^  // Apply ambient time/light context to all themes, preserving manual choices\.\r?\n(?<body>.*?)(?=^\})")
    if (!$init.Success -or !$ambient.Success) { throw "Parent personality source anchors changed; update the test extractor." }
    $initLine = 1 + [regex]::Matches($source.Substring(0, $init.Index), "\n").Count
    $body = $ambient.Groups["body"]
    $ambientLine = 1 + [regex]::Matches($source.Substring(0, $body.Index), "\n").Count
    $escapedPath = $inoPath.Replace("\", "/")
    $generated = "#line $initLine ""$escapedPath""`n" + $init.Value + "`nvoid tickAmbientParent() {`n#line $ambientLine ""$escapedPath""`n" + $body.Value + "`n}`n"
    [IO.File]::WriteAllText((Join-Path $OutputDir "parent_personality_under_test.h"), $generated)
    foreach ($resolution in @(@(160, 128), @(320, 240))) {
        $w, $h = $resolution
        foreach ($platform in @("esp8266", "esp32")) {
            $name = "eyes-thought-context-{0}x{1}-{2}" -f $w, $h, $platform
            $exe = Join-Path $OutputDir "$name.exe"
            $log = Join-Path $OutputDir "$name.build.log"
            $stderr = Join-Path $OutputDir "$name.build.stderr.log"
            $arguments = @("-std=c++11", "-O1", "-g", "-Wall", "-Wextra",
                "-Wno-unused-function", "-Wno-unused-variable", "-Wno-sign-compare",
                "-UNDEBUG", "-DSCREEN_W=$w", "-DSCREEN_H=$h", "-I$OutputDir")
            if ($platform -eq "esp32") { $arguments += "-DESP32" }
            $arguments += @("$PSScriptRoot/eyes_thought_context_regression.cpp", "-o", $exe)
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
