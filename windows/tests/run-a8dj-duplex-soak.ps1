param(
    [ValidateRange(1, 86400)]
    [int]$Seconds = 1800,
    [ValidateSet(44100, 48000)]
    [int]$Rate = 48000,
    [ValidateRange(16, 4096)]
    [int]$BlockSize = 512,
    [ValidateRange(0.001, 0.10)]
    [double]$Amplitude = 0.02,
    [ValidateRange(0.05, 60.0)]
    [double]$ProgressSeconds = 10.0,
    [ValidateRange(2, 60)]
    [int]$PnpMonitorIntervalSeconds = 10,
    [switch]$Exclusive,
    [ValidateSet('low', 'high')]
    [string]$Latency = 'high',
    [switch]$Strict = $true,
    [ValidateRange(1, 65535)]
    [int]$ExpectedApiVersion = 44,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedBuildFingerprint,
    [ValidateRange(0, 90000)]
    [int]$ProcessTimeoutSeconds = 0,
    [string]$OutDir = "",
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$common = Join-Path $repoRoot "windows\scripts\OpenA8DJ.WindowsCommon.psm1"
$probe = Join-Path $repoRoot "windows\tests\a8dj_duplex_soak.py"
$ctl = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
Import-Module $common -Force

$effectiveProcessTimeoutSeconds = if ($ProcessTimeoutSeconds -eq 0) {
    $Seconds + 120
} else {
    $ProcessTimeoutSeconds
}
if ($effectiveProcessTimeoutSeconds -le ($Seconds + 30)) {
    throw "ProcessTimeoutSeconds must exceed Seconds by more than 30 seconds"
}

if (-not (Test-Path $Python)) {
    $command = Get-Command python -ErrorAction SilentlyContinue
    if (-not $command) { throw "Python was not found." }
    $Python = $command.Source
}
if (-not (Test-Path $probe) -or -not (Test-Path $ctl)) {
    throw "Soak probe or opena8djctl.exe is missing."
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ("local-analysis\windows\quality-duplex-soak-{0}s-{1}" -f $Seconds, (Get-Date -Format "yyyyMMdd-HHmmss"))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

$counterNames = @(
    "underruns", "overruns", "packet-errors", "late-completions",
    "iso-out-empty", "iso-out-late", "iso-out-bad-start", "iso-out-other-err",
    "iso-output-panic", "iso-cap-late", "iso-cap-bad-start", "iso-cap-other-err",
    "rate-settle-fails"
)

function Read-Counters([string]$Text) {
    $values = [ordered]@{}
    foreach ($name in $counterNames) {
        if ([string]::IsNullOrWhiteSpace($Text)) {
            $values[$name] = $null
            continue
        }
        $pattern = "(?m)^\s*" + [regex]::Escape($name) + ":\s*(\d+)"
        $match = [regex]::Match($Text, $pattern)
        $values[$name] = if ($match.Success) { [int64]$match.Groups[1].Value } else { $null }
    }
    return $values
}

function Read-SafetyValue([string]$Text, [string]$Name) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
    $match = [regex]::Match(
        $Text,
        "(?m)^\s*" + [regex]::Escape($Name) + ":\s*(\S+)"
    )
    if ($match.Success) {
        return $match.Groups[1].Value
    }
    return $null
}

function Start-PnpContinuityMonitor(
    [string]$Path,
    [int]$DurationSeconds,
    [int]$IntervalSeconds,
    [string]$UsbInstanceId,
    [string]$RenderInstanceId,
    [string]$CaptureInstanceId) {
    "time_epoch`taudio8_usb_ok`taudio8_render_ok`taudio8_capture_ok" |
        Set-Content -LiteralPath $Path -Encoding UTF8
    $initialValues = @(
        [int][bool](Get-PnpDevice -InstanceId $UsbInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
        [int][bool](Get-PnpDevice -InstanceId $RenderInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
        [int][bool](Get-PnpDevice -InstanceId $CaptureInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
    )
    "{0}`t{1}" -f ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0), ($initialValues -join "`t") |
        Add-Content -LiteralPath $Path -Encoding UTF8
    return Start-Job -ScriptBlock {
        param($Path, $DurationSeconds, $IntervalSeconds, $UsbInstanceId, $RenderInstanceId, $CaptureInstanceId)
        $deadline = (Get-Date).AddSeconds($DurationSeconds)
        Start-Sleep -Seconds $IntervalSeconds
        while ((Get-Date) -lt $deadline) {
            $values = @(
                [int][bool](Get-PnpDevice -InstanceId $UsbInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
                [int][bool](Get-PnpDevice -InstanceId $RenderInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
                [int][bool](Get-PnpDevice -InstanceId $CaptureInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
            )
            "{0}`t{1}" -f ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0), ($values -join "`t") |
                Add-Content -LiteralPath $Path -Encoding UTF8
            Start-Sleep -Seconds $IntervalSeconds
        }
    } -ArgumentList $Path, $DurationSeconds, $IntervalSeconds, $UsbInstanceId, $RenderInstanceId, $CaptureInstanceId
}

function Invoke-BoundedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )
    $argumentLine = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ }
    }) -join ' '
    $startInfo = [Diagnostics.ProcessStartInfo]::new($FilePath, $argumentLine)
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    $timeoutMessage = $null
    if (-not $process.Start()) { throw "Could not start bounded process: $FilePath" }
    try { $process.PriorityClass = 'High' } catch {}
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F |
            Out-Null
        $terminated = $process.WaitForExit(5000)
        $timeoutMessage = if ($terminated) {
            "Process timeout after $TimeoutSeconds seconds; PID $($process.Id) and its child tree were terminated."
        } else {
            "Process timeout after $TimeoutSeconds seconds; PID $($process.Id) did not confirm termination within 5 seconds."
        }
        $exitCode = -1
    } else {
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    $stdoutText = ''
    $stderrText = ''
    if ($stdoutTask.Wait(5000)) { $stdoutText = $stdoutTask.GetAwaiter().GetResult() } else { $process.StandardOutput.Close() }
    if ($stderrTask.Wait(5000)) { $stderrText = $stderrTask.GetAwaiter().GetResult() } else { $process.StandardError.Close() }
    [IO.File]::WriteAllText($StdoutPath, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        $StderrPath,
        (@($stderrText, $timeoutMessage) | Where-Object { -not [string]::IsNullOrEmpty($_) }) -join "`n",
        [Text.UTF8Encoding]::new($false))
    return $exitCode
}

