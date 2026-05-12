param(
    [string]$EvidenceDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $EvidenceDir = Join-Path $root "dist\m9-msi-parser-verify"
}
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null

$fixtureDir = Join-Path $EvidenceDir "fixtures"
$installerEvidenceDir = Join-Path $EvidenceDir "installer"
$generator = Join-Path $PSScriptRoot "generate-installer-fixtures.ps1"
$installer = Join-Path $PSScriptRoot "limitless-installer.ps1"
$parser = Join-Path $PSScriptRoot "parse-msi-dryrun-evidence.ps1"

& $generator -OutputDir $fixtureDir
if (-not $?) {
    throw "M9 MSI dry-run parser verifier failed: fixture generation failed."
}
New-Item -ItemType Directory -Force -Path $installerEvidenceDir | Out-Null

$image = Join-Path $fixtureDir "windows-like.img"
$dryRunOutput = Join-Path $installerEvidenceDir "msi-windows-like-dryrun.output.txt"
$parseOutput = Join-Path $installerEvidenceDir "msi-windows-like-dryrun.parsed.json"
$stdout = Join-Path $installerEvidenceDir "msi-windows-like-dryrun.stdout.txt"
$stderr = Join-Path $installerEvidenceDir "msi-windows-like-dryrun.stderr.txt"

$process = Start-Process `
    -FilePath "powershell.exe" `
    -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $installer,
        "-ImagePath", $image,
        "-Mode", "DryRun",
        "-GrantHardwareInventoryCapability",
        "-GrantReadOnlyBlockCapability"
    ) `
    -WorkingDirectory $root `
    -NoNewWindow `
    -Wait `
    -PassThru `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr

$combined = New-Object System.Collections.Generic.List[string]
if (Test-Path -LiteralPath $stdout) {
    foreach ($line in Get-Content -LiteralPath $stdout) { $combined.Add($line) }
}
if (Test-Path -LiteralPath $stderr) {
    foreach ($line in Get-Content -LiteralPath $stderr) { $combined.Add($line) }
}
$combined | Set-Content -LiteralPath $dryRunOutput -Encoding UTF8
if ($process.ExitCode -ne 0) {
    throw "M9 MSI dry-run parser verifier failed: installer dry-run exited $($process.ExitCode)."
}

& $parser -InputPath $dryRunOutput -OutputPath $parseOutput | Out-Null
if (-not $?) {
    throw "M9 MSI dry-run parser verifier failed: parser failed."
}

$report = Get-Content -LiteralPath $parseOutput -Raw | ConvertFrom-Json
function Assert-True
{
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw "M9 MSI dry-run parser verifier failed: $Message"
    }
}

Assert-True ($report.partitionTableType -eq "GPT") "partition table was not recognized as GPT."
Assert-True ([bool]$report.detectedWindowsEsp) "Windows ESP was not detected."
Assert-True ([bool]$report.detectedNtfs) "NTFS partition was not detected."
Assert-True ([bool]$report.detectedMicrosoftReserved) "Microsoft Reserved partition was not detected."
Assert-True ([bool]$report.detectedRecovery) "Recovery partition was not detected."
Assert-True ([bool]$report.writeDisabledStatus) "write-disabled status was not true."
Assert-True ([bool]$report.dryRunNoWriteStatus) "dry-run no-write status was not true."
Assert-True (-not [bool]$report.realInstallApprovalStatus) "real install approval was not false."
Assert-True (@($report.forbiddenPartitions).Count -ge 4) "forbidden partition list was incomplete."

Write-Host "M9 MSI dry-run parser verifier passed."
Write-Host "parsed-evidence: $parseOutput"
