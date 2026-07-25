param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,

    [int]$WaitSeconds = 10,

    [ValidateRange(10, 300)]
    [int]$PnpUtilTimeoutSeconds = 60,

    [switch]$AllowVirtualInstall,

    [switch]$AllowTestSigned,

    [switch]$RemoveAfter,

    [string]$ProbeCtlPath
)

$ErrorActionPreference = "Stop"

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]$identity
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run the virtual canary from an elevated PowerShell. Current user is $($identity.Name)."
    }
}

function Get-BugCheckEventsSince {
    param([datetime]$Since)

    Get-WinEvent -FilterHashtable @{
        LogName = "System"
        Id = 1001
        StartTime = $Since
    } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -eq "Microsoft-Windows-WER-SystemErrorReporting" }
}

function Get-VirtualDevices {
    @(Get-PnpDevice -ErrorAction SilentlyContinue |
        Where-Object {
            $_.InstanceId -like "ROOT\OpenA8DJVirtual*" -or
            $_.FriendlyName -eq "OpenA8DJ Virtual ACX Proof Endpoint"
        })
}

function Get-PhysicalAudio8DjUsbDevices {
    @(Get-PnpDevice -ErrorAction SilentlyContinue |
        Where-Object { $_.InstanceId -like "USB\VID_17CC&PID_1978*" })
}

function Invoke-ExternalProcessBounded {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [string]$DisplayName = "external process"
    )

    $errorPath = "$OutputPath.stderr"
    Remove-Item -LiteralPath $errorPath -Force -ErrorAction SilentlyContinue
    $argumentLine = ($Arguments | ForEach-Object {
        $argument = [string]$_
        if ($argument -match '[\s"]') {
            '"' + $argument.Replace('"', '\\"') + '"'
        } else {
            $argument
        }
    }) -join ' '
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = $argumentLine
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        throw "Could not start pnputil."
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $completed = $process.WaitForExit($TimeoutSeconds * 1000)
    if (-not $completed) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $null = $process.WaitForExit(5000)
        $exitCode = -1
        $prefix = "$DisplayName timeout after $TimeoutSeconds seconds; process $($process.Id) was terminated."
    } else {
        $process.WaitForExit()
        $exitCode = [int]$process.ExitCode
        $prefix = $null
    }

    $output = @()
    if ($prefix) {
        $output += $prefix
    }
    $output += $stdoutTask.GetAwaiter().GetResult() -split "`r?`n"
    $output += $stderrTask.GetAwaiter().GetResult() -split "`r?`n"
    $output | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    return $exitCode
}

function Invoke-PnpUtilBounded {
    param(
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    Invoke-ExternalProcessBounded `
        -FilePath (Join-Path $env:windir "System32\pnputil.exe") `
        -Arguments $Arguments `
        -OutputPath $OutputPath `
        -TimeoutSeconds $TimeoutSeconds `
        -DisplayName "pnputil"
}

function Invoke-DevconBounded {
    param(
        [Parameter(Mandatory = $true)][string]$DevconPath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds
    )

    Invoke-ExternalProcessBounded `
        -FilePath $DevconPath `
        -Arguments $Arguments `
        -OutputPath $OutputPath `
        -TimeoutSeconds $TimeoutSeconds `
        -DisplayName "devcon"
}

function Resolve-DevconPath {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools\x64\devcon.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools\arm64\devcon.exe")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    return $null
}

function Get-SecureBootState {
    try {
        return [ordered]@{
            state = if (Confirm-SecureBootUEFI) { "enabled" } else { "disabled" }
            error = $null
        }
    } catch {
        return [ordered]@{
            state = "unavailable"
            error = $_.Exception.Message
        }
    }
}

function Get-BootTimeUtc {
    try {
        $os = Get-CimInstance -ClassName Win32_OperatingSystem
        return ([datetime]$os.LastBootUpTime).ToUniversalTime().ToString("o")
    } catch {
        return $null
    }
}

function Find-PublishedDriverName {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$OriginalName
    )

    $blocks = $Text -split '(?im)(?=^\s*Published Name\s*:)'
    foreach ($block in $blocks) {
        if ($block -match ("(?im)^\s*Original Name\s*:\s*" + [regex]::Escape($OriginalName) + "\s*$") -and
            $block -match '(?im)^\s*Published Name\s*:\s*(oem\d+\.inf)\s*$') {
            return $Matches[1]
        }
    }
    return $null
}

