param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [string]$PackageDir,

    [string]$OutputDir,

    [switch]$SkipBuild,

    [switch]$SkipCabExport
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

function Find-OpenA8DJInfVerif {
    $toolsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools"
    if (-not (Test-Path $toolsRoot)) {
        return $null
    }

    Get-ChildItem -Path $toolsRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object {
            Join-Path $_.FullName "x64\infverif.exe"
            Join-Path $_.FullName "arm64\infverif.exe"
        } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1
}

function Invoke-OpenA8DJReadinessStep {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Block,
        [Parameter(Mandatory = $true)][string]$LogDir
    )

    $logPath = Join-Path $LogDir "$Name.log"
    $exitCode = 0
    $output = $null
    try {
        $global:LASTEXITCODE = 0
        $output = & $Block 2>&1
        if ($global:LASTEXITCODE -ne $null) {
            $exitCode = [int]$global:LASTEXITCODE
        }
    } catch {
        $exitCode = 1
        $output = @($output; $_.Exception.Message)
    }

    if ($output -ne $null) {
        $output | Set-Content -Path $logPath -Encoding UTF8
    } else {
        "" | Set-Content -Path $logPath -Encoding UTF8
    }

    [ordered]@{
        name = $Name
        passed = ($exitCode -eq 0)
        exit_code = $exitCode
        log = $logPath
    }
}

