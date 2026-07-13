param(
    [Parameter(Mandatory = $true)]
    [string]$MusicFile,
    [ValidateRange(3, 120)]
    [int]$Seconds = 20,
    [ValidateRange(0.001, 0.20)]
    [double]$Amplitude = 0.02,
    [ValidateSet(44100, 48000)]
    [int]$OutputRate = 48000,
    [ValidateSet(44100, 48000)]
    [int]$InputRate = 44100,
    [ValidateRange(16, 4096)]
    [int]$BlockSize = 512,
    [ValidateRange(1, 4)]
    [int]$OutputPair = 1,
    [ValidateRange(1, 65535)]
    [int]$ExpectedApiVersion = 44,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{64}$')]
    [string]$ExpectedSysSha256,
    [ValidateRange(30, 1800)]
    [int]$ProcessTimeoutSeconds = 300,
    [string]$BaselineJson = "",
    [string]$OutDir = "",
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($ProcessTimeoutSeconds -le ($Seconds + 30)) {
    throw "ProcessTimeoutSeconds must exceed Seconds by more than 30 seconds"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$common = Join-Path $repoRoot "windows\scripts\OpenA8DJ.WindowsCommon.psm1"
$probe = Join-Path $repoRoot "windows\tests\wasapi_irig_tone_probe.py"
$gate = Join-Path $repoRoot "scripts\physical-music-quality-gate"
$ctl = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
Import-Module $common -Force

foreach ($required in @($MusicFile, $Python, $probe, $gate, $ctl)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required path is missing: $required"
    }
}
$MusicFile = (Resolve-Path -LiteralPath $MusicFile).Path
if ($BaselineJson) {
    $BaselineJson = (Resolve-Path -LiteralPath $BaselineJson).Path
}
if (-not $OutDir) {
    $OutDir = Join-Path $repoRoot (
        "local-analysis\windows\quality-physical-music-{0}" -f (Get-Date -Format "yyyyMMdd-HHmmss")
    )
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

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
        $match = [regex]::Match(
            $Text,
            "(?m)^\s*" + [regex]::Escape($name) + ":\s*(\d+)"
        )
        $values[$name] = if ($match.Success) {
            [int64]$match.Groups[1].Value
        } else {
            $null
        }
    }
    return $values
}

