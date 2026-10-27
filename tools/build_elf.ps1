<#
.SYNOPSIS
    Runs a Linux-side build step in the Docker build image (ADR 003).
.DESCRIPTION
    Builds the image from tools/docker/Dockerfile (cached after the first run), then runs
    the given in-container script with the repo mounted at /work.
.EXAMPLE
    .\tools\build_elf.ps1                       # decomp ELF + analysis -> build-elf/
    .\tools\build_elf.ps1 -Script tools/recomp.sh
#>
param(
    [string]$Script = "tools/build_elf.sh"
)

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Image = "robotron64-recomp-build"

docker build -q -t $Image (Join-Path $Root "tools\docker") | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "docker build failed" -ForegroundColor Red; exit 1 }

# Compiler warnings arrive on stderr; in Windows PowerShell 5.1 any native stderr line becomes an
# error record, so rely on the exit code only. Merge streams through cmd to keep output in order.
cmd /c "docker run --rm -v ""${Root}:/work"" $Image bash $Script 2>&1"
exit $LASTEXITCODE
