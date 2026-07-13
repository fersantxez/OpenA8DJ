param(
    [int]$Seconds = 30,
    [int]$StartupSeconds = 25,
    [int]$SampleIntervalSeconds = 3,
    [int]$PnpMonitorIntervalSeconds = 10,
    [string]$TrackPath = "",
    [string]$TrackDirectory = "$env:USERPROFILE\Downloads\000_santxez_spring_25_select",
    [string]$TraktorPath = "C:\Program Files\Native Instruments\Traktor Pro 3\Traktor.exe",
    [switch]$CaptureIrig,
    [string]$IrigInputName = "Line In (iRig Stream)",
    [int]$IrigRate = 44100,
    [int]$IrigBlockSize = 4096,
    [double]$IrigCpuSampleIntervalSeconds = 1.0,
    [int]$TraktorTrimWheelNotches = 0,
    [string]$Python = "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe",
    [int]$MinTotalNonZeroDelta = 1,
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$ctl = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
$fallbackTrackPath = "C:\ProgramData\Native Instruments\Traktor Pro 3\Factory Sounds\Carbon Decay - In The Warehouse.mp3"

if (-not (Test-Path $ctl)) {
    throw "opena8djctl.exe not found at $ctl"
}
if (-not (Test-Path $TraktorPath)) {
    throw "Traktor not found at $TraktorPath"
}

function Resolve-TraktorTrackPath {
    param(
        [string]$RequestedTrackPath,
        [string]$RequestedTrackDirectory,
        [string]$FallbackTrackPath
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedTrackPath)) {
        if (-not (Test-Path -LiteralPath $RequestedTrackPath)) {
            throw "Track not found at $RequestedTrackPath"
        }
        return (Resolve-Path -LiteralPath $RequestedTrackPath).Path
    }

    if (-not [string]::IsNullOrWhiteSpace($RequestedTrackDirectory) -and (Test-Path -LiteralPath $RequestedTrackDirectory)) {
        $tracks = @(Get-ChildItem -LiteralPath $RequestedTrackDirectory -File -Filter *.mp3 -ErrorAction SilentlyContinue | Sort-Object Name)
        if ($tracks.Count -gt 0) {
            return $tracks[0].FullName
        }
    }

    if (Test-Path -LiteralPath $FallbackTrackPath) {
        return (Resolve-Path -LiteralPath $FallbackTrackPath).Path
    }

    throw "No MP3 track found. Checked TrackPath='$RequestedTrackPath', TrackDirectory='$RequestedTrackDirectory', fallback='$FallbackTrackPath'"
}

$resolvedTrackPath = Resolve-TraktorTrackPath -RequestedTrackPath $TrackPath -RequestedTrackDirectory $TrackDirectory -FallbackTrackPath $fallbackTrackPath
$TrackPath = $resolvedTrackPath
if ($CaptureIrig -and -not (Test-Path $Python)) {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $pythonCommand) {
        throw "Python was not found. Pass -Python or install Python."
    }
    $Python = $pythonCommand.Source
}

