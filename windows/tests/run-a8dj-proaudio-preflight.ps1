param(
    [string]$OutDir = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$MsBuild = "",
    [switch]$OfflineOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$projectRelative = "windows\proaudio\OpenA8DJProAudioProbe.vcxproj"
$project = Join-Path $repoRoot $projectRelative
$sourceInputs = @(
    "windows\proaudio\OpenA8DJProAudioBackend.h",
    "windows\proaudio\OpenA8DJEndpointSelection.cpp",
    "windows\proaudio\OpenA8DJWasapiProbe.cpp",
    "windows\tests\proaudio_endpoint_selection_test.cpp",
    $projectRelative,
    "windows\tests\run-a8dj-proaudio-preflight.ps1"
)
$compiledCppInputs = @(
    "windows\proaudio\OpenA8DJProAudioBackend.h",
    "windows\proaudio\OpenA8DJEndpointSelection.cpp",
    "windows\proaudio\OpenA8DJWasapiProbe.cpp",
    "windows\tests\proaudio_endpoint_selection_test.cpp"
)

function Resolve-MSBuild {
    param([string]$Requested)

    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        if (-not (Test-Path -LiteralPath $Requested)) {
            throw "MSBuild was not found at '$Requested'."
        }
        return (Resolve-Path -LiteralPath $Requested).Path
    }
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" |
            Select-Object -First 1
        if (-not [string]::IsNullOrWhiteSpace($found)) {
            return $found
        }
    }
    throw "MSBuild.exe was not found. Install Visual Studio 2022 C++ build tools or pass -MsBuild."
}

function Get-SourceHashes {
    $hashes = @{}
    foreach ($relativePath in $sourceInputs) {
        $absolutePath = Join-Path $repoRoot $relativePath
        if (-not (Test-Path -LiteralPath $absolutePath)) {
            throw "Expected source input is missing: $relativePath"
        }
        $hashes[$relativePath] = (Get-FileHash -LiteralPath $absolutePath -Algorithm SHA256).Hash.ToLowerInvariant()
    }
    return $hashes
}

function Assert-ProjectInputManifest {
    [xml]$projectXml = Get-Content -LiteralPath $project -Raw
    $namespace = New-Object System.Xml.XmlNamespaceManager($projectXml.NameTable)
    $namespace.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")

    $actualCompile = @($projectXml.SelectNodes("//msb:ClCompile", $namespace) |
        ForEach-Object { $_.GetAttribute("Include") } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object)
    $expectedCompile = @(
        "OpenA8DJEndpointSelection.cpp",
        "OpenA8DJWasapiProbe.cpp",
        "..\tests\proaudio_endpoint_selection_test.cpp"
    ) | Sort-Object
    if (@(Compare-Object $expectedCompile $actualCompile).Count -ne 0) {
        throw "Project compile-item manifest differs from the reviewed source set."
    }

    $actualHeaders = @($projectXml.SelectNodes("//msb:ClInclude", $namespace) |
        ForEach-Object { $_.GetAttribute("Include") } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Sort-Object)
    if (@(Compare-Object @("OpenA8DJProAudioBackend.h") $actualHeaders).Count -ne 0) {
        throw "Project header manifest differs from the reviewed source set."
    }
    if ($projectXml.SelectNodes(
            "//msb:CustomBuild | //msb:PreBuildEvent | //msb:PostBuildEvent",
            $namespace).Count -ne 0) {
        throw "Project contains an unreviewed custom or pre/post-build action."
    }
}

function Assert-DirectForbiddenTokenScreen {
    # This is deliberately described as a direct-token screen, not proof that
    # indirect calls or behavior are impossible.
    $patterns = @(
        '->\s*Initialize\s*\(',
        '->\s*Start\s*\(',
        '\bDeviceIoControl\s*\(',
        '\bDllRegisterServer\b',
        '\bDllUnregisterServer\b',
        '\bReg(Create|Open|Set|Delete)Key[A-Za-z]*\s*\('
    )
    foreach ($relativePath in $compiledCppInputs) {
        $source = Get-Content -LiteralPath (Join-Path $repoRoot $relativePath) -Raw
        foreach ($pattern in $patterns) {
            if ($source -match $pattern) {
                throw "Direct forbidden-token screen matched '$pattern' in '$relativePath'."
            }
        }
    }
}

if ([string]::IsNullOrWhiteSpace($OutDir)) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutDir = Join-Path $repoRoot "local-analysis\windows-proaudio-preflight-$stamp"
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null
$OutDir = (Resolve-Path -LiteralPath $OutDir).Path

Assert-ProjectInputManifest
Assert-DirectForbiddenTokenScreen
$hashesBefore = Get-SourceHashes
$msbuildPath = Resolve-MSBuild -Requested $MsBuild

function Invoke-Build {
    param(
        [string]$TargetOutDir,
        [string]$TargetIntDir,
        [bool]$OfflineTest,
        [string]$LogName
    )

    New-Item -ItemType Directory -Path $TargetOutDir, $TargetIntDir -Force | Out-Null
    $arguments = @(
        $project,
        "/nologo",
        "/m",
        "/t:Rebuild",
        "/p:Configuration=$Configuration",
        "/p:Platform=x64",
        "/p:OpenA8DJBuildOfflineTest=$($OfflineTest.ToString().ToLowerInvariant())",
        "/p:OutDir=$TargetOutDir\",
        "/p:IntDir=$TargetIntDir\"
    )
    $output = & $msbuildPath @arguments 2>&1
    $exitCode = $LASTEXITCODE
    $output | Set-Content -LiteralPath (Join-Path $OutDir $LogName) -Encoding UTF8
    if ($exitCode -ne 0) {
        throw "MSBuild failed with exit code $exitCode; see $LogName."
    }
}

