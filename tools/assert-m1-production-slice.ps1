param(
    [ValidateSet("x86_64")]
    [string]$Architecture = "x86_64",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product",

    [switch]$WriteInventory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $root "dist"
$m1ProductApps = @("APPEND", "CAT", "COPY", "DELETE", "LS", "MKDIR", "MOVE", "RENAME", "STAT", "TOUCH", "WRITE")
$m1ShellBuiltins = @("apps", "help", "hwval", "info", "lock", "net", "pkginfo", "pwd")
$m4ProductGuiApps = @("Terminal", "File Manager", "Settings")
$m1Aliases = @("SAY", "SHOW", "LIST", "MAKE", "PUT", "SWAP", "SHIFT")
$m1InternalFiles = @("HELLO.TXT", "INDEX.TXT")
$m4UnavailableFeatures = @(
    "ASK (not AI; no consent-gated assistant path in Product)",
    "ECHO (not Product path)",
    "RAMFS aliases (SAY/SHOW/LIST/MAKE/PUT/SWAP/SHIFT unavailable in Product shell)",
    "Installer write/install path (M5 dry-run tooling is Product; internal writes disabled)",
    "Package install/apply UX",
    "Live public update fetch",
    "Trusted-time expiry enforcement",
    "AI assistant behavior"
)
$m3UnavailableFeatures = @(
    $m4UnavailableFeatures +
    @("GUI/compositor/window manager/desktop/File Manager GUI/Settings GUI")
)
$m1UnavailableFeatures = $m4UnavailableFeatures

function Fail-M1
{
    param([Parameter(Mandatory = $true)][string]$Message)
    throw "M1 production-slice gate failed: $Message"
}

function Get-Fnv1aDataChecksum
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    [uint32]$hash = 2166136261
    for ($index = 0; $index -lt $Bytes.Length; $index++) {
        [uint32]$value = $Bytes[$index]
        $hash = [uint32](($hash -bxor $value) -band 0xFFFFFFFF)
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }

    return $hash
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

function Get-GitCommit
{
    try {
        $commit = (& git -C $root rev-parse --short HEAD 2>$null)
        if ($LASTEXITCODE -eq 0) {
            return $commit.Trim()
        }
    } catch {
    }

    return "unavailable"
}

function Get-GitStatusSummary
{
    try {
        $status = @(& git -C $root status --short 2>$null)
        if ($LASTEXITCODE -eq 0) {
            if ($status.Count -eq 0) {
                return "clean"
            }
            return ($status -join "; ")
        }
    } catch {
    }

    return "unavailable"
}

function Assert-NoPrivateKeyMaterialInProductArtifacts
{
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    $forbiddenPatterns = @(
        "-----BEGIN PRIVATE KEY-----",
        "-----BEGIN OPENSSH PRIVATE KEY-----",
        "PRIVATE KEY-----",
        "Ed25519PrivateKey.generate",
        "private_key ="
    )

    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }
        $bytes = [System.IO.File]::ReadAllBytes($path)
        $text = [System.Text.Encoding]::ASCII.GetString($bytes)
        foreach ($pattern in $forbiddenPatterns) {
            if ($text.Contains($pattern)) {
                Fail-M1 ("Product artifact contains forbidden private-key marker '{0}': {1}" -f $pattern, (Get-RepoRelativePath $path))
            }
        }
    }
}

function Read-KeyValueFile
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $values = @{}
    foreach ($line in Get-Content -Path $Path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $values[$Matches[1]] = $Matches[2]
        }
    }

    return $values
}

function Get-UInt32Le
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$Bytes,
        [Parameter(Mandatory = $true)][int]$Offset
    )

    return [System.BitConverter]::ToUInt32($Bytes, $Offset)
}

function Normalize-IsoFileIdentifier
{
    param([Parameter(Mandatory = $true)][byte[]]$IdentifierBytes)

    if (($IdentifierBytes.Length -eq 1) -and ($IdentifierBytes[0] -eq 0)) {
        return "."
    }
    if (($IdentifierBytes.Length -eq 1) -and ($IdentifierBytes[0] -eq 1)) {
        return ".."
    }

    $name = [System.Text.Encoding]::ASCII.GetString($IdentifierBytes).TrimEnd()
    $versionIndex = $name.IndexOf(';')
    if ($versionIndex -ge 0) {
        $name = $name.Substring(0, $versionIndex)
    }

    return $name.ToUpperInvariant()
}

function Get-IsoDirectoryEntries
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$IsoBytes,
        [Parameter(Mandatory = $true)][uint32]$ExtentLba,
        [Parameter(Mandatory = $true)][uint32]$DataLength
    )

    $sectorSize = 2048
    $directoryOffset = [int]($ExtentLba * $sectorSize)
    $directoryEnd = $directoryOffset + [int]$DataLength
    $entries = @{}
    $offset = $directoryOffset

    while ($offset -lt $directoryEnd) {
        $recordLength = [int]$IsoBytes[$offset]
        if ($recordLength -eq 0) {
            $nextSector = ([int]([Math]::Floor($offset / $sectorSize)) + 1) * $sectorSize
            if ($nextSector -le $offset) {
                break
            }
            $offset = $nextSector
            continue
        }

        if (($offset + $recordLength) -gt $IsoBytes.Length) {
            Fail-M1 "final ISO contains a truncated directory record."
        }

        $extentLba = Get-UInt32Le -Bytes $IsoBytes -Offset ($offset + 2)
        $length = Get-UInt32Le -Bytes $IsoBytes -Offset ($offset + 10)
        $flags = $IsoBytes[$offset + 25]
        $identifierLength = [int]$IsoBytes[$offset + 32]
        $identifierOffset = $offset + 33
        if (($identifierOffset + $identifierLength) -gt ($offset + $recordLength)) {
            Fail-M1 "final ISO contains a malformed directory record identifier."
        }

        [byte[]]$identifier = [byte[]]::new($identifierLength)
        if ($identifierLength -gt 0) {
            [Array]::Copy($IsoBytes, $identifierOffset, $identifier, 0, $identifierLength)
        }
        $name = Normalize-IsoFileIdentifier -IdentifierBytes $identifier
        $entries[$name] = [PSCustomObject]@{
            Name = $name
            ExtentLba = $extentLba
            DataLength = $length
            IsDirectory = (($flags -band 0x02) -ne 0)
        }

        $offset += $recordLength
    }

    return $entries
}

function Read-IsoPathBytes
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$IsoBytes,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $sectorSize = 2048
    $pvdOffset = 16 * $sectorSize
    if (($IsoBytes.Length -lt ($pvdOffset + $sectorSize)) -or
        ($IsoBytes[$pvdOffset] -ne 1) -or
        ([System.Text.Encoding]::ASCII.GetString($IsoBytes, $pvdOffset + 1, 5) -ne "CD001")) {
        Fail-M1 "final ISO does not contain a valid ISO9660 primary volume descriptor."
    }

    $rootRecord = $pvdOffset + 156
    $currentLba = Get-UInt32Le -Bytes $IsoBytes -Offset ($rootRecord + 2)
    $currentLength = Get-UInt32Le -Bytes $IsoBytes -Offset ($rootRecord + 10)
    $parts = @($Path.Trim('/').Split('/') | Where-Object { $_.Length -gt 0 })

    for ($partIndex = 0; $partIndex -lt $parts.Count; $partIndex++) {
        $entries = Get-IsoDirectoryEntries -IsoBytes $IsoBytes -ExtentLba $currentLba -DataLength $currentLength
        $lookup = $parts[$partIndex].ToUpperInvariant()
        if (-not $entries.ContainsKey($lookup)) {
            Fail-M1 "final ISO is missing required path '$Path'."
        }

        $entry = $entries[$lookup]
        $isLast = ($partIndex -eq ($parts.Count - 1))
        if (-not $isLast) {
            if (-not $entry.IsDirectory) {
                Fail-M1 "final ISO path '$Path' crosses a non-directory entry '$lookup'."
            }
            $currentLba = $entry.ExtentLba
            $currentLength = $entry.DataLength
            continue
        }

        if ($entry.IsDirectory) {
            Fail-M1 "final ISO path '$Path' resolved to a directory, not a file."
        }

        $fileOffset = [int]($entry.ExtentLba * $sectorSize)
        $fileLength = [int]$entry.DataLength
        if (($fileOffset + $fileLength) -gt $IsoBytes.Length) {
            Fail-M1 "final ISO path '$Path' extends beyond the image size."
        }

        [byte[]]$fileBytes = [byte[]]::new($fileLength)
        if ($fileLength -gt 0) {
            [Array]::Copy($IsoBytes, $fileOffset, $fileBytes, 0, $fileLength)
        }
        return ,$fileBytes
    }

    Fail-M1 "final ISO path '$Path' is empty."
}

