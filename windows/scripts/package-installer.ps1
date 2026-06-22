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
    $OutputDir = Join-Path $repoRoot "windows\dist\installer"
}

$PackageDir = (Resolve-Path $PackageDir).Path
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path

$stagingName = "OpenA8DJUsb-$Configuration-$Platform"
$stagingRoot = Join-Path $OutputDir $stagingName
$driverOut = Join-Path $stagingRoot "driver"
$scriptsOut = Join-Path $stagingRoot "scripts"
$zipPath = Join-Path $OutputDir "$stagingName-installer.zip"

if (Test-Path $stagingRoot) {
    Remove-Item -Path $stagingRoot -Recurse -Force
}
if (Test-Path $zipPath) {
    Remove-Item -Path $zipPath -Force
}
New-Item -ItemType Directory -Path $driverOut -Force | Out-Null
New-Item -ItemType Directory -Path $scriptsOut -Force | Out-Null

$driverFiles = @(
    "OpenA8DJUsb.inf",
    "OpenA8DJUsb.sys",
    "OpenA8DJUsb.cat",
    "OpenA8DJUsb-TestCertificate.cer",
    "OpenA8DJUsb.cer",
    "opena8djctl.exe",
    "signing-manifest.json"
)
foreach ($name in $driverFiles) {
    $source = Join-Path $PackageDir $name
    if (Test-Path $source) {
        Copy-Item -Path $source -Destination (Join-Path $driverOut $name) -Force
    }
}

$requiredDriverFiles = @("OpenA8DJUsb.inf", "OpenA8DJUsb.sys", "OpenA8DJUsb.cat", "OpenA8DJUsb-TestCertificate.cer")
foreach ($name in $requiredDriverFiles) {
    if (-not (Test-Path (Join-Path $driverOut $name))) {
        throw "Installable package is missing required file: $name"
    }
}

$scriptFiles = @(
    "OpenA8DJ.WindowsCommon.psm1",
    "install-driver.ps1",
    "uninstall-driver.ps1",
    "verify-driver.ps1"
)
foreach ($name in $scriptFiles) {
    Copy-Item -Path (Join-Path $PSScriptRoot $name) -Destination (Join-Path $scriptsOut $name) -Force
}

$installCmd = @(
    "@echo off",
    "set SCRIPT_DIR=%~dp0",
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File ""%SCRIPT_DIR%scripts\install-driver.ps1"" -PackageDir ""%SCRIPT_DIR%driver"" -SkipBuild -TrustTestCertificate -EnableTestSigning %*"
)
Set-Content -Path (Join-Path $stagingRoot "install.cmd") -Value $installCmd -Encoding ASCII

$verifyCmd = @(
    "@echo off",
    "set SCRIPT_DIR=%~dp0",
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File ""%SCRIPT_DIR%scripts\verify-driver.ps1"" -PackageDir ""%SCRIPT_DIR%driver"" -RunControlTool %*"
)
Set-Content -Path (Join-Path $stagingRoot "verify.cmd") -Value $verifyCmd -Encoding ASCII

$uninstallCmd = @(
    "@echo off",
    "set SCRIPT_DIR=%~dp0",
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File ""%SCRIPT_DIR%scripts\uninstall-driver.ps1"" %*"
)
Set-Content -Path (Join-Path $stagingRoot "uninstall.cmd") -Value $uninstallCmd -Encoding ASCII

$readmeFirst = @(
    "OpenA8DJ Windows Driver - Development Installer",
    "",
    "This package is for controlled Windows testing only.",
    "It is not Microsoft-signed and it is not a public consumer installer.",
    "",
    "Install:",
    "1. Extract this ZIP.",
    "2. Run install.cmd as Administrator.",
    "3. If the installer enables Windows test-signing and asks for a reboot, reboot.",
    "4. Run install.cmd as Administrator again after reboot.",
    "",
    "Verify:",
    "1. Run verify.cmd as Administrator.",
    "2. Save the JSON output.",
    "3. Run driver\\opena8djctl.exe surface.",
    "4. Run driver\\opena8djctl.exe diagnostics.",
    "",
    "Uninstall:",
    "1. Run uninstall.cmd as Administrator.",
    "",
    "Expected limitation:",
    "The current driver is a Windows USB surface/diagnostics driver.",
    "It intentionally rejects stream start until the real Windows audio endpoint",
    "and isochronous engine are implemented.",
    "",
    "Do not use this on a production DJ system."
)
Set-Content -Path (Join-Path $stagingRoot "README-FIRST.txt") -Value $readmeFirst -Encoding ASCII

$manifest = [ordered]@{
    packaged_at = (Get-Date).ToUniversalTime().ToString("o")
    configuration = $Configuration
    platform = $Platform
    source_package_dir = $PackageDir
    staging_root = $stagingRoot
    zip = $zipPath
    driver_files = foreach ($name in (Get-ChildItem -Path $driverOut -File | Sort-Object Name).Name) {
        $path = Join-Path $driverOut $name
        [ordered]@{
            name = $name
            sha256 = Get-OpenA8DJFileHashHex -Path $path
            bytes = (Get-Item $path).Length
        }
    }
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $stagingRoot "installer-manifest.json") -Encoding ASCII

Compress-Archive -Path (Join-Path $stagingRoot "*") -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "Installer staging directory: $stagingRoot"
Write-Host "Installer ZIP: $zipPath"
