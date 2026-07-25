param()

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$modulePath = Join-Path $repoRoot 'windows\scripts\OpenA8DJ.WindowsCommon.psm1'
$testRoot = Join-Path $env:TEMP ("OpenA8DJ\hardware-lock-test-{0}" -f [Guid]::NewGuid().ToString('N'))
$lockRoot = Join-Path $testRoot 'hardware-gate.lock'
$runRoot = Join-Path $testRoot 'run'
$oldLockRoot = $env:AUDIO_GATE_LOCK_ROOT

function Assert-Equal {
    param($Expected, $Actual, [string]$Message)
    if ($Expected -ne $Actual) {
        throw "$Message Expected '$Expected', got '$Actual'."
    }
}

function Set-LockOld {
    [IO.Directory]::SetLastWriteTime($lockRoot, (Get-Date).AddMinutes(-1))
}

try {
    $env:AUDIO_GATE_LOCK_ROOT = $lockRoot
    Import-Module $modulePath -Force

    Assert-Equal 'FREE' (Test-OpenA8DJHardwareLock).Status 'Missing lock directory must be free.'

    New-Item -ItemType Directory -Path $lockRoot -Force | Out-Null
    Assert-Equal 'BUSY' (Test-OpenA8DJHardwareLock).Status 'A fresh ownerless directory must preserve the mkdir/write race.'

    [IO.File]::WriteAllBytes((Join-Path $lockRoot 'owner'), [byte[]](0, 0, 0, 0))
    Set-LockOld
    Assert-Equal 'STALE' (Test-OpenA8DJHardwareLock).Status 'An old malformed owner must be reclaimable after a crash.'

    $lock = Acquire-OpenA8DJHardwareLock -Gate 'offline-lock-test' -RunDir $runRoot -TimeoutSeconds 0
    Assert-Equal 'BUSY' (Test-OpenA8DJHardwareLock).Status 'The current process must own the recovered lock.'
    $report = Get-Content (Join-Path $runRoot 'audio-gate-lock.txt') -Raw
    if ($report -notmatch 'recovered_stale_lock=1') {
        throw 'Recovered lock report did not preserve stale-lock evidence.'
    }
    Release-OpenA8DJHardwareLock -Lock $lock
    Assert-Equal 'FREE' (Test-OpenA8DJHardwareLock).Status 'Release must remove a lock owned by this process.'

    New-Item -ItemType Directory -Path $lockRoot -Force | Out-Null
    @(
        "pid=$PID",
        'gate=live-owner-test',
        "run_dir=$runRoot",
        "cwd=$repoRoot",
        "started_at=$((Get-Date).ToUniversalTime().ToString('o'))",
        'platform=windows-powershell'
    ) | Set-Content -LiteralPath (Join-Path $lockRoot 'owner') -Encoding ASCII
    Assert-Equal 'BUSY' (Test-OpenA8DJHardwareLock).Status 'A live PowerShell owner must never be reclaimed.'
    $busyRejected = $false
    try {
        $null = Acquire-OpenA8DJHardwareLock -Gate 'must-not-steal' -RunDir $runRoot -TimeoutSeconds 0
    } catch {
        $busyRejected = $_.Exception.Message -like 'Shared hardware lock is busy*'
    }
    if (-not $busyRejected) { throw 'Acquire did not reject a lock owned by a live process.' }

    @(
        'pid=2147483647',
        'gate=dead-owner-test',
        "run_dir=$runRoot",
        "cwd=$repoRoot",
        'started_at=2000-01-01T00:00:00.0000000Z',
        'platform=windows-powershell'
    ) | Set-Content -LiteralPath (Join-Path $lockRoot 'owner') -Encoding ASCII
    Assert-Equal 'STALE' (Test-OpenA8DJHardwareLock).Status 'A dead PowerShell owner must be reclaimable.'

    Write-Host 'PASS: OpenA8DJ Windows hardware lock lifecycle'
} finally {
    Remove-Module OpenA8DJ.WindowsCommon -Force -ErrorAction SilentlyContinue
    if ($null -eq $oldLockRoot) {
        Remove-Item Env:AUDIO_GATE_LOCK_ROOT -ErrorAction SilentlyContinue
    } else {
        $env:AUDIO_GATE_LOCK_ROOT = $oldLockRoot
    }
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}
