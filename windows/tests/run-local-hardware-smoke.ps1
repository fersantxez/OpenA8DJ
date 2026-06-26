param(
    [string]$CtlPath = "",
    [string]$Audio8InstanceId = "USB\VID_17CC&PID_1978\SN-HKM6Q6EDKP___"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($CtlPath)) {
    $CtlPath = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
}

if (-not (Test-Path -LiteralPath $CtlPath)) {
    throw "opena8djctl.exe not found at $CtlPath"
}

$audio8 = Get-PnpDevice -InstanceId $Audio8InstanceId -ErrorAction Stop
if ($audio8.Status -ne "OK") {
    throw "Audio 8 DJ PnP status is $($audio8.Status)"
}
if ($audio8.Manufacturer -ne "OpenA8DJ") {
    throw "Audio 8 DJ is bound to $($audio8.Manufacturer), expected OpenA8DJ"
}

$irig = Get-PnpDevice | Where-Object { $_.FriendlyName -match "iRig Stream" }
if (-not $irig) {
    throw "iRig Stream is not visible"
}
$badIrig = $irig | Where-Object { $_.Status -ne "OK" }
if ($badIrig) {
    throw "iRig Stream has a non-OK device entry"
}

$audioModule = Get-Module -ListAvailable -Name AudioDeviceCmdlets | Select-Object -First 1
if ($audioModule) {
    Import-Module AudioDeviceCmdlets
    $recordingDefault = Get-AudioDevice -List |
        Where-Object { $_.Type -eq "Recording" -and $_.Default } |
        Select-Object -First 1
    if (-not $recordingDefault) {
        throw "No default recording device found"
    }
    if ($recordingDefault.Name -notmatch "Microphone Array.*Realtek") {
        throw "Default recording device is '$($recordingDefault.Name)', expected Realtek Microphone Array for dictation"
    }
}

$usb = & $CtlPath usb
if ($LASTEXITCODE -ne 0) {
    throw "opena8djctl usb failed with exit code $LASTEXITCODE"
}

function Require-Match {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Description
    )
    if ($Text -notmatch $Pattern) {
        throw "USB smoke missing ${Description}: pattern '$Pattern'"
    }
}

$usbText = $usb -join "`n"
Require-Match $usbText "interfaces-total:\s+1" "one USB interface"
Require-Match $usbText "interfaces-config:\s+1" "one configured interface"
Require-Match $usbText "pipes-config:\s+4" "four configured pipes"
Require-Match $usbText "alt-settings:\s+2" "alternate settings"
Require-Match $usbText "alt-selected:\s+1" "selected alternate setting"
Require-Match $usbText "bulk-out:\s+yes" "bulk-out pipe"
Require-Match $usbText "bulk-in:\s+yes" "bulk-in pipe"
Require-Match $usbText "iso-in:\s+yes" "iso-in pipe"
Require-Match $usbText "iso-out:\s+yes" "iso-out pipe"

Write-Host "PASS: OpenA8DJ local hardware smoke"
Write-Host "Audio 8 DJ: OpenA8DJ driver loaded and USB pipes ready"
Write-Host "iRig Stream: present and OK"
if ($audioModule) {
    Write-Host "Dictation default: $($recordingDefault.Name)"
} else {
    Write-Host "Dictation default: skipped because AudioDeviceCmdlets is not installed"
}
