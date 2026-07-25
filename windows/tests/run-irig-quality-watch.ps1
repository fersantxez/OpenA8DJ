param(
    [int]$Iterations = 0,
    [int]$IntervalSeconds = 60,
    [int]$Seconds = 30,
    [int]$Rate = 44100,
    [int]$BlockSize = 512,
    [string]$Latency = "high",
    [ValidateSet("tone", "multitone")]
    [string]$Signal = "multitone",
    [double]$Amplitude = 0.08,
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    [string]$RunRoot = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$probe = Join-Path $repoRoot "windows\tests\run-irig-quality-probe.ps1"

function Test-HardwareVisible {
    $present = Get-PnpDevice -PresentOnly
    $irigUsb = $present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' }
    $irigEndpoint = $present | Where-Object { $_.FriendlyName -eq 'Line In (iRig Stream)' }
    $audio8Usb = $present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' }
    $audio8Out = $present | Where-Object { $_.FriendlyName -like 'Audio 8 DJ (Ch A, Out 1|2)*' }
    return [bool]($irigUsb -and $irigEndpoint -and $audio8Usb -and $audio8Out)
}

if (-not $RunRoot) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $RunRoot = Join-Path $repoRoot "local-analysis\windows-irig-quality-watch-$stamp"
}
New-Item -ItemType Directory -Path $RunRoot -Force | Out-Null

@(
    "watch_started=$(Get-Date -Format o)"
    "iterations=$Iterations"
    "interval_seconds=$IntervalSeconds"
    "seconds_per_probe=$Seconds"
    "safety_policy=passive_monitor_only_no_usb_reset_no_recovery"
    "stop_policy=stop_on_first_probe_failure_or_missing_hardware"
) | Set-Content -Path (Join-Path $RunRoot "watch-summary.txt") -Encoding UTF8

$cycle = 0
while ($true) {
    $cycle++
    if ($Iterations -gt 0 -and $cycle -gt $Iterations) {
        "watch_result=PASS_COMPLETED_REQUESTED_ITERATIONS" |
            Add-Content -Path (Join-Path $RunRoot "watch-summary.txt") -Encoding UTF8
        break
    }

    if (-not (Test-HardwareVisible)) {
        @(
            "watch_result=STOPPED"
            "reason=required_hardware_missing_before_cycle"
            "cycle=$cycle"
            "time=$(Get-Date -Format o)"
        ) | Add-Content -Path (Join-Path $RunRoot "watch-summary.txt") -Encoding UTF8
        exit 2
    }

    $cycleDir = Join-Path $RunRoot ("cycle-{0:D4}" -f $cycle)
    "cycle=$cycle start=$(Get-Date -Format o) dir=$cycleDir" |
        Add-Content -Path (Join-Path $RunRoot "watch-cycles.tsv") -Encoding UTF8

    & powershell -NoProfile -ExecutionPolicy Bypass -File $probe `
        -Seconds $Seconds `
        -Rate $Rate `
        -BlockSize $BlockSize `
        -Latency $Latency `
        -Signal $Signal `
        -Amplitude $Amplitude `
        -Python $Python `
        -OutDir $cycleDir

    if ($LASTEXITCODE -ne 0) {
        @(
            "watch_result=STOPPED"
            "reason=probe_failed"
            "cycle=$cycle"
            "exit_code=$LASTEXITCODE"
            "cycle_dir=$cycleDir"
            "time=$(Get-Date -Format o)"
        ) | Add-Content -Path (Join-Path $RunRoot "watch-summary.txt") -Encoding UTF8
        exit $LASTEXITCODE
    }

    if (-not (Test-HardwareVisible)) {
        @(
            "watch_result=STOPPED"
            "reason=required_hardware_missing_after_cycle"
            "cycle=$cycle"
            "cycle_dir=$cycleDir"
            "time=$(Get-Date -Format o)"
        ) | Add-Content -Path (Join-Path $RunRoot "watch-summary.txt") -Encoding UTF8
        exit 3
    }

    if ($Iterations -gt 0 -and $cycle -ge $Iterations) {
        continue
    }

    Start-Sleep -Seconds $IntervalSeconds
}