$repoRoot = Get-OpenA8DJRepoRoot
if (-not $PackageDir) {
    $PackageDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $repoRoot "local-analysis\windows\signing-readiness-$(Get-Date -Format yyyyMMdd-HHmmss)"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path

$steps = @()
if (-not $SkipBuild) {
    $steps += Invoke-OpenA8DJReadinessStep -Name "build-driver" -LogDir $OutputDir -Block {
        & (Join-Path $PSScriptRoot "build-driver.ps1") -Configuration $Configuration -Platform $Platform
    }
}

$PackageDir = (Resolve-Path $PackageDir).Path
$requiredSubmissionFiles = @(
    "OpenA8DJUsb.inf",
    "OpenA8DJUsb.sys",
    "OpenA8DJUsb.cat",
    "OpenA8DJUsb.pdb"
)
$forbiddenSubmissionFiles = @(
    "OpenA8DJUsb-TestCertificate.cer",
    "OpenA8DJUsb.cer"
)

$fileReports = foreach ($name in ($requiredSubmissionFiles + $forbiddenSubmissionFiles)) {
    $path = Join-Path $PackageDir $name
    [ordered]@{
        name = $name
        path = $path
        exists = Test-Path $path
        sha256 = if (Test-Path $path) { Get-OpenA8DJFileHashHex -Path $path } else { $null }
        bytes = if (Test-Path $path) { (Get-Item $path).Length } else { $null }
        submission_required = $requiredSubmissionFiles -contains $name
        forbidden_in_submission_cab = $forbiddenSubmissionFiles -contains $name
    }
}

$missingRequired = @($fileReports | Where-Object { $_.submission_required -and -not $_.exists })

$infPath = Join-Path $PackageDir "OpenA8DJUsb.inf"
$catPath = Join-Path $PackageDir "OpenA8DJUsb.cat"
$cabPath = $null
$cabManifestPath = $null
$cabListing = @()
$cabExtractedFiles = @()
$expectedCabDriverFolder = "OpenA8DJUsb-$Platform"
$cabExpectedFolderPresent = $false

$infVerif = Find-OpenA8DJInfVerif
if ($infVerif -and (Test-Path $infPath)) {
    $steps += Invoke-OpenA8DJReadinessStep -Name "infverif-windows-driver" -LogDir $OutputDir -Block {
        & $infVerif /w /v $infPath
    }
    $steps += Invoke-OpenA8DJReadinessStep -Name "infverif-hardware-signature" -LogDir $OutputDir -Block {
        & $infVerif /h /v $infPath
    }
} elseif (-not $infVerif) {
    $steps += [ordered]@{
        name = "infverif-windows-driver"
        passed = $false
        exit_code = $null
        log = $null
        skipped_reason = "InfVerif.exe was not found. Install the Windows SDK/WDK tools."
    }
}

$catalogSignature = $null
if (Test-Path $catPath) {
    $catSig = Get-AuthenticodeSignature -FilePath $catPath
    $catalogSignature = [ordered]@{
        status = [string]$catSig.Status
        status_message = $catSig.StatusMessage
        signer = if ($catSig.SignerCertificate) { $catSig.SignerCertificate.Subject } else { $null }
        thumbprint = if ($catSig.SignerCertificate) { $catSig.SignerCertificate.Thumbprint } else { $null }
        is_opena8dj_test_certificate = ($catSig.SignerCertificate -and $catSig.SignerCertificate.Subject -eq "CN=OpenA8DJ Windows Test Driver")
    }
}

if (-not $SkipCabExport -and $missingRequired.Count -eq 0) {
    $steps += Invoke-OpenA8DJReadinessStep -Name "export-attestation-cab" -LogDir $OutputDir -Block {
        & (Join-Path $PSScriptRoot "export-attestation-cab.ps1") -Configuration $Configuration -Platform $Platform -PackageDir $PackageDir
    }

    $attestationDir = Join-Path $repoRoot "windows\dist\attestation"
    $cabPath = Join-Path $attestationDir "OpenA8DJUsb-$Configuration-$Platform-attestation.cab"
    $cabManifestPath = Join-Path $attestationDir "OpenA8DJUsb-$Configuration-$Platform-attestation-manifest.json"
    if (Test-Path $cabPath) {
        $cabListing = @(& expand.exe -D $cabPath 2>&1)
        $cabListing | Set-Content -Path (Join-Path $OutputDir "attestation-cab-listing.txt") -Encoding UTF8
        $cabExtractRoot = Join-Path $env:TEMP "opena8dj-cab-readiness-$([guid]::NewGuid().ToString('n'))"
        New-Item -ItemType Directory -Path $cabExtractRoot -Force | Out-Null
        try {
            $extractOutput = & expand.exe -F:* $cabPath $cabExtractRoot 2>&1
            $extractOutput | Set-Content -Path (Join-Path $OutputDir "attestation-cab-extract.log") -Encoding UTF8
            $cabExtractedFiles = @(
                Get-ChildItem -Path $cabExtractRoot -File -Recurse |
                    ForEach-Object {
                        $index = $_.FullName.IndexOf($expectedCabDriverFolder, [System.StringComparison]::OrdinalIgnoreCase)
                        if ($index -ge 0) {
                            $_.FullName.Substring($index)
                        } else {
                            $_.Name
                        }
                    }
            )
            $cabExtractedFiles | Set-Content -Path (Join-Path $OutputDir "attestation-cab-extracted-files.txt") -Encoding UTF8
            $cabExpectedFolderPresent = [bool]($cabExtractedFiles | Where-Object { $_ -like "$expectedCabDriverFolder\*" })
        } finally {
            if (Test-Path $cabExtractRoot) {
                Remove-Item -Path $cabExtractRoot -Recurse -Force
            }
        }
    }
}

$cabSignature = $null
if ($cabPath -and (Test-Path $cabPath)) {
    $cabSig = Get-AuthenticodeSignature -FilePath $cabPath
    $cabSignature = [ordered]@{
        status = [string]$cabSig.Status
        status_message = $cabSig.StatusMessage
        signer = if ($cabSig.SignerCertificate) { $cabSig.SignerCertificate.Subject } else { $null }
        thumbprint = if ($cabSig.SignerCertificate) { $cabSig.SignerCertificate.Thumbprint } else { $null }
    }
}

$cabContainsForbiddenFiles = $false
foreach ($name in $forbiddenSubmissionFiles) {
    if (($cabExtractedFiles -join "`n") -match [regex]::Escape($name)) {
        $cabContainsForbiddenFiles = $true
    }
}

$allStepsPassed = -not (@($steps | Where-Object { -not $_.passed }))
$submissionPayloadReady =
    ($missingRequired.Count -eq 0) -and
    $allStepsPassed -and
    ($cabPath -and (Test-Path $cabPath)) -and
    $cabExpectedFolderPresent -and
    (-not $cabContainsForbiddenFiles)

$partnerCenterReady =
    $submissionPayloadReady -and
    $cabSignature -and
    $cabSignature.status -eq "Valid"

$blockers = @()
foreach ($missing in $missingRequired) {
    $blockers += "Missing required submission file: $($missing.name)"
}
foreach ($step in $steps) {
    if (-not $step.passed) {
        $blockers += "Step failed: $($step.name)"
    }
}
if ($cabContainsForbiddenFiles) {
    $blockers += "CAB contains development certificate files"
}
if ($cabPath -and (Test-Path $cabPath) -and -not $cabExpectedFolderPresent) {
    $blockers += "CAB does not extract into expected driver folder: $expectedCabDriverFolder"
}
if ($submissionPayloadReady -and -not $partnerCenterReady) {
    $blockers += "CAB is not EV-signed yet; sign it before Partner Center submission"
}

$report = [ordered]@{
    generated_at = (Get-Date).ToUniversalTime().ToString("o")
    configuration = $Configuration
    platform = $Platform
    package_dir = $PackageDir
    report_dir = $OutputDir
    submission_payload_ready = [bool]$submissionPayloadReady
    partner_center_ready = [bool]$partnerCenterReady
    blockers = $blockers
    steps = $steps
    files = $fileReports
    catalog_signature = $catalogSignature
    attestation_cab = if ($cabPath -and (Test-Path $cabPath)) {
        [ordered]@{
            path = $cabPath
            sha256 = Get-OpenA8DJFileHashHex -Path $cabPath
            bytes = (Get-Item $cabPath).Length
            manifest = $cabManifestPath
            expected_driver_folder = $expectedCabDriverFolder
            expected_driver_folder_present = [bool]$cabExpectedFolderPresent
            extracted_files = $cabExtractedFiles
            contains_forbidden_development_files = [bool]$cabContainsForbiddenFiles
            signature = $cabSignature
        }
    } else {
        $null
    }
}

$reportPath = Join-Path $OutputDir "signing-readiness-report.json"
$summaryPath = Join-Path $OutputDir "signing-readiness-summary.txt"
$report | ConvertTo-Json -Depth 8 | Set-Content -Path $reportPath -Encoding ASCII
@(
    "OpenA8DJ Windows signing readiness",
    "generated_at=$($report.generated_at)",
    "configuration=$Configuration",
    "platform=$Platform",
    "submission_payload_ready=$submissionPayloadReady",
    "partner_center_ready=$partnerCenterReady",
    "cab=$cabPath",
    "report=$reportPath",
    "blockers:",
    $(if ($blockers.Count -eq 0) { "  none" } else { $blockers | ForEach-Object { "  $_" } })
) | Set-Content -Path $summaryPath -Encoding ASCII

$report | ConvertTo-Json -Depth 8
Write-Host "Signing readiness report written to $reportPath"
Write-Host "Signing readiness summary written to $summaryPath"

if (-not $submissionPayloadReady) {
    exit 1
}
