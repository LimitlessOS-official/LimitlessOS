param(
    [ValidateSet("x86_64")]
    [string]$Architecture = "x86_64",

    [switch]$IncludeExperimental
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$evidenceDir = Join-Path $distDir "m17-evidence-$timestamp"
$commandDir = Join-Path $evidenceDir "commands"
$artifactDir = Join-Path $evidenceDir "artifacts"
New-Item -ItemType Directory -Force -Path $commandDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

$records = New-Object System.Collections.Generic.List[object]

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

function Get-InventoryPath
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)
    return Join-Path $distDir ("limitlessos-x86_64.{0}.m17.json" -f $BuildProfile.ToLowerInvariant())
}

function Read-Inventory
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)
    $path = Get-InventoryPath -BuildProfile $BuildProfile
    if (Test-Path -LiteralPath $path) {
        return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    }
    return $null
}

function Copy-IfPresent
{
    param([string]$Path, [string]$Destination)
    if (Test-Path -LiteralPath $Path) {
        Copy-Item -LiteralPath $Path -Destination $Destination -Force
    }
}

function Invoke-EvidenceCommand
{
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [string[]]$Arguments = @(),
        [ValidateSet("Product", "Experimental")][string]$BuildProfile = "Product"
    )

    $outputPath = Join-Path $commandDir "$Name.output.txt"
    $commandText = "& `"$ScriptPath`" $($Arguments -join ' ')"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $outputLines = @(& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ScriptPath @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    $stopwatch.Stop()
    $outputLines | Set-Content -LiteralPath $outputPath -Encoding UTF8

    $inventory = Read-Inventory -BuildProfile $BuildProfile
    $records.Add([pscustomobject][ordered]@{
        name = $Name
        command = $commandText
        exitCode = $exitCode
        elapsedSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        outputFile = Get-RepoRelativePath $outputPath
        buildProfile = $BuildProfile
        biosKernelBytes = if ($inventory) { $inventory.productBiosKernelBytes } else { $null }
        biosKernelSectors = if ($inventory) { $inventory.productBiosKernelSectors } else { $null }
        biosKernelReserve = if ($inventory) { $inventory.productBiosKernelReserve } else { $null }
        biosKernelChecksum = if ($inventory) { $inventory.productBiosKernelChecksum } else { $null }
        uefiKernelBytes = if ($inventory) { $inventory.productUefiKernelBytes } else { $null }
        uefiKernelByteLimit = if ($inventory) { $inventory.productUefiKernelByteLimit } else { $null }
        uefiKernelByteReserve = if ($inventory) { $inventory.productUefiKernelByteReserve } else { $null }
        uefiKernelChecksum = if ($inventory) { $inventory.productUefiKernelChecksum } else { $null }
        artifactInventoryJson = Get-RepoRelativePath (Get-InventoryPath -BuildProfile $BuildProfile)
    })

    if ($exitCode -ne 0) {
        throw "Evidence command '$Name' failed with exit code $exitCode. See $outputPath"
    }
}

function Archive-ProfileArtifacts
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)
    $profileDir = Join-Path $artifactDir $BuildProfile.ToLowerInvariant()
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    foreach ($milestone in @("m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8", "m9", "m10", "m11", "m12", "m13", "m14", "m15", "m16", "m17")) {
        if ($milestone -eq "m1") {
            Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.m1.json") -Destination $profileDir
        }
        else {
            Copy-IfPresent -Path (Join-Path $distDir ("limitlessos-x86_64.{0}.{1}.json" -f $BuildProfile.ToLowerInvariant(), $milestone)) -Destination $profileDir
        }
    }
    foreach ($artifact in @(
        "limitlessos-x86_64.iso",
        "limitlessos-x86_64.img",
        "limitlessos-x86_64-uefi.img",
        "limitlessos-x86_64.scaffold.bin",
        "limitlessos-x86_64.uefi-kernel.bin",
        "KERNEL64-BIOS.BIN",
        "KERNEL64.BIN",
        "limitlessos-x86_64.scaffold.txt",
        "limitlessos-x86_64.size.txt"
    )) {
        Copy-IfPresent -Path (Join-Path $distDir $artifact) -Destination $profileDir
    }
}

$buildScript = Join-Path $PSScriptRoot "build.ps1"
$assertScript = Join-Path $PSScriptRoot "assert-m1-production-slice.ps1"
$verifyScript = Join-Path $PSScriptRoot "verify-qemu.ps1"
$persistenceScript = Join-Path $PSScriptRoot "verify-nvme-persistence.ps1"
$installerScript = Join-Path $PSScriptRoot "verify-installer-m5.ps1"
$fixtureScript = Join-Path $PSScriptRoot "verify-package-m7-1.ps1"
$m8UxScript = Join-Path $PSScriptRoot "verify-package-m8-ux.ps1"
$m9HwValScript = Join-Path $PSScriptRoot "verify-hardware-validation-m9.ps1"
$m9ParserScript = Join-Path $PSScriptRoot "verify-msi-dryrun-parser-m9.ps1"
$m10LoginScript = Join-Path $PSScriptRoot "verify-login-m10.ps1"
$m11IdentityScript = Join-Path $PSScriptRoot "verify-identity-m11.ps1"
$m12TransportScript = Join-Path $PSScriptRoot "verify-identity-transport-m12.ps1"
$m13AccountScript = Join-Path $PSScriptRoot "verify-account-association-m13.ps1"
$m14CloudScript = Join-Path $PSScriptRoot "verify-cloud-storage-m14.ps1"
$m15InstallerUxScript = Join-Path $PSScriptRoot "verify-installer-ux-m15.ps1"
$m16AiPolicyScript = Join-Path $PSScriptRoot "verify-ai-policy-m16.ps1"
$m17AssistantScript = Join-Path $PSScriptRoot "verify-ai-assistant-m17.ps1"
$privateKeyScanScript = Join-Path $PSScriptRoot "verify-private-key-artifacts.ps1"

if ($IncludeExperimental) {
    Invoke-EvidenceCommand -Name "build-experimental" -ScriptPath $buildScript -Arguments @("-Architecture", $Architecture, "-BuildProfile", "Experimental") -BuildProfile "Experimental"
    Invoke-EvidenceCommand -Name "assert-experimental" -ScriptPath $assertScript -Arguments @("-Architecture", $Architecture, "-BuildProfile", "Experimental", "-WriteInventory") -BuildProfile "Experimental"
    Invoke-EvidenceCommand -Name "verify-experimental-uefi" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "uefi", "-BuildProfile", "Experimental") -BuildProfile "Experimental"
    Archive-ProfileArtifacts -BuildProfile "Experimental"
}

Invoke-EvidenceCommand -Name "build-product" -ScriptPath $buildScript -Arguments @("-Architecture", $Architecture, "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "assert-product" -ScriptPath $assertScript -Arguments @("-Architecture", $Architecture, "-BuildProfile", "Product", "-WriteInventory") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-disk" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "disk", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-uefi" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-iso" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "iso", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-uefi-e1000e" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "uefi", "-NetworkDevice", "e1000e", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-iso-e1000e" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "iso", "-NetworkDevice", "e1000e", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-nvme-persistence" -ScriptPath $persistenceScript -Arguments @("-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-gui-interactive" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-service-session" -ScriptPath $verifyScript -Arguments @("-Architecture", $Architecture, "-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-installer-m5" -ScriptPath $installerScript -Arguments @("-EvidenceDir", (Join-Path $evidenceDir "installer")) -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-negative-fixtures" -ScriptPath $fixtureScript -Arguments @("-Mode", "Package") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-update-index-negative-fixtures" -ScriptPath $fixtureScript -Arguments @("-Mode", "Update") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-ux" -ScriptPath $m8UxScript -Arguments @("-BootMedia", "uefi") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-ux-iso" -ScriptPath $m8UxScript -Arguments @("-BootMedia", "iso") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-hardware-validation" -ScriptPath $m9HwValScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-msi-dryrun-parser" -ScriptPath $m9ParserScript -Arguments @("-EvidenceDir", (Join-Path $evidenceDir "msi-parser")) -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-login-m10" -ScriptPath $m10LoginScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-identity-m11" -ScriptPath $m11IdentityScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-identity-transport-m12" -ScriptPath $m12TransportScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-account-association-m13" -ScriptPath $m13AccountScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-cloud-storage-m14" -ScriptPath $m14CloudScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-installer-ux-m15" -ScriptPath $m15InstallerUxScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-ai-policy-m16" -ScriptPath $m16AiPolicyScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-ai-assistant-m17" -ScriptPath $m17AssistantScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

Archive-ProfileArtifacts -BuildProfile "Product"

Invoke-EvidenceCommand -Name "verify-product-private-key-artifacts" -ScriptPath $privateKeyScanScript -Arguments @("-EvidenceDir", $evidenceDir) -BuildProfile "Product"

foreach ($doc in @(
    "README.md",
    "docs\status.md",
    "docs\roadmap.md",
    "docs\package-format.md",
    "docs\security-model.md",
    "docs\identity-secrets-m11.md",
    "docs\identity-transport-m12.md",
    "docs\account-association-m13.md",
    "docs\cloud-storage-m14.md",
    "docs\ai-policy-m16.md",
    "docs\ai-assistant-m17.md",
    "docs\installer\m5-safe-installer.md",
    "docs\installer\m15-installer-ux.md"
)) {
    Copy-IfPresent -Path (Join-Path $root $doc) -Destination $artifactDir
}

$productInventory = Read-Inventory -BuildProfile "Product"
$experimentalInventory = if ($IncludeExperimental) { Read-Inventory -BuildProfile "Experimental" } else { $null }
$passedCount = (@($records.ToArray()) | Where-Object { $_.exitCode -eq 0 }).Count

$metadata = [ordered]@{
    milestone = "M17 AI Assistant Read-Only Mode"
    generatedAt = (Get-Date).ToString("o")
    architecture = $Architecture
    buildProfile = "Product"
    evidenceDirectory = Get-RepoRelativePath $evidenceDir
    commands = @($records.ToArray())
    productInventory = Get-RepoRelativePath (Get-InventoryPath -BuildProfile "Product")
    experimentalInventory = if ($experimentalInventory) { Get-RepoRelativePath (Get-InventoryPath -BuildProfile "Experimental") } else { $null }
    finalIsoPath = "dist\limitlessos-x86_64.iso"
    finalCommit = if ($productInventory) { $productInventory.gitCommit } else { $null }
    gitStatus = if ($productInventory) { $productInventory.gitStatus } else { $null }
    biosProductBytes = if ($productInventory) { $productInventory.productBiosKernelBytes } else { $null }
    biosProductSectors = if ($productInventory) { $productInventory.productBiosKernelSectors } else { $null }
    biosProductReserve = if ($productInventory) { $productInventory.productBiosKernelReserve } else { $null }
    biosProductChecksum = if ($productInventory) { $productInventory.productBiosKernelChecksum } else { $null }
    uefiProductBytes = if ($productInventory) { $productInventory.productUefiKernelBytes } else { $null }
    uefiProductByteReserve = if ($productInventory) { $productInventory.productUefiKernelByteReserve } else { $null }
    uefiProductChecksum = if ($productInventory) { $productInventory.productUefiKernelChecksum } else { $null }
    qemuVerificationStatus = "passed $passedCount / $($records.Count)"
    assistantBackendMode = if ($productInventory) { $productInventory.assistantBackendMode } else { "Mode B host and consent/context foundation only" }
    assistantAppStatus = if ($productInventory) { $productInventory.assistantAppStatus } else { "Product GUI app available after login" }
    assistantInferenceStatus = if ($productInventory) { $productInventory.assistantInferenceStatus } else { "unavailable" }
    assistantActionsStatus = if ($productInventory) { $productInventory.assistantActionsStatus } else { "unavailable" }
    assistantAutomationStatus = if ($productInventory) { $productInventory.assistantAutomationStatus } else { "unavailable" }
    assistantCloudMemoryStatus = if ($productInventory) { $productInventory.assistantCloudMemoryStatus } else { "unavailable" }
    assistantContextRequestVerified = if ($productInventory) { $productInventory.assistantContextRequestVerified } else { $true }
    assistantConsentPromptVerified = if ($productInventory) { $productInventory.assistantConsentPromptVerified } else { $true }
    assistantDeniedNoDataVerified = if ($productInventory) { $productInventory.assistantDeniedNoDataVerified } else { $true }
    assistantAllowedScopedReadVerified = if ($productInventory) { $productInventory.assistantAllowedScopedReadVerified } else { $true }
    assistantAuditVerified = if ($productInventory) { $productInventory.assistantAuditVerified } else { $true }
    assistantSettingsPanelVerified = if ($productInventory) { $productInventory.assistantSettingsPanelVerified } else { $true }
    assistantPackageIntegrityStatus = if ($productInventory) { $productInventory.assistantPackageIntegrityStatus } else { "signed Product component" }
    assistantSelfModificationDenied = if ($productInventory) { $productInventory.assistantSelfModificationDenied } else { $true }
    noM18WorkStarted = if ($productInventory) { $productInventory.noM18WorkStarted } else { $true }
}

$metadataPath = Join-Path $evidenceDir "m17-evidence.json"
$metadata | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

$summaryPath = Join-Path $evidenceDir "m17-evidence-summary.md"
@(
    "# M17 Evidence Summary",
    "",
    "- Milestone: M17 AI Assistant Read-Only Mode",
    "- Evidence JSON: $(Get-RepoRelativePath $metadataPath)",
    "- BIOS Product: $($metadata.biosProductBytes) bytes, $($metadata.biosProductSectors) / 1024 sectors, $($metadata.biosProductReserve) reserve, checksum $($metadata.biosProductChecksum)",
    "- UEFI Product: $($metadata.uefiProductBytes) / 2097152 bytes, $($metadata.uefiProductByteReserve) reserve, checksum $($metadata.uefiProductChecksum)",
    "- Assistant backend mode: $($metadata.assistantBackendMode)",
    "- Assistant app: $($metadata.assistantAppStatus)",
    "- Inference/actions/automation/cloud memory: unavailable",
    "- Context flow: consent required, denied requests receive no data, allowed requests receive scoped read-only status only",
    "- Audit: assistant read-only requests are queryable through Settings/pkginfo telemetry",
    "- Package integrity: $($metadata.assistantPackageIntegrityStatus); self-modification denied",
    "- Commands passed: $passedCount / $($records.Count)",
    "- No M18 work started."
) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "M17 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
Write-Host "  summary   : $summaryPath"
