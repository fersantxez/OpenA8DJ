param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$ProbeCtlPath,
    [Parameter(Mandatory = $true)][switch]$AllowVirtualInstall,
    [Parameter(Mandatory = $true)][switch]$AllowTestSigned,
    [ValidateRange(1, 10)][int]$OutputStreamProbeSeconds = 1,
    [ValidateRange(10, 120)][int]$ProcessTimeoutSeconds = 30,
    [int]$WaitSeconds = 5
)

$ErrorActionPreference = "Stop"

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]$identity
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run the virtual output canary from an elevated PowerShell."
    }
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

function Get-SecureBootState {
    try {
        return [ordered]@{ state = if (Confirm-SecureBootUEFI) { "enabled" } else { "disabled" }; error = $null }
    } catch {
        return [ordered]@{ state = "unavailable"; error = $_.Exception.Message }
    }
}

function Get-BootTimeUtc {
    return ([datetime](Get-CimInstance Win32_OperatingSystem).LastBootUpTime).ToUniversalTime().ToString("o")
}

function Get-VirtualDevices {
    @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
        $_.InstanceId -like "ROOT\OpenA8DJVirtual*" -or
        $_.FriendlyName -eq "OpenA8DJ Virtual ACX Proof Endpoint"
    })
}

function Find-PublishedDriverName {
    param([Parameter(Mandatory = $true)][string]$Text)
    $blocks = $Text -split '(?im)(?=^\s*Published Name\s*:)'
    foreach ($block in $blocks) {
        if ($block -match '(?im)^\s*Original Name\s*:\s*OpenA8DJVirtual\.inf\s*$' -and
            $block -match '(?im)^\s*Published Name\s*:\s*(oem\d+\.inf)\s*$') {
            return $Matches[1]
        }
    }
    return $null
}

function Try-ApproveWindowsSecurityDriverPrompt {
    param(
        [Parameter(Mandatory = $true)][string]$ExpectedInfPath,
        [Parameter(Mandatory = $true)][string]$ExpectedInfHash
    )

    $installers = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "DrvInst.exe" -and $_.CommandLine -match "(?i)OpenA8DJVirtual\.inf" })
    $matchingInstaller = $false
    foreach ($process in $installers) {
        $match = [regex]::Match([string]$process.CommandLine, '(?i)"(?<path>[^"]*OpenA8DJVirtual\.inf)"')
        if (-not $match.Success) { continue }
        $path = $match.Groups["path"].Value
        if ((Test-Path -LiteralPath $path) -and
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ieq $ExpectedInfHash) {
            $matchingInstaller = $true
            break
        }
    }
    if (-not $matchingInstaller) { return $false }

    Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes -ErrorAction SilentlyContinue
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class OpenA8DjVirtualOutputFocus {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
}
'@ -ErrorAction SilentlyContinue
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children,[System.Windows.Automation.Condition]::TrueCondition)
    for ($index = 0; $index -lt $windows.Count; $index++) {
        $window = $windows.Item($index)
        if ([string]$window.Current.Name -ne "Windows Security") { continue }
        $condition = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::NameProperty,
            "Install this driver software anyway")
        $button = $window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition)
        if (-not $button -or -not $button.Current.IsEnabled) { continue }
        [void][OpenA8DjVirtualOutputFocus]::SetForegroundWindow([IntPtr]$window.Current.NativeWindowHandle)
        Start-Sleep -Milliseconds 100
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction SilentlyContinue
        [System.Windows.Forms.SendKeys]::SendWait("%i")
        return $true
    }
    return $false
}

function Invoke-ExternalProcessBounded {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$OutputPath,
        [Parameter(Mandatory = $true)][int]$TimeoutSeconds,
        [switch]$ApproveUnsignedDriverInstall,
        [string]$ExpectedInfPath,
        [string]$ExpectedInfHash,
        [string]$DisplayName = "external process"
    )

    $argumentLine = ($Arguments | ForEach-Object {
        $value = [string]$_
        if ($value -match '[\s"]') { '"' + $value.Replace('"','\\"') + '"' } else { $value }
    }) -join ' '
    $info = [System.Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $FilePath
    $info.Arguments = $argumentLine
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $info
    if (-not $process.Start()) { throw "Could not start $DisplayName." }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    $approvalAttempted = $false
    while (-not $process.HasExited) {
        if ($ApproveUnsignedDriverInstall -and -not $approvalAttempted -and
            (Get-Date) -lt $deadline -and
            (Try-ApproveWindowsSecurityDriverPrompt -ExpectedInfPath $ExpectedInfPath -ExpectedInfHash $ExpectedInfHash)) {
            $approvalAttempted = $true
        }
        if ((Get-Date) -ge $deadline) { break }
        [void]$process.WaitForExit(250)
    }
    $completed = $process.HasExited
    $prefix = $null
    if (-not $completed) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        [void]$process.WaitForExit(5000)
        $prefix = "$DisplayName timeout after $TimeoutSeconds seconds; process $($process.Id) was terminated."
        $exitCode = -1
    } else {
        [void]$process.WaitForExit()
        $exitCode = [int]$process.ExitCode
    }
    $lines = @()
    if ($prefix) { $lines += $prefix }
    if ($ApproveUnsignedDriverInstall -and $approvalAttempted) {
        $lines += "Windows Security exact OpenA8DJVirtual.inf prompt approved after SHA-256 match."
    }
    $lines += $stdout.GetAwaiter().GetResult() -split "`r?`n"
    $lines += $stderr.GetAwaiter().GetResult() -split "`r?`n"
    $lines | Set-Content -LiteralPath $OutputPath -Encoding UTF8
    return $exitCode
}

