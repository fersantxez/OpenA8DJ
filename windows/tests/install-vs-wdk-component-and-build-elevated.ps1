param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$runDir = Join-Path $repoRoot "local-analysis\windows\vs-wdk-component-$(Get-Date -Format yyyyMMdd-HHmmss)"
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$logPath = Join-Path $runDir "vs-wdk-component.log"

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

Log "run_dir=$runDir"
Log "repo_root=$repoRoot"
Log "is_admin=$(Test-Admin)"
if (-not (Test-Admin)) {
    throw "This script must run elevated."
}

Set-Location $repoRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found."
}

$installPath = (& $vswhere -products Microsoft.VisualStudio.Product.BuildTools -version "[17.0,18.0)" -property installationPath) | Select-Object -First 1
if (-not $installPath) {
    throw "Visual Studio 2022 Build Tools installation was not found."
}

$vsInstaller = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\setup.exe"
if (-not (Test-Path -LiteralPath $vsInstaller)) {
    throw "Visual Studio setup.exe was not found."
}

$toolsetPath = Get-KernelToolsetPath
Log "install_path=$installPath"
Log "toolset_path=$toolsetPath"
Log "toolset_exists_before=$(Test-Path -LiteralPath $toolsetPath)"

if (-not (Test-Path -LiteralPath $toolsetPath)) {
    Run-Logged "install-vs-component-windows-driver-kit" {
        $args = @(
            "modify",
            "--installPath", $installPath,
            "--add", "Component.Microsoft.Windows.DriverKit.BuildTools",
            "--add", "Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64.Spectre",
            "--passive",
            "--norestart"
        )
        Write-Host "vs_setup_command=$vsInstaller $($args -join ' ')"
        & $vsInstaller @args
        $exit = if ($LASTEXITCODE -ne $null) { $LASTEXITCODE } else { 0 }
        if ($exit -ne 0) {
            throw "Visual Studio setup failed with exit code $exit"
        }
    }
}

Log "toolset_exists_after_component=$(Test-Path -LiteralPath $toolsetPath)"
if (-not (Test-Path -LiteralPath $toolsetPath)) {
    throw "WindowsKernelModeDriver10.0 toolset is still missing after adding Component.Microsoft.Windows.DriverKit."
}

Run-Logged "build-and-install-opena8dj" {
    & (Join-Path $repoRoot "windows\tests\build-and-install-opena8dj-elevated.ps1") `
        -Configuration $Configuration `
        -Platform $Platform `
        -SkipDependencyInstall
}

Log "DONE run_dir=$runDir"
