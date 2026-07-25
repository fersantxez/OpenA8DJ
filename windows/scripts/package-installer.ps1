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
Test-OpenA8DJPackageManifest -PackageDir $PackageDir | Out-Null
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
    "OpenA8DJUsb.pdb",
    "OpenA8DJUsb-TestCertificate.cer",
    "OpenA8DJUsb.cer",
    "opena8djctl.exe",
    "signing-manifest.json",
    "package-manifest.json"
)
foreach ($name in $driverFiles) {
    $source = Join-Path $PackageDir $name
    if (Test-Path $source) {
        Copy-Item -Path $source -Destination (Join-Path $driverOut $name) -Force
    }
}

$requiredDriverFiles = @("OpenA8DJUsb.inf", "OpenA8DJUsb.sys", "OpenA8DJUsb.cat", "OpenA8DJUsb-TestCertificate.cer", "package-manifest.json")
foreach ($name in $requiredDriverFiles) {
    if (-not (Test-Path (Join-Path $driverOut $name))) {
        throw "Installable package is missing required file: $name"
    }
}
Test-OpenA8DJPackageManifest -PackageDir $driverOut | Out-Null

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
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File ""%SCRIPT_DIR%scripts\install-driver.ps1"" -PackageDir ""%SCRIPT_DIR%driver"" -SkipBuild -TrustTestCertificate %*"
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
    "STOP: this is an experimental Windows driver installer for controlled testing.",
    "It is publicly downloadable, but it is not Microsoft-signed, not certified,",
    "and not a production consumer installer.",
    "",
    "This package can change the Windows Driver Store and bind a kernel driver to",
    "USB\\VID_17CC&PID_1978. Kernel driver failures can crash or reboot Windows.",
    "Use it only on a test machine with recovery access.",
    "",
    "What is in this ZIP:",
    "- install.cmd: elevated development installer entry point",
    "- verify.cmd: post-install verification entry point",
    "- uninstall.cmd: removal entry point",
    "- driver\\OpenA8DJUsb.inf/cat/sys: test-signed driver package",
    "- driver\\opena8djctl.exe: diagnostics/control CLI",
    "- manifest files with hashes and signing state",
    "",
    "Install (simple path):",
    "1. Close audio applications and unplug the Audio 8 DJ.",
    "2. In an Administrator Terminal run: shutdown /r /o /t 0",
    "3. Choose Troubleshoot > Advanced options > Startup Settings > Restart,",
    "   then press 7 (Disable driver signature enforcement).",
    "4. Double-click the EXE, or run install.cmd from this extracted ZIP, and",
    "   approve the UAC prompt.",
    "5. If Windows Security asks, choose Install this driver software anyway.",
    "6. Reconnect the Audio 8 DJ and wait for Windows to detect it.",
    "7. Option 7 is one-boot only. Do not use -ForceInstallDespiteSecureBoot or",
    "   combine option 7 with -EnableTestSigning.",
    "",
    "Secure Boot limitation:",
    "Windows blocks bcdedit /set testsigning on while Secure Boot is enabled.",
    "Installing this test-signed package anyway can bind it to the Audio 8 DJ",
    "but leave the device failed with Code 52 / 0xC0000428.",
    "Use a Microsoft-signed package for Secure Boot systems.",
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
    "Current limitations:",
    "- This build is experimental and not Microsoft-signed.",
    "- It is not the macOS OpenA8DJ driver and does not install macOS payloads.",
    "- It is not yet a production-quality Windows DJ/audio driver.",
    "- Full DVS/timecode, MIDI, ASIO, hotplug, sleep/wake, and clean-install",
    "  validation remain release gates.",
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
