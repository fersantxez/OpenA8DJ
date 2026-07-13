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
        if ($line -match 'api-version:\s+(\d+)') {
            $result.ApiVersion = [int]$Matches[1]
        } elseif ($line -match 'control-writes:\s+(\d+)') {
            $result.ControlWrites = [int64]$Matches[1]
        } elseif ($line -match 'ctl-read-status:\s+(\S+)') {
            $result.ReadStatus = $Matches[1]
        } elseif ($line -match 'ctl-rdbk-mismatch:\s+(\S+)') {
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

function Parse-ControlState {
    param([string]$Text)

    $result = [ordered]@{}
    foreach ($line in ($Text -split "`r?`n")) {
        if ($line -match 'input-mode:\s+(\d+)') {
            $result.InputMode = [int]$Matches[1]
        } elseif ($line -match 'gnd-vinyl:\s+(on|off)') {
            $result.GndVinyl = $Matches[1]
        } elseif ($line -match 'gnd-cd-line:\s+(on|off)') {
            $result.GndCdLine = $Matches[1]
        } elseif ($line -match 'gnd-phono:\s+(on|off)') {
            $result.GndPhono = $Matches[1]
        } elseif ($line -match 'software-lock:\s+(on|off)') {
            $result.SoftwareLock = $Matches[1]
        }
    }
    return [pscustomobject]$result
}

function Invoke-Ctl {
    param(
        [string[]]$Arguments,
        [string]$LogPrefix
    )

    $oldErrorActionPreference = $ErrorActionPreference
    $text = ""
    $exitCode = -1
    $ErrorActionPreference = "Continue"
    try {
        $text = & $CtlPath @Arguments 2>&1 | ForEach-Object { $_.ToString() } | Out-String
        $exitCode = $LASTEXITCODE
    } catch {
        $text = "command invocation failed: $($_.Exception.Message)"
        $exitCode = -1
    } finally {
        $ErrorActionPreference = $oldErrorActionPreference
    }
    $result = [pscustomobject]@{
        Command = "opena8djctl $($Arguments -join ' ')"
        ExitCode = $exitCode
        Failed = ($exitCode -ne 0)
        Text = $text
        LogPrefix = $LogPrefix
    }
    $script:commandResults.Add($result)
    return $result
}

function Write-CtlResult {
    param(
        [pscustomobject]$Result,
        [string]$Path,
        [switch]$Append
    )

    $lines = @(
        "> $($Result.Command)"
        "exit_code=$($Result.ExitCode)"
        $Result.Text
    )
    if ($Append) {
        $lines | Add-Content -Path $Path -Encoding UTF8
    } else {
        $lines | Set-Content -Path $Path -Encoding UTF8
    }
}

function Assert-CtlSucceeded {
    param([pscustomobject]$Result)

    if ($Result.ExitCode -ne 0) {
        throw "$($Result.Command) failed with exit code $($Result.ExitCode) during $($Result.LogPrefix)"
    }
}

function Invoke-SupportedWriteCase {
    param(
        [string]$Name,
        [string[]]$Arguments
    )

    $caseName = "supported-$Name"
    $casePath = Join-Path $OutDir "$caseName.txt"
    $writeResult = Invoke-Ctl -Arguments $Arguments -LogPrefix $caseName
    Write-CtlResult -Result $writeResult -Path $casePath
    Assert-CtlSucceeded $writeResult

    $diagnosticsResult = Invoke-Ctl -Arguments @("diagnostics") -LogPrefix $caseName
    Write-CtlResult -Result $diagnosticsResult -Path $casePath -Append
    Assert-CtlSucceeded $diagnosticsResult
    $diagnostics = Parse-Diagnostics $diagnosticsResult.Text
    $passed =
        ($diagnostics.ApiVersion -eq 34 -and
         $diagnostics.Mismatch -eq "no" -and
         $diagnostics.WriteStatus -eq "0x00000000")

    $script:rows += [pscustomobject]@{
        Kind = "supported"
        Name = $Name
        Command = $writeResult.Command
        RequestedValue = $Arguments[-1]
        ExpectedExitCode = 0
        ActualExitCode = $writeResult.ExitCode
        ApiVersion = $diagnostics.ApiVersion
        ControlWritesBefore = $null
        ControlWritesAfter = $diagnostics.ControlWrites
        WriteStatusBefore = $null
        WriteStatusAfter = $diagnostics.WriteStatus
        ReadbackStatusBefore = $null
        ReadbackStatusAfter = $diagnostics.ReadbackStatus
        MismatchBefore = $null
        MismatchAfter = $diagnostics.Mismatch
        RequestBefore = $null
        RequestAfter = $diagnostics.Request
        ReadbackBefore = $null
        ReadbackAfter = $diagnostics.Readback
        RawBefore = $null
        RawAfter = $diagnostics.Raw
        ReadStatusBefore = $null
        ReadStatusAfter = $diagnostics.ReadStatus
        Passed = $passed
    }
    if (-not $passed) {
        throw "$caseName violated API34 supported-write semantics"
    }
}

function Invoke-ExpectedUnsupportedGroundCase {
    param(
        [string]$Control,
        [string]$BaselineValue
    )

    $requestedValue = if ($BaselineValue -eq "on") { "off" } else { "on" }
    $caseName = "expected-unsupported-$Control"
    $casePath = Join-Path $OutDir "$caseName.txt"

    $beforeResult = Invoke-Ctl -Arguments @("diagnostics") -LogPrefix $caseName
    Write-CtlResult -Result $beforeResult -Path $casePath
    Assert-CtlSucceeded $beforeResult
    $before = Parse-Diagnostics $beforeResult.Text

    $writeResult = Invoke-Ctl -Arguments @($Control, $requestedValue) -LogPrefix $caseName
    Write-CtlResult -Result $writeResult -Path $casePath -Append
    $truthfulMessage =
        ($writeResult.Text -match 'Ground-lift controls are hardware readback-only on this Audio 8 DJ:\s*error=50')
    if ($writeResult.ExitCode -ne 1 -or -not $truthfulMessage) {
        throw "$caseName did not return exit 1 with error=50/readback-only"
    }

    $afterResult = Invoke-Ctl -Arguments @("diagnostics") -LogPrefix $caseName
    Write-CtlResult -Result $afterResult -Path $casePath -Append
    Assert-CtlSucceeded $afterResult
    $after = Parse-Diagnostics $afterResult.Text
    $writeHistoryPresent =
        ($null -ne $before.ControlWrites -and $null -ne $after.ControlWrites -and
         -not [string]::IsNullOrWhiteSpace($before.WriteStatus) -and
         -not [string]::IsNullOrWhiteSpace($after.WriteStatus) -and
         -not [string]::IsNullOrWhiteSpace($before.ReadbackStatus) -and
         -not [string]::IsNullOrWhiteSpace($after.ReadbackStatus) -and
         -not [string]::IsNullOrWhiteSpace($before.Mismatch) -and
         -not [string]::IsNullOrWhiteSpace($after.Mismatch) -and
         -not [string]::IsNullOrWhiteSpace($before.Request) -and
         -not [string]::IsNullOrWhiteSpace($after.Request) -and
         -not [string]::IsNullOrWhiteSpace($before.Readback) -and
         -not [string]::IsNullOrWhiteSpace($after.Readback))
    $passed =
        ($writeResult.ExitCode -eq 1 -and
         $truthfulMessage -and
         $before.ApiVersion -eq 34 -and
         $after.ApiVersion -eq 34 -and
         $writeHistoryPresent -and
         $after.ControlWrites -eq $before.ControlWrites -and
         $after.WriteStatus -eq $before.WriteStatus -and
         $after.ReadbackStatus -eq $before.ReadbackStatus -and
         $after.Mismatch -eq $before.Mismatch -and
         $after.Request -eq $before.Request -and
         $after.Readback -eq $before.Readback)

    $script:rows += [pscustomobject]@{
        Kind = "expected_unsupported"
        Name = $Control
        Command = $writeResult.Command
        RequestedValue = $requestedValue
        ExpectedExitCode = 1
        ActualExitCode = $writeResult.ExitCode
        ApiVersion = $after.ApiVersion
        ControlWritesBefore = $before.ControlWrites
        ControlWritesAfter = $after.ControlWrites
        WriteStatusBefore = $before.WriteStatus
        WriteStatusAfter = $after.WriteStatus
        ReadbackStatusBefore = $before.ReadbackStatus
        ReadbackStatusAfter = $after.ReadbackStatus
        MismatchBefore = $before.Mismatch
        MismatchAfter = $after.Mismatch
        RequestBefore = $before.Request
        RequestAfter = $after.Request
        ReadbackBefore = $before.Readback
        ReadbackAfter = $after.Readback
        RawBefore = $before.Raw
        RawAfter = $after.Raw
        ReadStatusBefore = $before.ReadStatus
        ReadStatusAfter = $after.ReadStatus
        Passed = $passed
    }
    if (-not $passed) {
        throw "$caseName violated API34 readback-only semantics"
    }
}

$OutDir = New-RunDirectory $OutDir
$lockDir = Join-Path $repoRoot "local-analysis"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null
$lockPath = Join-Path $lockDir "windows-a8dj-control-readback-matrix.lock"
$lockTaken = $false
$rows = @()
$commandResults = New-Object System.Collections.Generic.List[object]
$runFailure = $null

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
        "never_arms_or_attempts_iso_silence=1"
        "api_contract=34"
        "ground_controls=hardware_readback_only"
    ) | Set-Content -Path (Join-Path $OutDir "safety.txt") -Encoding UTF8

    $capsResult = Invoke-Ctl -Arguments @("caps") -LogPrefix "setup"
    Write-CtlResult -Result $capsResult -Path (Join-Path $OutDir "00-api-contract.txt")
    Assert-CtlSucceeded $capsResult
    if ($capsResult.Text -notmatch 'api-version:\s+37') {
        throw "Expected OpenA8DJ API 37 capabilities"
    }

    $beforeRestorePath = Join-Path $OutDir "00-restore-before.txt"
    $setupResult = Invoke-Ctl -Arguments @("profile", "timecode-vinyl") -LogPrefix "setup"
    Write-CtlResult -Result $setupResult -Path $beforeRestorePath
    Assert-CtlSucceeded $setupResult
    $setupResult = Invoke-Ctl -Arguments @("set-format", "48000", "512") -LogPrefix "setup"
    Write-CtlResult -Result $setupResult -Path $beforeRestorePath -Append
    Assert-CtlSucceeded $setupResult
    "skipped: control matrix never arms or attempts iso-silence" |
        Set-Content -Path (Join-Path $OutDir "00-iso-silence-before.txt") -Encoding UTF8

    foreach ($profile in @("timecode-vinyl", "timecode-cd-line", "phono", "unlock")) {
        Invoke-SupportedWriteCase -Name "profile-$profile" -Arguments @("profile", $profile)
    }
    foreach ($mode in @("0", "1", "2")) {
        Invoke-SupportedWriteCase -Name "input-mode-$mode" -Arguments @("input-mode", $mode)
    }
    foreach ($lock in @("off", "on")) {
        Invoke-SupportedWriteCase -Name "software-lock-$lock" -Arguments @("software-lock", $lock)
    }

    $baselineControlsResult = Invoke-Ctl -Arguments @("controls") -LogPrefix "ground-baseline"
    Write-CtlResult -Result $baselineControlsResult -Path (Join-Path $OutDir "ground-baseline.txt")
    Assert-CtlSucceeded $baselineControlsResult
    $baselineControls = Parse-ControlState $baselineControlsResult.Text
    $baselineDiagnosticsResult = Invoke-Ctl -Arguments @("diagnostics") -LogPrefix "ground-baseline"
    Write-CtlResult -Result $baselineDiagnosticsResult -Path (Join-Path $OutDir "ground-baseline.txt") -Append
    Assert-CtlSucceeded $baselineDiagnosticsResult
    $baselineDiagnostics = Parse-Diagnostics $baselineDiagnosticsResult.Text
    if ($baselineDiagnostics.ApiVersion -ne 34 -or
        [string]::IsNullOrWhiteSpace($baselineDiagnostics.Raw) -or
        $baselineControls.GndVinyl -notin @("on", "off") -or
        $baselineControls.GndCdLine -notin @("on", "off") -or
        $baselineControls.GndPhono -notin @("on", "off")) {
        throw "Could not establish API34 ground-control baseline"
    }

    Invoke-ExpectedUnsupportedGroundCase -Control "gnd-vinyl" -BaselineValue $baselineControls.GndVinyl
    Invoke-ExpectedUnsupportedGroundCase -Control "gnd-cd-line" -BaselineValue $baselineControls.GndCdLine
    Invoke-ExpectedUnsupportedGroundCase -Control "gnd-phono" -BaselineValue $baselineControls.GndPhono
} catch {
    $runFailure = $_
} finally {
    $afterRestorePath = Join-Path $OutDir "98-restore-after.txt"
    $finalRestoreCommands = @(
        [pscustomobject]@{ Arguments = @("profile", "timecode-vinyl"); Path = $afterRestorePath; Append = $false },
        [pscustomobject]@{ Arguments = @("set-format", "48000", "512"); Path = $afterRestorePath; Append = $true }
    )
    foreach ($restoreCommand in $finalRestoreCommands) {
        try {
            $restoreResult = Invoke-Ctl -Arguments $restoreCommand.Arguments -LogPrefix "finally-restore"
            Write-CtlResult -Result $restoreResult -Path $restoreCommand.Path -Append:$restoreCommand.Append
        } catch {
            $message = "restore failed: $($_.Exception.Message)"
            $message | Add-Content -Path (Join-Path $OutDir "restore-error.txt") -Encoding UTF8
            $commandResults.Add([pscustomobject]@{
                Command = "opena8djctl $($restoreCommand.Arguments -join ' ')"
                ExitCode = -1
                Failed = $true
                Text = $message
                LogPrefix = "finally-restore"
            })
        }
    }
    "skipped: control matrix never arms or attempts iso-silence" |
        Set-Content -Path (Join-Path $OutDir "99-iso-silence-after.txt") -Encoding UTF8
    if ($lockTaken -and (Test-Path $lockPath)) {
        Remove-Item -LiteralPath $lockPath -Force
    }
}

