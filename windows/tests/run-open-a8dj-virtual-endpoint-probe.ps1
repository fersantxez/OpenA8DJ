param(
    [Parameter(Mandatory = $true)]
    [string]$CtlPath,

    [int]$ExpectedApiVersion = 27
)

$ErrorActionPreference = "Stop"

function Get-Value {
    param(
        [Parameter(Mandatory = $true)][string]$Text,
        [Parameter(Mandatory = $true)][string]$Name
    )

    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -match ("^\s*" + [regex]::Escape($Name) + ":\s+(.+?)\s*$")) {
            return $Matches[1]
        }
    }
    return $null
}

function Invoke-CtlReadOnly {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [Parameter(Mandatory = $true)][string]$OutPath
    )

    $text = & $CtlPath $Command 2>&1 | ForEach-Object { $_.ToString() } | Out-String
    $text | Set-Content -Path $OutPath -Encoding UTF8
    if ($LASTEXITCODE -ne 0) {
        throw "opena8djctl $Command failed with exit code $LASTEXITCODE. See $OutPath"
    }
    return $text
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

$CtlPath = (Resolve-Path -LiteralPath $CtlPath).Path
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$outDir = Join-Path $repoRoot "local-analysis\windows-open-a8dj-virtual-probe-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

@(
    "safety_policy=virtual_endpoint_read_only_probe",
    "does_not_install_or_remove_devices=1",
    "does_not_open_audio_streams=1",
    "does_not_start_usb_streaming=1",
    "does_not_touch_audio_8_dj_usb=1",
    "does_not_write_control_state=1",
    "ctl_path=$CtlPath",
    "expected_api_version=$ExpectedApiVersion"
) | Set-Content -Path (Join-Path $outDir "safety.txt") -Encoding ASCII

$startedAt = Get-Date
$allPnp = @(Get-PnpDevice -ErrorAction SilentlyContinue)
$virtualPnp = @($allPnp | Where-Object {
    $_.InstanceId -like "ROOT\OpenA8DJVirtual*" -or
    $_.FriendlyName -eq "OpenA8DJ Virtual ACX Proof Endpoint"
})
$mediaPnp = @($allPnp | Where-Object {
    $_.Class -eq "MEDIA" -and
    ($_.FriendlyName -match "OpenA8DJ Virtual|OpenA8DJVirtual")
})
$virtualPnp | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-virtual.txt") -Encoding UTF8
$mediaPnp | Format-List * | Out-File -FilePath (Join-Path $outDir "pnp-media.txt") -Encoding UTF8

if ($virtualPnp.Count -ne 1) {
    throw "Expected exactly one OpenA8DJ virtual hardware device; found $($virtualPnp.Count)."
}
if ($virtualPnp[0].Status -ne "OK" -or $virtualPnp[0].Problem -ne "CM_PROB_NONE") {
    throw "Virtual PnP device is not clean: Status=$($virtualPnp[0].Status) Problem=$($virtualPnp[0].Problem)."
}

$virtualInstanceId = [string]$virtualPnp[0].InstanceId
$env:OPENA8DJ_INSTANCE_ID = $virtualInstanceId
Add-Content -Path (Join-Path $outDir "safety.txt") -Value "ctl_instance_id=$virtualInstanceId"

$surface = Invoke-CtlReadOnly -Command "surface" -OutPath (Join-Path $outDir "surface.txt")
$capabilities = Invoke-CtlReadOnly -Command "capabilities" -OutPath (Join-Path $outDir "capabilities.txt")
$topology = Invoke-CtlReadOnly -Command "topology" -OutPath (Join-Path $outDir "topology.txt")
$format = Invoke-CtlReadOnly -Command "format" -OutPath (Join-Path $outDir "format.txt")
$stream = Invoke-CtlReadOnly -Command "stream" -OutPath (Join-Path $outDir "stream.txt")
$diagnostics = Invoke-CtlReadOnly -Command "diagnostics" -OutPath (Join-Path $outDir "diagnostics.txt")

$apiValues = @(
    Get-Value -Text $surface -Name "api-version"
    Get-Value -Text $capabilities -Name "api-version"
    Get-Value -Text $diagnostics -Name "api-version"
) | Where-Object { $_ -ne $null }
foreach ($api in $apiValues) {
    if ([int]$api -ne $ExpectedApiVersion) {
        throw "Unexpected virtual API version $api. Expected $ExpectedApiVersion. Artifacts: $outDir"
    }
}

$surfaceEndpoint = Get-Value -Text $surface -Name "audio-endpoints"
$surfaceUsb = Get-Value -Text $surface -Name "usb-transport"
$capEndpoint = Get-Value -Text $capabilities -Name "audio-endpoint"
$capUsb = Get-Value -Text $capabilities -Name "usb-transport"
$streaming = Get-Value -Text $stream -Name "streaming"
$workerIterations = Get-Value -Text $diagnostics -Name "worker-iterations"
$bugChecks = @(Get-BugCheckEventsSince -Since $startedAt)

$summary = [ordered]@{
    started_at = $startedAt.ToUniversalTime().ToString("o")
    finished_at = (Get-Date).ToUniversalTime().ToString("o")
    ctl_path = $CtlPath
    ctl_instance_id = $virtualInstanceId
    expected_api_version = $ExpectedApiVersion
    virtual_pnp_count = $virtualPnp.Count
    media_pnp_count = $mediaPnp.Count
    api_versions_seen = $apiValues
    surface_audio_endpoints = $surfaceEndpoint
    surface_usb_transport = $surfaceUsb
    capabilities_audio_endpoint = $capEndpoint
    capabilities_usb_transport = $capUsb
    streaming = $streaming
    worker_iterations = $workerIterations
    bugcheck_events_since_start = $bugChecks.Count
    artifact_dir = $outDir
}
$summary | ConvertTo-Json -Depth 6 | Tee-Object -FilePath (Join-Path $outDir "summary.json")

if ($surfaceEndpoint -ne "ready" -or $capEndpoint -ne "yes") {
    throw "Virtual ACX endpoint was not reported ready. Artifacts: $outDir"
}
if ($surfaceUsb -ne "stub" -or $capUsb -ne "not-ready") {
    throw "Virtual target reported an unexpected USB state. Artifacts: $outDir"
}
if ($streaming -ne "no") {
    throw "Virtual endpoint was already streaming during read-only probe. Artifacts: $outDir"
}
if ($workerIterations -ne $null -and [UInt64]$workerIterations -ne 0) {
    throw "Virtual worker advanced during read-only probe: $workerIterations. Artifacts: $outDir"
}
if ($bugChecks.Count -ne 0) {
    throw "Bugcheck event appeared during virtual read-only probe. Artifacts: $outDir"
}

Write-Host "OpenA8DJ virtual endpoint read-only probe passed. Artifacts: $outDir"
