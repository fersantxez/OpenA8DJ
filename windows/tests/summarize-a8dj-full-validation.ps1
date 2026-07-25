param(
    [Parameter(Mandatory = $true)]
    [string]$RunDir,
    [double]$WarnSystemCpuPercent = 85.0,
    [double]$FailSystemCpuPercent = 98.0,
    [switch]$StrictCandidateReady
)

$ErrorActionPreference = "Stop"

$RunDir = (Resolve-Path $RunDir).Path
$summaryPath = Join-Path $RunDir "summary.json"
if (-not (Test-Path -LiteralPath $summaryPath)) {
    throw "Full validation summary not found: $summaryPath"
}

function Read-Json {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return Get-Content -Raw -Path $Path | ConvertFrom-Json
}

function Add-Issue {
    param(
        [System.Collections.Generic.List[object]]$List,
        [string]$Severity,
        [string]$Area,
        [string]$Message
    )
    $List.Add([pscustomobject]@{
        severity = $Severity
        area = $Area
        message = $Message
    }) | Out-Null
}

function Parse-Diagnostics {
    param([string]$Path)

    $result = [ordered]@{}
    if (-not (Test-Path -LiteralPath $Path)) {
        return [pscustomobject]$result
    }
    foreach ($line in (Get-Content -Path $Path)) {
        if ($line -match '^\s*api-version:\s+(\d+)') {
            $result.api_version = [int]$Matches[1]
        } elseif ($line -match '^\s*underruns:\s+(\d+)') {
            $result.underruns = [int64]$Matches[1]
        } elseif ($line -match '^\s*overruns:\s+(\d+)') {
            $result.overruns = [int64]$Matches[1]
        } elseif ($line -match '^\s*packet-errors:\s+(\d+)') {
            $result.packet_errors = [int64]$Matches[1]
        } elseif ($line -match '^\s*late-completions:\s+(\d+)') {
            $result.late_completions = [int64]$Matches[1]
        } elseif ($line -match '^\s*ctl-rdbk-mismatch:\s+(\S+)') {
            $result.control_readback_mismatch = $Matches[1]
        } elseif ($line -match '^\s*streaming:\s+(\S+)') {
            $result.streaming = $Matches[1]
        } elseif ($line -match '^\s*sample-rate:\s+(\d+)') {
            $result.sample_rate = [int]$Matches[1]
        } elseif ($line -match '^\s*buffer-frames:\s+(\d+)') {
            $result.buffer_frames = [int]$Matches[1]
        }
    }
    return [pscustomobject]$result
}

function Delta-OrZero {
    param($Before, $After, [string]$Name)
    $beforeValue = if ($Before.PSObject.Properties.Name -contains $Name) { [int64]$Before.$Name } else { 0L }
    $afterValue = if ($After.PSObject.Properties.Name -contains $Name) { [int64]$After.$Name } else { 0L }
    return $afterValue - $beforeValue
}

function Get-RawByte {
    param(
        [string]$RawText,
        [int]$Index
    )

    if ([string]::IsNullOrWhiteSpace($RawText)) {
        return $null
    }
    $parts = @($RawText -split '\s+' | Where-Object { $_ -ne "" })
    if ($Index -lt 0 -or $Index -ge $parts.Count) {
        return $null
    }
    return [Convert]::ToInt32($parts[$Index], 16)
}

