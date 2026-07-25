param(
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutDir = Join-Path $repoRoot "local-analysis\windows-a8dj-midi-endpoint-smoke-$stamp"
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

if (-not (Test-Path -LiteralPath $Python)) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        throw "Python was not found. Pass -Python or install Python."
    }
    $Python = $pythonCommand.Source
}

@(
    "safety_policy=midi_endpoint_enumeration_only"
    "does_not_open_audio_stream=1"
    "does_not_reset_usb=1"
    "does_not_change_default_audio_devices=1"
    "does_not_open_midi_devices=1"
) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

$probe = Join-Path $PSScriptRoot "a8dj_midi_endpoint_probe.py"
& $Python $probe --out-dir $OutDir
if ($LASTEXITCODE -ne 0) {
    throw "Audio 8 DJ MIDI endpoint probe failed with exit code $LASTEXITCODE"
}

Write-Host "Audio 8 DJ MIDI endpoint smoke artifacts: $OutDir"
