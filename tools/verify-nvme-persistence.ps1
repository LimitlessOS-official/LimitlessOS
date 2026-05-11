param(
    [ValidateSet("uefi", "iso")]
    [string]$BootMedia = "uefi",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-QemuEdk2CodePath
{
    $candidates = @(
        "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "QEMU x86_64 UEFI firmware was not found. Expected edk2-x86_64-code.fd in the QEMU share directory."
}

function ConvertTo-ArgumentString
{
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') {
            '"' + ($_.Replace('"', '\"')) + '"'
        }
        else {
            $_
        }
    }) -join ' ')
}

function Normalize-ConsoleLine
{
    param([string]$Line)

    if ($null -eq $Line) {
        return ""
    }

    return ([regex]::Replace($Line, '\x1B\[[0-9;=?]*[A-Za-z]', '')).Trim()
}

function Assert-OutputContains
{
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Message
    )

    $matched = $Lines | Where-Object { $_ -match $Pattern } | Select-Object -First 1
    if (-not $matched) {
        throw "NVMe two-boot verification failed: $Message"
    }
}

function Wait-ForLogPattern
{
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$TimeoutMilliseconds
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path $Path) {
            $matched = Get-Content $Path -ErrorAction SilentlyContinue |
                Where-Object { $_ -match $Pattern } |
                Select-Object -First 1
            if ($matched) {
                return
            }
        }

        Start-Sleep -Milliseconds 100
    }

    throw "NVMe two-boot verification failed: timed out waiting for log marker $Pattern."
}

function Ensure-NvmeGptImage
{
    param([Parameter(Mandatory = $true)][string]$Root)

    $imagePath = Join-Path $Root "dist\limitlessos-x86_64-nvme-gpt.img"
    $generatorPath = Join-Path $Root "tools\generate-nvme-image.ps1"
    $imageBytes = 16777216

    if (-not (Test-Path $generatorPath)) {
        throw "NVMe GPT image generator not found: $generatorPath"
    }

    & $generatorPath -OutputPath $imagePath
    if (-not $?) {
        throw "Failed to generate NVMe GPT image."
    }
    if ((-not (Test-Path $imagePath)) -or ((Get-Item $imagePath).Length -ne $imageBytes)) {
        throw "NVMe GPT image has an unexpected size."
    }

    return $imagePath
}

