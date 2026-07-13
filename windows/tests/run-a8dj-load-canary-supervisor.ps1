param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedBuildFingerprint,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedSysSha256,
    [string]$ExpectedInstanceId = 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___',
    [ValidateRange(90, 300)][int]$GlobalTimeoutSeconds = 180,
    [string]$OutDir = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1') -Force
$packageDir = Join-Path $repoRoot 'windows\dist\Release\x64'
$canaryScript = Join-Path $PSScriptRoot 'run-a8dj-driver-load-canary.ps1'
$sourceRecoveryScript = Join-Path $PSScriptRoot 'recover-a8dj-physical-canary.ps1'
$bundleDir = Join-Path $env:ProgramData 'OpenA8DJCanary'
$recoveryScript = Join-Path $bundleDir 'recover.ps1'
$recoveryStatePath = Join-Path $bundleDir 'recovery-state.json'
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot ('local-analysis\windows\physical-load-supervised-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path
$stdoutPath = Join-Path $OutDir 'load-canary.stdout.txt'
$stderrPath = Join-Path $OutDir 'load-canary.stderr.txt'
$summaryPath = Join-Path $OutDir 'load-supervisor-summary.json'

function Invoke-BoundedProcess {
    param([string]$FilePath, [string[]]$Arguments, [int]$TimeoutSeconds, [string]$StdoutPath, [string]$StderrPath)
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
    $taskkillExit = $null
    $terminationConfirmed = -not $timedOut
    if ($timedOut) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        $taskkillExit = $LASTEXITCODE
        $terminationConfirmed = ($taskkillExit -eq 0 -and $process.WaitForExit(10000))
    } else {
        $process.WaitForExit()
    }
    $exitCode = if ($timedOut) { -1 } else { [int]$process.ExitCode }
    $stdoutText = if ($stdoutTask.Wait(5000)) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
    $stderrText = if ($stderrTask.Wait(5000)) { $stderrTask.GetAwaiter().GetResult() } else { '' }
    [IO.File]::WriteAllText($StdoutPath, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($StderrPath, $stderrText, [Text.UTF8Encoding]::new($false))
    $process.Dispose()
    return [ordered]@{ exit = $exitCode; timed_out = $timedOut; taskkill_exit = $taskkillExit; termination_confirmed = $terminationConfirmed }
}

function Test-DeviceDisabledStable {
    $first = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
    if (-not $first -or [string]$first.Problem -notin @('22', 'CM_PROB_DISABLED')) { return $false }
    Start-Sleep -Seconds 1
    $second = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
    return [bool]($second -and [string]$second.Problem -in @('22', 'CM_PROB_DISABLED'))
}

function Disable-ExpectedDeviceOnly {
    $result = Invoke-BoundedProcess -FilePath 'pnputil.exe' -TimeoutSeconds 20 `
        -Arguments @('/disable-device', $ExpectedInstanceId, '/force') `
        -StdoutPath (Join-Path $OutDir 'emergency-disable.stdout.txt') `
        -StderrPath (Join-Path $OutDir 'emergency-disable.stderr.txt')
    $verified = $result.exit -in @(0, 3010) -and -not $result.timed_out -and
        $result.termination_confirmed -and (Test-DeviceDisabledStable)
    return [ordered]@{ process = $result; verified = [bool]$verified }
}

function Test-RecoveryStateExact {
    if (-not (Test-Path -LiteralPath $recoveryStatePath)) { return $false }
    try {
        $candidate = Get-Content -LiteralPath $recoveryStatePath -Raw | ConvertFrom-Json
        $expectedArtifactReport = Join-Path $OutDir 'recovery-report.json'
        $stateFresh = (Get-Item -LiteralPath $recoveryStatePath).LastWriteTimeUtc -ge $supervisorStartUtc
        $scriptExact = (Test-Path -LiteralPath $recoveryScript) -and
            (Get-FileHash -LiteralPath $recoveryScript -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $sourceRecoveryScript -Algorithm SHA256).Hash
        $published = @(Get-PublishedOpenA8DJPackages)
        $store = @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir)
        return [bool]($stateFresh -and $scriptExact -and
            [IO.Path]::GetFullPath([string]$candidate.artifact_recovery_report) -ieq [IO.Path]::GetFullPath($expectedArtifactReport) -and
            $published.Count -eq 1 -and $published[0] -ieq [string]$candidate.published_name -and
            $store.Count -eq 1 -and [bool]$store[0].Exact -and
            [string]$candidate.instance_id -eq $ExpectedInstanceId -and
            [string]$candidate.build_fingerprint -ieq $ExpectedBuildFingerprint -and
            [string]$candidate.package_sys_sha256 -ieq $ExpectedSysSha256 -and
            [string]$candidate.phase -eq 'LoadInert' -and
            [string]$candidate.recovery_task_name -eq 'OpenA8DJCanaryRecovery' -and
            [string]$candidate.published_name -match '^oem\d+\.inf$')
    } catch { return $false }
}

function Get-PublishedOpenA8DJPackages {
    $found = @()
    $text = (& pnputil.exe /enum-drivers | Out-String)
    foreach ($block in ($text -split '(?im)(?=^\s*Published Name\s*:)')) {
        if ($block -match '(?im)^\s*Original Name\s*:\s*OpenA8DJUsb\.inf\s*$' -and
            $block -match '(?im)^\s*Published Name\s*:\s*(oem\d+\.inf)\s*$') {
            $found += $Matches[1].ToLowerInvariant()
        }
    }
    return @($found)
}

function Invoke-Recovery {
    return Invoke-BoundedProcess -FilePath 'powershell.exe' -TimeoutSeconds 90 `
        -Arguments @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$recoveryScript,'-StatePath',$recoveryStatePath) `
        -StdoutPath (Join-Path $OutDir 'recovery.stdout.txt') -StderrPath (Join-Path $OutDir 'recovery.stderr.txt')
}

function Test-CleanupFinal {
    return [bool]((Test-DeviceDisabledStable) -and
        -not [bool](Get-Service OpenA8DJUsbAcx -ErrorAction SilentlyContinue) -and
        @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir).Count -eq 0)
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not [Security.Principal.WindowsPrincipal]::new($identity).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Load supervisor must run elevated.'
}
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$supervisorStartUtc = (Get-Date).ToUniversalTime()
$manifest = Test-OpenA8DJPackageManifest -PackageDir $packageDir
$manifestSys = [string](@($manifest.files | Where-Object name -eq 'OpenA8DJUsb.sys')[0].sha256)
if ([string]$manifest.build_fingerprint -ine $ExpectedBuildFingerprint -or $manifestSys -ine $ExpectedSysSha256) {
    throw 'Package identity does not match the supervised load candidate.'
}
$deviceBefore = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction Stop
if ([string]$deviceBefore.Problem -notin @('22','CM_PROB_DISABLED') -or
    [bool](Get-Service OpenA8DJUsbAcx -ErrorAction SilentlyContinue) -or
    @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir).Count -ne 0) {
    throw 'Supervised load requires a disabled device, absent service, and clean DriverStore.'
}
$staleTask = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction SilentlyContinue
if ($staleTask) { throw 'A pre-existing recovery task must be resolved before supervised load.' }
foreach ($stalePath in @($recoveryStatePath, $recoveryScript)) {
    if (Test-Path -LiteralPath $stalePath) { Remove-Item -LiteralPath $stalePath -Force }
}

$loadResult = $null
$recoveryResult = $null
$emergencyDisable = $null
$rollbackVerified = $null
$passed = $false
$failure = $null
$loadStarted = $false
try {
    $arguments = @(
        '-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',$canaryScript,
        '-PackageDir',$packageDir,'-Phase','LoadInert','-ExpectedInstanceId',$ExpectedInstanceId,
        '-ProcessTimeoutSeconds','90','-AllowPhysicalLoad','-AcknowledgeCrashRisk','-KeepInstalled','-OutDir',$OutDir
    )
    $loadStarted = $true
    $loadResult = Invoke-BoundedProcess -FilePath 'powershell.exe' -Arguments $arguments `
        -TimeoutSeconds $GlobalTimeoutSeconds -StdoutPath $stdoutPath -StderrPath $stderrPath
    if ($loadResult.timed_out) { throw "Load canary exceeded ${GlobalTimeoutSeconds}s." }
    if ($loadResult.exit -ne 0) { throw "Load canary exited with code $($loadResult.exit)." }
    $canarySummary = Get-Content -LiteralPath (Join-Path $OutDir 'summary.json') -Raw | ConvertFrom-Json
    if (-not [bool]$canarySummary.keep_installed -or $null -ne $canarySummary.error -or
        [string]$canarySummary.build_fingerprint -ine $ExpectedBuildFingerprint -or
        [string]$canarySummary.sys_sha256 -ine $ExpectedSysSha256) {
        throw 'Load canary summary did not prove the exact installed candidate.'
    }
    $checkpoints = @(Get-Content -LiteralPath (Join-Path $OutDir 'checkpoints.jsonl') | ForEach-Object { $_ | ConvertFrom-Json })
    if ($checkpoints.Count -eq 0 -or @($checkpoints | Where-Object status -eq 'failed').Count -ne 0 -or
        @($checkpoints.boot_time_utc | Select-Object -Unique).Count -ne 1) { throw 'Load checkpoints are incomplete, failed, or crossed a reboot.' }
    if (-not (Test-RecoveryStateExact)) { throw 'Recovery state is not exact after supervised load.' }
    $safety = Get-Content -LiteralPath (Join-Path $OutDir 'safety-after-disarm.txt') -Raw
    if ($safety -notmatch '(?mi)^\s*api-version:\s*44\s*$' -or
        $safety -notmatch ('(?mi)^\s*build-fingerprint:\s*' + [regex]::Escape($ExpectedBuildFingerprint) + '\s*$') -or
        $safety -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
        $safety -notmatch '(?mi)^\s*operations-left:\s*0\s*$' -or
        $safety -notmatch '(?mi)^\s*nonce:\s*0{32}\s*$' -or
        $safety -notmatch '(?mi)^\s*prepared/stopping/worker:\s*1/0/0\s*$') { throw 'Post-load safety state is not clean.' }
    $serviceStart = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx' -Name Start -ErrorAction Stop).Start
    $deviceAfter = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction Stop
    $activeInf = [string](Get-PnpDeviceProperty -InstanceId $ExpectedInstanceId -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction Stop).Data
    $state = Get-Content -LiteralPath $recoveryStatePath -Raw | ConvertFrom-Json
    if ([int]$serviceStart -ne 4 -or [string]$deviceAfter.Status -ne 'OK' -or
        [string]$deviceAfter.Problem -notin @('0','CM_PROB_NONE') -or $activeInf -ine [string]$state.published_name -or
        @(Get-OpenA8DJDriverStoreMatches -PackageDir $packageDir).Count -ne 1) { throw 'Final loaded PnP/service/DriverStore state is not exact.' }
    $task = Get-ScheduledTask -TaskName 'OpenA8DJCanaryRecovery' -ErrorAction Stop
    $actions = @($task.Actions)
    $expectedTaskArguments = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -File $recoveryScript -StatePath $recoveryStatePath"
    if ([string]$task.State -ne 'Ready' -or $actions.Count -ne 1 -or
        [IO.Path]::GetFileName([string]$actions[0].Execute) -ine 'powershell.exe' -or
        [string]$actions[0].Arguments -cne $expectedTaskArguments) {
        throw 'Recovery task is not exact and Ready after supervised load.'
    }
    if ((Get-CimInstance Win32_OperatingSystem).LastBootUpTime -ne $bootBefore) { throw 'Boot identity changed during supervised load.' }
    $passed = $true
} catch {
    $failure = $_.Exception.Message
} finally {
    if (-not $passed) {
        $treeSafe = -not $loadStarted -or ($null -ne $loadResult -and [bool]$loadResult.termination_confirmed)
        if (-not $treeSafe) {
            try { $emergencyDisable = Disable-ExpectedDeviceOnly; $treeSafe = [bool]$emergencyDisable.verified } catch {}
        }
        if ($treeSafe -and (Test-RecoveryStateExact) -and (Test-Path -LiteralPath $recoveryScript)) {
            try { $recoveryResult = Invoke-Recovery } catch {}
            try { $rollbackVerified = $recoveryResult -and -not $recoveryResult.timed_out -and $recoveryResult.exit -eq 0 -and (Test-CleanupFinal) } catch { $rollbackVerified = $false }
            if (-not $rollbackVerified) {
                try { $emergencyDisable = Disable-ExpectedDeviceOnly } catch {}
            }
        } else {
            try { $emergencyDisable = Disable-ExpectedDeviceOnly } catch {}
            $rollbackVerified = $false
        }
    }
    $report = [ordered]@{
        passed = $passed; failure = $failure; boot_before = $bootBefore.ToString('o');
        boot_after = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToString('o');
        load = $loadResult; recovery = $recoveryResult; emergency_disable = $emergencyDisable;
        rollback_verified = $rollbackVerified; out_dir = $OutDir
    }
    [IO.File]::WriteAllText($summaryPath, ($report | ConvertTo-Json -Depth 8) + "`n", [Text.UTF8Encoding]::new($false))
}
Get-Content -LiteralPath $summaryPath -Raw
if (-not $passed) { exit 2 }