$full = Read-Json $summaryPath
$hardFailures = New-Object System.Collections.Generic.List[object]
$warnings = New-Object System.Collections.Generic.List[object]
$knownGaps = New-Object System.Collections.Generic.List[object]
$metrics = [ordered]@{
    full_runner_pass = [bool]$full.pass
    steps = @()
    source_api_version = $null
    source_driver_ver = $null
    dist_driver_ver = $null
    loaded_api_version = $null
    loaded_matches_source_api = $null
    pair_matrix_cases = 0
    pair_matrix_clipped_frames = 0
    pair_matrix_status_events = 0
    pair_matrix_driver_error_delta = 0
    traktor_present = $false
    traktor_seconds = 0
    traktor_playback_gesture_active = $null
    traktor_playback_gesture_delta = $null
    traktor_cpu_percent_one_core = $null
    traktor_top_cpu_process_names = $null
    traktor_top_cpu_processes = @()
    traktor_system_cpu_avg = $null
    traktor_driver_error_delta = $null
    traktor_worker_iterations_delta = $null
    traktor_worker_iterations_per_second = $null
    traktor_worker_capture_bytes_delta = $null
    traktor_worker_playback_bytes_delta = $null
    traktor_worker_no_render_delta = $null
    traktor_worker_render_mask = $null
    traktor_worker_capture_mask = $null
    irig_capture_clipped_frames = $null
    irig_capture_near_clip_frames = $null
    irig_raw_click_outliers = $null
    normal_input_targets = $null
    normal_input_opened = $null
    normal_input_signal_like = $null
    wdmks_targets = $null
    wdmks_failed = $null
    control_cases = $null
    control_mismatches = $null
    control_expected_unsupported_phono_ground = $null
    control_unexpected_mismatches = $null
    midi_input_device_count = $null
    midi_output_device_count = $null
    midi_matching_input_count = $null
    midi_matching_output_count = $null
    midi_ready = $null
    asio_driver_count = $null
    asio_matching_opena8dj_count = $null
    asio_third_party_audio8dj_count = $null
    asio_ready = $null
}

if (-not $full.pass) {
    Add-Issue $hardFailures "fail" "runner" "Full validation runner pass=false."
}

foreach ($step in @($full.steps)) {
    $metrics.steps += [pscustomobject]@{
        name = $step.name
        pass = [bool]$step.pass
        exit_code = [int]$step.exit_code
        seconds = [double]$step.seconds
    }
    if (-not $step.pass) {
        Add-Issue $hardFailures "fail" $step.name "Step did not meet its expected exit code."
    }
}

$versionPreflight = Read-Json (Join-Path $RunDir "version-preflight\summary.json")
if ($versionPreflight -ne $null) {
    $metrics.source_api_version = [int]$versionPreflight.source_api_version
    $metrics.source_driver_ver = [string]$versionPreflight.source_driver_ver
    $metrics.dist_driver_ver = [string]$versionPreflight.dist_driver_ver
    $metrics.loaded_api_version = if ($versionPreflight.loaded_api_version -ne $null) { [int]$versionPreflight.loaded_api_version } else { $null }
    $metrics.loaded_matches_source_api = [bool]$versionPreflight.loaded_matches_source_api
    if ($versionPreflight.loaded_api_version -eq $null) {
        Add-Issue $hardFailures "fail" "version" "Could not read the loaded OpenA8DJ driver API version."
    } elseif ([bool]$versionPreflight.loaded_is_older_than_source) {
        Add-Issue $knownGaps "gap" "version" "Hardware validation is running loaded API $($versionPreflight.loaded_api_version), while source/package is API $($versionPreflight.source_api_version) ($($versionPreflight.dist_driver_ver)); API $($versionPreflight.source_api_version) behavior is not locally loaded."
    } elseif (-not [bool]$versionPreflight.loaded_matches_source_api) {
        Add-Issue $warnings "warn" "version" "Loaded API $($versionPreflight.loaded_api_version) does not match source API $($versionPreflight.source_api_version)."
    }
} else {
    $finalDiagnostics = Parse-Diagnostics (Join-Path $RunDir "99-diagnostics-after.txt")
    if ($finalDiagnostics.PSObject.Properties.Name -contains "api_version") {
        $metrics.loaded_api_version = [int]$finalDiagnostics.api_version
    }
    Add-Issue $knownGaps "gap" "version" "No version preflight artifact was present; loaded driver/source API alignment was not proven for this run."
}

