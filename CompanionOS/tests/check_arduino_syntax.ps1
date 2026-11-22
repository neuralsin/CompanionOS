param([Parameter(Mandatory=$true)][string]$CompilationDatabase)
$ErrorActionPreference = "Stop"
$entries = Get-Content -Raw -LiteralPath $CompilationDatabase | ConvertFrom-Json
$entry = $entries | Where-Object { $_.file -like '*CompanionOS_Main.ino.cpp' } | Select-Object -First 1
if (!$entry -or !$entry.arguments) { throw "No CompanionOS Arduino compilation entry found." }
$compiler = $entry.arguments[0]
$flags = @()
for ($i=1; $i -lt $entry.arguments.Count; $i++) {
    $arg = $entry.arguments[$i]
    if ($arg -in @('-o','-MF','-MT','-MQ')) { $i++; continue }
    if ($arg -in @('-MMD','-MD','-MP','-c') -or $arg -eq $entry.file) { continue }
    $flags += $arg
}
# Use current sources and real library headers, not Arduino's cached sketch copy.
$source = (Resolve-Path "$PSScriptRoot/../arduino/CompanionOS_Main/CompanionOS_Main.ino").Path
$flags += @('-fsyntax-only','-x','c++','-include','Arduino.h',$source)
& $compiler @flags
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output "PASS current sketch syntax with real Arduino/TFT libraries; no firmware generated."
