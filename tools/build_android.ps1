<#
.SYNOPSIS
    Builds the Android test APK in the Android Docker image (ADR 007).
.DESCRIPTION
    Builds the image from tools/docker/android/Dockerfile (cached after the first run), then runs
    tools/build_android.sh with the repo mounted at /work. Build files stay in the Docker volume
    robotron64-android-build, which is much faster than building on the mounted Windows folder.
    Output: build-android/Robotron64-debug.apk
.EXAMPLE
    .\tools\build_android.ps1
#>
param(
    [string]$Step = "all"
)

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Image = "robotron64-android-build"

docker build -q -t $Image (Join-Path $Root "tools\docker\android") | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "docker build failed" -ForegroundColor Red; exit 1 }

cmd /c "docker run --rm -v ""${Root}:/work"" -v robotron64-android-build:/build $Image bash tools/build_android.sh $Step 2>&1"
exit $LASTEXITCODE