function Get-DriverIdentity {
    $surface = (& $ctl surface | Out-String)
    $apiMatch = [regex]::Match($surface, '(?m)^\s*api-version:\s*(\d+)')
    if (-not $apiMatch.Success) {
        throw "Could not parse loaded OpenA8DJ API version"
    }
    $driver = Get-CimInstance Win32_SystemDriver |
        Where-Object {
            $_.Name -in @('OpenA8DJUsb', 'OpenA8DJUsbAcx') -and $_.State -eq 'Running'
        } |
        Select-Object -First 1
    if ($null -eq $driver) {
        throw "No running OpenA8DJ kernel service was found"
    }
    $imagePath = [Environment]::ExpandEnvironmentVariables(
        ([string]$driver.PathName).Trim('"')
    )
    if ($imagePath.StartsWith('\SystemRoot\', [StringComparison]::OrdinalIgnoreCase)) {
        $imagePath = Join-Path $env:SystemRoot $imagePath.Substring(12)
    }
    if (-not (Test-Path -LiteralPath $imagePath)) {
        throw "Loaded OpenA8DJ image was not found: $imagePath"
    }
    $file = Get-Item -LiteralPath $imagePath
    return [ordered]@{
        api_version = [int]$apiMatch.Groups[1].Value
        service_name = [string]$driver.Name
        service_state = [string]$driver.State
        image_path = $file.FullName
        image_sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        file_version = [string]$file.VersionInfo.FileVersion
    }
}

function Start-PnpContinuityMonitor(
    [string]$Path,
    [int]$DurationSeconds,
    [string]$Audio8UsbInstanceId,
    [string]$IrigUsbInstanceId,
    [string]$RenderInstanceId,
    [string]$CaptureInstanceId) {
    return Start-Job -ScriptBlock {
        param($Path, $DurationSeconds, $Audio8UsbInstanceId, $IrigUsbInstanceId, $RenderInstanceId, $CaptureInstanceId)
        "time_epoch`taudio8_usb_ok`tirig_usb_ok`taudio8_render_ok`tirig_capture_ok" |
            Set-Content -LiteralPath $Path -Encoding UTF8
        $deadline = (Get-Date).AddSeconds($DurationSeconds)
        while ((Get-Date) -lt $deadline) {
            $values = @(
                [int][bool](Get-PnpDevice -InstanceId $Audio8UsbInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
                [int][bool](Get-PnpDevice -InstanceId $IrigUsbInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
                [int][bool](Get-PnpDevice -InstanceId $RenderInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
                [int][bool](Get-PnpDevice -InstanceId $CaptureInstanceId -ErrorAction SilentlyContinue | Where-Object Status -eq 'OK')
            )
            "{0}`t{1}" -f ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds() / 1000.0), ($values -join "`t") |
                Add-Content -LiteralPath $Path -Encoding UTF8
            Start-Sleep -Seconds 10
        }
    } -ArgumentList $Path, $DurationSeconds, $Audio8UsbInstanceId, $IrigUsbInstanceId, $RenderInstanceId, $CaptureInstanceId
}

function Get-PnpContainerId([string]$InstanceId) {
    $property = Get-PnpDeviceProperty `
        -InstanceId $InstanceId `
        -KeyName 'DEVPKEY_Device_ContainerId' `
        -ErrorAction Stop
    return ([string]$property.Data).ToUpperInvariant()
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
        if ($_ -match '[\s"]') {
            '"' + $_.Replace('"', '\"') + '"'
        } else {
            $_
        }
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
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    try { $process.PriorityClass = 'High' } catch {}
    $completed = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $completed) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F |
            Out-Null
        $terminated = $process.WaitForExit(5000)
        $exitCode = -1
        $timeoutMessage = if ($terminated) {
            "Process timeout after $TimeoutSeconds seconds; PID $($process.Id) and its child tree were terminated."
        } else {
            "Process timeout after $TimeoutSeconds seconds; PID $($process.Id) did not confirm termination within 5 seconds."
        }
    } else {
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    $stdoutText = ''
    $stderrText = ''
    if ($stdoutTask.Wait(5000)) {
        $stdoutText = $stdoutTask.GetAwaiter().GetResult()
    } else {
        $process.StandardOutput.Close()
    }
    if ($stderrTask.Wait(5000)) {
        $stderrText = $stderrTask.GetAwaiter().GetResult()
    } else {
        $process.StandardError.Close()
    }
    [IO.File]::WriteAllText($StdoutPath, $stdoutText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText(
        $StderrPath,
        (@($stderrText, $timeoutMessage) | Where-Object { -not [string]::IsNullOrEmpty($_) }) -join "`n",
        [Text.UTF8Encoding]::new($false))
    return $exitCode
}

@(
    "safety_policy=physical_audio_no_reset_no_restart"
    "shared_hardware_lock=required"
    "does_not_install_or_reload_driver=1"
    "does_not_reset_usb=1"
    "does_not_restart_windows_audio=1"
    "records_boot_identity=1"
    "records_driver_counters_before_after=1"
    "physical_route=Audio 8 DJ 8ch WASAPI output to iRig Stream DirectSound input"
) | Set-Content -LiteralPath (Join-Path $OutDir "safety.txt") -Encoding UTF8

$lock = $null
$probeExit = $null
$gateExit = $null
$gateVerdict = $null
$failure = $null
$bootBefore = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$diagnosticsBefore = $null
$identityBefore = $null
$pnpMonitor = $null
$chatProcesses = @()
$savedPriorities = @{}

try {
    $lock = Acquire-OpenA8DJHardwareLock `
        -Gate "physical-music-quality" `
        -RunDir $OutDir `
        -TimeoutSeconds 0

    $present = @(Get-PnpDevice -PresentOnly)
    $audio8Usb = @(
        $present | Where-Object {
            $_.Status -eq "OK" -and $_.InstanceId -match '^USB\\VID_17CC&PID_1978\\'
        }
    )
    $irigUsb = @(
        $present | Where-Object {
            $_.Status -eq "OK" -and $_.InstanceId -match '^USB\\VID_1963&PID_0059\\'
        }
    )
    $audio8Render = @(
        $present | Where-Object {
            $_.Status -eq "OK" -and $_.FriendlyName -eq 'Audio 8 DJ (8ch Out) (Audio 8 DJ)'
        }
    )
    $irigCapture = @(
        $present | Where-Object {
            $_.Status -eq "OK" -and $_.FriendlyName -eq 'Line In (iRig Stream)'
        }
    )
    if ($audio8Usb.Count -ne 1 -or $irigUsb.Count -ne 1 -or
        $audio8Render.Count -ne 1 -or $irigCapture.Count -ne 1) {
        throw "Physical route requires exactly one USB device and one exact endpoint for each side"
    }
    $audio8UsbContainer = Get-PnpContainerId $audio8Usb[0].InstanceId
    $irigUsbContainer = Get-PnpContainerId $irigUsb[0].InstanceId
    $audio8EndpointContainer = Get-PnpContainerId $audio8Render[0].InstanceId
    $irigEndpointContainer = Get-PnpContainerId $irigCapture[0].InstanceId
    $hardwareProof = [ordered]@{
        schema_version = 2
        captured_at = (Get-Date).ToUniversalTime().ToString("o")
        audio8_usb_present_ok = ($audio8Usb.Count -gt 0)
        irig_usb_present_ok = ($irigUsb.Count -gt 0)
        audio8_render_endpoint_present_ok = ($audio8Render.Count -gt 0)
        irig_capture_endpoint_present_ok = ($irigCapture.Count -gt 0)
        audio8_usb_instance_ids = @($audio8Usb.InstanceId)
        irig_usb_instance_ids = @($irigUsb.InstanceId)
        audio8_render_instance_ids = @($audio8Render.InstanceId)
        irig_capture_instance_ids = @($irigCapture.InstanceId)
        audio8_usb_container_id = $audio8UsbContainer
        audio8_render_container_id = $audio8EndpointContainer
        irig_usb_container_id = $irigUsbContainer
        irig_capture_container_id = $irigEndpointContainer
        audio8_endpoint_matches_usb_container = ($audio8EndpointContainer -eq $audio8UsbContainer)
        irig_endpoint_matches_usb_container = ($irigEndpointContainer -eq $irigUsbContainer)
    }
    [IO.File]::WriteAllText(
        (Join-Path $OutDir "hardware-route-preflight.json"),
        ($hardwareProof | ConvertTo-Json -Depth 5) + "`n",
        [Text.UTF8Encoding]::new($false))
    if (
        -not $hardwareProof.audio8_usb_present_ok -or
        -not $hardwareProof.irig_usb_present_ok -or
        -not $hardwareProof.audio8_render_endpoint_present_ok -or
        -not $hardwareProof.irig_capture_endpoint_present_ok -or
        -not $hardwareProof.audio8_endpoint_matches_usb_container -or
        -not $hardwareProof.irig_endpoint_matches_usb_container
    ) {
        throw "Physical Audio 8 DJ to iRig route preflight failed"
    }

    $diagnosticsBefore = (& $ctl diagnostics | Out-String)
    $diagnosticsBefore | Set-Content `
        -LiteralPath (Join-Path $OutDir "driver-diagnostics-pre.txt") `
        -Encoding UTF8
    (& $ctl safety | Out-String) | Set-Content `
        -LiteralPath (Join-Path $OutDir "driver-safety-pre.txt") `
        -Encoding UTF8
    $identityBefore = Get-DriverIdentity
    $identityBefore | ConvertTo-Json -Depth 4 | Set-Content `
        -LiteralPath (Join-Path $OutDir "driver-identity-pre.json") `
        -Encoding UTF8
    if ($identityBefore.api_version -ne $ExpectedApiVersion) {
        throw "Loaded API $($identityBefore.api_version) does not match expected API $ExpectedApiVersion"
    }
    if ($identityBefore.image_sha256 -ne $ExpectedSysSha256.ToLowerInvariant()) {
        throw "Loaded image SHA-256 does not match the expected candidate"
    }
    $pnpMonitor = Start-PnpContinuityMonitor `
        -Path (Join-Path $OutDir "pnp-continuity.tsv") `
        -DurationSeconds ($Seconds + 120) `
        -Audio8UsbInstanceId $audio8Usb[0].InstanceId `
        -IrigUsbInstanceId $irigUsb[0].InstanceId `
        -RenderInstanceId $audio8Render[0].InstanceId `
        -CaptureInstanceId $irigCapture[0].InstanceId

    $chatProcesses = @(Get-Process ChatGPT -ErrorAction SilentlyContinue)
    foreach ($chatProcess in $chatProcesses) {
        try {
            $savedPriorities[$chatProcess.Id] = $chatProcess.PriorityClass
            $chatProcess.PriorityClass = 'Idle'
        } catch {}
    }

    $probeArgs = @(
        $probe,
        "--seconds", $Seconds,
        "--amplitude", $Amplitude,
        "--signal", "multi",
        "--output-rate", $OutputRate,
        "--input-rate", $InputRate,
        "--output-channels", 8,
        "--output-pair", $OutputPair,
        "--output-blocksize", $BlockSize,
        "--input-blocksize", $BlockSize,
        "--output-name", "Audio 8 DJ (8ch Out) (Audio 8 DJ)",
        "--output-hostapi", "Windows WASAPI",
        "--input-name", "Line In (iRig Stream)",
        "--input-hostapi", "Windows DirectSound",
        "--reference-file", $MusicFile,
        "--out-dir", $OutDir
    )
    $probeExit = Invoke-BoundedProcess `
        -FilePath $Python `
        -Arguments $probeArgs `
        -StdoutPath (Join-Path $OutDir "probe.stdout.txt") `
        -StderrPath (Join-Path $OutDir "probe.stderr.txt") `
        -TimeoutSeconds $ProcessTimeoutSeconds
    if ($probeExit -ne 0) {
        throw "Physical music probe failed with exit code $probeExit"
    }

    $gateArgs = @(
        $gate,
        "--run-dir", $OutDir,
        "--json-out", (Join-Path $OutDir "physical-music-gate.json"),
        "--coupling-profile-out", (Join-Path $OutDir "physical-music-coupling.json"),
        "--max-seconds", $Seconds,
        "--max-lag", 360000,
        "--min-alignment", 0.925,
        "--max-lag-jumps", 45,
        "--max-mid-band-cpu-corr", 0.16,
        "--windows-two-clock"
    )
    if ($BaselineJson) {
        $gateArgs += @("--baseline-json", $BaselineJson)
    }
    $gateExit = Invoke-BoundedProcess `
        -FilePath $Python `
        -Arguments $gateArgs `
        -StdoutPath (Join-Path $OutDir "physical-music-gate.stdout.txt") `
        -StderrPath (Join-Path $OutDir "physical-music-gate.stderr.txt") `
        -TimeoutSeconds $ProcessTimeoutSeconds
    if ($gateExit -ne 0) {
        throw "Physical music quality gate failed with exit code $gateExit"
    }
    $gateReportPath = Join-Path $OutDir 'physical-music-gate.json'
    if (-not (Test-Path -LiteralPath $gateReportPath)) {
        throw 'Physical music quality gate did not produce its JSON verdict'
    }
    $gateReport = Get-Content -LiteralPath $gateReportPath -Raw | ConvertFrom-Json
    $gateVerdict = [string]$gateReport.verdict
    if ($gateVerdict -ne 'PASS') {
        throw "Physical music quality gate JSON verdict is $gateVerdict"
    }
}
catch {
    $failure = $_.Exception.Message
}
finally {
    try {
        foreach ($chatProcess in $chatProcesses) {
            if ($savedPriorities.ContainsKey($chatProcess.Id)) {
                try { $chatProcess.PriorityClass = $savedPriorities[$chatProcess.Id] } catch {}
            }
        }
        $diagnosticsAfter = $null
        $bootAfter = $null
        $identityAfter = $null
        try {
            $diagnosticsAfter = (& $ctl diagnostics | Out-String)
            $diagnosticsAfter | Set-Content `
                -LiteralPath (Join-Path $OutDir "driver-diagnostics-post.txt") `
                -Encoding UTF8
            (& $ctl safety | Out-String) | Set-Content `
                -LiteralPath (Join-Path $OutDir "driver-safety-post.txt") `
                -Encoding UTF8
        }
        catch {
            $cleanupFailure = "post-run driver diagnostics failed: $($_.Exception.Message)"
            $failure = if ($null -eq $failure) {
                $cleanupFailure
            } else {
                "$failure; $cleanupFailure"
            }
        }
        try {
            $bootAfter = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
        }
        catch {
            $cleanupFailure = "post-run boot query failed: $($_.Exception.Message)"
            $failure = if ($null -eq $failure) {
                $cleanupFailure
            } else {
                "$failure; $cleanupFailure"
            }
        }

        if ($pnpMonitor) {
            Stop-Job $pnpMonitor -ErrorAction SilentlyContinue
            Remove-Job $pnpMonitor -Force -ErrorAction SilentlyContinue
        }
        try {
            $identityAfter = Get-DriverIdentity
            $identityAfter | ConvertTo-Json -Depth 4 | Set-Content `
                -LiteralPath (Join-Path $OutDir "driver-identity-post.json") `
                -Encoding UTF8
        }
        catch {
            $cleanupFailure = "post-run driver identity failed: $($_.Exception.Message)"
            $failure = if ($null -eq $failure) { $cleanupFailure } else { "$failure; $cleanupFailure" }
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
                $_.irig_usb_ok -ne '1' -or
                $_.audio8_render_ok -ne '1' -or
                $_.irig_capture_ok -ne '1'
            }).Count -eq 0
        )
        $identityUnchanged = (
            $null -ne $identityBefore -and
            $null -ne $identityAfter -and
            $identityBefore.api_version -eq $identityAfter.api_version -and
            $identityBefore.service_name -eq $identityAfter.service_name -and
            $identityBefore.image_path -eq $identityAfter.image_path -and
            $identityBefore.image_sha256 -eq $identityAfter.image_sha256
        )

        $before = Read-Counters $diagnosticsBefore
        $after = Read-Counters $diagnosticsAfter
        $deltas = [ordered]@{}
        foreach ($name in $counterNames) {
            $deltas[$name] = if ($null -ne $before[$name] -and $null -ne $after[$name]) {
                $after[$name] - $before[$name]
            } else {
                $null
            }
        }
        $badDeltas = @(
            $deltas.GetEnumerator() |
                Where-Object { $null -eq $_.Value -or $_.Value -ne 0 }
        )
        $summary = [ordered]@{
            music_file = $MusicFile
            seconds = $Seconds
            amplitude = $Amplitude
            output_rate = $OutputRate
            input_rate = $InputRate
            blocksize = $BlockSize
            output_pair = $OutputPair
            process_timeout_seconds = $ProcessTimeoutSeconds
            probe_exit = $probeExit
            gate_exit = $gateExit
            gate_verdict = $gateVerdict
            boot_before = $bootBefore.ToString("o")
            boot_after = if ($null -ne $bootAfter) { $bootAfter.ToString("o") } else { $null }
            boot_unchanged = ($null -ne $bootAfter -and $bootBefore -eq $bootAfter)
            expected_api_version = $ExpectedApiVersion
            expected_sys_sha256 = $ExpectedSysSha256.ToLowerInvariant()
            driver_identity_unchanged = $identityUnchanged
            pnp_continuity_samples = $pnpRows.Count
            pnp_continuity = $pnpContinuity
            driver_counter_deltas = $deltas
            failure = $failure
            passed = (
                $null -eq $failure -and
                $probeExit -eq 0 -and
                $gateExit -eq 0 -and
                $gateVerdict -eq 'PASS' -and
                $null -ne $bootAfter -and
                $bootBefore -eq $bootAfter -and
                $identityUnchanged -and
                $pnpContinuity -and
                $badDeltas.Count -eq 0
            )
        }
        $summary | ConvertTo-Json -Depth 6 | Set-Content `
            -LiteralPath (Join-Path $OutDir "run-summary.json") `
            -Encoding UTF8
    }
    finally {
        if ($lock) {
            Release-OpenA8DJHardwareLock -Lock $lock
        }
    }
}

Get-Content -LiteralPath (Join-Path $OutDir "run-summary.json") -Raw
if (-not $summary.passed) {
    exit 2
}
