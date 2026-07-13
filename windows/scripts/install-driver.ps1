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

    [switch]$ForceInstallDespiteSecureBoot,

    [switch]$DisableDeviceOnInstallProblem,

    [switch]$AllowPhysicalBind,

    [switch]$AcknowledgeCrashRisk,

    [string]$ExpectedInstanceId = "USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___",

    [int]$WaitLockSeconds = 0
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force
Assert-OpenA8DJAdministrator

function Add-OpenA8DJCertificateToStore {
    param(
        [Parameter(Mandatory = $true)][string]$CertificatePath,
        [Parameter(Mandatory = $true)][System.Security.Cryptography.X509Certificates.StoreName]$StoreName,
        [Parameter(Mandatory = $true)][System.Security.Cryptography.X509Certificates.StoreLocation]$StoreLocation
    )

    $cert = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new($CertificatePath)
    $store = [System.Security.Cryptography.X509Certificates.X509Store]::new($StoreName, $StoreLocation)
    try {
        $store.Open([System.Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
        $store.Add($cert)
    } finally {
        $store.Close()
    }
}

function Get-OpenA8DJTestSigningActive {
    $bootConfig = (& bcdedit.exe /enum "{current}" 2>$null) -join "`n"
    return ($bootConfig -match "testsigning\s+Yes")
}

function Get-OpenA8DJSecureBootEnabled {
    try {
        return [bool](Confirm-SecureBootUEFI)
    } catch {
        return $false
    }
}

function Get-OpenA8DJAudio8Device {
    Get-PnpDevice -InstanceId "USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___" -ErrorAction SilentlyContinue
}

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
    $packageManifest = Test-OpenA8DJPackageManifest -PackageDir $PackageDir
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

    $signature = Get-AuthenticodeSignature -FilePath $catPath
    $isOpenA8DJTestSignedPackage =
        $signature.SignerCertificate -and
        $signature.SignerCertificate.Subject -eq "CN=OpenA8DJ Windows Test Driver"
    $secureBootEnabled = Get-OpenA8DJSecureBootEnabled
    $testSigningAlreadyActive = Get-OpenA8DJTestSigningActive

    if ($EnableTestSigning -and $secureBootEnabled) {
        throw "Secure Boot is enabled, so Windows will reject bcdedit /set testsigning on. Disable Secure Boot in UEFI or use a Microsoft-signed driver package before installing OpenA8DJ."
    }

    if ($isOpenA8DJTestSignedPackage -and $secureBootEnabled -and -not $testSigningAlreadyActive -and -not $ForceInstallDespiteSecureBoot) {
        throw "Refusing to install OpenA8DJ test-signed kernel driver while Secure Boot is enabled. Windows would bind the package but fail the device with Code 52 / 0xC0000428. Disable Secure Boot in UEFI, use a Microsoft-signed package, or pass -ForceInstallDespiteSecureBoot for diagnostics only."
    }

    if ($TrustTestCertificate) {
        if (Test-Path $certPath) {
            Write-Host "Importing test certificate into Trusted Root and Trusted Publishers"
            Add-OpenA8DJCertificateToStore -CertificatePath $certPath -StoreName Root -StoreLocation LocalMachine
            Add-OpenA8DJCertificateToStore -CertificatePath $certPath -StoreName TrustedPublisher -StoreLocation LocalMachine
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

    if ($AllowPhysicalBind) {
        if (-not $AcknowledgeCrashRisk) {
            throw "Physical binding requires both -AllowPhysicalBind and -AcknowledgeCrashRisk."
        }
        $deviceBefore = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
        if (-not $deviceBefore -or $deviceBefore.Problem -ne 'CM_PROB_DISABLED') {
            throw "Physical binding is only permitted while the exact Audio 8 DJ device is disabled."
        }
    }

    Write-Host $(if ($AllowPhysicalBind) { "Staging and binding driver package with pnputil" } else { "Staging driver package without binding or loading it" })
    $pnputilArguments = @('/add-driver', $infPath)
    if ($AllowPhysicalBind) { $pnputilArguments += '/install' }
    & pnputil.exe @pnputilArguments
    $pnputilExit = $LASTEXITCODE
    $acceptedPnPUtilExits = @(0, 259, 3010)
    if ($acceptedPnPUtilExits -notcontains $pnputilExit) {
        throw "pnputil install failed with exit code $pnputilExit"
    }

    $storeMatches = @(Get-OpenA8DJDriverStoreMatches -PackageDir $PackageDir)
    $exactStoreMatches = @($storeMatches | Where-Object Exact)
    if ($exactStoreMatches.Count -ne 1 -or $storeMatches.Count -ne 1) {
        throw "DriverStore identity gate failed: expected exactly one OpenA8DJ package and one exact hash match; found $($storeMatches.Count) total/$($exactStoreMatches.Count) exact."
    }

    $audio8Device = Get-OpenA8DJAudio8Device
    $installProblem = $false
    if ($audio8Device) {
        $installProblem = $audio8Device.Problem -ne "CM_PROB_NONE"
        if ($DisableDeviceOnInstallProblem -and $installProblem) {
            Write-Host "Disabling Audio 8 DJ because install left device in problem state: $($audio8Device.Problem)"
            Disable-PnpDevice -InstanceId $audio8Device.InstanceId -Confirm:$false
            $audio8Device = Get-OpenA8DJAudio8Device
        }
    }

    $manifest = [ordered]@{
        installed_at = (Get-Date).ToUniversalTime().ToString("o")
        package_dir = $PackageDir
        package_build_fingerprint = $packageManifest.build_fingerprint
        package_sys_sha256 = (@($packageManifest.files | Where-Object name -eq 'OpenA8DJUsb.sys')[0].sha256)
        driver_store_path = $exactStoreMatches[0].Path
        driver_store_sys_sha256 = $exactStoreMatches[0].SysSha256
        stage_only = -not [bool]$AllowPhysicalBind
        inf = $infPath
        catalog = $catPath
        inf_sha256 = Get-OpenA8DJFileHashHex -Path $infPath
        catalog_sha256 = Get-OpenA8DJFileHashHex -Path $catPath
        test_sign_requested = [bool]$TestSign
        trust_test_certificate_requested = [bool]$TrustTestCertificate
        test_signing_enabled_requested = [bool]$EnableTestSigning
        force_install_despite_secure_boot = [bool]$ForceInstallDespiteSecureBoot
        disable_device_on_install_problem = [bool]$DisableDeviceOnInstallProblem
        secure_boot_enabled = [bool]$secureBootEnabled
        test_signing_active = [bool]$testSigningAlreadyActive
        test_signed_package_detected = [bool]$isOpenA8DJTestSignedPackage
        pnputil_exit = $pnputilExit
        device_problem_after_install = if ($audio8Device) { [string]$audio8Device.Problem } else { $null }
        device_status_after_install = if ($audio8Device) { [string]$audio8Device.Status } else { $null }
        lock_run_dir = $runDir
    }
    $manifestPath = Join-Path $runDir "install-manifest.json"
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -Path $manifestPath -Encoding ASCII
    Write-Host "Install manifest written to $manifestPath"
} finally {
    Release-OpenA8DJHardwareLock -Lock $lock
}