$rows | Export-Csv -Path (Join-Path $OutDir "ground-readback-matrix.csv") -NoTypeInformation -Encoding UTF8
$rows | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir "ground-readback-matrix.json") -Encoding UTF8
$commandResults | Export-Csv -Path (Join-Path $OutDir "command-results.csv") -NoTypeInformation -Encoding UTF8
$commandResults | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir "command-results.json") -Encoding UTF8
$failedCommands = @($commandResults | Where-Object { $_.ExitCode -ne 0 })
$expectedUnsupportedExits = @($failedCommands | Where-Object {
    $_.LogPrefix -like "expected-unsupported-*" -and $_.ExitCode -eq 1
})
$unexpectedCommandFailures = @($failedCommands | Where-Object {
    -not ($_.LogPrefix -like "expected-unsupported-*" -and $_.ExitCode -eq 1)
})
$summary = [ordered]@{
    api_contract = 34
    aborted = ($null -ne $runFailure)
    abort_reason = if ($null -ne $runFailure) { $runFailure.Exception.Message } else { $null }
    cases = $rows.Count
    passed = @($rows | Where-Object { $_.Passed }).Count
    failed = @($rows | Where-Object { -not $_.Passed }).Count
    supported_cases = @($rows | Where-Object { $_.Kind -eq "supported" }).Count
    supported_passed = @($rows | Where-Object { $_.Kind -eq "supported" -and $_.Passed }).Count
    expected_unsupported_cases = @($rows | Where-Object { $_.Kind -eq "expected_unsupported" }).Count
    expected_unsupported_passed = @($rows | Where-Object { $_.Kind -eq "expected_unsupported" -and $_.Passed }).Count
    baseline_raw = $baselineDiagnostics.Raw
    command_attempts = $commandResults.Count
    nonzero_command_exits = $failedCommands.Count
    expected_unsupported_exit_1 = $expectedUnsupportedExits.Count
    unexpected_command_failures = $unexpectedCommandFailures.Count
    setup_command_failures = @($failedCommands | Where-Object { $_.LogPrefix -eq "setup" }).Count
    restore_command_failures = @($failedCommands | Where-Object { $_.LogPrefix -eq "finally-restore" }).Count
    nonzero_command_records = @($failedCommands | Select-Object Command, ExitCode, LogPrefix)
    unique_raw_states = @($rows | Select-Object -ExpandProperty RawAfter -Unique)
    artifact_dir = $OutDir
}
$summary | ConvertTo-Json -Depth 6 | Tee-Object -FilePath (Join-Path $OutDir "summary.json")

if ($null -ne $runFailure) {
    throw $runFailure
}
if ($summary.unexpected_command_failures -ne 0) {
    throw "API34 control validation recorded $($summary.unexpected_command_failures) unexpected command failure(s); see $OutDir"
}
if ($summary.failed -ne 0 -or
    $summary.supported_cases -ne 9 -or
    $summary.expected_unsupported_cases -ne 3) {
    throw "API34 control semantics validation was incomplete or failed; see $OutDir"
}

Write-Host "Audio 8 DJ control readback matrix artifacts: $OutDir"