$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$infPath = Join-Path $PackageDir "OpenA8DJVirtual.inf"
$sysPath = Join-Path $PackageDir "OpenA8DJVirtual.sys"
$catPath = Join-Path $PackageDir "OpenA8DJVirtual.cat"
$requiredFiles = @($infPath, $sysPath, $catPath)

foreach ($path in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required virtual package file not found: $path"
    }
}

$infText = Get-Content -LiteralPath $infPath -Raw
if ($infText -notmatch '(?im)^Class\s*=\s*MEDIA\s*$') {
    throw "Refusing canary: virtual INF is not a MEDIA package."
}
if ($infText -notmatch '(?im)ROOT\\OpenA8DJVirtual') {
    throw "Refusing canary: virtual INF does not target ROOT\OpenA8DJVirtual."
}
if ($infText -match '(?im)USB\\VID_') {
    throw "Refusing canary: virtual INF contains a USB VID/PID match."
}
if ($infText -notmatch '(?im)OpenA8DJVirtual\.sys') {
    throw "Refusing canary: virtual INF does not install OpenA8DJVirtual.sys."
}
if ($RemoveAfter -and -not $AllowVirtualInstall) {
    throw "-RemoveAfter requires -AllowVirtualInstall."
}
if ($ProbeCtlPath -and -not $AllowVirtualInstall) {
    throw "-ProbeCtlPath requires -AllowVirtualInstall."
}
if ($ProbeCtlPath -and -not $RemoveAfter) {
    throw "-ProbeCtlPath requires -RemoveAfter so the temporary virtual device is cleaned up."
}
if ($ProbeCtlPath) {
    $ProbeCtlPath = (Resolve-Path -LiteralPath $ProbeCtlPath).Path
}

$outDir = Join-Path $repoRoot "local-analysis\windows-open-a8dj-virtual-canary-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$checkpointPath = Join-Path $outDir "checkpoints.jsonl"
$script:CanaryCheckpointPath = $checkpointPath
$script:CanaryCheckpointSequence = 0
function Write-CanaryCheckpoint {
    param(
        [Parameter(Mandatory = $true)][string]$Stage,
        [Parameter(Mandatory = $true)][string]$Status,
        [hashtable]$Data = @{}
    )

    $script:CanaryCheckpointSequence++
    $record = [ordered]@{
        sequence = $script:CanaryCheckpointSequence
        timestamp_utc = (Get-Date).ToUniversalTime().ToString("o")
        stage = $Stage
        status = $Status
        pid = $PID
        boot_time_utc = Get-BootTimeUtc
        data = $Data
    }
    [System.IO.File]::AppendAllText(
        $script:CanaryCheckpointPath,
        (($record | ConvertTo-Json -Compress -Depth 8) + [Environment]::NewLine),
        [System.Text.Encoding]::UTF8)
}

$secureBoot = Get-SecureBootState
$bootTimeUtc = Get-BootTimeUtc
$physicalUsbDevices = @(Get-PhysicalAudio8DjUsbDevices)
@(
    "safety_policy=virtual_endpoint_only",
    "does_not_target_audio_8_dj_usb=1",
    "does_not_open_physical_audio_endpoints=1",
    "does_not_start_usb_streaming=1",
    "does_not_run_opena8djctl=1",
    "package_dir=$PackageDir",
    "allow_virtual_install=$([bool]$AllowVirtualInstall)",
    "allow_test_signed=$([bool]$AllowTestSigned)",
    "remove_after=$([bool]$RemoveAfter)",
    "probe_ctl_path=$ProbeCtlPath",
    "secure_boot_state=$($secureBoot.state)",
    "boot_time_utc=$bootTimeUtc",
    "checkpoint_path=$checkpointPath",
    "pnputil_timeout_seconds=$PnpUtilTimeoutSeconds",
    "physical_audio8dj_usb_device_count=$($physicalUsbDevices.Count)"
) | Set-Content -Path (Join-Path $outDir "safety.txt") -Encoding ASCII

