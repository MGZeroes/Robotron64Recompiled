<#
.SYNOPSIS
    Texture pack helper for Robotron 64: Recompiled (see docs/TEXTURE_PACKS.md).

.DESCRIPTION
    -Init <name>      Creates an editable texture pack folder in the game's mods folder (mod.json, rt64.json
                      and README.txt from tools/texture_pack_template). Never overwrites an existing pack.
    -DevMode On|Off   Sets "developer_mode" in graphics.json, which enables RT64's inspector (F1). Read when
                      the game starts, so set it before launching. Other settings are kept.
    -Pack <name>      Zips the pack folder into <name>.rtz next to it (in the mods folder), without work files
                      (.afphoto/.afdesign, ~bak/~lock~ files, rt64.json.old). Move or delete the folder
                      afterwards, or the game loads both.

    The game's config folder is %LOCALAPPDATA%\Robotron64Recompiled; pass -ConfigDir for a portable install
    (the folder with portable.txt).

.EXAMPLE
    .\tools\texture_pack.ps1 -Init robotron64-hud -Author "Your Name"
    .\tools\texture_pack.ps1 -DevMode On
    .\tools\texture_pack.ps1 -Pack robotron64-hud
#>
param(
    [string]$Init,
    [ValidateSet("On", "Off")]
    [string]$DevMode,
    [string]$Pack,
    [string]$Author = "Unknown",
    [string]$DisplayName,
    [string]$ConfigDir = (Join-Path $env:LOCALAPPDATA "Robotron64Recompiled")
)

$ErrorActionPreference = "Stop"
$templateDir = Join-Path $PSScriptRoot "texture_pack_template"
$modsDir = Join-Path $ConfigDir "mods"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false

function Assert-PackName([string]$name) {
    if ($name -notmatch '^[A-Za-z0-9._-]+$') {
        throw "Pack name '$name' may only contain letters, digits, '.', '_' and '-' (it is also the mod id)."
    }
}

if (-not ($Init -or $DevMode -or $Pack)) {
    Get-Help $PSCommandPath -Detailed
    exit 1
}

if ($Init) {
    Assert-PackName $Init
    $packDir = Join-Path $modsDir $Init
    if (Test-Path $packDir) {
        throw "Pack folder already exists: $packDir"
    }
    if (-not $DisplayName) {
        $DisplayName = $Init
    }
    New-Item -ItemType Directory -Force $packDir | Out-Null
    $manifest = [System.IO.File]::ReadAllText((Join-Path $templateDir "mod.json"))
    $manifest = $manifest.Replace("__ID__", $Init).Replace("__NAME__", $DisplayName).Replace("__AUTHOR__", $Author)
    [System.IO.File]::WriteAllText((Join-Path $packDir "mod.json"), $manifest, $utf8NoBom)
    Copy-Item (Join-Path $templateDir "rt64.json") $packDir
    Copy-Item (Join-Path $templateDir "README.txt") $packDir
    Write-Host "Created $packDir" -ForegroundColor Green
}

if ($DevMode) {
    $graphicsPath = Join-Path $ConfigDir "graphics.json"
    if (Test-Path $graphicsPath) {
        $config = Get-Content -Raw $graphicsPath | ConvertFrom-Json
    } else {
        New-Item -ItemType Directory -Force $ConfigDir | Out-Null
        $config = New-Object PSObject
    }
    $enabled = $DevMode -eq "On"
    if ($config.PSObject.Properties.Name -contains "developer_mode") {
        $config.developer_mode = $enabled
    } else {
        $config | Add-Member -NotePropertyName developer_mode -NotePropertyValue $enabled
    }
    [System.IO.File]::WriteAllText($graphicsPath, ($config | ConvertTo-Json -Depth 10), $utf8NoBom)
    Write-Host "developer_mode = $enabled in $graphicsPath (takes effect at the next game start)" -ForegroundColor Green
}

if ($Pack) {
    Assert-PackName $Pack
    $packDir = Join-Path $modsDir $Pack
    if (-not (Test-Path (Join-Path $packDir "rt64.json"))) {
        throw "No texture pack folder with rt64.json at $packDir"
    }
    $rtzPath = Join-Path $modsDir "$Pack.rtz"
    if (Test-Path $rtzPath) { Remove-Item $rtzPath }
    # Pack the folder's contents (not the folder itself), leaving out work files: rt64.json.old is the
    # inspector's backup, the rest are Affinity documents, backups and lock files.
    $exclude = @("rt64.json.old", "*.afphoto", "*.afdesign", "*~bak*", "*~lock~")
    $root = (Resolve-Path $packDir).Path.TrimEnd('\') + '\'
    $files = Get-ChildItem $packDir -Recurse -File | Where-Object {
        $name = $_.Name
        -not ($exclude | Where-Object { $name -like $_ })
    }
    # ZipArchive directly, not Compress-Archive: Windows PowerShell 5.1 writes backslashes into entry names.
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::Open($rtzPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $files) {
            $entry = $file.FullName.Substring($root.Length).Replace('\', '/')
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $entry) | Out-Null
        }
    } finally {
        $zip.Dispose()
    }
    Write-Host "Wrote $rtzPath ($($files.Count) files; work files left out)" -ForegroundColor Green
    Write-Host "Move the folder $packDir out of the mods folder before testing the .rtz, or both load." -ForegroundColor Yellow
}
