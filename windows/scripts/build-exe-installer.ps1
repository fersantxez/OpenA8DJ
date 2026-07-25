param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\.." )).Path
$installerDir = Join-Path $repoRoot "windows\dist\installer"
$stagingName = "OpenA8DJUsb-$Configuration-$Platform"
$stagingRoot = Join-Path $installerDir $stagingName
$zipPath = Join-Path $installerDir "$stagingName-installer.zip"
$exePath = Join-Path $installerDir "$stagingName-installer.exe"
$workRoot = Join-Path $installerDir "_exe-build-$Platform"
$sourcePath = Join-Path $repoRoot "windows\installer\OpenA8DJBootstrapper.cs"
$cscCandidates = @(
    (Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"),
    (Join-Path $env:WINDIR "Microsoft.NET\Framework\v4.0.30319\csc.exe")
)
$csc = $cscCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1

if (-not (Test-Path -LiteralPath $stagingRoot)) {
    throw "Installer staging directory not found: $stagingRoot. Run package-installer.ps1 first."
}
if (-not (Test-Path -LiteralPath $zipPath)) {
    throw "Installer ZIP not found: $zipPath. Run package-installer.ps1 first."
}
if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Bootstrapper source not found: $sourcePath"
}
if (-not $csc) {
    throw "The .NET Framework C# compiler (csc.exe) was not found."
}

if (Test-Path -LiteralPath $workRoot) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $workRoot -Force | Out-Null
Copy-Item -LiteralPath $zipPath -Destination (Join-Path $workRoot (Split-Path -Leaf $zipPath)) -Force

if (Test-Path -LiteralPath $exePath) {
    Remove-Item -LiteralPath $exePath -Force
}

$embeddedZip = Join-Path $workRoot (Split-Path -Leaf $zipPath)
$resourceName = Split-Path -Leaf $zipPath
$arguments = @(
    "/nologo",
    "/target:winexe",
    "/out:$exePath",
    "/reference:System.Windows.Forms.dll",
    "/reference:System.IO.Compression.FileSystem.dll",
    "/resource:$embeddedZip,$resourceName",
    $sourcePath
)
& $csc @arguments
if ($LASTEXITCODE -ne 0) {
    throw "C# bootstrapper compilation failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Bootstrapper compilation completed without creating $exePath"
}

$hash = (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash.ToLowerInvariant()
$metadata = [ordered]@{
    installer = Split-Path -Leaf $exePath
    format = ".NET Framework self-extracting EXE bootstrapper"
    configuration = $Configuration
    platform = $Platform
    payload_zip = $resourceName
    sha256 = $hash
    bytes = (Get-Item -LiteralPath $exePath).Length
    signed = $false
    signing_note = "Development package; not Microsoft-signed and not code-signed."
    install_mode = "stage-only by default; physical binding requires explicit installer script switches"
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $installerDir "$stagingName-installer.exe.json") -Encoding ASCII

Write-Host "Installer EXE: $exePath"
Write-Host "SHA256: $hash"
