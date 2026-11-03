<#
.SYNOPSIS
    Builds the Windows, Linux and Android release packages from one clean commit (ADR 010).
.DESCRIPTION
    Needs the recompile steps (docs/BUILDING.md), Docker Desktop and the key from tools/make_release_key.ps1.
    The Linux and Android parts run in Docker. Output goes to dist/, which is gitignored: the packages
    contain recompiled game code, so they must never be committed.
.EXAMPLE
    .\tools\package_release.ps1
#>
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Dist = Join-Path $Root "dist"
$Cache = Join-Path $Root "build-tools\release-cache"

# RT64's own dxil.dll and dxcompiler.dll are from v1.7.2212, whose licence forbids distributing them.
# ADR 010 explains the switch to this release for packages.
$DxcUrl = "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip"
$DxcSha256 = "9AD895A6B039E3A8F8C22A1009F866800B840A74B50DB9218D13319E215EA8A4"

function Stop-Release([string]$Message) {
    Write-Host $Message -ForegroundColor Red
    exit 1
}

function Get-TreeState {
    (git -C $Root status --porcelain --ignore-submodules=none) -join "`n"
}

if (Get-TreeState) { Stop-Release "The git tree isn't clean. Commit first, so the packages match one commit." }
$Commit = git -C $Root rev-parse HEAD

$VersionLine = Select-String -Path (Join-Path $Root "CMakeLists.txt") -Pattern "^project\(Robotron64Recompiled VERSION ([0-9.]+)"
if (-not $VersionLine) { Stop-Release "No version found in CMakeLists.txt" }
$Version = $VersionLine.Matches[0].Groups[1].Value
Write-Host "== Robotron 64: Recompiled $Version from $Commit"

if (Test-Path $Dist) { Remove-Item -Recurse -Force $Dist }
New-Item -ItemType Directory -Force $Dist, $Cache | Out-Null

Write-Host "== Windows"
& (Join-Path $PSScriptRoot "build.ps1") -Config Release -NoConsole
if ($LASTEXITCODE -ne 0) { Stop-Release "Windows build failed" }

$DxcZip = Join-Path $Cache "dxc-v1.8.2505.1.zip"
if (-not (Test-Path $DxcZip)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $DxcUrl -OutFile $DxcZip -UseBasicParsing
}
if ((Get-FileHash -Algorithm SHA256 $DxcZip).Hash -ne $DxcSha256) { Stop-Release "The DXC download doesn't match its pinned hash" }
$DxcDir = Join-Path $Cache "dxc"
if (Test-Path $DxcDir) { Remove-Item -Recurse -Force $DxcDir }
Expand-Archive -Path $DxcZip -DestinationPath $DxcDir

$Name = "Robotron64Recompiled-$Version-windows-x64"
$Stage = Join-Path $Cache $Name
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force $Stage | Out-Null
$Build = Join-Path $Root "build-cmake"
Copy-Item (Join-Path $Build "Robotron64Recompiled.exe"), (Join-Path $Build "SDL2.dll") $Stage
Copy-Item (Join-Path $DxcDir "bin\x64\dxil.dll"), (Join-Path $DxcDir "bin\x64\dxcompiler.dll") $Stage
Copy-Item -Recurse (Join-Path $Root "assets") $Stage
Copy-Item (Join-Path $Root "recompcontrollerdb.txt"), (Join-Path $Root "LICENSE"), (Join-Path $Root "CREDITS.md") $Stage
cmd /c "python ""$(Join-Path $PSScriptRoot 'collect_licenses.py')"" ""$(Join-Path $Stage 'licenses')"" 2>&1"
if ($LASTEXITCODE -ne 0) { Stop-Release "Collecting licences failed" }
foreach ($License in "LICENSE-MS", "LICENSE-LLVM", "LICENSE-MIT") {
    Copy-Item (Join-Path $DxcDir "$License.txt") (Join-Path $Stage "licenses\DirectXShaderCompiler-$License.txt")
}
Compress-Archive -Path $Stage -DestinationPath (Join-Path $Dist "$Name.zip")

Write-Host "== Linux (Docker)"
& (Join-Path $PSScriptRoot "build_elf.ps1") -Script tools/build_linux.sh
if ($LASTEXITCODE -ne 0) { Stop-Release "Linux build failed" }
Copy-Item (Join-Path $Root "build-linux\Robotron64Recompiled-linux-x64.tar.gz") (Join-Path $Dist "Robotron64Recompiled-$Version-linux-x64.tar.gz")

Write-Host "== Android (Docker)"
& (Join-Path $PSScriptRoot "build_android.ps1") -Release
if ($LASTEXITCODE -ne 0) { Stop-Release "Android build failed" }
Copy-Item (Join-Path $Root "build-android\Robotron64-$Version.apk") (Join-Path $Dist "Robotron64Recompiled-$Version-android-arm64.apk")

if (Get-TreeState) { Stop-Release "The builds changed tracked files:`n$(Get-TreeState)" }

$Lines = Get-ChildItem $Dist -File | Sort-Object Name | ForEach-Object {
    "$((Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower())  $($_.Name)"
}
[System.IO.File]::WriteAllText((Join-Path $Dist "SHA256SUMS.txt"), (($Lines -join "`n") + "`n"))

Get-ChildItem $Dist | Format-Table Name, Length
Write-Host "== done: $Dist, from commit $Commit" -ForegroundColor Green
