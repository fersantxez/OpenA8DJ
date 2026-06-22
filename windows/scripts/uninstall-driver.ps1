param(
    [int]$WaitLockSeconds = 0
)

$ErrorActionPreference = "Stop"
Import-Module (Join-Path $PSScriptRoot "OpenA8DJ.WindowsCommon.psm1") -Force
Assert-OpenA8DJAdministrator

$repoRoot = Get-OpenA8DJRepoRoot
$runDir = Join-Path $repoRoot "local-analysis\windows\uninstall-$(Get-Date -Format yyyyMMdd-HHmmss)"
$lock = Acquire-OpenA8DJHardwareLock -Gate "windows-uninstall-driver" -RunDir $runDir -TimeoutSeconds $WaitLockSeconds

try {
    $drivers = & pnputil.exe /enum-drivers
    $publishedNames = @()
    $current = @{}
    foreach ($line in ($drivers + "")) {
        if ($line -match "^\s*$") {
            if (($current.Provider -eq "OpenA8DJ") -or ($current.ClassName -eq "USBDevice" -and $current.OriginalName -eq "OpenA8DJUsb.inf")) {
                if ($current.PublishedName) {
                    $publishedNames += $current.PublishedName
                }
            }
            $current = @{}
            continue
        }
        if ($line -match "Published Name\s*:\s*(.+)$") { $current.PublishedName = $matches[1].Trim() }
        if ($line -match "Original Name\s*:\s*(.+)$") { $current.OriginalName = $matches[1].Trim() }
        if ($line -match "Provider Name\s*:\s*(.+)$") { $current.Provider = $matches[1].Trim() }
        if ($line -match "Class Name\s*:\s*(.+)$") { $current.ClassName = $matches[1].Trim() }
    }

    $publishedNames = $publishedNames | Sort-Object -Unique
    if (-not $publishedNames) {
        Write-Host "No OpenA8DJ driver package found in the driver store."
    }

    foreach ($name in $publishedNames) {
        Write-Host "Deleting driver package $name"
        & pnputil.exe /delete-driver $name /uninstall /force
        if ($LASTEXITCODE -ne 0) {
            throw "pnputil delete-driver $name failed with exit code $LASTEXITCODE"
        }
    }

    $manifest = [ordered]@{
        uninstalled_at = (Get-Date).ToUniversalTime().ToString("o")
        removed_driver_packages = $publishedNames
        lock_run_dir = $runDir
    }
    $manifestPath = Join-Path $runDir "uninstall-manifest.json"
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -Path $manifestPath -Encoding ASCII
    Write-Host "Uninstall manifest written to $manifestPath"
} finally {
    Release-OpenA8DJHardwareLock -Lock $lock
}