foreach ($pairName in @("pair-matrix-48k-512-full", "pair-matrix-44k-512-full", "pair-matrix-48k-256-stress")) {
    $pairDir = Join-Path $RunDir $pairName
    $pair = Read-Json (Join-Path $pairDir "summary.json")
    if ($null -eq $pair) {
        Add-Issue $hardFailures "fail" $pairName "Missing pair matrix summary."
        continue
    }
    foreach ($case in @($pair.results)) {
        $metrics.pair_matrix_cases++
        $metrics.pair_matrix_clipped_frames += [int64]$case.capture_clipped_frames
        $metrics.pair_matrix_status_events += @($case.status_events).Count
        if ([int64]$case.capture_clipped_frames -ne 0) {
            Add-Issue $hardFailures "fail" $pairName "$($case.name) clipped $($case.capture_clipped_frames) frames."
        }
        if (@($case.status_events).Count -ne 0) {
            Add-Issue $hardFailures "fail" $pairName "$($case.name) had PortAudio status events."
        }
        if ([double]$case.cpu_system_avg_percent -ge $WarnSystemCpuPercent) {
            Add-Issue $warnings "warn" $pairName "$($case.name) system CPU average was $([Math]::Round([double]$case.cpu_system_avg_percent, 2))%."
        }
        if ([double]$case.cpu_system_avg_percent -ge $FailSystemCpuPercent) {
            Add-Issue $hardFailures "fail" $pairName "$($case.name) system CPU average reached $([Math]::Round([double]$case.cpu_system_avg_percent, 2))%."
        }
    }
    foreach ($caseDir in (Get-ChildItem -Path $pairDir -Directory -ErrorAction SilentlyContinue)) {
        $before = Parse-Diagnostics (Join-Path $caseDir.FullName "diagnostics-before.txt")
        $after = Parse-Diagnostics (Join-Path $caseDir.FullName "diagnostics-after.txt")
        $driverDelta =
            (Delta-OrZero $before $after "underruns") +
            (Delta-OrZero $before $after "overruns") +
            (Delta-OrZero $before $after "packet_errors") +
            (Delta-OrZero $before $after "late_completions")
        $metrics.pair_matrix_driver_error_delta += $driverDelta
        if ($driverDelta -ne 0) {
            Add-Issue $hardFailures "fail" $pairName "$($caseDir.Name) moved driver error counters by $driverDelta."
        }
    }
}

$normalInput = Read-Json (Join-Path $RunDir "input-endpoints-normal\summary.json")
if ($normalInput -eq $null) {
    Add-Issue $hardFailures "fail" "input" "Missing normal input endpoint summary."
} else {
    $metrics.normal_input_targets = [int]$normalInput.target_count
    $metrics.normal_input_opened = [int]$normalInput.opened_count
    $metrics.normal_input_signal_like = [int]$normalInput.signal_like_endpoint_count
    if (-not [bool]$normalInput.pass_endpoint_open) {
        Add-Issue $hardFailures "fail" "input" "Normal MME/DirectSound/WASAPI input endpoints did not all open cleanly."
    }
    foreach ($name in @("underruns", "overruns", "packet_errors", "late_completions")) {
        if ([int64]$normalInput.driver_deltas.$name -ne 0) {
            Add-Issue $hardFailures "fail" "input" "Normal input driver delta $name=$($normalInput.driver_deltas.$name)."
        }
    }
    if ([int]$normalInput.signal_like_endpoint_count -eq 0) {
        Add-Issue $knownGaps "gap" "input-quality" "No Audio 8 DJ input endpoint crossed the known-signal threshold; DVS/timecode input quality is not proven."
    }
}

$wdmInput = Read-Json (Join-Path $RunDir "input-endpoints-wdmks-diagnostic\summary.json")
if ($wdmInput -ne $null) {
    $metrics.wdmks_targets = [int]$wdmInput.target_count
    $metrics.wdmks_failed = [int]$wdmInput.failed_count
    if ([int]$wdmInput.failed_count -ne 0) {
        Add-Issue $knownGaps "gap" "wdmks" "$($wdmInput.failed_count) WDM-KS input endpoints failed to open, matching the capture-position IOCTL compatibility gap."
    }
}

