param(
    [int]$Seconds = 3,
    [int]$Rate = 48000,
    [int]$BlockSize = 512,
    [int]$Channels = 8,
    [double]$SignalThreshold = 0.001,
    [string[]]$HostApi = @("MME", "Windows DirectSound", "Windows WASAPI"),
    [switch]$IncludeWdmKs,
    [string]$InputName = "Audio 8 DJ",
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$script = Join-Path $repoRoot "windows\tests\a8dj_input_endpoint_probe.py"
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
if (-not (Test-Path $script)) {
    throw "input endpoint probe not found at $script"
}

if ($IncludeWdmKs -and -not ($HostApi -contains "Windows WDM-KS")) {
    $HostApi += "Windows WDM-KS"
}

function New-RunDirectory {
    param([string]$Requested)

    if ($Requested) {
        New-Item -ItemType Directory -Path $Requested -Force | Out-Null
        return (Resolve-Path $Requested).Path
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $path = Join-Path $repoRoot "local-analysis\windows-a8dj-input-endpoint-smoke-$stamp"
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
            $_.FriendlyName -match 'iRig|Audio 8 DJ|IK Multimedia|Native Instruments|Realtek' -or
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

function Assert-RequiredHardware {
    param([string]$RunDir)

    $present = Get-PnpDevice -PresentOnly
    $audio8Usb = $present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' }
    $audio8Inputs = $present | Where-Object { $_.FriendlyName -like 'Microphone (Audio 8 DJ)*' }
    $irigUsb = $present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' }

    $lines = @(
        "hardware_preflight_time=$(Get-Date -Format o)"
        "found_audio8_usb=$([int][bool]$audio8Usb)"
        "found_audio8_input_endpoint=$([int][bool]$audio8Inputs)"
        "found_irig_usb=$([int][bool]$irigUsb)"
        "policy=block_if_audio8_missing_do_not_recover_unattended"
    )
    $lines | Set-Content -Path (Join-Path $RunDir "hardware-preflight.txt") -Encoding UTF8

    if (-not $audio8Usb -or -not $audio8Inputs) {
        throw "Audio 8 DJ input hardware/endpoints are not stable/visible; see $(Join-Path $RunDir 'hardware-preflight.txt')"
    }
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-a8dj-input-endpoint-smoke.lock"
$lockTaken = $false

try {
    if (Test-Path $lockPath) {
        $existing = Get-Content -Raw -Path $lockPath -ErrorAction SilentlyContinue
        throw "Another Windows Audio 8 DJ input probe may be active: $lockPath $existing"
    }

    @(
        "pid=$PID"
        "started=$(Get-Date -Format o)"
        "run_dir=$OutDir"
        "resource=Audio 8 DJ capture endpoints"
    ) | Set-Content -Path $lockPath -Encoding UTF8
    $lockTaken = $true

    @(
        "safety_policy=conservative_unattended_audio8_input_probe"
        "does_not_reset_usb=1"
        "does_not_disable_or_enable_pnp_devices=1"
        "does_not_restart_windows_audio=1"
        "does_not_change_default_audio_devices=1"
        "does_not_attempt_irig_recovery=1"
        "records_pnp_before_after=1"
        "always_attempts_iso_silence_after=1"
        "known_signal_required_for_quality_claim=1"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-before.txt") -Label "before"
    Assert-RequiredHardware -RunDir $OutDir

    $args = @(
        $script,
        "--ctl", $ctl,
        "--out-dir", $OutDir,
        "--seconds", $Seconds,
        "--rate", $Rate,
        "--blocksize", $BlockSize,
        "--channels", $Channels,
        "--signal-threshold", $SignalThreshold,
        "--name", $InputName
    )
    foreach ($api in $HostApi) {
        $args += @("--hostapi", $api)
    }
    if ($IncludeWdmKs) {
        $args += "--include-wdm-ks"
    }

    & $Python @args
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Audio 8 DJ input endpoint probe failed with exit code $exitCode"
    }
} finally {
    try {
        & $ctl iso-silence | Set-Content -Path (Join-Path $OutDir "iso-silence-after.txt") -Encoding UTF8
    } catch {
        "iso-silence-after failed: $($_.Exception.Message)" |
            Set-Content -Path (Join-Path $OutDir "iso-silence-after-error.txt") -Encoding UTF8
    }
    try {
        Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-after.txt") -Label "after"
    } catch {
        "pnp-after failed: $($_.Exception.Message)" |
            Set-Content -Path (Join-Path $OutDir "pnp-after-error.txt") -Encoding UTF8
    }
    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -LiteralPath $lockPath -Force
    }
}

Write-Host "Audio 8 DJ input endpoint smoke artifacts: $OutDir"
