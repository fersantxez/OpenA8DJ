param(
    [ValidateRange(5, 300)][int]$Seconds = 30,
    [ValidateSet(44100, 48000)][int]$Rate = 48000,
    [ValidateRange(16, 4096)][int]$BlockSize = 512,
    [ValidateSet('render', 'capture', 'render,capture')][string]$Directions = 'render,capture',
    [ValidateSet('cadence', 'restart')][string]$ProbeMode = 'cadence',
    [ValidateRange(1, 10)][int]$RestartRepeats = 1,
    [ValidateRange(100, 2000)][int]$StreamMilliseconds = 200,
    [ValidateRange(1, 30)][int]$BaselineSeconds = 5,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedBuildFingerprint,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedSysSha256,
    [string]$ExpectedInstanceId = 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___',
    [ValidateRange(30, 900)][int]$ProcessTimeoutSeconds = 180,
    [string]$OutDir = "",
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$env:OPENA8DJ_INSTANCE_ID = $ExpectedInstanceId
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1') -Force
$ctl = Join-Path $repoRoot 'windows\dist\Release\x64\opena8djctl.exe'
$probe = Join-Path $repoRoot $(if ($ProbeMode -eq 'restart') {
    'windows\tests\a8dj_stream_restart_probe.py'
} else {
    'windows\tests\a8dj_stream_cadence_probe.py'
})
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ('local-analysis\windows\performance-cadence-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$checkpointPath = Join-Path $OutDir 'checkpoints.jsonl'
$script:checkpointSequence = 0

$counterNames = @(
    'underruns', 'overruns', 'packet-errors', 'late-completions',
    'iso-out-empty', 'iso-out-late', 'iso-out-bad-start', 'iso-out-other-err',
    'iso-output-panic', 'iso-cap-late', 'iso-cap-bad-start', 'iso-cap-other-err',
    'rate-settle-fails', 'iso-frame-query-failures'
)

function Write-Checkpoint([string]$Name, [string]$Status, [hashtable]$Data) {
    $script:checkpointSequence++
    $record = [ordered]@{
        sequence = $script:checkpointSequence
        time = (Get-Date).ToUniversalTime().ToString('o')
        name = $Name
        status = $Status
        data = $Data
    }
    [IO.File]::AppendAllText(
        $checkpointPath,
        ($record | ConvertTo-Json -Compress -Depth 8) + "`n",
        [Text.UTF8Encoding]::new($false))
}

function Read-Counters([string]$Text) {
    $values = [ordered]@{}
    foreach ($name in $counterNames) {
        $pattern = if ($name -eq 'iso-frame-query-failures') {
            '(?m)^\s*iso-frame-query:\s*runs=\d+\s+failures=(\d+)\b'
        } else {
            '(?m)^\s*' + [regex]::Escape($name) + ':\s*(\d+)'
        }
        $match = [regex]::Match($Text, $pattern)
        $values[$name] = if ($match.Success) { [int64]$match.Groups[1].Value } else { $null }
    }
    return $values
}

function Get-DriverIdentity {
    $driver = Get-CimInstance Win32_SystemDriver |
        Where-Object { $_.Name -in @('OpenA8DJUsb', 'OpenA8DJUsbAcx') -and $_.State -eq 'Running' } |
        Select-Object -First 1
    if ($null -eq $driver) { throw 'No running OpenA8DJ kernel service was found' }
    $imagePath = [Environment]::ExpandEnvironmentVariables(([string]$driver.PathName).Trim('"'))
    if ($imagePath.StartsWith('\SystemRoot\', [StringComparison]::OrdinalIgnoreCase)) {
        $imagePath = Join-Path $env:SystemRoot $imagePath.Substring(12)
    }
    $file = Get-Item -LiteralPath $imagePath -ErrorAction Stop
    return [ordered]@{
        service_name = [string]$driver.Name
        service_state = [string]$driver.State
        image_path = $file.FullName
        image_sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        file_version = [string]$file.VersionInfo.FileVersion
    }
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
    if (-not $process.Start()) { throw "Could not start bounded process: $FilePath" }
    try { $process.PriorityClass = 'High' } catch {}
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    $timeoutMessage = $null
    if ($timedOut) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        $terminated = $process.WaitForExit(5000)
        $exitCode = -1
        $timeoutMessage = if ($terminated) {
            "Process timeout after $TimeoutSeconds seconds; process tree terminated."
        } else {
            "Process timeout after $TimeoutSeconds seconds; termination was not confirmed."
        }
    } else {
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    $stdoutText = if ($stdoutTask.Wait(5000)) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
    $stderrText = if ($stderrTask.Wait(5000)) { $stderrTask.GetAwaiter().GetResult() } else { '' }
    [IO.File]::WriteAllText($StdoutPath, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        $StderrPath,
        (@($stderrText, $timeoutMessage) | Where-Object { -not [string]::IsNullOrEmpty($_) }) -join "`n",
        [Text.UTF8Encoding]::new($false))
    $process.Dispose()
    return $exitCode
}

@(
    'safety_policy=independent_endpoint_performance_no_reset_no_restart'
    'shared_hardware_lock=required'
    'does_not_install_or_reload_driver=1'
    'does_not_reset_usb=1'
    'does_not_restart_windows_audio=1'
    'records_boot_identity=1'
    'records_driver_counters_before_after=1'
    'opens_render_then_capture_sequentially=1'
) | Set-Content -LiteralPath (Join-Path $OutDir 'safety.txt') -Encoding UTF8

$lock = $null
$chat = @()
$savedPriorities = @{}
$failure = $null
$probeExit = $null
$identityBefore = $null
$identityAfter = $null
$diagnosticsBefore = ''
$diagnosticsAfter = ''
$safetyAfter = ''
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
try {
    $lock = Acquire-OpenA8DJHardwareLock -Gate 'stream-performance-cadence' -RunDir $OutDir -TimeoutSeconds 0
    $recovery = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction Stop
    if ([string]$recovery.State -ne 'Ready') { throw 'Recovery task is not Ready' }
    $safetyBefore = (& $ctl safety | Out-String)
    $diagnosticsBefore = (& $ctl diagnostics | Out-String)
    $identityBefore = Get-DriverIdentity
    $safetyBefore | Set-Content (Join-Path $OutDir 'driver-safety-pre.txt') -Encoding UTF8
    $diagnosticsBefore | Set-Content (Join-Path $OutDir 'driver-diagnostics-pre.txt') -Encoding UTF8
    if ($safetyBefore -notmatch ('(?mi)^\s*build-fingerprint:\s*' + [regex]::Escape($ExpectedBuildFingerprint) + '\s*$')) {
        throw 'Loaded driver fingerprint does not match the expected candidate'
    }
    if ($identityBefore.image_sha256 -ne $ExpectedSysSha256.ToLowerInvariant()) {
        throw 'Loaded driver SYS hash does not match the expected candidate'
    }
    if ($safetyBefore -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
        $diagnosticsBefore -notmatch '(?mi)^\s*streaming:\s*no\s*$') {
        throw 'Driver is armed or streaming before the performance run'
    }
    Write-Checkpoint 'preflight' 'ok' @{
        boot = $bootBefore.ToString('o')
        fingerprint = $ExpectedBuildFingerprint.ToLowerInvariant()
        sys_sha256 = $identityBefore.image_sha256
        recovery_task_state = [string]$recovery.State
        intended_operations = @('idle CPU baseline', '8-channel render', '8-channel capture', 'postflight')
    }

    $chat = @(Get-Process ChatGPT -ErrorAction SilentlyContinue)
    foreach ($process in $chat) {
        try {
            $savedPriorities[$process.Id] = $process.PriorityClass
            $process.PriorityClass = 'BelowNormal'
        } catch {}
    }
    $arguments = if ($ProbeMode -eq 'restart') {
        @($probe, '--rate', [string]$Rate, '--blocksize', [string]$BlockSize,
            '--ctl', $ctl, '--out-dir', $OutDir, '--repeats', [string]$RestartRepeats,
            '--stream-milliseconds', [string]$StreamMilliseconds)
    } else {
        @($probe, '--seconds', [string]$Seconds, '--rate', [string]$Rate,
            '--blocksize', [string]$BlockSize, '--directions', $Directions,
            '--ctl', $ctl, '--baseline-seconds', [string]$BaselineSeconds, '--out-dir', $OutDir)
    }
    Write-Checkpoint 'probe-start' 'pending' @{
        seconds_per_direction = $Seconds
        baseline_seconds = $BaselineSeconds
        rate = $Rate
        blocksize = $BlockSize
        directions = $Directions
        probe_mode = $ProbeMode
        restart_repeats = $RestartRepeats
        stream_milliseconds = $StreamMilliseconds
    }
    $probeExit = Invoke-BoundedProcess `
        -FilePath $Python `
        -Arguments $arguments `
        -StdoutPath (Join-Path $OutDir 'probe.stdout.txt') `
        -StderrPath (Join-Path $OutDir 'probe.stderr.txt') `
        -TimeoutSeconds $ProcessTimeoutSeconds
    Write-Checkpoint 'probe-returned' $(if ($probeExit -eq 0) { 'ok' } else { 'failed' }) @{ exit = $probeExit }
    if ($probeExit -ne 0) { throw "Performance cadence probe failed: $probeExit" }
}
catch {
    $failure = $_.Exception.Message
    Write-Checkpoint 'exception' 'failed' @{ message = $failure }
}
finally {
    foreach ($process in $chat) {
        if ($savedPriorities.ContainsKey($process.Id)) {
            try { $process.PriorityClass = $savedPriorities[$process.Id] } catch {}
        }
    }
    $safetyAfter = (& $ctl safety | Out-String)
    $diagnosticsAfter = (& $ctl diagnostics | Out-String)
    try { $identityAfter = Get-DriverIdentity } catch {
        if ($null -eq $failure) { $failure = $_.Exception.Message }
    }
    $safetyAfter | Set-Content (Join-Path $OutDir 'driver-safety-post.txt') -Encoding UTF8
    $diagnosticsAfter | Set-Content (Join-Path $OutDir 'driver-diagnostics-post.txt') -Encoding UTF8
    $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
    if ($lock) { Release-OpenA8DJHardwareLock -Lock $lock }
}

$beforeCounters = Read-Counters $diagnosticsBefore
$afterCounters = Read-Counters $diagnosticsAfter
$counterDeltas = [ordered]@{}
$counterDeltasZero = $true
foreach ($name in $counterNames) {
    if ($null -eq $beforeCounters[$name] -or $null -eq $afterCounters[$name]) {
        $counterDeltas[$name] = $null
        $counterDeltasZero = $false
    } else {
        $counterDeltas[$name] = [int64]$afterCounters[$name] - [int64]$beforeCounters[$name]
        if ($counterDeltas[$name] -ne 0) { $counterDeltasZero = $false }
    }
}
$probeSummary = $null
$probeSummaryPath = Join-Path $OutDir 'cadence-summary.json'
if (Test-Path $probeSummaryPath) { $probeSummary = Get-Content $probeSummaryPath -Raw | ConvertFrom-Json }
$identityUnchanged = (
    $null -ne $identityBefore -and $null -ne $identityAfter -and
    $identityBefore.service_name -eq $identityAfter.service_name -and
    $identityBefore.image_sha256 -eq $identityAfter.image_sha256
)
$summary = [ordered]@{
    probe_exit = $probeExit
    probe = $probeSummary
    boot_before = $bootBefore.ToString('o')
    boot_after = $bootAfter.ToString('o')
    boot_unchanged = ($bootBefore -eq $bootAfter)
    identity_before = $identityBefore
    identity_after = $identityAfter
    identity_unchanged = $identityUnchanged
    counter_deltas = $counterDeltas
    counter_deltas_zero = $counterDeltasZero
    disarmed = ($safetyAfter -match '(?mi)^\s*armed-phase:\s*0\s*$')
    stream_stopped = ($diagnosticsAfter -match '(?mi)^\s*streaming:\s*no\s*$')
    failure = $failure
    passed = ($null -eq $failure -and $probeExit -eq 0 -and $null -ne $probeSummary -and
        $probeSummary.passed -and $bootBefore -eq $bootAfter -and $identityUnchanged -and
        $counterDeltasZero -and $safetyAfter -match '(?mi)^\s*armed-phase:\s*0\s*$' -and
        $diagnosticsAfter -match '(?mi)^\s*streaming:\s*no\s*$')
}
[IO.File]::WriteAllText(
    (Join-Path $OutDir 'run-summary.json'),
    ($summary | ConvertTo-Json -Depth 12) + "`n",
    [Text.UTF8Encoding]::new($false))
Get-Content (Join-Path $OutDir 'run-summary.json') -Raw
if (-not $summary.passed) { exit 2 }