$control = Read-Json (Join-Path $RunDir "control-readback-matrix\summary.json")
if ($control -ne $null) {
    $metrics.control_cases = [int]$control.cases
    $metrics.control_mismatches = [int]$control.mismatched
    if ($control.PSObject.Properties.Name -contains "expected_unsupported_phono_ground_cases") {
        $metrics.control_expected_unsupported_phono_ground = [int]$control.expected_unsupported_phono_ground_cases
        $metrics.control_unexpected_mismatches = [int]$control.unexpected_mismatches
    } else {
        $matrixRows = Read-Json (Join-Path $RunDir "control-readback-matrix\ground-readback-matrix.json")
        $expectedUnsupported = 0
        $unexpected = [int]$control.mismatched
        if ($matrixRows -ne $null) {
            $unexpected = 0
            foreach ($row in @($matrixRows)) {
                $isMismatch = "$($row.Mismatch)" -eq "yes"
                $requestGroundByte = Get-RawByte -RawText "$($row.Request)" -Index 3
                $readbackGroundByte = Get-RawByte -RawText "$($row.Readback)" -Index 3
                $expectedUnsupportedPhonoGround =
                    ($isMismatch -and
                     "$($row.GndPhono)" -eq "on" -and
                     $requestGroundByte -ne $null -and
                     $readbackGroundByte -ne $null -and
                     (($requestGroundByte -band 0x04) -ne 0) -and
                     (($readbackGroundByte -band 0x04) -eq 0))
                if ($expectedUnsupportedPhonoGround) {
                    $expectedUnsupported++
                } elseif ($isMismatch) {
                    $unexpected++
                }
            }
        }
        $metrics.control_expected_unsupported_phono_ground = $expectedUnsupported
        $metrics.control_unexpected_mismatches = $unexpected
    }
    if ([int]$control.stream_error_cases -ne 0) {
        Add-Issue $hardFailures "fail" "controls" "Control matrix moved stream error counters."
    }
    if ([int]$metrics.control_unexpected_mismatches -ne 0) {
        Add-Issue $hardFailures "fail" "controls" "$($metrics.control_unexpected_mismatches)/$($control.cases) control combinations had unexpected readback mismatch."
    }
    if ([int]$metrics.control_expected_unsupported_phono_ground -ne 0) {
        Add-Issue $knownGaps "gap" "controls" "$($metrics.control_expected_unsupported_phono_ground)/$($control.cases) control combinations requested the phono ground bit, which this local Audio 8 DJ did not confirm in hardware readback."
    }
}

$midi = Read-Json (Join-Path $RunDir "midi-endpoint-smoke\summary.json")
if ($midi -ne $null) {
    $metrics.midi_input_device_count = [int]$midi.input_device_count
    $metrics.midi_output_device_count = [int]$midi.output_device_count
    $metrics.midi_matching_input_count = [int]$midi.matching_input_count
    $metrics.midi_matching_output_count = [int]$midi.matching_output_count
    $metrics.midi_ready = [bool]$midi.midi_ready
    if (-not [bool]$midi.pass_enumeration) {
        Add-Issue $hardFailures "fail" "midi" "MIDI endpoint enumeration failed."
    } elseif (-not [bool]$midi.midi_ready) {
        Add-Issue $knownGaps "gap" "midi" "No matching Audio 8 DJ/OpenA8DJ MIDI input+output endpoints were found by Windows winmm enumeration."
    }
} else {
    Add-Issue $knownGaps "gap" "midi" "MIDI publication is still planned, not validated as functional."
}

$asio = Read-Json (Join-Path $RunDir "asio-endpoint-smoke\summary.json")
if ($asio -ne $null) {
    $metrics.asio_driver_count = [int]$asio.asio_driver_count
    $metrics.asio_matching_opena8dj_count = [int]$asio.matching_opena8dj_count
    $metrics.asio_third_party_audio8dj_count = [int]$asio.third_party_audio8dj_count
    $metrics.asio_ready = [bool]$asio.opena8dj_asio_ready
    if (-not [bool]$asio.pass_enumeration) {
        Add-Issue $hardFailures "fail" "asio" "ASIO registry enumeration failed."
    } elseif (-not [bool]$asio.opena8dj_asio_ready) {
        if ([int]$asio.third_party_audio8dj_count -gt 0) {
            Add-Issue $knownGaps "gap" "asio" "Windows has third-party Audio 8 DJ ASIO registration but no OpenA8DJ ASIO driver registration."
        } else {
            Add-Issue $knownGaps "gap" "asio" "No OpenA8DJ ASIO driver registration was found."
        }
    }
} else {
    Add-Issue $knownGaps "gap" "asio" "ASIO is still planned, not validated as functional."
}

