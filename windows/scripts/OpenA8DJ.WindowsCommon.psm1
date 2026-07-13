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
    if ($owner.Count -eq 0) {
        return $null
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
        # A crash can leave the directory and a zero-filled owner file behind.
        # Do not reclaim a just-created directory because another process may be
        # between mkdir and writing its owner record.
        $lockAge = (Get-Date) - (Get-Item -LiteralPath $lockRoot -ErrorAction Stop).LastWriteTime
        [pscustomobject]@{
            Status = if ($lockAge.TotalSeconds -ge 5) { "STALE" } else { "BUSY" }
            LockRoot = $lockRoot
            Owner = $null
        }
        return
    }

    $pidValue = 0
    $pidAlive = $false
    $ownerPropertyNames = @($owner.PSObject.Properties | ForEach-Object Name)
    $canReclaim = $ownerPropertyNames -contains "platform" -and $owner.platform -eq "windows-powershell"
    if ($canReclaim -and $ownerPropertyNames -contains "pid" -and [int]::TryParse([string]$owner.pid, [ref]$pidValue)) {
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

function Get-OpenA8DJDriverVersion {
    param([Parameter(Mandatory = $true)][string]$InfPath)

    foreach ($line in Get-Content -LiteralPath $InfPath) {
        if ($line -match '^DriverVer\s*=\s*([^,]+),(.+)$') {
            return $Matches[2].Trim()
        }
    }
    throw "DriverVer not found in $InfPath"
}

function Get-OpenA8DJPeTimestampUtc {
    param([Parameter(Mandatory = $true)][string]$Path)

    $bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path).Path)
    if ($bytes.Length -lt 256 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
        throw "Not a PE image: $Path"
    }
    $peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
    if ($peOffset -lt 0 -or $peOffset + 12 -gt $bytes.Length) {
        throw "Invalid PE header offset: $Path"
    }
    $timestamp = [BitConverter]::ToUInt32($bytes, $peOffset + 8)
    return [DateTimeOffset]::FromUnixTimeSeconds($timestamp).UtcDateTime.ToString('o')
}

function Get-OpenA8DJBuildFingerprint {
    param([Parameter(Mandatory = $true)][string]$HeaderPath)

    $text = Get-Content -LiteralPath $HeaderPath -Raw
    if ($text -notmatch '#define\s+OPENA8DJ_BUILD_FINGERPRINT\s+"([0-9a-fA-F]+)"') {
        throw "Build fingerprint not found in $HeaderPath"
    }
    return $Matches[1].ToLowerInvariant()
}

function New-OpenA8DJPackageManifest {
    param(
        [Parameter(Mandatory = $true)][string]$PackageDir,
        [Parameter(Mandatory = $true)][string]$Configuration,
        [Parameter(Mandatory = $true)][string]$Platform,
        [Parameter(Mandatory = $true)][string]$BuildFingerprint
    )

    $PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
    $infPath = Join-Path $PackageDir 'OpenA8DJUsb.inf'
    $sysPath = Join-Path $PackageDir 'OpenA8DJUsb.sys'
    foreach ($path in @($infPath, $sysPath)) {
        if (-not (Test-Path -LiteralPath $path)) { throw "Package payload missing: $path" }
    }

    $names = @(
        'OpenA8DJUsb.inf',
        'OpenA8DJUsb.sys',
        'OpenA8DJUsb.pdb',
        'OpenA8DJUsb.cat',
        'OpenA8DJUsb.cer',
        'OpenA8DJUsb-TestCertificate.cer',
        'opena8djctl.exe'
    )
    $files = @()
    foreach ($name in $names) {
        $path = Join-Path $PackageDir $name
        if (Test-Path -LiteralPath $path) {
            $item = Get-Item -LiteralPath $path
            $files += [ordered]@{
                name = $name
                bytes = [long]$item.Length
                sha256 = Get-OpenA8DJFileHashHex -Path $path
            }
        }
    }
    $manifest = [ordered]@{
        schema = 'opena8dj-driver-package-v1'
        generated_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        configuration = $Configuration
        platform = $Platform
        driver_version = Get-OpenA8DJDriverVersion -InfPath $infPath
        build_fingerprint = $BuildFingerprint.ToLowerInvariant()
        sys_pe_timestamp_utc = Get-OpenA8DJPeTimestampUtc -Path $sysPath
        files = $files
    }
    $manifestPath = Join-Path $PackageDir 'package-manifest.json'
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding ASCII
    return $manifestPath
}

function Test-OpenA8DJPackageManifest {
    param([Parameter(Mandatory = $true)][string]$PackageDir)

    $PackageDir = (Resolve-Path -LiteralPath $PackageDir).Path
    $manifestPath = Join-Path $PackageDir 'package-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath)) {
        throw "Mandatory package manifest missing: $manifestPath"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 'opena8dj-driver-package-v1') {
        throw "Unsupported package manifest schema: $($manifest.schema)"
    }
    foreach ($file in @($manifest.files)) {
        $path = Join-Path $PackageDir ([string]$file.name)
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Manifest payload missing: $path"
        }
        $item = Get-Item -LiteralPath $path
        $actualHash = Get-OpenA8DJFileHashHex -Path $path
        if ($item.Length -ne [long]$file.bytes -or $actualHash -ne ([string]$file.sha256).ToLowerInvariant()) {
            throw "Manifest mismatch for $($file.name): expected $($file.sha256)/$($file.bytes), actual $actualHash/$($item.Length)"
        }
    }
    foreach ($required in @(
        'OpenA8DJUsb.inf',
        'OpenA8DJUsb.sys',
        'OpenA8DJUsb.cat',
        'OpenA8DJUsb-TestCertificate.cer'
    )) {
        if (-not (@($manifest.files.name) -contains $required)) {
            throw "Manifest does not identify required payload: $required"
        }
    }
    return $manifest
}

function Get-OpenA8DJDriverStoreMatches {
    param([Parameter(Mandatory = $true)][string]$PackageDir)

    $manifest = Test-OpenA8DJPackageManifest -PackageDir $PackageDir
    $expectedSys = [string](@($manifest.files | Where-Object name -eq 'OpenA8DJUsb.sys')[0].sha256)
    $expectedInf = [string](@($manifest.files | Where-Object name -eq 'OpenA8DJUsb.inf')[0].sha256)
    $root = Join-Path $env:windir 'System32\DriverStore\FileRepository'
    $matches = @()
    foreach ($dir in @(Get-ChildItem -LiteralPath $root -Directory -Filter 'opena8djusb.inf_*' -ErrorAction SilentlyContinue)) {
        $sys = Join-Path $dir.FullName 'OpenA8DJUsb.sys'
        $inf = Join-Path $dir.FullName 'OpenA8DJUsb.inf'
        if (-not (Test-Path -LiteralPath $sys) -or -not (Test-Path -LiteralPath $inf)) { continue }
        $sysHash = Get-OpenA8DJFileHashHex -Path $sys
        $infHash = Get-OpenA8DJFileHashHex -Path $inf
        $matches += [pscustomobject]@{
            Path = $dir.FullName
            SysPath = $sys
            InfPath = $inf
            SysSha256 = $sysHash
            InfSha256 = $infHash
            Exact = ($sysHash -eq $expectedSys -and $infHash -eq $expectedInf)
        }
    }
    return @($matches)
}
