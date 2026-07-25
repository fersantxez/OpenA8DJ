param(
    [string]$CtlPath = "",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($CtlPath)) {
    $CtlPath = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
}
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutDir = Join-Path $repoRoot "local-analysis\windows-a8dj-version-preflight-$stamp"
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

if (-not (Test-Path -LiteralPath $CtlPath)) {
    throw "opena8djctl.exe not found at $CtlPath"
}

@(
    "safety_policy=version_preflight_read_only"
    "does_not_open_audio_stream=1"
    "does_not_reset_usb=1"
    "does_not_change_default_audio_devices=1"
) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

$headerPath = Join-Path $repoRoot "windows\include\OpenA8DJShared.h"
$infPath = Join-Path $repoRoot "windows\driver\OpenA8DJUsb.inf"
$distInfPath = Join-Path $repoRoot "windows\dist\Release\x64\OpenA8DJUsb.inf"
if (-not (Test-Path -LiteralPath $distInfPath)) {
    $distInfPath = Join-Path $repoRoot "windows\dist\Release\x64\opena8djusb.inf"
}

$sourceApi = $null
foreach ($line in Get-Content -Path $headerPath) {
    if ($line -match 'OPENA8DJ_DRIVER_API_VERSION\s+(\d+)') {
        $sourceApi = [int]$Matches[1]
        break
    }
}
if ($null -eq $sourceApi) {
    throw "Could not parse OPENA8DJ_DRIVER_API_VERSION from $headerPath"
}

function Get-DriverVer {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    foreach ($line in Get-Content -Path $Path) {
        if ($line -match '^DriverVer\s*=\s*(.+)$') {
            return $Matches[1].Trim()
        }
    }
    return $null
}

function Get-ApiVersion {
    param([string]$Text)

    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -match 'api-version:\s+(\d+)') {
            return [int]$Matches[1]
        }
    }
    return $null
}

$diagnosticsText = & $CtlPath diagnostics 2>&1 | ForEach-Object { $_.ToString() } | Out-String
$diagnosticsText | Set-Content -Path (Join-Path $OutDir "loaded-diagnostics.txt") -Encoding UTF8
$capabilitiesText = & $CtlPath capabilities 2>&1 | ForEach-Object { $_.ToString() } | Out-String
$capabilitiesText | Set-Content -Path (Join-Path $OutDir "loaded-capabilities.txt") -Encoding UTF8

$loadedApi = Get-ApiVersion -Text $diagnosticsText
if ($null -eq $loadedApi) {
    $loadedApi = Get-ApiVersion -Text $capabilitiesText
}

$summary = [ordered]@{
    source_api_version = $sourceApi
    source_driver_ver = Get-DriverVer -Path $infPath
    dist_driver_ver = Get-DriverVer -Path $distInfPath
    loaded_api_version = $loadedApi
    loaded_matches_source_api = ($loadedApi -eq $sourceApi)
    loaded_is_older_than_source = ($loadedApi -ne $null -and $loadedApi -lt $sourceApi)
    ctl_path = $CtlPath
    artifact_dir = $OutDir
}
$summary | ConvertTo-Json -Depth 4 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")
Write-Host "OpenA8DJ version preflight artifacts: $OutDir"