$lock = $null
$probeExit = $null
$failure = $null
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$diagnosticsBefore = $null
$safetyBefore = $null
$pnpMonitor = $null
$chat = @()
$savedPriorities = @{}

try {
    $lock = Acquire-OpenA8DJHardwareLock -Gate "duplex-soak" -RunDir $OutDir -TimeoutSeconds 0
    $diagnosticsBefore = (& $ctl diagnostics | Out-String)
    $diagnosticsBefore | Set-Content (Join-Path $OutDir "driver-diagnostics-pre.txt") -Encoding UTF8
    $safetyBefore = (& $ctl safety | Out-String)
    $safetyBefore | Set-Content (Join-Path $OutDir "driver-safety-pre.txt") -Encoding UTF8
    $apiBefore = Read-SafetyValue $safetyBefore "api-version"
    $fingerprintBefore = Read-SafetyValue $safetyBefore "build-fingerprint"
    if ($apiBefore -ne [string]$ExpectedApiVersion -or
        $fingerprintBefore -ne $ExpectedBuildFingerprint.ToLowerInvariant()) {
        throw "Loaded driver identity preflight failed"
    }
    $present = @(Get-PnpDevice -PresentOnly -ErrorAction Stop)
    $audio8Usb = @($present | Where-Object { $_.Status -eq 'OK' -and $_.InstanceId -match '^USB\\VID_17CC&PID_1978\\' })
    $audio8Render = @($present | Where-Object { $_.Status -eq 'OK' -and $_.FriendlyName -eq 'Audio 8 DJ (8ch Out) (Audio 8 DJ)' })
    $audio8Capture = @($present | Where-Object { $_.Status -eq 'OK' -and $_.FriendlyName -eq 'Audio 8 DJ (8ch In) (Audio 8 DJ)' })
    if ($audio8Usb.Count -ne 1 -or $audio8Render.Count -ne 1 -or $audio8Capture.Count -ne 1) {
        throw "Exact Audio 8 DJ PnP identity preflight failed"
    }
    $pnpMonitor = Start-PnpContinuityMonitor `
        -Path (Join-Path $OutDir "pnp-continuity.tsv") `
        -DurationSeconds ($Seconds + 60) `
        -IntervalSeconds $PnpMonitorIntervalSeconds `
        -UsbInstanceId $audio8Usb[0].InstanceId `
        -RenderInstanceId $audio8Render[0].InstanceId `
        -CaptureInstanceId $audio8Capture[0].InstanceId

    $chat = @(Get-Process ChatGPT -ErrorAction SilentlyContinue)
    foreach ($chatProcess in $chat) {
        try {
            $savedPriorities[$chatProcess.Id] = $chatProcess.PriorityClass
            $chatProcess.PriorityClass = 'Idle'
        } catch {}
    }

    $arguments = @(
        $probe, "--seconds", $Seconds, "--rate", $Rate,
        "--blocksize", $BlockSize, "--amplitude", $Amplitude,
        "--latency", $Latency,
        "--progress-seconds", $ProgressSeconds, "--out-dir", $OutDir
    )
    if ($Exclusive) { $arguments += "--exclusive" }
    if ($Strict) { $arguments += "--strict" }
    $probeExit = Invoke-BoundedProcess `
        -FilePath $Python `
        -Arguments $arguments `
        -StdoutPath (Join-Path $OutDir 'runner-stdout.log') `
        -StderrPath (Join-Path $OutDir 'runner-stderr.log') `
        -TimeoutSeconds $effectiveProcessTimeoutSeconds
    if ($probeExit -ne 0) { throw "Duplex soak probe failed with exit code $probeExit" }
}
catch {
    $failure = $_.Exception.Message
}
finally {
    try {
        foreach ($chatProcess in $chat) {
            if ($savedPriorities.ContainsKey($chatProcess.Id)) {
                try { $chatProcess.PriorityClass = $savedPriorities[$chatProcess.Id] } catch {}
            }
        }
        Start-Sleep -Milliseconds 500
        $diagnosticsAfter = $null
        $safetyAfter = $null
        $bootAfter = $null
        try {
            $diagnosticsAfter = (& $ctl diagnostics | Out-String)
            $diagnosticsAfter | Set-Content (Join-Path $OutDir "driver-diagnostics-post.txt") -Encoding UTF8
            $safetyAfter = (& $ctl safety | Out-String)
            $safetyAfter | Set-Content (Join-Path $OutDir "driver-safety-post.txt") -Encoding UTF8
            $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
        }
        catch {
            $cleanupFailure = "post-run diagnostics failed: $($_.Exception.Message)"
            $failure = if ($null -eq $failure) { $cleanupFailure } else { "$failure; $cleanupFailure" }
        }

        if ($pnpMonitor) {
            Stop-Job $pnpMonitor -ErrorAction SilentlyContinue
            Remove-Job $pnpMonitor -Force -ErrorAction SilentlyContinue
        }
        $pnpRows = @()
        $pnpPath = Join-Path $OutDir "pnp-continuity.tsv"
        if (Test-Path -LiteralPath $pnpPath) {
            $pnpRows = @(Import-Csv -LiteralPath $pnpPath -Delimiter "`t")
        }
        $pnpContinuity = (
            $pnpRows.Count -gt 0 -and
            @($pnpRows | Where-Object {
                $_.audio8_usb_ok -ne '1' -or
                $_.audio8_render_ok -ne '1' -or
                $_.audio8_capture_ok -ne '1'
            }).Count -eq 0
        )

        $probeSummary = $null
        $probeSummaryPath = Join-Path $OutDir "summary.json"
        if (Test-Path -LiteralPath $probeSummaryPath) {
            try { $probeSummary = Get-Content -Raw $probeSummaryPath | ConvertFrom-Json } catch {}
        }
        $minimumFrames = [int64]$Rate * [int64]$Seconds - 2L * [int64]$BlockSize
        $probeStatsClean = (
            $null -ne $probeSummary -and
            [int64]$probeSummary.callbacks -gt 0 -and
            [int64]$probeSummary.render_frames -ge $minimumFrames -and
            [int64]$probeSummary.capture_frames -ge $minimumFrames -and
            [int64]$probeSummary.capture_clipped_frames -eq 0 -and
            [int64]$probeSummary.raw_click_outliers -eq 0 -and
            @($probeSummary.status_events).Count -eq 0 -and
            -not [bool]$probeSummary.watchdog_expired
        )

        $before = Read-Counters $diagnosticsBefore
        $after = Read-Counters $diagnosticsAfter
        $deltas = [ordered]@{}
        foreach ($name in $counterNames) {
            $deltas[$name] = if ($null -ne $before[$name] -and $null -ne $after[$name]) {
                $after[$name] - $before[$name]
            } else { $null }
        }
        $badDeltas = @($deltas.GetEnumerator() | Where-Object { $null -eq $_.Value -or $_.Value -ne 0 })
        $apiAfter = Read-SafetyValue $safetyAfter "api-version"
        $fingerprintAfter = Read-SafetyValue $safetyAfter "build-fingerprint"
        $armedAfter = Read-SafetyValue $safetyAfter "armed-phase"
        $identityUnchanged = (
            $apiBefore -eq $apiAfter -and
            $apiAfter -eq [string]$ExpectedApiVersion -and
            $fingerprintBefore -eq $fingerprintAfter -and
            $armedAfter -eq '0'
        )
        $streamStopped = $diagnosticsAfter -match '(?m)^\s*streaming:\s*no\s*$'
        $summary = [ordered]@{
            seconds = $Seconds
            rate = $Rate
            blocksize = $BlockSize
            exclusive = [bool]$Exclusive
            latency = $Latency
            strict = [bool]$Strict
            expected_api_version = $ExpectedApiVersion
            expected_build_fingerprint = $ExpectedBuildFingerprint.ToLowerInvariant()
            process_timeout_seconds = $effectiveProcessTimeoutSeconds
            build_fingerprint = $fingerprintBefore
            identity_unchanged = $identityUnchanged
            probe_exit = $probeExit
            probe_stats_clean = $probeStatsClean
            minimum_required_frames = $minimumFrames
            stream_stopped = $streamStopped
            pnp_continuity_samples = $pnpRows.Count
            pnp_continuity = $pnpContinuity
            boot_before = $bootBefore.ToString("o")
            boot_after = if ($null -ne $bootAfter) { $bootAfter.ToString("o") } else { $null }
            boot_unchanged = ($null -ne $bootAfter -and $bootBefore -eq $bootAfter)
            driver_counter_deltas = $deltas
            failure = $failure
            passed = (
                $null -eq $failure -and
                [bool]$Strict -and
                $probeExit -eq 0 -and
                $probeStatsClean -and
                $identityUnchanged -and
                $streamStopped -and
                $pnpContinuity -and
                $null -ne $bootAfter -and
                $bootBefore -eq $bootAfter -and
                $badDeltas.Count -eq 0
            )
        }
        $summary | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $OutDir "run-summary.json") -Encoding UTF8
    }
    finally {
        if ($lock) { Release-OpenA8DJHardwareLock -Lock $lock }
    }
}

Get-Content (Join-Path $OutDir "run-summary.json") -Raw
if (-not $summary.passed) { exit 2 }
