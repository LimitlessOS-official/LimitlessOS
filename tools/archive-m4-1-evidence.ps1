param(
    [string]$M4EvidenceDir = "",
    [string]$MachineModel = "MSI Cyborg 15 A13VE"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"

if ([string]::IsNullOrWhiteSpace($M4EvidenceDir)) {
    $latestM4 = Get-ChildItem -LiteralPath $distDir -Directory -Filter "m4-evidence-*" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $latestM4) {
        throw "No M4 evidence directory found under dist."
    }
    $M4EvidenceDir = $latestM4.FullName
}

$m4EvidencePath = Join-Path $M4EvidenceDir "m4-evidence.json"
if (-not (Test-Path -LiteralPath $m4EvidencePath)) {
    throw "M4 evidence metadata was not found: $m4EvidencePath"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$evidenceDir = Join-Path $distDir "m4-1-evidence-$timestamp"
$artifactDir = Join-Path $evidenceDir "artifacts"
$commandsDir = Join-Path $evidenceDir "m4-commands"
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
New-Item -ItemType Directory -Force -Path $commandsDir | Out-Null

function Copy-IfPresent
{
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    if (Test-Path -LiteralPath $Path) {
        Copy-Item -LiteralPath $Path -Destination $Destination -Force
    }
}

function Get-RepoRelativePath
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetFullPath($root).TrimEnd('\')
    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length + 1)
    }

    return $fullPath
}

$m4Evidence = Get-Content -LiteralPath $m4EvidencePath -Raw | ConvertFrom-Json
$productInventoryPath = Join-Path $root $m4Evidence.productInventory
if (-not (Test-Path -LiteralPath $productInventoryPath)) {
    $productInventoryPath = Join-Path $M4EvidenceDir "artifacts\product\limitlessos-x86_64.product.m4.json"
}
if (-not (Test-Path -LiteralPath $productInventoryPath)) {
    throw "Product M4 inventory was not found."
}

$inventory = Get-Content -LiteralPath $productInventoryPath -Raw | ConvertFrom-Json
$checklistPath = Join-Path $root "docs\hardware\msi-cyborg-15-a13ve.md"
Copy-IfPresent -Path $m4EvidencePath -Destination $artifactDir
Copy-IfPresent -Path $productInventoryPath -Destination $artifactDir
Copy-IfPresent -Path $checklistPath -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "dist\limitlessos-x86_64.size.txt") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "dist\limitlessos-x86_64.scaffold.txt") -Destination $artifactDir

$sourceCommandDir = Join-Path $M4EvidenceDir "commands"
if (Test-Path -LiteralPath $sourceCommandDir) {
    Copy-Item -LiteralPath (Join-Path $sourceCommandDir "*") -Destination $commandsDir -Force
}

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

$budget = [ordered]@{
    productKernelBytes = $inventory.productKernelBytes
    productKernelSectors = $inventory.productKernelSectors
    productKernelReserve = $inventory.productKernelReserve
    productKernelChecksum = $inventory.productKernelChecksum
    productUefiKernelByteLimit = $inventory.productUefiKernelByteLimit
    productUefiKernelByteReserve = $inventory.productUefiKernelByteReserve
    sectorBudgetStatus = $inventory.sectorBudgetStatus
    biosReserveWarningThreshold = 128
    biosReserveHardFloor = 96
    biosReserveSafetyDecision = "M5 installer code must not be added to the BIOS-constrained Product kernel while reserve remains below 128 sectors."
    uefiContract = "UEFI remains governed by KERNEL64.BIN byte budget, manifest/checksum correctness, placement/load correctness, and artifact inventory correctness; UEFI is not blocked by the BIOS 1024-sector ceiling."
}

$manualEvidenceTemplate = [ordered]@{
    machineModel = $MachineModel
    bootMode = $null
    secureBootState = $null
    inputBackendUsed = $null
    displayResolution = $null
    mouseTouchpadResult = $null
    keyboardResult = $null
    terminalResult = $null
    fileManagerResult = $null
    settingsResult = $null
    internalStorageWriteStatus = "pending; must remain disabled unless explicitly enabled through a later safe installer path"
    photosOrVideoFilenames = @()
    testerNotes = $null
}

$metadata = [ordered]@{
    milestone = "M4.1 Real Hardware GUI Validation + BIOS Reserve Safety"
    generatedAt = (Get-Date).ToString("o")
    architecture = "x86_64"
    evidenceDirectory = Get-RepoRelativePath $evidenceDir
    m4AcceptedEvidence = Get-RepoRelativePath $m4EvidencePath
    archivedM4CommandOutputs = Get-RepoRelativePath $commandsDir
    manualHardwareChecklist = "docs\hardware\msi-cyborg-15-a13ve.md"
    physicalValidationStatus = "pending user-supplied MSI Cyborg 15 A13VE results; Codex cannot access physical hardware"
    productGuiPreserved = $true
    productGuiApps = @("Terminal", "File Manager", "Settings")
    noM5Started = $true
    kernelBudget = $budget
    manualEvidenceTemplate = $manualEvidenceTemplate
    gitStatus = Get-RepoRelativePath $gitStatusPath
}

$metadataPath = Join-Path $evidenceDir "m4-1-evidence.json"
$metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

Write-Host "M4.1 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