$traktor = Read-Json (Join-Path $RunDir "traktor-active-irig\summary.json")
if ($traktor -ne $null) {
    $metrics.traktor_present = $true
    $metrics.traktor_seconds = [int]$traktor.seconds
    if ($traktor.PSObject.Properties.Name -contains "playback_gesture_active") {
        $metrics.traktor_playback_gesture_active = [bool]$traktor.playback_gesture_active
        $metrics.traktor_playback_gesture_delta = [int64]$traktor.playback_gesture_delta
    }
    $metrics.traktor_cpu_percent_one_core = [double]$traktor.traktor_cpu_percent_one_core
    if ($traktor.PSObject.Properties.Name -contains "top_cpu_process_names") {
        $metrics.traktor_top_cpu_process_names = [string]$traktor.top_cpu_process_names
    }
    if ($traktor.PSObject.Properties.Name -contains "top_cpu_processes") {
        $metrics.traktor_top_cpu_processes = @($traktor.top_cpu_processes)
    }
    $metrics.traktor_system_cpu_avg = [double]$traktor.system_cpu_avg
    if ($traktor.PSObject.Properties.Name -contains "worker_iterations_delta") {
        $metrics.traktor_worker_iterations_delta = [int64]$traktor.worker_iterations_delta
        $metrics.traktor_worker_iterations_per_second = [double]$traktor.worker_iterations_per_second
        $metrics.traktor_worker_capture_bytes_delta = [int64]$traktor.worker_capture_bytes_delta
        $metrics.traktor_worker_playback_bytes_delta = [int64]$traktor.worker_playback_bytes_delta
        $metrics.traktor_worker_no_render_delta = [int64]$traktor.worker_no_render_delta
        $metrics.traktor_worker_render_mask = [int]$traktor.worker_render_mask
        $metrics.traktor_worker_capture_mask = [int]$traktor.worker_capture_mask
    }
    $traktorDriverDelta =
        [int64]$traktor.underruns_delta +
        [int64]$traktor.overruns_delta +
        [int64]$traktor.packet_errors_delta +
        [int64]$traktor.late_completions_delta
    $metrics.traktor_driver_error_delta = $traktorDriverDelta
    if (-not [bool]$traktor.pass) {
        Add-Issue $hardFailures "fail" "traktor" "Traktor active smoke pass=false."
    }
    if (($traktor.PSObject.Properties.Name -contains "playback_gesture_active") -and -not [bool]$traktor.playback_gesture_active) {
        Add-Issue $hardFailures "fail" "traktor" "Traktor playback gesture did not produce render-pair activity."
    }
    if ($traktorDriverDelta -ne 0) {
        Add-Issue $hardFailures "fail" "traktor" "Traktor moved driver error counters by $traktorDriverDelta."
    }
    if ([double]$traktor.system_cpu_avg -ge $WarnSystemCpuPercent) {
        $topNames = if ($metrics.traktor_top_cpu_process_names) { " Top CPU processes: $($metrics.traktor_top_cpu_process_names)." } else { "" }
        Add-Issue $warnings "warn" "traktor" "Traktor system CPU average was $([Math]::Round([double]$traktor.system_cpu_avg, 2))%.$topNames"
    }
    if ([double]$traktor.system_cpu_avg -ge $FailSystemCpuPercent) {
        Add-Issue $hardFailures "fail" "traktor" "Traktor system CPU average reached $([Math]::Round([double]$traktor.system_cpu_avg, 2))%."
    }
    if (($traktor.PSObject.Properties.Name -contains "worker_iterations_delta") -and
        [int64]$traktor.worker_iterations_delta -eq 0 -and
        [int64]$traktor.render_frames_delta -gt 0) {
        Add-Issue $warnings "warn" "worker-diagnostics" "Traktor streamed audio but worker iteration counters did not advance; the loaded driver likely predates API 25 worker diagnostics."
    }
    if (-not ($traktor.PSObject.Properties.Name -contains "top_cpu_processes")) {
        Add-Issue $knownGaps "gap" "performance-attribution" "Traktor run did not include top-process CPU attribution."
    }
}