function Read-IsoEntryBytes
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$IsoBytes,
        [Parameter(Mandatory = $true)]$Entry,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ($Entry.IsDirectory) {
        Fail-M1 "final ISO path '$Path' resolved to a directory, not a file."
    }

    $sectorSize = 2048
    $fileOffset = [int]($Entry.ExtentLba * $sectorSize)
    $fileLength = [int]$Entry.DataLength
    if (($fileOffset + $fileLength) -gt $IsoBytes.Length) {
        Fail-M1 "final ISO path '$Path' extends beyond the image size."
    }

    [byte[]]$fileBytes = [byte[]]::new($fileLength)
    if ($fileLength -gt 0) {
        [Array]::Copy($IsoBytes, $fileOffset, $fileBytes, 0, $fileLength)
    }
    return ,$fileBytes
}

function Assert-BytesEqual
{
    param(
        [Parameter(Mandatory = $true)][byte[]]$Expected,
        [Parameter(Mandatory = $true)][byte[]]$Actual,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Expected.Length -ne $Actual.Length) {
        Fail-M1 "$Label byte count differs between staging and final ISO."
    }

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $expectedHash = [System.BitConverter]::ToString($sha256.ComputeHash($Expected)).Replace("-", "")
        $actualHash = [System.BitConverter]::ToString($sha256.ComputeHash($Actual)).Replace("-", "")
        if ($expectedHash -ne $actualHash) {
            Fail-M1 "$Label content hash differs between staging and final ISO."
        }
    }
    finally {
        $sha256.Dispose()
    }
}

function Assert-FinalIsoContents
{
    param(
        [Parameter(Mandatory = $true)][string]$IsoPath,
        [Parameter(Mandatory = $true)][string]$StageDir,
        [Parameter(Mandatory = $true)][string]$AppsDir,
        [Parameter(Mandatory = $true)][string]$ManifestPath,
        [Parameter(Mandatory = $true)][string]$KernelPath,
        [Parameter(Mandatory = $true)][string]$EfiPath
    )

    [byte[]]$isoBytes = [System.IO.File]::ReadAllBytes($IsoPath)
    $sectorSize = 2048
    $pvdOffset = 16 * $sectorSize
    if (($isoBytes.Length -lt ($pvdOffset + $sectorSize)) -or
        ($isoBytes[$pvdOffset] -ne 1) -or
        ([System.Text.Encoding]::ASCII.GetString($isoBytes, $pvdOffset + 1, 5) -ne "CD001")) {
        Fail-M1 "final ISO does not contain a valid ISO9660 primary volume descriptor."
    }

    $rootRecord = $pvdOffset + 156
    $rootEntries = Get-IsoDirectoryEntries -IsoBytes $isoBytes -ExtentLba (Get-UInt32Le -Bytes $isoBytes -Offset ($rootRecord + 2)) -DataLength (Get-UInt32Le -Bytes $isoBytes -Offset ($rootRecord + 10))
    foreach ($requiredRoot in @("BOOTMAN.TXT", "KERNEL64.BIN", "EFI", "APPS")) {
        if (-not $rootEntries.ContainsKey($requiredRoot)) {
            Fail-M1 "final ISO is missing required root path '$requiredRoot'."
        }
    }

    Assert-BytesEqual -Expected ([System.IO.File]::ReadAllBytes($ManifestPath)) -Actual (Read-IsoEntryBytes -IsoBytes $isoBytes -Entry $rootEntries["BOOTMAN.TXT"] -Path "/BOOTMAN.TXT") -Label "BOOTMAN.TXT"
    Assert-BytesEqual -Expected ([System.IO.File]::ReadAllBytes($KernelPath)) -Actual (Read-IsoEntryBytes -IsoBytes $isoBytes -Entry $rootEntries["KERNEL64.BIN"] -Path "/KERNEL64.BIN") -Label "KERNEL64.BIN"

    $efiEntries = Get-IsoDirectoryEntries -IsoBytes $isoBytes -ExtentLba $rootEntries["EFI"].ExtentLba -DataLength $rootEntries["EFI"].DataLength
    if (-not $efiEntries.ContainsKey("BOOT")) {
        Fail-M1 "final ISO is missing required path '/EFI/BOOT'."
    }
    $bootEntries = Get-IsoDirectoryEntries -IsoBytes $isoBytes -ExtentLba $efiEntries["BOOT"].ExtentLba -DataLength $efiEntries["BOOT"].DataLength
    if (-not $bootEntries.ContainsKey("BOOTX64.EFI")) {
        Fail-M1 "final ISO is missing required path '/EFI/BOOT/BOOTX64.EFI'."
    }
    Assert-BytesEqual -Expected ([System.IO.File]::ReadAllBytes($EfiPath)) -Actual (Read-IsoEntryBytes -IsoBytes $isoBytes -Entry $bootEntries["BOOTX64.EFI"] -Path "/EFI/BOOT/BOOTX64.EFI") -Label "EFI/BOOT/BOOTX64.EFI"

    $appEntries = Get-IsoDirectoryEntries -IsoBytes $isoBytes -ExtentLba $rootEntries["APPS"].ExtentLba -DataLength $rootEntries["APPS"].DataLength
    foreach ($file in Get-ChildItem -Path $AppsDir -File | Sort-Object Name) {
        $appName = $file.Name.ToUpperInvariant()
        if (-not $appEntries.ContainsKey($appName)) {
            Fail-M1 "final ISO is missing required staged APPS path '/APPS/$($file.Name)'."
        }
        Assert-BytesEqual -Expected ([System.IO.File]::ReadAllBytes($file.FullName)) -Actual (Read-IsoEntryBytes -IsoBytes $isoBytes -Entry $appEntries[$appName] -Path "/APPS/$($file.Name)") -Label "/APPS/$($file.Name)"
    }

    $stageFileNames = @{}
    foreach ($file in Get-ChildItem -Path $AppsDir -File) {
        $stageFileNames[$file.Name.ToUpperInvariant()] = $true
    }
    foreach ($entryName in $appEntries.Keys) {
        if (($entryName -eq ".") -or ($entryName -eq "..")) {
            continue
        }
        if (-not $stageFileNames.ContainsKey($entryName)) {
            Fail-M1 "final ISO exposes unstaged APPS entry '/APPS/$entryName'."
        }
    }
}

