param(
    [int]$Seconds = 6,
    [int]$Rate = 48000,
    [int]$BlockSize = 512,
    [string]$Latency = "high",
    [double]$Amplitude = 0.10,
    [ValidateSet("full", "all-pairs-only")]
    [string]$Mode = "full",
    [string]$InputHostApi = "MME",
    [string]$OutputHostApi = "MME",
    [string]$InputName = "Line In (iRig Stream)",
    [string]$OutputName = "Audio 8 DJ (8ch Out)",
    [ValidateRange(0, 10)]
    [int]$StatusRetries = 0,
    [ValidateRange(1, 120)]
    [int]$WatchdogGraceSeconds = 15,
    [ValidateRange(0, 60)]
    [int]$InterCaseCooldownSeconds = 2,
    [switch]$AllowSoftFailures,
    [int]$PnpMonitorIntervalSeconds = 10,
    [switch]$WriteWavs,
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$script = Join-Path $repoRoot "windows\tests\a8dj_pair_matrix_probe.py"
$ctl = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"

if (-not (Test-Path $Python)) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        throw "Python was not found. Pass -Python or install Python."
    }
    $Python = $pythonCommand.Source
}

if (-not (Test-Path $ctl)) {
    throw "opena8djctl.exe not found at $ctl"
}

function New-RunDirectory {
    param([string]$Requested)

    if ($Requested) {
        New-Item -ItemType Directory -Path $Requested -Force | Out-Null
        return (Resolve-Path $Requested).Path
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $path = Join-Path $repoRoot "local-analysis\windows-a8dj-pair-matrix-$stamp"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return $path
}

function Write-PnpSnapshot {
    param(
        [string]$Path,
        [string]$Label
    )

    $devices = Get-PnpDevice -PresentOnly |
        Where-Object {
            $_.FriendlyName -match 'iRig|Audio 8 DJ|IK Multimedia|Native Instruments' -or
            $_.InstanceId -match 'VID_1963&PID_0059|VID_17CC&PID_1978'
        } |
        Sort-Object Class,FriendlyName,InstanceId |
        Select-Object Class,Status,FriendlyName,InstanceId

    @(
        "snapshot=$Label"
        "time=$(Get-Date -Format o)"
        "note=read_only_pnp_enumeration_no_usb_reset_no_device_restart"
        ""
        ($devices | Format-Table -AutoSize | Out-String)
    ) | Set-Content -Path $Path -Encoding UTF8
}

function Test-RequiredHardware {
    param([string]$RunDir)

    $present = Get-PnpDevice -PresentOnly
    $irigUsb = $present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' }
    $irigEndpoint = $present | Where-Object { $_.FriendlyName -eq $InputName }
    $audio8Usb = $present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' }
    $audio8Out = $present | Where-Object {
        $_.FriendlyName -like "$OutputName*" -or
        ($OutputName -match 'Audio 8 DJ' -and $_.FriendlyName -like 'Audio 8 DJ (*Out)*')
    }

    $lines = @(
        "hardware_preflight_time=$(Get-Date -Format o)"
        "found_irig_usb=$([int][bool]$irigUsb)"
        "found_irig_capture_endpoint=$([int][bool]$irigEndpoint)"
        "found_audio8_usb=$([int][bool]$audio8Usb)"
        "requested_output_name=$OutputName"
        "found_audio8_output_endpoint=$([int][bool]$audio8Out)"
        "policy=block_if_missing_do_not_recover_unattended"
    )
    $lines | Set-Content -Path (Join-Path $RunDir "hardware-preflight.txt") -Encoding UTF8

    if (-not $irigUsb -or -not $irigEndpoint -or -not $audio8Usb -or -not $audio8Out) {
        throw "Required audio hardware is not stable/visible; see $(Join-Path $RunDir 'hardware-preflight.txt')"
    }
}

function Start-PnpMonitor {
    param(
        [string]$Path,
        [int]$Seconds,
        [int]$IntervalSeconds = 10
    )

    Start-Job -ScriptBlock {
        param($Path, $Seconds, $IntervalSeconds, $InputName)
        $deadline = (Get-Date).AddSeconds($Seconds)
        "time`tirig_usb`tirig_capture_endpoint`taudio8_usb`taudio8_output_endpoint" |
            Set-Content -Path $Path -Encoding UTF8

        while ((Get-Date) -lt $deadline) {
            $present = Get-PnpDevice -PresentOnly
            $irigUsb = [int][bool]($present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' })
            $irigEndpoint = [int][bool]($present | Where-Object { $_.FriendlyName -eq $InputName })
            $audio8Usb = [int][bool]($present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' })
            $audio8Out = [int][bool]($present | Where-Object { $_.FriendlyName -like 'Speakers (Audio 8 DJ)*' })
            "$(Get-Date -Format o)`t$irigUsb`t$irigEndpoint`t$audio8Usb`t$audio8Out" |
                Add-Content -Path $Path -Encoding UTF8
            Start-Sleep -Seconds $IntervalSeconds
        }
    } -ArgumentList $Path, $Seconds, $IntervalSeconds, $InputName
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-a8dj-pair-matrix.lock"
$lockTaken = $false
$monitorJob = $null

try {
    if (Test-Path $lockPath) {
        $existing = Get-Content -Raw -Path $lockPath -ErrorAction SilentlyContinue
        throw "Another Windows Audio 8 DJ matrix run may be active: $lockPath $existing"
    }

    @(
        "pid=$PID"
        "started=$(Get-Date -Format o)"
        "run_dir=$OutDir"
        "resource=Audio 8 DJ and iRig Stream on shared USB hub"
    ) | Set-Content -Path $lockPath -Encoding UTF8
    $lockTaken = $true

    @(
        "safety_policy=conservative_unattended_shared_usb_hub"
        "does_not_reset_usb=1"
        "does_not_disable_or_enable_pnp_devices=1"
        "does_not_restart_windows_audio=1"
        "does_not_change_default_audio_devices=1"
        "does_not_attempt_irig_recovery=1"
        "blocks_if_irig_or_audio8_missing_before_start=1"
        "records_pnp_before_during_after=1"
        "never_arms_or_attempts_iso_silence=1"
        "pnp_monitor_interval_seconds=$PnpMonitorIntervalSeconds"
        "status_retries=$StatusRetries"
        "watchdog_grace_seconds=$WatchdogGraceSeconds"
        "inter_case_cooldown_seconds=$InterCaseCooldownSeconds"
        "strict_failure_policy=$([int](-not $AllowSoftFailures))"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-before.txt") -Label "before"
    Test-RequiredHardware -RunDir $OutDir
    $monitorSeconds = [Math]::Max(($Seconds + 1) * ($(if ($Mode -eq "full") { 5 } else { 1 })) + 20, 30)
    $monitorJob = Start-PnpMonitor -Path (Join-Path $OutDir "pnp-monitor.tsv") -Seconds $monitorSeconds -IntervalSeconds $PnpMonitorIntervalSeconds

    $argsList = @(
        $script,
        "--seconds", $Seconds,
        "--rate", $Rate,
        "--blocksize", $BlockSize,
        "--latency", $Latency,
        "--amplitude", $Amplitude,
        "--mode", $Mode,
        "--input-hostapi", $InputHostApi,
        "--output-hostapi", $OutputHostApi,
        "--input-name", $InputName,
        "--output-name", $OutputName,
        "--status-retries", $StatusRetries,
        "--watchdog-grace-seconds", $WatchdogGraceSeconds,
        "--inter-case-cooldown-seconds", $InterCaseCooldownSeconds,
        "--out-dir", $OutDir
    )
    if (-not $AllowSoftFailures) {
        $argsList += "--strict"
    }
    if ($WriteWavs) {
        $argsList += "--write-wavs"
    }

    & $Python @argsList
    if ($LASTEXITCODE -ne 0) {
        throw "Audio 8 DJ pair matrix probe failed with exit code $LASTEXITCODE"
    }
}
finally {
    if ($monitorJob) {
        Wait-Job $monitorJob -Timeout 5 | Out-Null
        Stop-Job $monitorJob -ErrorAction SilentlyContinue
        Remove-Job $monitorJob -Force -ErrorAction SilentlyContinue
    }

    if (Test-Path $OutDir) {
        Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-after.txt") -Label "after"
    }

    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -Path $lockPath -Force
    }
}