$irig = Read-Json (Join-Path $RunDir "traktor-active-irig\traktor-irig-metrics.json")
if ($irig -ne $null) {
    $metrics.irig_capture_clipped_frames = [int64]$irig.capture_clipped_frames
    $metrics.irig_capture_near_clip_frames = [int64]$irig.capture_near_clip_frames
    $metrics.irig_raw_click_outliers = [int64]$irig.raw_click_outliers
    if ([int64]$irig.capture_clipped_frames -ne 0 -or [int64]$irig.capture_near_clip_frames -ne 0) {
        Add-Issue $hardFailures "fail" "irig" "Traktor iRig capture clipped or near-clipped."
    }
    if ([int64]$irig.raw_click_outliers -ne 0) {
        Add-Issue $hardFailures "fail" "irig" "Traktor iRig capture had raw click outliers."
    }
    if (@($irig.status_events).Count -ne 0) {
        Add-Issue $hardFailures "fail" "irig" "Traktor iRig capture had PortAudio status events."
    }
}

Add-Issue $knownGaps "gap" "signing-install" "Production signing and public install matrix are not complete."

$hardPass = ($hardFailures.Count -eq 0)
$candidateReady = $hardPass -and ($warnings.Count -eq 0) -and ($knownGaps.Count -eq 0)
$hardFailureArray = $hardFailures.ToArray()
$warningArray = $warnings.ToArray()
$knownGapArray = $knownGaps.ToArray()
$report = [ordered]@{
    run_dir = $RunDir
    generated = (Get-Date).ToString("o")
    hard_regression_pass = $hardPass
    candidate_ready = $candidateReady
    warning_count = $warnings.Count
    known_gap_count = $knownGaps.Count
    metrics = $metrics
    hard_failures = $hardFailureArray
    warnings = $warningArray
    known_gaps = $knownGapArray
}

