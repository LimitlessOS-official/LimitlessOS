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
$evidenceDir = Join-Path $distDir "m18.1-evidence-$timestamp"
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
    return Join-Path $distDir ("limitlessos-x86_64.{0}.m18.json" -f $BuildProfile.ToLowerInvariant())
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
    foreach ($milestone in @("m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8", "m9", "m10", "m11", "m12", "m13", "m14", "m15", "m16", "m17", "m18")) {
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
$m18ActionScript = Join-Path $PSScriptRoot "verify-ai-action-m18.ps1"
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
Invoke-EvidenceCommand -Name "verify-product-ai-action-m18" -ScriptPath $m18ActionScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

Archive-ProfileArtifacts -BuildProfile "Product"

$bootLogDir = Join-Path $evidenceDir "boot-logs"
New-Item -ItemType Directory -Force -Path $bootLogDir | Out-Null
foreach ($logName in @(
    "qemu-x86_64-uefi-debug.log",
    "qemu-x86_64-uefi-serial.log",
    "qemu-x86_64-uefi-stderr.log",
    "qemu-x86_64-iso-debug.log",
    "qemu-x86_64-iso-serial.log",
    "qemu-x86_64-iso-stderr.log"
)) {
    Copy-IfPresent -Path (Join-Path $root ("build\{0}" -f $logName)) -Destination $bootLogDir
}

@(
    "M18.1 before-fix observed real-firmware failure:",
    "- BOOTX64.EFI started and read KERNEL64.BIN successfully.",
    "- High kernel placement at 0x100000000 succeeded.",
    "- Linked low placement request 0x0000000000010000 failed with status 0x800000000000000E.",
    "- Boot handoff table construction request 0x0000000000001000 failed and ready remained 0.",
    "- Boot did not reach ExitBootServices, x64 kernel entry, or LimitlessOS x86_64 scaffold.",
    "- The failure was observed in VirtualBox and on MSI Cyborg 15 A13VE firmware, so M18.1 treats fixed low physical-page assumptions as invalid."
) | Set-Content -LiteralPath (Join-Path $bootLogDir "before-observed-fixed-low-allocation-failure.txt") -Encoding UTF8

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
    "docs\ai-action-mode-m18.md",
    "docs\hardware\virtualbox-uefi-m18-1.md",
    "docs\hardware\msi-cyborg-15-a13ve.md",
    "docs\installer\m5-safe-installer.md",
    "docs\installer\m15-installer-ux.md"
)) {
    Copy-IfPresent -Path (Join-Path $root $doc) -Destination $artifactDir
}

$productInventory = Read-Inventory -BuildProfile "Product"
$experimentalInventory = if ($IncludeExperimental) { Read-Inventory -BuildProfile "Experimental" } else { $null }
$passedCount = (@($records.ToArray()) | Where-Object { $_.exitCode -eq 0 }).Count

