param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDir
)

$ErrorActionPreference = "Stop"
$ArtifactDir = (Resolve-Path -LiteralPath $ArtifactDir).Path
$checkpointPath = Join-Path $ArtifactDir "checkpoints.jsonl"
if (-not (Test-Path -LiteralPath $checkpointPath)) {
    throw "Checkpoint log not found: $checkpointPath"
}

function Get-BootTimeUtc {
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem
        return ([datetime]$os.LastBootUpTime).ToUniversalTime().ToString("o")
    } catch {
        return $null
    }
}

function Get-PhysicalAudio8DjUsbDevices {
    @(Get-PnpDevice -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -like "USB\VID_17CC&PID_1978*" } |
        ForEach-Object { [string]$_.InstanceId })
}

function Get-EventsSince {
    param(
        [Parameter(Mandatory = $true)][datetime]$Since,
        [Parameter(Mandatory = $true)][int[]]$Ids
    )

    try {
        return @(Get-WinEvent -FilterHashtable @{
            LogName = "System"
            Id = $Ids
            StartTime = $Since
        } -ErrorAction SilentlyContinue | ForEach-Object {
            [ordered]@{
                time_created = $_.TimeCreated.ToUniversalTime().ToString("o")
                id = $_.Id
                provider = $_.ProviderName
                message = $_.Message
            }
        })
    } catch {
        return @()
    }
}

$checkpoints = @(
    Get-Content -LiteralPath $checkpointPath |
        Where-Object { $_.Trim().Length -gt 0 } |
        ForEach-Object { $_ | ConvertFrom-Json }
)
if ($checkpoints.Count -eq 0) {
    throw "Checkpoint log is empty: $checkpointPath"
}

$first = $checkpoints | Sort-Object sequence | Select-Object -First 1
$last = $checkpoints | Sort-Object sequence | Select-Object -Last 1
$failedCheckpoints = @($checkpoints | Where-Object { $_.status -eq "failed" } | Sort-Object sequence)
$lastFailure = $failedCheckpoints | Select-Object -Last 1
$firstTime = [datetime]$first.timestamp_utc
$nextByStage = @{
    "preflight-start" = "package-validated"
    "package-validated" = "pnp-preflight-snapshot"
    "pnp-preflight-snapshot" = "install-authority-verified or preflight-complete"
    "install-authority-verified" = "install-start"
    "install-start" = "install-command-returned"
    "install-command-returned" = "published-driver-resolved"
    "published-driver-resolved" = "device-create-start"
    "device-creation-tool-verified" = "device-create-start"
    "device-create-start" = "device-create-returned"
    "device-create-returned" = "pnp-postinstall-snapshot"
    "pnp-postinstall-snapshot" = "probe-start or cleanup-start"
    "probe-start" = "probe-returned"
    "probe-returned" = "cleanup-start"
    "cleanup-start" = "remove-device-returned"
    "remove-device-returned" = "delete-driver-returned"
    "delete-driver-returned" = "cleanup-pnp-snapshot"
    "cleanup-pnp-snapshot" = "complete"
    "exception" = "inspect the preceding checkpoint and cleanup artifacts"
    "preflight-complete" = "none; preflight only"
    "complete" = "none"
}

$virtualDevices = @(
    Get-PnpDevice -PresentOnly:$false -ErrorAction SilentlyContinue |
        Where-Object {
            $_.InstanceId -like "ROOT\OpenA8DJVirtual*" -or
            $_.FriendlyName -eq "OpenA8DJ Virtual ACX Proof Endpoint"
        } |
        ForEach-Object {
            [ordered]@{
                status = [string]$_.Status
                problem = [string]$_.Problem
                class = [string]$_.Class
                instance_id = [string]$_.InstanceId
                friendly_name = [string]$_.FriendlyName
            }
        }
)
$driverServices = @(
    Get-CimInstance -ClassName Win32_SystemDriver -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "OpenA8DJVirtual" } |
        ForEach-Object {
            [ordered]@{
                name = $_.Name
                state = $_.State
                status = $_.Status
                start_mode = $_.StartMode
                path_name = $_.PathName
            }
        }
)
$bugchecks = Get-EventsSince -Since $firstTime -Ids @(1001, 41)
$currentBoot = Get-BootTimeUtc
$physicalUsbDevicesNow = @(Get-PhysicalAudio8DjUsbDevices)
$summaryPath = Join-Path $ArtifactDir "summary.json"
$summary = [ordered]@{
    analyzed_at_utc = (Get-Date).ToUniversalTime().ToString("o")
    artifact_dir = $ArtifactDir
    checkpoint_path = $checkpointPath
    summary_exists = Test-Path -LiteralPath $summaryPath
    first_checkpoint = $first
    last_checkpoint = $last
    last_failed_checkpoint = $lastFailure
    expected_next_stage = $nextByStage[[string]$last.stage]
    current_boot_time_utc = $currentBoot
    physical_audio8dj_usb_devices_now = $physicalUsbDevicesNow
    virtual_devices_now = $virtualDevices
    driver_services_now = $driverServices
    system_events_since_first_checkpoint = $bugchecks
    diagnosis = if ($lastFailure) {
        "A failure was recorded at '$($lastFailure.stage)' ($($lastFailure.status)); the last durable checkpoint is '$($last.stage)'. Expected next stage: '$($nextByStage[[string]$last.stage])'."
    } elseif ([string]$last.stage -eq "complete") {
        "Canary reached its durable complete checkpoint. No crash is inferred from checkpoint state."
    } elseif ([string]$last.stage -eq "preflight-complete") {
        "Canary only performed preflight; no installation was attempted."
    } else {
        "Execution stopped after '$($last.stage)' ($($last.status)); inspect the next expected stage and system events."
    }
}
$summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $ArtifactDir "recovery-analysis.json") -Encoding UTF8
$summary | ConvertTo-Json -Depth 12

if ([string]$last.stage -notin @("complete", "preflight-complete")) {
    exit 2
}
exit 0