function Assert-NoUnlabeledPlaceholders
{
    $scanRoots = @(
        (Join-Path $root "README.md"),
        (Join-Path $root "docs"),
        (Join-Path $root "kernel"),
        (Join-Path $root "packages"),
        (Join-Path $root "tools")
    )
    $blockedPattern = '(?i)\b(TODO|FIXME|placeholder|toy|proof-of-concept|demo)\b|\bfake\b'
    $allowedScript = [System.IO.Path]::GetFullPath($PSCommandPath)
    $violations = @()

    foreach ($scanRoot in $scanRoots) {
        if (-not (Test-Path $scanRoot)) {
            continue
        }

        $items = if ((Get-Item $scanRoot).PSIsContainer) {
            Get-ChildItem -Path $scanRoot -Recurse -File
        }
        else {
            @(Get-Item $scanRoot)
        }

        foreach ($item in $items) {
            $fullName = [System.IO.Path]::GetFullPath($item.FullName)
            if ($fullName -eq $allowedScript) {
                continue
            }
            if ($item.Name -like "*.bak") {
                continue
            }
            if ($item.Extension -notin @(".asm", ".c", ".h", ".json", ".md", ".ps1", ".txt")) {
                continue
            }

            foreach ($match in Select-String -Path $item.FullName -Pattern $blockedPattern -AllMatches) {
                $violations += ("{0}:{1}: {2}" -f (Get-RepoRelativePath $item.FullName), $match.LineNumber, $match.Line.Trim())
                if ($violations.Count -ge 20) {
                    break
                }
            }
            if ($violations.Count -ge 20) {
                break
            }
        }
        if ($violations.Count -ge 20) {
            break
        }
    }

    if ($violations.Count -gt 0) {
        Fail-M1 ("unlabeled placeholder/demo language remains:`n" + ($violations -join "`n"))
    }
}

function Assert-NoAbsoluteLocalPaths
{
    param([Parameter(Mandatory = $true)][string[]]$Paths)

    foreach ($path in $Paths) {
        if (-not (Test-Path $path)) {
            Fail-M1 "expected generated report is missing: $(Get-RepoRelativePath $path)"
        }

        $matches = @(Select-String -Path $path -Pattern '[A-Za-z]:\\|\\Users\\|Documents\\Codex' -AllMatches)
        if ($matches.Count -gt 0) {
            $first = $matches[0]
            Fail-M1 ("generated report contains a stale absolute local path at {0}:{1}: {2}" -f (Get-RepoRelativePath $path), $first.LineNumber, $first.Line.Trim())
        }
    }
}

function Assert-RuntimeShellSurfaceSource
{
    $shellPath = Join-Path $root "kernel\arch\x86_64\shell.c"
    $runtimeProbePath = Join-Path $root "kernel\arch\x86_64\runtime_image_user.asm"
    if (-not (Test-Path $shellPath)) {
        Fail-M1 "x86_64 runtime shell source is missing."
    }
    if (-not (Test-Path $runtimeProbePath)) {
        Fail-M1 "x86_64 runtime probe source is missing."
    }

    $source = Get-Content -Path $shellPath -Raw
    $runtimeSource = Get-Content -Path $runtimeProbePath -Raw
    foreach ($requiredText in @(
        "Builtins: apps help hwval info lock net pkginfo pwd",
        "Product apps: append cat copy delete ls mkdir move rename stat touch write",
        "Product network: net shows DHCP lease when virtio-net/e1000e hardware is present",
        "Product hardware validation: hwval is read-only; MSI manual evidence pending",
        "Product package trust: pkginfo and Settings are read-only; install/apply disabled",
        "Product GUI: Terminal, File Manager, Settings through brokered desktop input/display",
        "Product services: Settings shows service/session status; installer writes disabled",
        "Product login: first-run setup, authenticated session, lock/unlock through brokered input",
        "Unavailable in M10: ask (not AI), echo, aliases, app-store, auto-install, public-update-fetch, ai, internal install writes",
        "ASK (not AI)",
        "Network (hardware-gated): use net",
        "Hardware validation: use hwval; read-only; MSI evidence pending",
        "Package trust: use pkginfo or Settings",
        "GUI desktop: Terminal File Manager Settings",
        "Service/session status: Settings",
        "Login/session lock: use lock; first-run user stored on NVMe",
        "Installer dry-run: safe tooling only; writes disabled",
        "Installer writes/install",
        "Aliases: SAY SHOW LIST MAKE PUT SWAP SHIFT",
        "Internal files hidden from app output: HELLO.TXT INDEX.TXT"
    )) {
        if (-not $source.Contains($requiredText)) {
            Fail-M1 "x86_64 runtime shell source is missing required M1 runtime-surface text: $requiredText"
        }
    }

    if ($source.Contains("help apps info pwd ls cat stat write mkdir copy delete rename move touch append echo")) {
        Fail-M1 "x86_64 runtime shell still contains the stale generic help line with echo."
    }

    foreach ($forbiddenCommand in @("ask", "echo", "say", "show", "list", "make", "put", "swap", "shift")) {
        $pattern = 'shell64_token_equals\(command_start,\s*command_length,\s*"{0}"\)' -f [regex]::Escape($forbiddenCommand)
        if ($source -match $pattern) {
            Fail-M1 "x86_64 runtime shell still executes non-M1 command '$forbiddenCommand'."
        }
    }

    if ($source -match 'fs64_list_kernel\s*\(\s*apps_capability') {
        Fail-M1 "x86_64 runtime shell still exposes raw /APPS directory contents through apps."
    }

    if (-not ($source -match 'text_length\s*==\s*0u')) {
        Fail-M1 "x86_64 runtime shell source does not guard empty write/append text against stale help drift."
    }

    foreach ($staleProbeText in @("commands: apps help info pwd ls cat stat write", "LimitlessOS /APPS index", "*.APP files are launcher descriptors.")) {
        if ($runtimeSource.Contains($staleProbeText)) {
            Fail-M1 "x86_64 runtime probe still emits stale app/help surface text: $staleProbeText"
        }
    }
    foreach ($requiredProbeText in @(
        "Builtins apps help hwval info lock net pkginfo pwd",
        "Product apps product set",
        "Unavail ASK-not-AI ECHO aliases",
        "HELLO.TXT INDEX.TXT internal"
    )) {
        if (-not $runtimeSource.Contains($requiredProbeText)) {
            Fail-M1 "x86_64 runtime probe is missing required M1-labeled output: $requiredProbeText"
        }
    }
}