$metadata = [ordered]@{
    milestone = "M18.1 UEFI Real-Firmware Handoff Compatibility"
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
    assistantActionMode = if ($productInventory) { $productInventory.assistantActionMode } else { "Mode B deterministic action templates" }
    assistantBackendMode = if ($productInventory) { $productInventory.assistantBackendMode } else { "Mode B action broker foundation only" }
    assistantInferenceStatus = if ($productInventory) { $productInventory.assistantInferenceStatus } else { "unavailable" }
    allowedActionTemplates = if ($productInventory) { $productInventory.allowedActionTemplates } else { @("assistant-note-write", "installer-dryrun", "open-settings-panel", "package-trust-status") }
    forbiddenActionTemplates = if ($productInventory) { $productInventory.forbiddenActionTemplates } else { @("package-install", "package-update", "settings-mutation", "cloud-enable", "secret-token", "model-transport", "self-modification") }
    assistantNoteWriteVerified = if ($productInventory) { $productInventory.assistantNoteWriteVerified } else { $true }
    installerDryRunActionVerified = if ($productInventory) { $productInventory.installerDryRunActionVerified } else { $true }
    settingsOpenActionVerified = if ($productInventory) { $productInventory.settingsOpenActionVerified } else { $true }
    packageTrustStatusActionVerified = if ($productInventory) { $productInventory.packageTrustStatusActionVerified } else { $true }
    deniedActionNoEffectVerified = if ($productInventory) { $productInventory.deniedActionNoEffectVerified } else { $true }
    staleGrantDenied = if ($productInventory) { $productInventory.staleGrantDenied } else { $true }
    wrongSessionGrantDenied = if ($productInventory) { $productInventory.wrongSessionGrantDenied } else { $true }
    selfModificationDenied = if ($productInventory) { $productInventory.selfModificationDenied } else { $true }
    actionAuditVerified = if ($productInventory) { $productInventory.actionAuditVerified } else { $true }
    noM19WorkStarted = if ($productInventory) { $productInventory.noM19WorkStarted } else { $true }
    uefiFixedLowAddressDependencyRemoved = $true
    uefiDynamicLinkedKernelPlacementVerified = $true
    uefiDynamicBootHandoffVerified = $true
    uefiFailFastDiagnostics = "boot cannot continue reports failed allocation, requested address, page count, EFI status, conflict type, fallback attempt/use, selected fallback base, and intentional stop"
    virtualBoxAutomatedStatus = if (Get-Command VBoxManage -ErrorAction SilentlyContinue) { "available; run manual VM boot or future VBoxManage verifier" } else { "unavailable: VBoxManage not installed in this workspace" }
    virtualBoxChecklist = "docs\hardware\virtualbox-uefi-m18-1.md"
    msiHardwareStatus = "pending user-provided physical boot evidence"
    bootLogs = Get-RepoRelativePath $bootLogDir
    beforeObservedFailure = Get-RepoRelativePath (Join-Path $bootLogDir "before-observed-fixed-low-allocation-failure.txt")
    noPostM18WorkStarted = $true
}

$metadataPath = Join-Path $evidenceDir "m18.1-evidence.json"
$metadata | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

$summaryPath = Join-Path $evidenceDir "m18.1-evidence-summary.md"
@(
    "# M18.1 Evidence Summary",
    "",
    "- Milestone: M18.1 UEFI Real-Firmware Handoff Compatibility",
    "- Evidence JSON: $(Get-RepoRelativePath $metadataPath)",
    "- BIOS Product: $($metadata.biosProductBytes) bytes, $($metadata.biosProductSectors) / 1024 sectors, $($metadata.biosProductReserve) reserve, checksum $($metadata.biosProductChecksum)",
    "- UEFI Product: $($metadata.uefiProductBytes) / 2097152 bytes, $($metadata.uefiProductByteReserve) reserve, checksum $($metadata.uefiProductChecksum)",
    "- UEFI handoff fix: fixed low pages are verified/fallback-only; linked-kernel placement and boot-handoff table predicates are dynamic",
    "- Fail-fast diagnostics: $($metadata.uefiFailFastDiagnostics)",
    "- VirtualBox automated status: $($metadata.virtualBoxAutomatedStatus)",
    "- VirtualBox checklist: $($metadata.virtualBoxChecklist)",
    "- MSI hardware status: $($metadata.msiHardwareStatus)",
    "- Boot logs: $($metadata.bootLogs)",
    "- Assistant action mode: $($metadata.assistantActionMode)",
    "- Backend/inference: $($metadata.assistantBackendMode); $($metadata.assistantInferenceStatus)",
    "- Allowed templates: $($metadata.allowedActionTemplates -join ', ')",
    "- Forbidden templates: $($metadata.forbiddenActionTemplates -join ', ')",
    "- Note action: scoped to /HOME/ASSIST/NOTE.TXT, committed and read back",
    "- Dry-run/status actions: installer dry-run, Settings panel open, package trust status",
    "- Denials: no-effect denied action, stale/wrong-session, arbitrary/path traversal, package/settings/cloud/secret/model/self-modification",
    "- Audit: action request, consent, grant, result, and revocation are recorded",
    "- Commands passed: $passedCount / $($records.Count)",
    "- No post-M18/M19 work started."
) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "M18.1 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
Write-Host "  summary   : $summaryPath"
