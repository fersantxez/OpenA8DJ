param(
    [string]$StatePath
)

$ErrorActionPreference = 'Continue'
if (-not $StatePath) {
    $StatePath = Join-Path $PSScriptRoot 'recovery-state.json'
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'OpenA8DJ recovery must run elevated.'
}

$state = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace([string]$state.instance_id) -or
    [string]::IsNullOrWhiteSpace([string]$state.published_name)) {
    throw "Recovery state is incomplete: $StatePath"
}
$reportPath = [IO.Path]::ChangeExtension($StatePath, '.recovery.json')
$actions = @()
$checkpointBeforeCleanup = $null
try {
    $parameters = Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx\Parameters' -ErrorAction Stop
    $checkpointBeforeCleanup = [ordered]@{
        last_safety_checkpoint = $parameters.LastSafetyCheckpoint
        last_safety_sequence = $parameters.LastSafetySequence
        last_safety_status = $parameters.LastSafetyStatus
        canary_phase = $parameters.CanaryPhase
        operations_remaining = $parameters.CanaryOperationsRemaining
    }
} catch {}

try {
    & pnputil.exe /disable-device ([string]$state.instance_id) /force | Out-String | ForEach-Object {
        $actions += [ordered]@{ action = 'disable-device'; exit = $LASTEXITCODE; output = $_ }
    }
} catch {
    $actions += [ordered]@{ action = 'disable-device'; error = $_.Exception.Message }
}

try {
    & sc.exe config OpenA8DJUsbAcx start= disabled | Out-String | ForEach-Object {
        $actions += [ordered]@{ action = 'disable-service'; exit = $LASTEXITCODE; output = $_ }
    }
} catch {
    $actions += [ordered]@{ action = 'disable-service'; error = $_.Exception.Message }
}

if ($state.published_name -match '^oem\d+\.inf$') {
    try {
        & pnputil.exe /delete-driver ([string]$state.published_name) /uninstall /force | Out-String | ForEach-Object {
            $actions += [ordered]@{ action = 'delete-driver'; exit = $LASTEXITCODE; output = $_ }
        }
    } catch {
        $actions += [ordered]@{ action = 'delete-driver'; error = $_.Exception.Message }
    }
}

try {
    & sc.exe delete OpenA8DJUsbAcx | Out-String | ForEach-Object {
        $actions += [ordered]@{ action = 'delete-service'; exit = $LASTEXITCODE; output = $_ }
    }
} catch {
    $actions += [ordered]@{ action = 'delete-service'; error = $_.Exception.Message }
}

try {
    # Package removal may rebind and enable the vendor driver. The final state
    # must therefore be asserted after all package/service cleanup and PnP
    # settling.
    for ($attempt = 1; $attempt -le 5; $attempt++) {
        & pnputil.exe /disable-device ([string]$state.instance_id) /force | Out-String | ForEach-Object {
            $actions += [ordered]@{ action = 'disable-device-final'; attempt = $attempt; exit = $LASTEXITCODE; output = $_ }
        }
        Start-Sleep -Seconds 1
        $settledDevice = Get-PnpDevice -InstanceId ([string]$state.instance_id) -ErrorAction SilentlyContinue
        if ($settledDevice -and [string]$settledDevice.Problem -eq 'CM_PROB_DISABLED') {
            Start-Sleep -Seconds 1
            $settledDevice = Get-PnpDevice -InstanceId ([string]$state.instance_id) -ErrorAction SilentlyContinue
            if ($settledDevice -and [string]$settledDevice.Problem -eq 'CM_PROB_DISABLED') { break }
        }
    }
} catch {
    $actions += [ordered]@{ action = 'disable-device-final'; error = $_.Exception.Message }
}

$device = Get-PnpDevice -InstanceId ([string]$state.instance_id) -ErrorAction SilentlyContinue
$driverStoreText = (& pnputil.exe /enum-drivers | Out-String)
$packageStillPresent = $driverStoreText -match '(?im)^\s*Original Name\s*:\s*OpenA8DJUsb\.inf\s*$'
$serviceStillPresent = [bool](Get-Service OpenA8DJUsbAcx -ErrorAction SilentlyContinue)
$recoverySucceeded = $device -and
    [string]$device.Problem -eq 'CM_PROB_DISABLED' -and
    -not $packageStillPresent -and
    -not $serviceStillPresent
$report = [ordered]@{
    recovered_at_utc = (Get-Date).ToUniversalTime().ToString('o')
    state_path = $StatePath
    actions = $actions
    driver_checkpoint_before_cleanup = $checkpointBeforeCleanup
    final_device_status = if ($device) { [string]$device.Status } else { $null }
    final_device_problem = if ($device) { [string]$device.Problem } else { $null }
    package_still_present = $packageStillPresent
    service_still_present = $serviceStillPresent
    recovery_succeeded = [bool]$recoverySucceeded
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
if ($state.artifact_recovery_report) {
    try {
        $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath ([string]$state.artifact_recovery_report) -Encoding UTF8
    } catch {}
}
if ($recoverySucceeded -and $state.recovery_task_name) {
    & schtasks.exe /Delete /TN ([string]$state.recovery_task_name) /F | Out-Null
}
if (-not $recoverySucceeded) { exit 2 }
