param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

$repoRoot = Get-OpenA8DJRepoRoot
& (Join-Path $PSScriptRoot "build-driver.ps1") -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "build-driver.ps1 failed with exit code $LASTEXITCODE"
}

& (Join-Path $PSScriptRoot "sign-driver.ps1") -Configuration $Configuration -Platform $Platform -CreateTestCertificate
if ($LASTEXITCODE -ne 0) {
    throw "sign-driver.ps1 failed with exit code $LASTEXITCODE"
}

$packageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
$verify = & (Join-Path $PSScriptRoot "verify-driver.ps1") -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "verify-driver.ps1 failed with exit code $LASTEXITCODE"
}

& (Join-Path $PSScriptRoot "package-installer.ps1") -Configuration $Configuration -Platform $Platform
if ($LASTEXITCODE -ne 0) {
    throw "package-installer.ps1 failed with exit code $LASTEXITCODE"
}

Write-Host "Installable test package ready: $packageDir"
Write-Host "Install with elevated PowerShell:"
Write-Host "  .\windows\scripts\install-driver.ps1 -Configuration $Configuration -Platform $Platform -TrustTestCertificate -EnableTestSigning -SkipBuild"