function Write-Checkpoint {
    param([string]$Stage,[string]$Status,[hashtable]$Data = @{})
    $script:Sequence++
    $record = [ordered]@{
        sequence = $script:Sequence
        timestamp_utc = (Get-Date).ToUniversalTime().ToString("o")
        stage = $Stage
        status = $Status
        pid = $PID
        boot_time_utc = Get-BootTimeUtc
        data = $Data
    }
    [System.IO.File]::AppendAllText($checkpointPath,(($record | ConvertTo-Json -Compress -Depth 8) + [Environment]::NewLine),[Text.Encoding]::UTF8)
}

function Get-BugCheckEventsSince {
    param([datetime]$Since)
    @(Get-WinEvent -FilterHashtable @{LogName="System";Id=1001;StartTime=$Since} -ErrorAction SilentlyContinue |
        Where-Object {$_.ProviderName -eq "Microsoft-Windows-WER-SystemErrorReporting"})
}

if (-not $AllowVirtualInstall -or -not $AllowTestSigned) { throw "Virtual output canary requires both install authorization switches." }
Assert-Administrator
$PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
$ProbeCtlPath = (Resolve-Path -LiteralPath $ProbeCtlPath).Path
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$infPath = Join-Path $PackageDir "OpenA8DJVirtual.inf"
$sysPath = Join-Path $PackageDir "OpenA8DJVirtual.sys"
$catPath = Join-Path $PackageDir "OpenA8DJVirtual.cat"
foreach ($path in @($infPath,$sysPath,$catPath)) { if (-not (Test-Path $path)) { throw "Missing package file: $path" } }
$infText = Get-Content $infPath -Raw
if ($infText -notmatch '(?im)^Class\s*=\s*MEDIA\s*$' -or
    $infText -notmatch '(?im)ROOT\\OpenA8DJVirtual' -or
    $infText -match '(?im)USB\\VID_') { throw "Refusing non-isolated virtual package." }
$infHash = (Get-FileHash -LiteralPath $infPath -Algorithm SHA256).Hash
$preVirtual = @(Get-VirtualDevices)
if ($preVirtual.Count -ne 0) { throw "Refusing output canary with a pre-existing virtual device." }
$physical = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {$_.InstanceId -like "USB\VID_17CC&PID_1978*"})
$secureBoot = Get-SecureBootState
$outDir = Join-Path $repoRoot "local-analysis\windows-open-a8dj-virtual-output-canary-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$checkpointPath = Join-Path $outDir "checkpoints.jsonl"
$script:Sequence = 0
@(
    "safety_policy=isolated_virtual_output_only",
    "does_not_target_audio_8_dj_usb=1",
    "physical_audio8dj_usb_device_count=$($physical.Count)",
    "secure_boot_state=$($secureBoot.state)",
    "output_stream_probe_seconds=$OutputStreamProbeSeconds",
    "process_timeout_seconds=$ProcessTimeoutSeconds",
    "checkpoint_path=$checkpointPath"
) | Set-Content (Join-Path $outDir "safety.txt") -Encoding ASCII
Write-Checkpoint "preflight-start" "ok" @{secure_boot=$secureBoot;physical_audio8dj_usb_device_count=$physical.Count;package_dir=$PackageDir;inf_sha256=$infHash}