Write-CanaryCheckpoint "preflight-start" "ok" @{
    package_dir = $PackageDir
    secure_boot = $secureBoot
    allow_virtual_install = [bool]$AllowVirtualInstall
    allow_test_signed = [bool]$AllowTestSigned
    remove_after = [bool]$RemoveAfter
    pnputil_timeout_seconds = $PnpUtilTimeoutSeconds
    physical_audio8dj_usb_device_count = $physicalUsbDevices.Count
    physical_audio8dj_usb_instance_ids = @($physicalUsbDevices | ForEach-Object { $_.InstanceId })
}

$fileReports = foreach ($path in $requiredFiles) {
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    [ordered]@{
        path = $path
        exists = $true
        bytes = (Get-Item -LiteralPath $path).Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash.ToLowerInvariant()
        signature_status = [string]$signature.Status
        signer = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
    }
}
Write-CanaryCheckpoint "package-validated" "ok" @{
    files = @($fileReports | ForEach-Object { $_.path })
    inf_is_media = $true
    inf_is_root_only = $true
    inf_has_usb_vid = $false
}

$preDevices = @(Get-VirtualDevices)
$preDevices | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-before.txt") -Encoding UTF8
Write-CanaryCheckpoint "pnp-preflight-snapshot" "ok" @{
    virtual_device_count = $preDevices.Count
    virtual_instance_ids = @($preDevices | ForEach-Object { $_.InstanceId })
    physical_audio8dj_usb_device_count = $physicalUsbDevices.Count
    physical_audio8dj_usb_instance_ids = @($physicalUsbDevices | ForEach-Object { $_.InstanceId })
}
$report = [ordered]@{
    mode = if ($AllowVirtualInstall) { "install" } else { "preflight" }
    package_dir = $PackageDir
    inf_path = $infPath
    sys_path = $sysPath
    cat_path = $catPath
    files = $fileReports
    pre_existing_virtual_devices = @($preDevices | ForEach-Object { $_.InstanceId })
    checkpoint_path = $checkpointPath
    pnputil_timeout_seconds = $PnpUtilTimeoutSeconds
    boot_time_utc = $bootTimeUtc
    secure_boot = $secureBoot
    physical_audio8dj_usb_devices_present = @($physicalUsbDevices | ForEach-Object { $_.InstanceId })
    artifact_dir = $outDir
}

if (-not $AllowVirtualInstall) {
    Write-CanaryCheckpoint "preflight-complete" "ok" @{ no_install = $true }
    $report.preflight_only = $true
    $report | ConvertTo-Json -Depth 8 | Set-Content -Path (Join-Path $outDir "summary.json") -Encoding UTF8
    $report | ConvertTo-Json -Depth 8
    Write-Host "Virtual endpoint canary preflight passed. No installation was attempted. Artifacts: $outDir"
    exit 0
}

if (-not $AllowTestSigned) {
    throw "Refusing kernel installation without both -AllowVirtualInstall and -AllowTestSigned."
}
Assert-Administrator
Write-CanaryCheckpoint "install-authority-verified" "ok" @{
    secure_boot = $secureBoot
    user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
}
$devconPath = Resolve-DevconPath
if (-not $devconPath) {
    throw "Refusing virtual device creation: WDK devcon.exe was not found in the standard Windows Kits tools path."
}
Write-CanaryCheckpoint "device-creation-tool-verified" "ok" @{
    tool = "devcon"
    path = $devconPath
    hardware_id = "ROOT\OpenA8DJVirtual"
}
if ($preDevices.Count -ne 0) {
    throw "Refusing install because ROOT\OpenA8DJVirtual is already present. Remove it or use a clean test VM."
}

$startedAt = Get-Date
$devconInstallPath = Join-Path $outDir "devcon-install.txt"
$pnputilEnumPath = Join-Path $outDir "pnputil-enum-drivers.txt"
$publishedName = $null
$postDevices = @()
$remainingDevices = @()
$bugChecks = @()
$installExit = $null
$probeExit = $null
$removeExit = $null
$removeDeviceExit = $null

