param(
    [Parameter(Mandatory = $true)]
    [string]$CtlPath,

    [int]$ExpectedApiVersion = 27,

    [int]$WaitSeconds = 5
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

function Get-BugCheckEventsSince {
    param([datetime]$Since)

    Get-WinEvent -FilterHashtable @{
        LogName = "System"
        Id = 1001
        StartTime = $Since
    } -ErrorAction SilentlyContinue |
        Where-Object { $_.ProviderName -eq "Microsoft-Windows-WER-SystemErrorReporting" }
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

$CtlPath = (Resolve-Path -LiteralPath $CtlPath).Path
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$outDir = Join-Path $repoRoot "local-analysis\windows-a8dj-readonly-control-canary-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $outDir -Force | Out-Null

@(
    "safety_policy=readonly_control_canary_only"
    "does_not_open_audio_endpoints=1"
    "does_not_start_streaming=1"
    "does_not_play_audio=1"
    "does_not_touch_traktor=1"
    "ctl_path=$CtlPath"
    "expected_api_version=$ExpectedApiVersion"
) | Set-Content -Path (Join-Path $outDir "safety.txt") -Encoding ASCII

$startedAt = Get-Date
Start-Sleep -Seconds $WaitSeconds

$surface = Invoke-CtlReadOnly -Command "surface" -OutPath (Join-Path $outDir "surface.txt")
$capabilities = Invoke-CtlReadOnly -Command "capabilities" -OutPath (Join-Path $outDir "capabilities.txt")
$stream = Invoke-CtlReadOnly -Command "stream" -OutPath (Join-Path $outDir "stream.txt")
$diagnostics = Invoke-CtlReadOnly -Command "diagnostics" -OutPath (Join-Path $outDir "diagnostics.txt")

$apiValues = @(
    Get-Value -Text $surface -Name "api-version"
    Get-Value -Text $capabilities -Name "api-version"
    Get-Value -Text $diagnostics -Name "api-version"
) | Where-Object { $_ -ne $null }
foreach ($api in $apiValues) {
    if ([int]$api -ne $ExpectedApiVersion) {
        throw "Unexpected loaded API version $api. Expected $ExpectedApiVersion. Artifacts: $outDir"
    }
}

$streaming = Get-Value -Text $stream -Name "streaming"
$workerIterations = Get-Value -Text $diagnostics -Name "worker-iterations"
$startRequests = Get-Value -Text $diagnostics -Name "start-requests"
$bugChecks = @(Get-BugCheckEventsSince -Since $startedAt)
$bugChecks |
    Select-Object TimeCreated, Id, ProviderName, Message |
    ConvertTo-Json -Depth 4 |
    Set-Content -Path (Join-Path $outDir "bugchecks-since-start.json") -Encoding UTF8

$summary = [ordered]@{
    started_at = $startedAt.ToUniversalTime().ToString("o")
    finished_at = (Get-Date).ToUniversalTime().ToString("o")
    ctl_path = $CtlPath
    expected_api_version = $ExpectedApiVersion
    api_versions_seen = $apiValues
    streaming = $streaming
    start_requests = $startRequests
    worker_iterations = $workerIterations
    bugcheck_events_since_start = $bugChecks.Count
    artifact_dir = $outDir
}
$summary | ConvertTo-Json -Depth 5 | Tee-Object -FilePath (Join-Path $outDir "summary.json")

if ($bugChecks.Count -ne 0) {
    throw "Bugcheck event appeared during read-only canary. Stop. Artifacts: $outDir"
}
if ($streaming -ne "no") {
    throw "Driver reports streaming=$streaming during read-only canary. Stop. Artifacts: $outDir"
}
if ($workerIterations -ne $null -and [UInt64]$workerIterations -ne 0) {
    throw "Worker iterations changed during read-only canary: $workerIterations. Stop. Artifacts: $outDir"
}
if ($startRequests -ne $null -and [UInt64]$startRequests -ne 0) {
    throw "Start requests changed during read-only canary: $startRequests. Stop. Artifacts: $outDir"
}

Write-Host "OpenA8DJ read-only control canary passed. Artifacts: $outDir"