function New-RunDirectory {
    param([string]$Requested)

    if ($Requested) {
        New-Item -ItemType Directory -Path $Requested -Force | Out-Null
        return (Resolve-Path $Requested).Path
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $path = Join-Path $repoRoot "local-analysis\windows-traktor-active-smoke-$stamp"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return $path
}

function Write-PnpSnapshot {
    param(
        [string]$Path,
        [string]$Label
    )

    $devices = Get-PnpDevice -PresentOnly |
        Where-Object {
            $_.FriendlyName -match 'iRig|Audio 8 DJ|IK Multimedia|Native Instruments' -or
            $_.InstanceId -match 'VID_1963&PID_0059|VID_17CC&PID_1978'
        } |
        Sort-Object Class,FriendlyName,InstanceId |
        Select-Object Class,Status,FriendlyName,InstanceId

    @(
        "snapshot=$Label"
        "time=$(Get-Date -Format o)"
        "note=read_only_pnp_enumeration_no_usb_reset_no_device_restart"
        ""
        ($devices | Format-Table -AutoSize | Out-String)
    ) | Set-Content -Path $Path -Encoding UTF8
}

function Test-RequiredHardware {
    param([string]$RunDir)

    $present = Get-PnpDevice -PresentOnly
    $audio8Usb = $present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' }
    $audio8Out = $present | Where-Object { $_.FriendlyName -like 'Speakers (Audio 8 DJ)*' }
    $irigUsb = $present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' }
    $irigEndpoint = $present | Where-Object { $_.FriendlyName -eq $IrigInputName }

    $lines = @(
        "hardware_preflight_time=$(Get-Date -Format o)"
        "found_audio8_usb=$([int][bool]$audio8Usb)"
        "found_audio8_output_endpoint=$([int][bool]$audio8Out)"
        "capture_irig=$([int][bool]$CaptureIrig)"
        "found_irig_usb=$([int][bool]$irigUsb)"
        "found_irig_capture_endpoint=$([int][bool]$irigEndpoint)"
        "policy=block_if_missing_do_not_recover_unattended"
    )
    $lines | Set-Content -Path (Join-Path $RunDir "hardware-preflight.txt") -Encoding UTF8

    if (-not $audio8Usb -or -not $audio8Out) {
        throw "Audio 8 DJ hardware/endpoints are not stable/visible; see $(Join-Path $RunDir 'hardware-preflight.txt')"
    }
    if ($CaptureIrig -and (-not $irigUsb -or -not $irigEndpoint)) {
        throw "iRig hardware/endpoint is not stable/visible; see $(Join-Path $RunDir 'hardware-preflight.txt')"
    }
}

function Parse-Diagnostics {
    param([string[]]$Lines)

    $result = [ordered]@{
        streaming = $null
        channels = $null
        render_frames = 0L
        capture_frames = 0L
        underruns = 0L
        overruns = 0L
        packet_errors = 0L
        late_completions = 0L
        pair1 = 0L
        pair2 = 0L
        pair3 = 0L
        pair4 = 0L
        worker_iterations = 0L
        worker_capture_bytes = 0L
        worker_playback_bytes = 0L
        worker_no_render = 0L
        worker_last_capture_bytes = 0L
        worker_last_playback_bytes = 0L
        worker_render_mask = 0
        worker_capture_mask = 0
        worker_max_capture_bytes = 0L
        worker_max_playback_bytes = 0L
    }

    foreach ($line in $Lines) {
        if ($line -match '^\s*streaming:\s+(\S+)') { $result.streaming = $Matches[1] }
        elseif ($line -match '^\s*acx-rt-channels:\s+(\d+)') { $result.channels = [int]$Matches[1] }
        elseif ($line -match '^\s*render-frames:\s+(\d+)') { $result.render_frames = [int64]$Matches[1] }
        elseif ($line -match '^\s*capture-frames:\s+(\d+)') { $result.capture_frames = [int64]$Matches[1] }
        elseif ($line -match '^\s*underruns:\s+(\d+)') { $result.underruns = [int64]$Matches[1] }
        elseif ($line -match '^\s*overruns:\s+(\d+)') { $result.overruns = [int64]$Matches[1] }
        elseif ($line -match '^\s*packet-errors:\s+(\d+)') { $result.packet_errors = [int64]$Matches[1] }
        elseif ($line -match '^\s*late-completions:\s+(\d+)') { $result.late_completions = [int64]$Matches[1] }
        elseif ($line -match '^\s*acx-render-pair-1:\s+nonzero=(\d+)') { $result.pair1 = [int64]$Matches[1] }
        elseif ($line -match '^\s*acx-render-pair-2:\s+nonzero=(\d+)') { $result.pair2 = [int64]$Matches[1] }
        elseif ($line -match '^\s*acx-render-pair-3:\s+nonzero=(\d+)') { $result.pair3 = [int64]$Matches[1] }
        elseif ($line -match '^\s*acx-render-pair-4:\s+nonzero=(\d+)') { $result.pair4 = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-iterations:\s+(\d+)') { $result.worker_iterations = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-cap-bytes:\s+(\d+)') { $result.worker_capture_bytes = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-play-bytes:\s+(\d+)') { $result.worker_playback_bytes = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-no-render:\s+(\d+)') { $result.worker_no_render = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-last-cap:\s+(\d+)') { $result.worker_last_capture_bytes = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-last-play:\s+(\d+)') { $result.worker_last_playback_bytes = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-render-mask:\s*0x([0-9a-fA-F]+)') { $result.worker_render_mask = [Convert]::ToInt32($Matches[1], 16) }
        elseif ($line -match '^\s*worker-capt-mask:\s*0x([0-9a-fA-F]+)') { $result.worker_capture_mask = [Convert]::ToInt32($Matches[1], 16) }
        elseif ($line -match '^\s*worker-max-cap:\s+(\d+)') { $result.worker_max_capture_bytes = [int64]$Matches[1] }
        elseif ($line -match '^\s*worker-max-play:\s+(\d+)') { $result.worker_max_playback_bytes = [int64]$Matches[1] }
    }
    return [pscustomobject]$result
}

function Get-RenderPairTotal {
    param($Diagnostics)

    return ([int64]$Diagnostics.pair1 + [int64]$Diagnostics.pair2 + [int64]$Diagnostics.pair3 + [int64]$Diagnostics.pair4)
}

function Test-RenderPairActivity {
    param(
        [int]$Seconds = 4
    )

    $beforeText = & $ctl diagnostics
    $before = Parse-Diagnostics $beforeText
    Start-Sleep -Seconds $Seconds
    $afterText = & $ctl diagnostics
    $after = Parse-Diagnostics $afterText

    return [pscustomobject]@{
        active = ((Get-RenderPairTotal $after) -gt (Get-RenderPairTotal $before))
        before_total = Get-RenderPairTotal $before
        after_total = Get-RenderPairTotal $after
        delta = (Get-RenderPairTotal $after) - (Get-RenderPairTotal $before)
        seconds = $Seconds
    }
}

function Initialize-MouseInterop {
    if ("Win32.MouseNative" -as [type]) {
        return
    }
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetProcessDPIAware();
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetForegroundWindow(System.IntPtr hWnd);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool ShowWindow(System.IntPtr hWnd, int nCmdShow);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern bool SetCursorPos(int X, int Y);
[System.Runtime.InteropServices.DllImport("user32.dll")]
public static extern void mouse_event(int dwFlags, int dx, int dy, int dwData, int dwExtraInfo);
"@ -Name MouseNative -Namespace Win32
    [Win32.MouseNative]::SetProcessDPIAware() | Out-Null
}

function Set-TraktorForeground {
    param(
        [System.__ComObject]$Shell,
        [int]$ProcessId
    )

    Initialize-MouseInterop
    $process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($process -and $process.MainWindowHandle -ne [IntPtr]::Zero) {
        [Win32.MouseNative]::ShowWindow($process.MainWindowHandle, 3) | Out-Null
        Start-Sleep -Milliseconds 250
        [Win32.MouseNative]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
    }
    $Shell.AppActivate($ProcessId) | Out-Null
    Start-Sleep -Milliseconds 900
}

function Invoke-MouseClickAt {
    param(
        [int]$X,
        [int]$Y,
        [int]$Count = 1
    )

    Initialize-MouseInterop
    [Win32.MouseNative]::SetCursorPos($X, $Y) | Out-Null
    Start-Sleep -Milliseconds 120
    for ($i = 0; $i -lt $Count; $i++) {
        [Win32.MouseNative]::mouse_event(0x0002, 0, 0, 0, 0)
        Start-Sleep -Milliseconds 50
        [Win32.MouseNative]::mouse_event(0x0004, 0, 0, 0, 0)
        Start-Sleep -Milliseconds 160
    }
}

function Invoke-MouseDragAt {
    param(
        [int]$FromX,
        [int]$FromY,
        [int]$ToX,
        [int]$ToY,
        [int]$Steps = 16
    )

    Initialize-MouseInterop
    [Win32.MouseNative]::SetCursorPos($FromX, $FromY) | Out-Null
    Start-Sleep -Milliseconds 180
    [Win32.MouseNative]::mouse_event(0x0002, 0, 0, 0, 0)
    Start-Sleep -Milliseconds 180
    for ($i = 1; $i -le $Steps; $i++) {
        $x = [int]($FromX + (($ToX - $FromX) * $i / $Steps))
        $y = [int]($FromY + (($ToY - $FromY) * $i / $Steps))
        [Win32.MouseNative]::SetCursorPos($x, $y) | Out-Null
        Start-Sleep -Milliseconds 45
    }
    Start-Sleep -Milliseconds 180
    [Win32.MouseNative]::mouse_event(0x0004, 0, 0, 0, 0)
    Start-Sleep -Milliseconds 500
}

function Invoke-MouseWheelAt {
    param(
        [int]$X,
        [int]$Y,
        [int]$Notches
    )

    Initialize-MouseInterop
    if ($Notches -eq 0) {
        return
    }
    [Win32.MouseNative]::SetCursorPos($X, $Y) | Out-Null
    Start-Sleep -Milliseconds 120
    $count = [Math]::Abs($Notches)
    $delta = if ($Notches -gt 0) { 120 } else { -120 }
    for ($i = 0; $i -lt $count; $i++) {
        [Win32.MouseNative]::mouse_event(0x0800, 0, 0, $delta, 0)
        Start-Sleep -Milliseconds 80
    }
}

function Invoke-TraktorOutputTrimGesture {
    param(
        [System.__ComObject]$Shell,
        [int]$ProcessId,
        [int]$WheelDownNotches
    )

    if ($WheelDownNotches -le 0) {
        return
    }

    Initialize-MouseInterop
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    Set-TraktorForeground -Shell $Shell -ProcessId $ProcessId

    # Traktor persists knob values. During capture validation, trim the channel
    # A gain and master output down so the external mixer/iRig route does not
    # invalidate click metrics by hard-clipping the capture.
    $channelAGainX = [int]($screen.Width * 0.426)
    $channelAGainY = [int]($screen.Height * 0.119)
    $mainX = [int]($screen.Width * 0.572)
    $mainY = [int]($screen.Height * 0.120)

    Invoke-MouseClickAt -X $channelAGainX -Y $channelAGainY -Count 1
    Invoke-MouseWheelAt -X $channelAGainX -Y $channelAGainY -Notches (-1 * $WheelDownNotches)
    Start-Sleep -Milliseconds 250
    Invoke-MouseClickAt -X $mainX -Y $mainY -Count 1
    Invoke-MouseWheelAt -X $mainX -Y $mainY -Notches (-1 * $WheelDownNotches)
    Start-Sleep -Seconds 1
}

function Invoke-TraktorBrowserLoadGesture {
    param(
        [System.__ComObject]$Shell,
        [int]$ProcessId,
        [double]$AllTracksY = 0.40,
        [double]$AllTracksTabY = 0.20,
        [double]$TrackRowY = 0.292,
        [double]$DeckDropY = 0.15,
        [double]$PlayY = 0.153
    )

    Initialize-MouseInterop
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    Set-TraktorForeground -Shell $Shell -ProcessId $ProcessId

    # Coordinates are deliberately relative to the primary screen: this tablet
    # runs Traktor full-screen during unattended validation.
    $allTracksX = [int]($screen.Width * 0.09)
    $allTracksTreeY = [int]($screen.Height * $AllTracksY)
    $allTracksTabX = [int]($screen.Width * 0.70)
    $allTracksTabY = [int]($screen.Height * $AllTracksTabY)
    $trackX = [int]($screen.Width * 0.52)
    $trackY = [int]($screen.Height * $TrackRowY)
    $deckDropX = [int]($screen.Width * 0.18)
    $deckDropY = [int]($screen.Height * $DeckDropY)
    $deckPlayX = [int]($screen.Width * 0.036)
    $deckPlayY = [int]($screen.Height * $PlayY)

    Invoke-MouseClickAt -X $allTracksX -Y $allTracksTreeY -Count 1
    Start-Sleep -Milliseconds 700
    Invoke-MouseClickAt -X $allTracksTabX -Y $allTracksTabY -Count 1
    Start-Sleep -Milliseconds 700
    Invoke-MouseClickAt -X $trackX -Y $trackY -Count 1
    Start-Sleep -Milliseconds 700
    $Shell.SendKeys("{ENTER}")
    Start-Sleep -Seconds 1
    Invoke-MouseClickAt -X $trackX -Y $trackY -Count 2
    Start-Sleep -Seconds 1
    Invoke-MouseDragAt -FromX $trackX -FromY $trackY -ToX $deckDropX -ToY $deckDropY
    Start-Sleep -Seconds 2
    Invoke-MouseClickAt -X $deckPlayX -Y $deckPlayY -Count 1
    Start-Sleep -Milliseconds 700
    $Shell.SendKeys(" ")
    Start-Sleep -Seconds 2
}

function Invoke-TraktorPlaybackGesture {
    param(
        [System.__ComObject]$Shell,
        [int]$ProcessId,
        [string]$OutDir,
        [int]$TrimWheelNotches = 0
    )

    $attempts = New-Object System.Collections.Generic.List[object]
    Set-TraktorForeground -Shell $Shell -ProcessId $ProcessId
    Invoke-TraktorOutputTrimGesture -Shell $Shell -ProcessId $ProcessId -WheelDownNotches $TrimWheelNotches
    @("{ENTER}", " ", "{F1}", "{F5}") | ForEach-Object {
        $Shell.SendKeys($_)
        Start-Sleep -Milliseconds 800
    }
    Start-Sleep -Seconds 3
    $activity = Test-RenderPairActivity -Seconds 4
    $attempts.Add([pscustomobject]@{
        method = "keyboard-playback-gesture"
        active = [bool]$activity.active
        delta = [int64]$activity.delta
        before_total = [int64]$activity.before_total
        after_total = [int64]$activity.after_total
    }) | Out-Null

    if (-not $activity.active) {
        $browserAttempts = @(
            @{ Method = "browser-compact-all-tracks-row1"; AllTracksY = 0.40; AllTracksTabY = 0.20; TrackRowY = 0.292; DeckDropY = 0.15; PlayY = 0.153 },
            @{ Method = "browser-compact-all-tracks-row2"; AllTracksY = 0.40; AllTracksTabY = 0.20; TrackRowY = 0.338; DeckDropY = 0.15; PlayY = 0.153 },
            @{ Method = "browser-mixer-all-tracks-row1"; AllTracksY = 0.84; AllTracksTabY = 0.64; TrackRowY = 0.735; DeckDropY = 0.20; PlayY = 0.487 },
            @{ Method = "browser-mixer-all-tracks-row2"; AllTracksY = 0.84; AllTracksTabY = 0.64; TrackRowY = 0.780; DeckDropY = 0.20; PlayY = 0.487 }
        )
        foreach ($browserAttempt in $browserAttempts) {
            Invoke-TraktorBrowserLoadGesture `
                -Shell $Shell `
                -ProcessId $ProcessId `
                -AllTracksY $browserAttempt.AllTracksY `
                -AllTracksTabY $browserAttempt.AllTracksTabY `
                -TrackRowY $browserAttempt.TrackRowY `
                -DeckDropY $browserAttempt.DeckDropY `
                -PlayY $browserAttempt.PlayY
            $activity = Test-RenderPairActivity -Seconds 4
            $attempts.Add([pscustomobject]@{
                method = $browserAttempt.Method
                active = [bool]$activity.active
                delta = [int64]$activity.delta
                before_total = [int64]$activity.before_total
                after_total = [int64]$activity.after_total
            }) | Out-Null
            if ($activity.active) {
                break
            }
        }
    }

    $attempts | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir "traktor-playback-gesture.json") -Encoding UTF8
    if (-not $activity.active) {
        try {
            Add-Type -AssemblyName System.Drawing
            $screen = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
            $bitmap = New-Object System.Drawing.Bitmap $screen.Width, $screen.Height
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            $graphics.CopyFromScreen($screen.Location, [System.Drawing.Point]::Empty, $screen.Size)
            $bitmap.Save((Join-Path $OutDir "traktor-playback-gesture-failed.png"), [System.Drawing.Imaging.ImageFormat]::Png)
            $graphics.Dispose()
            $bitmap.Dispose()
        } catch {
            "screenshot failed: $($_.Exception.Message)" | Set-Content -Path (Join-Path $OutDir "traktor-playback-gesture-screenshot-error.txt") -Encoding UTF8
        }
    }
    return $activity
}

function Get-ProcessCpuSnapshot {
    $rows = foreach ($process in (Get-Process -ErrorAction SilentlyContinue)) {
        if ($null -eq $process.CPU) {
            continue
        }
        [pscustomobject]@{
            pid = [int]$process.Id
            name = [string]$process.ProcessName
            cpu_seconds = [double]$process.CPU
            working_set_mb = [Math]::Round(([double]$process.WorkingSet64 / 1MB), 1)
        }
    }
    return @($rows)
}

function Get-TopProcessCpuDelta {
    param(
        [object[]]$Before,
        [object[]]$After,
        [double]$ElapsedSeconds,
        [int]$Top = 10
    )

    if ($ElapsedSeconds -le 0.0) {
        return @()
    }

    $beforeByPid = @{}
    foreach ($process in @($Before)) {
        $beforeByPid[[int]$process.pid] = $process
    }

    $rows = foreach ($process in @($After)) {
        $processId = [int]$process.pid
        if (-not $beforeByPid.ContainsKey($processId)) {
            continue
        }
        $delta = [double]$process.cpu_seconds - [double]$beforeByPid[$processId].cpu_seconds
        if ($delta -le 0.0) {
            continue
        }
        [pscustomobject]@{
            pid = $processId
            name = [string]$process.name
            cpu_seconds_delta = [Math]::Round($delta, 3)
            cpu_percent_one_core = [Math]::Round(($delta / $ElapsedSeconds) * 100.0, 2)
            working_set_mb = [double]$process.working_set_mb
        }
    }

    return @($rows | Sort-Object cpu_percent_one_core -Descending | Select-Object -First $Top)
}

function Get-TopProcessCpuAggregate {
    param(
        [object[]]$Samples,
        [int]$Top = 12
    )

    $rows = @()
    foreach ($sample in @($Samples)) {
        foreach ($process in @($sample.top_processes)) {
            $rows += [pscustomobject]@{
                name = [string]$process.name
                cpu_seconds_delta = [double]$process.cpu_seconds_delta
                cpu_percent_one_core = [double]$process.cpu_percent_one_core
                working_set_mb = [double]$process.working_set_mb
            }
        }
    }

    $groups = $rows | Group-Object name
    $aggregate = foreach ($group in @($groups)) {
        $cpu = @($group.Group | Select-Object -ExpandProperty cpu_seconds_delta)
        $pct = @($group.Group | Select-Object -ExpandProperty cpu_percent_one_core)
        $mem = @($group.Group | Select-Object -ExpandProperty working_set_mb)
        [pscustomobject]@{
            name = [string]$group.Name
            samples = [int]$group.Count
            cpu_seconds_delta = [Math]::Round((($cpu | Measure-Object -Sum).Sum), 3)
            max_cpu_percent_one_core = [Math]::Round((($pct | Measure-Object -Maximum).Maximum), 2)
            avg_cpu_percent_one_core = [Math]::Round((($pct | Measure-Object -Average).Average), 2)
            max_working_set_mb = [Math]::Round((($mem | Measure-Object -Maximum).Maximum), 1)
        }
    }

    return @($aggregate | Sort-Object cpu_seconds_delta -Descending | Select-Object -First $Top)
}

function New-SystemCpuSampler {
    try {
        $counter = New-Object System.Diagnostics.PerformanceCounter("Processor", "% Processor Time", "_Total")
        [void]$counter.NextValue()
        return [pscustomobject]@{
            source = "PerformanceCounter"
            counter = $counter
        }
    } catch {
        return [pscustomobject]@{
            source = "CIM"
            counter = $null
        }
    }
}

function Get-SystemCpuSample {
    param($Sampler)

    if ($Sampler -and $Sampler.source -eq "PerformanceCounter" -and $Sampler.counter) {
        try {
            return [Math]::Round([double]$Sampler.counter.NextValue(), 2)
        } catch {
            return $null
        }
    }

    try {
        return [double](Get-CimInstance Win32_PerfFormattedData_PerfOS_Processor -Filter "Name='_Total'" -ErrorAction Stop).PercentProcessorTime
    } catch {
        return $null
    }
}

function New-IrigCaptureScript {
    param([string]$Path)

    $script = @"
from pathlib import Path
import json
import time

import numpy as np
import sounddevice as sd
import soundfile as sf

try:
    import psutil
except Exception:
    psutil = None

out = Path(r"$Path")
out.mkdir(parents=True, exist_ok=True)
rate = int($IrigRate)
seconds = float($Seconds)
blocksize = int($IrigBlockSize)
cpu_interval = float($IrigCpuSampleIntervalSeconds)
input_name = "$IrigInputName"
frames_total = int(rate * seconds)
capture = np.zeros((frames_total, 2), dtype=np.float32)
status_events = []
pos = 0
cpu = []

def hostapi_name(device):
    return sd.query_hostapis(device["hostapi"])["name"]

def find_input():
    fallback = None
    for index, device in enumerate(sd.query_devices()):
        if input_name.lower() not in device["name"].lower():
            continue
        if int(device["max_input_channels"]) < 2:
            continue
        if fallback is None:
            fallback = index
        if "MME" in hostapi_name(device):
            return index
    if fallback is None:
        raise SystemExit(f"input not found: {input_name}")
    return fallback

def callback(indata, frames, _time_info, status):
    global pos
    if status:
        status_events.append(str(status))
    end = min(pos + frames, frames_total)
    count = end - pos
    if count:
        capture[pos:end, :] = indata[:count, :]
    pos += frames
    if pos >= frames_total:
        raise sd.CallbackStop

input_device = find_input()
if psutil is not None:
    psutil.cpu_percent(interval=None)
capture_process_priority = "default"
if psutil is not None:
    try:
        psutil.Process().nice(psutil.HIGH_PRIORITY_CLASS)
        capture_process_priority = "high"
    except Exception as exc:
        capture_process_priority = "default_priority_set_failed:" + str(exc)
start = time.perf_counter()
with sd.InputStream(device=input_device, channels=2, samplerate=rate, blocksize=blocksize, latency="high", dtype="float32", callback=callback):
    while pos < frames_total:
        if psutil is not None:
            cpu.append(float(psutil.cpu_percent(interval=cpu_interval)))
        else:
            time.sleep(cpu_interval)
elapsed = time.perf_counter() - start
abs_capture = np.abs(capture)
diff = np.diff(capture, axis=0)
diff_abs = np.max(np.abs(diff), axis=1) if len(diff) else np.zeros(0, dtype=np.float32)
mad = float(np.median(np.abs(diff_abs - np.median(diff_abs)))) if len(diff_abs) else 0.0
sigma = mad / 0.6745 if mad > 0.0 else 0.0
threshold = max(0.075, 12.0 * sigma)
metrics = {
    "input_device": int(input_device),
    "input_name": sd.query_devices(input_device)["name"],
    "rate": rate,
    "seconds": seconds,
    "elapsed_seconds": elapsed,
    "capture_peak": float(np.max(abs_capture)),
    "capture_rms": float(np.sqrt(np.mean(capture * capture))),
    "capture_clipped_frames": int(np.sum(np.any(abs_capture >= 0.999, axis=1))),
    "capture_near_clip_frames": int(np.sum(np.any(abs_capture >= 0.98, axis=1))),
    "raw_click_threshold": float(threshold),
    "raw_click_outliers": int(np.sum(diff_abs > threshold)),
    "status_events": status_events,
    "status_event_count": int(len(status_events)),
    "system_cpu_avg_percent": float(np.mean(cpu)) if cpu else None,
    "system_cpu_max_percent": float(np.max(cpu)) if cpu else None,
    "system_cpu_samples": int(len(cpu)),
    "system_cpu_sample_interval_seconds": cpu_interval,
    "capture_process_priority": capture_process_priority,
}
sf.write(out / "traktor-irig-captured.wav", capture, rate, subtype="PCM_24")
(out / "traktor-irig-metrics.json").write_text(json.dumps(metrics, indent=2) + "\n", encoding="utf-8")
print(json.dumps(metrics, indent=2))
"@
    $scriptPath = Join-Path $Path "capture-irig.py"
    $script | Set-Content -Path $scriptPath -Encoding UTF8
    return $scriptPath
}

function Start-PnpMonitor {
    param(
        [string]$Path,
        [int]$MonitorSeconds,
        [int]$IntervalSeconds = 10
    )

    Start-Job -ScriptBlock {
        param($Path, $Seconds, $IntervalSeconds, $IrigInputName)
        $deadline = (Get-Date).AddSeconds($Seconds)
        "time`tirig_usb`tirig_capture_endpoint`taudio8_usb`taudio8_output_endpoint" |
            Set-Content -Path $Path -Encoding UTF8

        while ((Get-Date) -lt $deadline) {
            $present = Get-PnpDevice -PresentOnly
            $irigUsb = [int][bool]($present | Where-Object { $_.InstanceId -match 'VID_1963&PID_0059' })
            $irigEndpoint = [int][bool]($present | Where-Object { $_.FriendlyName -eq $IrigInputName })
            $audio8Usb = [int][bool]($present | Where-Object { $_.InstanceId -match 'VID_17CC&PID_1978' })
            $audio8Out = [int][bool]($present | Where-Object { $_.FriendlyName -like 'Speakers (Audio 8 DJ)*' })
            "$(Get-Date -Format o)`t$irigUsb`t$irigEndpoint`t$audio8Usb`t$audio8Out" |
                Add-Content -Path $Path -Encoding UTF8
            Start-Sleep -Seconds $IntervalSeconds
        }
    } -ArgumentList $Path, $MonitorSeconds, $IntervalSeconds, $IrigInputName
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-traktor-active-smoke.lock"
$lockTaken = $false
$monitorJob = $null
$startedTraktor = $false
$proc = $null

try {
    if (Test-Path $lockPath) {
        $existing = Get-Content -Raw -Path $lockPath -ErrorAction SilentlyContinue
        throw "Another Traktor active smoke run may be active: $lockPath $existing"
    }

    @(
        "pid=$PID"
        "started=$(Get-Date -Format o)"
        "run_dir=$OutDir"
        "resource=Traktor, Audio 8 DJ, optional iRig Stream"
    ) | Set-Content -Path $lockPath -Encoding UTF8
    $lockTaken = $true

    @(
        "safety_policy=conservative_traktor_audio_smoke"
        "does_not_reset_usb=1"
        "does_not_disable_or_enable_pnp_devices=1"
        "does_not_restart_windows_audio=1"
        "does_not_change_default_audio_devices=1"
        "never_arms_or_attempts_iso_silence=1"
        "track_path=$TrackPath"
        "pnp_monitor_interval_seconds=$PnpMonitorIntervalSeconds"
        "traktor_trim_wheel_notches=$TraktorTrimWheelNotches"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-before.txt") -Label "before"
    Test-RequiredHardware -RunDir $OutDir
    "skipped: Traktor smoke never arms or attempts iso-silence" |
        Set-Content -Path (Join-Path $OutDir "iso-silence-before.txt") -Encoding UTF8
    & $ctl diagnostics | Set-Content -Path (Join-Path $OutDir "diagnostics-before.txt") -Encoding UTF8

    $monitorJob = Start-PnpMonitor -Path (Join-Path $OutDir "pnp-monitor.tsv") -MonitorSeconds ($StartupSeconds + $Seconds + 40) -IntervalSeconds $PnpMonitorIntervalSeconds

    $existingTraktor = Get-Process Traktor -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($existingTraktor) {
        $proc = $existingTraktor
    } else {
        $proc = Start-Process -FilePath $TraktorPath -ArgumentList ('"' + $TrackPath + '"') -PassThru
        $startedTraktor = $true
    }

    Start-Sleep -Seconds $StartupSeconds
    $shell = New-Object -ComObject WScript.Shell
    $playbackActivity = Invoke-TraktorPlaybackGesture -Shell $shell -ProcessId $proc.Id -OutDir $OutDir -TrimWheelNotches $TraktorTrimWheelNotches

    $startDiagText = & $ctl diagnostics
    $startDiagText | Set-Content -Path (Join-Path $OutDir "diagnostics-after-play-keys.txt") -Encoding UTF8
    $startDiag = Parse-Diagnostics $startDiagText
    $startTime = Get-Date
    $startTraktorProcess = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    $startTraktorCpu = if ($startTraktorProcess) { $startTraktorProcess.CPU } else { $null }

    $captureJob = $null
    if ($CaptureIrig) {
        $captureScript = New-IrigCaptureScript -Path $OutDir
        $captureJob = Start-Job -ScriptBlock {
            param($Python, $ScriptPath, $OutputPath)
            & $Python $ScriptPath *> $OutputPath
            exit $LASTEXITCODE
        } -ArgumentList $Python, $captureScript, (Join-Path $OutDir "irig-capture-output.txt")
    }

    $samples = @()
    $processCpuSamples = @()
    $systemCpuSampler = New-SystemCpuSampler
    $deadline = (Get-Date).AddSeconds($Seconds)
    $sampleIndex = 0
    while ((Get-Date) -lt $deadline) {
        $diagText = & $ctl diagnostics
        $diagText | Set-Content -Path (Join-Path $OutDir ("diagnostics-sample-{0:D2}.txt" -f $sampleIndex)) -Encoding UTF8
        $parsed = Parse-Diagnostics $diagText
        $traktorProcess = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
        $systemCpu = Get-SystemCpuSample -Sampler $systemCpuSampler
        $samples += [pscustomobject]@{
            sample = $sampleIndex
            time = (Get-Date).ToString("o")
            traktor_cpu_seconds = if ($traktorProcess) { $traktorProcess.CPU } else { $null }
            system_cpu_percent = $systemCpu
            system_cpu_source = $systemCpuSampler.source
            streaming = $parsed.streaming
            channels = $parsed.channels
            packet_errors = $parsed.packet_errors
            late_completions = $parsed.late_completions
            pair1 = $parsed.pair1
            pair2 = $parsed.pair2
            pair3 = $parsed.pair3
            pair4 = $parsed.pair4
            worker_iterations = $parsed.worker_iterations
            worker_capture_bytes = $parsed.worker_capture_bytes
            worker_playback_bytes = $parsed.worker_playback_bytes
            worker_no_render = $parsed.worker_no_render
            worker_render_mask = $parsed.worker_render_mask
            worker_capture_mask = $parsed.worker_capture_mask
        }
        $processBefore = Get-ProcessCpuSnapshot
        $processStarted = Get-Date
        $sampleIndex++
        Start-Sleep -Seconds $SampleIntervalSeconds
        $processAfter = Get-ProcessCpuSnapshot
        $processElapsed = [Math]::Max(0.001, ((Get-Date) - $processStarted).TotalSeconds)
        $processCpuSamples += [pscustomobject]@{
            sample = ($sampleIndex - 1)
            start_time = $processStarted.ToString("o")
            elapsed_seconds = [Math]::Round($processElapsed, 3)
            top_processes = @(Get-TopProcessCpuDelta -Before $processBefore -After $processAfter -ElapsedSeconds $processElapsed)
        }
    }
    $samples | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir "samples.json") -Encoding UTF8
    $processCpuSamples | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $OutDir "top-process-samples.json") -Encoding UTF8

    if ($captureJob) {
        Wait-Job $captureJob -Timeout ($Seconds + 15) | Out-Null
        $captureState = $captureJob.State
        $captureExit = Receive-Job $captureJob -Keep
        $captureExit | Out-String | Set-Content -Path (Join-Path $OutDir "irig-capture-job.txt") -Encoding UTF8
        if ($captureState -ne "Completed") {
            Stop-Job $captureJob -ErrorAction SilentlyContinue
            throw "iRig capture job did not complete: state=$captureState"
        }
        Remove-Job $captureJob -Force -ErrorAction SilentlyContinue
    }

    $endDiagText = & $ctl diagnostics
    $endDiagText | Set-Content -Path (Join-Path $OutDir "diagnostics-before-close.txt") -Encoding UTF8
    $endDiag = Parse-Diagnostics $endDiagText
    $elapsedSeconds = [Math]::Max(0.001, ((Get-Date) - $startTime).TotalSeconds)
    $endTraktorProcess = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    $endTraktorCpu = if ($endTraktorProcess) { $endTraktorProcess.CPU } else { $null }
    $systemCpuValues = @($samples | Where-Object { $_.system_cpu_percent -ne $null } | Select-Object -ExpandProperty system_cpu_percent)
    $topProcessAggregate = @(Get-TopProcessCpuAggregate -Samples $processCpuSamples)

    $summary = [ordered]@{
        track_path = $TrackPath
        traktor_pid = $proc.Id
        started_traktor = [bool]$startedTraktor
        seconds = $Seconds
        elapsed_seconds = [Math]::Round($elapsedSeconds, 2)
        capture_irig = [bool]$CaptureIrig
        pnp_monitor_interval_seconds = $PnpMonitorIntervalSeconds
        traktor_trim_wheel_notches = $TraktorTrimWheelNotches
        playback_gesture_active = [bool]$playbackActivity.active
        playback_gesture_delta = [int64]$playbackActivity.delta
        traktor_cpu_seconds_delta = if ($startTraktorCpu -ne $null -and $endTraktorCpu -ne $null) { [Math]::Round(($endTraktorCpu - $startTraktorCpu), 2) } else { $null }
        traktor_cpu_percent_one_core = if ($startTraktorCpu -ne $null -and $endTraktorCpu -ne $null) { [Math]::Round((($endTraktorCpu - $startTraktorCpu) / $elapsedSeconds) * 100.0, 2) } else { $null }
        top_cpu_processes = $topProcessAggregate
        top_cpu_process_names = (($topProcessAggregate | Select-Object -First 5 -ExpandProperty name) -join ",")
        system_cpu_source = $systemCpuSampler.source
        system_cpu_avg = if ($systemCpuValues.Count -gt 0) { [Math]::Round((($systemCpuValues | Measure-Object -Average).Average), 2) } else { $null }
        system_cpu_max = if ($systemCpuValues.Count -gt 0) { ($systemCpuValues | Measure-Object -Maximum).Maximum } else { $null }
        streaming_values = (($samples | Select-Object -ExpandProperty streaming -Unique) -join ",")
        channels_values = (($samples | Select-Object -ExpandProperty channels -Unique) -join ",")
        render_frames_delta = $endDiag.render_frames - $startDiag.render_frames
        capture_frames_delta = $endDiag.capture_frames - $startDiag.capture_frames
        underruns_delta = $endDiag.underruns - $startDiag.underruns
        overruns_delta = $endDiag.overruns - $startDiag.overruns
        packet_errors_delta = $endDiag.packet_errors - $startDiag.packet_errors
        late_completions_delta = $endDiag.late_completions - $startDiag.late_completions
        pair1_nonzero_delta = $endDiag.pair1 - $startDiag.pair1
        pair2_nonzero_delta = $endDiag.pair2 - $startDiag.pair2
        pair3_nonzero_delta = $endDiag.pair3 - $startDiag.pair3
        pair4_nonzero_delta = $endDiag.pair4 - $startDiag.pair4
        worker_iterations_delta = $endDiag.worker_iterations - $startDiag.worker_iterations
        worker_iterations_per_second = [Math]::Round(($endDiag.worker_iterations - $startDiag.worker_iterations) / $elapsedSeconds, 2)
        worker_capture_bytes_delta = $endDiag.worker_capture_bytes - $startDiag.worker_capture_bytes
        worker_playback_bytes_delta = $endDiag.worker_playback_bytes - $startDiag.worker_playback_bytes
        worker_no_render_delta = $endDiag.worker_no_render - $startDiag.worker_no_render
        worker_capture_bytes_per_second = [Math]::Round(($endDiag.worker_capture_bytes - $startDiag.worker_capture_bytes) / $elapsedSeconds, 2)
        worker_playback_bytes_per_second = [Math]::Round(($endDiag.worker_playback_bytes - $startDiag.worker_playback_bytes) / $elapsedSeconds, 2)
        worker_last_capture_bytes = $endDiag.worker_last_capture_bytes
        worker_last_playback_bytes = $endDiag.worker_last_playback_bytes
        worker_render_mask = $endDiag.worker_render_mask
        worker_capture_mask = $endDiag.worker_capture_mask
        worker_max_capture_bytes = $endDiag.worker_max_capture_bytes
        worker_max_playback_bytes = $endDiag.worker_max_playback_bytes
    }
    $summary.total_pair_nonzero_delta =
        $summary.pair1_nonzero_delta +
        $summary.pair2_nonzero_delta +
        $summary.pair3_nonzero_delta +
        $summary.pair4_nonzero_delta
    $summary.pass =
        ($summary.streaming_values -match "yes") -and
        ($summary.channels_values -match "8") -and
        ($summary.underruns_delta -eq 0) -and
        ($summary.overruns_delta -eq 0) -and
        ($summary.packet_errors_delta -eq 0) -and
        ($summary.late_completions_delta -eq 0) -and
        ($summary.playback_gesture_active) -and
        ($summary.total_pair_nonzero_delta -ge $MinTotalNonZeroDelta)

    $summary | ConvertTo-Json -Depth 8 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")
    if (-not $summary.pass) {
        throw "Traktor active smoke did not meet pass criteria; see $(Join-Path $OutDir 'summary.json')"
    }
}
finally {
    if ($startedTraktor -and $proc) {
        $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
        if ($p) {
            [void]$p.CloseMainWindow()
            Start-Sleep -Seconds 8
            $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
            if ($p) {
                Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            }
        }
    }

    Start-Sleep -Seconds 2
    & $ctl diagnostics | Set-Content -Path (Join-Path $OutDir "diagnostics-after-close.txt") -Encoding UTF8
    "skipped: Traktor smoke never arms or attempts iso-silence" |
        Set-Content -Path (Join-Path $OutDir "iso-silence-after.txt") -Encoding UTF8

    if ($monitorJob) {
        Wait-Job $monitorJob -Timeout 5 | Out-Null
        Stop-Job $monitorJob -ErrorAction SilentlyContinue
        Remove-Job $monitorJob -Force -ErrorAction SilentlyContinue
    }

    if (Test-Path $OutDir) {
        Write-PnpSnapshot -Path (Join-Path $OutDir "pnp-after.txt") -Label "after"
    }
    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -Path $lockPath -Force
    }
}