try {
    Write-CanaryCheckpoint "install-start" "pending" @{
        command = "devcon install <virtual-inf> ROOT\\OpenA8DJVirtual"
        tool = $devconPath
        hardware_id = "ROOT\\OpenA8DJVirtual"
    }
    $installExit = Invoke-DevconBounded `
        -DevconPath $devconPath `
        -Arguments @("install", $infPath, "ROOT\OpenA8DJVirtual") `
        -OutputPath $devconInstallPath `
        -TimeoutSeconds $PnpUtilTimeoutSeconds
    if (@(0, 259, 3010) -notcontains $installExit) {
        $installText = Get-Content -LiteralPath $devconInstallPath -Raw
        if ($installText -match '(?im)Published Name\s*:\s*(oem\d+\.inf)') {
            $publishedName = $Matches[1]
        }
        if (-not $publishedName) {
            $enumExit = Invoke-PnpUtilBounded -Arguments @("/enum-drivers") -OutputPath $pnputilEnumPath -TimeoutSeconds $PnpUtilTimeoutSeconds
            if ($enumExit -eq 0) {
                $publishedName = Find-PublishedDriverName `
                    -Text (Get-Content -LiteralPath $pnputilEnumPath -Raw) `
                    -OriginalName "OpenA8DJVirtual.inf"
            }
        }
        Write-CanaryCheckpoint "install-command-returned" "failed" @{ exit_code = $installExit; published_name = $publishedName }
        throw "devcon install failed with exit code $installExit. See $devconInstallPath"
    }
    Write-CanaryCheckpoint "install-command-returned" "ok" @{ exit_code = $installExit }

    $installText = Get-Content -LiteralPath $devconInstallPath -Raw
    if ($installText -match '(?im)Published Name\s*:\s*(oem\d+\.inf)') {
        $publishedName = $Matches[1]
    }
    if (-not $publishedName) {
        $enumExit = Invoke-PnpUtilBounded -Arguments @("/enum-drivers") -OutputPath $pnputilEnumPath -TimeoutSeconds $PnpUtilTimeoutSeconds
        if ($enumExit -eq 0) {
            $publishedName = Find-PublishedDriverName `
                -Text (Get-Content -LiteralPath $pnputilEnumPath -Raw) `
                -OriginalName "OpenA8DJVirtual.inf"
        }
    }
    if (-not $publishedName) {
        throw "Could not determine the published OpenA8DJVirtual package name; refusing to continue without a cleanup target. See $pnputilInstallPath and $pnputilEnumPath"
    }
    Write-CanaryCheckpoint "published-driver-resolved" "ok" @{ published_name = $publishedName }
    Start-Sleep -Seconds $WaitSeconds
    $postDevices = @(Get-VirtualDevices)
    $postDevices | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-after.txt") -Encoding UTF8
    $postDevices | ForEach-Object {
        Invoke-PnpUtilBounded -Arguments @("/enum-devices", "/instanceid", [string]$_.InstanceId) -OutputPath (Join-Path $outDir "pnputil-device-after.txt") -TimeoutSeconds $PnpUtilTimeoutSeconds | Out-Null
    }
    Write-CanaryCheckpoint "pnp-postinstall-snapshot" "ok" @{
        virtual_device_count = $postDevices.Count
        virtual_instance_ids = @($postDevices | ForEach-Object { $_.InstanceId })
        statuses = @($postDevices | ForEach-Object { [string]$_.Status })
        problems = @($postDevices | ForEach-Object { [string]$_.Problem })
    }
    $bugChecks = @(Get-BugCheckEventsSince -Since $startedAt)
    if ($ProbeCtlPath) {
        $probeScript = Join-Path $PSScriptRoot "run-open-a8dj-virtual-endpoint-probe.ps1"
        Write-CanaryCheckpoint "probe-start" "pending" @{ ctl_path = $ProbeCtlPath }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $probeScript -CtlPath $ProbeCtlPath *> (Join-Path $outDir "virtual-probe.txt")
        $probeExit = $LASTEXITCODE
        Write-CanaryCheckpoint "probe-returned" $(if ($probeExit -eq 0) { "ok" } else { "failed" }) @{ exit_code = $probeExit }
        if ($probeExit -ne 0) {
            throw "Virtual endpoint read-only probe failed with exit code $probeExit. See $(Join-Path $outDir 'virtual-probe.txt')"
        }
    }
} catch {
    Write-CanaryCheckpoint "exception" "failed" @{
        message = $_.Exception.Message
        fully_qualified_error_id = $_.FullyQualifiedErrorId
    }
    throw
} finally {
    if ($RemoveAfter) {
        Write-CanaryCheckpoint "cleanup-start" "pending" @{}
        if ($postDevices.Count -eq 0) {
            $postDevices = @(Get-VirtualDevices)
        }
        foreach ($device in $postDevices) {
            $removeDeviceExit = Invoke-PnpUtilBounded -Arguments @("/remove-device", [string]$device.InstanceId) -OutputPath (Join-Path $outDir "pnputil-remove-device.txt") -TimeoutSeconds $PnpUtilTimeoutSeconds
            Write-CanaryCheckpoint "remove-device-returned" $(if (@(0, 3010) -contains $removeDeviceExit) { "ok" } else { "failed" }) @{ instance_id = $device.InstanceId; exit_code = $removeDeviceExit }
        }
        if ($publishedName) {
            $removeExit = Invoke-PnpUtilBounded -Arguments @("/delete-driver", [string]$publishedName, "/uninstall", "/force") -OutputPath (Join-Path $outDir "pnputil-delete-driver.txt") -TimeoutSeconds $PnpUtilTimeoutSeconds
            Write-CanaryCheckpoint "delete-driver-returned" $(if (@(0, 3010) -contains $removeExit) { "ok" } else { "failed" }) @{ published_name = $publishedName; exit_code = $removeExit }
        }
        $remainingDevices = @(Get-VirtualDevices)
        Write-CanaryCheckpoint "cleanup-pnp-snapshot" $(if ($remainingDevices.Count -eq 0) { "ok" } else { "failed" }) @{
            remaining_virtual_device_count = $remainingDevices.Count
            remaining_instance_ids = @($remainingDevices | ForEach-Object { $_.InstanceId })
        }
    }
}

