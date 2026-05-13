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
$evidenceDir = Join-Path $distDir "m10-evidence-$timestamp"
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
    return Join-Path $distDir ("limitlessos-x86_64.{0}.m10.json" -f $BuildProfile.ToLowerInvariant())
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

    $stdoutPath = Join-Path $commandDir "$Name.stdout.txt"
    $stderrPath = Join-Path $commandDir "$Name.stderr.txt"
    $outputPath = Join-Path $commandDir "$Name.output.txt"
    $commandText = "& `"$ScriptPath`" $($Arguments -join ' ')"
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
        foreach ($line in Get-Content -LiteralPath $stdoutPath) { $combined.Add($line) }
    }
    if (Test-Path -LiteralPath $stderrPath) {
        foreach ($line in Get-Content -LiteralPath $stderrPath) { $combined.Add($line) }
    }
    $combined | Set-Content -LiteralPath $outputPath -Encoding UTF8

    $inventory = Read-Inventory -BuildProfile $BuildProfile
    $records.Add([pscustomobject][ordered]@{
        name = $Name
        command = $commandText
        exitCode = $process.ExitCode
        elapsedSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        outputFile = Get-RepoRelativePath $outputPath
        stdoutFile = Get-RepoRelativePath $stdoutPath
        stderrFile = Get-RepoRelativePath $stderrPath
        buildProfile = $BuildProfile
        biosKernelBytes = if ($inventory) { $inventory.productBiosKernelBytes } else { $null }
        biosKernelSectors = if ($inventory) { $inventory.productBiosKernelSectors } else { $null }
        biosKernelReserve = if ($inventory) { $inventory.productBiosKernelReserve } else { $null }
        biosKernelChecksum = if ($inventory) { $inventory.productBiosKernelChecksum } else { $null }
        uefiKernelBytes = if ($inventory) { $inventory.productUefiKernelBytes } else { $null }
        uefiKernelByteLimit = if ($inventory) { $inventory.productUefiKernelByteLimit } else { $null }
        uefiKernelByteReserve = if ($inventory) { $inventory.productUefiKernelByteReserve } else { $null }
        uefiKernelChecksum = if ($inventory) { $inventory.productUefiKernelChecksum } else { $null }
        finalIsoPath = if ($inventory -and $inventory.artifacts) { $inventory.artifacts.finalIso } else { "dist\limitlessos-x86_64.iso" }
        artifactInventoryJson = Get-RepoRelativePath (Get-InventoryPath -BuildProfile $BuildProfile)
    })

    if ($process.ExitCode -ne 0) {
        throw "Evidence command '$Name' failed with exit code $($process.ExitCode). See $outputPath"
    }
}

function Archive-ProfileArtifacts
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)
    $profileDir = Join-Path $artifactDir $BuildProfile.ToLowerInvariant()
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    foreach ($milestone in @("m1", "m2", "m3", "m4", "m5", "m6", "m7", "m8", "m9", "m10")) {
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

function Export-HardwareValidationOutput
{
    $statusPath = Join-Path $evidenceDir "hardware-validation-status.txt"
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($file in Get-ChildItem -LiteralPath $commandDir -Filter "*.output.txt") {
        foreach ($line in Get-Content -LiteralPath $file.FullName) {
            if (($line -match 'drs-hwval') -or ($line -match 'hardware validation:') -or ($line -match 'internal writes:') -or ($line -match 'real install approved:') -or ($line -match 'installer dry-run:')) {
                $lines.Add("$($file.Name): $line")
            }
        }
    }
    $lines | Set-Content -LiteralPath $statusPath -Encoding UTF8
    return $statusPath
}

function Update-M10InventoryEvidenceStatus
{
    param([string]$GitStatusPath)
    $inventoryPath = Get-InventoryPath -BuildProfile "Product"
    if (-not (Test-Path -LiteralPath $inventoryPath)) { return }
    $inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
    $inventory.persistenceVerified = $true
    $inventory.hardwareValidationVerifierStatus = "passed"
    $inventory.loginScreenVerified = $true
    $inventory.loginAuthSuccessVerified = $true
    $inventory.wrongPasswordDeniedVerified = $true
    $inventory.rateLimitVerified = $true
    $inventory.sessionLockVerified = $true
    $inventory.sessionUnlockVerified = $true
    $inventory.sessionAuthorityScopedVerified = $true
    $inventory.passwordHashAlgorithm = 'bcrypt $2b$ cost 04 via crypt_blowfish'
    $inventory.msiManualEvidenceStatus = "pending user-provided laptop boot and installer dry-run output"
    $inventory.realInstallApprovalStatus = $false
    $inventory.internalWriteStatus = "disabled by default"
    $inventory | Add-Member -Force -NotePropertyName m10EvidenceDirectory -NotePropertyValue (Get-RepoRelativePath $evidenceDir)
    try {
        $commit = (& git -C $root rev-parse --short HEAD 2>$null)
        if ($LASTEXITCODE -eq 0) { $inventory.gitCommit = $commit.Trim() }
    } catch {
    }
    if (Test-Path -LiteralPath $GitStatusPath) {
        $status = @((Get-Content -LiteralPath $GitStatusPath) | Where-Object { $_ -notmatch '^\?\? dist/' })
        $inventory.gitStatus = if ($status.Count -eq 0) { "clean" } else { ($status -join "; ") }
    }
    $inventory | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $inventoryPath -Encoding Ascii
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
$installerEvidenceDir = Join-Path $evidenceDir "installer"
Invoke-EvidenceCommand -Name "verify-product-installer-m5" -ScriptPath $installerScript -Arguments @("-EvidenceDir", $installerEvidenceDir) -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-negative-fixtures" -ScriptPath $fixtureScript -Arguments @("-Mode", "Package") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-update-index-negative-fixtures" -ScriptPath $fixtureScript -Arguments @("-Mode", "Update") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-ux" -ScriptPath $m8UxScript -Arguments @("-BootMedia", "uefi") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-package-ux-iso" -ScriptPath $m8UxScript -Arguments @("-BootMedia", "iso") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-hardware-validation" -ScriptPath $m9HwValScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-msi-dryrun-parser" -ScriptPath $m9ParserScript -Arguments @("-EvidenceDir", (Join-Path $evidenceDir "msi-parser")) -BuildProfile "Product"
Invoke-EvidenceCommand -Name "verify-product-login-m10" -ScriptPath $m10LoginScript -Arguments @("-BootMedia", "uefi", "-BuildProfile", "Product") -BuildProfile "Product"

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

Update-M10InventoryEvidenceStatus -GitStatusPath $gitStatusPath
Archive-ProfileArtifacts -BuildProfile "Product"
$hardwareStatusPath = Export-HardwareValidationOutput

Invoke-EvidenceCommand -Name "verify-product-private-key-artifacts" -ScriptPath $privateKeyScanScript -Arguments @("-EvidenceDir", $evidenceDir) -BuildProfile "Product"

foreach ($doc in @(
    "README.md",
    "docs\status.md",
    "docs\roadmap.md",
    "docs\package-format.md",
    "docs\security-model.md",
    "docs\service-session-m6.md",
    "docs\installer\m5-safe-installer.md",
    "docs\hardware\msi-cyborg-15-a13ve.md"
)) {
    Copy-IfPresent -Path (Join-Path $root $doc) -Destination $artifactDir
}

$productInventory = Read-Inventory -BuildProfile "Product"
$experimentalInventory = if ($IncludeExperimental) { Read-Inventory -BuildProfile "Experimental" } else { $null }
$passedCount = (@($records.ToArray()) | Where-Object { $_.exitCode -eq 0 }).Count

$metadata = [ordered]@{
    milestone = "M10 User Authentication and Login"
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
    packageTrustStatus = if ($productInventory) { $productInventory.packageManagerUxStatus } else { $null }
    installerDryRunVerifierStatus = "passed"
    hardwareValidationVerifierStatus = "passed"
    hardwareValidationStatusOutput = Get-RepoRelativePath $hardwareStatusPath
    hardwareChecklistStatus = "template archived; physical validation pending"
    msiManualEvidenceStatus = "pending user-provided MSI Cyborg 15 A13VE results"
    loginVerifierStatus = "passed"
    loginScreenStatus = "Product UEFI login screen blocks desktop until authentication succeeds"
    firstRunSetupStatus = if ($productInventory) { $productInventory.firstRunSetupStatus } else { $null }
    localUserStore = if ($productInventory) { $productInventory.localUserStore } else { $null }
    passwordHashAlgorithm = if ($productInventory -and ($productInventory.PSObject.Properties.Name -contains "passwordHashAlgorithm")) { $productInventory.passwordHashAlgorithm } else { 'bcrypt $2b$ cost 04 via crypt_blowfish' }
    sessionLockStatus = "lock command and Settings lock path return to login screen; unlock resumes the session"
    fullMultiuserAuthStatus = "unavailable/non-product"
    machineModel = "pending manual evidence"
    bootMode = "pending manual evidence"
    displayResult = "pending manual evidence"
    keyboardResult = "pending manual evidence"
    mouseTouchpadResult = "pending manual evidence"
    guiResult = "pending manual evidence"
    terminalResult = "pending manual evidence"
    settingsResult = "pending manual evidence"
    pkginfoResult = "pending manual evidence"
    fileManagerResult = "pending manual evidence"
    networkResult = "pending manual evidence"
    nvmeDetectionResult = "pending manual evidence"
    installerDryRunResult = "pending manual evidence"
    internalWriteStatus = "disabled by default"
    forbiddenPartitionDetectionStatus = "verified in QEMU fixtures; MSI dry-run output pending"
    realInstallApprovalStatus = $false
    blockers = @(
        "MSI Cyborg 15 A13VE physical boot evidence not yet supplied",
        "MSI installer dry-run output not yet reviewed",
        "real internal install remains blocked",
        "multiuser account management and password-change UI remain unavailable",
        "package install/apply UX remains unavailable",
        "live public update fetching remains unavailable",
        "trusted-time expiry enforcement remains unavailable"
    )
}

$metadataPath = Join-Path $evidenceDir "m10-evidence.json"
$metadata | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

$summaryPath = Join-Path $evidenceDir "m10-evidence-summary.md"
@(
    "# M10 Evidence Summary",
    "",
    "- Milestone: M10 User Authentication and Login",
    "- Evidence JSON: $(Get-RepoRelativePath $metadataPath)",
    "- BIOS Product: $($metadata.biosProductBytes) bytes, $($metadata.biosProductSectors) / 1024 sectors, $($metadata.biosProductReserve) reserve, checksum $($metadata.biosProductChecksum)",
    "- UEFI Product: $($metadata.uefiProductBytes) / 2097152 bytes, $($metadata.uefiProductByteReserve) reserve, checksum $($metadata.uefiProductChecksum)",
    "- Login verifier: $($metadata.loginVerifierStatus)",
    "- Password hash: $($metadata.passwordHashAlgorithm)",
    "- Session lock/unlock: $($metadata.sessionLockStatus)",
    "- Hardware validation verifier: $($metadata.hardwareValidationVerifierStatus)",
    "- MSI manual evidence: $($metadata.msiManualEvidenceStatus)",
    "- Installer dry-run parser: passed",
    "- Internal writes: $($metadata.internalWriteStatus)",
    "- Real install approved: $($metadata.realInstallApprovalStatus)",
    "- Commands passed: $passedCount / $($records.Count)",
    "- No M11 work started."
) | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host "M10 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
Write-Host "  summary   : $summaryPath"