function Start-NvmeTwoBoot
{
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$QemuPath,
        [Parameter(Mandatory = $true)][string]$FirmwarePath,
        [Parameter(Mandatory = $true)][string]$MediaPath,
        [Parameter(Mandatory = $true)][string]$NvmeImagePath,
        [Parameter(Mandatory = $true)][string]$BootMedia,
        [Parameter(Mandatory = $true)][int]$RunIndex,
        [Parameter(Mandatory = $true)][int]$ExpectedPersisted
    )

    $logStem = "qemu-x86_64-$BootMedia-nvme-persist-$RunIndex"
    $logPath = Join-Path $Root ("build\{0}-debug.log" -f $logStem)
    $serialLogPath = Join-Path $Root ("build\{0}-serial.log" -f $logStem)
    $stderrLogPath = Join-Path $Root ("build\{0}-stderr.log" -f $logStem)

    foreach ($path in @($logPath, $serialLogPath, $stderrLogPath)) {
        if (Test-Path $path) {
            Remove-Item $path -Force
        }
    }

    $arguments = @(
        "-display", "none",
        "-monitor", "none",
        "-no-reboot",
        "-no-shutdown",
        "-serial", "file:$serialLogPath",
        "-debugcon", "file:$logPath",
        "-global", "isa-debugcon.iobase=0xe9",
        "-machine", "q35",
        "-drive", "if=pflash,format=raw,readonly=on,file=$FirmwarePath",
        "-device", "uefi-vars-x64",
        "-drive", "if=none,id=nvmeprobe,format=raw,file=$NvmeImagePath",
        "-device", "nvme,drive=nvmeprobe,serial=LIMITLESSOSNVME,bootindex=3"
    )

    if ($BootMedia -eq "uefi") {
        $uefiAhciIsoPath = Join-Path $Root "dist\limitlessos-x86_64.iso"
        if (-not (Test-Path $uefiAhciIsoPath)) {
            throw "UEFI AHCI ISO sidecar not found: $uefiAhciIsoPath"
        }
        $arguments += @(
            "-device", "qemu-xhci,id=xhci",
            "-drive", "if=none,id=usbstick,format=raw,file=$MediaPath",
            "-device", "usb-storage,bus=xhci.0,drive=usbstick,removable=true,bootindex=1",
            "-drive", "if=none,id=ahciuefi,media=cdrom,format=raw,readonly=on,file=$uefiAhciIsoPath",
            "-device", "ide-cd,drive=ahciuefi,bootindex=2"
        )
    }
    else {
        $arguments += @(
            "-drive", "if=none,id=cdrom,media=cdrom,file=$MediaPath",
            "-device", "ide-cd,drive=cdrom,bootindex=1"
        )
    }

    $argumentLine = ConvertTo-ArgumentString -Arguments $arguments
    $process = Start-Process -FilePath $QemuPath -ArgumentList $argumentLine -PassThru -WindowStyle Hidden -RedirectStandardError $stderrLogPath

    try {
        Wait-ForLogPattern -Path $logPath -Pattern '\[x64\] PIT at 100 Hz' -TimeoutMilliseconds 45000
        Wait-ForLogPattern -Path $logPath -Pattern '\[x64\] drs-nvme-rw' -TimeoutMilliseconds 90000
        Start-Sleep -Seconds 4
    }
    finally {
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }

    if (-not (Test-Path $logPath)) {
        throw "NVMe two-boot verification failed: no debug log was captured for run $RunIndex."
    }

    $outputLines = @(Get-Content $logPath |
        ForEach-Object { Normalize-ConsoleLine -Line $_ } |
        Where-Object { $_.Trim().Length -gt 0 })

    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-load-full 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} .* drs-load-full-source disk .* drs-load-full-unavailable 0' -Message "run $RunIndex did not preserve the AHCI-backed disk launch chain."
    Assert-OutputContains -Lines $outputLines -Pattern ('\[x64\] drs-nvme-rw delegated 1 cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{{8}} wrong-owner 1 stale 1 revoked 1 shell-write 1 shell-readback 1 write-bytes 15 write-checksum 0x46A2678A persisted {0} audit [1-9][0-9]* commits [1-9][0-9]* write-authority 1 commit-authority 1 unavailable 0 error 0' -f $ExpectedPersisted) -Message "run $RunIndex did not report the expected persisted=$ExpectedPersisted scoped NVMe write proof."

    return $outputLines
}

function Assert-PersistenceSecurityEvidence
{
    param(
        [Parameter(Mandatory = $true)][string[]]$Run1Lines,
        [Parameter(Mandatory = $true)][string[]]$Run2Lines,
        [Parameter(Mandatory = $true)][string]$NvmeImagePath
    )

    foreach ($run in @(
        [PSCustomObject]@{ Index = 1; Lines = $Run1Lines; Persisted = 0 },
        [PSCustomObject]@{ Index = 2; Lines = $Run2Lines; Persisted = 1 }
    )) {
        Assert-OutputContains -Lines $run.Lines -Pattern '\[x64\] drs-nvme-fat .* drs-nvme-fat-write-gate 1 .* fs-delegation 0 block-endpoint 0 write-authority 0 commit-authority 0 unavailable 0 error 0' -Message "run $($run.Index) did not preserve the broker-private FAT read/write gate before scoped shell delegation."
        Assert-OutputContains -Lines $run.Lines -Pattern ('\[x64\] drs-nvme-rw delegated 1 cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{{8}} wrong-owner 1 stale 1 revoked 1 shell-write 1 shell-readback 1 write-bytes 15 write-checksum 0x46A2678A persisted {0} audit [1-9][0-9]* commits [1-9][0-9]* write-authority 1 commit-authority 1 unavailable 0 error 0' -f $run.Persisted) -Message "run $($run.Index) did not preserve scoped write authority, commit authority, and denial evidence."
    }

    Assert-OutputContains -Lines $Run1Lines -Pattern '\[x64\] drs-nvme-fat .* drs-nvme-fat-flushes [1-9][0-9]* .* unavailable 0 error 0' -Message "run 1 did not report a nonzero broker-private FAT flush counter."

    if (-not (Test-Path $NvmeImagePath)) {
        throw "NVMe two-boot verification failed: expected reused NVMe image is missing: $NvmeImagePath"
    }

    Write-Output "persistence-security: scoped-write-authority-required 1"
    Write-Output "persistence-security: commit-authority-required 1"
    Write-Output "persistence-security: wrong-owner-denial-observed 1"
    Write-Output "persistence-security: stale-revoked-denial-observed 1"
    Write-Output "persistence-security: read-only-write-denial-observed 1"
    Write-Output "persistence-security: nonzero-commit-flush-counter 1"
    Write-Output ("persistence-security: same-nvme-image-reused 1 path {0}" -f $NvmeImagePath)
    Write-Output "persistence-security: not-ram-backed 1"
    Write-Output "persistence-security: exit-code 0"
}

