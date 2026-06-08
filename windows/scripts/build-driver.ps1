param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",

    [switch]$SkipInf2Cat
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$solution = Join-Path $repoRoot "windows\OpenA8DJWindows.sln"
$driverDir = Join-Path $repoRoot "windows\driver"
$distDir = Join-Path $repoRoot "windows\dist\$Configuration\$Platform"
$infSource = Join-Path $driverDir "OpenA8DJUsb.inf"
$infDest = Join-Path $distDir "OpenA8DJUsb.inf"

function Find-MsBuild {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\Current\Bin\amd64\MSBuild.exe" | Select-Object -First 1
        if ($path) {
            return $path
        }
    }

    $fallback = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    if (Test-Path $fallback) {
        return $fallback
    }

    throw "MSBuild was not found. Install Visual Studio 2022 Build Tools with the Windows Driver Kit."
}

function Find-Inf2Cat {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $kitsRoot)) {
        throw "Windows Kits bin directory was not found. Install the Windows SDK and WDK."
    }

    $candidate = Get-ChildItem -Path $kitsRoot -Directory |
        Sort-Object Name -Descending |
        ForEach-Object {
            Join-Path $_.FullName "x64\Inf2Cat.exe"
            Join-Path $_.FullName "x86\Inf2Cat.exe"
        } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $candidate) {
        throw "Inf2Cat.exe was not found. Install the Windows Driver Kit."
    }

    return $candidate
}

$msbuild = Find-MsBuild

Write-Host "Building $solution ($Configuration|$Platform)"
& $msbuild $solution /m /restore /p:Configuration=$Configuration /p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
Copy-Item $infSource $infDest -Force
Get-ChildItem -Path $distDir -Filter "*.ilk" -ErrorAction SilentlyContinue | Remove-Item -Force

$sysPath = Join-Path $distDir "OpenA8DJUsb.sys"
if (-not (Test-Path $sysPath)) {
    throw "Expected driver output not found: $sysPath"
}

if (-not $SkipInf2Cat) {
    $inf2cat = Find-Inf2Cat
    $osTargets = if ($Platform -eq "ARM64") {
        "10_VB_ARM64,10_CO_ARM64,10_GE_ARM64"
    } else {
        "10_VB_X64,10_CO_X64,10_GE_X64"
    }

    Write-Host "Generating catalog with Inf2Cat for $osTargets"
    & $inf2cat /driver:$distDir /os:$osTargets /verbose
    if ($LASTEXITCODE -ne 0) {
        throw "Inf2Cat failed with exit code $LASTEXITCODE"
    }
}

Write-Host "Windows driver package staged in $distDir"
