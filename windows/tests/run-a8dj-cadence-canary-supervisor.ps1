param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedBuildFingerprint,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedSysSha256,
    [string]$ExpectedInstanceId = 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___',
    [ValidateSet('render', 'capture', 'render,capture')][string]$Directions = 'render',
    [ValidateSet('cadence', 'restart', 'quality', 'duplex')][string]$ProbeMode = 'cadence',
    [ValidateRange(1, 10)][int]$RestartRepeats = 1,
    [ValidateRange(100, 2000)][int]$StreamMilliseconds = 200,
    [ValidateRange(5, 300)][int]$Seconds = 5,
    [ValidateRange(1, 30)][int]$BaselineSeconds = 5,
    [ValidateRange(16, 4096)][int]$BlockSize = 512,
    [switch]$DuplexExclusive,
    [ValidateSet('low', 'high')][string]$DuplexLatency = 'high',
    [string]$MusicFile = '',
    [string]$QualityBaselineJson = '',
    [ValidateRange(0.001, 0.20)][double]$Amplitude = 0.02,
    [ValidateSet(44100, 48000)][int]$OutputRate = 48000,
    [ValidateSet(44100, 48000)][int]$InputRate = 44100,
    [ValidateRange(1, 4)][int]$OutputPair = 1,
    [string]$RecoveryStatePath = "$env:ProgramData\OpenA8DJCanary\recovery-state.json",
    [ValidateRange(30, 840)][int]$GlobalTimeoutSeconds = 45,
    [ValidateRange(30, 900)][int]$ProcessTimeoutSeconds = 30,
    [string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1') -Force
$packageDir = Join-Path $repoRoot 'windows\dist\Release\x64'
$cadenceScript = Join-Path $PSScriptRoot 'run-a8dj-cadence-diagnostic.ps1'
$qualityScript = Join-Path $PSScriptRoot 'run-a8dj-physical-music-quality.ps1'
$duplexScript = Join-Path $PSScriptRoot 'run-a8dj-duplex-soak.ps1'
$recoveryScript = Join-Path $env:ProgramData 'OpenA8DJCanary\recover.ps1'
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ('local-analysis\windows\cadence-canary-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$supervisorSummaryPath = Join-Path $OutDir 'supervisor-summary.json'
$stdoutPath = Join-Path $OutDir 'cadence.stdout.txt'
$stderrPath = Join-Path $OutDir 'cadence.stderr.txt'

function Invoke-BoundedProcess {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [Parameter(Mandatory = $true)][string]$StdoutPath,
        [Parameter(Mandatory = $true)][string]$StderrPath
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
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timedOut = -not $process.WaitForExit($TimeoutSeconds * 1000)
    $terminationConfirmed = -not $timedOut
    $taskkillExit = $null
    if ($timedOut) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        $taskkillExit = $LASTEXITCODE
        $parentExited = $process.WaitForExit(10000)
        $terminationConfirmed = ($taskkillExit -eq 0 -and $parentExited)
    } else {
        $process.WaitForExit()
    }
    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }
    $stdoutText = if ($stdoutTask.Wait(5000)) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
    $stderrText = if ($stderrTask.Wait(5000)) { $stderrTask.GetAwaiter().GetResult() } else { '' }
    [IO.File]::WriteAllText($StdoutPath, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($StderrPath, $stderrText, [Text.UTF8Encoding]::new($false))
    $process.Dispose()
    return [ordered]@{
        exit = $exitCode
        timed_out = $timedOut
        taskkill_exit = $taskkillExit
        termination_confirmed = $terminationConfirmed
    }
}

function Get-PublishedOpenA8DJPackage {
    $text = (& pnputil.exe /enum-drivers | Out-String)
    foreach ($block in ($text -split '(?im)(?=^\s*Published Name\s*:)')) {
        if ($block -match '(?im)^\s*Original Name\s*:\s*OpenA8DJUsb\.inf\s*$' -and
            $block -match '(?im)^\s*Published Name\s*:\s*(oem\d+\.inf)\s*$') {
            return $Matches[1].ToLowerInvariant()
        }
    }
    return $null
}

function Disable-ExpectedDeviceOnly {
    $stdout = Join-Path $OutDir 'emergency-disable.stdout.txt'
    $stderr = Join-Path $OutDir 'emergency-disable.stderr.txt'
    $processResult = Invoke-BoundedProcess -FilePath 'pnputil.exe' -TimeoutSeconds 20 `
        -Arguments @('/disable-device', $ExpectedInstanceId, '/force') `
        -StdoutPath $stdout -StderrPath $stderr
    $stableDisabled = $false
    if ($processResult.exit -in @(0, 3010) -and -not $processResult.timed_out -and $processResult.termination_confirmed) {
        $first = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
        if ($first -and [string]$first.Problem -in @('22', 'CM_PROB_DISABLED')) {
            Start-Sleep -Seconds 1
            $second = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
            $stableDisabled = $second -and [string]$second.Problem -in @('22', 'CM_PROB_DISABLED')
        }
    }
    return [ordered]@{ process = $processResult; verified = [bool]$stableDisabled }
}

function Read-Scalar([string]$Text, [string]$Name) {
    $match = [regex]::Match($Text, '(?m)^\s*' + [regex]::Escape($Name) + ':\s*(\d+)')
    if (-not $match.Success) { throw "Missing diagnostic scalar: $Name" }
    return [int64]$match.Groups[1].Value
}

function Invoke-Recovery {
    if (-not (Test-Path -LiteralPath $recoveryScript)) { throw "Recovery script missing: $recoveryScript" }
    $recoveryStdout = Join-Path $OutDir 'recovery.stdout.txt'
    $recoveryStderr = Join-Path $OutDir 'recovery.stderr.txt'
    return Invoke-BoundedProcess -FilePath 'powershell.exe' -TimeoutSeconds 90 `
        -Arguments @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $recoveryScript, '-StatePath', $RecoveryStatePath) `
        -StdoutPath $recoveryStdout -StderrPath $recoveryStderr
}

function Test-RecoveryFinalState {
    $device = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
    $deviceDisabled = $device -and [string]$device.Problem -in @('22', 'CM_PROB_DISABLED')
    $serviceAbsent = -not [bool](Get-Service OpenA8DJUsbAcx -ErrorAction SilentlyContinue)
    $exactPackageAbsent = @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir).Count -eq 0
    $publishedPackageAbsent = $null -eq (Get-PublishedOpenA8DJPackage)
    return [ordered]@{
        passed = [bool]($deviceDisabled -and $serviceAbsent -and $exactPackageAbsent -and $publishedPackageAbsent)
        device_disabled = [bool]$deviceDisabled
        service_absent = [bool]$serviceAbsent
        exact_package_absent = [bool]$exactPackageAbsent
        published_package_absent = [bool]$publishedPackageAbsent
    }
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Cadence supervisor must run elevated.'
}

