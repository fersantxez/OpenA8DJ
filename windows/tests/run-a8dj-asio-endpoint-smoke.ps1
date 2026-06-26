param(
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutDir = Join-Path $repoRoot "local-analysis\windows-a8dj-asio-endpoint-smoke-$stamp"
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

@(
    "safety_policy=asio_registry_enumeration_only"
    "does_not_open_audio_stream=1"
    "does_not_reset_usb=1"
    "does_not_change_default_audio_devices=1"
    "does_not_create_com_objects=1"
) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

function Test-ContainsAny {
    param(
        [string]$Text,
        [string[]]$Needles
    )

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return $false
    }
    $lower = $Text.ToLowerInvariant()
    foreach ($needle in $Needles) {
        if ($lower.Contains($needle)) {
            return $true
        }
    }
    return $false
}

function Get-ComServerPath {
    param([string]$Clsid)

    if ([string]::IsNullOrWhiteSpace($Clsid)) {
        return $null
    }
    foreach ($path in @(
        "Registry::HKEY_CLASSES_ROOT\CLSID\$Clsid\InprocServer32",
        "Registry::HKEY_CLASSES_ROOT\WOW6432Node\CLSID\$Clsid\InprocServer32",
        "HKLM:\SOFTWARE\Classes\CLSID\$Clsid\InprocServer32",
        "HKLM:\SOFTWARE\Classes\WOW6432Node\CLSID\$Clsid\InprocServer32"
    )) {
        if (Test-Path -LiteralPath $path) {
            $item = Get-ItemProperty -LiteralPath $path
            $default = $item.'(default)'
            if ([string]::IsNullOrWhiteSpace($default)) {
                $default = (Get-Item -LiteralPath $path).GetValue("")
            }
            if (-not [string]::IsNullOrWhiteSpace($default)) {
                return [string]$default
            }
        }
    }
    return $null
}

$entries = @()
foreach ($root in @("HKLM:\SOFTWARE\ASIO", "HKLM:\SOFTWARE\WOW6432Node\ASIO", "HKCU:\SOFTWARE\ASIO")) {
    if (-not (Test-Path -LiteralPath $root)) {
        continue
    }
    foreach ($child in Get-ChildItem -LiteralPath $root) {
        $props = Get-ItemProperty -LiteralPath $child.PSPath
        $clsid = [string]$props.CLSID
        $description = [string]$props.Description
        $dll = [string]$props.Dll
        $comServer = Get-ComServerPath -Clsid $clsid
        $haystack = @($child.PSChildName, $description, $dll, $comServer) -join " "
        $matchesOpenA8DJ = Test-ContainsAny -Text $haystack -Needles @("opena8dj", "open a8dj")
        $matchesAudio8DJ = Test-ContainsAny -Text $haystack -Needles @("audio 8 dj", "a8dj", "native instruments")

        $entries += [pscustomobject]@{
            registry_root = $root
            name = $child.PSChildName
            clsid = $clsid
            description = $description
            dll = $dll
            com_server = $comServer
            matches_opena8dj = $matchesOpenA8DJ
            matches_audio8dj = $matchesAudio8DJ
        }
    }
}

$openA8DJEntries = @($entries | Where-Object { $_.matches_opena8dj })
$audio8Entries = @($entries | Where-Object { $_.matches_audio8dj -and -not $_.matches_opena8dj })
$summary = [ordered]@{
    pass_enumeration = $true
    asio_driver_count = @($entries).Count
    matching_opena8dj_count = $openA8DJEntries.Count
    third_party_audio8dj_count = $audio8Entries.Count
    opena8dj_asio_ready = $openA8DJEntries.Count -gt 0
    entries = $entries
}

$entries | ConvertTo-Json -Depth 5 | Set-Content -Path (Join-Path $OutDir "asio-drivers.json") -Encoding UTF8
$summary | ConvertTo-Json -Depth 6 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")
Write-Host "Audio 8 DJ ASIO endpoint smoke artifacts: $OutDir"