$testBuildDir = Join-Path $OutDir "build\test"
$testObjDir = Join-Path $OutDir "obj\test"
Invoke-Build -TargetOutDir $testBuildDir -TargetIntDir $testObjDir -OfflineTest $true -LogName "build-offline-test.log"

$offlineTest = Join-Path $testBuildDir "proaudio_endpoint_selection_test.exe"
$offlineOutput = & $offlineTest 2>&1
$offlineExit = $LASTEXITCODE
$offlineOutput | Set-Content -LiteralPath (Join-Path $OutDir "offline-test.txt") -Encoding UTF8
if ($offlineExit -ne 0) {
    throw "Offline endpoint-selection test failed with exit code $offlineExit."
}

$probeExecuted = $false
$probeExit = $null
$probePass = $null
$result = $null
if (-not $OfflineOnly) {
    $probeBuildDir = Join-Path $OutDir "build\probe"
    $probeObjDir = Join-Path $OutDir "obj\probe"
    Invoke-Build -TargetOutDir $probeBuildDir -TargetIntDir $probeObjDir -OfflineTest $false -LogName "build-probe.log"

    $probe = Join-Path $probeBuildDir "OpenA8DJProAudioProbe.exe"
    $stderrPath = Join-Path $OutDir "probe-stderr.txt"
    $jsonLines = & $probe 2> $stderrPath
    $probeExit = $LASTEXITCODE
    $probeExecuted = $true
    $jsonText = $jsonLines -join [Environment]::NewLine
    $jsonPath = Join-Path $OutDir "probe.json"
    $jsonText | Set-Content -LiteralPath $jsonPath -Encoding UTF8
    try {
        $result = $jsonText | ConvertFrom-Json
        $probePass = [bool]$result.pass
    } catch {
        throw "Probe did not produce valid JSON; see '$jsonPath' and '$stderrPath'."
    }
} else {
    "probe_execution=skipped_by_offline_only_policy" |
        Set-Content -LiteralPath (Join-Path $OutDir "probe-status.txt") -Encoding UTF8
}

$hashesAfter = Get-SourceHashes
foreach ($relativePath in $sourceInputs) {
    if ($hashesBefore[$relativePath] -ne $hashesAfter[$relativePath]) {
        throw "Source changed during verification: $relativePath"
    }
}

$compiledBy = @{
    "windows\proaudio\OpenA8DJProAudioBackend.h" = "probe-and-offline-test"
    "windows\proaudio\OpenA8DJEndpointSelection.cpp" = "probe-and-offline-test"
    "windows\proaudio\OpenA8DJWasapiProbe.cpp" = "probe"
    "windows\tests\proaudio_endpoint_selection_test.cpp" = "offline-test"
    $projectRelative = "msbuild-control"
    "windows\tests\run-a8dj-proaudio-preflight.ps1" = "verification-orchestrator"
}
$hashManifest = @(
    foreach ($relativePath in $sourceInputs) {
        [pscustomobject]@{
            relative_path = $relativePath
            sha256 = $hashesAfter[$relativePath]
            role = $compiledBy[$relativePath]
            compiled_this_run = if ($relativePath -eq "windows\proaudio\OpenA8DJWasapiProbe.cpp") {
                -not $OfflineOnly
            } elseif ($relativePath.EndsWith(".cpp") -or $relativePath.EndsWith(".h")) {
                $relativePath -ne "windows\proaudio\OpenA8DJWasapiProbe.cpp"
            } else {
                $false
            }
        }
    }
)
$hashPath = Join-Path $OutDir "source-hashes.json"
$hashManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $hashPath -Encoding UTF8

# Safety evidence is emitted only after the requested build/test/probe steps and
# after confirming the reviewed source hashes did not change during the run.
@(
    "evidence_generated_after_requested_steps=1"
    "execution_mode=$(if ($OfflineOnly) { 'offline_only' } else { 'passive_probe' })"
    "live_probe_executed=$([int]$probeExecuted)"
    "project_item_manifest_check=pass"
    "direct_forbidden_token_screen=pass"
    "direct_token_screen_scope=all_declared_cpp_and_headers_for_probe_and_offline_targets"
    "direct_token_screen_limitation=does_not_prove_absence_of_indirect_calls_or_runtime_behavior"
    "source_hash_stability=pass"
    "source_hash_manifest=$hashPath"
    "driver_install_or_registration_performed=not_observed_by_this_orchestrator"
) | Set-Content -LiteralPath (Join-Path $OutDir "safety.txt") -Encoding UTF8

$summaryLines = @(
    "overall_pass=$([int]($offlineExit -eq 0 -and ($OfflineOnly -or ($probeExit -eq 0 -and $probePass))))"
    "offline_selection_test=pass"
    "probe_executed=$([int]$probeExecuted)"
    "probe_exit=$(if ($probeExit -eq $null) { 'not-run' } else { $probeExit })"
    "probe_pass=$(if ($probePass -eq $null) { 'not-run' } else { [int]$probePass })"
    "source_hash_manifest=$hashPath"
)
if ($result -ne $null) {
    $summaryLines += @(
        "selection_status=$($result.selection_status)"
        "common_identity=$($result.common_identity)"
        "selected_formats_pass=$([int][bool]$result.selected_formats_pass)"
        "selected_periods_pass=$([int][bool]$result.selected_periods_pass)"
    )
}
$summaryLines | Set-Content -LiteralPath (Join-Path $OutDir "summary.txt") -Encoding UTF8

Write-Host "Passive pro-audio preflight artifacts: $OutDir"
if (-not $OfflineOnly -and ($probeExit -ne 0 -or -not $probePass)) {
    throw "Passive pro-audio preflight did not pass; see the generated probe evidence."
}
