param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$PackageDir,

    [string]$CertificateSubject = "CN=OpenA8DJ Windows Test Driver",

    [string]$PfxPath,

    [string]$PfxPassword,

    [switch]$CreateTestCertificate
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

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

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $PackageDir) {
    $PackageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
}
$PackageDir = (Resolve-Path $PackageDir).Path
$catPath = Join-Path $PackageDir "OpenA8DJUsb.cat"
$certExportPath = Join-Path $PackageDir "OpenA8DJUsb-TestCertificate.cer"
$legacyCertExportPath = Join-Path $PackageDir "OpenA8DJUsb.cer"

if (-not (Test-Path $catPath)) {
    throw "Catalog not found: $catPath. Run windows\scripts\build-driver.ps1 first."
}

$signTool = Find-OpenA8DJSignTool
$certificate = $null

if ($PfxPath) {
    if (-not (Test-Path $PfxPath)) {
        throw "PFX not found: $PfxPath"
    }
    if (-not $PfxPassword) {
        throw "-PfxPassword is required when -PfxPath is used."
    }
    Write-Host "Signing catalog with PFX $PfxPath"
    & $signTool sign /fd SHA256 /f $PfxPath /p $PfxPassword /tr http://timestamp.digicert.com /td SHA256 $catPath
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed with exit code $LASTEXITCODE"
    }
} else {
    $certificate = Get-ChildItem Cert:\CurrentUser\My |
        Where-Object { $_.Subject -eq $CertificateSubject -and $_.HasPrivateKey } |
        Sort-Object NotAfter -Descending |
        Select-Object -First 1

    if (-not $certificate -and $CreateTestCertificate) {
        Write-Host "Creating test certificate $CertificateSubject"
        $certificate = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $CertificateSubject `
            -CertStoreLocation Cert:\CurrentUser\My `
            -KeyAlgorithm RSA `
            -KeyLength 3072 `
            -HashAlgorithm SHA256 `
            -NotAfter (Get-Date).AddYears(3)
    }

    if (-not $certificate) {
        throw "No certificate found for $CertificateSubject. Use -CreateTestCertificate or provide -PfxPath."
    }

    Export-Certificate -Cert $certificate -FilePath $certExportPath -Force | Out-Null
    Copy-Item -Path $certExportPath -Destination $legacyCertExportPath -Force
    Add-OpenA8DJCertificateToStore -CertificatePath $certExportPath -StoreName Root -StoreLocation CurrentUser
    Add-OpenA8DJCertificateToStore -CertificatePath $certExportPath -StoreName TrustedPublisher -StoreLocation CurrentUser
    Write-Host "Signing catalog with CurrentUser certificate $($certificate.Thumbprint)"
    & $signTool sign /fd SHA256 /sha1 $certificate.Thumbprint /tr http://timestamp.digicert.com /td SHA256 $catPath
    if ($LASTEXITCODE -ne 0) {
        throw "signtool failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Verifying signature"
& $signTool verify /pa /v $catPath
if ($LASTEXITCODE -ne 0) {
    throw "signtool verify failed with exit code $LASTEXITCODE"
}

$manifest = [ordered]@{
    signed_at = (Get-Date).ToUniversalTime().ToString("o")
    package_dir = $PackageDir
    catalog = $catPath
    catalog_sha256 = Get-OpenA8DJFileHashHex -Path $catPath
    certificate_subject = if ($certificate) { $certificate.Subject } else { "pfx" }
    certificate_thumbprint = if ($certificate) { $certificate.Thumbprint } else { $null }
    certificate_export = if (Test-Path $certExportPath) { $certExportPath } else { $null }
    legacy_certificate_export = if (Test-Path $legacyCertExportPath) { $legacyCertExportPath } else { $null }
}
$manifestPath = Join-Path $PackageDir "signing-manifest.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding ASCII
Write-Host "Signing manifest written to $manifestPath"
