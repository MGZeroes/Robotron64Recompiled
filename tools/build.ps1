<#
.SYNOPSIS
    Native Windows build: LLVM clang + Ninja, with MSVC headers/libs from Visual Studio.
.DESCRIPTION
    Needs RecompiledFuncs/, rsp/aspMain.cpp and syms/ from the recompile steps (docs/BUILDING.md).
    Imports the VS x64 developer environment, then configures and builds build-cmake/.
.EXAMPLE
    .\tools\build.ps1                     # RelWithDebInfo with a console window (bring-up default)
    .\tools\build.ps1 -Config Release -NoConsole
#>
param(
    [ValidateSet("Debug", "RelWithDebInfo", "Release")]
    [string]$Config = "RelWithDebInfo",
    [switch]$NoConsole,
    [string]$LlvmDir = "C:\Program Files\LLVM\bin"
)

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildDir = Join-Path $Root "build-cmake"

foreach ($required in "RecompiledFuncs\funcs.h", "rsp\aspMain.cpp", "syms\robotron64.us.syms.toml") {
    if (-not (Test-Path (Join-Path $Root $required))) {
        Write-Host "Missing $required - run the recompile steps first (docs/BUILDING.md)." -ForegroundColor Red
        exit 1
    }
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { Write-Host "Visual Studio C++ build tools not found." -ForegroundColor Red; exit 1 }

# Import the environment vcvars64.bat sets up (INCLUDE, LIB, PATH for the Windows SDK and MSVC STL).
# vcvars itself calls vswhere from PATH.
$env:Path = "$(Split-Path $vswhere);$env:Path"
$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
cmd /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match "^([^=]+)=(.*)$") { Set-Item -Path "env:$($Matches[1])" -Value $Matches[2] }
}
$env:Path = "$LlvmDir;$env:Path"

# clang-cl, as in the template's CMakeSettings.json (clang_cl_x64): the CMake files pass MSVC-style
# flags (/W4, /SUBSYSTEM) that the GNU-style clang++ driver rejects.
$clangCl = (Join-Path $LlvmDir "clang-cl.exe").Replace("\", "/")
$console = if ($NoConsole) { "OFF" } else { "ON" }

# Native tools print warnings on stderr; in Windows PowerShell 5.1 that would surface as error
# records, so run them through cmd and rely on exit codes.
cmd /c "cmake -S ""$Root"" -B ""$BuildDir"" -G Ninja -DCMAKE_C_COMPILER=""$clangCl"" -DCMAKE_CXX_COMPILER=""$clangCl"" -DCMAKE_BUILD_TYPE=$Config -DROBOTRON_CONSOLE=$console -DPATCHES_BUILD_ELF=OFF 2>&1"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmd /c "cmake --build ""$BuildDir"" --target Robotron64Recompiled 2>&1"
exit $LASTEXITCODE
