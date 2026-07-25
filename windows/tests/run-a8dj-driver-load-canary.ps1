param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [ValidateSet('Stage', 'LoadInert', 'ControlRead', 'IsoCapture', 'IsoOutput', 'IsoStress', 'Streaming')]
    [string]$Phase = 'Stage',
    [string]$ExpectedInstanceId = 'USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___',
    [ValidateRange(10, 300)][int]$ProcessTimeoutSeconds = 60,
    [ValidateRange(1, 2)][int]$StreamingSeconds = 1,
    [ValidateSet(320, 352)][int]$IsoStressPacketBytes = 352,
    [switch]$AllowPhysicalLoad,
    [switch]$AcknowledgeCrashRisk,
    [string]$OutDir = '',
    [switch]$KeepInstalled
)

$ErrorActionPreference = 'Stop'
$HardwareId = 'USB\VID_17CC&PID_1978'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Import-Module (Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1') -Force
Assert-OpenA8DJAdministrator

function Write-Checkpoint {
    param([string]$Stage, [string]$Status, [hashtable]$Data = @{})
    $script:checkpointSequence++
    $record = [ordered]@{
        sequence = $script:checkpointSequence
        timestamp_utc = (Get-Date).ToUniversalTime().ToString('o')
        stage = $Stage
        status = $Status
        boot_time_utc = ([datetime](Get-CimInstance Win32_OperatingSystem).LastBootUpTime).ToUniversalTime().ToString('o')
        data = $Data
    }
    [IO.File]::AppendAllText($script:checkpointPath, (($record | ConvertTo-Json -Compress -Depth 10) + [Environment]::NewLine), [Text.Encoding]::UTF8)
}

function Get-DeviceSnapshot {
    $device = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
    if (-not $device) { return $null }
    $properties = @{}
    Get-PnpDeviceProperty -InstanceId $ExpectedInstanceId -KeyName `
        'DEVPKEY_Device_DriverVersion', 'DEVPKEY_Device_DriverInfPath', 'DEVPKEY_Device_Service' `
        -ErrorAction SilentlyContinue | ForEach-Object { $properties[$_.KeyName] = $_.Data }
    [ordered]@{
        instance_id = $device.InstanceId
        present = [bool]$device.Present
        status = [string]$device.Status
        problem = [string]$device.Problem
        driver_version = $properties['DEVPKEY_Device_DriverVersion']
        driver_inf = $properties['DEVPKEY_Device_DriverInfPath']
        service = $properties['DEVPKEY_Device_Service']
    }
}

function Find-PublishedDriverName {
    param([string]$Text)
    foreach ($block in ($Text -split '(?im)(?=^\s*Published Name\s*:)')) {
        if ($block -match '(?im)^\s*Original Name\s*:\s*OpenA8DJUsb\.inf\s*$' -and
            $block -match '(?im)^\s*Published Name\s*:\s*(oem\d+\.inf)\s*$') {
            return $Matches[1]
        }
    }
    return $null
}

function Add-TestCertificateToMachineStore {
    param([Parameter(Mandatory = $true)][string]$CertificatePath)
    $certificate = [Security.Cryptography.X509Certificates.X509Certificate2]::new($CertificatePath)
    foreach ($storeName in @(
        [Security.Cryptography.X509Certificates.StoreName]::Root,
        [Security.Cryptography.X509Certificates.StoreName]::TrustedPublisher)) {
        $store = [Security.Cryptography.X509Certificates.X509Store]::new(
            $storeName,
            [Security.Cryptography.X509Certificates.StoreLocation]::LocalMachine)
        try {
            $store.Open([Security.Cryptography.X509Certificates.OpenFlags]::ReadWrite)
            $store.Add($certificate)
        } finally {
            $store.Close()
        }
    }
    return $certificate.Thumbprint
}

function Try-ApproveExactDriverPrompt {
    param([string]$ExpectedInfHash)
    $drvInst = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq 'DrvInst.exe' -and $_.CommandLine -match '(?i)OpenA8DJUsb\.inf' })
    $hashMatched = $false
    foreach ($process in $drvInst) {
        foreach ($match in [regex]::Matches([string]$process.CommandLine, '(?i)"(?<path>[^"]*OpenA8DJUsb\.inf)"')) {
            $path = $match.Groups['path'].Value
            $actualHash = $null
            if (Test-Path -LiteralPath $path) {
                try {
                    $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256 -ErrorAction Stop).Hash
                } catch [System.IO.IOException], [System.Management.Automation.ItemNotFoundException] {
                    # DrvInst owns this transient copy and may remove it between
                    # discovery and hashing. A vanished prompt artifact is not
                    # a staging failure; keep polling the authoritative process.
                    continue
                }
            }
            if ($actualHash -and $actualHash -ieq $ExpectedInfHash) {
                $hashMatched = $true
            }
        }
    }
    if (-not $hashMatched) { return $false }

    # The package identity is verified above, but Windows Security must remain
    # a user-mediated boundary. Installing the test certificate normally keeps
    # this path non-interactive; if Windows still prompts, pnputil waits for the
    # user instead of synthesizing input into a security dialog.
    return $false
}