$report.finished_at = (Get-Date).ToUniversalTime().ToString("o")
$report.preflight_only = $false
$report.started_at = $startedAt.ToUniversalTime().ToString("o")
$report.install_exit = $installExit
$report.published_name = $publishedName
$report.probe_ctl_path = $ProbeCtlPath
$report.probe_exit = $probeExit
$report.pnp_status = @($postDevices | ForEach-Object { [string]$_.Status })
$report.pnp_problem = @($postDevices | ForEach-Object { [string]$_.Problem })
$report.bugcheck_events_since_start = $bugChecks.Count
$report.remove_device_exit = $removeDeviceExit
$report.delete_driver_exit = $removeExit
$report.remaining_virtual_devices = @($remainingDevices | ForEach-Object { $_.InstanceId })
$report | ConvertTo-Json -Depth 8 | Set-Content -Path (Join-Path $outDir "summary.json") -Encoding UTF8
$report | ConvertTo-Json -Depth 8

if ($RemoveAfter) {
    if ($removeDeviceExit -notin @(0, 3010)) {
        throw "Virtual device cleanup failed with exit code $removeDeviceExit. Artifacts: $outDir"
    }
    if ($removeExit -notin @(0, 3010)) {
        throw "Virtual driver package cleanup failed with exit code $removeExit. Artifacts: $outDir"
    }
    if ($remainingDevices.Count -ne 0) {
        throw "Virtual device cleanup left $($remainingDevices.Count) device(s) present. Artifacts: $outDir"
    }
}
if ($bugChecks.Count -ne 0) {
    throw "Bugcheck event appeared during virtual canary. Stop. Artifacts: $outDir"
}
if ($postDevices.Count -ne 1) {
    throw "Expected exactly one virtual PnP device, found $($postDevices.Count). Artifacts: $outDir"
}
if ($postDevices[0].Status -ne "OK" -or $postDevices[0].Problem -ne "CM_PROB_NONE") {
    throw "Virtual endpoint did not reach clean PnP state. Artifacts: $outDir"
}

Write-CanaryCheckpoint "complete" "ok" @{
    probe_exit = $probeExit
    remove_device_exit = $removeDeviceExit
    delete_driver_exit = $removeExit
    remaining_virtual_device_count = $remainingDevices.Count
}

Write-Host "OpenA8DJ virtual endpoint canary passed. Artifacts: $outDir"
