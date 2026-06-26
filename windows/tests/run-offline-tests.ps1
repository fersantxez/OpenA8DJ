param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$outDir = Join-Path $env:TEMP "OpenA8DJ\offline-tests\$Configuration\bin"
$objDir = Join-Path $env:TEMP "OpenA8DJ\offline-tests\$Configuration\obj"
$testExe = Join-Path $outDir "opena8dj-audio-engine-contract.exe"

function Find-Python {
    $python = Get-Command python -ErrorAction SilentlyContinue
    if ($python) {
        return @{
            Path = $python.Source
            Args = @()
        }
    }

    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        return @{
            Path = $py.Source
            Args = @("-3")
        }
    }

    throw "Python was not found. Install Python 3 or add python.exe to PATH."
}

function Find-CCompiler {
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    if ($clang) {
        return @{
            Kind = "clang"
            Path = $clang.Source
        }
    }

    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) {
        return @{
            Kind = "msvc"
            Path = $cl.Source
        }
    }

    throw "No C compiler found. Use a Developer PowerShell for Visual Studio or install LLVM/clang."
}

New-Item -ItemType Directory -Path $outDir -Force | Out-Null
New-Item -ItemType Directory -Path $objDir -Force | Out-Null

$python = Find-Python
& $python.Path @($python.Args) "windows\tests\validate_windows_surface_contract.py"
if ($LASTEXITCODE -ne 0) {
    throw "Windows surface contract test failed with exit code $LASTEXITCODE"
}

$compiler = Find-CCompiler
if ($compiler.Kind -eq "clang") {
    & $compiler.Path `
        -std=c11 `
        -Wall `
        -Wextra `
        -Werror `
        -Iwindows\audio `
        windows\audio\OpenA8DJAudioEngine.c `
        windows\tests\audio_engine_contract_test.c `
        -o $testExe
} else {
    & $compiler.Path `
        /nologo `
        /W4 `
        /WX `
        /Iwindows\audio `
        windows\audio\OpenA8DJAudioEngine.c `
        windows\tests\audio_engine_contract_test.c `
        /Fo"$objDir\" `
        /Fe:$testExe
}

if ($LASTEXITCODE -ne 0) {
    throw "Audio engine contract build failed with exit code $LASTEXITCODE"
}

& $testExe
if ($LASTEXITCODE -ne 0) {
    throw "Audio engine contract test failed with exit code $LASTEXITCODE"
}

Write-Host "PASS: OpenA8DJ Windows offline tests"
Write-Host "Offline test executable: $testExe"
