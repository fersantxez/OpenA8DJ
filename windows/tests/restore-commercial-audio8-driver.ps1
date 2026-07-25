param(
    [string]$BackupDir,
    [string]$PublishedInf = "C:\Windows\INF\oem44.inf"
)

$ErrorActionPreference = "Stop"

$instanceId = "USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___"
$hardwareId = "USB\VID_17CC&PID_1978"
$devcon = "C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\devcon.exe"

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]$identity
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "This restore must run from an elevated PowerShell. Current user is $($identity.Name)."
}

if (-not $BackupDir -and -not (Test-Path $PublishedInf)) {
    $repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
    $latest = Get-ChildItem -Path (Join-Path $repoRoot "local-analysis") -Directory |
        Where-Object { $_.Name -like "windows-commercial-driver-backup-*" } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if (-not $latest) {
        throw "No commercial driver backup found under local-analysis."
    }
    $BackupDir = $latest.FullName
}

if ($BackupDir) {
    $inf = Join-Path $BackupDir "a8djusb.inf"
    if (-not (Test-Path $inf)) {
        throw "Missing commercial driver INF: $inf"
    }
} else {
    $inf = $PublishedInf
}

Write-Host "Restoring Native Instruments Audio 8 DJ commercial driver:"
Write-Host "  $inf"

if (Test-Path $devcon) {
    & $devcon update $inf $hardwareId
} else {
    pnputil /add-driver $inf /install
}
if ($LASTEXITCODE -ne 0) {
    throw "Commercial driver restore failed with exit code $LASTEXITCODE"
}

Start-Sleep -Seconds 5
$pnp = pnputil /enum-devices /instanceid $instanceId
$pnp
$pnpText = $pnp -join "`n"
if ($pnpText -notmatch "Manufacturer Name:\s+Native Instruments" -or
    $pnpText -notmatch "Status:\s+Started") {
    throw "Commercial driver did not return to Native Instruments/Started."
}

try {
    Import-Module AudioDeviceCmdlets -ErrorAction Stop
    $mic = Get-AudioDevice -List |
        Where-Object { $_.Type -eq "Recording" -and $_.Name -like "Microphone Array*" } |
        Select-Object -First 1
    $realtek = Get-AudioDevice -List |
        Where-Object { $_.Type -eq "Playback" -and $_.Name -like "Speakers (Realtek*" } |
        Select-Object -First 1

    if ($mic) {
        Set-AudioDevice -Index $mic.Index | Out-Null
        Set-AudioDevice -Index $mic.Index -CommunicationOnly | Out-Null
    }
    if ($realtek) {
        Set-AudioDevice -Index $realtek.Index | Out-Null
        Set-AudioDevice -Index $realtek.Index -CommunicationOnly | Out-Null
    }
} catch {
    Write-Warning "Could not restore Realtek defaults: $($_.Exception.Message)"
}

Write-Host "Commercial Audio 8 DJ driver restored and safe audio defaults applied."
