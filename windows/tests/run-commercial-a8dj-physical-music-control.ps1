param(
    [Parameter(Mandatory = $true)]
    [string]$MusicFile,
    [ValidateSet('LegacyMme', 'Wasapi')]
    [string]$Mode = 'LegacyMme',
    [ValidateRange(3, 120)]
    [int]$Seconds = 6,
    [ValidateRange(0.001, 0.20)]
    [double]$Amplitude = 0.02,
    [ValidateRange(30, 600)]
    [int]$ProcessTimeoutSeconds = 90,
    [string]$OutDir = '',
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$common = Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1'
$legacyProbe = Join-Path $repoRoot 'windows\tests\irig_music_probe.py'
$wasapiProbe = Join-Path $repoRoot 'windows\tests\wasapi_irig_tone_probe.py'
$instanceId = 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___'
Import-Module $common -Force

foreach ($required in @($MusicFile, $Python, $legacyProbe, $wasapiProbe)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required path is missing: $required"
    }
}
$MusicFile = (Resolve-Path -LiteralPath $MusicFile).Path
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot (
        'local-analysis\commercial-control-{0}-{1}' -f $Mode.ToLowerInvariant(),
        (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

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
    if (-not $process.Start()) { throw "Could not start process: $FilePath" }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    try { $process.PriorityClass = 'High' } catch {}
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        if (-not $process.WaitForExit(5000)) {
            throw "Process timed out and PID $($process.Id) did not confirm termination"
        }
        throw "Process timed out after $TimeoutSeconds seconds and its process tree was terminated"
    }
    $process.WaitForExit()
    $stdout = if ($stdoutTask.Wait(5000)) { $stdoutTask.GetAwaiter().GetResult() } else { '' }
    $stderr = if ($stderrTask.Wait(5000)) { $stderrTask.GetAwaiter().GetResult() } else { '' }
    [IO.File]::WriteAllText($StdoutPath, $stdout, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($StderrPath, $stderr, [Text.UTF8Encoding]::new($false))
    if ($process.ExitCode -ne 0) {
        throw "Process failed with exit code $($process.ExitCode): $stderr"
    }
}

$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$driver = Get-CimInstance Win32_PnPSignedDriver |
    Where-Object DeviceID -eq $instanceId |
    Select-Object -First 1
if ($null -eq $driver -or $driver.Manufacturer -ne 'Native Instruments' -or
    $driver.InfName -ne 'oem44.inf' -or -not $driver.IsSigned) {
    throw 'The signed Native Instruments commercial driver is not active on the exact Audio 8 DJ device'
}
$device = Get-PnpDevice -InstanceId $instanceId -ErrorAction Stop
if ($device.Status -ne 'OK' -or $device.Problem -ne 'CM_PROB_NONE') {
    throw "Commercial Audio 8 DJ device is not healthy: status=$($device.Status) problem=$($device.Problem)"
}

$lock = $null
$chatProcesses = @()
$savedPriorities = @{}
$failure = $null
try {
    $lock = Acquire-OpenA8DJHardwareLock -Gate "commercial-$Mode-quality-control" -RunDir $OutDir -TimeoutSeconds 0
    $chatProcesses = @(Get-Process ChatGPT -ErrorAction SilentlyContinue)
    foreach ($process in $chatProcesses) {
        try {
            $savedPriorities[$process.Id] = $process.PriorityClass
            $process.PriorityClass = 'Idle'
        } catch {}
    }

    if ($Mode -eq 'LegacyMme') {
        $arguments = @(
            $legacyProbe,
            '--music-file', $MusicFile,
            '--input-name', 'Line In (iRig Stream)',
            '--output-name', 'Audio 8 DJ (Ch A, Out 1|2)',
            '--hostapi', 'MME',
            '--rate', '48000',
            '--seconds', [string]$Seconds,
            '--start', '30',
            '--target-peak', [string]$Amplitude,
            '--blocksize', '512',
            '--output-channels', '2',
            '--output-pair', '1',
            '--out-dir', $OutDir)
        $probe = $legacyProbe
    } else {
        $arguments = @(
            $wasapiProbe,
            '--seconds', [string]$Seconds,
            '--amplitude', [string]$Amplitude,
            '--signal', 'multi',
            '--output-rate', '48000',
            '--input-rate', '44100',
            '--output-channels', '2',
            '--output-pair', '1',
            '--output-blocksize', '512',
            '--input-blocksize', '512',
            '--output-name', 'Audio 8 DJ (Ch A, Out 1|2) (Audio 8 DJ WDM Audio)',
            '--output-hostapi', 'Windows WASAPI',
            '--input-name', 'Line In (iRig Stream)',
            '--input-hostapi', 'Windows DirectSound',
            '--reference-file', $MusicFile,
            '--out-dir', $OutDir)
        $probe = $wasapiProbe
    }

    Invoke-BoundedProcess -FilePath $Python -Arguments $arguments `
        -StdoutPath (Join-Path $OutDir 'probe.stdout.txt') `
        -StderrPath (Join-Path $OutDir 'probe.stderr.txt') `
        -TimeoutSeconds $ProcessTimeoutSeconds
} catch {
    $failure = $_.Exception.Message
} finally {
    foreach ($process in $chatProcesses) {
        if ($savedPriorities.ContainsKey($process.Id)) {
            try { $process.PriorityClass = $savedPriorities[$process.Id] } catch {}
        }
    }
    if ($lock) { Release-OpenA8DJHardwareLock -Lock $lock }
}

$bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$summary = [ordered]@{
    schema_version = 1
    verdict = if ($failure) { 'FAIL' } elseif ($bootAfter -ne $bootBefore) { 'FAIL' } else { 'PASS' }
    mode = $Mode
    driver_provider = [string]$driver.DriverProviderName
    driver_version = [string]$driver.DriverVersion
    driver_inf = [string]$driver.InfName
    driver_signed = [bool]$driver.IsSigned
    boot_before = $bootBefore.ToUniversalTime().ToString('o')
    boot_after = $bootAfter.ToUniversalTime().ToString('o')
    boot_unchanged = ($bootAfter -eq $bootBefore)
    failure = $failure
    out_dir = $OutDir
}
[IO.File]::WriteAllText(
    (Join-Path $OutDir 'commercial-control-summary.json'),
    ($summary | ConvertTo-Json -Depth 4) + "`n",
    [Text.UTF8Encoding]::new($false))
if ($summary.verdict -ne 'PASS') {
    throw "Commercial quality control failed: failure=$failure boot_unchanged=$($summary.boot_unchanged)"
}
Write-Host "Commercial quality control PASS: $OutDir"
Get-Content -LiteralPath (Join-Path $OutDir 'metrics.json') -Raw