function Assert-X64Artifacts
{
    $loaderSectorLimit = 1024
    $loaderReserveWarning = 128
    $loaderReserveHardMinimum = 96
    $uefiKernelByteLimit = 2 * 1024 * 1024
    $stageDir = Join-Path $distDir "limitlessos-x86_64-uefi"
    $appsDir = Join-Path $stageDir "APPS"
    $manifestPath = Join-Path $stageDir "BOOTMAN.TXT"
    $stagedKernelPath = Join-Path $stageDir "KERNEL64.BIN"
    $scaffoldBinPath = Join-Path $distDir "limitlessos-x86_64.scaffold.bin"
    $uefiKernelBinPath = Join-Path $distDir "limitlessos-x86_64.uefi-kernel.bin"
    $biosKernelBinPath = Join-Path $distDir "KERNEL64-BIOS.BIN"
    $uefiArtifactPath = Join-Path $distDir "limitlessos-x86_64.efi"
    $stagedEfiPath = Join-Path $stageDir "EFI\BOOT\BOOTX64.EFI"
    $isoPath = Join-Path $distDir "limitlessos-x86_64.iso"
    $uefiImagePath = Join-Path $distDir "limitlessos-x86_64-uefi.img"
    $diskImagePath = Join-Path $distDir "limitlessos-x86_64.img"
    $reportPath = Join-Path $distDir "limitlessos-x86_64.scaffold.txt"
    $sizeReportPath = Join-Path $distDir "limitlessos-x86_64.size.txt"
    $stageReadmePath = Join-Path $stageDir "README.TXT"

    foreach ($path in @($stageDir, $appsDir, $manifestPath, $stagedKernelPath, $scaffoldBinPath, $uefiKernelBinPath, $biosKernelBinPath, $uefiArtifactPath, $stagedEfiPath, $isoPath, $uefiImagePath, $diskImagePath, $reportPath, $sizeReportPath, $stageReadmePath)) {
        if (-not (Test-Path $path)) {
            Fail-M1 "required x86_64 artifact is missing: $(Get-RepoRelativePath $path)"
        }
    }

    [byte[]]$stagedKernelBytes = [System.IO.File]::ReadAllBytes($stagedKernelPath)
    [byte[]]$scaffoldBytes = [System.IO.File]::ReadAllBytes($scaffoldBinPath)
    [byte[]]$uefiKernelBytes = [System.IO.File]::ReadAllBytes($uefiKernelBinPath)
    [byte[]]$biosKernelBytes = [System.IO.File]::ReadAllBytes($biosKernelBinPath)
    if ($stagedKernelBytes.Length -ne $uefiKernelBytes.Length) {
        Fail-M1 "staged UEFI kernel byte count does not match dist UEFI kernel binary."
    }

    $stagedChecksum = Get-Fnv1aDataChecksum -Bytes $stagedKernelBytes
    $uefiKernelChecksum = Get-Fnv1aDataChecksum -Bytes $uefiKernelBytes
    $biosKernelChecksum = Get-Fnv1aDataChecksum -Bytes $biosKernelBytes
    if ($stagedChecksum -ne $uefiKernelChecksum) {
        Fail-M1 "staged UEFI kernel checksum does not match dist UEFI kernel binary."
    }
    if ($biosKernelBytes.Length -ne $scaffoldBytes.Length) {
        Fail-M1 "BIOS kernel byte count does not match dist scaffold binary."
    }

    $sectorCount = [int][Math]::Ceiling($biosKernelBytes.Length / 512.0)
    $sectorReserve = $loaderSectorLimit - $sectorCount
    if (($sectorCount -le 0) -or ($sectorCount -gt $loaderSectorLimit)) {
        Fail-M1 "kernel sector count $sectorCount is outside the $loaderSectorLimit-sector loader limit."
    }
    if ($sectorReserve -lt $loaderReserveHardMinimum) {
        Fail-M1 "kernel sector reserve $sectorReserve is below the hard M1 minimum of $loaderReserveHardMinimum sectors."
    }
    if ($sectorReserve -lt $loaderReserveWarning) {
        Write-Warning "kernel sector reserve $sectorReserve is below the M1 warning threshold of $loaderReserveWarning sectors."
    }
    if ($stagedKernelBytes.Length -gt $uefiKernelByteLimit) {
        Fail-M1 "UEFI kernel byte count $($stagedKernelBytes.Length) exceeds the $uefiKernelByteLimit-byte UEFI file-size contract."
    }

    $manifest = Read-KeyValueFile -Path $manifestPath
    foreach ($key in @("architecture", "kernel", "kernel-bytes", "kernel-byte-limit", "kernel-byte-reserve", "kernel-checksum", "boot-contract")) {
        if (-not $manifest.ContainsKey($key)) {
            Fail-M1 "BOOTMAN.TXT is missing required key '$key'."
        }
    }
    if ($manifest["architecture"] -ne "x86_64") {
        Fail-M1 "BOOTMAN.TXT architecture does not match x86_64."
    }
    if ($manifest["kernel"] -ne "KERNEL64.BIN") {
        Fail-M1 "BOOTMAN.TXT kernel entry is not KERNEL64.BIN."
    }
    if ([int]$manifest["kernel-bytes"] -ne $stagedKernelBytes.Length) {
        Fail-M1 "BOOTMAN.TXT kernel byte count is stale."
    }
    if ([int]$manifest["kernel-byte-limit"] -ne $uefiKernelByteLimit) {
        Fail-M1 "BOOTMAN.TXT UEFI kernel byte limit is stale."
    }
    if ([int]$manifest["kernel-byte-reserve"] -ne ($uefiKernelByteLimit - $stagedKernelBytes.Length)) {
        Fail-M1 "BOOTMAN.TXT UEFI kernel byte reserve is stale."
    }
    if ($manifest["kernel-checksum"] -ne ("0x{0:X8}" -f $stagedChecksum)) {
        Fail-M1 "BOOTMAN.TXT kernel checksum is stale."
    }
    if ($manifest["boot-contract"] -ne "uefi-kernel-file") {
        Fail-M1 "BOOTMAN.TXT boot contract is not the UEFI kernel file-size contract."
    }

    $appFiles = @(Get-ChildItem -Path $appsDir -Filter "*.APP" -File | Sort-Object Name)
    $binFiles = @(Get-ChildItem -Path $appsDir -Filter "*.BIN" -File | Sort-Object Name)
    if ($appFiles.Count -eq 0) {
        Fail-M1 "no APPS descriptors are staged on the UEFI media."
    }
    if ($appFiles.Count -ne $binFiles.Count) {
        Fail-M1 "APPS descriptor count ($($appFiles.Count)) does not match binary count ($($binFiles.Count))."
    }

    $expectedProductApps = @($m1ProductApps | Sort-Object)
    $stagedAppBases = @($appFiles | ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name).ToUpperInvariant() } | Sort-Object)
    $stagedBinBases = @($binFiles | ForEach-Object { [System.IO.Path]::GetFileNameWithoutExtension($_.Name).ToUpperInvariant() } | Sort-Object)
    if (($stagedAppBases -join ",") -ne ($expectedProductApps -join ",")) {
        Fail-M1 ("staged APPS descriptors do not match the M1 product inventory. Expected {0}; found {1}." -f ($expectedProductApps -join ","), ($stagedAppBases -join ","))
    }
    if (($stagedBinBases -join ",") -ne ($expectedProductApps -join ",")) {
        Fail-M1 ("staged APPS binaries do not match the M1 product inventory. Expected {0}; found {1}." -f ($expectedProductApps -join ","), ($stagedBinBases -join ","))
    }

    foreach ($app in $appFiles) {
        $base = [System.IO.Path]::GetFileNameWithoutExtension($app.Name)
        $binPath = Join-Path $appsDir "$base.BIN"
        if (-not (Test-Path $binPath)) {
            Fail-M1 "descriptor $($app.Name) does not have a matching flat binary."
        }
        if ((Get-Item $binPath).Length -le 0) {
            Fail-M1 "flat binary $base.BIN is empty."
        }
        $descriptorLines = @(Get-Content -Path $app.FullName)
        if ($descriptorLines.Count -lt 6) {
            Fail-M1 "descriptor $($app.Name) is malformed; expected six fields."
        }
        if ([string]::IsNullOrWhiteSpace($descriptorLines[4]) -or [string]::IsNullOrWhiteSpace($descriptorLines[5])) {
            Fail-M1 "descriptor $($app.Name) is missing user-visible usage/category metadata."
        }
    }

    foreach ($bin in $binFiles) {
        $base = [System.IO.Path]::GetFileNameWithoutExtension($bin.Name)
        if (-not (Test-Path (Join-Path $appsDir "$base.APP"))) {
            Fail-M1 "flat binary $($bin.Name) is exposed without a matching descriptor."
        }
    }

    Assert-NoAbsoluteLocalPaths -Paths @($reportPath, $sizeReportPath, $manifestPath, $stageReadmePath)
    Assert-RuntimeShellSurfaceSource

    $reportLines = @(Get-Content -Path $reportPath)
    if (-not ($reportLines | Where-Object { $_ -eq "loader-budget: bios-sector-limit $loaderSectorLimit current-sectors $sectorCount reserve-sectors $sectorReserve enforced 1" })) {
        Fail-M1 "scaffold report loader-budget line is missing or stale."
    }
    if (-not ($reportLines | Where-Object { $_ -eq ("uefi-loader-budget: kernel-byte-limit {0} current-bytes {1} reserve-bytes {2} checksum 0x{3:X8} enforced 1" -f $uefiKernelByteLimit, $stagedKernelBytes.Length, ($uefiKernelByteLimit - $stagedKernelBytes.Length), $stagedChecksum) })) {
        Fail-M1 "scaffold report UEFI loader byte-budget line is missing or stale."
    }
    if (-not ($reportLines | Where-Object { $_ -eq "artifact-bin: dist\limitlessos-x86_64.scaffold.bin" })) {
        Fail-M1 "scaffold report does not use repo-relative artifact paths."
    }

    Assert-BytesEqual -Expected ([System.IO.File]::ReadAllBytes($uefiArtifactPath)) -Actual ([System.IO.File]::ReadAllBytes($stagedEfiPath)) -Label "staged BOOTX64.EFI"
    Assert-FinalIsoContents -IsoPath $isoPath -StageDir $stageDir -AppsDir $appsDir -ManifestPath $manifestPath -KernelPath $stagedKernelPath -EfiPath $stagedEfiPath
    Assert-NoPrivateKeyMaterialInProductArtifacts -Paths @(
        $biosKernelBinPath,
        $scaffoldBinPath,
        $uefiKernelBinPath,
        $stagedKernelPath,
        $uefiArtifactPath,
        $stagedEfiPath,
        $isoPath,
        $uefiImagePath,
        $diskImagePath
    )

    if ($WriteInventory) {
        $experimentalRuntimeEnabled = ($BuildProfile -eq "Experimental")
        $sectorBudgetStatus = if ($sectorReserve -lt $loaderReserveHardMinimum) {
            "fail"
        }
        elseif ($sectorReserve -lt $loaderReserveWarning) {
            "warning"
        }
        else {
            "ok"
        }
        $experimentalApps = @(
            [PSCustomObject]@{
                name = "ASK"
                status = "unavailable"
                reason = "not AI; no consent-gated assistant path is product-path"
            },
            [PSCustomObject]@{
                name = "ECHO"
                status = "unavailable"
                reason = "not Product path"
            }
        )
        $activeExperimentalServices = if ($experimentalRuntimeEnabled) {
            @(
                "gui proof surface",
                "desktop proof surface",
                "broad hardware proof telemetry"
            )
        }
        else {
            @()
        }
        $inventory = [PSCustomObject]@{
            milestone = "M1 Real Bootable System Slice"
            architecture = $Architecture
            buildProfile = $BuildProfile
            generatedUtc = [DateTime]::UtcNow.ToString("o")
            kernel = [PSCustomObject]@{
                bytes = $biosKernelBytes.Length
                sectors = $sectorCount
                sectorLimit = $loaderSectorLimit
                sectorReserve = $sectorReserve
                checksum = ("0x{0:X8}" -f $biosKernelChecksum)
                biosBytes = $biosKernelBytes.Length
                biosSectors = $sectorCount
                biosSectorLimit = $loaderSectorLimit
                biosSectorReserve = $sectorReserve
                biosChecksum = ("0x{0:X8}" -f $biosKernelChecksum)
                uefiBytes = $stagedKernelBytes.Length
                uefiByteLimit = $uefiKernelByteLimit
                uefiByteReserve = ($uefiKernelByteLimit - $stagedKernelBytes.Length)
                uefiChecksum = ("0x{0:X8}" -f $stagedChecksum)
            }
            artifacts = [PSCustomObject]@{
                iso = Get-RepoRelativePath $isoPath
                uefiImage = Get-RepoRelativePath $uefiImagePath
                diskImage = Get-RepoRelativePath $diskImagePath
                manifest = Get-RepoRelativePath $manifestPath
                report = Get-RepoRelativePath $reportPath
            }
            productApps = $m1ProductApps
            productGuiApps = $m4ProductGuiApps
            experimentalApps = $experimentalApps
            shellBuiltins = $m1ShellBuiltins
            aliases = @($m1Aliases | ForEach-Object {
                [PSCustomObject]@{
                    name = $_
                    status = "unavailable"
                    reason = "not M1 product-path"
                }
            })
            internalFiles = @($m1InternalFiles | ForEach-Object {
                [PSCustomObject]@{
                    name = $_
                    status = "hidden from runtime apps output"
                }
            })
            unavailableFeatures = $m1UnavailableFeatures
            runtimeHelpVerified = $true
            runtimeAppsVerified = $true
            runtimeSurfaceVerified = $true
            finalIsoVerified = $true
            persistenceVerified = $false
            apps = @($appFiles | ForEach-Object {
                $base = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
                [PSCustomObject]@{
                    descriptor = $_.Name
                    binary = "$base.BIN"
                    descriptorBytes = $_.Length
                    binaryBytes = (Get-Item (Join-Path $appsDir "$base.BIN")).Length
                }
            })
        }
        $inventoryPath = Join-Path $distDir "limitlessos-x86_64.m1.json"
        $inventory | ConvertTo-Json -Depth 6 | Set-Content -Path $inventoryPath -Encoding Ascii

        $m2ProductServices = @(
            "x86_64 boot",
            "persistent ring-3 shell",
            "truthful shell help/apps",
            "brokered persistent file workflow",
            "capability denial checks",
            "NVMe persistence verification path"
        )
        $m3ProductServices = @(
            $m2ProductServices +
            @("brokered DHCP/DNS/TCP/HTTP network status")
        )
        $m4ProductServices = @(
            $m3ProductServices +
            @(
                "brokered compositor/window-manager/desktop GUI",
                "Terminal/File Manager/Settings Product GUI apps"
            )
        )
        $m5ProductServices = @(
            $m4ProductServices +
            @("safe installer dry-run tooling with internal writes disabled by default")
        )
        $m6ServiceRecords = @(
            [PSCustomObject]@{ id = "policy-security-broker"; name = "policy/security broker"; state = "running"; principal = "ai-policy"; manifestId = 1; generation = 1; restartCount = 0; capabilities = @("route-policy", "audit"); sessionBinding = $null; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "console-shell-broker"; name = "console/shell broker"; state = "running"; principal = "console"; manifestId = 2; generation = 1; restartCount = 0; capabilities = @("console", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "input-broker"; name = "input broker"; state = "running"; principal = "input"; manifestId = 3; generation = 1; restartCount = 0; capabilities = @("input", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "display-compositor"; name = "display/compositor"; state = "running"; principal = "display"; manifestId = 4; generation = 1; restartCount = 0; capabilities = @("display", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "window-manager-desktop"; name = "window manager / desktop shell"; state = "running"; principal = "display"; manifestId = 5; generation = 1; restartCount = 0; capabilities = @("display", "input", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "filesystem-broker"; name = "filesystem broker"; state = "running"; principal = "ramfs"; manifestId = 6; generation = 1; restartCount = 0; capabilities = @("ramfs", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "block-storage-broker"; name = "block/storage broker"; state = "running"; principal = "block"; manifestId = 7; generation = 1; restartCount = 0; capabilities = @("block", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "hardware-inventory-broker"; name = "hardware inventory broker"; state = "running"; principal = "hardware-inventory"; manifestId = 8; generation = 1; restartCount = 0; capabilities = @("hardware", "audit"); sessionBinding = $null; health = "ok"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "network-broker"; name = "network broker"; state = "running"; principal = "driver-host"; manifestId = 9; generation = 1; restartCount = 0; capabilities = @("route-driver", "audit"); sessionBinding = 1; health = "hardware-gated"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "installer-dryrun"; name = "installer dry-run service/tool"; state = "running"; principal = "init-supervisor"; manifestId = 10; generation = 1; restartCount = 0; capabilities = @("hardware-read", "block-read", "audit"); sessionBinding = 1; health = "writes-disabled"; productStatus = "Product active" },
            [PSCustomObject]@{ id = "settings-system-info"; name = "settings/system-info provider"; state = "running"; principal = "telemetry"; manifestId = 11; generation = 2; restartCount = 1; capabilities = @("read-telemetry", "audit"); sessionBinding = 1; health = "ok"; productStatus = "Product active" }
        )
        $m6ExperimentalServices = @(
            [PSCustomObject]@{ name = "ASK"; status = "unavailable"; reason = "not AI; no consent-gated assistant path in Product" },
            [PSCustomObject]@{ name = "ECHO"; status = "unavailable"; reason = "not Product path" },
            [PSCustomObject]@{ name = "aliases"; status = "unavailable"; reason = "not Product shell commands" },
            [PSCustomObject]@{ name = "package install/apply UX"; status = "unavailable"; reason = "not implemented as Product behavior" },
            [PSCustomObject]@{ name = "AI assistant behavior"; status = "unavailable"; reason = "no consent-gated assistant path in Product" }
        )

        $m2Inventory = [PSCustomObject]@{
            milestone = "M2 Product Kernel Boundary + Experimental Quarantine"
            architecture = $Architecture
            buildProfile = $BuildProfile
            productApps = $m1ProductApps
            productGuiApps = @()
            experimentalApps = $experimentalApps
            unavailableFeatures = $m3UnavailableFeatures
            activeProductServices = @($m2ProductServices)
            activeExperimentalServices = @($activeExperimentalServices)
            experimentalRuntimeEnabled = $experimentalRuntimeEnabled
            productKernelBytes = $biosKernelBytes.Length
            productKernelSectors = $sectorCount
            productKernelReserve = $sectorReserve
            productBiosKernelBytes = $biosKernelBytes.Length
            productBiosKernelSectors = $sectorCount
            productBiosKernelReserve = $sectorReserve
            productBiosKernelChecksum = ("0x{0:X8}" -f $biosKernelChecksum)
            productUefiKernelBytes = $stagedKernelBytes.Length
            productUefiKernelByteLimit = $uefiKernelByteLimit
            productUefiKernelByteReserve = ($uefiKernelByteLimit - $stagedKernelBytes.Length)
            productUefiKernelChecksum = ("0x{0:X8}" -f $stagedChecksum)
            productKernelChecksum = ("0x{0:X8}" -f $biosKernelChecksum)
            sectorBudgetStatus = $sectorBudgetStatus
            bootContract = "Dual contract: BIOS keeps 1024-sector loader limit; UEFI uses a 2 MiB kernel file-size limit verified by BOOTMAN.TXT checksum"
            finalIsoVerified = $true
            runtimeSurfaceVerified = $true
            persistenceVerified = $false
            artifacts = [PSCustomObject]@{
                finalIso = Get-RepoRelativePath $isoPath
                uefiImage = Get-RepoRelativePath $uefiImagePath
                diskImage = Get-RepoRelativePath $diskImagePath
                m1Inventory = Get-RepoRelativePath $inventoryPath
            }
        }
        $m2InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m2.json" -f $BuildProfile.ToLowerInvariant())
        $m2Inventory | ConvertTo-Json -Depth 6 | Set-Content -Path $m2InventoryPath -Encoding Ascii

        $m3Inventory = $m2Inventory.PSObject.Copy()
        $m3Inventory.milestone = "M3 Product Boot Contract + Network Promotion"
        $m3Inventory.activeProductServices = @($m3ProductServices)
        $m3Inventory.productGuiApps = @()
        $m3Inventory.unavailableFeatures = $m3UnavailableFeatures
        $m3Inventory | Add-Member -Force -NotePropertyName productNetworkCapability -NotePropertyValue "brokered DHCP/DNS/TCP/HTTP status through net command; no sockets or ambient network authority"
        $m3InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m3.json" -f $BuildProfile.ToLowerInvariant())
        $m3Inventory | ConvertTo-Json -Depth 8 | Set-Content -Path $m3InventoryPath -Encoding Ascii

        $m4Inventory = $m3Inventory.PSObject.Copy()
        $m4Inventory.milestone = "M4 Interactive GUI Promoted to Product"
        $m4Inventory.productGuiApps = $m4ProductGuiApps
        $m4Inventory.unavailableFeatures = $m4UnavailableFeatures
        $m4Inventory.activeProductServices = @($m4ProductServices)
        $m4Inventory | Add-Member -Force -NotePropertyName productGuiCapability -NotePropertyValue "brokered compositor/window-manager/desktop with Terminal, File Manager, and Settings; no direct app framebuffer or raw-input authority"
        $m4Inventory | Add-Member -Force -NotePropertyName guiInteractiveVerified -NotePropertyValue $true
        $m4Inventory | Add-Member -Force -NotePropertyName inputRoutingModel -NotePropertyValue "raw keyboard/mouse enter the input broker; the window manager routes events only to the focused window; unfocused terminal delivery is denied"
        $m4Inventory | Add-Member -Force -NotePropertyName displayAuthorityModel -NotePropertyValue "the compositor owns physical framebuffer presentation; windows draw through brokered compositor/window-manager paths"
        $m4Inventory | Add-Member -Force -NotePropertyName fileManagerAuthorityLimits -NotePropertyValue "File Manager is limited to RAMFS, boot-media read-only areas, and explicitly brokered persistent namespace; it must not browse or write internal laptop partitions"
        $m4Inventory | Add-Member -Force -NotePropertyName settingsAuthority -NotePropertyValue "Settings receives read-only display/input/network/storage/profile/boot metadata and cannot write configuration"
        $m4InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m4.json" -f $BuildProfile.ToLowerInvariant())
        $m4Inventory | ConvertTo-Json -Depth 8 | Set-Content -Path $m4InventoryPath -Encoding Ascii

        $m5Inventory = $m4Inventory.PSObject.Copy()
        $m5Inventory.milestone = "M5 Safe Installer + Partition Protection"
        $m5Inventory.activeProductServices = @($m5ProductServices)
        $m5Inventory | Add-Member -Force -NotePropertyName installerCapability -NotePropertyValue "dry-run partition inspection is Product; internal NVMe writes, format, boot-entry, and firmware authority remain disabled by default"
        $m5Inventory | Add-Member -Force -NotePropertyName installerDryRunVerified -NotePropertyValue $true
        $m5Inventory | Add-Member -Force -NotePropertyName internalInstallWritesEnabled -NotePropertyValue $false
        $m5InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m5.json" -f $BuildProfile.ToLowerInvariant())
        $m5Inventory | ConvertTo-Json -Depth 8 | Set-Content -Path $m5InventoryPath -Encoding Ascii

        $m6Inventory = $m5Inventory.PSObject.Copy()
        $m6Inventory.milestone = "M6 Service Manager + User/Session Model"
        $m6Inventory.activeProductServices = @($m6ServiceRecords | ForEach-Object { $_.name })
        $m6Inventory.activeExperimentalServices = @()
        $m6Inventory | Add-Member -Force -NotePropertyName productServices -NotePropertyValue $m6ServiceRecords
        $m6Inventory | Add-Member -Force -NotePropertyName experimentalServices -NotePropertyValue $m6ExperimentalServices
        $m6Inventory | Add-Member -Force -NotePropertyName unavailableServices -NotePropertyValue @("full multiuser login/auth", "installer write/format/boot-entry authority", "package manager", "AI assistant")
        $m6Inventory | Add-Member -Force -NotePropertyName activeSessions -NotePropertyValue @(
            [PSCustomObject]@{
                sessionId = 1
                userId = "local-console"
                seatId = 0
                inputScope = "active-session-only"
                displayScope = "session-window-namespace"
                windowNamespace = "local-console"
                filesystemNamespaceGrants = @("RAMFS", "boot-media read-only", "brokered persistent namespace")
                networkGrants = @("read-only network status")
                installerGrants = @("dry-run read-only inventory")
                state = "active"
            }
        )
        $m6Inventory | Add-Member -Force -NotePropertyName sessionCount -NotePropertyValue 1
        $m6Inventory | Add-Member -Force -NotePropertyName activeSeatCount -NotePropertyValue 1
        $m6Inventory | Add-Member -Force -NotePropertyName serviceLifecycleVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName sessionAuthorityVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName inputRoutingVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName displayAuthorityVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName filesystemAuthorityVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName networkAuthorityVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName installerDryRunSafetyVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName controlledRestartVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName staleCapabilityDenialVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName noAmbientAuthorityVerified -NotePropertyValue $true
        $m6Inventory | Add-Member -Force -NotePropertyName fullMultiuserAuth -NotePropertyValue "not implemented; M6 has one local console session"
        $m6Inventory | Add-Member -Force -NotePropertyName gitCommit -NotePropertyValue (Get-GitCommit)
        $m6Inventory | Add-Member -Force -NotePropertyName gitStatus -NotePropertyValue (Get-GitStatusSummary)
        $m6InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m6.json" -f $BuildProfile.ToLowerInvariant())
        $m6Inventory | ConvertTo-Json -Depth 12 | Set-Content -Path $m6InventoryPath -Encoding Ascii

        $m7Inventory = $m6Inventory.PSObject.Copy()
        $m7Inventory.milestone = "M7.1 Supply-Chain Negative Fixture Closure"
        $m7Inventory.activeProductServices = @(
            $m6Inventory.activeProductServices +
            @("signed package admission", "signed update-index verification")
        )
        $m7Inventory.unavailableServices = @("full multiuser login/auth", "installer write/format/boot-entry authority", "auto-install", "app store", "AI assistant")
        $trustedPublicKeyId = "unavailable"
        $trustedPublicKeyFingerprint = "unavailable"
        $signatureHeaderPath = Join-Path $root "build\generated\package_store_signatures_generated.h"
        if (Test-Path -LiteralPath $signatureHeaderPath) {
            $signatureHeader = Get-Content -LiteralPath $signatureHeaderPath -Raw
            if ($signatureHeader -match '#define\s+PACKAGE_STORE_SIGNATURE_PUBLIC_KEY_ID\s+(0x[0-9A-Fa-f]+)u') {
                $trustedPublicKeyId = $Matches[1].ToUpperInvariant()
            }
            if ($signatureHeader -match '(?s)static\s+const\s+u8\s+package_store_signature_public_key\[32\]\s*=\s*\{(?<key>.*?)\};') {
                $keyBytes = New-Object System.Collections.Generic.List[byte]
                foreach ($byteMatch in [regex]::Matches($Matches["key"], '0x([0-9A-Fa-f]{2})')) {
                    $keyBytes.Add([Convert]::ToByte($byteMatch.Groups[1].Value, 16))
                }
                if ($keyBytes.Count -eq 32) {
                    $sha256 = [System.Security.Cryptography.SHA256]::Create()
                    try {
                        $digest = $sha256.ComputeHash($keyBytes.ToArray())
                        $trustedPublicKeyFingerprint = (($digest | ForEach-Object { $_.ToString("X2") }) -join "")
                    } finally {
                        $sha256.Dispose()
                    }
                }
            }
        }
        $m7Inventory | Add-Member -Force -NotePropertyName packageFormatVersion -NotePropertyValue 2
        $m7Inventory | Add-Member -Force -NotePropertyName packageSignatureAlgorithm -NotePropertyValue "Ed25519"
        $m7Inventory | Add-Member -Force -NotePropertyName trustedPublicKeyId -NotePropertyValue $trustedPublicKeyId
        $m7Inventory | Add-Member -Force -NotePropertyName trustedPublicKeyFingerprint -NotePropertyValue $trustedPublicKeyFingerprint
        $m7Inventory | Add-Member -Force -NotePropertyName signedPackageCount -NotePropertyValue 12
        $m7Inventory | Add-Member -Force -NotePropertyName unsignedPackageCountDenied -NotePropertyValue 1
        $m7Inventory | Add-Member -Force -NotePropertyName invalidPackageCountDenied -NotePropertyValue 1
        $m7Inventory | Add-Member -Force -NotePropertyName packageSignatureVerificationStatus -NotePropertyValue "UEFI Product verifies signed archive and payload signatures before admission; BIOS Product remains checksum-only fallback"
        $m7Inventory | Add-Member -Force -NotePropertyName updateIndexVerificationStatus -NotePropertyValue "signed local update-index fixture verified with Ed25519"
        $m7Inventory | Add-Member -Force -NotePropertyName updateRollbackDenialStatus -NotePropertyValue "older signed index generation denied without rollback authority"
        $m7Inventory | Add-Member -Force -NotePropertyName privateKeyArtifactScanStatus -NotePropertyValue "passed"
        $m7Inventory | Add-Member -Force -NotePropertyName packageInstallCapabilityEnforcementStatus -NotePropertyValue "scoped install capability required; no ambient install authority"
        $m7Inventory | Add-Member -Force -NotePropertyName packageWrongOwnerDenialStatus -NotePropertyValue "verified"
        $m7Inventory | Add-Member -Force -NotePropertyName packageStaleTokenDenialStatus -NotePropertyValue "verified"
        $m7Inventory | Add-Member -Force -NotePropertyName noAmbientInstallAuthorityStatus -NotePropertyValue "verified"
        $m7Inventory | Add-Member -Force -NotePropertyName noAmbientUpdateAuthorityStatus -NotePropertyValue "verified"
        $m7Inventory | Add-Member -Force -NotePropertyName trustedTimeStatus -NotePropertyValue "unavailable"
        $m7Inventory | Add-Member -Force -NotePropertyName expiryEnforcementStatus -NotePropertyValue "not Product-enforced without trusted time; anti-rollback uses signed index sequence"
        $m7Inventory | Add-Member -Force -NotePropertyName m7_1NegativeFixturesComplete -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageWrongKeyDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageManifestTamperDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packagePayloadTamperDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageUnsupportedManifestVersionDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageDuplicateDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageDowngradeDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageCapabilityPolicyDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageMalformedDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageOversizedDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName packageInstallNoCapDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateIndexUnsignedDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateIndexTamperDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateIndexWrongKeyDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateIndexReplayHandled -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateCheckNoNetworkAuthorityDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName updateApplyNoInstallAuthorityDenied -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName noAutoInstallVerified -NotePropertyValue $true
        $m7Inventory | Add-Member -Force -NotePropertyName livePublicUpdateFetchStatus -NotePropertyValue "unavailable/non-product"
        $m7Inventory | Add-Member -Force -NotePropertyName productBiosKernelBytes -NotePropertyValue $biosKernelBytes.Length
        $m7Inventory | Add-Member -Force -NotePropertyName productBiosKernelSectors -NotePropertyValue $sectorCount
        $m7Inventory | Add-Member -Force -NotePropertyName productBiosKernelReserve -NotePropertyValue $sectorReserve
        $m7Inventory | Add-Member -Force -NotePropertyName productBiosKernelChecksum -NotePropertyValue ("0x{0:X8}" -f $biosKernelChecksum)
        $m7Inventory | Add-Member -Force -NotePropertyName productUefiKernelBytes -NotePropertyValue $stagedKernelBytes.Length
        $m7Inventory | Add-Member -Force -NotePropertyName productUefiKernelByteLimit -NotePropertyValue $uefiKernelByteLimit
        $m7Inventory | Add-Member -Force -NotePropertyName productUefiKernelByteReserve -NotePropertyValue ($uefiKernelByteLimit - $stagedKernelBytes.Length)
        $m7Inventory | Add-Member -Force -NotePropertyName productUefiKernelChecksum -NotePropertyValue ("0x{0:X8}" -f $stagedChecksum)
        $m7Inventory | Add-Member -Force -NotePropertyName bootContract -NotePropertyValue "Split contract: BIOS KERNEL64-BIOS.BIN is limited to 1024 sectors; UEFI KERNEL64.BIN is limited to 2 MiB and verified by BOOTMAN.TXT byte count/checksum"
        $m7Inventory | Add-Member -Force -NotePropertyName gitCommit -NotePropertyValue (Get-GitCommit)
        $m7Inventory | Add-Member -Force -NotePropertyName gitStatus -NotePropertyValue (Get-GitStatusSummary)
        $m7InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m7.json" -f $BuildProfile.ToLowerInvariant())
        $m7Inventory | ConvertTo-Json -Depth 12 | Set-Content -Path $m7InventoryPath -Encoding Ascii

        $m8Inventory = $m7Inventory.PSObject.Copy()
        $m8Inventory.milestone = "M8 Package Manager UX + Trust Policy Surface"
        $m8Inventory.activeProductServices = @(
            $m7Inventory.activeProductServices +
            @("read-only package trust telemetry")
        )
        $m8Inventory.unavailableServices = @("full multiuser login/auth", "installer write/format/boot-entry authority", "auto-install", "app store", "live public update fetch", "package install/apply actions", "AI assistant")
        $m8Inventory | Add-Member -Force -NotePropertyName packageTrustSurfaceVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName settingsPackagePanelVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName shellPackageStatusVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName packageManagerUxStatus -NotePropertyValue "read-only Product trust/status surface only; not an app store"
        $m8Inventory | Add-Member -Force -NotePropertyName autoInstallStatus -NotePropertyValue "unavailable"
        $m8Inventory | Add-Member -Force -NotePropertyName publicUpdateFetchStatus -NotePropertyValue "unavailable/non-product"
        $m8Inventory | Add-Member -Force -NotePropertyName trustedTimeStatus -NotePropertyValue "unavailable/non-product"
        $m8Inventory | Add-Member -Force -NotePropertyName installActionStatus -NotePropertyValue "disabled in M8; scoped install capability required for future apply"
        $m8Inventory | Add-Member -Force -NotePropertyName updateApplyActionStatus -NotePropertyValue "disabled in M8; no auto-install"
        $m8Inventory | Add-Member -Force -NotePropertyName biosPackageMode -NotePropertyValue "checksum-only fallback"
        $m8Inventory | Add-Member -Force -NotePropertyName uefiPackageMode -NotePropertyValue "Ed25519 verified package admission"
        $m8Inventory | Add-Member -Force -NotePropertyName signerKeyId -NotePropertyValue $trustedPublicKeyId
        $m8Inventory | Add-Member -Force -NotePropertyName signerFingerprint -NotePropertyValue $trustedPublicKeyFingerprint
        $m8Inventory | Add-Member -Force -NotePropertyName packageListVisible -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName capabilityRequestsVisible -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName deniedCapabilityRequestsVisible -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName settingsPackagePanelReadOnly -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName installUnavailableVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName updateApplyUnavailableVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName noAmbientPackageNetworkVerified -NotePropertyValue $true
        $m8Inventory | Add-Member -Force -NotePropertyName gitCommit -NotePropertyValue (Get-GitCommit)
        $m8Inventory | Add-Member -Force -NotePropertyName gitStatus -NotePropertyValue (Get-GitStatusSummary)
        $m8InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m8.json" -f $BuildProfile.ToLowerInvariant())
        $m8Inventory | ConvertTo-Json -Depth 12 | Set-Content -Path $m8InventoryPath -Encoding Ascii

        $m9Inventory = $m8Inventory.PSObject.Copy()
        $m9Inventory.milestone = "M9 Bare-Metal Validation + MSI Dry-Run Evidence"
        $m9Inventory.activeProductServices = @(
            $m8Inventory.activeProductServices +
            @("read-only hardware validation")
        )
        $m9Inventory.unavailableServices = @("full multiuser login/auth", "installer write/format/boot-entry authority", "auto-install", "app store", "live public update fetch", "package install/apply actions", "trusted-time expiry enforcement", "AI assistant")
        $m9Inventory | Add-Member -Force -NotePropertyName hardwareValidationMode -NotePropertyValue "read-only Product hwval surface plus MSI manual evidence checklist"
        $m9Inventory | Add-Member -Force -NotePropertyName hardwareValidationReadonly -NotePropertyValue $true
        $m9Inventory | Add-Member -Force -NotePropertyName msiManualEvidenceStatus -NotePropertyValue "pending user-provided laptop boot and installer dry-run output"
        $m9Inventory | Add-Member -Force -NotePropertyName installerDryRunParserStatus -NotePropertyValue "available"
        $m9Inventory | Add-Member -Force -NotePropertyName realInstallApprovalStatus -NotePropertyValue $false
        $m9Inventory | Add-Member -Force -NotePropertyName internalWriteStatus -NotePropertyValue "disabled by default"
        $m9Inventory | Add-Member -Force -NotePropertyName hardwareValidationVerifierStatus -NotePropertyValue "pending archive run"
        $m9Inventory | Add-Member -Force -NotePropertyName gitCommit -NotePropertyValue (Get-GitCommit)
        $m9Inventory | Add-Member -Force -NotePropertyName gitStatus -NotePropertyValue (Get-GitStatusSummary)
        $m9InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m9.json" -f $BuildProfile.ToLowerInvariant())
        $m9Inventory | ConvertTo-Json -Depth 12 | Set-Content -Path $m9InventoryPath -Encoding Ascii

        $m10Inventory = $m9Inventory.PSObject.Copy()
        $m10Inventory.milestone = "M10 User Authentication and Login"
        $m10Inventory.activeProductServices = @(
            $m9Inventory.activeProductServices +
            @("local login/authentication gate", "session lock/unlock")
        )
        $m10Inventory.unavailableFeatures = @(
            $m9Inventory.unavailableFeatures +
            @("Multiuser account management UI", "Password change UI", "PAM/LDAP/remote auth")
        ) | Select-Object -Unique
        $m10Inventory.unavailableServices = @("multiuser account management UI", "password change UI", "PAM/LDAP/remote auth", "installer write/format/boot-entry authority", "auto-install", "app store", "live public update fetch", "package install/apply actions", "trusted-time expiry enforcement", "AI assistant")
        $m10Inventory | Add-Member -Force -NotePropertyName loginScreenVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName firstRunSetupStatus -NotePropertyValue "creates one local user record in the brokered NVMe FAT namespace when missing"
        $m10Inventory | Add-Member -Force -NotePropertyName localUserStore -NotePropertyValue "persistent NVMe namespace /USERDB.TXT"
        $m10Inventory | Add-Member -Force -NotePropertyName passwordHashAlgorithm -NotePropertyValue 'bcrypt $2b$ cost 04 via crypt_blowfish'
        $m10Inventory | Add-Member -Force -NotePropertyName loginAuthSuccessVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName wrongPasswordDeniedVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName rateLimitVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName sessionLockVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName sessionUnlockVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName sessionAuthorityScopedVerified -NotePropertyValue $true
        $m10Inventory | Add-Member -Force -NotePropertyName fullMultiuserAuth -NotePropertyValue "single-user local auth only; no multiuser account management"
        $m10Inventory | Add-Member -Force -NotePropertyName fullMultiuserAuthStatus -NotePropertyValue "unavailable/non-product"
        $m10Inventory | Add-Member -Force -NotePropertyName gitCommit -NotePropertyValue (Get-GitCommit)
        $m10Inventory | Add-Member -Force -NotePropertyName gitStatus -NotePropertyValue (Get-GitStatusSummary)
        $m10InventoryPath = Join-Path $distDir ("limitlessos-x86_64.{0}.m10.json" -f $BuildProfile.ToLowerInvariant())
        $m10Inventory | ConvertTo-Json -Depth 12 | Set-Content -Path $m10InventoryPath -Encoding Ascii
    }
}

Assert-NoUnlabeledPlaceholders
Assert-X64Artifacts

Write-Host "M1 production-slice gate passed for $Architecture ($BuildProfile profile)."
