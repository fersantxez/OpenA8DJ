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

function Get-PhysicalDeviceSnapshot {
    param([Parameter(Mandatory = $true)][string]$InstanceId)

    $device = Get-PnpDevice -InstanceId $InstanceId -ErrorAction SilentlyContinue
    if (-not $device) {
        return $null
    }
    $props = @(Get-PnpDeviceProperty -InstanceId $InstanceId -KeyName @(
        "DEVPKEY_Device_DriverInfPath",
        "DEVPKEY_Device_DriverVersion"
    ) -ErrorAction SilentlyContinue)
    return [ordered]@{
        instance_id = [string]$device.InstanceId
        present = [bool]$device.Present
        status = [string]$device.Status
        problem = [string]$device.Problem
        friendly_name = [string]$device.FriendlyName
        service = [string]$device.Service
        driver_inf = [string](($props | Where-Object KeyName -eq "DEVPKEY_Device_DriverInfPath").Data)
        driver_version = [string](($props | Where-Object KeyName -eq "DEVPKEY_Device_DriverVersion").Data)
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
    "preflight" = "stage-start"
    "stage-start" = "driverstore-verified"
    "driverstore-verified" = "boot-recovery-armed or package-removed"
    "boot-recovery-armed" = "physical-bind-load-start"
    "physical-bind-load-start" = "physical-bind-returned or crash recovery"
    "physical-bind-returned" = "physical-enable-start or physical-load-returned"
    "physical-enable-start" = "physical-load-returned"
    "physical-load-returned" = "loaded-binary-verified-inert"
    "loaded-binary-verified-inert" = "arm-start or device-disabled"
    "arm-start" = "operation-start"
    "operation-start" = "operation-returned"
    "operation-returned" = "device-disabled"
    "device-disabled" = "package-removed"
    "package-removed" = "none"
    "exception" = "inspect the preceding checkpoint and rollback artifacts"
    "complete" = "none"
}

$instanceId = if ($first.data.device.instance_id) { [string]$first.data.device.instance_id } else { 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___' }
$physicalNow = Get-PhysicalDeviceSnapshot -InstanceId $instanceId
$driverServices = @(
    Get-CimInstance -ClassName Win32_SystemDriver -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq "OpenA8DJUsbAcx" } |
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
$bugchecks = Get-EventsSince -Since $firstTime -Ids @(1001, 41, 6008)
$bugcheck7eSeen = @($bugchecks | Where-Object { $_.message -match "0x0000007e" }).Count -gt 0
$currentBoot = Get-BootTimeUtc
$summaryPath = Join-Path $ArtifactDir "summary.json"
$recoveryReportPath = [IO.Path]::ChangeExtension((Join-Path $ArtifactDir 'recovery-state.json'), '.recovery.json')
$recoveryReport = if (Test-Path -LiteralPath $recoveryReportPath) { Get-Content -LiteralPath $recoveryReportPath -Raw | ConvertFrom-Json } else { $null }
$driverCheckpoint = $null
try {
    $parameters = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx\Parameters' -ErrorAction Stop
    $driverCheckpoint = [ordered]@{
        last_safety_checkpoint = $parameters.LastSafetyCheckpoint
        last_safety_sequence = $parameters.LastSafetySequence
        last_safety_status = $parameters.LastSafetyStatus
        canary_phase = $parameters.CanaryPhase
        operations_remaining = $parameters.CanaryOperationsRemaining
    }
} catch {
    if ($recoveryReport) { $driverCheckpoint = $recoveryReport.driver_checkpoint_before_cleanup }
}
$dumps = @(Get-ChildItem (Join-Path $env:windir 'Minidump') -Filter '*.dmp' -ErrorAction SilentlyContinue |
    Where-Object CreationTimeUtc -ge $firstTime.ToUniversalTime() | Sort-Object CreationTimeUtc)
$latestDump = $dumps | Select-Object -Last 1
if ($latestDump) {
    $cdb = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\cdb.exe'
    if (Test-Path -LiteralPath $cdb) {
        & $cdb -z $latestDump.FullName -y 'srv*' -c '.bugcheck; kv; lmvm OpenA8DJUsb; !wdfkd.wdflogdump OpenA8DJUsb 0x1; q' |
            Set-Content -LiteralPath (Join-Path $ArtifactDir 'bugcheck-and-ifr.txt') -Encoding UTF8
    }
}
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
    physical_device_now = $physicalNow
    driver_services_now = $driverServices
    system_events_since_first_checkpoint = $bugchecks
    driver_checkpoint = $driverCheckpoint
    recovery_report = $recoveryReport
    minidumps_since_first_checkpoint = @($dumps | ForEach-Object { [ordered]@{ path = $_.FullName; created_utc = $_.CreationTimeUtc.ToString('o') } })
    bugcheck_0x7e_seen = $bugcheck7eSeen
    diagnosis = if ($bugcheck7eSeen) {
        "Bugcheck 0x0000007e was recorded after the first checkpoint; correlate its timestamp with the last durable checkpoint and process logs."
    } elseif ($bugchecks.Count -gt 0) {
        "A system crash or unexpected shutdown event was recorded after the first checkpoint; correlate its timestamp with the last durable checkpoint and process logs."
    } elseif ($lastFailure) {
        "A failure was recorded at '$($lastFailure.stage)' ($($lastFailure.status)); the last durable checkpoint is '$($last.stage)'. Expected next stage: '$($nextByStage[[string]$last.stage])'."
    } elseif ([string]$last.stage -eq "package-removed" -and [string]$last.status -eq 'ok') {
        "Canary completed cleanup and removed the candidate package. No crash is inferred from checkpoint state."
    } else {
        "Execution stopped after '$($last.stage)' ($($last.status)); inspect the next expected stage and system events."
    }
}
$summary | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath (Join-Path $ArtifactDir "recovery-analysis.json") -Encoding UTF8
$summary | ConvertTo-Json -Depth 16

if ([string]$last.stage -ne "package-removed" -or [string]$last.status -ne 'ok') {
    exit 2
}
exit 0
