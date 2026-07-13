param(
    [int]$PairSeconds = 6,
    [int]$PairStressSeconds = 10,
    [int]$InputSeconds = 4,
    [int]$TraktorSeconds = 900,
    [int]$TraktorStartupSeconds = 25,
    [int]$TraktorTrimWheelNotches = 0,
    [string]$TraktorTrackDirectory = "$env:USERPROFILE\Downloads\000_santxez_spring_25_select",
    [int]$CooldownSeconds = 8,
    [switch]$SkipAsio,
    [switch]$SkipControlMatrix,
    [switch]$SkipMidi,
    [switch]$SkipTraktor,
    [switch]$SkipWdmKsDiagnostic,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ctl = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
if (-not (Test-Path -LiteralPath $ctl)) {
    throw "opena8djctl.exe not found at $ctl"
}

function New-RunDirectory {
    param([string]$Requested)

    if ($Requested) {
        New-Item -ItemType Directory -Path $Requested -Force | Out-Null
        return (Resolve-Path $Requested).Path
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $path = Join-Path $repoRoot "local-analysis\windows-a8dj-full-validation-$stamp"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return $path
}

function Invoke-ScriptStep {
    param(
        [string]$Name,
        [string]$ScriptPath,
        [string[]]$Arguments,
        [int[]]$ExpectedExitCodes = @(0)
    )

    $stepDir = Join-Path $OutDir $Name
    New-Item -ItemType Directory -Path $stepDir -Force | Out-Null
    $logPath = Join-Path $stepDir "step-output.txt"
    $started = Get-Date
    $powershellArgs = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $ScriptPath
    ) + $Arguments

    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & powershell @powershellArgs 2>&1 | ForEach-Object { $_.ToString() } | Out-String
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    $output | Set-Content -Path $logPath -Encoding UTF8

    $summaryPath = Join-Path $stepDir "summary.json"
    $hasSummary = Test-Path -LiteralPath $summaryPath
    return [pscustomobject]@{
        name = $Name
        script = $ScriptPath
        exit_code = $exitCode
        expected_exit_codes = $ExpectedExitCodes
        pass = ($ExpectedExitCodes -contains $exitCode)
        seconds = [Math]::Round(((Get-Date) - $started).TotalSeconds, 2)
        log = $logPath
        summary = if ($hasSummary) { $summaryPath } else { $null }
    }
}

function Invoke-InlineStep {
    param(
        [string]$Name,
        [scriptblock]$Body
    )

    $stepDir = Join-Path $OutDir $Name
    New-Item -ItemType Directory -Path $stepDir -Force | Out-Null
    $started = Get-Date
    $logPath = Join-Path $stepDir "step-output.txt"
    try {
        $output = & $Body 2>&1 | ForEach-Object { $_.ToString() } | Out-String
        $exitCode = $LASTEXITCODE
        if ($null -eq $exitCode) {
            $exitCode = 0
        }
    } catch {
        $output = $_.Exception.ToString()
        $exitCode = 1
    }
    $output | Set-Content -Path $logPath -Encoding UTF8
    return [pscustomobject]@{
        name = $Name
        script = "inline"
        exit_code = $exitCode
        expected_exit_codes = @(0)
        pass = ($exitCode -eq 0)
        seconds = [Math]::Round(((Get-Date) - $started).TotalSeconds, 2)
        log = $logPath
        summary = $null
    }
}

function Invoke-Cooldown {
    param([string]$Label)

    if ($CooldownSeconds -le 0) {
        return
    }
    "cooldown=$Label seconds=$CooldownSeconds time=$(Get-Date -Format o)" |
        Add-Content -Path (Join-Path $OutDir "cooldowns.log") -Encoding UTF8
    Start-Sleep -Seconds $CooldownSeconds
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-a8dj-full-validation.lock"
$lockTaken = $false
$steps = @()

try {
    if (Test-Path $lockPath) {
        $existing = Get-Content -Raw -Path $lockPath -ErrorAction SilentlyContinue
        throw "Another Audio 8 DJ full validation may be active: $lockPath $existing"
    }
    @(
        "pid=$PID"
        "started=$(Get-Date -Format o)"
        "run_dir=$OutDir"
        "resource=Audio 8 DJ, iRig Stream, optional Traktor"
    ) | Set-Content -Path $lockPath -Encoding UTF8
    $lockTaken = $true

    @(
        "safety_policy=conservative_full_validation_shared_usb_hub"
        "does_not_reset_usb=1"
        "does_not_disable_or_enable_pnp_devices=1"
        "does_not_restart_windows_audio=1"
        "does_not_change_default_audio_devices=1"
        "always_restores_timecode_vinyl_48k_512=1"
        "never_arms_or_attempts_iso_silence=1"
        "version_preflight_captures_source_package_loaded_api=1"
        "asio_endpoint_probe_is_registry_read_only=1"
        "midi_endpoint_probe_is_read_only=1"
        "wdmks_input_diagnostic_accepts_legacy_failure_or_083_no_broken_capture=1"
        "traktor_trim_wheel_notches=$TraktorTrimWheelNotches"
        "traktor_track_directory=$TraktorTrackDirectory"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    $steps += Invoke-ScriptStep `
        -Name "version-preflight" `
        -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-version-preflight.ps1") `
        -Arguments @("-OutDir", (Join-Path $OutDir "version-preflight"))

    & $ctl profile timecode-vinyl | Set-Content -Path (Join-Path $OutDir "00-restore-before.txt") -Encoding UTF8
    & $ctl set-format 48000 512 | Add-Content -Path (Join-Path $OutDir "00-restore-before.txt") -Encoding UTF8
    "skipped: full validation never arms or attempts iso-silence" |
        Set-Content -Path (Join-Path $OutDir "00-iso-silence-before.txt") -Encoding UTF8

    $steps += Invoke-InlineStep "local-hardware-smoke" {
        & (Join-Path $repoRoot "windows\tests\run-local-hardware-smoke.ps1")
    }

    if (-not $SkipControlMatrix) {
        $steps += Invoke-ScriptStep `
            -Name "control-readback-matrix" `
            -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-control-readback-matrix.ps1") `
            -Arguments @("-OutDir", (Join-Path $OutDir "control-readback-matrix"))
    }

    if (-not $SkipMidi) {
        $steps += Invoke-ScriptStep `
            -Name "midi-endpoint-smoke" `
            -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-midi-endpoint-smoke.ps1") `
            -Arguments @("-OutDir", (Join-Path $OutDir "midi-endpoint-smoke"))
    }

    if (-not $SkipAsio) {
        $steps += Invoke-ScriptStep `
            -Name "asio-endpoint-smoke" `
            -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-asio-endpoint-smoke.ps1") `
            -Arguments @("-OutDir", (Join-Path $OutDir "asio-endpoint-smoke"))
    }

    Invoke-Cooldown "before-pair-matrix-48k-512-full"
    $steps += Invoke-ScriptStep `
        -Name "pair-matrix-48k-512-full" `
        -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-pair-matrix.ps1") `
        -Arguments @("-Seconds", "$PairSeconds", "-Rate", "48000", "-BlockSize", "512", "-Latency", "high", "-Mode", "full", "-OutDir", (Join-Path $OutDir "pair-matrix-48k-512-full"))

    Invoke-Cooldown "before-pair-matrix-44k-512-full"
    $steps += Invoke-ScriptStep `
        -Name "pair-matrix-44k-512-full" `
        -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-pair-matrix.ps1") `
        -Arguments @("-Seconds", "$PairSeconds", "-Rate", "44100", "-BlockSize", "512", "-Latency", "high", "-Mode", "full", "-OutDir", (Join-Path $OutDir "pair-matrix-44k-512-full"))

    Invoke-Cooldown "before-pair-matrix-48k-256-stress"
    $steps += Invoke-ScriptStep `
        -Name "pair-matrix-48k-256-stress" `
        -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-pair-matrix.ps1") `
        -Arguments @("-Seconds", "$PairStressSeconds", "-Rate", "48000", "-BlockSize", "256", "-Latency", "high", "-Mode", "all-pairs-only", "-OutDir", (Join-Path $OutDir "pair-matrix-48k-256-stress"))

    Invoke-Cooldown "before-input-endpoints-normal"
    $steps += Invoke-ScriptStep `
        -Name "input-endpoints-normal" `
        -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-input-endpoint-smoke.ps1") `
        -Arguments @("-Seconds", "$InputSeconds", "-Rate", "48000", "-BlockSize", "512", "-OutDir", (Join-Path $OutDir "input-endpoints-normal"))

    if (-not $SkipWdmKsDiagnostic) {
        Invoke-Cooldown "before-input-endpoints-wdmks-diagnostic"
        $steps += Invoke-ScriptStep `
            -Name "input-endpoints-wdmks-diagnostic" `
            -ScriptPath (Join-Path $repoRoot "windows\tests\run-a8dj-input-endpoint-smoke.ps1") `
            -Arguments @("-Seconds", "$InputSeconds", "-Rate", "48000", "-BlockSize", "512", "-IncludeWdmKs", "-OutDir", (Join-Path $OutDir "input-endpoints-wdmks-diagnostic")) `
            -ExpectedExitCodes @(0, 1)
    }

    if (-not $SkipTraktor) {
        Invoke-Cooldown "before-traktor-active-irig"
        $steps += Invoke-ScriptStep `
            -Name "traktor-active-irig" `
            -ScriptPath (Join-Path $repoRoot "windows\tests\run-traktor-active-smoke.ps1") `
            -Arguments @("-Seconds", "$TraktorSeconds", "-StartupSeconds", "$TraktorStartupSeconds", "-SampleIntervalSeconds", "5", "-TraktorTrimWheelNotches", "$TraktorTrimWheelNotches", "-TrackDirectory", "$TraktorTrackDirectory", "-CaptureIrig", "-OutDir", (Join-Path $OutDir "traktor-active-irig"))
    }
} finally {
    try {
        & $ctl profile timecode-vinyl | Set-Content -Path (Join-Path $OutDir "98-restore-after.txt") -Encoding UTF8
        & $ctl set-format 48000 512 | Add-Content -Path (Join-Path $OutDir "98-restore-after.txt") -Encoding UTF8
        "skipped: full validation never arms or attempts iso-silence" |
            Set-Content -Path (Join-Path $OutDir "99-iso-silence-after.txt") -Encoding UTF8
        & $ctl diagnostics | Set-Content -Path (Join-Path $OutDir "99-diagnostics-after.txt") -Encoding UTF8
    } catch {
        "restore failed: $($_.Exception.Message)" | Set-Content -Path (Join-Path $OutDir "restore-error.txt") -Encoding UTF8
    }
    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -LiteralPath $lockPath -Force
    }
}

$summary = [ordered]@{
    artifact_dir = $OutDir
    started = (Get-Item -LiteralPath $OutDir).CreationTime.ToString("o")
    finished = (Get-Date).ToString("o")
    pair_seconds = $PairSeconds
    pair_stress_seconds = $PairStressSeconds
    input_seconds = $InputSeconds
    traktor_seconds = if ($SkipTraktor) { 0 } else { $TraktorSeconds }
    traktor_trim_wheel_notches = $TraktorTrimWheelNotches
    traktor_track_directory = $TraktorTrackDirectory
    cooldown_seconds = $CooldownSeconds
    pass = (@($steps | Where-Object { -not $_.pass }).Count -eq 0)
    steps = $steps
}
$summary | ConvertTo-Json -Depth 6 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")
$qualitySummaryScript = Join-Path $repoRoot "windows\tests\summarize-a8dj-full-validation.ps1"
if (Test-Path -LiteralPath $qualitySummaryScript) {
    $qualityOutput = & $qualitySummaryScript -RunDir $OutDir 2>&1 | ForEach-Object { $_.ToString() } | Out-String
    $qualityExit = $LASTEXITCODE
    $qualityOutput | Set-Content -Path (Join-Path $OutDir "candidate-quality-step-output.txt") -Encoding UTF8
    if ($qualityExit -ne 0) {
        throw "OpenA8DJ candidate quality summary failed with exit code $qualityExit; see $OutDir"
    }
}
if (-not $summary.pass) {
    throw "OpenA8DJ full validation round had non-passing steps; see $OutDir"
}

Write-Host "OpenA8DJ full validation artifacts: $OutDir"
