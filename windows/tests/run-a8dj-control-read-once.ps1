param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedBuildFingerprint,
    [ValidateRange(5, 60)][int]$ProcessTimeoutSeconds = 30,
    [string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1') -Force
$ctl = Join-Path $repoRoot 'windows\dist\Release\x64\opena8djctl.exe'
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ('local-analysis\windows\control-read-once-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$checkpointPath = Join-Path $OutDir 'checkpoints.jsonl'
$script:checkpointSequence = 0

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
        ($record | ConvertTo-Json -Compress -Depth 6) + "`n",
        [Text.UTF8Encoding]::new($false))
}

function Invoke-BoundedCtl([string[]]$Arguments, [string]$Name) {
    $stdout = Join-Path $OutDir ($Name + '.stdout.txt')
    $stderr = Join-Path $OutDir ($Name + '.stderr.txt')
    $argumentLine = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ }
    }) -join ' '
    $startInfo = [Diagnostics.ProcessStartInfo]::new($ctl, $argumentLine)
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { return -3 }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($ProcessTimeoutSeconds * 1000)
    if ($timedOut) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        [void]$process.WaitForExit(5000)
        $exitCode = -1
    } else {
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    $stdoutText = ''
    $stderrText = ''
    if ($stdoutTask.Wait(5000)) { $stdoutText = $stdoutTask.GetAwaiter().GetResult() } else { $process.StandardOutput.Close() }
    if ($stderrTask.Wait(5000)) { $stderrText = $stderrTask.GetAwaiter().GetResult() } else { $process.StandardError.Close() }
    [IO.File]::WriteAllText($stdout, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($stderr, $stderrText, [Text.UTF8Encoding]::new($false))
    return $exitCode
}

$lock = $null
$armed = $false
$failure = $null
$readExit = $null
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
try {
    $lock = Acquire-OpenA8DJHardwareLock -Gate 'control-read-once' -RunDir $OutDir -TimeoutSeconds 0
    $recovery = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction Stop
    if ([string]$recovery.State -ne 'Ready') { throw 'Recovery task is not Ready' }
    $safetyBefore = (& $ctl safety | Out-String)
    $diagnosticsBefore = (& $ctl diagnostics | Out-String)
    $safetyBefore | Set-Content (Join-Path $OutDir 'safety-pre.txt') -Encoding UTF8
    $diagnosticsBefore | Set-Content (Join-Path $OutDir 'diagnostics-pre.txt') -Encoding UTF8
    if ($safetyBefore -notmatch ('(?mi)^\s*build-fingerprint:\s*' + [regex]::Escape($ExpectedBuildFingerprint) + '\s*$')) {
        throw 'Loaded driver fingerprint does not match expected candidate'
    }
    if ($safetyBefore -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
        $diagnosticsBefore -notmatch '(?mi)^\s*streaming:\s*no\s*$') {
        throw 'Driver is armed or streaming before control read'
    }
    Write-Checkpoint 'preflight' 'ok' @{
        boot = $bootBefore.ToString('o')
        fingerprint = $ExpectedBuildFingerprint.ToLowerInvariant()
        recovery_task_state = [string]$recovery.State
        intended_operations = @('arm control-read once', 'read controls once', 'capture safety and diagnostics', 'disarm')
    }

    $nonceBytes = [byte[]]::new(16)
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($nonceBytes)
    $nonceHigh = '0x{0:X16}' -f [BitConverter]::ToUInt64($nonceBytes, 0)
    $nonceLow = '0x{0:X16}' -f [BitConverter]::ToUInt64($nonceBytes, 8)
    Write-Checkpoint 'arm-start' 'pending' @{ phase = 'control-read'; nonce_high = $nonceHigh; nonce_low = $nonceLow }
    $armExit = Invoke-BoundedCtl @('arm', 'control-read', $nonceHigh, $nonceLow) 'arm'
    if ($armExit -ne 0) { throw "Control-read arm failed: $armExit" }
    $armed = $true
    $safetyArmed = (& $ctl safety | Out-String)
    $safetyArmed | Set-Content (Join-Path $OutDir 'safety-armed.txt') -Encoding UTF8
    if ($safetyArmed -notmatch '(?mi)^\s*armed-phase:\s*1\s*$' -or
        $safetyArmed -notmatch '(?mi)^\s*checkpoint:\s*200\s*$') {
        throw 'Control-read canary did not reach armed checkpoint 200'
    }
    Write-Checkpoint 'arm-complete' 'ok' @{ driver_checkpoint = 200 }

    Write-Checkpoint 'control-read-start' 'pending' @{ command = 'opena8djctl controls'; writes_controls = $false }
    $readExit = Invoke-BoundedCtl @('controls') 'controls'
    if ($readExit -ne 0) { throw "Control read failed: $readExit" }
    Write-Checkpoint 'control-read-returned' 'ok' @{ exit = $readExit }
}
catch {
    $failure = $_.Exception.Message
    Write-Checkpoint 'exception' 'failed' @{ message = $failure }
}
finally {
    if ($armed) {
        $disarmExit = Invoke-BoundedCtl @('disarm') 'disarm'
        Write-Checkpoint 'disarm' $(if ($disarmExit -eq 0) { 'ok' } else { 'failed' }) @{ exit = $disarmExit }
        if ($disarmExit -ne 0 -and $null -eq $failure) { $failure = "Disarm failed: $disarmExit" }
    }
    $safetyAfter = (& $ctl safety | Out-String)
    $diagnosticsAfter = (& $ctl diagnostics | Out-String)
    $safetyAfter | Set-Content (Join-Path $OutDir 'safety-post.txt') -Encoding UTF8
    $diagnosticsAfter | Set-Content (Join-Path $OutDir 'diagnostics-post.txt') -Encoding UTF8
    $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
    if ($lock) { Release-OpenA8DJHardwareLock -Lock $lock }
}

$summary = [ordered]@{
    read_exit = $readExit
    boot_before = $bootBefore.ToString('o')
    boot_after = $bootAfter.ToString('o')
    boot_unchanged = ($bootBefore -eq $bootAfter)
    disarmed = ($safetyAfter -match '(?mi)^\s*armed-phase:\s*0\s*$')
    stream_stopped = ($diagnosticsAfter -match '(?mi)^\s*streaming:\s*no\s*$')
    failure = $failure
    passed = ($null -eq $failure -and $readExit -eq 0 -and $bootBefore -eq $bootAfter -and
        $safetyAfter -match '(?mi)^\s*armed-phase:\s*0\s*$' -and
        $diagnosticsAfter -match '(?mi)^\s*streaming:\s*no\s*$')
}
[IO.File]::WriteAllText(
    (Join-Path $OutDir 'run-summary.json'),
    ($summary | ConvertTo-Json -Depth 6) + "`n",
    [Text.UTF8Encoding]::new($false))
Get-Content (Join-Path $OutDir 'run-summary.json') -Raw
if (-not $summary.passed) { exit 2 }