function Invoke-Bounded {
    param([string]$FilePath, [string[]]$Arguments, [string]$OutputPath, [switch]$ApprovePrompt, [string]$ExpectedInfHash)
    $argumentLine = ($Arguments | ForEach-Object { if ($_ -match '[\s"]') { '"' + $_.Replace('"', '\"') + '"' } else { $_ } }) -join ' '
    $start = [Diagnostics.ProcessStartInfo]::new($FilePath, $argumentLine)
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) { throw "Could not start $FilePath" }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $deadline = (Get-Date).AddSeconds($ProcessTimeoutSeconds)
    $approved = $false
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
        if ($ApprovePrompt -and -not $approved) {
            $approved = Try-ApproveExactDriverPrompt -ExpectedInfHash $ExpectedInfHash
        }
        if (-not $process.WaitForExit(100)) { continue }
    }
    if (-not $process.HasExited) {
        & (Join-Path $env:SystemRoot 'System32\taskkill.exe') /PID $process.Id /T /F | Out-Null
        $taskkillExit = $LASTEXITCODE
        $parentExited = $process.WaitForExit(10000)
        if ($taskkillExit -ne 0 -or -not $parentExited) {
            throw "Timed-out process tree termination was not confirmed for $FilePath."
        }
        $exitCode = -1
    } else {
        if (-not $process.WaitForExit(5000)) { throw "Exited process did not settle for $FilePath." }
        $exitCode = [int]$process.ExitCode
    }
    @(
        "prompt_approved=$([int]$approved)",
        $stdout.GetAwaiter().GetResult(),
        $stderr.GetAwaiter().GetResult()
    ) | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    return $exitCode
}

function Invoke-Ctl {
    param([string[]]$Arguments, [string]$Name)
    $old = $env:OPENA8DJ_INSTANCE_ID
    $env:OPENA8DJ_INSTANCE_ID = $ExpectedInstanceId
    try {
        return Invoke-Bounded -FilePath $ctlPath -Arguments $Arguments -OutputPath (Join-Path $outDir "$Name.txt")
    } finally {
        if ($null -eq $old) { Remove-Item Env:OPENA8DJ_INSTANCE_ID -ErrorAction SilentlyContinue } else { $env:OPENA8DJ_INSTANCE_ID = $old }
    }
}

