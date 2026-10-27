<#
.SYNOPSIS
    Verifies the Robotron 64 (USA) ROM and normalizes it to big-endian .z64.
.DESCRIPTION
    Accepts .z64 (big-endian), .v64 (byte-swapped) or .n64 (word-swapped) input.
    If the input is not already .z64, writes a converted copy to -Output.
    Exit code 0 = ROM matches the expected hash, 1 = mismatch or bad input.
.EXAMPLE
    .\tools\check_rom.ps1
    .\tools\check_rom.ps1 -Rom D:\roms\robotron.v64 -Output .\baserom.us.z64
#>
param(
    [string]$Rom = (Join-Path $PSScriptRoot "..\baserom.us.z64"),
    [string]$Output = (Join-Path $PSScriptRoot "..\baserom.us.z64")
)

# Robotron 64 (USA), rev 0 -- same values as decomp/config/target.json
$ExpectedSha1 = "44D158BC2AEEFB111A620B61E043B2703E6C5808"
$ExpectedSize = 8388608

if (-not (Test-Path -LiteralPath $Rom)) { Write-Error "ROM not found: $Rom"; exit 1 }
$Rom = (Resolve-Path -LiteralPath $Rom).Path
$data = [IO.File]::ReadAllBytes($Rom)

if ($data.Length -ne $ExpectedSize) {
    Write-Error ("Unexpected size {0} (expected {1})" -f $data.Length, $ExpectedSize); exit 1
}

$magic = ($data[0..3] | ForEach-Object { $_.ToString("X2") }) -join ""
switch ($magic) {
    "80371240" { $format = "z64" }
    "37804012" {
        $format = "v64"
        for ($i = 0; $i -lt $data.Length; $i += 2) {
            $t = $data[$i]; $data[$i] = $data[$i + 1]; $data[$i + 1] = $t
        }
    }
    "40123780" {
        $format = "n64"
        for ($i = 0; $i -lt $data.Length; $i += 4) {
            $t = $data[$i]; $data[$i] = $data[$i + 3]; $data[$i + 3] = $t
            $t = $data[$i + 1]; $data[$i + 1] = $data[$i + 2]; $data[$i + 2] = $t
        }
    }
    default { Write-Error "Not an N64 ROM (magic $magic)"; exit 1 }
}

$sha1 = [Security.Cryptography.SHA1]::Create()
$hash = (($sha1.ComputeHash($data) | ForEach-Object { $_.ToString("X2") }) -join "")
$code = [Text.Encoding]::ASCII.GetString($data, 0x3B, 4)
$crc1 = ($data[0x10..0x13] | ForEach-Object { $_.ToString("X2") }) -join ""
$crc2 = ($data[0x14..0x17] | ForEach-Object { $_.ToString("X2") }) -join ""

Write-Host ("Input:    {0} ({1})" -f $Rom, $format)
Write-Host ("Game ID:  {0}, revision {1}, CRC {2}/{3}" -f $code, $data[0x3F], $crc1, $crc2)
Write-Host ("SHA-1:    {0}" -f $hash)

if ($format -ne "z64") {
    if ((Test-Path -LiteralPath $Output) -and ((Resolve-Path -LiteralPath $Output).Path -eq $Rom)) {
        Write-Error "Input is $format; pass -Output with a different path for the converted .z64"; exit 1
    }
    [IO.File]::WriteAllBytes($Output, $data)
    Write-Host "Wrote big-endian copy to $Output"
}

if ($hash -eq $ExpectedSha1) {
    Write-Host "OK: Robotron 64 (USA) rev 0 -- matches the decomp's target." -ForegroundColor Green
    exit 0
}
if ($crc1 -eq "AC8E4B32" -and $crc2 -eq "E7B47326") {
    Write-Host "MISMATCH: header CRCs are correct (code is likely intact) but the full-file hash differs -- bad or modified dump." -ForegroundColor Yellow
} else {
    Write-Host "MISMATCH: this is not Robotron 64 (USA) rev 0." -ForegroundColor Red
}
exit 1
