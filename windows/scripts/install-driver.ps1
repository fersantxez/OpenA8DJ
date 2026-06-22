param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$PackageDir,

    [switch]$SkipBuild,

    [switch]$TestSign,

    [switch]$TrustTestCertificate,

    [switch]$EnableTestSigning,

    [int]$WaitLockSeconds = 0
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force
Assert-OpenA8DJAdministrator

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $PackageDir) {
    $PackageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
}
$runDir = Join-Path $repoRoot "local-analysis\windows\install-$(Get-Date -Format yyyyMMdd-HHmmss)"
$lock = Acquire-OpenA8DJHardwareLock -Gate "windows-install-driver" -RunDir $runDir -TimeoutSeconds $WaitLockSeconds

try {
    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot "build-driver.ps1") -Configuration $Configuration -Platform $Platform
        if ($LASTEXITCODE -ne 0) {
            throw "build-driver.ps1 failed with exit code $LASTEXITCODE"
        }
    }

    $PackageDir = (Resolve-Path $PackageDir).Path
    $infPath = Join-Path $PackageDir "OpenA8DJUsb.inf"
    $catPath = Join-Path $PackageDir "OpenA8DJUsb.cat"
    $certPath = Join-Path $PackageDir "OpenA8DJUsb-TestCertificate.cer"

    if (-not (Test-Path $infPath)) {
        throw "INF not found: $infPath"
    }
    if (-not (Test-Path $catPath)) {
        throw "Catalog not found: $catPath"
    }

    if ($TestSign) {
        & (Join-Path $PSScriptRoot "sign-driver.ps1") -Configuration $Configuration -Platform $Platform -PackageDir $PackageDir -CreateTestCertificate
        if ($LASTEXITCODE -ne 0) {
            throw "sign-driver.ps1 failed with exit code $LASTEXITCODE"
        }
        $TrustTestCertificate = $true
    }

    if ($TrustTestCertificate) {
        if (Test-Path $certPath) {
            Write-Host "Importing test certificate into Trusted Root and Trusted Publishers"
            Import-Certificate -FilePath $certPath -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
            Import-Certificate -FilePath $certPath -CertStoreLocation Cert:\LocalMachine\TrustedPublisher | Out-Null
        } else {
            throw "Test certificate not found: $certPath"
        }
    }

    if ($EnableTestSigning) {
        $bootConfigBefore = & bcdedit.exe /enum
        $testSigningAlreadyActive = ($bootConfigBefore -join "`n") -match "testsigning\s+Yes"
        Write-Host "Enabling Windows test-signing mode. A reboot is normally required."
        & bcdedit.exe /set testsigning on
        if ($LASTEXITCODE -ne 0) {
            throw "bcdedit testsigning failed with exit code $LASTEXITCODE"
        }
        if (-not $testSigningAlreadyActive) {
            throw "Windows test-signing was enabled. Reboot Windows, then rerun this installer."
        }
    }

    Write-Host "Installing driver package with pnputil"
    & pnputil.exe /add-driver $infPath /install
    $pnputilExit = $LASTEXITCODE
    if ($pnputilExit -ne 0) {
        throw "pnputil install failed with exit code $pnputilExit"
    }

    $manifest = [ordered]@{
        installed_at = (Get-Date).ToUniversalTime().ToString("o")
        package_dir = $PackageDir
        inf = $infPath
        catalog = $catPath
        inf_sha256 = Get-OpenA8DJFileHashHex -Path $infPath
        catalog_sha256 = Get-OpenA8DJFileHashHex -Path $catPath
        test_sign_requested = [bool]$TestSign
        trust_test_certificate_requested = [bool]$TrustTestCertificate
        test_signing_enabled_requested = [bool]$EnableTestSigning
        lock_run_dir = $runDir
    }
    $manifestPath = Join-Path $runDir "install-manifest.json"
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -Path $manifestPath -Encoding ASCII
    Write-Host "Install manifest written to $manifestPath"
} finally {
    Release-OpenA8DJHardwareLock -Lock $lock
}
