<#
.SYNOPSIS
    Builds the Android APK in the Android Docker image (ADR 007).
.DESCRIPTION
    Builds the image from tools/docker/android/Dockerfile (cached after the first run), then runs
    tools/build_android.sh with the repo mounted at /work. Build files stay in the Docker volume
    robotron64-android-build, which is much faster than building on the mounted Windows folder.
    Output: build-android/Robotron64-debug.apk, or with -Release build-android/Robotron64-<version>.apk,
    signed with the key from tools/make_release_key.ps1 (ADR 010).
.EXAMPLE
    .\tools\build_android.ps1
.EXAMPLE
    .\tools\build_android.ps1 -Release
#>
param(
    [string]$Step = "all",
    [switch]$Release,
    [string]$KeyDir = (Join-Path $env:USERPROFILE ".robotron64")
)

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Image = "robotron64-android-build"

docker build -q -t $Image (Join-Path $Root "tools\docker\android") | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "docker build failed" -ForegroundColor Red; exit 1 }

$Signing = ""
if ($Release) {
    if (-not (Test-Path (Join-Path $KeyDir "release.jks"))) {
        Write-Host "No release key in $KeyDir; run tools/make_release_key.ps1 first." -ForegroundColor Red
        exit 1
    }
    $Signing = "-e ROBOTRON_RELEASE=1 -v ""${KeyDir}:/signing:ro"""
}

cmd /c "docker run --rm -v ""${Root}:/work"" -v robotron64-android-build:/build $Signing $Image bash tools/build_android.sh $Step 2>&1"
exit $LASTEXITCODE
