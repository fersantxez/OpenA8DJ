param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [string]$ExpectedDriverVersion = "0.0.134.0",

    [int]$WaitSeconds = 10,

    [switch]$DisableDeviceOnProblem
)

$ErrorActionPreference = "Stop"

$instanceId = "USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___"

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]$identity
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run this canary from an elevated PowerShell. Current user is $($identity.Name)."
    }
}

function Get-DriverVer {
    param([Parameter(Mandatory = $true)][string]$InfPath)

    foreach ($line in Get-Content -LiteralPath $InfPath) {
        if ($line -match '^DriverVer\s*=\s*([^,]+),(.+)$') {
            return $Matches[2].Trim()
        }
    }
    throw "DriverVer not found in $InfPath"
}

function Get-BugCheckEventsSince {
    param([datetime]$Since)

    Get-WinEvent -FilterHashtable @{
        LogName = "System"
        Id = 1001
        StartTime = $Since
    } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -eq "Microsoft-Windows-WER-SystemErrorReporting" }
}

Assert-Administrator

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$infPath = Join-Path $PackageDir "OpenA8DJUsb.inf"
$sysPath = Join-Path $PackageDir "OpenA8DJUsb.sys"
$catPath = Join-Path $PackageDir "opena8djusb.cat"
if (-not (Test-Path -LiteralPath $catPath)) {
    $catPath = Join-Path $PackageDir "OpenA8DJUsb.cat"
}

foreach ($path in @($infPath, $sysPath, $catPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required package file not found: $path"
    }
}

$driverVersion = Get-DriverVer -InfPath $infPath
if ($driverVersion -ne $ExpectedDriverVersion) {
    throw "Refusing canary for DriverVer $driverVersion. Expected $ExpectedDriverVersion."
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$outDir = Join-Path $repoRoot "local-analysis\windows-a8dj-load-canary-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

@(
    "safety_policy=driver_load_canary_only"
    "does_not_run_opena8djctl=1"
    "does_not_open_audio_endpoints=1"
    "does_not_start_streaming=1"
    "does_not_play_audio=1"
    "does_not_touch_traktor=1"
    "package_dir=$PackageDir"
    "expected_driver_version=$ExpectedDriverVersion"
) | Set-Content -Path (Join-Path $outDir "safety.txt") -Encoding ASCII

$startedAt = Get-Date
$preDevice = Get-PnpDevice -InstanceId $instanceId -ErrorAction SilentlyContinue
$preDevice | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-before.txt") -Encoding UTF8

Write-Host "Installing OpenA8DJ load-canary package:"
Write-Host "  $infPath"
Write-Host "This canary stops after PnP load/status checks; it does not open audio streams."

& pnputil.exe /add-driver $infPath /install *> (Join-Path $outDir "pnputil-install.txt")
$pnputilExit = $LASTEXITCODE
if (@(0, 259, 3010) -notcontains $pnputilExit) {
    throw "pnputil failed with exit code $pnputilExit. See $outDir\pnputil-install.txt"
}

Start-Sleep -Seconds $WaitSeconds

$postDevice = Get-PnpDevice -InstanceId $instanceId -ErrorAction SilentlyContinue
$postDevice | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-after.txt") -Encoding UTF8
pnputil.exe /enum-devices /instanceid $instanceId *> (Join-Path $outDir "pnputil-device-after.txt")

$bugChecks = @(Get-BugCheckEventsSince -Since $startedAt)
$bugChecks |
    Select-Object TimeCreated, Id, ProviderName, Message |
    ConvertTo-Json -Depth 4 |
    Set-Content -Path (Join-Path $outDir "bugchecks-since-start.json") -Encoding UTF8

$problem = if ($postDevice) { [string]$postDevice.Problem } else { "DEVICE_NOT_FOUND" }
$status = if ($postDevice) { [string]$postDevice.Status } else { "Missing" }

if ($DisableDeviceOnProblem -and $postDevice -and $postDevice.Problem -ne "CM_PROB_NONE") {
    Disable-PnpDevice -InstanceId $instanceId -Confirm:$false
    $postDevice = Get-PnpDevice -InstanceId $instanceId -ErrorAction SilentlyContinue
    $postDevice | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-after-disable.txt") -Encoding UTF8
}

$summary = [ordered]@{
    started_at = $startedAt.ToUniversalTime().ToString("o")
    finished_at = (Get-Date).ToUniversalTime().ToString("o")
    package_dir = $PackageDir
    inf_path = $infPath
    sys_path = $sysPath
    cat_path = $catPath
    expected_driver_version = $ExpectedDriverVersion
    actual_driver_version = $driverVersion
    pnputil_exit = $pnputilExit
    pnp_status = $status
    pnp_problem = $problem
    bugcheck_events_since_start = $bugChecks.Count
    disabled_on_problem = [bool]$DisableDeviceOnProblem
    artifact_dir = $outDir
}
$summary | ConvertTo-Json -Depth 4 | Tee-Object -FilePath (Join-Path $outDir "summary.json")

if ($bugChecks.Count -ne 0) {
    throw "Bugcheck event appeared during canary. Stop. Artifacts: $outDir"
}
if (-not $postDevice) {
    throw "Audio 8 DJ device not found after canary. Artifacts: $outDir"
}
if ($postDevice.Problem -ne "CM_PROB_NONE" -or $postDevice.Status -ne "OK") {
    throw "Canary did not reach clean PnP state: Status=$status Problem=$problem. Artifacts: $outDir"
}

Write-Host "OpenA8DJ load canary passed. Artifacts: $outDir"
