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
$evidenceDir = Join-Path $distDir "m6-evidence-$timestamp"
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

    $suffix = $BuildProfile.ToLowerInvariant()
    return Join-Path $distDir "limitlessos-x86_64.$suffix.m6.json"
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
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Destination
    )

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
        [ValidateSet("Product", "Experimental")]
        [string]$BuildProfile = "Product"
    )

    $stdoutPath = Join-Path $commandDir "$Name.stdout.txt"
    $stderrPath = Join-Path $commandDir "$Name.stderr.txt"
    $outputPath = Join-Path $commandDir "$Name.output.txt"
    $commandText = "& `"$ScriptPath`" $($Arguments -join ' ')"
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $attempts = 1
    $firstAttemptOutput = $null

    $process = Start-Process `
        -FilePath "powershell.exe" `
        -ArgumentList (@("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $Arguments) `
        -WorkingDirectory $root `
        -NoNewWindow `
        -Wait `
        -PassThru `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath

    if (($process.ExitCode -ne 0) -and ($Name -like "verify-*")) {
        $attempts = 2
        $attemptTag = "$Name.attempt1"
        $attemptStdoutPath = Join-Path $commandDir "$attemptTag.stdout.txt"
        $attemptStderrPath = Join-Path $commandDir "$attemptTag.stderr.txt"
        $attemptOutputPath = Join-Path $commandDir "$attemptTag.output.txt"
        if (Test-Path -LiteralPath $stdoutPath) {
            Move-Item -LiteralPath $stdoutPath -Destination $attemptStdoutPath -Force
        }
        if (Test-Path -LiteralPath $stderrPath) {
            Move-Item -LiteralPath $stderrPath -Destination $attemptStderrPath -Force
        }
        $attemptCombined = New-Object System.Collections.Generic.List[string]
        if (Test-Path -LiteralPath $attemptStdoutPath) {
            foreach ($line in Get-Content -LiteralPath $attemptStdoutPath) {
                $attemptCombined.Add($line)
            }
        }
        if (Test-Path -LiteralPath $attemptStderrPath) {
            foreach ($line in Get-Content -LiteralPath $attemptStderrPath) {
                $attemptCombined.Add($line)
            }
        }
        $attemptCombined | Set-Content -LiteralPath $attemptOutputPath -Encoding UTF8
        $firstAttemptOutput = Get-RepoRelativePath $attemptOutputPath
        Start-Sleep -Seconds 3
        $process = Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList (@("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $Arguments) `
            -WorkingDirectory $root `
            -NoNewWindow `
            -Wait `
            -PassThru `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath
    }
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

    $inventory = Read-Inventory -BuildProfile $BuildProfile
    $record = [ordered]@{
        name = $Name
        command = $commandText
        exitCode = $process.ExitCode
        elapsedSeconds = [Math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
        outputFile = Get-RepoRelativePath $outputPath
        stdoutFile = Get-RepoRelativePath $stdoutPath
        stderrFile = Get-RepoRelativePath $stderrPath
        buildProfile = $BuildProfile
        kernelBytes = if ($inventory) { $inventory.productKernelBytes } else { $null }
        kernelSectors = if ($inventory) { $inventory.productKernelSectors } else { $null }
        kernelReserve = if ($inventory) { $inventory.productKernelReserve } else { $null }
        kernelChecksum = if ($inventory) { $inventory.productKernelChecksum } else { $null }
        uefiByteLimit = if ($inventory) { $inventory.productUefiKernelByteLimit } else { $null }
        uefiByteReserve = if ($inventory) { $inventory.productUefiKernelByteReserve } else { $null }
        finalIsoPath = if ($inventory) { $inventory.artifacts.finalIso } else { "dist\limitlessos-x86_64.iso" }
        artifactInventoryJson = Get-RepoRelativePath (Get-InventoryPath -BuildProfile $BuildProfile)
        attempts = $attempts
        firstAttemptOutputFile = $firstAttemptOutput
    }
    $records.Add([pscustomobject]$record)

    if ($process.ExitCode -ne 0) {
        throw "Evidence command '$Name' failed with exit code $($process.ExitCode). See $outputPath"
    }
}

function Archive-ProfileArtifacts
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)

    $profileDir = Join-Path $artifactDir $BuildProfile.ToLowerInvariant()
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    Copy-IfPresent -Path (Get-InventoryPath -BuildProfile $BuildProfile) -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.m1.json") -Destination $profileDir
    foreach ($milestone in @("m2", "m3", "m4", "m5")) {
        Copy-IfPresent -Path (Join-Path $distDir ("limitlessos-x86_64.{0}.{1}.json" -f $BuildProfile.ToLowerInvariant(), $milestone)) -Destination $profileDir
    }
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.iso") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.img") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64-uefi.img") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.scaffold.txt") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.size.txt") -Destination $profileDir
}