$devcon = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools\x64\devcon.exe"
if (-not (Test-Path $devcon)) { throw "devcon.exe not found." }
$startedAt = Get-Date
$publishedName = $null
$postVirtual = @()
$remainingVirtual = @()
$installExit = $null
$probeExit = $null
$outputExit = $null
$scriptError = $null
$restoreError = $null
try {
    Write-Checkpoint "package-validated" "ok" @{inf_path=$infPath;sys_path=$sysPath;cat_path=$catPath;inf_sha256=$infHash}
    $certPath = Join-Path $PackageDir 'OpenA8DJVirtual-TestCertificate.cer'
    if (-not (Test-Path -LiteralPath $certPath)) { throw "Virtual test certificate missing: $certPath" }
    $certThumbprint = Add-TestCertificateToMachineStore -CertificatePath $certPath
    Write-Checkpoint "test-certificate-trusted" "ok" @{certificate_path=$certPath;thumbprint=$certThumbprint}
    Write-Checkpoint "install-start" "pending" @{command="devcon install <virtual-inf> ROOT\\OpenA8DJVirtual"}
    $installExit = Invoke-ExternalProcessBounded -FilePath $devcon -Arguments @("install",$infPath,"ROOT\OpenA8DJVirtual") -OutputPath (Join-Path $outDir "devcon-install.txt") -TimeoutSeconds $ProcessTimeoutSeconds -ApproveUnsignedDriverInstall -ExpectedInfPath $infPath -ExpectedInfHash $infHash -DisplayName "virtual devcon install"
    Write-Checkpoint "install-command-returned" $(if(@(0,259,3010) -contains $installExit){"ok"}else{"failed"}) @{exit_code=$installExit}
    if (@(0,259,3010) -notcontains $installExit) { throw "Virtual install failed with exit code $installExit." }
    $enumPath = Join-Path $outDir "pnputil-enum-drivers.txt"
    $enumExit = Invoke-ExternalProcessBounded -FilePath (Join-Path $env:windir "System32\pnputil.exe") -Arguments @("/enum-drivers") -OutputPath $enumPath -TimeoutSeconds $ProcessTimeoutSeconds -DisplayName "pnputil enum drivers"
    if ($enumExit -eq 0) { $publishedName = Find-PublishedDriverName (Get-Content $enumPath -Raw) }
    if (-not $publishedName) { throw "Could not resolve virtual package for cleanup." }
    Write-Checkpoint "published-driver-resolved" "ok" @{published_name=$publishedName}
    Start-Sleep -Seconds $WaitSeconds
    $postVirtual = @(Get-VirtualDevices)
    Write-Checkpoint "pnp-postinstall-snapshot" $(if($postVirtual.Count -eq 1 -and $postVirtual[0].Status -eq "OK" -and $postVirtual[0].Problem -eq "CM_PROB_NONE"){"ok"}else{"failed"}) @{virtual_device_count=$postVirtual.Count;instance_ids=@($postVirtual|ForEach-Object{$_.InstanceId});statuses=@($postVirtual|ForEach-Object{$_.Status});problems=@($postVirtual|ForEach-Object{$_.Problem})}
    if ($postVirtual.Count -ne 1 -or $postVirtual[0].Status -ne "OK" -or $postVirtual[0].Problem -ne "CM_PROB_NONE") { throw "Virtual device did not reach a clean PnP state." }
    $probeScript = Join-Path $PSScriptRoot "run-open-a8dj-virtual-endpoint-probe.ps1"
    Write-Checkpoint "probe-start" "pending" @{ctl_path=$ProbeCtlPath;instance_id=$postVirtual[0].InstanceId}
    $oldInstance = $env:OPENA8DJ_INSTANCE_ID
    $env:OPENA8DJ_INSTANCE_ID = [string]$postVirtual[0].InstanceId
    try {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $probeScript -CtlPath $ProbeCtlPath *> (Join-Path $outDir "virtual-probe.txt")
        $probeExit = $LASTEXITCODE
    } finally {
        if ($null -eq $oldInstance) { Remove-Item Env:OPENA8DJ_INSTANCE_ID -ErrorAction SilentlyContinue } else { $env:OPENA8DJ_INSTANCE_ID = $oldInstance }
    }
    Write-Checkpoint "probe-returned" $(if($probeExit -eq 0){"ok"}else{"failed"}) @{exit_code=$probeExit}
    if ($probeExit -ne 0) { throw "Virtual read-only probe failed with exit code $probeExit." }
    $python = Join-Path $env:LOCALAPPDATA "Programs\Python\Python313\python.exe"
    if (-not (Test-Path $python)) { $python = (Get-Command python -ErrorAction Stop).Source }
    $outputScript = Join-Path $PSScriptRoot "a8dj_output_endpoint_probe.py"
    $outputDir = Join-Path $outDir "output"
    New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
    Write-Checkpoint "output-stream-start" "pending" @{seconds=$OutputStreamProbeSeconds;instance_id=$postVirtual[0].InstanceId;host_api="MME"}
    $oldInstance = $env:OPENA8DJ_INSTANCE_ID
    $env:OPENA8DJ_INSTANCE_ID = [string]$postVirtual[0].InstanceId
    try {
        $outputExit = Invoke-ExternalProcessBounded -FilePath $python -Arguments @($outputScript,"--ctl",$ProbeCtlPath,"--out-dir",$outputDir,"--seconds",[string]$OutputStreamProbeSeconds,"--rate","44100","--channels","8","--hostapi","MME","--name","OpenA8DJ Virtual ACX","--max-targets","1") -OutputPath (Join-Path $outDir "output-probe.txt") -TimeoutSeconds $ProcessTimeoutSeconds -DisplayName "virtual output stream probe"
    } finally {
        if ($null -eq $oldInstance) { Remove-Item Env:OPENA8DJ_INSTANCE_ID -ErrorAction SilentlyContinue } else { $env:OPENA8DJ_INSTANCE_ID = $oldInstance }
    }
    Write-Checkpoint "output-stream-returned" $(if($outputExit -eq 0){"ok"}else{"failed"}) @{exit_code=$outputExit;seconds=$OutputStreamProbeSeconds}
    if ($outputExit -ne 0) { throw "Virtual output stream probe failed with exit code $outputExit." }
} catch {
    $scriptError = $_.Exception.Message
    Write-Checkpoint "exception" "failed" @{message=$scriptError;fully_qualified_error_id=$_.FullyQualifiedErrorId}
} finally {
    Write-Checkpoint "cleanup-start" "pending" @{}
    $postVirtual = @(Get-VirtualDevices)
    foreach ($device in $postVirtual) {
        $removeDeviceExit = Invoke-ExternalProcessBounded -FilePath (Join-Path $env:windir "System32\pnputil.exe") -Arguments @("/remove-device",[string]$device.InstanceId) -OutputPath (Join-Path $outDir "pnputil-remove-device.txt") -TimeoutSeconds $ProcessTimeoutSeconds -DisplayName "virtual remove-device"
        Write-Checkpoint "remove-device-returned" $(if(@(0,3010) -contains $removeDeviceExit){"ok"}else{"failed"}) @{instance_id=$device.InstanceId;exit_code=$removeDeviceExit}
    }
    if ($publishedName) {
        $deleteExit = Invoke-ExternalProcessBounded -FilePath (Join-Path $env:windir "System32\pnputil.exe") -Arguments @("/delete-driver",$publishedName,"/uninstall","/force") -OutputPath (Join-Path $outDir "pnputil-delete-driver.txt") -TimeoutSeconds $ProcessTimeoutSeconds -DisplayName "virtual delete-driver"
        Write-Checkpoint "delete-driver-returned" $(if(@(0,3010) -contains $deleteExit){"ok"}else{"failed"}) @{published_name=$publishedName;exit_code=$deleteExit}
    } else { $deleteExit = $null }
    $remainingVirtual = @(Get-VirtualDevices)
    Write-Checkpoint "cleanup-pnp-snapshot" $(if($remainingVirtual.Count -eq 0){"ok"}else{"failed"}) @{remaining_virtual_device_count=$remainingVirtual.Count;remaining_instance_ids=@($remainingVirtual|ForEach-Object{$_.InstanceId})}
    $bugChecks = @(Get-BugCheckEventsSince -Since $startedAt)
}

