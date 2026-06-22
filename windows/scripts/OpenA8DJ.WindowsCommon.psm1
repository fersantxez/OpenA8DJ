Set-StrictMode -Version 3.0

function Get-OpenA8DJRepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

function Get-OpenA8DJLockRoot {
    if ($env:AUDIO_GATE_LOCK_ROOT) {
        return $env:AUDIO_GATE_LOCK_ROOT
    }
    return (Join-Path $HOME ".opena8dj\hardware-gate.lock")
}

function Test-OpenA8DJAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-OpenA8DJAdministrator {
    if (-not (Test-OpenA8DJAdministrator)) {
        throw "Run this script from an elevated Administrator PowerShell."
    }
}

function Test-OpenA8DJProcessAlive {
    param([Parameter(Mandatory = $true)][int]$ProcessId)

    try {
        $null = Get-Process -Id $ProcessId -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Read-OpenA8DJLockOwner {
    param([Parameter(Mandatory = $true)][string]$LockRoot)

    $ownerPath = Join-Path $LockRoot "owner"
    if (-not (Test-Path $ownerPath)) {
        return $null
    }
    $owner = [ordered]@{}
    foreach ($line in Get-Content -Path $ownerPath -ErrorAction Stop) {
        if ($line -match "^([^=]+)=(.*)$") {
            $owner[$matches[1]] = $matches[2]
        }
    }
    return [pscustomobject]$owner
}

function Test-OpenA8DJHardwareLock {
    $lockRoot = Get-OpenA8DJLockRoot
    if (-not (Test-Path $lockRoot)) {
        [pscustomobject]@{
            Status = "FREE"
            LockRoot = $lockRoot
            Owner = $null
        }
        return
    }

    $owner = Read-OpenA8DJLockOwner -LockRoot $lockRoot
    if (-not $owner) {
        [pscustomobject]@{
            Status = "BUSY"
            LockRoot = $lockRoot
            Owner = $null
        }
        return
    }

    $pidValue = 0
    $pidAlive = $false
    $canReclaim = $owner.PSObject.Properties.Name -contains "platform" -and $owner.platform -eq "windows-powershell"
    if ($canReclaim -and $owner.PSObject.Properties.Name -contains "pid" -and [int]::TryParse([string]$owner.pid, [ref]$pidValue)) {
        $pidAlive = Test-OpenA8DJProcessAlive -ProcessId $pidValue
    }

    [pscustomobject]@{
        Status = if ($canReclaim -and -not $pidAlive) { "STALE" } else { "BUSY" }
        LockRoot = $lockRoot
        Owner = $owner
    }
}

function Acquire-OpenA8DJHardwareLock {
    param(
        [Parameter(Mandatory = $true)][string]$Gate,
        [Parameter(Mandatory = $true)][string]$RunDir,
        [int]$TimeoutSeconds = 0
    )

    $lockRoot = Get-OpenA8DJLockRoot
    $lockParent = Split-Path -Parent $lockRoot
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    New-Item -ItemType Directory -Path $lockParent -Force | Out-Null
    New-Item -ItemType Directory -Path $RunDir -Force | Out-Null

    do {
        if ($env:AUDIO_GATE_LOCK_HELD -eq "1") {
            $lockReport = @(
                "audio_gate_lock=INHERITED",
                "lock_dir=$lockRoot",
                "gate=$Gate",
                "owner_gate=$($env:AUDIO_GATE_LOCK_OWNER_GATE)",
                "owner_run_dir=$($env:AUDIO_GATE_LOCK_OWNER_RUN_DIR)",
                "owner_cwd=$($env:AUDIO_GATE_LOCK_OWNER_CWD)"
            )
            Set-Content -Path (Join-Path $RunDir "audio-gate-lock.txt") -Value $lockReport -Encoding ASCII
            return [pscustomobject]@{
                LockRoot = $lockRoot
                OwnerPath = $null
                Gate = $Gate
                RunDir = $RunDir
                Inherited = $true
            }
        }

        $status = Test-OpenA8DJHardwareLock
        if ($status.Status -eq "STALE") {
            Remove-Item -Path $lockRoot -Recurse -Force
        }

        if ($status.Status -eq "FREE" -or $status.Status -eq "STALE") {
            try {
                New-Item -ItemType Directory -Path $lockRoot -ErrorAction Stop | Out-Null
            } catch {
                Start-Sleep -Milliseconds 250
                continue
            }

            $ownerPath = Join-Path $lockRoot "owner"
            $ownerText = @(
                "pid=$PID",
                "gate=$Gate",
                "run_dir=$RunDir",
                "cwd=$((Get-Location).Path)",
                "started_at=$((Get-Date).ToUniversalTime().ToString("o"))",
                "platform=windows-powershell"
            )
            Set-Content -Path $ownerPath -Value $ownerText -Encoding ASCII
            $lockReport = @(
                "audio_gate_lock=ACQUIRED",
                "lock_dir=$lockRoot",
                "gate=$Gate"
            )
            if ($status.Status -eq "STALE") {
                $lockReport += "recovered_stale_lock=1"
            }
            Set-Content -Path (Join-Path $RunDir "audio-gate-lock.txt") -Value $lockReport -Encoding ASCII
            return [pscustomobject]@{
                LockRoot = $lockRoot
                OwnerPath = $ownerPath
                Gate = $Gate
                RunDir = $RunDir
            }
        }

        if ($TimeoutSeconds -le 0 -or (Get-Date) -ge $deadline) {
            throw "Shared hardware lock is busy. Owner: $($status.Owner | ConvertTo-Json -Compress)"
        }

        Start-Sleep -Seconds 2
    } while ($true)
}

function Release-OpenA8DJHardwareLock {
    param([Parameter(Mandatory = $true)]$Lock)

    if ($Lock.PSObject.Properties.Name -contains "Inherited" -and $Lock.Inherited) {
        return
    }
    if (-not $Lock.OwnerPath) {
        return
    }
    if (Test-Path $Lock.OwnerPath) {
        $owner = Read-OpenA8DJLockOwner -LockRoot $Lock.LockRoot
        if ($owner -and ($owner.PSObject.Properties.Name -contains "pid") -and ([string]$owner.pid -eq [string]$PID)) {
            Remove-Item -Path $Lock.LockRoot -Recurse -Force
        }
    }
}

function Find-OpenA8DJSignTool {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $kitsRoot)) {
        throw "Windows Kits bin directory was not found. Install the Windows SDK and WDK."
    }

    $candidate = Get-ChildItem -Path $kitsRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object {
            Join-Path $_.FullName "x64\signtool.exe"
            Join-Path $_.FullName "x86\signtool.exe"
        } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $candidate) {
        throw "signtool.exe was not found. Install the Windows SDK and WDK."
    }

    return $candidate
}

function Get-OpenA8DJFileHashHex {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}