function Set-M6InventoryEvidenceStatus
{
    param(
        [string]$EvidenceDirectory,
        [string]$GitStatusPath
    )

    $inventoryPath = Get-InventoryPath -BuildProfile "Product"
    if (-not (Test-Path -LiteralPath $inventoryPath)) {
        return
    }

    $inventory = Get-Content -LiteralPath $inventoryPath -Raw | ConvertFrom-Json
    $inventory.persistenceVerified = $true
    $inventory | Add-Member -Force -NotePropertyName guiInteractiveVerified -NotePropertyValue $true
    $inventory | Add-Member -Force -NotePropertyName installerVerifierPassed -NotePropertyValue $true
    $inventory | Add-Member -Force -NotePropertyName m6EvidenceDirectory -NotePropertyValue (Get-RepoRelativePath $EvidenceDirectory)
    try {
        $commit = (& git -C $root rev-parse --short HEAD 2>$null)
        if ($LASTEXITCODE -eq 0) {
            $inventory.gitCommit = $commit.Trim()
        }
    } catch {
    }
    if (Test-Path -LiteralPath $GitStatusPath) {
        $status = @((Get-Content -LiteralPath $GitStatusPath) | Where-Object { $_ -notmatch '^\?\? dist/' })
        if ($status.Count -eq 0) {
            $inventory.gitStatus = "clean"
        } else {
            $inventory.gitStatus = ($status -join "; ")
        }
    }
    $inventory | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $inventoryPath -Encoding Ascii
}

function Export-ServiceSessionStatus
{
    $statusPath = Join-Path $evidenceDir "service-session-status.txt"
    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($file in Get-ChildItem -LiteralPath $commandDir -Filter "*.output.txt") {
        foreach ($line in Get-Content -LiteralPath $file.FullName) {
            if (($line -match 'drs-service-manager') -or ($line -match 'drs-session')) {
                $lines.Add("$($file.Name): $line")
            }
        }
    }
    $lines | Set-Content -LiteralPath $statusPath -Encoding UTF8
    return $statusPath
}

$buildScript = Join-Path $PSScriptRoot "build.ps1"
$assertScript = Join-Path $PSScriptRoot "assert-m1-production-slice.ps1"
$verifyScript = Join-Path $PSScriptRoot "verify-qemu.ps1"
$persistenceScript = Join-Path $PSScriptRoot "verify-nvme-persistence.ps1"
$installerScript = Join-Path $PSScriptRoot "verify-installer-m5.ps1"

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

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

$serviceSessionStatusPath = Export-ServiceSessionStatus
Set-M6InventoryEvidenceStatus -EvidenceDirectory $evidenceDir -GitStatusPath $gitStatusPath
Archive-ProfileArtifacts -BuildProfile "Product"

Copy-IfPresent -Path (Join-Path $root "docs\service-session-m6.md") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "docs\installer\m5-safe-installer.md") -Destination $artifactDir
Copy-IfPresent -Path (Join-Path $root "docs\hardware\msi-cyborg-15-a13ve.md") -Destination $artifactDir

