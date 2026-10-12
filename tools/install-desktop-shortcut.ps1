# Creates a "LimitlessOS" shortcut on the current user's desktop that runs
# tools\run-desktop.ps1 (rebuild when sources changed, then boot in QEMU).
# Run it again after moving the repository. Remove the shortcut to uninstall.

param(
    [string]$Name = "LimitlessOS"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$launcher = Join-Path $PSScriptRoot "run-desktop.ps1"
$root = Split-Path -Parent $PSScriptRoot
$desktop = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktop "$Name.lnk"

$shellPath = $null
$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
if ($pwsh) {
    $shellPath = $pwsh.Source
    # A Store install resolves to a version-stamped WindowsApps folder that changes on
    # every update; target the stable per-user app alias instead.
    $alias = Join-Path $env:LOCALAPPDATA "Microsoft\WindowsApps\pwsh.exe"
    if (($shellPath -like "*\WindowsApps\*") -and (Test-Path $alias)) {
        $shellPath = $alias
    }
}
if (-not $shellPath) {
    $shellPath = (Get-Command powershell -ErrorAction Stop).Source
}

# Keep the console open only when the launch fails, so build errors stay readable.
$command = "try {{ & '{0}' }} catch {{ Write-Host `$_ -ForegroundColor Red; Read-Host 'LimitlessOS failed to start. Press Enter to close' }}" -f $launcher.Replace("'", "''")

$qemuIcon = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$wsh = New-Object -ComObject WScript.Shell
$shortcut = $wsh.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $shellPath
$shortcut.Arguments = "-NoProfile -ExecutionPolicy Bypass -Command `"$command`""
$shortcut.WorkingDirectory = $root
$shortcut.Description = "Build if needed and run the LimitlessOS desktop in QEMU"
if (Test-Path $qemuIcon) {
    $shortcut.IconLocation = "$qemuIcon,0"
}
$shortcut.Save()

Write-Host "Created $shortcutPath"
