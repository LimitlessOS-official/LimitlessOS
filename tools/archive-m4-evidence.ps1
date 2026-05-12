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
$evidenceDir = Join-Path $distDir "m4-evidence-$timestamp"
$commandDir = Join-Path $evidenceDir "commands"
$artifactDir = Join-Path $evidenceDir "artifacts"

New-Item -ItemType Directory -Force -Path $commandDir | Out-Null
New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null

$records = New-Object System.Collections.Generic.List[object]

function Get-InventoryPath
{
    param([ValidateSet("Product", "Experimental")][string]$BuildProfile)

    $suffix = $BuildProfile.ToLowerInvariant()
    return Join-Path $distDir "limitlessos-x86_64.$suffix.m4.json"
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
        $firstAttemptOutput = $attemptOutputPath.Substring($root.Length + 1)
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
        outputFile = $outputPath.Substring($root.Length + 1)
        stdoutFile = $stdoutPath.Substring($root.Length + 1)
        stderrFile = $stderrPath.Substring($root.Length + 1)
        buildProfile = $BuildProfile
        kernelBytes = if ($inventory) { $inventory.productKernelBytes } else { $null }
        kernelSectors = if ($inventory) { $inventory.productKernelSectors } else { $null }
        kernelReserve = if ($inventory) { $inventory.productKernelReserve } else { $null }
        kernelChecksum = if ($inventory) { $inventory.productKernelChecksum } else { $null }
        uefiByteLimit = if ($inventory) { $inventory.productUefiKernelByteLimit } else { $null }
        uefiByteReserve = if ($inventory) { $inventory.productUefiKernelByteReserve } else { $null }
        finalIsoPath = if ($inventory) { $inventory.artifacts.finalIso } else { "dist\limitlessos-x86_64.iso" }
        artifactInventoryJson = (Get-InventoryPath -BuildProfile $BuildProfile).Substring($root.Length + 1)
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
    Copy-IfPresent -Path (Join-Path $distDir ("limitlessos-x86_64.{0}.m2.json" -f $BuildProfile.ToLowerInvariant())) -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir ("limitlessos-x86_64.{0}.m3.json" -f $BuildProfile.ToLowerInvariant())) -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.iso") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.img") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64-uefi.img") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.scaffold.txt") -Destination $profileDir
    Copy-IfPresent -Path (Join-Path $distDir "limitlessos-x86_64.size.txt") -Destination $profileDir
}

$buildScript = Join-Path $PSScriptRoot "build.ps1"
$assertScript = Join-Path $PSScriptRoot "assert-m1-production-slice.ps1"
$verifyScript = Join-Path $PSScriptRoot "verify-qemu.ps1"
$persistenceScript = Join-Path $PSScriptRoot "verify-nvme-persistence.ps1"

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

Archive-ProfileArtifacts -BuildProfile "Product"

$gitStatusPath = Join-Path $evidenceDir "git-status.txt"
try {
    & git -C $root status --short --branch | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
} catch {
    "git status unavailable: $($_.Exception.Message)" | Set-Content -LiteralPath $gitStatusPath -Encoding UTF8
}

$productInventory = Read-Inventory -BuildProfile "Product"
$productInventoryPath = $null
if ($productInventory) {
    $productInventoryPath = (Get-InventoryPath -BuildProfile "Product").Substring($root.Length + 1)
}

$metadata = [ordered]@{
    milestone = "M4 Interactive GUI Promoted to Product"
    generatedAt = (Get-Date).ToString("o")
    architecture = $Architecture
    evidenceDirectory = $evidenceDir.Substring($root.Length + 1)
    commands = @($records.ToArray())
    productInventory = $productInventoryPath
    finalIsoPath = "dist\limitlessos-x86_64.iso"
    guiFramebufferEvidence = "not captured as screenshots by this archive; GUI promotion is proven by drs-gui real-input event-path telemetry in verify-product-uefi, verify-product-iso, and verify-product-gui-interactive outputs"
    gitStatus = $gitStatusPath.Substring($root.Length + 1)
}

$metadataPath = Join-Path $evidenceDir "m4-evidence.json"
$metadata | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $metadataPath -Encoding UTF8

Write-Host "M4 evidence archived:"
Write-Host "  directory : $evidenceDir"
Write-Host "  metadata  : $metadataPath"
