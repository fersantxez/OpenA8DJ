param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$runDir = Join-Path $repoRoot "local-analysis\windows\wdk-vs2022-fix-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$logPath = Join-Path $runDir "wdk-vs2022-fix.log"

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
    $global:LASTEXITCODE = 0
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

function Get-KernelToolsetPath {
    Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\MSBuild\Microsoft\VC\v170\Platforms\$Platform\PlatformToolsets\WindowsKernelModeDriver10.0"
}

function Find-VsixInstaller {
    $candidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\Common7\IDE\VSIXInstaller.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\resources\app\ServiceHub\Services\Microsoft.VisualStudio.Setup.Service\VsixInstaller\VSIXInstaller.exe")
    )
    $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

function Find-WdkVsix2022 {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10"
    if (-not (Test-Path -LiteralPath $kitsRoot)) {
        return $null
    }

    Get-ChildItem -LiteralPath $kitsRoot -Recurse -Filter "WDK.vsix" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match "\\Vsix\\VS2022\\" } |
        Select-Object -First 1 -ExpandProperty FullName
}

Log "run_dir=$runDir"
Log "repo_root=$repoRoot"
Log "is_admin=$(Test-Admin)"
if (-not (Test-Admin)) {
    throw "This script must run elevated."
}

Set-Location $repoRoot

$toolsetPath = Get-KernelToolsetPath
Log "toolset_path=$toolsetPath"
Log "toolset_exists_before=$(Test-Path -LiteralPath $toolsetPath)"

Get-ChildItem -LiteralPath (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10") -Recurse -Filter "WDK.vsix" -ErrorAction SilentlyContinue |
    Select-Object FullName,Length |
    Format-List |
    Out-String |
    Set-Content -Path (Join-Path $runDir "wdk-vsix-before.txt") -Encoding UTF8

if (-not (Test-Path -LiteralPath $toolsetPath)) {
    Run-Logged "install-wdk-26100" {
        winget install --id Microsoft.WindowsWDK.10.0.26100 --exact `
            --accept-source-agreements --accept-package-agreements --disable-interactivity
    }
}

$wdkVsix2022 = Find-WdkVsix2022
Log "wdk_vsix_2022=$wdkVsix2022"
if ((-not (Test-Path -LiteralPath $toolsetPath)) -and $wdkVsix2022) {
    $vsixInstaller = Find-VsixInstaller
    if (-not $vsixInstaller) {
        throw "VSIXInstaller.exe was not found."
    }

    Log "vsix_installer=$vsixInstaller"
    Run-Logged "install-wdk-vsix-vs2022" {
        & $vsixInstaller /quiet /admin $wdkVsix2022
    }
}

Log "toolset_exists_after=$(Test-Path -LiteralPath $toolsetPath)"
Get-ChildItem -LiteralPath (Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10") -Recurse -Filter "WDK.vsix" -ErrorAction SilentlyContinue |
    Select-Object FullName,Length |
    Format-List |
    Out-String |
    Set-Content -Path (Join-Path $runDir "wdk-vsix-after.txt") -Encoding UTF8

if (-not (Test-Path -LiteralPath $toolsetPath)) {
    throw "WindowsKernelModeDriver10.0 toolset is still missing after WDK 26100 install."
}

Run-Logged "build-and-install-opena8dj" {
    & (Join-Path $repoRoot "windows\tests\build-and-install-opena8dj-elevated.ps1") `
        -Configuration $Configuration `
        -Platform $Platform `
        -SkipDependencyInstall
}

Log "DONE run_dir=$runDir"