$root = Split-Path -Parent $PSScriptRoot
$mediaPath = if ($BootMedia -eq "iso") {
    Join-Path $root "dist\limitlessos-x86_64.iso"
}
else {
    Join-Path $root "dist\limitlessos-x86_64-uefi.img"
}

if (-not (Test-Path $mediaPath)) {
    throw "Build media not found. Run .\tools\build.ps1 -Architecture x86_64 first."
}

$qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
if (-not $qemu) {
    $fallbackPath = "C:\Program Files\qemu\qemu-system-x86_64.exe"
    if (Test-Path $fallbackPath) {
        $qemu = @{ Source = $fallbackPath }
    }
}
if (-not $qemu) {
    throw "QEMU is not installed or not on PATH."
}

$firmwarePath = Get-QemuEdk2CodePath
$nvmeImagePath = Ensure-NvmeGptImage -Root $root

$run1Lines = @(Start-NvmeTwoBoot -Root $root -QemuPath $qemu.Source -FirmwarePath $firmwarePath -MediaPath $mediaPath -NvmeImagePath $nvmeImagePath -BootMedia $BootMedia -RunIndex 1 -ExpectedPersisted 0)
$run2Lines = @(Start-NvmeTwoBoot -Root $root -QemuPath $qemu.Source -FirmwarePath $firmwarePath -MediaPath $mediaPath -NvmeImagePath $nvmeImagePath -BootMedia $BootMedia -RunIndex 2 -ExpectedPersisted 1)

Assert-PersistenceSecurityEvidence -Run1Lines $run1Lines -Run2Lines $run2Lines -NvmeImagePath $nvmeImagePath

$m2InventoryPath = Join-Path $root ("dist\limitlessos-x86_64.{0}.m2.json" -f $BuildProfile.ToLowerInvariant())
if (Test-Path $m2InventoryPath) {
    $m2Inventory = Get-Content -Path $m2InventoryPath -Raw | ConvertFrom-Json
    $persistenceEvidence = [PSCustomObject]@{
        scopedWriteAuthorityRequired = $true
        commitAuthorityRequired = $true
        wrongOwnerDenialObserved = $true
        staleRevokedDenialObserved = $true
        readOnlyWriteDenialObserved = $true
        nonzeroCommitFlushCounter = $true
        sameNvmeImageReused = $true
        notRamBacked = $true
    }
    $inventoryUpdates = @{
        persistenceVerified = $true
        persistenceBootMedia = $BootMedia
        persistenceEvidence = $persistenceEvidence
    }
    foreach ($entry in $inventoryUpdates.GetEnumerator()) {
        if ($m2Inventory.PSObject.Properties.Name -contains $entry.Key) {
            $m2Inventory.$($entry.Key) = $entry.Value
        } else {
            $m2Inventory | Add-Member -NotePropertyName $entry.Key -NotePropertyValue $entry.Value
        }
    }
    $m2Inventory | ConvertTo-Json -Depth 8 | Set-Content -Path $m2InventoryPath -Encoding Ascii
}

Write-Output ("NVMe FAT32 shell persistence proof passed on x86_64 {0}: first boot wrote SHELL.TXT, second boot observed persisted content on the same NVMe image." -f $BootMedia)