function Flush-RegistryKey {
    param([Parameter(Mandatory = $true)][string]$SubKey)

    if (-not ('OpenA8DJ.NativeRegistry' -as [type])) {
        Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace OpenA8DJ {
    public static class NativeRegistry {
        [DllImport("advapi32.dll", ExactSpelling = true)]
        public static extern int RegFlushKey(SafeRegistryHandle hKey);
    }
}
'@
    }
    $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($SubKey, $true)
    if (-not $key) { throw "Registry key not found for flush: HKLM\$SubKey" }
    try {
        $result = [OpenA8DJ.NativeRegistry]::RegFlushKey($key.Handle)
        if ($result -ne 0) { throw "RegFlushKey failed for HKLM\$SubKey with Win32 error $result." }
    } finally {
        $key.Dispose()
    }
}

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$manifest = Test-OpenA8DJPackageManifest -PackageDir $PackageDir
$infPath = Join-Path $PackageDir 'OpenA8DJUsb.inf'
$ctlPath = Join-Path $PackageDir 'opena8djctl.exe'
$expectedInfHash = (Get-FileHash -LiteralPath $infPath -Algorithm SHA256).Hash
$expectedSysHash = [string](@($manifest.files | Where-Object name -eq 'OpenA8DJUsb.sys')[0].sha256)
$expectedFingerprint = [string]$manifest.build_fingerprint
$outDir = if ($OutDir) { $OutDir } else {
    Join-Path $repoRoot ("local-analysis\windows\physical-one-shot-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $Phase.ToLowerInvariant())
}
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$outDir = (Resolve-Path -LiteralPath $outDir).Path
$script:checkpointPath = Join-Path $outDir 'checkpoints.jsonl'
$script:checkpointSequence = 0
$lock = Acquire-OpenA8DJHardwareLock -Gate "physical-one-shot-$Phase" -RunDir $outDir -TimeoutSeconds 0
$publishedName = $null
$recoveryTaskName = 'OpenA8DJCanaryRecovery'
$recoveryStatePath = Join-Path $outDir 'recovery-state.json'
$recoveryBundleDir = Join-Path $env:ProgramData 'OpenA8DJCanary'
$recoveryBundleScript = Join-Path $recoveryBundleDir 'recover.ps1'
$recoveryBundleState = Join-Path $recoveryBundleDir 'recovery-state.json'
$loadAttempted = $false
$errorMessage = $null
$keptInstalled = $false

try {
    $device = Get-DeviceSnapshot
    Write-Checkpoint 'preflight' 'ok' @{ phase = $Phase; device = $device; manifest = $manifest }
    $matchingPhysicalDevices = @(Get-PnpDevice -ErrorAction SilentlyContinue |
        Where-Object InstanceId -Like 'USB\VID_17CC&PID_1978\*')
    if ($matchingPhysicalDevices.Count -ne 1 -or $matchingPhysicalDevices[0].InstanceId -ne $ExpectedInstanceId) {
        throw 'Physical canary requires exactly one Audio 8 DJ and the exact expected instance ID.'
    }
    if (-not $device -or -not $device.present -or $device.problem -ne 'CM_PROB_DISABLED') {
        throw 'Exact Audio 8 DJ device must be present and disabled before every canary phase.'
    }
    $existing = @(Get-OpenA8DJDriverStoreMatches -PackageDir $PackageDir)
    if ($existing.Count -ne 0) { throw "DriverStore must be clean before staging; found $($existing.Count) OpenA8DJ package(s)." }
    if ($Phase -ne 'Stage' -and (-not $AllowPhysicalLoad -or -not $AcknowledgeCrashRisk)) {
        throw 'Any phase beyond Stage requires -AllowPhysicalLoad and -AcknowledgeCrashRisk.'
    }
    if ($Phase -ne 'Stage' -and -not (Test-Path -LiteralPath $ctlPath)) {
        throw "Control tool missing from package: $ctlPath"
    }

    $certPath = Join-Path $PackageDir 'OpenA8DJUsb-TestCertificate.cer'
    if (-not (Test-Path -LiteralPath $certPath)) { throw "Physical test certificate missing: $certPath" }
    $certThumbprint = Add-TestCertificateToMachineStore -CertificatePath $certPath
    Write-Checkpoint 'test-certificate-trusted' 'ok' @{ certificate_path = $certPath; thumbprint = $certThumbprint }

    Write-Checkpoint 'stage-start' 'pending' @{ inf_sha256 = $expectedInfHash; sys_sha256 = $expectedSysHash; build_fingerprint = $expectedFingerprint }
    $stageExit = Invoke-Bounded -FilePath (Join-Path $env:windir 'System32\pnputil.exe') `
        -Arguments @('/add-driver', $infPath) -OutputPath (Join-Path $outDir 'pnputil-stage.txt') `
        -ApprovePrompt -ExpectedInfHash $expectedInfHash
    if (@(0, 259, 3010) -notcontains $stageExit) { throw "pnputil staging failed: $stageExit" }
    $enumText = (& pnputil.exe /enum-drivers) -join "`n"
    $publishedName = Find-PublishedDriverName -Text $enumText
    if (-not $publishedName) { throw 'Could not resolve staged OpenA8DJ OEM INF.' }
    $store = @(Get-OpenA8DJDriverStoreMatches -PackageDir $PackageDir)
    $exact = @($store | Where-Object Exact)
    if ($store.Count -ne 1 -or $exact.Count -ne 1) {
        throw "DriverStore hash gate failed: total=$($store.Count) exact=$($exact.Count)"
    }
    Write-Checkpoint 'driverstore-verified' 'ok' @{ published_name = $publishedName; path = $exact[0].Path; sys_sha256 = $exact[0].SysSha256; inf_sha256 = $exact[0].InfSha256 }

    if ($Phase -eq 'Stage') { return }

    $recoveryState = [ordered]@{
        instance_id = $ExpectedInstanceId
        published_name = $publishedName
        package_sys_sha256 = $expectedSysHash
        build_fingerprint = $expectedFingerprint
        phase = $Phase
        recovery_task_name = $recoveryTaskName
        artifact_recovery_report = (Join-Path $outDir 'recovery-report.json')
    }
    $recoveryState | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $recoveryStatePath -Encoding UTF8
    $recoveryScript = Join-Path $PSScriptRoot 'recover-a8dj-physical-canary.ps1'
    New-Item -ItemType Directory -Path $recoveryBundleDir -Force | Out-Null
    Copy-Item -LiteralPath $recoveryScript -Destination $recoveryBundleScript -Force
    $recoveryState | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $recoveryBundleState -Encoding UTF8
    $taskCommand = "powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$recoveryBundleScript`" -StatePath `"$recoveryBundleState`""
    if ($taskCommand.Length -gt 261) { throw "Recovery task command unexpectedly exceeds schtasks limit: $($taskCommand.Length)" }
    & schtasks.exe /Create /SC ONSTART /DELAY 0000:20 /RU SYSTEM /RL HIGHEST /TN $recoveryTaskName /TR $taskCommand /F | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not register boot recovery task.' }
    Write-Checkpoint 'boot-recovery-armed' 'ok' @{ task = $recoveryTaskName; state = $recoveryStatePath }

    $devcon = Get-ChildItem (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Tools') -Recurse -Filter devcon.exe -ErrorAction SilentlyContinue |
        Where-Object FullName -Match '\\x64\\devcon\.exe$' | Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $devcon) { throw 'devcon.exe x64 not found.' }
    $loadAttempted = $true
    Write-Checkpoint 'physical-bind-load-start' 'pending' @{ command = 'devcon update'; exact_instance = $ExpectedInstanceId; build_fingerprint = $expectedFingerprint; driver_entry_next_boot_failsafe = 4 }
    $bindExit = Invoke-Bounded -FilePath $devcon -Arguments @('update', $infPath, $HardwareId) -OutputPath (Join-Path $outDir 'devcon-bind.txt')
    if ($bindExit -ne 0) { throw "devcon bind failed: $bindExit" }
    Start-Sleep -Seconds 2
    $boundDevice = Get-DeviceSnapshot
    if (-not $boundDevice -or [string]$boundDevice.driver_inf -ne $publishedName -or [string]$boundDevice.service -ne 'OpenA8DJUsbAcx') {
        throw 'Candidate did not bind to the exact disabled physical device.'
    }
    Write-Checkpoint 'physical-bind-returned' 'ok' @{ exit = $bindExit; device = $boundDevice }
    if ([string]$boundDevice.problem -eq 'CM_PROB_UNSIGNED_DRIVER') {
        throw 'Windows rejected the candidate as CM_PROB_UNSIGNED_DRIVER; reboot through Startup Settings option 7 before retrying.'
    }

    $serviceKey = 'HKLM\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx'
    $startAfterBind = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx' -Name Start -ErrorAction Stop).Start
    $enableExit = $null
    if ([int]$startAfterBind -ne 4) {
        & reg.exe add $serviceKey /v Start /t REG_DWORD /d 3 /f | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Could not arm demand-start for the one-shot physical load.' }
        Flush-RegistryKey -SubKey 'SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx'
        Write-Checkpoint 'physical-enable-start' 'pending' @{ command = 'pnputil enable-device'; exact_instance = $ExpectedInstanceId }
        $enableExit = Invoke-Bounded -FilePath (Join-Path $env:windir 'System32\pnputil.exe') `
            -Arguments @('/enable-device', $ExpectedInstanceId, '/force') `
            -OutputPath (Join-Path $outDir 'pnputil-enable-device.txt')
        if (@(0, 3010) -notcontains $enableExit) { throw "Physical one-shot enable failed: $enableExit" }
    }
    Start-Sleep -Seconds 2
    $loadedDevice = Get-DeviceSnapshot
    Write-Checkpoint 'physical-load-returned' 'ok' @{ bind_exit = $bindExit; enable_exit = $enableExit; device = $loadedDevice }
    $safetyExit = Invoke-Ctl -Arguments @('safety') -Name 'safety-after-load'
    if ($safetyExit -ne 0) { throw 'Inert safety query failed after load.' }
    $safetyText = Get-Content -LiteralPath (Join-Path $outDir 'safety-after-load.txt') -Raw
    if ($safetyText -notmatch [regex]::Escape($expectedFingerprint) -or $safetyText -notmatch 'armed-phase:\s+0') {
        throw 'Loaded driver did not report the exact build fingerprint in a disarmed state.'
    }
    $startAfterLoad = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Services\OpenA8DJUsbAcx' -Name Start -ErrorAction Stop).Start
    if ([int]$startAfterLoad -ne 4) { throw 'DriverEntry did not persist the next-boot Start=disabled fail-safe.' }
    Write-Checkpoint 'loaded-binary-verified-inert' 'ok' @{ build_fingerprint = $expectedFingerprint }

    if ($Phase -ne 'LoadInert') {
        $phaseMap = @{ ControlRead = 'control-read'; IsoCapture = 'iso-capture'; IsoOutput = 'iso-output'; IsoStress = 'iso-output'; Streaming = 'streaming' }
        $operationMap = @{ ControlRead = @('controls'); IsoCapture = @('iso-capture'); IsoOutput = @('iso-silence'); IsoStress = @('iso-tone', '25', '0', '1', [string]$IsoStressPacketBytes, '40'); Streaming = @('start') }
        $nonce = [Guid]::NewGuid().ToByteArray()
        $nonceHigh = [BitConverter]::ToUInt64($nonce, 0)
        $nonceLow = [BitConverter]::ToUInt64($nonce, 8)
        Write-Checkpoint 'arm-start' 'pending' @{ driver_phase = $phaseMap[$Phase]; nonce_high = $nonceHigh; nonce_low = $nonceLow }
        if ((Invoke-Ctl -Arguments @('arm', $phaseMap[$Phase], [string]$nonceHigh, [string]$nonceLow) -Name 'arm') -ne 0) { throw 'Driver rejected one-shot authorization.' }
        Write-Checkpoint 'operation-start' 'pending' @{ phase = $Phase; command = $operationMap[$Phase] }
        $operationExit = Invoke-Ctl -Arguments $operationMap[$Phase] -Name 'operation'
        if ($operationExit -ne 0) {
            try { Invoke-Ctl -Arguments @('diagnostics') -Name 'diagnostics-after-operation-failure' | Out-Null } catch {}
            try { Invoke-Ctl -Arguments @('safety') -Name 'safety-after-operation-failure' | Out-Null } catch {}
            throw "One-shot $Phase operation failed."
        }
        $operationText = Get-Content -LiteralPath (Join-Path $outDir 'operation.txt') -Raw
        if ($Phase -eq 'IsoCapture') {
            if ($operationText -notmatch '(?im)^\s*nt-status:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*error-count:\s+0\s*$' -or
                $operationText -notmatch '(?im)^\s*packet\[\d+\]:.*completed=[1-9]\d*\s+usbd=0x00000000\s*$') {
                throw 'IsoCapture returned without a successful nonempty physical packet.'
            }
        } elseif ($Phase -eq 'IsoOutput') {
            if ($operationText -notmatch '(?im)^\s*nt-status:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*playback-status:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*playback-errors:\s+0\s*$' -or
                $operationText -notmatch '(?im)^\s*playback-bytes:\s+[1-9]\d*\s*$') {
                throw 'IsoOutput returned without a successful nonempty physical transfer.'
            }
        } elseif ($Phase -eq 'IsoStress') {
            if ($operationText -notmatch '(?im)^\s*nt-status:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*requested:\s+25\s*$' -or
                $operationText -notmatch '(?im)^\s*completed:\s+25\s*$' -or
                $operationText -notmatch '(?im)^\s*first-capture:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*first-playback:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*last-capture:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*last-playback:\s+0x00000000\s*$' -or
                $operationText -notmatch '(?im)^\s*capture-errors:\s+0\s*$' -or
                $operationText -notmatch '(?im)^\s*playback-errors:\s+0\s*$' -or
                $operationText -notmatch '(?im)^\s*playback-bytes:\s+[1-9]\d*\s*$') {
                throw 'IsoStress did not complete all 25 physical IN/OUT transfers cleanly.'
            }
        }
        if ($Phase -eq 'Streaming') {
            Start-Sleep -Seconds $StreamingSeconds
            Invoke-Ctl -Arguments @('diagnostics') -Name 'diagnostics-during-streaming' | Out-Null
            $streamDiagnostics = Get-Content -LiteralPath (Join-Path $outDir 'diagnostics-during-streaming.txt') -Raw
            if ($streamDiagnostics -notmatch '(?im)^\s*worker-iterations:\s+[1-9]\d*\s*$' -or
                $streamDiagnostics -notmatch '(?im)^\s*usb-in-packets:\s+[1-9]\d*\s*$' -or
                $streamDiagnostics -notmatch '(?im)^\s*worker-cap-bytes:\s+[1-9]\d*\s*$') {
                throw 'Streaming started but did not complete a nonempty physical capture transfer.'
            }
            Invoke-Ctl -Arguments @('stop') -Name 'stream-stop' | Out-Null
            Invoke-Ctl -Arguments @('diagnostics') -Name 'diagnostics-after-streaming-stop' | Out-Null
            Invoke-Ctl -Arguments @('safety') -Name 'safety-after-streaming-stop' | Out-Null
        } else {
            if ((Invoke-Ctl -Arguments @('safety') -Name 'safety-after-operation') -ne 0) {
                throw "Could not read the post-$Phase driver safety checkpoint."
            }
            # Control reads are intentionally non-destructive and no longer
            # consume one-shot authorization; checkpoint 200 proves that the
            # exact arm state remained intact until the finally-block disarm.
            $expectedCheckpoint = @{ ControlRead = 200; IsoCapture = 304; IsoOutput = 314; IsoStress = 314 }[$Phase]
            $safetyText = Get-Content -LiteralPath (Join-Path $outDir 'safety-after-operation.txt') -Raw
            if ($safetyText -notmatch "(?im)^\s*checkpoint:\s+$expectedCheckpoint\s*$" -or
                $safetyText -notmatch '(?im)^\s*checkpoint-status:\s+0x00000000\s*$') {
                throw "Post-$Phase safety checkpoint did not reach clean terminal state $expectedCheckpoint."
            }
        }
        Write-Checkpoint 'operation-returned' 'ok' @{ phase = $Phase }
    }
} catch {
    $errorMessage = $_.Exception.Message
    Write-Checkpoint 'exception' 'failed' @{ message = $errorMessage; load_attempted = $loadAttempted }
} finally {
    $disarmVerified = $false
    if ($loadAttempted -and (Test-Path -LiteralPath $ctlPath)) {
        try {
            $disarmExit = Invoke-Ctl -Arguments @('disarm') -Name 'disarm'
            if ($disarmExit -ne 0) { throw "Disarm command failed with exit code $disarmExit." }
            $postDisarmExit = Invoke-Ctl -Arguments @('safety') -Name 'safety-after-disarm'
            if ($postDisarmExit -ne 0) { throw "Post-disarm safety query failed with exit code $postDisarmExit." }
            $postDisarmText = Get-Content -LiteralPath (Join-Path $outDir 'safety-after-disarm.txt') -Raw
            if ($postDisarmText -notmatch '(?mi)^\s*armed-phase:\s*0\s*$' -or
                $postDisarmText -notmatch '(?mi)^\s*operations-left:\s*0\s*$' -or
                $postDisarmText -notmatch '(?mi)^\s*nonce:\s*0{32}\s*$') {
                throw 'Post-disarm safety state is not clean.'
            }
            $disarmVerified = $true
            Write-Checkpoint 'disarm-verified' 'ok' @{ exit = $disarmExit; safety_exit = $postDisarmExit }
        } catch {
            $disarmError = $_.Exception.Message
            Write-Checkpoint 'disarm-verified' 'failed' @{ message = $disarmError }
            if (-not $errorMessage) { $errorMessage = "Disarm verification failed: $disarmError" }
        }
    }
    if ($KeepInstalled -and $loadAttempted -and -not $errorMessage -and $disarmVerified) {
        $keptInstalled = $true
        Write-Checkpoint 'keep-installed' 'ok' @{ recovery_task_left_armed = $recoveryTaskName; published_name = $publishedName }
        $finalDevice = Get-DeviceSnapshot
        $summary = [ordered]@{
            phase = $Phase
            build_fingerprint = $expectedFingerprint
            sys_sha256 = $expectedSysHash
            published_name = $publishedName
            load_attempted = $loadAttempted
            keep_installed = $true
            recovery_task_left_armed = $recoveryTaskName
            error = $errorMessage
            final_device = $finalDevice
            checkpoint_path = $script:checkpointPath
        }
        $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outDir 'summary.json') -Encoding UTF8
    } else {
        $cleanupVerified = $false
        try {
            & pnputil.exe /disable-device $ExpectedInstanceId /force | Out-File (Join-Path $outDir 'disable-device.txt')
            $disableExit = $LASTEXITCODE
            $disableOk = @(0, 3010) -contains $disableExit
            Write-Checkpoint 'device-disabled' $(if ($disableOk) { 'ok' } else { 'failed' }) @{ exit = $disableExit }
            if (-not $disableOk -and -not $errorMessage) { $errorMessage = "Initial device disable failed: $disableExit" }
        } catch {
            Write-Checkpoint 'device-disabled' 'failed' @{ message = $_.Exception.Message }
            if (-not $errorMessage) { $errorMessage = "Initial device disable failed: $($_.Exception.Message)" }
        }
        if ($publishedName -match '^oem\d+\.inf$') {
            try {
                & pnputil.exe /delete-driver $publishedName /uninstall /force | Out-File (Join-Path $outDir 'delete-driver.txt')
                $deleteExit = $LASTEXITCODE
                $deleteOk = @(0, 3010) -contains $deleteExit
                Write-Checkpoint 'package-removed' $(if ($deleteOk) { 'ok' } else { 'failed' }) @{ exit = $deleteExit; published_name = $publishedName }
                if (-not $deleteOk -and -not $errorMessage) { $errorMessage = "Driver package removal failed: $deleteExit" }
            } catch {
                Write-Checkpoint 'package-removed' 'failed' @{ message = $_.Exception.Message }
                if (-not $errorMessage) { $errorMessage = "Driver package removal failed: $($_.Exception.Message)" }
            }
        }
        & sc.exe delete OpenA8DJUsbAcx | Out-Null
        $serviceDeleteExit = $LASTEXITCODE
        if (@(0, 1060, 1072) -notcontains $serviceDeleteExit -and -not $errorMessage) {
            $errorMessage = "Driver service deletion failed: $serviceDeleteExit"
        }
        try {
            # Package removal can trigger an asynchronous vendor-driver rebind.
            # Require the disabled state to survive several PnP settling polls.
            $finalDisableOk = $false
            $finalDisableExit = $null
            for ($disableAttempt = 1; $disableAttempt -le 5; $disableAttempt++) {
                $settledDevice = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
                if ($settledDevice -and [string]$settledDevice.Problem -eq 'CM_PROB_DISABLED') {
                    Start-Sleep -Seconds 1
                    $settledDevice = Get-PnpDevice -InstanceId $ExpectedInstanceId -ErrorAction SilentlyContinue
                    if ($settledDevice -and [string]$settledDevice.Problem -eq 'CM_PROB_DISABLED') {
                        $finalDisableOk = $true
                        break
                    }
                }
                & pnputil.exe /disable-device $ExpectedInstanceId /force | Out-File (Join-Path $outDir 'disable-device-final.txt') -Append
                $finalDisableExit = $LASTEXITCODE
                Start-Sleep -Seconds 1
            }
            Write-Checkpoint 'device-disabled-final' $(if ($finalDisableOk) { 'ok' } else { 'failed' }) @{ exit = $finalDisableExit; attempts = $disableAttempt }
            if (-not $finalDisableOk -and -not $errorMessage) { $errorMessage = 'Device did not remain disabled after cleanup.' }
        } catch {
            Write-Checkpoint 'device-disabled-final' 'failed' @{ message = $_.Exception.Message }
            if (-not $errorMessage) { $errorMessage = 'Final device disable verification failed.' }
        }
        try {
            $packageResidual = @(Get-OpenA8DJDriverStoreMatches -PackageDir $PackageDir).Count -ne 0
            $serviceResidual = [bool](Get-Service OpenA8DJUsbAcx -ErrorAction SilentlyContinue)
            $cleanupVerified = -not $packageResidual -and -not $serviceResidual -and $finalDisableOk
            Write-Checkpoint 'cleanup-verified' $(if ($cleanupVerified) { 'ok' } else { 'failed' }) @{
                package_residual = $packageResidual
                service_residual = $serviceResidual
                device_disabled = $finalDisableOk
            }
            if (-not $cleanupVerified -and -not $errorMessage) {
                $errorMessage = 'Cleanup verification found a residual package, service, or enabled device.'
            }
        } catch {
            Write-Checkpoint 'cleanup-verified' 'failed' @{ message = $_.Exception.Message }
            if (-not $errorMessage) { $errorMessage = "Cleanup verification failed: $($_.Exception.Message)" }
        }
        if ($cleanupVerified) {
            try {
                $previousErrorPreference = $ErrorActionPreference
                $ErrorActionPreference = 'SilentlyContinue'
                & schtasks.exe /Delete /TN $recoveryTaskName /F 2>&1 | Out-Null
            } finally {
                $ErrorActionPreference = $previousErrorPreference
            }
        }
    }
    $finalDevice = Get-DeviceSnapshot
    $summary = [ordered]@{
        phase = $Phase
        build_fingerprint = $expectedFingerprint
        sys_sha256 = $expectedSysHash
        published_name = $publishedName
        load_attempted = $loadAttempted
        keep_installed = $keptInstalled
        recovery_task_left_armed = if ($keptInstalled) { $recoveryTaskName } else { $null }
        error = $errorMessage
        final_device = $finalDevice
        checkpoint_path = $script:checkpointPath
    }
    $summary | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outDir 'summary.json') -Encoding UTF8
    Release-OpenA8DJHardwareLock -Lock $lock
}

if ($errorMessage) { throw "$errorMessage Artifacts: $outDir" }
if ($keptInstalled) {
    Write-Host "OpenA8DJ one-shot phase $Phase completed; device left installed with boot recovery armed. Artifacts: $outDir"
} else {
    Write-Host "OpenA8DJ one-shot phase $Phase completed; device left disabled and package removed. Artifacts: $outDir"
}
