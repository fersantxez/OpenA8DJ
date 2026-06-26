param(
    [string]$CtlPath = "",
    [string]$OutDir = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
if ([string]::IsNullOrWhiteSpace($CtlPath)) {
    $CtlPath = Join-Path $repoRoot "windows\dist\Release\x64\opena8djctl.exe"
}
if (-not (Test-Path -LiteralPath $CtlPath)) {
    throw "opena8djctl.exe not found at $CtlPath"
}

function New-RunDirectory {
    param([string]$Requested)

    if ($Requested) {
        New-Item -ItemType Directory -Path $Requested -Force | Out-Null
        return (Resolve-Path $Requested).Path
    }

    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $path = Join-Path $repoRoot "local-analysis\windows-a8dj-control-readback-matrix-$stamp"
    New-Item -ItemType Directory -Path $path -Force | Out-Null
    return $path
}

function Parse-Diagnostics {
    param([string]$Text)

    $result = [ordered]@{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -match 'ctl-rdbk-mismatch:\s+(\S+)') {
            $result.Mismatch = $Matches[1]
        } elseif ($line -match 'ctl-raw:\s+(.+)$') {
            $result.Raw = $Matches[1].Trim()
        } elseif ($line -match 'ctl-write-req:\s+(.+)$') {
            $result.Request = $Matches[1].Trim()
        } elseif ($line -match 'ctl-write-rdbk:\s+(.+)$') {
            $result.Readback = $Matches[1].Trim()
        } elseif ($line -match 'ctl-write-status:\s+(\S+)') {
            $result.WriteStatus = $Matches[1]
        } elseif ($line -match 'ctl-rdbk-status:\s+(\S+)') {
            $result.ReadbackStatus = $Matches[1]
        } elseif ($line -match 'underruns:\s+(\d+)') {
            $result.Underruns = [int64]$Matches[1]
        } elseif ($line -match 'overruns:\s+(\d+)') {
            $result.Overruns = [int64]$Matches[1]
        } elseif ($line -match 'packet-errors:\s+(\d+)') {
            $result.PacketErrors = [int64]$Matches[1]
        } elseif ($line -match 'late-completions:\s+(\d+)') {
            $result.LateCompletions = [int64]$Matches[1]
        }
    }
    return [pscustomobject]$result
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

function Invoke-Ctl {
    param(
        [string[]]$Arguments,
        [string]$LogPrefix
    )

    $oldErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $text = & $CtlPath @Arguments 2>&1 | ForEach-Object { $_.ToString() } | Out-String
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    return [pscustomobject]@{
        Command = "opena8djctl $($Arguments -join ' ')"
        ExitCode = $exitCode
        Text = $text
        LogPrefix = $LogPrefix
    }
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-a8dj-control-readback-matrix.lock"
$lockTaken = $false
$rows = @()

try {
    if (Test-Path $lockPath) {
        $existing = Get-Content -Raw -Path $lockPath -ErrorAction SilentlyContinue
        throw "Another Audio 8 DJ control matrix may be active: $lockPath $existing"
    }
    @(
        "pid=$PID"
        "started=$(Get-Date -Format o)"
        "run_dir=$OutDir"
        "resource=Audio 8 DJ hardware controls"
    ) | Set-Content -Path $lockPath -Encoding UTF8
    $lockTaken = $true

    @(
        "safety_policy=control_only_no_audio_stream_no_usb_reset"
        "does_not_reset_usb=1"
        "does_not_disable_or_enable_pnp_devices=1"
        "always_restores_timecode_vinyl_48k_512=1"
        "always_attempts_iso_silence_after=1"
        "known_api24_local_result=ground_byte_3_reads_back_as_03_on_this_tablet"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    & $CtlPath profile timecode-vinyl | Set-Content -Path (Join-Path $OutDir "00-restore-before.txt") -Encoding UTF8
    & $CtlPath set-format 48000 512 | Add-Content -Path (Join-Path $OutDir "00-restore-before.txt") -Encoding UTF8
    & $CtlPath iso-silence | Set-Content -Path (Join-Path $OutDir "00-iso-silence-before.txt") -Encoding UTF8

    foreach ($mode in @("0", "1", "2")) {
        foreach ($lock in @("off", "on")) {
            foreach ($vinyl in @("off", "on")) {
                foreach ($cdLine in @("off", "on")) {
                    foreach ($phono in @("off", "on")) {
                        $caseName = "mode-$mode-lock-$lock-vinyl-$vinyl-cdline-$cdLine-phono-$phono"
                        $log = New-Object System.Collections.Generic.List[string]
                        $exitCodes = New-Object System.Collections.Generic.List[int]
                        foreach ($arguments in @(
                            @("input-mode", $mode),
                            @("software-lock", $lock),
                            @("gnd-vinyl", $vinyl),
                            @("gnd-cd-line", $cdLine),
                            @("gnd-phono", $phono)
                        )) {
                            $result = Invoke-Ctl -Arguments $arguments -LogPrefix $caseName
                            $exitCodes.Add($result.ExitCode)
                            $log.Add("> $($result.Command)")
                            $log.Add($result.Text)
                        }

                        $diagnostics = & $CtlPath diagnostics 2>&1 | Out-String
                        $log.Add("> opena8djctl diagnostics")
                        $log.Add($diagnostics)
                        $log | Set-Content -Path (Join-Path $OutDir "$caseName.txt") -Encoding UTF8

                        $parsed = Parse-Diagnostics $diagnostics
                        $requestGroundByte = Get-RawByte -RawText $parsed.Request -Index 3
                        $readbackGroundByte = Get-RawByte -RawText $parsed.Readback -Index 3
                        $expectedUnsupportedPhonoGround =
                            ($parsed.Mismatch -eq "yes" -and
                             $phono -eq "on" -and
                             $requestGroundByte -ne $null -and
                             $readbackGroundByte -ne $null -and
                             (($requestGroundByte -band 0x04) -ne 0) -and
                             (($readbackGroundByte -band 0x04) -eq 0))
                        $unexpectedMismatch =
                            ($parsed.Mismatch -eq "yes" -and -not $expectedUnsupportedPhonoGround)
                        $streamErrors =
                            (($parsed.Underruns + $parsed.Overruns + $parsed.PacketErrors + $parsed.LateCompletions) -ne 0)
                        $rows += [pscustomobject]@{
                            InputMode = $mode
                            SoftwareLock = $lock
                            GndVinyl = $vinyl
                            GndCdLine = $cdLine
                            GndPhono = $phono
                            ExitCodes = ($exitCodes -join ",")
                            Mismatch = $parsed.Mismatch
                            Raw = $parsed.Raw
                            Request = $parsed.Request
                            Readback = $parsed.Readback
                            WriteStatus = $parsed.WriteStatus
                            ReadbackStatus = $parsed.ReadbackStatus
                            ExpectedUnsupportedPhonoGround = $expectedUnsupportedPhonoGround
                            UnexpectedMismatch = $unexpectedMismatch
                            StreamErrors = $streamErrors
                        }

                        & $CtlPath profile timecode-vinyl | Out-Null
                    }
                }
            }
        }
    }

    $rows | Export-Csv -Path (Join-Path $OutDir "ground-readback-matrix.csv") -NoTypeInformation -Encoding UTF8
    $rows | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir "ground-readback-matrix.json") -Encoding UTF8

    $summary = [ordered]@{
        cases = $rows.Count
        matched = @($rows | Where-Object { $_.Mismatch -eq "no" }).Count
        mismatched = @($rows | Where-Object { $_.Mismatch -eq "yes" }).Count
        expected_unsupported_phono_ground_cases = @($rows | Where-Object { $_.ExpectedUnsupportedPhonoGround }).Count
        unexpected_mismatches = @($rows | Where-Object { $_.UnexpectedMismatch }).Count
        stream_error_cases = @($rows | Where-Object { $_.StreamErrors }).Count
        unique_raw_states = @($rows | Select-Object -ExpandProperty Raw -Unique)
        artifact_dir = $OutDir
    }
    $summary | ConvertTo-Json -Depth 4 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")
    if ($summary.stream_error_cases -ne 0) {
        throw "Control matrix changed stream error counters; see $OutDir"
    }
} finally {
    try {
        & $CtlPath profile timecode-vinyl | Set-Content -Path (Join-Path $OutDir "98-restore-after.txt") -Encoding UTF8
        & $CtlPath set-format 48000 512 | Add-Content -Path (Join-Path $OutDir "98-restore-after.txt") -Encoding UTF8
        & $CtlPath iso-silence | Set-Content -Path (Join-Path $OutDir "99-iso-silence-after.txt") -Encoding UTF8
    } catch {
        "restore failed: $($_.Exception.Message)" | Set-Content -Path (Join-Path $OutDir "restore-error.txt") -Encoding UTF8
    }
    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -LiteralPath $lockPath -Force
    }
}

Write-Host "Audio 8 DJ control readback matrix artifacts: $OutDir"
