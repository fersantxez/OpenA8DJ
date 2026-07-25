param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$CabPath,

    [string]$CertificateThumbprint,

    [string]$CertificateSubject,

    [ValidateSet("CurrentUser", "LocalMachine")]
    [string]$StoreLocation = "CurrentUser",

    [string]$TimestampUrl = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $CabPath) {
    $CabPath = Join-Path $repoRoot "windows\dist\attestation\OpenA8DJUsb-$Configuration-$Platform-attestation.cab"
}
$CabPath = (Resolve-Path $CabPath).Path

$storePath = "Cert:\$StoreLocation\My"
$certificates = Get-ChildItem $storePath -CodeSigningCert -ErrorAction Stop |
    Where-Object { $_.HasPrivateKey }

if ($CertificateThumbprint) {
    $normalizedThumbprint = $CertificateThumbprint.Replace(" ", "").ToUpperInvariant()
    $certificates = $certificates | Where-Object { $_.Thumbprint -eq $normalizedThumbprint }
}
if ($CertificateSubject) {
    $certificates = $certificates | Where-Object { $_.Subject -like "*$CertificateSubject*" }
}

$certificate = $certificates |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $certificate) {
    throw "No code-signing certificate with private key was found in $storePath. For Microsoft attestation signing, use an organization EV code-signing certificate associated with the Hardware Dev Center account."
}

if ($certificate.Subject -like "*OpenA8DJ Windows Test Driver*" -or $certificate.Subject -like "*WDKTestCert*") {
    throw "Refusing to sign the attestation CAB with a test certificate: $($certificate.Subject). Use the organization EV code-signing certificate."
}

$signTool = Find-OpenA8DJSignTool
Write-Host "Signing attestation CAB with $($certificate.Subject)"
& $signTool sign /fd SHA256 /sha1 $certificate.Thumbprint /tr $TimestampUrl /td SHA256 /v $CabPath
if ($LASTEXITCODE -ne 0) {
    throw "signtool failed with exit code $LASTEXITCODE"
}

Write-Host "Verifying CAB signature"
& $signTool verify /pa /v $CabPath
if ($LASTEXITCODE -ne 0) {
    throw "signtool verify failed with exit code $LASTEXITCODE"
}

$signature = Get-AuthenticodeSignature -FilePath $CabPath
$manifest = [ordered]@{
    signed_at = (Get-Date).ToUniversalTime().ToString("o")
    cab = $CabPath
    cab_sha256 = Get-OpenA8DJFileHashHex -Path $CabPath
    certificate_subject = $certificate.Subject
    certificate_thumbprint = $certificate.Thumbprint
    signature_status = [string]$signature.Status
    signature_status_message = $signature.StatusMessage
}
$manifestPath = Join-Path (Split-Path -Parent $CabPath) "OpenA8DJUsb-$Configuration-$Platform-attestation-cab-signature.json"
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding ASCII
Write-Host "CAB signing manifest written to $manifestPath"
