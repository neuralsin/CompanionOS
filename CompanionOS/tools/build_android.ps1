param([string]$Gradle = "", [switch]$Offline)
$ErrorActionPreference = "Stop"
$project = (Resolve-Path "$PSScriptRoot/../android/CompanionApp").Path
if (!$Gradle) {
    $Gradle = Get-ChildItem "$env:USERPROFILE/.gradle/wrapper/dists/gradle-8.7-bin" -Filter gradle.bat -Recurse |
        Select-Object -First 1 -ExpandProperty FullName
}
if (!$Gradle) { throw "Gradle 8.7 is required. Pass its executable with -Gradle." }
$build = Join-Path $env:TEMP ("companion-android-" + (Get-Date -Format "yyyyMMdd-HHmmss"))
$arguments = @("--project-dir", $project, "--project-cache-dir", "$build/cache",
    "-PcompanionBuildDir=$build/app", ":app:assembleRelease", ":app:testDebugUnitTest", ":app:lintDebug",
    "--no-daemon", "--no-parallel", "--console=plain")
if ($Offline) { $arguments += "--offline" }
& $Gradle @arguments
if ($LASTEXITCODE -ne 0) { throw "Android build failed. Intermediates: $build" }
$output = Join-Path $PSScriptRoot "../OUTPUT"
New-Item -ItemType Directory -Force -Path $output | Out-Null
Copy-Item -LiteralPath "$build/app/outputs/apk/release/app-release.apk" -Destination "$output/CompanionOS-7.3.0.apk"
Copy-Item -LiteralPath "$build/app/reports/lint-results-debug.html" -Destination "$output/android-lint.html"
Write-Output "APK: $output/CompanionOS-7.3.0.apk"
Write-Output "Test reports: $build/app/test-results/testDebugUnitTest"
Get-FileHash -LiteralPath "$output/CompanionOS-7.3.0.apk" -Algorithm SHA256