$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$failure = $null
$cadenceResult = $null
$recoveryResult = $null
$emergencyDisableResult = $null
$rollbackFinalState = $null
$passed = $false
$state = $null
$stateValidated = $false
$cadenceStarted = $false
$preflightProcessesSafe = $true
try {
    $state = Get-Content -LiteralPath $RecoveryStatePath -Raw | ConvertFrom-Json
    if ([string]$state.instance_id -ne $ExpectedInstanceId -or
        [string]$state.build_fingerprint -ine $ExpectedBuildFingerprint -or
        [string]$state.package_sys_sha256 -ine $ExpectedSysSha256 -or
        [string]$state.published_name -notmatch '^oem\d+\.inf$' -or
        [string]$state.phase -ne 'LoadInert' -or
        [string]$state.recovery_task_name -ne 'OpenA8DJCanaryRecovery') {
        throw 'Recovery state does not match the exact cadence candidate.'
    }
    $task = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction Stop
    if ([string]$task.State -ne 'Ready') { throw 'Recovery task is not Ready.' }
    $actions = @($task.Actions)
    $expectedTaskArguments = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File $recoveryScript -StatePath $RecoveryStatePath"
    if ($actions.Count -ne 1 -or
        [IO.Path]::GetFileName([string]$actions[0].Execute) -ine 'powershell.exe' -or
        [string]$actions[0].Arguments -cne $expectedTaskArguments) {
        throw 'Recovery task action does not target the exact recovery bundle.'
    }
    $manifest = Test-OpenA8DJPackageManifest -PackageDir $packageDir
    $manifestSys = [string](@($manifest.files | Where-Object name -eq 'OpenA8DJUsb.sys')[0].sha256)
    if ([string]$manifest.build_fingerprint -ine $ExpectedBuildFingerprint -or $manifestSys -ine $ExpectedSysSha256) {
        throw 'Local package manifest does not match the exact cadence candidate.'
    }
    $storeMatches = @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir)
    if ($storeMatches.Count -ne 1 -or -not [bool]$storeMatches[0].Exact) {
        throw 'DriverStore does not contain exactly one hash-exact cadence candidate.'
    }
    $publishedPackage = Get-PublishedOpenA8DJPackage
    if ($publishedPackage -ne ([string]$state.published_name).ToLowerInvariant()) {
        throw 'Recovery-state OEM INF is not the published OpenA8DJ package.'
    }
    $start = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx' -Name Start -ErrorAction Stop).Start
    if ([int]$start -ne 4) { throw 'Driver service is not next-boot disabled (Start=4).' }
    $device = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction Stop
    if ([string]$device.Status -ne 'OK' -or [string]$device.Problem -notin @('0', 'CM_PROB_NONE')) {
        throw 'Exact physical device is not healthy before cadence.'
    }
    $activeInf = [string](Get-PnpDeviceProperty -InstanceId $ExpectedInstanceId -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction Stop).Data
    if ($activeInf -ine [string]$state.published_name) { throw 'Exact device is not bound to the recovery-state OEM INF.' }
    $stateValidated = $true

    $env:OPENA8DJ_INSTANCE_ID = $ExpectedInstanceId
    $preflightSafetyResult = Invoke-BoundedProcess -FilePath (Join-Path $packageDir 'opena8djctl.exe') `
        -Arguments @('safety') -TimeoutSeconds 10 `
        -StdoutPath (Join-Path $OutDir 'supervisor-safety-preflight.txt') `
        -StderrPath (Join-Path $OutDir 'supervisor-safety-preflight.stderr.txt')
    if (-not $preflightSafetyResult.termination_confirmed) {
        $preflightProcessesSafe = $false
        throw 'Supervisor safety preflight process termination was not confirmed.'
    }
    if ($preflightSafetyResult.exit -ne 0 -or $preflightSafetyResult.timed_out) {
        throw 'Bounded supervisor safety preflight failed.'
    }
    $preflightDiagnosticsResult = Invoke-BoundedProcess -FilePath (Join-Path $packageDir 'opena8djctl.exe') `
        -Arguments @('diagnostics') -TimeoutSeconds 10 `
        -StdoutPath (Join-Path $OutDir 'supervisor-diagnostics-preflight.txt') `
        -StderrPath (Join-Path $OutDir 'supervisor-diagnostics-preflight.stderr.txt')
    if (-not $preflightDiagnosticsResult.termination_confirmed) {
        $preflightProcessesSafe = $false
        throw 'Supervisor diagnostics preflight process termination was not confirmed.'
    }
    if ($preflightDiagnosticsResult.exit -ne 0 -or $preflightDiagnosticsResult.timed_out) {
        throw 'Bounded supervisor diagnostics preflight failed.'
    }
    $preflightSafety = Get-Content -LiteralPath (Join-Path $OutDir 'supervisor-safety-preflight.txt') -Raw
    $preflightDiagnostics = Get-Content -LiteralPath (Join-Path $OutDir 'supervisor-diagnostics-preflight.txt') -Raw
    if ($preflightDiagnostics -notmatch '(?mi)^\s*streaming:\s*no\s*$' -or
        $preflightDiagnostics -notmatch '(?mi)^\s*iso-input-reset:\s*runs=\d+\s+failures=0\s+status=0x00000000\s*$' -or
        $preflightDiagnostics -notmatch '(?mi)^\s*iso-output-reset:\s*runs=\d+\s+failures=0\s+status=0x00000000\s*$' -or
        $preflightDiagnostics -notmatch '(?mi)^\s*iso-transport:\s*healthy=1\s+generation=-?\d+\s+one-shot=0\s+input-start=0x00000000\s+output-start=0x00000000\s*$' -or
        $preflightSafety -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
        $preflightSafety -notmatch '(?mi)^\s*operations-left:\s*0\s*$' -or
        $preflightSafety -notmatch '(?mi)^\s*nonce:\s*0{32}\s*$' -or
        $preflightSafety -notmatch '(?mi)^\s*prepared/stopping/worker:\s*1/0/0\s*$') {
        throw 'Supervisor preflight found an armed, streaming, or active worker state.'
    }

    $effectiveProcessTimeout = [Math]::Max($ProcessTimeoutSeconds, $GlobalTimeoutSeconds + 30)
    $arguments = if ($ProbeMode -eq 'quality') {
        if (-not $MusicFile -or -not $QualityBaselineJson) {
            throw 'Quality mode requires MusicFile and QualityBaselineJson.'
        }
        @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $qualityScript,
            '-MusicFile', $MusicFile, '-Seconds', [string]$Seconds, '-Amplitude', [string]$Amplitude,
            '-OutputRate', [string]$OutputRate, '-InputRate', [string]$InputRate,
            '-BlockSize', [string]$BlockSize, '-OutputPair', [string]$OutputPair,
            '-ExpectedApiVersion', '44', '-ExpectedSysSha256', $ExpectedSysSha256,
            '-ProcessTimeoutSeconds', [string]$effectiveProcessTimeout,
            '-BaselineJson', $QualityBaselineJson, '-OutDir', $OutDir)
    } elseif ($ProbeMode -eq 'duplex') {
        @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $duplexScript,
            '-Seconds', [string]$Seconds, '-Rate', [string]$OutputRate,
            '-BlockSize', [string]$BlockSize, '-Amplitude', [string]$Amplitude,
            '-Latency', $DuplexLatency,
            '-ProgressSeconds', '5', '-PnpMonitorIntervalSeconds', '5',
            '-ExpectedApiVersion', '44', '-ExpectedBuildFingerprint', $ExpectedBuildFingerprint,
            '-ProcessTimeoutSeconds', [string]$effectiveProcessTimeout,
            '-OutDir', $OutDir)
    } else {
        @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', $cadenceScript,
            '-Seconds', [string]$Seconds, '-Rate', [string]$OutputRate, '-BlockSize', [string]$BlockSize, '-Directions', $Directions,
            '-BaselineSeconds', [string]$BaselineSeconds, '-ProcessTimeoutSeconds', [string]$effectiveProcessTimeout,
            '-ProbeMode', $ProbeMode, '-RestartRepeats', [string]$RestartRepeats,
            '-StreamMilliseconds', [string]$StreamMilliseconds,
            '-ExpectedBuildFingerprint', $ExpectedBuildFingerprint,
            '-ExpectedSysSha256', $ExpectedSysSha256,
            '-ExpectedInstanceId', $ExpectedInstanceId, '-OutDir', $OutDir)
    }
    if ($ProbeMode -eq 'duplex' -and $DuplexExclusive) {
        $arguments += '-Exclusive'
    }
    $cadenceStarted = $true
    $cadenceResult = Invoke-BoundedProcess -FilePath 'powershell.exe' -Arguments $arguments `
        -TimeoutSeconds $GlobalTimeoutSeconds -StdoutPath $stdoutPath -StderrPath $stderrPath
    if ($cadenceResult.timed_out) { throw "Cadence exceeded the global ${GlobalTimeoutSeconds}s deadline." }
    if ($cadenceResult.exit -ne 0) { throw "Cadence exited with code $($cadenceResult.exit)." }

    $runSummaryPath = Join-Path $OutDir 'run-summary.json'
    if (-not (Test-Path -LiteralPath $runSummaryPath)) { throw 'Cadence run summary is missing.' }
    $run = Get-Content -LiteralPath $runSummaryPath -Raw | ConvertFrom-Json
    if (-not [bool]$run.passed) { throw 'Cadence run summary did not pass.' }
    $counterObject = if ($ProbeMode -in @('quality', 'duplex')) { $run.driver_counter_deltas } else { $run.counter_deltas }
    $expectedCounterCount = if ($ProbeMode -in @('quality', 'duplex')) { 13 } else { 14 }
    $counterProperties = @($counterObject.PSObject.Properties)
    if ($counterProperties.Count -ne $expectedCounterCount -or @($counterProperties | Where-Object { $null -eq $_.Value -or [int64]$_.Value -ne 0 }).Count -ne 0) {
        throw 'Cadence error-counter gate is incomplete or nonzero.'
    }
    $preText = Get-Content -LiteralPath (Join-Path $OutDir 'driver-diagnostics-pre.txt') -Raw
    $postText = Get-Content -LiteralPath (Join-Path $OutDir 'driver-diagnostics-post.txt') -Raw
    $postSafety = Get-Content -LiteralPath (Join-Path $OutDir 'driver-safety-post.txt') -Raw
    $activityNames = @('worker-iterations','worker-cap-bytes','worker-play-bytes','usb-in-packets','usb-out-packets')
    # Quality playback is rendered by Audio 8 DJ but captured by the physically
    # connected iRig. Directions applies only to cadence/restart probes; it must
    # not make the supervisor expect OpenA8DJ capture endpoint activity here.
    if ($ProbeMode -eq 'quality') {
        $activityNames += 'render-frames'
    } elseif ($ProbeMode -eq 'duplex') {
        $activityNames += @('render-frames', 'capture-frames')
    } else {
        if ($Directions -match 'render' -or $ProbeMode -eq 'restart') { $activityNames += 'render-frames' }
        if ($Directions -match 'capture' -or $ProbeMode -eq 'restart') { $activityNames += 'capture-frames' }
    }
    foreach ($name in $activityNames) {
        if ((Read-Scalar $postText $name) -le (Read-Scalar $preText $name)) {
            throw "Required activity did not advance: $name"
        }
    }
    $preQuery = [regex]::Match($preText, '(?m)^\s*iso-frame-query:\s*runs=(\d+)\s+failures=(\d+)')
    $postQuery = [regex]::Match($postText, '(?m)^\s*iso-frame-query:\s*runs=(\d+)\s+failures=(\d+)')
    $expectedQueryDelta = if ($ProbeMode -eq 'restart') { $RestartRepeats * 56 } elseif ($ProbeMode -in @('quality', 'duplex')) { 1 } else { @($Directions -split ',').Count }
    # A duplex session configures the shared transport when the first stream
    # enters Run, then ACX tears down render and capture separately.  Those
    # three bounded lifecycle transitions reset both pipes.  They are expected
    # only when the exact count is three and both failure counters stay zero.
    $expectedResetDelta = if ($ProbeMode -eq 'duplex') { 3 } else { $expectedQueryDelta }
    if (-not $preQuery.Success -or -not $postQuery.Success -or
        ([int64]$postQuery.Groups[1].Value - [int64]$preQuery.Groups[1].Value) -ne $expectedQueryDelta -or
        [int64]$postQuery.Groups[2].Value -ne [int64]$preQuery.Groups[2].Value) {
        throw 'USB frame-query activity/failure gate did not pass.'
    }
    $preInputReset = [regex]::Match($preText, '(?m)^\s*iso-input-reset:\s*runs=(\d+)\s+failures=(\d+)\s+status=0x([0-9a-fA-F]+)')
    $postInputReset = [regex]::Match($postText, '(?m)^\s*iso-input-reset:\s*runs=(\d+)\s+failures=(\d+)\s+status=0x([0-9a-fA-F]+)')
    $preOutputReset = [regex]::Match($preText, '(?m)^\s*iso-output-reset:\s*runs=(\d+)\s+failures=(\d+)\s+status=0x([0-9a-fA-F]+)')
    $postOutputReset = [regex]::Match($postText, '(?m)^\s*iso-output-reset:\s*runs=(\d+)\s+failures=(\d+)\s+status=0x([0-9a-fA-F]+)')
    if (-not $preInputReset.Success -or -not $postInputReset.Success -or
        -not $preOutputReset.Success -or -not $postOutputReset.Success -or
        ([int64]$postInputReset.Groups[1].Value - [int64]$preInputReset.Groups[1].Value) -ne $expectedResetDelta -or
        ([int64]$postOutputReset.Groups[1].Value - [int64]$preOutputReset.Groups[1].Value) -ne $expectedResetDelta -or
        [int64]$postInputReset.Groups[2].Value -ne [int64]$preInputReset.Groups[2].Value -or
        [int64]$postOutputReset.Groups[2].Value -ne [int64]$preOutputReset.Groups[2].Value -or
        [Convert]::ToUInt32($postInputReset.Groups[3].Value, 16) -ne 0 -or
        [Convert]::ToUInt32($postOutputReset.Groups[3].Value, 16) -ne 0) {
        throw 'Input/output reset activity/failure gate did not pass.'
    }
    if ($postText -notmatch '(?mi)^\s*streaming:\s*no\s*$' -or
        $postText -notmatch '(?mi)^\s*iso-transport:\s*healthy=1\s+generation=-?\d+\s+one-shot=0\s+input-start=0x00000000\s+output-start=0x00000000\s*$' -or
        $postSafety -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
        $postSafety -notmatch '(?mi)^\s*operations-left:\s*0\s*$' -or
        $postSafety -notmatch '(?mi)^\s*nonce:\s*0{32}\s*$' -or
        $postSafety -notmatch '(?mi)^\s*prepared/stopping/worker:\s*1/0/0\s*$' -or
        $postSafety -notmatch '(?mi)^\s*checkpoint:\s*509\s*$' -or
        $postSafety -notmatch '(?mi)^\s*checkpoint-status:\s*0x00000000\s*$') {
        throw 'Final stream/disarm/worker/checkpoint safety state is not clean.'
    }
    $startAfter = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx' -Name Start -ErrorAction Stop).Start
    if ([int]$startAfter -ne 4) { throw 'Driver service lost its next-boot Start=4 fail-safe.' }
    $taskAfter = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction Stop
    if ([string]$taskAfter.State -ne 'Ready') { throw 'Recovery task is not Ready after cadence.' }
    $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
    if ($bootAfter -ne $bootBefore) { throw 'Boot identity changed during cadence.' }
    $passed = $true
} catch {
    $failure = $_.Exception.Message
} finally {
    if (-not $passed) {
        if (-not $stateValidated) {
            try { $emergencyDisableResult = Disable-ExpectedDeviceOnly } catch {
                $emergencyDisableResult = [ordered]@{ exit = -2; timed_out = $false; termination_confirmed = $true; error = $_.Exception.Message }
            }
        } else {
            $treeSafeForRecovery = $preflightProcessesSafe -and (
                -not $cadenceStarted -or
                ($null -ne $cadenceResult -and [bool]$cadenceResult.termination_confirmed))
            if (-not $treeSafeForRecovery) {
                try {
                    $emergencyDisableResult = Disable-ExpectedDeviceOnly
                    $treeSafeForRecovery = $preflightProcessesSafe -and [bool]$emergencyDisableResult.verified
                } catch {
                    $emergencyDisableResult = [ordered]@{ exit = -2; timed_out = $false; termination_confirmed = $true; error = $_.Exception.Message }
                }
            }
            if ($treeSafeForRecovery) {
                try { $recoveryResult = Invoke-Recovery } catch {
                    $recoveryResult = [ordered]@{ exit = -2; timed_out = $false; termination_confirmed = $true; error = $_.Exception.Message }
                }
                try { $rollbackFinalState = Test-RecoveryFinalState } catch {
                    $rollbackFinalState = [ordered]@{ passed = $false; error = $_.Exception.Message }
                }
                if ($null -eq $recoveryResult -or [bool]$recoveryResult.timed_out -or
                    [int]$recoveryResult.exit -ne 0 -or -not [bool]$rollbackFinalState.passed) {
                    try { $emergencyDisableResult = Disable-ExpectedDeviceOnly } catch {}
                }
            } else {
                $rollbackFinalState = [ordered]@{ passed = $false; error = 'Cadence tree termination and emergency disable were not confirmed; package deletion was withheld.' }
            }
        }
    }
    $summary = [ordered]@{
        passed = $passed
        failure = $failure
        boot_before = $bootBefore.ToString('o')
        boot_after = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToString('o')
        cadence = $cadenceResult
        recovery = $recoveryResult
        emergency_disable = $emergencyDisableResult
        rollback_final_state = $rollbackFinalState
        recovery_state_validated = $stateValidated
        recovery_state = $RecoveryStatePath
        out_dir = $OutDir
    }
    [IO.File]::WriteAllText($supervisorSummaryPath, ($summary | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
}

Get-Content -LiteralPath $supervisorSummaryPath -Raw
if (-not $passed) { exit 2 }