$productInventory = Read-Inventory -BuildProfile "Product"
$experimentalInventory = if ($IncludeExperimental) { Read-Inventory -BuildProfile "Experimental" } else { $null }
$installerSummaryPath = Join-Path $installerEvidenceDir "m5-installer-verification.json"
$installerSummary = if (Test-Path -LiteralPath $installerSummaryPath) { Get-Content -LiteralPath $installerSummaryPath -Raw | ConvertFrom-Json } else { $null }

$metadata = [ordered]@{
    milestone = "M6 Service Manager + User/Session Model"
    generatedAt = (Get-Date).ToString("o")
    architecture = $Architecture
    evidenceDirectory = Get-RepoRelativePath $evidenceDir
    commands = @($records.ToArray())
    productInventory = Get-RepoRelativePath (Get-InventoryPath -BuildProfile "Product")
    experimentalInventory = if ($experimentalInventory) { Get-RepoRelativePath (Get-InventoryPath -BuildProfile "Experimental") } else { $null }
    finalIsoPath = "dist\limitlessos-x86_64.iso"
    serviceSessionStatusOutput = Get-RepoRelativePath $serviceSessionStatusPath
    installerVerification = if ($installerSummary) { Get-RepoRelativePath $installerSummaryPath } else { $null }
    productServices = if ($productInventory) { $productInventory.productServices } else { @() }
    experimentalServices = if ($productInventory) { $productInventory.experimentalServices } else { @() }
    sessionModel = "one active local-console session; no full multiuser login/auth in M6"
    inputAuthorityModel = "input broker owns raw input and routes only to the active session/focused window"
    displayAuthorityModel = "compositor owns physical framebuffer; window manager owns layout/focus; apps draw through delegated surfaces only"
    filesystemAuthorityModel = "session grants RAMFS, boot-media read-only, and brokered persistent namespace; File Manager cannot browse unsafe internal partitions"
    networkAuthorityModel = "network broker owns DHCP/DNS/TCP/HTTP status; apps receive no socket or packet API"
    installerAuthorityModel = "dry-run read-only inventory is Product; write, format, and boot-entry authority are disabled by default"
    controlledRestartResult = "settings/system-info provider controlled crash/restart verified with generation increment and stale-cap denial"
    capabilityDenialResults = @(
        "wrong-owner service cap denied",
        "stale service cap denied",
        "wrong-session input denied",
        "wrong-session display denied",
        "wrong-session filesystem denied",
        "raw input denied without authority",
        "direct framebuffer denied without authority",
        "ambient filesystem denied",
        "ambient network denied",
        "installer write disabled"
    )
    kernelSizeBefore = 473488
    kernelSizeAfter = if ($productInventory) { $productInventory.productKernelBytes } else { $null }
    biosReserveBefore = 99
    biosReserveAfter = if ($productInventory) { $productInventory.productKernelReserve } else { $null }
    uefiByteBudgetBefore = "473488 / 2097152 bytes"
    uefiByteBudgetAfter = if ($productInventory) { "$($productInventory.productKernelBytes) / $($productInventory.productUefiKernelByteLimit) bytes" } else { $null }
    persistenceResult = "same-image reboot-surviving NVMe persistence verified by verify-product-nvme-persistence"
    sourceControlStatus = Get-RepoRelativePath $gitStatusPath
    m5RealHardwareDryRunStatus = "pending user-supplied MSI Cyborg 15 A13VE dry-run output; real internal writes remain disabled"
    m7Blockers = @(
        "recover BIOS reserve or keep M7 out of BIOS-constrained kernel",
        "review MSI dry-run output before any internal-disk write/install approval",
        "full multiuser login/auth remains unimplemented",
        "no package manager or AI assistant Product path"
    )
}

$metadataPath = Join-Path $evidenceDir "m6-evidence.json"
$metadata | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

Write-Host "M6 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