$jsonPath = Join-Path $RunDir "candidate-quality-summary.json"
$mdPath = Join-Path $RunDir "candidate-quality-summary.md"
$report | ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding UTF8

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("# OpenA8DJ Candidate Quality Summary") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- hard_regression_pass: $hardPass") | Out-Null
$lines.Add("- candidate_ready: $candidateReady") | Out-Null
$lines.Add("- warnings: $($warnings.Count)") | Out-Null
$lines.Add("- known_gaps: $($knownGaps.Count)") | Out-Null
$lines.Add("- run_dir: $RunDir") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("## Key Metrics") | Out-Null
$lines.Add("") | Out-Null
$lines.Add("- source_api_version: $($metrics.source_api_version)") | Out-Null
$lines.Add("- source_driver_ver: $($metrics.source_driver_ver)") | Out-Null
$lines.Add("- dist_driver_ver: $($metrics.dist_driver_ver)") | Out-Null
$lines.Add("- loaded_api_version: $($metrics.loaded_api_version)") | Out-Null
$lines.Add("- loaded_matches_source_api: $($metrics.loaded_matches_source_api)") | Out-Null
$lines.Add("- pair_matrix_cases: $($metrics.pair_matrix_cases)") | Out-Null
$lines.Add("- pair_matrix_clipped_frames: $($metrics.pair_matrix_clipped_frames)") | Out-Null
$lines.Add("- pair_matrix_status_events: $($metrics.pair_matrix_status_events)") | Out-Null
$lines.Add("- pair_matrix_driver_error_delta: $($metrics.pair_matrix_driver_error_delta)") | Out-Null
$lines.Add("- traktor_seconds: $($metrics.traktor_seconds)") | Out-Null
$lines.Add("- traktor_playback_gesture_active: $($metrics.traktor_playback_gesture_active)") | Out-Null
$lines.Add("- traktor_playback_gesture_delta: $($metrics.traktor_playback_gesture_delta)") | Out-Null
$lines.Add("- traktor_cpu_percent_one_core: $($metrics.traktor_cpu_percent_one_core)") | Out-Null
$lines.Add("- traktor_top_cpu_process_names: $($metrics.traktor_top_cpu_process_names)") | Out-Null
$lines.Add("- traktor_system_cpu_avg: $($metrics.traktor_system_cpu_avg)") | Out-Null
$lines.Add("- traktor_driver_error_delta: $($metrics.traktor_driver_error_delta)") | Out-Null
$lines.Add("- traktor_worker_iterations_delta: $($metrics.traktor_worker_iterations_delta)") | Out-Null
$lines.Add("- traktor_worker_iterations_per_second: $($metrics.traktor_worker_iterations_per_second)") | Out-Null
$lines.Add("- traktor_worker_capture_bytes_delta: $($metrics.traktor_worker_capture_bytes_delta)") | Out-Null
$lines.Add("- traktor_worker_playback_bytes_delta: $($metrics.traktor_worker_playback_bytes_delta)") | Out-Null
$lines.Add("- traktor_worker_no_render_delta: $($metrics.traktor_worker_no_render_delta)") | Out-Null
$lines.Add("- traktor_worker_render_mask: $($metrics.traktor_worker_render_mask)") | Out-Null
$lines.Add("- traktor_worker_capture_mask: $($metrics.traktor_worker_capture_mask)") | Out-Null
$lines.Add("- irig_capture_clipped_frames: $($metrics.irig_capture_clipped_frames)") | Out-Null
$lines.Add("- irig_raw_click_outliers: $($metrics.irig_raw_click_outliers)") | Out-Null
$lines.Add("- normal_input_opened: $($metrics.normal_input_opened)/$($metrics.normal_input_targets)") | Out-Null
$lines.Add("- normal_input_signal_like: $($metrics.normal_input_signal_like)") | Out-Null
$lines.Add("- wdmks_failed: $($metrics.wdmks_failed)/$($metrics.wdmks_targets)") | Out-Null
$lines.Add("- control_mismatches: $($metrics.control_mismatches)/$($metrics.control_cases)") | Out-Null
$lines.Add("- control_expected_unsupported_phono_ground: $($metrics.control_expected_unsupported_phono_ground)") | Out-Null
$lines.Add("- control_unexpected_mismatches: $($metrics.control_unexpected_mismatches)") | Out-Null
$lines.Add("- midi_matching_inputs: $($metrics.midi_matching_input_count)") | Out-Null
$lines.Add("- midi_matching_outputs: $($metrics.midi_matching_output_count)") | Out-Null
$lines.Add("- asio_matching_opena8dj: $($metrics.asio_matching_opena8dj_count)") | Out-Null
$lines.Add("- asio_third_party_audio8dj: $($metrics.asio_third_party_audio8dj_count)") | Out-Null
foreach ($section in @(
    @{ Title = "Hard Failures"; Items = $hardFailures },
    @{ Title = "Warnings"; Items = $warnings },
    @{ Title = "Known Gaps"; Items = $knownGaps }
)) {
    $lines.Add("") | Out-Null
    $lines.Add("## $($section.Title)") | Out-Null
    $lines.Add("") | Out-Null
    if ($section.Items.Count -eq 0) {
        $lines.Add("- none") | Out-Null
    } else {
        foreach ($item in $section.Items) {
            $lines.Add("- [$($item.area)] $($item.message)") | Out-Null
        }
    }
}
$lines | Set-Content -Path $mdPath -Encoding UTF8

Write-Host "Candidate quality summary: $jsonPath"
Write-Host "Candidate quality markdown: $mdPath"
Write-Host "hard_regression_pass=$hardPass"
Write-Host "candidate_ready=$candidateReady"
Write-Host "warnings=$($warnings.Count)"
Write-Host "known_gaps=$($knownGaps.Count)"

if (-not $hardPass) {
    exit 1
}
if ($StrictCandidateReady -and -not $candidateReady) {
    exit 2
}
