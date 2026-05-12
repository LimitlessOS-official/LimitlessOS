param(
    [string]$M4EvidenceDir = "",
    [string]$M41EvidenceDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$evidenceDir = Join-Path $distDir "m5-evidence-$timestamp"
$commandDir = Join-Path $evidenceDir "commands"
$artifactDir = Join-Path $evidenceDir "artifacts"
New-Item -ItemType Directory -Force -Path $commandDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

function Get-LatestEvidenceDir
{
    param([string]$Filter)

    $latest = Get-ChildItem -LiteralPath $distDir -Directory -Filter $Filter |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $latest) {
        return ""
    }
    return $latest.FullName
}

function Copy-IfPresent
{
    param([string]$Path, [string]$Destination)

    if (Test-Path -LiteralPath $Path) {
        Copy-Item -LiteralPath $Path -Destination $Destination -Force
    }
}

function Get-RepoRelativePath
{
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetFullPath($root).TrimEnd('\')
    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length + 1)
    }
    return $fullPath
}

function Invoke-EvidenceCommand
{
    param(
        [string]$Name,
        [string]$ScriptPath,
        [string[]]$Arguments = @()
    )

    $stdoutPath = Join-Path $commandDir "$Name.stdout.txt"
    $stderrPath = Join-Path $commandDir "$Name.stderr.txt"
    $outputPath = Join-Path $commandDir "$Name.output.txt"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList (@("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $Arguments) `
        -WorkingDirectory $root `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath
    $stopwatch.Stop()

    $combined = New-Object System.Collections.Generic.List[string]
    if (Test-Path -LiteralPath $stdoutPath) {
        foreach ($line in Get-Content -LiteralPath $stdoutPath) {
            $combined.Add($line)
        }
    }
    if (Test-Path -LiteralPath $stderrPath) {
        foreach ($line in Get-Content -LiteralPath $stderrPath) {
            $combined.Add($line)
        }
    }
    $combined | Set-Content -LiteralPath $outputPath -Encoding UTF8

    $record = [PSCustomObject]@{
        name = $Name
        command = "& `"$ScriptPath`" $($Arguments -join ' ')"
        exitCode = $process.ExitCode
        elapsedSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        outputFile = Get-RepoRelativePath $outputPath
        stdoutFile = Get-RepoRelativePath $stdoutPath
        stderrFile = Get-RepoRelativePath $stderrPath
    }
    if ($process.ExitCode -ne 0) {
        throw "Evidence command '$Name' failed with exit code $($process.ExitCode). See $outputPath"
    }
    return $record
}

if ([string]::IsNullOrWhiteSpace($M4EvidenceDir)) {
    $M4EvidenceDir = Get-LatestEvidenceDir -Filter "m4-evidence-*"
}
if ([string]::IsNullOrWhiteSpace($M41EvidenceDir)) {
    $M41EvidenceDir = Get-LatestEvidenceDir -Filter "m4-1-evidence-*"
}

$m4EvidencePath = Join-Path $M4EvidenceDir "m4-evidence.json"
$m41EvidencePath = Join-Path $M41EvidenceDir "m4-1-evidence.json"
if (-not (Test-Path -LiteralPath $m4EvidencePath)) {
    throw "Accepted M4 evidence was not found."
}
if (-not (Test-Path -LiteralPath $m41EvidencePath)) {
    throw "M4.1 evidence was not found."
}

$records = New-Object System.Collections.Generic.List[object]
$records.Add((Invoke-EvidenceCommand -Name "assert-product" -ScriptPath (Join-Path $PSScriptRoot "assert-m1-production-slice.ps1") -Arguments @("-Architecture", "x86_64", "-BuildProfile", "Product", "-WriteInventory")))
$installerEvidenceDir = Join-Path $evidenceDir "installer"
$records.Add((Invoke-EvidenceCommand -Name "verify-installer-m5" -ScriptPath (Join-Path $PSScriptRoot "verify-installer-m5.ps1") -Arguments @("-EvidenceDir", $installerEvidenceDir)))

Copy-IfPresent -Path $m4EvidencePath -Destination $artifactDir
Copy-IfPresent -Path $m41EvidencePath -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "docs\installer\m5-safe-installer.md") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "docs\hardware\msi-cyborg-15-a13ve.md") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "dist\limitlessos-x86_64.product.m4.json") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "dist\limitlessos-x86_64.size.txt") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "dist\limitlessos-x86_64.scaffold.txt") -Destination $artifactDir

$installerSummaryPath = Join-Path $installerEvidenceDir "m5-installer-verification.json"
$installerSummary = Get-Content -LiteralPath $installerSummaryPath -Raw | ConvertFrom-Json
$productInventory = Get-Content -LiteralPath (Join-Path $root "dist\limitlessos-x86_64.product.m4.json") -Raw | ConvertFrom-Json

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

$metadata = [ordered]@{
    milestone = "M5 Safe Installer + Partition Protection"
    generatedAt = (Get-Date).ToString("o")
    architecture = "x86_64"
    evidenceDirectory = Get-RepoRelativePath $evidenceDir
    m4AcceptedEvidence = Get-RepoRelativePath $m4EvidencePath
    m41Evidence = Get-RepoRelativePath $m41EvidencePath
    commands = @($records.ToArray())
    installerVerification = Get-RepoRelativePath $installerSummaryPath
    dryRunNoWrites = $installerSummary.dryRunNoWrites
    forbiddenPartitionsDetected = $installerSummary.forbiddenPartitionsDetected
    unknownPartitionsRefused = $installerSummary.unknownPartitionsRefused
    dedicatedTargetAccepted = $installerSummary.dedicatedTargetAccepted
    writeRequiresScopedCapability = $installerSummary.writeRequiresScopedCapability
    bootEntryRequiresSeparateAuthority = $installerSummary.bootEntryRequiresSeparateAuthority
    failedConfirmationPreventsWrites = $installerSummary.failedConfirmationPreventsWrites
    successfulInstallVerified = $installerSummary.successfulInstallVerified
    noAmbientAuthority = $installerSummary.noAmbientAuthority
    productKernelBytes = $productInventory.productKernelBytes
    productKernelSectors = $productInventory.productKernelSectors
    productKernelReserve = $productInventory.productKernelReserve
    productKernelChecksum = $productInventory.productKernelChecksum
    productUefiKernelByteLimit = $productInventory.productUefiKernelByteLimit
    productUefiKernelByteReserve = $productInventory.productUefiKernelByteReserve
    biosReserveSafetyDecision = "Installer code remains outside the BIOS-constrained Product kernel while reserve is below 128 sectors."
    realHardwareWritePolicy = "MSI Cyborg 15 A13VE may run dry-run only; internal NVMe writes remain disabled by default until dry-run output is reviewed."
    gitStatus = Get-RepoRelativePath $gitStatusPath
}

$metadataPath = Join-Path $evidenceDir "m5-evidence.json"
$metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

Write-Host "M5 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
