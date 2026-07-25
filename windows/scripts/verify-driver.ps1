param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$PackageDir,

    [switch]$RunControlTool
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $PackageDir) {
    $PackageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
}
$PackageDir = (Resolve-Path $PackageDir).Path
$infPath = Join-Path $PackageDir "OpenA8DJUsb.inf"
$catPath = Join-Path $PackageDir "OpenA8DJUsb.cat"
$sysPath = Join-Path $PackageDir "OpenA8DJUsb.sys"
$ctlPath = Join-Path $PackageDir "opena8djctl.exe"
$reportDir = Join-Path $repoRoot "local-analysis\windows\verify-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $reportDir -Force | Out-Null

$files = @($infPath, $catPath, $sysPath, $ctlPath)
$fileReports = foreach ($file in $files) {
    [pscustomobject]@{
        path = $file
        exists = Test-Path $file
        sha256 = if (Test-Path $file) { Get-OpenA8DJFileHashHex -Path $file } else { $null }
        bytes = if (Test-Path $file) { (Get-Item $file).Length } else { $null }
    }
}

$signature = $null
if (Test-Path $catPath) {
    $sig = Get-AuthenticodeSignature -FilePath $catPath
    $signature = [ordered]@{
        status = [string]$sig.Status
        status_message = $sig.StatusMessage
        signer = if ($sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { $null }
        thumbprint = if ($sig.SignerCertificate) { $sig.SignerCertificate.Thumbprint } else { $null }
    }
}

$pnputilDrivers = & pnputil.exe /enum-drivers
$driverStoreText = ($pnputilDrivers -join "`n")
$driverStoreHasOpenA8DJ = $driverStoreText -match "Provider Name\s*:\s*OpenA8DJ" -or $driverStoreText -match "Original Name\s*:\s*OpenA8DJUsb.inf"

$deviceText = & pnputil.exe /enum-devices /connected /class USBDevice
$deviceTextJoined = ($deviceText -join "`n")
$devicePresent = $deviceTextJoined -match "VID_17CC&PID_1978"

$control = $null
if ($RunControlTool -and (Test-Path $ctlPath)) {
    $surface = & $ctlPath surface 2>&1
    $surfaceExit = $LASTEXITCODE
    $diagnostics = & $ctlPath diagnostics 2>&1
    $diagnosticsExit = $LASTEXITCODE
    $control = [ordered]@{
        surface_exit = $surfaceExit
        surface_output = ($surface -join "`n")
        diagnostics_exit = $diagnosticsExit
        diagnostics_output = ($diagnostics -join "`n")
    }
}

$report = [ordered]@{
    verified_at = (Get-Date).ToUniversalTime().ToString("o")
    package_dir = $PackageDir
    files = $fileReports
    catalog_signature = $signature
    driver_store_has_opena8dj = [bool]$driverStoreHasOpenA8DJ
    connected_audio8dj_usbdevice = [bool]$devicePresent
    control_tool = $control
}

$reportPath = Join-Path $reportDir "verify-report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -Path $reportPath -Encoding ASCII
$report | ConvertTo-Json -Depth 8
Write-Host "Verification report written to $reportPath"
