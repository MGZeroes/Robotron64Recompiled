<#
.SYNOPSIS
    Creates the Android release signing key, once (ADR 010).
.DESCRIPTION
    Writes release.jks and a random keystore-password.txt to a folder outside the repo. Back both up:
    without them, updates can't be installed over the app without uninstalling it, and that deletes the
    app's saves on the phone. Refuses to overwrite an existing key.
.EXAMPLE
    .\tools\make_release_key.ps1
#>
param(
    [string]$KeyDir = (Join-Path $env:USERPROFILE ".robotron64")
)

$Keystore = Join-Path $KeyDir "release.jks"
$PasswordFile = Join-Path $KeyDir "keystore-password.txt"

if ((Test-Path $Keystore) -or (Test-Path $PasswordFile)) {
    Write-Host "A key already exists in $KeyDir. Not overwriting it." -ForegroundColor Red
    exit 1
}
if (-not (Get-Command keytool -ErrorAction SilentlyContinue)) {
    Write-Host "keytool not found; it comes with a JDK." -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Force $KeyDir | Out-Null

$bytes = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
$password = ([BitConverter]::ToString($bytes)) -replace "-", ""
[System.IO.File]::WriteAllText($PasswordFile, $password)

# The certificate ships in every APK and is readable by anyone, so it holds no personal details.
cmd /c "keytool -genkeypair -keystore ""$Keystore"" -storetype PKCS12 -storepass:file ""$PasswordFile"" -alias robotron64 -keyalg RSA -keysize 4096 -validity 18250 -dname ""CN=Robotron 64 Recompiled"" 2>&1"
if ($LASTEXITCODE -ne 0) {
    Remove-Item -Force $PasswordFile
    Write-Host "keytool failed" -ForegroundColor Red
    exit 1
}

Write-Host "Key created in $KeyDir. Back up release.jks and keystore-password.txt." -ForegroundColor Green
