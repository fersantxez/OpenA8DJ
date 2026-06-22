param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$PackageDir,

    [string]$OutputDir
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $PackageDir) {
    $PackageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $repoRoot "windows\dist\attestation"
}
$PackageDir = (Resolve-Path $PackageDir).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path

$required = @(
    "OpenA8DJUsb.inf",
    "OpenA8DJUsb.sys",
    "OpenA8DJUsb.cat"
)
foreach ($name in $required) {
    $path = Join-Path $PackageDir $name
    if (-not (Test-Path $path)) {
        throw "Required attestation package file is missing: $path"
    }
}

$cabName = "OpenA8DJUsb-$Configuration-$Platform-attestation.cab"
$ddfPath = Join-Path $OutputDir "OpenA8DJUsb-$Configuration-$Platform.ddf"
$cabPath = Join-Path $OutputDir $cabName
if (Test-Path $cabPath) {
    Remove-Item -Path $cabPath -Force
}

$ddf = @(
    ".OPTION EXPLICIT",
    ".Set CabinetNameTemplate=$cabName",
    ".Set DiskDirectoryTemplate=`"$($OutputDir.Replace('"', '""'))`"",
    ".Set Cabinet=on",
    ".Set Compress=on"
)
foreach ($name in $required) {
    $source = Join-Path $PackageDir $name
    $escapedSource = $source.Replace('"', '""')
    $ddf += "`"$escapedSource`" $name"
}
$ddf | Set-Content -Path $ddfPath -Encoding ASCII

& makecab.exe /F $ddfPath
if ($LASTEXITCODE -ne 0) {
    throw "makecab failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path $cabPath)) {
    throw "Expected CAB was not produced: $cabPath"
}

$manifest = [ordered]@{
    exported_at = (Get-Date).ToUniversalTime().ToString("o")
    package_dir = $PackageDir
    cab = $cabPath
    cab_sha256 = Get-OpenA8DJFileHashHex -Path $cabPath
    files = foreach ($name in $required) {
        $path = Join-Path $PackageDir $name
        [ordered]@{
            name = $name
            sha256 = Get-OpenA8DJFileHashHex -Path $path
            bytes = (Get-Item $path).Length
        }
    }
}
$manifestPath = Join-Path $OutputDir "OpenA8DJUsb-$Configuration-$Platform-attestation-manifest.json"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -Path $manifestPath -Encoding ASCII
Write-Host "Attestation CAB written to $cabPath"
Write-Host "Manifest written to $manifestPath"