$summary = [ordered]@{started_at=$startedAt.ToUniversalTime().ToString("o");finished_at=(Get-Date).ToUniversalTime().ToString("o");artifact_dir=$outDir;checkpoint_path=$checkpointPath;install_exit=$installExit;published_name=$publishedName;probe_exit=$probeExit;output_stream_probe_exit=$outputExit;output_stream_probe_seconds=$OutputStreamProbeSeconds;remaining_virtual_devices=@($remainingVirtual|ForEach-Object{$_.InstanceId});delete_driver_exit=$deleteExit;bugcheck_events_since_start=$bugChecks.Count;secure_boot=$secureBoot;error=$scriptError;restore_error=$restoreError}
$summary | ConvertTo-Json -Depth 10 | Set-Content (Join-Path $outDir "summary.json") -Encoding UTF8
if (-not $scriptError -and $remainingVirtual.Count -eq 0 -and $bugChecks.Count -eq 0 -and $outputExit -eq 0) { Write-Checkpoint "complete" "ok" @{output_exit=$outputExit;delete_driver_exit=$deleteExit} }
$summary | ConvertTo-Json -Depth 10
if ($scriptError -or $remainingVirtual.Count -ne 0 -or $bugChecks.Count -ne 0 -or $outputExit -ne 0) { exit 2 }
exit 0
