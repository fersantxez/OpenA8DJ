param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [switch]$SkipDependencyInstall,
    [switch]$SkipInstall
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$runDir = Join-Path $repoRoot "local-analysis\windows\build-install-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$logPath = Join-Path $runDir "build-install.log"

function Log {
    param([string]$Message)
    $line = "$(Get-Date -Format o) $Message"
    Write-Host $line
    Add-Content -Path $logPath -Value $line -Encoding UTF8
}

function Run-Logged {
    param(
        [string]$Label,
        [scriptblock]$Block
    )

    Log "STEP_START $Label"
    & $Block *>&1 | Tee-Object -FilePath $logPath -Append
    $exit = if ($LASTEXITCODE -ne $null) { $LASTEXITCODE } else { 0 }
    Log "STEP_END $Label exit=$exit"
    if ($exit -ne 0) {
        throw "$Label failed with exit code $exit"
    }
}

function Test-Admin {
    $principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Find-Inf2Cat {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $kitsRoot)) {
        return $null
    }
    Get-ChildItem -Path $kitsRoot -Recurse -Filter Inf2Cat.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

Log "run_dir=$runDir"
Log "repo_root=$repoRoot"
Log "is_admin=$(Test-Admin)"
if (-not (Test-Admin)) {
    throw "This script must run elevated."
}

Set-Location $repoRoot

@(
    "started=$(Get-Date -Format o)"
    "configuration=$Configuration"
    "platform=$Platform"
    "skip_dependency_install=$([bool]$SkipDependencyInstall)"
    "skip_install=$([bool]$SkipInstall)"
    "policy=build_existing_github_source_locally_because_actions_artifacts_not_published"
) | Set-Content -Path (Join-Path $runDir "manifest.txt") -Encoding UTF8

Get-PnpDevice -PresentOnly |
    Where-Object {
        $_.FriendlyName -match 'Audio 8 DJ|iRig Stream|IK Multimedia|Native Instruments' -or
        $_.InstanceId -match 'VID_17CC&PID_1978|VID_1963&PID_0059'
    } |
    Sort-Object FriendlyName |
    Select-Object Class,Status,FriendlyName,InstanceId |
    Format-Table -AutoSize |
    Out-String |
    Set-Content -Path (Join-Path $runDir "pnp-before.txt") -Encoding UTF8

if (-not $SkipDependencyInstall) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Run-Logged "install-visual-studio-build-tools" {
            winget install --id Microsoft.VisualStudio.2022.BuildTools --exact `
                --accept-source-agreements --accept-package-agreements --disable-interactivity `
                --override "--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows10SDK.19041 --includeRecommended"
        }
    } else {
        Log "Visual Studio installer already present: $vswhere"
    }

    if (-not (Find-Inf2Cat)) {
        Run-Logged "install-windows-driver-kit" {
            winget install --id Microsoft.WindowsWDK.10.0.19041 --exact `
                --accept-source-agreements --accept-package-agreements --disable-interactivity
        }
    } else {
        Log "Inf2Cat already present: $(Find-Inf2Cat)"
    }
}

Run-Logged "offline-tests" {
    $env:Path = "C:\Users\fersanchez\Documents\Codex\tools\w64devkit-2.8.0-extract\w64devkit\bin;$env:LOCALAPPDATA\Programs\Python\Python313;$env:LOCALAPPDATA\Programs\Python\Python313\Scripts;$env:Path"
    $env:CC = "C:\Users\fersanchez\Documents\Codex\tools\w64devkit-2.8.0-extract\w64devkit\bin\gcc.exe"
    & "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe" "windows\tests\run_offline_tests.py"
}

Run-Logged "build-installable-package" {
    & (Join-Path $repoRoot "windows\scripts\build-installable-package.ps1") -Configuration $Configuration -Platform $Platform
}

Run-Logged "verify-driver" {
    & (Join-Path $repoRoot "windows\scripts\verify-driver.ps1") -Configuration $Configuration -Platform $Platform
}

if (-not $SkipInstall) {
    $bootConfigBefore = (& bcdedit.exe /enum) -join "`n"
    $testSigningActive = $bootConfigBefore -match "testsigning\s+Yes"
    Log "testsigning_active_before=$testSigningActive"

    try {
        Run-Logged "install-driver" {
            & (Join-Path $repoRoot "windows\scripts\install-driver.ps1") `
                -Configuration $Configuration `
                -Platform $Platform `
                -SkipBuild `
                -TrustTestCertificate `
                -EnableTestSigning `
                -WaitLockSeconds 0
        }
    } catch {
        $message = $_.Exception.Message
        Log "install-driver-exception=$message"
        if ($message -match "Reboot Windows") {
            "REBOOT_REQUIRED_FOR_TESTSIGNING" | Set-Content -Path (Join-Path $runDir "reboot-required.txt") -Encoding UTF8
        } elseif ($message -match "Secure Boot|Code 52|0xC0000428") {
            $message | Set-Content -Path (Join-Path $runDir "secureboot-blocker.txt") -Encoding UTF8
            Log "INSTALL_BLOCKED_BY_SECURE_BOOT"
        } else {
            throw
        }
    }
}

Get-PnpDevice -PresentOnly |
    Where-Object {
        $_.FriendlyName -match 'Audio 8 DJ|iRig Stream|IK Multimedia|Native Instruments|OpenA8DJ' -or
        $_.InstanceId -match 'VID_17CC&PID_1978|VID_1963&PID_0059'
    } |
    Sort-Object FriendlyName |
    Select-Object Class,Status,FriendlyName,InstanceId |
    Format-Table -AutoSize |
    Out-String |
    Set-Content -Path (Join-Path $runDir "pnp-after.txt") -Encoding UTF8

Log "DONE run_dir=$runDir"
