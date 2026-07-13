param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$solution = Join-Path $repoRoot "windows\OpenA8DJVirtual.sln"
$driverDir = Join-Path $repoRoot "windows\driver"
$distDir = Join-Path $repoRoot "windows\dist\virtual\$Configuration\$Platform"
$infSource = Join-Path $driverDir "OpenA8DJVirtual.inf"
$infDest = Join-Path $distDir "OpenA8DJVirtual.inf"
$objDir = Join-Path $repoRoot "windows\obj\virtual\$Configuration\$Platform\OpenA8DJVirtual"

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

function Find-InfVerif {
    $toolsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Tools"
    $candidate = Get-ChildItem -Path $toolsRoot -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "infverif.exe" } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "infverif.exe was not found. Install the Windows SDK/WDK tools."
    }
    return $candidate.FullName
}

function Find-Inf2Cat {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    $candidate = Get-ChildItem -Path $kitsRoot -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq "Inf2Cat.exe" } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $candidate) {
        throw "Inf2Cat.exe was not found. Install the Windows Driver Kit."
    }
    return $candidate.FullName
}

$msbuild = Find-MsBuild
$infVerif = Find-InfVerif
$inf2cat = Find-Inf2Cat

foreach ($cleanPath in @($distDir, $objDir)) {
    if (Test-Path -LiteralPath $cleanPath) {
        Remove-Item -LiteralPath $cleanPath -Recurse -Force
    }
}

Write-Host "Building virtual ACX proof target $solution ($Configuration|$Platform)"
& $msbuild $solution /m /restore /p:Configuration=$Configuration /p:Platform=$Platform
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

New-Item -ItemType Directory -Path $distDir -Force | Out-Null
Copy-Item $infSource $infDest -Force
$sysPath = Join-Path $distDir "OpenA8DJVirtual.sys"
if (-not (Test-Path $sysPath)) {
    throw "Expected virtual driver output not found: $sysPath"
}

Write-Host "Validating virtual INF with InfVerif: $infDest"
& $infVerif /w /v $infDest
if ($LASTEXITCODE -ne 0) {
    throw "Virtual INF Windows-driver validation failed with exit code $LASTEXITCODE"
}
& $infVerif /h /v $infDest
if ($LASTEXITCODE -ne 0) {
    throw "Virtual INF hardware-signature validation failed with exit code $LASTEXITCODE"
}

Write-Host "Generating virtual catalog with Inf2Cat"
& $inf2cat /driver:$distDir /os:10_VB_X64,10_CO_X64,10_GE_X64 /verbose
if ($LASTEXITCODE -ne 0) {
    throw "Virtual Inf2Cat failed with exit code $LASTEXITCODE"
}

$catalogPath = Join-Path $distDir 'OpenA8DJVirtual.cat'
$sysSignature = Get-AuthenticodeSignature -LiteralPath $sysPath
if (-not $sysSignature.SignerCertificate) {
    throw 'The virtual SYS must be signed before its catalog can use the same test identity.'
}
$signTool = Find-OpenA8DJSignTool
Write-Host "Signing virtual catalog with SYS certificate $($sysSignature.SignerCertificate.Thumbprint)"
& $signTool sign /fd sha256 /sha1 $sysSignature.SignerCertificate.Thumbprint $catalogPath
if ($LASTEXITCODE -ne 0) {
    throw "Virtual catalog signing failed with exit code $LASTEXITCODE"
}
$certPath = Join-Path $distDir 'OpenA8DJVirtual-TestCertificate.cer'
[IO.File]::WriteAllBytes(
    $certPath,
    $sysSignature.SignerCertificate.Export(
        [Security.Cryptography.X509Certificates.X509ContentType]::Cert))

Write-Host "Virtual ACX proof package staged in $distDir"
