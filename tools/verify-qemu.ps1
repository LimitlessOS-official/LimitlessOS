param(
    [ValidateSet("x86", "x86_64")]
    [string]$Architecture = "x86",

    [ValidateSet("disk", "iso", "uefi")]
    [string]$BootMedia = "disk",

    [ValidateSet("virtio", "e1000e", "e1000")]
    [string]$NetworkDevice = "virtio",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product",

    [switch]$RealBinaryGate,
    [string]$BusyBoxPath = "",
    [string]$BusyBoxSource = "",
    [string]$BusyBoxVersion = "",
    [string]$ExtraAppPath = "",
    [string]$ExtraAppName = "SMOKE",
    [string]$ExtraAppSource = "",
    [string]$ExtraAppVersion = "",
    [string]$ExtraApp2Path = "",
    [string]$ExtraApp2Name = "EXTRA2",
    [string]$ExtraApp2Source = "",
    [string]$ExtraApp2Version = "",
    [string]$ExtraApp3Path = "",
    [string]$ExtraApp3Name = "EXTRA3",
    [string]$ExtraApp3Source = "",
    [string]$ExtraApp3Version = "",
    [string[]]$ExtraShellLine = @(),
    [switch]$HardwareRegistryGate,
    [switch]$HardwareDisplayGate,
    [switch]$HardwareStorageGate,
    [switch]$HardwareStorageStageGate
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$script:LegacyMmioAssertionLines = @()

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

function Ensure-NvmeGptImage
{
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [bool]$StageRealBinary = $false,
        [string]$StageBusyBoxPath = "",
        [string]$StageBusyBoxSource = "",
        [string]$StageBusyBoxVersion = "",
        [string]$StageExtraAppPath = "",
        [string]$StageExtraAppName = "SMOKE",
        [string]$StageExtraAppSource = "",
        [string]$StageExtraAppVersion = "",
        [string]$StageExtraApp2Path = "",
        [string]$StageExtraApp2Name = "EXTRA2",
        [string]$StageExtraApp2Source = "",
        [string]$StageExtraApp2Version = "",
        [string]$StageExtraApp3Path = "",
        [string]$StageExtraApp3Name = "EXTRA3",
        [string]$StageExtraApp3Source = "",
        [string]$StageExtraApp3Version = ""
    )

    $imagePath = Join-Path $Root "dist\limitlessos-x86_64-nvme-gpt.img"
    $generatorPath = Join-Path $Root "tools\generate-nvme-image.ps1"
    $imageBytes = 16777216

    if (-not (Test-Path $generatorPath)) {
        throw "QEMU verification failed: NVMe GPT image generator not found: $generatorPath"
    }

    if ($StageRealBinary -and ((-not [string]::IsNullOrWhiteSpace($StageBusyBoxPath)) -or (-not [string]::IsNullOrWhiteSpace($StageExtraAppPath)) -or (-not [string]::IsNullOrWhiteSpace($StageExtraApp2Path)) -or (-not [string]::IsNullOrWhiteSpace($StageExtraApp3Path)))) {
        & $generatorPath `
            -OutputPath $imagePath `
            -BusyBoxPath $StageBusyBoxPath `
            -BusyBoxSource $StageBusyBoxSource `
            -BusyBoxVersion $StageBusyBoxVersion `
            -ExtraAppPath $StageExtraAppPath `
            -ExtraAppName $StageExtraAppName `
            -ExtraAppSource $StageExtraAppSource `
            -ExtraAppVersion $StageExtraAppVersion `
            -ExtraApp2Path $StageExtraApp2Path `
            -ExtraApp2Name $StageExtraApp2Name `
            -ExtraApp2Source $StageExtraApp2Source `
            -ExtraApp2Version $StageExtraApp2Version `
            -ExtraApp3Path $StageExtraApp3Path `
            -ExtraApp3Name $StageExtraApp3Name `
            -ExtraApp3Source $StageExtraApp3Source `
            -ExtraApp3Version $StageExtraApp3Version
    }
    else {
        & $generatorPath -OutputPath $imagePath
    }
    if (-not $?) {
        throw "QEMU verification failed: could not generate NVMe GPT image."
    }
    if ((-not (Test-Path $imagePath)) -or ((Get-Item $imagePath).Length -ne $imageBytes)) {
        throw "QEMU verification failed: NVMe GPT image has an unexpected size."
    }

    return $imagePath
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

function Get-BinutilsSectionSizes
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $lines = @(size -A $Path)
    if ($LASTEXITCODE -ne 0) {
        throw "QEMU verification failed: could not inspect section sizes for $Path."
    }

    [int64]$text = 0
    [int64]$rodata = 0
    [int64]$data = 0
    [int64]$bss = 0

    foreach ($line in $lines) {
        if ($line -match '^\s*(\.[^\s]+)\s+([0-9]+)\s+') {
            $sectionName = $Matches[1]
            [int64]$sectionSize = $Matches[2]

            if ($sectionName -like ".text*") {
                $text += $sectionSize
            }
            elseif (($sectionName -like ".rodata*") -or ($sectionName -like ".rdata*")) {
                $rodata += $sectionSize
            }
            elseif ($sectionName -like ".data*") {
                $data += $sectionSize
            }
            elseif ($sectionName -like ".bss*") {
                $bss += $sectionSize
            }
        }
    }

    return [PSCustomObject]@{
        Text = $text
        Rodata = $rodata
        Data = $data
        Bss = $bss
    }
}

function Get-Fnv1aDataChecksum
{
    param([byte[]]$Bytes)

    [uint32]$hash = 2166136261
    foreach ($byte in $Bytes) {
        [uint32]$value = $byte
        $hash = [uint32](($hash -bxor $value) -band 0xFFFFFFFF)
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }

    return $hash
}

function Get-Sha256Hex
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($Bytes)
        return ([System.BitConverter]::ToString($hashBytes)).Replace("-", "").ToLowerInvariant()
    }
    finally {
        $sha256.Dispose()
    }
}

function Assert-OutputContains
{
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Message
    )

    $candidateLines = @($Lines + $script:LegacyMmioAssertionLines)
    $matched = $candidateLines | Where-Object { $_ -match $Pattern } | Select-Object -First 1
    if (-not $matched) {
        throw "QEMU verification failed: $Message"
    }
}

function Assert-OutputNotContains
{
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Message
    )

    $matched = $Lines | Where-Object { $_ -match $Pattern } | Select-Object -First 1
    if ($matched) {
        throw "QEMU verification failed: $Message Matched line: $matched"
    }
}

function Get-X64PersistentShellLines
{
    param([string[]]$Lines)

    $startIndex = -1
    for ($index = 0; $index -lt $Lines.Count; $index++) {
        if ($Lines[$index] -match '\[x64:shell\] persistent ring3 shell online') {
            $startIndex = $index
            break
        }
    }

    if ($startIndex -lt 0) {
        return @()
    }

    return @($Lines[$startIndex..($Lines.Count - 1)])
}

function Assert-X64M1RuntimeSurface
{
    param(
        [string[]]$Lines,
        [bool]$LoginExpected,
        [string]$BootMedia
    )

    $persistentLines = @(Get-X64PersistentShellLines -Lines $Lines)
    if ($persistentLines.Count -eq 0) {
        throw "QEMU verification failed: x64 persistent shell transcript was not found for M1 runtime surface validation."
    }

    if ($LoginExpected) {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Builtins: apps devices help hwval info linux lock net pkginfo ports pwd$' -Message "M10 runtime help did not label authenticated shell builtins."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Builtins: apps help hwval info linux net pkginfo pwd$' -Message "M10 BIOS fallback help did not omit unavailable lock builtin."
    }
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product apps: append cat copy delete ls mkdir move nethello rename stat touch write$' -Message "M1 runtime help product app list is missing or stale."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product network: net shows DHCP lease; net curl example\.com performs a scoped HTTP GET$' -Message "M3 runtime help did not describe Product network status."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product hardware validation: hwval is read-only(; MSI manual evidence pending)?$' -Message "M9 runtime help did not describe hardware validation status."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product package trust: pkginfo and Settings are read-only; installation disabled$' -Message "M8 runtime help did not describe Product package trust status."
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product GUI: unavailable on BIOS checksum fallback$' -Message "M15 BIOS fallback help did not label Product GUI as unavailable."
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product services: BIOS fallback shows service and session status; installer UX unavailable$' -Message "M15 BIOS fallback help did not label service/session and installer status truthfully."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product GUI: Terminal, File Manager, Settings, Installer(, Assistant)? through brokered desktop input/display$' -Message "M15/M17 runtime help did not describe Product GUI, installer, and Assistant status."
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product services: Settings shows service and session status; installer planning writes disabled$' -Message "M15 runtime help did not describe Product service/session and installer-planning status."
    }
    if ($LoginExpected) {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product login: first-run setup, authenticated session, lock/unlock through brokered input$' -Message "M10 runtime help did not describe Product login/session lock."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^UEFI login/session lock: unavailable on BIOS checksum fallback$' -Message "M10 BIOS fallback help did not label login/session lock as unavailable."
    }
    if ($LoginExpected) {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product identity/cloud/installer: Settings shows local account, cloud policy, and dry-run installer planning; remote/cloud login unavailable$' -Message "M15 runtime help did not describe Product identity/account/cloud/installer status."
    }
    elseif ($BootMedia -ne "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product identity/cloud/installer: Settings shows local account, cloud policy, and dry-run installer planning; remote/cloud login unavailable$' -Message "M15 UEFI runtime help did not describe identity/account/cloud/installer visibility."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product identity: unavailable on BIOS checksum fallback$' -Message "M11 BIOS fallback help did not label identity/vault as unavailable."
    }
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product cloud storage: Settings and File Manager show broker policy; sync unavailable; transfers denied$' -Message "M14 runtime help did not describe Product cloud storage status."
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product installer UX: unavailable on BIOS checksum fallback; dry-run safety tooling only$' -Message "M15 BIOS fallback help did not label installer UX as unavailable."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product installer UX: launcher and Settings show dry-run planning; writes, formatting, and boot-entry changes disabled$' -Message "M15 runtime help did not describe Product installer UX status."
    }
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product AI policy: unavailable on BIOS checksum fallback; AI actions unavailable$' -Message "M16 BIOS fallback help did not label AI policy as unavailable."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Product AI (policy: Settings/pkginfo show request-deny-audit only; AI actions unavailable|assistant: launcher, Settings, and pkginfo show (read-only consent flow|consent-scoped action templates); inference unavailable)$' -Message "M16-M18 runtime help did not describe Product AI policy/Assistant status."
    }
    Assert-OutputContains -Lines $persistentLines -Pattern '^Unavailable in M21: ask \(not AI\), echo, aliases, personal-login, enterprise-login, account-linking, real-cloud-storage, cloud-sync, auto-upload-download, general-sockets, server-sockets, raw-packets, arbitrary-network-send-receive, encrypted-secrets, encrypted-identity-transport, credential-transport, token-storage, ai-inference, ai-autonomy, ai-automation, cloud-ai, ai-assisted-setup, real-install$|^Unavailable in M(20|1[6789]): ask \(not AI\), echo, aliases, personal-login, enterprise-login, account-linking, real-cloud-storage, cloud-sync, auto-upload-download, (general-sockets, server-sockets, raw-packets, arbitrary-network-send-receive, )?encrypted-secrets, encrypted-identity-transport, credential-transport, token-storage, ai-(assistant|inference), ai-(actions|autonomy), ai-automation, cloud-ai, ai-assisted-setup, real-install$' -Message "M16-M21 runtime help did not label unavailable account/cloud/network/installer/AI surfaces."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Product apps:$' -Message "M1 apps output did not show a product-app section."

    foreach ($productApp in @('APPEND', 'CAT', 'COPY', 'DELETE', 'LS', 'MKDIR', 'MOVE', 'NETHELLO', 'RENAME', 'STAT', 'TOUCH', 'WRITE')) {
        Assert-OutputContains -Lines $persistentLines -Pattern ("^{0}$" -f [regex]::Escape($productApp)) -Message "M1 apps output did not include product app $productApp."
    }

    Assert-OutputContains -Lines $persistentLines -Pattern '^ASK \(not AI\)$' -Message "M1 apps output did not explicitly quarantine ASK as not AI."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Network \(hardware-gated\): use net or net curl example\.com$' -Message "M3 apps output did not label Product network status."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Hardware validation: use hwval; read-only; hardware evidence pending$' -Message "M9 apps output did not label hardware validation visibility."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Package trust: use pkginfo or Settings$' -Message "M8 apps output did not label Package trust visibility."
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^GUI desktop: unavailable on BIOS checksum fallback$' -Message "M15 BIOS apps output did not label GUI as unavailable."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^GUI desktop: Terminal File Manager Settings Installer( Assistant)?$' -Message "M15/M17 apps output did not label Product GUI apps."
    }
    Assert-OutputContains -Lines $persistentLines -Pattern '^Service and session status: Settings$' -Message "M6 apps output did not label service and session status visibility."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Identity, account, vault, and transport status: Settings; local only; no secret storage$' -Message "M13 apps output did not label identity, account, vault, and transport visibility."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Cloud storage status: Settings and File Manager; policy only; sync unavailable$' -Message "M14 apps output did not label cloud storage status visibility."
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Installer UX: unavailable on BIOS checksum fallback; dry-run safety tooling only$' -Message "M15 BIOS apps output did not label installer UX as unavailable."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Installer UX: launcher and Settings show dry-run planning; writes disabled$' -Message "M15 apps output did not label installer UX visibility."
    }
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $persistentLines -Pattern '^AI policy: unavailable on BIOS checksum fallback; no actions$' -Message "M16 BIOS apps output did not label AI policy as unavailable."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^(AI policy: Settings/pkginfo; request-deny-audit only; no actions|AI Assistant: launcher, Settings, and pkginfo show (read-only consent flow|consent-scoped action templates); inference unavailable)$' -Message "M16-M18 apps output did not label AI policy/Assistant visibility."
    }
    if ($LoginExpected) {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Login/session lock: use lock; first-run user stored on NVMe$' -Message "M10 apps output did not label login/session lock visibility."
    }
    else {
        Assert-OutputContains -Lines $persistentLines -Pattern '^Login/session lock: unavailable on BIOS checksum fallback$' -Message "M10 BIOS apps output did not label login/session lock as unavailable."
    }
    Assert-OutputContains -Lines $persistentLines -Pattern '^Installer dry-run: validation tools only; writes disabled$' -Message "M6 apps output did not label installer dry-run validation-only write-disable status."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Aliases: SAY SHOW LIST MAKE PUT SWAP SHIFT$' -Message "M1 apps output did not label alias descriptors."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Personal login$' -Message "M11 apps output did not label personal login unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Enterprise login$' -Message "M11 apps output did not label enterprise login unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Account linking$' -Message "M13 apps output did not label account linking unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Real cloud storage$' -Message "M14 apps output did not label real cloud storage unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Encrypted secret storage$' -Message "M11 apps output did not label encrypted secret storage unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Encrypted identity transport$' -Message "M12 apps output did not label encrypted identity transport unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Security key login$' -Message "M13 apps output did not label security key login unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Credential transport$' -Message "M13 apps output did not label credential transport unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Token storage$' -Message "M13 apps output did not label token storage unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Enterprise policy$' -Message "M13 apps output did not label enterprise policy unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Cloud sync$' -Message "M14 apps output did not label cloud sync unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Automatic cloud transfers$' -Message "M14 apps output did not label automatic cloud transfers unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Arbitrary network transfers$' -Message "M19 apps output did not label arbitrary network transfers unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^AI cloud access$' -Message "M14 apps output did not label AI cloud access unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^(AI assistant|AI inference backend)$' -Message "M16-M18 apps output did not label unavailable AI assistant or inference surface."
    Assert-OutputContains -Lines $persistentLines -Pattern '^AI (actions|autonomous actions)$' -Message "M16-M18 apps output did not label unavailable AI autonomy."
    Assert-OutputContains -Lines $persistentLines -Pattern '^AI automation$' -Message "M16 apps output did not label AI automation unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Cloud AI$' -Message "M16 apps output did not label cloud AI unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^AI-assisted setup$' -Message "M15 apps output did not label AI-assisted setup unavailable."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Real internal installation and write access$' -Message "M15 apps output did not quarantine internal installation and write authority."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Formatting$' -Message "M15 apps output did not quarantine formatting authority."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Boot entry changes$' -Message "M15 apps output did not quarantine boot-entry authority."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Package installation and update actions$' -Message "M8 apps output did not quarantine package installation and update authority."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Auto-install$' -Message "M8 apps output did not quarantine auto-install."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Public update fetch$' -Message "M8 apps output did not quarantine public update fetch."
    Assert-OutputContains -Lines $persistentLines -Pattern '^Internal files hidden from app output: HELLO\.TXT INDEX\.TXT$' -Message "M1 apps output did not label hidden internal app files."

    foreach ($forbidden in @('^ASK\.APP$', '^ECHO\.APP$', '^SAY\.APP$', '^SHOW\.APP$', '^LIST\.APP$', '^MAKE\.APP$', '^PUT\.APP$', '^SWAP\.APP$', '^SHIFT\.APP$', '^HELLO\.TXT$', '^INDEX\.TXT$', '^help apps info pwd .*echo')) {
        Assert-OutputNotContains -Lines $persistentLines -Pattern $forbidden -Message "M1 runtime shell exposed a non-product app or stale help line without an M1 label."
    }
}

function Assert-X64M6ServiceSessionSurface
{
    param([string[]]$Lines)

    Assert-OutputContains -Lines $Lines -Pattern '\[x64\] drs-service-manager drs-service-manager-product 1 drs-service-declared 1 drs-service-running 1 drs-service-status-query 1 drs-service-controlled-crash 1 drs-service-restart 1 drs-service-generation-increment 1 drs-service-stale-cap-denied 1 service-count 11 running-count 11 restart-count 1 wrong-owner-denied 1 restart-authority 1 extra-caps 0 health 1' -Message "M6 Product service lifecycle/status surface was not observed."
    Assert-OutputContains -Lines $Lines -Pattern '\[x64\] drs-session drs-session-created 1 drs-session-active 1 drs-session-input-bound 1 drs-session-display-bound 1 drs-session-fs-bound 1 drs-session-network-bound 1 drs-wrong-session-input-denied 1 drs-wrong-session-display-denied 1 drs-wrong-session-fs-denied 1 drs-no-ambient-input 1 drs-no-ambient-display 1 drs-no-ambient-fs 1 drs-no-ambient-network 1 drs-installer-write-disabled 1 drs-installer-dryrun-no-writes 1 session-id 1 seat 0 installer-bound 1' -Message "M6 local console session authority surface was not observed."
}

function Assert-X64LoaderBudget
{
    param([string]$Root)

    $loaderSectorLimit = 1024
    $loaderReserveHardMinimum = 96
    $uefiKernelByteLimit = 2 * 1024 * 1024
    $kernelPePath = Join-Path $Root "dist\\limitlessos-x86_64-bios.pe"
    $artifactBinPath = Join-Path $Root "dist\\limitlessos-x86_64.scaffold.bin"
    $uefiKernelPePath = Join-Path $Root "dist\\limitlessos-x86_64-uefi-kernel.pe"
    $uefiArtifactBinPath = Join-Path $Root "dist\\limitlessos-x86_64.uefi-kernel.bin"
    $reportPath = Join-Path $Root "dist\\limitlessos-x86_64.scaffold.txt"
    $sizeReportPath = Join-Path $Root "dist\\limitlessos-x86_64.size.txt"
    $manifestPath = Join-Path $Root "dist\\limitlessos-x86_64-uefi\\BOOTMAN.TXT"

    if (-not (Test-Path $kernelPePath)) {
        throw "QEMU verification failed: x64 scaffold PE was not found for size-map validation."
    }
    if (-not (Test-Path $artifactBinPath)) {
        throw "QEMU verification failed: x64 scaffold binary was not found for loader budget validation."
    }
    if (-not (Test-Path $uefiKernelPePath)) {
        throw "QEMU verification failed: x64 UEFI kernel PE was not found for size-map validation."
    }
    if (-not (Test-Path $uefiArtifactBinPath)) {
        throw "QEMU verification failed: x64 UEFI kernel binary was not found for byte-contract validation."
    }
    if (-not (Test-Path $reportPath)) {
        throw "QEMU verification failed: x64 scaffold report was not found for loader budget validation."
    }
    if (-not (Test-Path $sizeReportPath)) {
        throw "QEMU verification failed: x64 size-map report was not found for loader budget validation."
    }
    if (-not (Test-Path $manifestPath)) {
        throw "QEMU verification failed: x64 UEFI manifest was not found for loader budget validation."
    }

    [byte[]]$kernelBytes = [System.IO.File]::ReadAllBytes($artifactBinPath)
    [byte[]]$uefiKernelBytes = [System.IO.File]::ReadAllBytes($uefiArtifactBinPath)
    $sectorCount = [int][Math]::Ceiling($kernelBytes.Length / 512.0)
    if (($sectorCount -le 0) -or ($sectorCount -gt $loaderSectorLimit)) {
        throw "QEMU verification failed: x64 scaffold consumes $sectorCount BIOS sectors, outside the $loaderSectorLimit-sector loader budget."
    }

    $loaderSectorReserve = $loaderSectorLimit - $sectorCount
    if ($loaderSectorReserve -lt $loaderReserveHardMinimum) {
        throw "QEMU verification failed: x64 scaffold BIOS reserve $loaderSectorReserve is below the hard minimum of $loaderReserveHardMinimum sectors."
    }
    if ($uefiKernelBytes.Length -gt $uefiKernelByteLimit) {
        throw "QEMU verification failed: x64 UEFI kernel payload $($uefiKernelBytes.Length) exceeds the $uefiKernelByteLimit-byte UEFI file contract."
    }
    $uefiKernelChecksum = Get-Fnv1aDataChecksum -Bytes $uefiKernelBytes
    $uefiKernelChecksumHex = "0x{0:X8}" -f $uefiKernelChecksum
    $uefiKernelSha256Hex = Get-Sha256Hex -Bytes $uefiKernelBytes
    $kernelSizeMap = Get-BinutilsSectionSizes -Path $kernelPePath
    $uefiKernelSizeMap = Get-BinutilsSectionSizes -Path $uefiKernelPePath
    $reportLines = @(Get-Content $reportPath)
    $sizeReportLines = @(Get-Content $sizeReportPath)
    $manifestLines = @(Get-Content $manifestPath)

    Assert-OutputContains -Lines $reportLines -Pattern ("^loader-budget: bios-sector-limit {0} current-sectors {1} reserve-sectors {2} enforced 1$" -f $loaderSectorLimit, $sectorCount, $loaderSectorReserve) -Message "x64 scaffold report loader budget proof is missing or stale."
    Assert-OutputContains -Lines $reportLines -Pattern ("^uefi-loader-budget: kernel-byte-limit {0} current-bytes {1} reserve-bytes {2} checksum {3} enforced 1$" -f $uefiKernelByteLimit, $uefiKernelBytes.Length, ($uefiKernelByteLimit - $uefiKernelBytes.Length), $uefiKernelChecksumHex) -Message "x64 scaffold report UEFI byte-budget proof is missing or stale."
    Assert-OutputContains -Lines $reportLines -Pattern ("^size-map: text-bytes {0} rodata-bytes {1} data-bytes {2} bss-bytes {3} top-object .+ top-object-total [1-9][0-9]*$" -f $kernelSizeMap.Text, $kernelSizeMap.Rodata, $kernelSizeMap.Data, $kernelSizeMap.Bss) -Message "x64 scaffold report section size map is missing or stale."
    Assert-OutputContains -Lines $reportLines -Pattern ("^uefi-size-map: text-bytes {0} rodata-bytes {1} data-bytes {2} bss-bytes {3} top-object .+ top-object-total [1-9][0-9]*$" -f $uefiKernelSizeMap.Text, $uefiKernelSizeMap.Rodata, $uefiKernelSizeMap.Data, $uefiKernelSizeMap.Bss) -Message "x64 scaffold report UEFI section size map is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^kernel-bytes={0}$" -f $kernelBytes.Length) -Message "x64 size-map kernel byte count is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^bios-kernel-sectors={0}$" -f $sectorCount) -Message "x64 size-map BIOS kernel sector count is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^bios-sector-limit={0}$" -f $loaderSectorLimit) -Message "x64 size-map BIOS sector limit is missing."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^bios-sector-reserve={0}$" -f $loaderSectorReserve) -Message "x64 size-map BIOS sector reserve is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-kernel-bytes={0}$" -f $uefiKernelBytes.Length) -Message "x64 size-map UEFI kernel byte count is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-kernel-byte-limit={0}$" -f $uefiKernelByteLimit) -Message "x64 size-map UEFI byte limit is missing."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-kernel-byte-reserve={0}$" -f ($uefiKernelByteLimit - $uefiKernelBytes.Length)) -Message "x64 size-map UEFI byte reserve is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^section-text={0}$" -f $kernelSizeMap.Text) -Message "x64 size-map text section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^section-rodata={0}$" -f $kernelSizeMap.Rodata) -Message "x64 size-map rodata section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^section-data={0}$" -f $kernelSizeMap.Data) -Message "x64 size-map data section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^section-bss={0}$" -f $kernelSizeMap.Bss) -Message "x64 size-map bss section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern '^top-object=(scaffold|display)-x86_64-bios\.o ' -Message "x64 size-map top object contributor is missing."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-section-text={0}$" -f $uefiKernelSizeMap.Text) -Message "x64 size-map UEFI text section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-section-rodata={0}$" -f $uefiKernelSizeMap.Rodata) -Message "x64 size-map UEFI rodata section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-section-data={0}$" -f $uefiKernelSizeMap.Data) -Message "x64 size-map UEFI data section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern ("^uefi-section-bss={0}$" -f $uefiKernelSizeMap.Bss) -Message "x64 size-map UEFI bss section is missing or stale."
    Assert-OutputContains -Lines $sizeReportLines -Pattern '^uefi-top-object=.+-x86_64-uefi\.o ' -Message "x64 size-map UEFI top object contributor is missing."
    Assert-OutputContains -Lines $manifestLines -Pattern ("^kernel-bytes={0}$" -f $uefiKernelBytes.Length) -Message "x64 UEFI manifest kernel byte count is missing or stale."
    Assert-OutputContains -Lines $manifestLines -Pattern ("^kernel-byte-limit={0}$" -f $uefiKernelByteLimit) -Message "x64 UEFI manifest kernel byte limit is missing."
    Assert-OutputContains -Lines $manifestLines -Pattern ("^kernel-byte-reserve={0}$" -f ($uefiKernelByteLimit - $uefiKernelBytes.Length)) -Message "x64 UEFI manifest kernel byte reserve is missing or stale."
    Assert-OutputContains -Lines $manifestLines -Pattern '^kernel-checksum-algorithm=fnv1a-32$' -Message "x64 UEFI manifest kernel checksum algorithm is not documented."
    Assert-OutputContains -Lines $manifestLines -Pattern ("^kernel-checksum={0}$" -f $uefiKernelChecksumHex) -Message "x64 UEFI manifest kernel checksum is missing or stale."
    Assert-OutputContains -Lines $manifestLines -Pattern ("^kernel-sha256={0}$" -f $uefiKernelSha256Hex) -Message "x64 UEFI manifest kernel SHA-256 is missing or stale."
    Assert-OutputContains -Lines $manifestLines -Pattern '^boot-contract=uefi-kernel-file$' -Message "x64 UEFI manifest boot contract is missing or stale."
    Assert-OutputContains -Lines $manifestLines -Pattern '^non-product-package-registry-stubs=ASK,ECHO$' -Message "x64 UEFI manifest does not label ASK/ECHO as non-product package registry stubs."
    Assert-OutputNotContains -Lines $manifestLines -Pattern '^kernel-sector' -Message "x64 UEFI manifest still carries BIOS sector arithmetic."
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
            if (Select-String -Path $Path -Pattern $Pattern -Quiet -ErrorAction SilentlyContinue) {
                return
            }
        }

        Start-Sleep -Milliseconds 100
    }

    throw "QEMU verification failed: timed out waiting for log marker $Pattern."
}

function Wait-ForAnyLogPattern
{
    param(
        [string[]]$Paths,
        [string]$Pattern,
        [int]$TimeoutMilliseconds
    )

    $candidatePaths = @($Paths | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
    if ($candidatePaths.Count -eq 0) {
        throw "QEMU verification failed: no log path was available while waiting for marker $Pattern."
    }

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMilliseconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        foreach ($candidatePath in $candidatePaths) {
            if ((Test-Path $candidatePath) -and (Select-String -Path $candidatePath -Pattern $Pattern -Quiet -ErrorAction SilentlyContinue)) {
                return
            }
        }

        Start-Sleep -Milliseconds 100
    }

    throw "QEMU verification failed: timed out waiting for log marker $Pattern."
}

function Send-QemuKeyboardProbe
{
    param(
        [int]$Port,
        [int]$DurationMilliseconds,
        [int]$KeyDelayMilliseconds,
        [int]$LineDelayMilliseconds,
        [string]$DebugLogPath = "",
        [string]$FramebufferLogPath = "",
        [bool]$GuiProbeEnabled = $false,
        [bool]$LoginProbeEnabled = $false,
        [bool]$RealBinaryProbeEnabled = $false,
        [bool]$HardwareRegistryProbeEnabled = $false,
        [bool]$HardwareDisplayProbeEnabled = $false,
        [bool]$HardwareStorageProbeEnabled = $false,
        [string[]]$ExtraTextLines = @()
    )

    $client = [System.Net.Sockets.TcpClient]::new()
    $connected = $false

    for ($attempt = 0; $attempt -lt 40; $attempt++) {
        try {
            $client.Connect("127.0.0.1", $Port)
            $connected = $true
            break
        }
        catch {
            Start-Sleep -Milliseconds 50
        }
    }

    if (-not $connected) {
        $client.Dispose()
        throw "QEMU verification failed: could not connect to QMP keyboard probe port $Port."
    }

    try {
        $stream = $client.GetStream()
        $writer = [System.IO.StreamWriter]::new($stream)
        $writer.NewLine = "`n"
        $writer.AutoFlush = $true
        $qmpDrainBuffer = New-Object byte[] 4096
        $drainQmp = {
            while ($stream.DataAvailable) {
                [void]$stream.Read($qmpDrainBuffer, 0, $qmpDrainBuffer.Length)
            }
        }

        $writer.WriteLine('{"execute":"qmp_capabilities"}')
        & $drainQmp

        $deadline = [DateTime]::UtcNow.AddMilliseconds($DurationMilliseconds)
        $keyHoldMilliseconds = [Math]::Max(80, [int]($KeyDelayMilliseconds / 2))
        $frameWidth = 1024
        $frameHeight = 768
        $frameSourcePaths = @($FramebufferLogPath, $DebugLogPath) |
            Where-Object { ($_.Length -gt 0) -and (Test-Path $_) } |
            Select-Object -Unique
        foreach ($frameSourcePath in $frameSourcePaths) {
            $frameLine = Get-Content $frameSourcePath -ErrorAction SilentlyContinue |
                Where-Object {
                    ($_ -match 'framebuffer mode .* ([0-9]+)x([0-9]+)') -or
                    ($_ -match 'fb-geometry ([0-9]+)x([0-9]+)') -or
                    ($_ -match 'framebuffer-width ([0-9]+) framebuffer-height ([0-9]+)')
                } |
                Select-Object -Last 1
            if ($frameLine) {
                if (($frameLine -match 'fb-geometry ([0-9]+)x([0-9]+)') -or ($frameLine -match 'framebuffer-width ([0-9]+) framebuffer-height ([0-9]+)') -or ($frameLine -match 'framebuffer mode .*? ([0-9]+)x([0-9]+)\s')) {
                    $frameWidth = [int]$Matches[1]
                    $frameHeight = [int]$Matches[2]
                    break
                }
            }
        }

        # The guest initializes the brokered mouse cursor from the GOP bounds.
        # Keep the QMP-side relative mouse model aligned with that runtime
        # state so GUI clicks prove the intended hit-test path.
        $cursor = @{ X = [int]($frameWidth / 2); Y = [int]($frameHeight / 2) }
        $sendMoveTo = {
            param([int]$X, [int]$Y)

            while (($cursor.X -ne $X) -or ($cursor.Y -ne $Y)) {
                $dx = $X - $cursor.X
                $dy = $Y - $cursor.Y
                if ($dx -gt 80) { $dx = 80 }
                if ($dx -lt -80) { $dx = -80 }
                if ($dy -gt 80) { $dy = 80 }
                if ($dy -lt -80) { $dy = -80 }
                if ($dx -ne 0) {
                    $writer.WriteLine(('{{"execute":"input-send-event","arguments":{{"events":[{{"type":"rel","data":{{"axis":"x","value":{0}}}}}]}}}}' -f $dx))
                    & $drainQmp
                }
                if ($dy -ne 0) {
                    $writer.WriteLine(('{{"execute":"input-send-event","arguments":{{"events":[{{"type":"rel","data":{{"axis":"y","value":{0}}}}}]}}}}' -f $dy))
                    & $drainQmp
                }
                $cursor.X += $dx
                $cursor.Y += $dy
                Start-Sleep -Milliseconds 55
            }
        }
        $sendHome = {
            for ($homeStep = 0; $homeStep -lt 20; $homeStep++) {
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"x","value":-80}}]}}')
                & $drainQmp
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"rel","data":{"axis":"y","value":-80}}]}}')
                & $drainQmp
                Start-Sleep -Milliseconds 55
            }
            $cursor.X = 0
            $cursor.Y = 0
        }
        $sendClick = {
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds 120
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds 220
        }
        $sendRightClick = {
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"right"}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds 120
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"right"}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds 220
        }
        $sendWheelUp = {
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"wheel-up"}},{"type":"btn","data":{"down":false,"button":"wheel-up"}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds 160
        }
        $sendKey = {
            param([string]$Key)

            $writer.WriteLine(('{{"execute":"input-send-event","arguments":{{"events":[{{"type":"key","data":{{"down":true,"key":{{"type":"qcode","data":"{0}"}}}}}}]}}}}' -f $Key))
            & $drainQmp
            Start-Sleep -Milliseconds $keyHoldMilliseconds
            $writer.WriteLine(('{{"execute":"input-send-event","arguments":{{"events":[{{"type":"key","data":{{"down":false,"key":{{"type":"qcode","data":"{0}"}}}}}}]}}}}' -f $Key))
            & $drainQmp
            Start-Sleep -Milliseconds $KeyDelayMilliseconds
            if ($Key -eq "ret") {
                Start-Sleep -Milliseconds $LineDelayMilliseconds
            }
        }
        $sendShiftedKey = {
            param([string]$Key)

            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"down":true,"key":{"type":"qcode","data":"shift"}}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds $keyHoldMilliseconds
            & $sendKey $Key
            $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"key","data":{"down":false,"key":{"type":"qcode","data":"shift"}}}]}}')
            & $drainQmp
            Start-Sleep -Milliseconds $KeyDelayMilliseconds
        }
        $sendTextLine = {
            param([string]$Text)

            foreach ($character in $Text.ToCharArray()) {
                if ($character -eq '|') {
                    & $sendShiftedKey "backslash"
                    continue
                }
                if ($character -eq ':') {
                    & $sendShiftedKey "semicolon"
                    continue
                }
                if ($character -eq '$') {
                    & $sendShiftedKey "4"
                    continue
                }
                if ($character -eq '>') {
                    & $sendShiftedKey "dot"
                    continue
                }
                if ($character -eq '_') {
                    & $sendShiftedKey "minus"
                    continue
                }
                if (($character -ge 'A') -and ($character -le 'Z')) {
                    & $sendShiftedKey ([string]$character).ToLowerInvariant()
                    continue
                }
                $key = switch ($character) {
                    ' ' { "spc"; break }
                    '.' { "dot"; break }
                    '-' { "minus"; break }
                    '=' { "equal"; break }
                    '/' { "slash"; break }
                    "'" { "apostrophe"; break }
                    ';' { "semicolon"; break }
                    default { ([string]$character).ToLowerInvariant(); break }
                }
                & $sendKey $key
            }
            & $sendKey "ret"
        }

        if ($LoginProbeEnabled) {
            Start-Sleep -Milliseconds 300
            $authDeadline = [DateTime]::UtcNow.AddSeconds(240)
            $setupSent = $false
            $loginSent = $false
            while (([DateTime]::UtcNow -lt $authDeadline) -and (-not $loginSent)) {
                $logText = ""
                if (($DebugLogPath.Length -gt 0) -and (Test-Path $DebugLogPath)) {
                    $logText = Get-Content -Path $DebugLogPath -Raw -ErrorAction SilentlyContinue
                }
                if ((-not $setupSent) -and ($logText -match '\[x64\] first-run setup input wait')) {
                    & $sendTextLine "limitless"
                    & $sendTextLine "limitless"
                    $setupSent = $true
                    Start-Sleep -Milliseconds 1200
                    continue
                }
                if ($logText -match '\[x64\] login input wait') {
                    & $sendTextLine "limitless"
                    & $sendTextLine "limitless"
                    $loginSent = $true
                    Start-Sleep -Milliseconds 1200
                    break
                }
                if (($logText -match '\[x64\] stage LOGIN OK') -or ($logText -match '\[x64:shell\] persistent ring3 shell online')) {
                    $loginSent = $true
                    Start-Sleep -Milliseconds 1200
                    break
                }
                Start-Sleep -Milliseconds 120
            }
            if (-not $loginSent) {
                throw "QEMU verification failed: login prompt was not observed."
            }
        }

        if ($GuiProbeEnabled) {
            Start-Sleep -Milliseconds 300

            $taskbarY = $frameHeight - 24 - 32
            $launcherY = $taskbarY + 16
            $panelY = $taskbarY - 168 - 8
            $terminalIconY = $panelY + 52
            $fileIconY = $panelY + 88
            $settingsIconY = $panelY + 52
            $installerIconY = $panelY + 88
            $assistantIconY = $panelY + 124
            $newTerminalWidth = [Math]::Min(760, [Math]::Max(160, $frameWidth - 96))
            $dragStartX = 120
            $dragStartY = 110
            $dragEndX = 180
            $dragEndY = 150
            $newTerminalX = 66 + ($dragEndX - $dragStartX)
            $newTerminalY = 98 + ($dragEndY - $dragStartY)
            $originalTerminalWidth = [Math]::Min(920, [Math]::Max(160, $frameWidth - 64))
            $originalCloseX = 32 + $originalTerminalWidth - 15
            $originalMinimizeX = 32 + $originalTerminalWidth - 44
            $sideReserved = if ($frameWidth -gt 960) { 336 + (16 * 2) } else { 0 }
            $sideRightEdge = if (($frameWidth -gt $sideReserved) -and (($frameWidth - $sideReserved) -gt 384)) { $frameWidth - $sideReserved } else { $frameWidth }
            $fileWindowX = if ($sideRightEdge -gt 368) { $sideRightEdge - 344 } else { 24 }
            $fileBodyX = $fileWindowX + 16
            $fileBodyY = 64 + 28 + 14
            $fileButtonX = $fileBodyX + 53

            if ($DebugLogPath.Length -gt 0) {
                try {
                    Wait-ForLogPattern -Path $DebugLogPath -Pattern '\[x64\] gui interactive input wait' -TimeoutMilliseconds 30000
                }
                catch {
                    $debugText = ""
                    if (Test-Path $DebugLogPath) {
                        $debugText = Get-Content -Path $DebugLogPath -Raw -ErrorAction SilentlyContinue
                    }
                    if (($debugText -notmatch '\[x64\] persistent ring3 shell default') -and
                        ($debugText -notmatch '\[x64:shell\] persistent ring3 shell online')) {
                        throw
                    }
                }
            }
            else {
                Start-Sleep -Milliseconds 5200
            }
            foreach ($frameSourcePath in $frameSourcePaths) {
                $frameLine = Get-Content $frameSourcePath -ErrorAction SilentlyContinue |
                    Where-Object {
                        ($_ -match 'framebuffer mode .* ([0-9]+)x([0-9]+)') -or
                        ($_ -match 'fb-geometry ([0-9]+)x([0-9]+)') -or
                        ($_ -match 'framebuffer-width ([0-9]+) framebuffer-height ([0-9]+)')
                    } |
                    Select-Object -Last 1
                if ($frameLine) {
                    if (($frameLine -match 'fb-geometry ([0-9]+)x([0-9]+)') -or ($frameLine -match 'framebuffer-width ([0-9]+) framebuffer-height ([0-9]+)') -or ($frameLine -match 'framebuffer mode .*? ([0-9]+)x([0-9]+)\s')) {
                        $frameWidth = [int]$Matches[1]
                        $frameHeight = [int]$Matches[2]
                        break
                    }
                }
            }
            $cursor.X = [int]($frameWidth / 2)
            $cursor.Y = [int]($frameHeight / 2)
            $taskbarY = $frameHeight - 24 - 32
            $launcherY = $taskbarY + 16
            $panelY = $taskbarY - 168 - 8
            $terminalIconY = $panelY + 52
            $fileIconY = $panelY + 88
            $settingsIconY = $panelY + 52
            $installerIconY = $panelY + 88
            $assistantIconY = $panelY + 124
            $newTerminalWidth = [Math]::Min(760, [Math]::Max(160, $frameWidth - 96))
            $newTerminalX = 66 + ($dragEndX - $dragStartX)
            $newTerminalY = 98 + ($dragEndY - $dragStartY)
            $originalTerminalWidth = [Math]::Min(920, [Math]::Max(160, $frameWidth - 64))
            $originalCloseX = 32 + $originalTerminalWidth - 15
            $originalMinimizeX = 32 + $originalTerminalWidth - 44
            $sideReserved = if ($frameWidth -gt 960) { 336 + (16 * 2) } else { 0 }
            $sideRightEdge = if (($frameWidth -gt $sideReserved) -and (($frameWidth - $sideReserved) -gt 384)) { $frameWidth - $sideReserved } else { $frameWidth }
            $fileWindowX = if ($sideRightEdge -gt 368) { $sideRightEdge - 344 } else { 24 }
            $fileBodyX = $fileWindowX + 16
            $fileBodyY = 64 + 28 + 14
            $fileButtonX = $fileBodyX + 53
            for ($guiAttempt = 0; $guiAttempt -lt 2; $guiAttempt++) {
                & $sendHome
                & $sendMoveTo 22 $launcherY
                & $sendClick
                & $sendMoveTo 70 $terminalIconY
                & $sendClick
                & $sendMoveTo $dragStartX $dragStartY
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}')
                & $drainQmp
                Start-Sleep -Milliseconds 160
                & $sendMoveTo $dragEndX $dragEndY
                Start-Sleep -Milliseconds 160
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}')
                & $drainQmp
                Start-Sleep -Milliseconds 260
                & $sendMoveTo ([Math]::Min($frameWidth - 40, $newTerminalX + 150)) ($newTerminalY + 116)
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}')
                & $drainQmp
                Start-Sleep -Milliseconds 120
                & $sendMoveTo ([Math]::Min($frameWidth - 40, $newTerminalX + 330)) ($newTerminalY + 172)
                Start-Sleep -Milliseconds 120
                $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}')
                & $drainQmp
                Start-Sleep -Milliseconds 220
                & $sendWheelUp
                if ($guiAttempt -eq 0) {
                    & $sendMoveTo ([Math]::Min($frameWidth - 24, $newTerminalX + $newTerminalWidth - 10)) ([Math]::Min($frameHeight - 64, $newTerminalY + 410))
                    $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":true,"button":"left"}}]}}')
                    & $drainQmp
                    Start-Sleep -Milliseconds 120
                    & $sendMoveTo ([Math]::Min($frameWidth - 24, $newTerminalX + $newTerminalWidth - 70)) ([Math]::Min($frameHeight - 64, $newTerminalY + 360))
                    Start-Sleep -Milliseconds 120
                    $writer.WriteLine('{"execute":"input-send-event","arguments":{"events":[{"type":"btn","data":{"down":false,"button":"left"}}]}}')
                    & $drainQmp
                    Start-Sleep -Milliseconds 220
                    & $sendMoveTo $originalMinimizeX 63
                    & $sendClick
                    & $sendMoveTo 90 $launcherY
                    & $sendClick
                }
                if ($guiAttempt -eq 0) {
                    $settingsX = [Math]::Max(40, $frameWidth - 368 - 344 + 40)
                    & $sendMoveTo 22 $launcherY
                    & $sendClick
                    & $sendMoveTo 170 $settingsIconY
                    & $sendClick
                    Start-Sleep -Milliseconds 180
                    & $sendMoveTo $settingsX 532
                    & $sendClick
                    & $sendMoveTo $settingsX 576
                    & $sendClick
                    & $sendMoveTo $settingsX 664
                    & $sendClick
                    & $sendMoveTo $settingsX 708
                    & $sendClick
                }
                & $sendMoveTo 22 $launcherY
                & $sendClick
                & $sendMoveTo 70 $fileIconY
                & $sendClick
                if ($guiAttempt -eq 0) {
                    & $sendMoveTo ($fileWindowX + 72) 92
                    & $sendRightClick
                    & $sendMoveTo ($fileWindowX + 96) 105
                    & $sendClick
                }
                if ($guiAttempt -eq 0) {
                    & $sendMoveTo $fileButtonX ($fileBodyY + 142)
                    & $sendClick
                    foreach ($folderClickY in @(162, 166, 170)) {
                        & $sendMoveTo $fileButtonX ($fileBodyY + $folderClickY)
                        & $sendClick
                    }
                    Start-Sleep -Milliseconds 120
                    & $sendKey "ret"
                    & $sendKey "z"
                }
                & $sendMoveTo 22 $launcherY
                & $sendClick
                & $sendMoveTo 170 $settingsIconY
                & $sendClick
                & $sendMoveTo 22 $launcherY
                & $sendClick
                & $sendMoveTo 170 $installerIconY
                & $sendClick
                & $sendMoveTo 22 $launcherY
                & $sendClick
                & $sendMoveTo 70 $assistantIconY
                & $sendClick
                & $sendMoveTo 90 $launcherY
                & $sendClick
                & $sendKey "a"
                & $sendMoveTo $originalCloseX 63
                & $sendClick
                & $sendMoveTo ([Math]::Min($frameWidth - 40, $newTerminalX + [int]($newTerminalWidth / 2))) ($newTerminalY + 120)
                & $sendClick
                Start-Sleep -Milliseconds 350
            }
        }
        else {
            & $sendMoveTo 560 420
            Start-Sleep -Milliseconds 300
            & $sendKey "ret"
            Start-Sleep -Milliseconds 100
        }

        if ($DebugLogPath.Length -gt 0) {
            if ($GuiProbeEnabled) {
                Wait-ForLogPattern -Path $DebugLogPath -Pattern '\[x64\] drs-app-m21 ' -TimeoutMilliseconds 600000
                Start-Sleep -Milliseconds 500
                & $sendKey "ret"
                Wait-ForLogPattern -Path $DebugLogPath -Pattern '\[x64:shell\] persistent ring3 shell online' -TimeoutMilliseconds 600000
                Start-Sleep -Milliseconds 1200
            }
            else {
                Wait-ForLogPattern -Path $DebugLogPath -Pattern '\[x64:shell\] persistent ring3 shell online' -TimeoutMilliseconds 600000
                Start-Sleep -Milliseconds 500
            }
        }

        if ($RealBinaryProbeEnabled) {
            $realBinaryTextLines = @($ExtraTextLines | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            foreach ($extraTextLine in $realBinaryTextLines) {
                if (-not [string]::IsNullOrWhiteSpace($extraTextLine)) {
                    & $sendTextLine $extraTextLine
                }
            }
            if ($realBinaryTextLines.Count -gt 1) {
                & $sendTextLine "exit"
            }
            Wait-ForAnyLogPattern -Paths @($DebugLogPath, $FramebufferLogPath) -Pattern 'drs-realbin' -TimeoutMilliseconds 180000
            return
        }
        if ($HardwareRegistryProbeEnabled -or $HardwareDisplayProbeEnabled -or $HardwareStorageProbeEnabled) {
            & $sendTextLine "hwval full"
            if ($HardwareRegistryProbeEnabled) {
                Wait-ForAnyLogPattern -Paths @($DebugLogPath, $FramebufferLogPath) -Pattern 'drs-hardware-registry' -TimeoutMilliseconds 180000
            }
            if ($HardwareDisplayProbeEnabled) {
                Wait-ForAnyLogPattern -Paths @($DebugLogPath, $FramebufferLogPath) -Pattern 'drs-display-readability' -TimeoutMilliseconds 180000
            }
            if ($HardwareStorageProbeEnabled) {
                Wait-ForAnyLogPattern -Paths @($DebugLogPath, $FramebufferLogPath) -Pattern 'drs-nvme-triage' -TimeoutMilliseconds 180000
            }
            & $sendTextLine "exit"
            return
        }

        if ($DebugLogPath.Length -gt 0) {
            $shellSynced = $false
            for ($shellSyncAttempt = 0; $shellSyncAttempt -lt 4; $shellSyncAttempt++) {
                & $sendTextLine "ls"
                Start-Sleep -Milliseconds 900
                $shellSyncText = ""
                if (Test-Path $DebugLogPath) {
                    $shellSyncText = Get-Content -Path $DebugLogPath -Raw -ErrorAction SilentlyContinue
                }
                if ($shellSyncText -match '\[x64\] \$ ls') {
                    $shellSynced = $true
                    break
                }
                & $sendKey "ret"
                Start-Sleep -Milliseconds 900
            }
            if (-not $shellSynced) {
                & $sendTextLine "ls"
            }
        }

        $shellTextLines = @(
            "help",
            "help cat",
            "help write",
            "apps",
            "pwd",
            "ls",
            "ls apps",
            "cat readme.txt",
            "stat readme.txt",
            "info write",
            "net",
            "net curl example.com",
            "pkginfo",
            "hwval full"
        )
        if ($LoginProbeEnabled) {
            $shellTextLines += @(
                "lock",
                "limitless"
            )
        }
        $shellTextLines += @(
            "write w.txt ok",
            "cat w.txt"
        )
        foreach ($shellTextLine in $shellTextLines) {
            if (-not [string]::IsNullOrWhiteSpace($shellTextLine)) {
                & $sendTextLine $shellTextLine
            }
        }
        foreach ($extraTextLine in $ExtraTextLines) {
            if (-not [string]::IsNullOrWhiteSpace($extraTextLine)) {
                & $sendTextLine $extraTextLine
            }
        }
        & $sendTextLine "exit"

        if (($DebugLogPath.Length -gt 0) -and $GuiProbeEnabled) {
            Wait-ForLogPattern -Path $DebugLogPath -Pattern '\[x64:shell\] persistent ring3 shell online' -TimeoutMilliseconds 600000
        }

        $remainingMilliseconds = [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds
        if ($remainingMilliseconds -gt 0) {
            Start-Sleep -Milliseconds $remainingMilliseconds
        }
    }
    finally {
        $client.Dispose()
    }
}

$root = Split-Path -Parent $PSScriptRoot
$logStem = "qemu-{0}-{1}" -f $Architecture, $BootMedia
if ($NetworkDevice -ne "virtio") {
    $logStem = "{0}-{1}" -f $logStem, $NetworkDevice
}
$mediaPath = if ($Architecture -eq "x86") {
    if ($BootMedia -eq "disk") {
        Join-Path $root "dist\\limitlessos.img"
    }
    elseif ($BootMedia -eq "iso") {
        Join-Path $root "dist\\limitlessos.iso"
    }
    else {
        throw "UEFI boot media is only available on the x86_64 lane."
    }
} else {
    if ($BootMedia -eq "disk") {
        Join-Path $root "dist\\limitlessos-x86_64.img"
    }
    elseif ($BootMedia -eq "iso") {
        Join-Path $root "dist\\limitlessos-x86_64.iso"
    }
    else {
        Join-Path $root "dist\\limitlessos-x86_64-uefi.img"
    }
}
$logPath = Join-Path $root ("build\\{0}-debug.log" -f $logStem)
$serialLogPath = Join-Path $root ("build\\{0}-serial.log" -f $logStem)
$stderrLogPath = Join-Path $root ("build\\{0}-stderr.log" -f $logStem)
$qmpPort = if ($Architecture -eq "x86_64") { Get-Random -Minimum 42000 -Maximum 49000 } else { 0 }

if (-not (Test-Path $mediaPath)) {
    throw "Build media not found. Run .\\tools\\build.ps1 -Architecture $Architecture first."
}

if ($Architecture -eq "x86_64") {
    Assert-X64LoaderBudget -Root $root
    $m1ProductionGate = Join-Path $root "tools\assert-m1-production-slice.ps1"
    & $m1ProductionGate -Architecture x86_64 -BuildProfile $BuildProfile
    if (-not $?) {
        throw "QEMU verification failed: M1 production-slice artifact gate failed."
    }
}

$qemu = if ($Architecture -eq "x86") {
    Get-Command qemu-system-i386 -ErrorAction SilentlyContinue
} else {
    Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
}

if (-not $qemu -and ($Architecture -eq "x86")) {
    $qemu = Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue
}

if (-not $qemu) {
    $fallbackPath = if ($Architecture -eq "x86") {
        "C:\\Program Files\\qemu\\qemu-system-i386.exe"
    } else {
        "C:\\Program Files\\qemu\\qemu-system-x86_64.exe"
    }
    if (Test-Path $fallbackPath) {
        $qemu = @{ Source = $fallbackPath }
    }
}

if (-not $qemu) {
    throw "QEMU is not installed or not on PATH."
}

if (Test-Path $logPath) {
    Remove-Item $logPath -Force
}
if (Test-Path $serialLogPath) {
    Remove-Item $serialLogPath -Force
}
if (Test-Path $stderrLogPath) {
    Remove-Item $stderrLogPath -Force
}

$arguments = @(
    "-display", "none",
    "-monitor", "none",
    "-no-reboot",
    "-no-shutdown"
)

if ($Architecture -eq "x86_64") {
    $arguments += @(
        "-qmp", "tcp:127.0.0.1:$qmpPort,server=on,wait=off"
    )
}

if (($Architecture -eq "x86_64") -and ($BootMedia -ne "disk")) {
    $firmwarePath = Get-QemuEdk2CodePath
    $stageNvmeArtifacts = ($RealBinaryGate.IsPresent -or $HardwareStorageStageGate.IsPresent)
    $nvmeGptPath = Ensure-NvmeGptImage -Root $root -StageRealBinary:$stageNvmeArtifacts -StageBusyBoxPath $BusyBoxPath -StageBusyBoxSource $BusyBoxSource -StageBusyBoxVersion $BusyBoxVersion -StageExtraAppPath $ExtraAppPath -StageExtraAppName $ExtraAppName -StageExtraAppSource $ExtraAppSource -StageExtraAppVersion $ExtraAppVersion -StageExtraApp2Path $ExtraApp2Path -StageExtraApp2Name $ExtraApp2Name -StageExtraApp2Source $ExtraApp2Source -StageExtraApp2Version $ExtraApp2Version -StageExtraApp3Path $ExtraApp3Path -StageExtraApp3Name $ExtraApp3Name -StageExtraApp3Source $ExtraApp3Source -StageExtraApp3Version $ExtraApp3Version
    $networkDeviceArgument = if ($NetworkDevice -eq "e1000e") {
        "e1000e,netdev=net0,mac=52:54:00:12:34:56"
    }
    elseif ($NetworkDevice -eq "e1000") {
        "e1000,netdev=net0,mac=52:54:00:12:34:56"
    }
    else {
        "virtio-net-pci,netdev=net0,disable-legacy=on,mac=52:54:00:12:34:56"
    }
    $arguments += @(
        "-serial", "file:$serialLogPath",
        "-debugcon", "file:$logPath",
        "-global", "isa-debugcon.iobase=0xe9",
        "-machine", "q35",
        "-drive", "if=pflash,format=raw,readonly=on,file=$firmwarePath",
        "-device", "uefi-vars-x64",
        "-drive", "if=none,id=nvmeprobe,format=raw,snapshot=on,file=$nvmeGptPath",
        "-device", "nvme,drive=nvmeprobe,serial=LIMITLESSOSNVME,bootindex=3",
        "-netdev", "user,id=net0",
        "-device", $networkDeviceArgument
    )

    if ($BootMedia -eq "uefi") {
        $uefiAhciIsoPath = Join-Path $root "dist\limitlessos-x86_64.iso"
        if (-not (Test-Path $uefiAhciIsoPath)) {
            throw "UEFI AHCI ISO sidecar not found: $uefiAhciIsoPath"
        }
        $arguments += @(
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0",
            "-device", "usb-mouse,id=usbmouse,bus=xhci.0",
            "-drive", "if=none,id=usbstick,format=raw,file=$mediaPath",
            "-device", "usb-storage,bus=xhci.0,drive=usbstick,removable=true,bootindex=1",
            "-drive", "if=none,id=ahciuefi,media=cdrom,format=raw,readonly=on,file=$uefiAhciIsoPath",
            "-device", "ide-cd,drive=ahciuefi,bootindex=2"
        )
    }
    else {
        $isoUsbSidecarPath = Join-Path $root "dist\limitlessos-x86_64-uefi.img"
        if (-not (Test-Path $isoUsbSidecarPath)) {
            throw "UEFI USB sidecar image not found for ISO xHCI topology: $isoUsbSidecarPath"
        }
        $arguments += @(
            "-device", "qemu-xhci,id=xhci",
            "-device", "usb-kbd,bus=xhci.0",
            "-device", "usb-mouse,id=usbmouse,bus=xhci.0",
            "-drive", "if=none,id=isousbsidecar,format=raw,file=$isoUsbSidecarPath",
            "-device", "usb-storage,bus=xhci.0,drive=isousbsidecar,removable=true,bootindex=4",
            "-drive", "if=none,id=cdrom,media=cdrom,file=$mediaPath",
            "-device", "ide-cd,drive=cdrom,bootindex=1"
        )
    }
}
else {
    $arguments += @(
        "-serial", "none",
        "-debugcon", "file:$logPath",
        "-global", "isa-debugcon.iobase=0xe9"
    )

    if ($BootMedia -eq "disk") {
        $arguments += @(
            "-drive", "format=raw,file=$mediaPath,if=ide,index=0"
        )
    }
    else {
        $arguments += @(
            "-boot", "d",
            "-drive", "file=$mediaPath,media=cdrom,if=ide,index=1"
        )
    }
}

$argumentLine = ConvertTo-ArgumentString -Arguments $arguments
$process = Start-Process -FilePath $qemu.Source -ArgumentList $argumentLine -PassThru -WindowStyle Hidden -RedirectStandardError $stderrLogPath

try {
    $bootWaitSeconds = if (($Architecture -eq "x86_64") -and ($BootMedia -ne "disk")) { 28 } else { 4 }
    if ($Architecture -eq "x86_64") {
        Wait-ForLogPattern -Path $logPath -Pattern '\[x64\] PIT at 100 Hz' -TimeoutMilliseconds 45000

        $probeMilliseconds = if ($BootMedia -eq "disk") { 52000 } elseif ($BuildProfile -eq "Product") { 120000 } else { 56000 }
        $keyDelayMilliseconds = if ($BootMedia -eq "disk") { 180 } else { 210 }
        $lineDelayMilliseconds = if ($BootMedia -eq "disk") { 1300 } else { 5500 }
        $extraTextLines = @($ExtraShellLine | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
        $hardwareStorageProbe = ($HardwareStorageGate.IsPresent -or $HardwareStorageStageGate.IsPresent)
        $guiProbeForRun = (($BootMedia -ne "disk") -and (-not $RealBinaryGate.IsPresent) -and (-not $HardwareRegistryGate.IsPresent) -and (-not $HardwareDisplayGate.IsPresent) -and (-not $hardwareStorageProbe))
        Send-QemuKeyboardProbe -Port $qmpPort -DurationMilliseconds $probeMilliseconds -KeyDelayMilliseconds $keyDelayMilliseconds -LineDelayMilliseconds $lineDelayMilliseconds -DebugLogPath $logPath -FramebufferLogPath $serialLogPath -GuiProbeEnabled:$guiProbeForRun -LoginProbeEnabled:(($BootMedia -ne "disk") -and ($BuildProfile -eq "Product")) -RealBinaryProbeEnabled:$($RealBinaryGate.IsPresent) -HardwareRegistryProbeEnabled:$($HardwareRegistryGate.IsPresent) -HardwareDisplayProbeEnabled:$($HardwareDisplayGate.IsPresent) -HardwareStorageProbeEnabled:$hardwareStorageProbe -ExtraTextLines $extraTextLines
        if ((-not $RealBinaryGate.IsPresent) -and (-not $HardwareRegistryGate.IsPresent) -and (-not $HardwareDisplayGate.IsPresent) -and (-not $hardwareStorageProbe)) {
            Wait-ForLogPattern -Path $logPath -Pattern '\[x64\] persistent ring3 shell default' -TimeoutMilliseconds 600000
        }
    }
    Start-Sleep -Seconds $bootWaitSeconds
}
finally {
    if (-not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        if (-not $process.WaitForExit(15000)) {
            throw "QEMU verification failed: QEMU process $($process.Id) did not exit after forced stop."
        }
    }
}

$outputLines = @()

if (($Architecture -eq "x86_64") -and ($BootMedia -ne "disk")) {
    $stderrLines = @()
    if (Test-Path $stderrLogPath) {
        $stderrLines = @(Get-Content $stderrLogPath | Where-Object { $_.Trim().Length -gt 0 })
    }

    $serialLines = @()
    if (Test-Path $serialLogPath) {
        $serialLines = @(Get-Content $serialLogPath |
            ForEach-Object { Normalize-ConsoleLine -Line $_ } |
            Where-Object {
                ($_ -match 'BdsDxe:') -or
                ($_ -match 'LimitlessOS x86_64 UEFI scaffold') -or
                ($_ -match '^\[uefi\]')
            } |
            Select-Object -Unique)
    }

    $debugLines = @()
    if (Test-Path $logPath) {
        $debugLines = @(Get-Content $logPath | Where-Object { $_.Trim().Length -gt 0 })
    }

    $outputLines += $stderrLines
    $outputLines += $serialLines
    $outputLines += $debugLines
    $outputLines = @($outputLines |
        ForEach-Object { Normalize-ConsoleLine -Line $_ } |
        Where-Object { $_.Trim().Length -gt 0 })
}
elseif (Test-Path $logPath) {
    $outputLines = @(Get-Content $logPath)
}
else {
    throw "QEMU verification failed: no debug output was captured."
}

if ($RealBinaryGate.IsPresent) {
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64:shell\] persistent ring3 shell online' -Message "x64 persistent ring-3 shell banner was not observed."
    foreach ($extraTextLine in ($ExtraShellLine | Where-Object { $_ -match '^\s*linux\s+' })) {
        if (-not [string]::IsNullOrWhiteSpace($extraTextLine)) {
            $normalizedExtraTextLine = (Normalize-ConsoleLine -Line $extraTextLine).ToLowerInvariant()
            Assert-OutputContains -Lines $outputLines -Pattern ([regex]::Escape("[x64] $ $normalizedExtraTextLine")) -Message "x64 persistent shell did not accept the real-binary gate command."
        }
    }
    $realBinaryTelemetry = @($outputLines | Where-Object { $_ -match 'drs-realbin' })
    if ($realBinaryTelemetry.Count -eq 0) {
        throw "QEMU verification failed: no drs-realbin telemetry was observed."
    }
    Write-Host "QEMU real-binary telemetry:"
    foreach ($line in $realBinaryTelemetry) {
        Write-Host "  $line"
    }
    return
}
if ($HardwareRegistryGate.IsPresent) {
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ hwval' -Message "x64 persistent shell did not accept the M106 hwval command."
    Assert-OutputContains -Lines $outputLines -Pattern '^hardware validation: read-only Product mode$' -Message "x64 M106 hwval did not report read-only Product mode."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-hardware-registry hardware-registry 1 refresh [1-9][0-9]* limit 32 inventory [1-9][0-9]* pci-enumerated [1-9][0-9]* pci-query-denial 0 .* driver-bound [1-9][0-9]* .* driver-failed 0 overflow 0 token 0x[0-9A-F]{8}' -Message "x64 M106 hardware registry proof was not observed."
    $outputLines
    return
}
if ($HardwareDisplayGate.IsPresent) {
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ hwval' -Message "x64 persistent shell did not accept the M107 hwval command."
    Assert-OutputContains -Lines $outputLines -Pattern '^hardware validation: read-only Product mode$' -Message "x64 M107 hwval did not report read-only Product mode."
    Assert-OutputContains -Lines $outputLines -Pattern '^machine model: not reported by Product$' -Message "x64 M107 hwval did not report machine model status with user-facing wording."
    Assert-OutputContains -Lines $outputLines -Pattern '^secure boot: not detected by Product$' -Message "x64 M107 hwval did not report secure boot status with user-facing wording."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-display-readability display-readability 1 available 1 width [1-9][0-9]* height [1-9][0-9]* pitch [1-9][0-9]* stride-ok 1 bounds-ok 1 scale [1-3] viewport-x [0-9]+ viewport-y [0-9]+ viewport-w [1-9][0-9]* viewport-h [1-9][0-9]* columns [1-9][0-9]* rows [1-9][0-9]* fit 1 readable 1 clip [0-9]+ cursor-visible [0-1] cursor-draws [0-9]+ direct-cursor-draws [0-9]+ token 0x[0-9A-F]{8}' -Message "x64 M107 display readability proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-ui-polish ui-polish 1 compositor-active 1 compositor-direct [0-1] font 1 wm 1 desktop 1 taskbar [1-9][0-9]* launcher [1-9][0-9]* windows [1-9][0-9]* cursor-visible 1 product-chrome [0-9]+ product-layout 1 startup-minimized [0-9]+ readiness-strip [0-9]+ display-ready 1 input-ready [0-1] storage-ready [0-1] network-ready [0-1] diagnostic-overlays-suppressed [0-9]+ token 0x[0-9A-F]{8}' -Message "x64 M109 UI polish proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-cursor-path cursor-path 1 surface-ready 1 format-supported 1 compositor-active 1 compositor-direct [0-1] visible 1 draws [1-9][0-9]* direct-draws [0-9]+ x [0-9]+ y [0-9]+ buttons [0-7] in-bounds 1 rect-w [1-9][0-9]* rect-h [1-9][0-9]* saved [0-1] drawn 1 token 0x[0-9A-F]{8}' -Message "x64 M149 cursor path proof was not observed."
    $outputLines
    return
}
if ($HardwareStorageGate.IsPresent) {
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ hwval' -Message "x64 persistent shell did not accept the M110 hwval command."
    Assert-OutputContains -Lines $outputLines -Pattern '^hardware validation: read-only Product mode$' -Message "x64 M110 hwval did not report read-only Product mode."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage [1-9][0-9]* pci-nvme [1-9][0-9]* pci-raid [0-9]+ pci-other-storage [0-9]+ pci-intel-system [0-9]+ pci-vmd [0-9]+ nvme-pci 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-vendor-device 0x(?!00000000)[0-9A-F]{8} nvme-class 0x0108[0-9A-F]{4} nvme-bar0 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-bar1 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-low 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-high 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-span [1-9][0-9]* nvme-mmio-flags 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-candidate-source 1 nvme-candidate-deferred 0 nvme-candidate-bdf 0xFFFFFFFF nvme-candidate-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} other-storage-pci 0x[0-9A-F]{8} other-storage-vendor-device 0x[0-9A-F]{8} other-storage-class 0x[0-9A-F]{8} other-storage-bar0 0x[0-9A-F]{8} other-storage-bar1 0x[0-9A-F]{8} intel-system-pci 0x[0-9A-F]{8} intel-system-vendor-device 0x[0-9A-F]{8} intel-system-class 0x[0-9A-F]{8} intel-system-bar0 0x[0-9A-F]{8} intel-system-bar1 0x[0-9A-F]{8} vmd-pci 0x[0-9A-F]{8} vmd-vendor-device 0x[0-9A-F]{8} vmd-class 0x[0-9A-F]{8} vmd-bar0 0x[0-9A-F]{8} vmd-bar1 0x[0-9A-F]{8} vmd-mmio-low 0x[0-9A-F]{8} vmd-mmio-high 0x[0-9A-F]{8} vmd-mmio-span [0-9]+ vmd-mmio-flags 0x[0-9A-F]{8} vmd-mmio-token 0x[0-9A-F]{8} vmd-nested-plan [0-9]+ vmd-nested-enum [0-9]+ vmd-nested-nvme [0-9]+ vmd-nested-status [0-9]+ vmd-nested-token 0x[0-9A-F]{8} vmd-nested-pci 0x[0-9A-F]{8} vmd-nested-vendor-device 0x[0-9A-F]{8} vmd-nested-class 0x[0-9A-F]{8} vmd-nested-bar0 0x[0-9A-F]{8} vmd-nested-bar1 0x[0-9A-F]{8} vmd-nested-scan-buses [0-9]+ vmd-nested-scan-devices [0-9]+ vmd-nested-scan-functions [0-9]+ vmd-nested-scan-windows [0-9]+ vmd-nested-scan-truncated [01] vmd-nested-mmio-low 0x[0-9A-F]{8} vmd-nested-mmio-high 0x[0-9A-F]{8} vmd-nested-mmio-span [0-9]+ vmd-nested-mmio-flags 0x[0-9A-F]{8} vmd-nested-mmio-token 0x[0-9A-F]{8} vmd-nested-bind-ready [0-9]+ vmd-nested-bind-status [0-9]+ vmd-nested-bind-token 0x[0-9A-F]{8} vmd-nested-register-candidate [0-9]+ vmd-nested-register-status [0-9]+ vmd-nested-register-token 0x[0-9A-F]{8} vmd-nested-driver-plan-result 0x[0-9A-F]{8} vmd-nested-driver-plan-state [0-9]+ vmd-nested-driver-plan-flags 0x[0-9A-F]{8} vmd-nested-driver-plan-token 0x[0-9A-F]{8} vmd-nested-driver-plan-stage-count [0-9]+ vmd-nested-driver-plan-denials [0-9]+ vmd-nested-driver-plan-unavailable [0-9]+ nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions [1-9][0-9]* fat32-start [1-9][0-9]* fat32-sectors [1-9][0-9]* gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat [01] busybox-bytes [0-9]+ dynldlimit-stat [01] dynldlimit-bytes [0-9]+ ldlimit-stat [01] ldlimit-bytes [0-9]+ boot-staged [01] boot-app-bytes [0-9]+ boot-interp-bytes [0-9]+ boot-status [0-9]+ stage-expected [01] dynldlimit-expected [01] ldlimit-expected [01] dynldlimit-match [01] ldlimit-match [01] stage-match [01] token 0x[0-9A-F]{8}' -Message "x64 M110 NVMe/FAT storage triage proof was not observed."
    $outputLines
    return
}
if ($HardwareStorageStageGate.IsPresent) {
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ hwval' -Message "x64 persistent shell did not accept the M111 hwval command."
    Assert-OutputContains -Lines $outputLines -Pattern '^hardware validation: read-only Product mode$' -Message "x64 M111 hwval did not report read-only Product mode."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-triage storage-triage 1 nvme-found 1 pci-storage [1-9][0-9]* pci-nvme [1-9][0-9]* pci-raid [0-9]+ pci-other-storage [0-9]+ pci-intel-system [0-9]+ pci-vmd [0-9]+ nvme-pci 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-vendor-device 0x(?!00000000)[0-9A-F]{8} nvme-class 0x0108[0-9A-F]{4} nvme-bar0 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-bar1 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-low 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-high 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-span [1-9][0-9]* nvme-mmio-flags 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-candidate-source 1 nvme-candidate-deferred 0 nvme-candidate-bdf 0xFFFFFFFF nvme-candidate-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} other-storage-pci 0x[0-9A-F]{8} other-storage-vendor-device 0x[0-9A-F]{8} other-storage-class 0x[0-9A-F]{8} other-storage-bar0 0x[0-9A-F]{8} other-storage-bar1 0x[0-9A-F]{8} intel-system-pci 0x[0-9A-F]{8} intel-system-vendor-device 0x[0-9A-F]{8} intel-system-class 0x[0-9A-F]{8} intel-system-bar0 0x[0-9A-F]{8} intel-system-bar1 0x[0-9A-F]{8} vmd-pci 0x[0-9A-F]{8} vmd-vendor-device 0x[0-9A-F]{8} vmd-class 0x[0-9A-F]{8} vmd-bar0 0x[0-9A-F]{8} vmd-bar1 0x[0-9A-F]{8} vmd-mmio-low 0x[0-9A-F]{8} vmd-mmio-high 0x[0-9A-F]{8} vmd-mmio-span [0-9]+ vmd-mmio-flags 0x[0-9A-F]{8} vmd-mmio-token 0x[0-9A-F]{8} vmd-nested-plan [0-9]+ vmd-nested-enum [0-9]+ vmd-nested-nvme [0-9]+ vmd-nested-status [0-9]+ vmd-nested-token 0x[0-9A-F]{8} vmd-nested-pci 0x[0-9A-F]{8} vmd-nested-vendor-device 0x[0-9A-F]{8} vmd-nested-class 0x[0-9A-F]{8} vmd-nested-bar0 0x[0-9A-F]{8} vmd-nested-bar1 0x[0-9A-F]{8} vmd-nested-scan-buses [0-9]+ vmd-nested-scan-devices [0-9]+ vmd-nested-scan-functions [0-9]+ vmd-nested-scan-windows [0-9]+ vmd-nested-scan-truncated [01] vmd-nested-mmio-low 0x[0-9A-F]{8} vmd-nested-mmio-high 0x[0-9A-F]{8} vmd-nested-mmio-span [0-9]+ vmd-nested-mmio-flags 0x[0-9A-F]{8} vmd-nested-mmio-token 0x[0-9A-F]{8} vmd-nested-bind-ready [0-9]+ vmd-nested-bind-status [0-9]+ vmd-nested-bind-token 0x[0-9A-F]{8} vmd-nested-register-candidate [0-9]+ vmd-nested-register-status [0-9]+ vmd-nested-register-token 0x[0-9A-F]{8} vmd-nested-driver-plan-result 0x[0-9A-F]{8} vmd-nested-driver-plan-state [0-9]+ vmd-nested-driver-plan-flags 0x[0-9A-F]{8} vmd-nested-driver-plan-token 0x[0-9A-F]{8} vmd-nested-driver-plan-stage-count [0-9]+ vmd-nested-driver-plan-denials [0-9]+ vmd-nested-driver-plan-unavailable [0-9]+ nvme-ready 1 nvme-identify 1 ioq 1 read-issued 1 read-completed 1 read-status 0 gpt-signature 1 gpt-partitions [1-9][0-9]* fat32-start [1-9][0-9]* fat32-sectors [1-9][0-9]* gpt-vbr 1 fat-bpb 1 fat-located 1 fat-unavailable 0 fat-error 0 rw-cap 1 rw-delegated 1 rw-error 0 apps-stat 1 apps-type 2 apps-dirent 1 apps-dir-result 1 busybox-stat [01] busybox-bytes [0-9]+ dynldlimit-stat 1 dynldlimit-bytes [1-9][0-9]+ ldlimit-stat 1 ldlimit-bytes [1-9][0-9]+ boot-staged 1 boot-app-bytes [1-9][0-9]+ boot-interp-bytes [1-9][0-9]+ boot-status 0 stage-expected 1 dynldlimit-expected 1 ldlimit-expected 1 dynldlimit-match 1 ldlimit-match 1 stage-match 1 token 0x[0-9A-F]{8}' -Message "x64 M111 staged boot/NVMe artifact proof was not observed."
    $outputLines
    return
}

if ($Architecture -eq "x86_64") {
    foreach ($line in $outputLines) {
        if ($line -match '^\[x64\] mmio planner service') {
            $readStatusFsShellLegacyLine = [regex]::Replace($line, ' drs-fs-shell .*? drs-fs-shell-unavailable [0-9]+', '')
            if ($readStatusFsShellLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusFsShellLegacyLine
                $line = $readStatusFsShellLegacyLine
            }

            $readStatusFsUserLegacyLine = [regex]::Replace($line, ' drs-fs-user .*? drs-fs-user-unavailable [0-9]+', '')
            if ($readStatusFsUserLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusFsUserLegacyLine
                $line = $readStatusFsUserLegacyLine
            }

            $readStatusFsLegacyLine = [regex]::Replace($line, ' drs-fs .*? drs-fs-unavailable [0-9]+', '')
            if ($readStatusFsLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusFsLegacyLine
                $line = $readStatusFsLegacyLine
            }

            $readStatusBlockLegacyLine = [regex]::Replace($line, ' denied-drs-block .*? drs-block-unavailable [0-9]+', '')
            $readStatusBlockLegacyLine = $readStatusBlockLegacyLine -replace ' queries 359 denials 98', ' queries 359 denials 97'
            $readStatusBlockLegacyLine = $readStatusBlockLegacyLine -replace ' queries 359 denials 101', ' queries 359 denials 99'
            if ($readStatusBlockLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusBlockLegacyLine
                $line = $readStatusBlockLegacyLine
            }

            $readStatusReadLegacyLine = [regex]::Replace($line, ' drs-read .*? drs-read-unavailable [0-9]+', '')
            if ($readStatusReadLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusReadLegacyLine
                $line = $readStatusReadLegacyLine
            }

            $readStatusDmaWindowLegacyLine = [regex]::Replace($line, ' denied-drs-dwin .*? drs-dwin-unavailable [0-9]+', '')
            $readStatusDmaWindowLegacyLine = $readStatusDmaWindowLegacyLine -replace ' queries 359 denials 97', ' queries 359 denials 96'
            $readStatusDmaWindowLegacyLine = $readStatusDmaWindowLegacyLine -replace ' queries 359 denials 99', ' queries 359 denials 97'
            if ($readStatusDmaWindowLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusDmaWindowLegacyLine
                $line = $readStatusDmaWindowLegacyLine
            }

            $readStatusMmioLegacyLine = [regex]::Replace($line, ' denied-drs-mmio .*? drs-mmio-unavailable [0-9]+', '')
            $readStatusMmioLegacyLine = $readStatusMmioLegacyLine -replace ' queries 359 denials 97', ' queries 359 denials 95'
            $readStatusMmioLegacyLine = $readStatusMmioLegacyLine -replace ' queries 359 denials 96', ' queries 359 denials 95'
            if ($readStatusMmioLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusMmioLegacyLine
                $line = $readStatusMmioLegacyLine
            }

            $readStatusDmaLegacyLine = [regex]::Replace($line, ' denied-drs-dma .*? drs-dma-unavailable [0-9]+', '')
            $readStatusDmaLegacyLine = $readStatusDmaLegacyLine -replace ' queries 359 denials 95', ' queries 359 denials 94'
            if ($readStatusDmaLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusDmaLegacyLine
                $line = $readStatusDmaLegacyLine
            }

            $readStatusExecLegacyLine = [regex]::Replace($line, ' denied-drs-exec .*? drs-exec-unavailable [0-9]+', '')
            $readStatusExecLegacyLine = $readStatusExecLegacyLine -replace ' queries 359 denials 94', ' queries 359 denials 93'
            if ($readStatusExecLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusExecLegacyLine
                $line = $readStatusExecLegacyLine
            }

            $readStatusArmLegacyLine = [regex]::Replace($line, ' denied-drs-arm .*? drs-arm-unavailable [0-9]+', '')
            $readStatusArmLegacyLine = $readStatusArmLegacyLine -replace ' queries 359 denials 93', ' queries 359 denials 92'
            if ($readStatusArmLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusArmLegacyLine
                $line = $readStatusArmLegacyLine
            }

            $readStatusIssueGrantLegacyLine = [regex]::Replace($line, ' denied-drs-grant .*? drs-grant-unavailable [0-9]+', '')
            $readStatusIssueGrantLegacyLine = $readStatusIssueGrantLegacyLine -replace ' queries 359 denials 92', ' queries 359 denials 91'
            if ($readStatusIssueGrantLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusIssueGrantLegacyLine
                $line = $readStatusIssueGrantLegacyLine
            }

            $readStatusCommandIssueLegacyLine = [regex]::Replace($line, ' denied-drs-issue .*? drs-issue-unavailable [0-9]+', '')
            $readStatusCommandIssueLegacyLine = $readStatusCommandIssueLegacyLine -replace ' queries 359 denials 91', ' queries 359 denials 90'
            if ($readStatusCommandIssueLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusCommandIssueLegacyLine
                $line = $readStatusCommandIssueLegacyLine
            }

            $readStatusCommandTableLegacyLine = [regex]::Replace($line, ' denied-drs-ctab .*? drs-ctab-unavailable [0-9]+', '')
            $readStatusCommandTableLegacyLine = $readStatusCommandTableLegacyLine -replace ' queries 359 denials 90', ' queries 359 denials 89'
            if ($readStatusCommandTableLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusCommandTableLegacyLine
                $line = $readStatusCommandTableLegacyLine
            }

            $readStatusDescriptorLegacyLine = [regex]::Replace($line, ' denied-drs-desc .*? drs-desc-unavailable [0-9]+', '')
            $readStatusDescriptorLegacyLine = $readStatusDescriptorLegacyLine -replace ' queries 359 denials 89', ' queries 359 denials 88'
            if ($readStatusDescriptorLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusDescriptorLegacyLine
                $line = $readStatusDescriptorLegacyLine
            }

            $readStatusReadAuthorityLegacyLine = [regex]::Replace($line, ' denied-drs-rauth .*? drs-rauth-unavailable [0-9]+', '')
            $readStatusReadAuthorityLegacyLine = $readStatusReadAuthorityLegacyLine -replace ' queries 359 denials 88', ' queries 359 denials 87'
            if ($readStatusReadAuthorityLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusReadAuthorityLegacyLine
                $line = $readStatusReadAuthorityLegacyLine
            }

            $readStatusWorkerLegacyLine = [regex]::Replace($line, ' denied-drs-w .*? drs-w-unavailable [0-9]+', '')
            $readStatusWorkerLegacyLine = $readStatusWorkerLegacyLine -replace ' queries 359 denials 87', ' queries 359 denials 86'
            if ($readStatusWorkerLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusWorkerLegacyLine
                $line = $readStatusWorkerLegacyLine
            }

            $readStatusQueueLegacyLine = [regex]::Replace($line, ' denied-drs-queue .*? drs-queue-unavailable [0-9]+', '')
            $readStatusQueueLegacyLine = $readStatusQueueLegacyLine -replace ' queries 359 denials 86', ' queries 359 denials 85'
            if ($readStatusQueueLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusQueueLegacyLine
                $line = $readStatusQueueLegacyLine
            }

            $readStatusDispatchLegacyLine = [regex]::Replace($line, ' denied-drs-dispatch .*? drs-dispatch-unavailable [0-9]+', '')
            $readStatusDispatchLegacyLine = $readStatusDispatchLegacyLine -replace ' queries 359 denials 85', ' queries 359 denials 84'
            if ($readStatusDispatchLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusDispatchLegacyLine
                $line = $readStatusDispatchLegacyLine
            }

            $readStatusAuthorizeLegacyLine = [regex]::Replace($line, ' denied-drs-authz .*? drs-authz-unavailable [0-9]+', '')
            $readStatusAuthorizeLegacyLine = $readStatusAuthorizeLegacyLine -replace ' queries 359 denials 84', ' queries 359 denials 83'
            if ($readStatusAuthorizeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusAuthorizeLegacyLine
                $line = $readStatusAuthorizeLegacyLine
            }

            $readStatusFinalizeLegacyLine = [regex]::Replace($line, ' denied-drs-final .*? drs-final-unavailable [0-9]+', '')
            $readStatusFinalizeLegacyLine = $readStatusFinalizeLegacyLine -replace ' queries 359 denials 83', ' queries 359 denials 82'
            if ($readStatusFinalizeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusFinalizeLegacyLine
                $line = $readStatusFinalizeLegacyLine
            }

            $readStatusDiscardLegacyLine = [regex]::Replace($line, ' denied-drs-discard .*? drs-discard-unavailable [0-9]+', '')
            $readStatusDiscardLegacyLine = $readStatusDiscardLegacyLine -replace ' queries 359 denials 82', ' queries 359 denials 81'
            if ($readStatusDiscardLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusDiscardLegacyLine
                $line = $readStatusDiscardLegacyLine
            }

            $readStatusUnsealLegacyLine = [regex]::Replace($line, ' denied-drs-unseal .*? drs-unseal-unavailable [0-9]+', '')
            $readStatusUnsealLegacyLine = $readStatusUnsealLegacyLine -replace ' queries 359 denials 81', ' queries 359 denials 80'
            if ($readStatusUnsealLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusUnsealLegacyLine
                $line = $readStatusUnsealLegacyLine
            }

            $readStatusSealLegacyLine = [regex]::Replace($line, ' denied-drs-seal .*? drs-seal-unavailable [0-9]+', '')
            $readStatusSealLegacyLine = $readStatusSealLegacyLine -replace ' queries 359 denials 80', ' queries 359 denials 79'
            if ($readStatusSealLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusSealLegacyLine
                $line = $readStatusSealLegacyLine
            }

            $readStatusCloseLegacyLine = [regex]::Replace($line, ' denied-drs-close .*? drs-close-unavailable [0-9]+', '')
            $readStatusCloseLegacyLine = $readStatusCloseLegacyLine -replace ' queries 359 denials 79', ' queries 359 denials 78'
            if ($readStatusCloseLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusCloseLegacyLine
                $line = $readStatusCloseLegacyLine
            }

            $readStatusAckLegacyLine = [regex]::Replace($line, ' denied-drs-ack .*? drs-ack-unavailable [0-9]+', '')
            $readStatusAckLegacyLine = $readStatusAckLegacyLine -replace ' queries 359 denials 78', ' queries 359 denials 77'
            if ($readStatusAckLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusAckLegacyLine
                $line = $readStatusAckLegacyLine
            }

            $readStatusReceiptLegacyLine = [regex]::Replace($line, ' denied-drs-receipt .*? drs-receipt-unavailable [0-9]+', '')
            $readStatusReceiptLegacyLine = $readStatusReceiptLegacyLine -replace ' queries 359 denials 77', ' queries 359 denials 76'
            if ($readStatusReceiptLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusReceiptLegacyLine
                $line = $readStatusReceiptLegacyLine
            }

            $readStatusReportLegacyLine = [regex]::Replace($line, ' denied-drs-report .*? drs-report-unavailable [0-9]+', '')
            $readStatusReportLegacyLine = $readStatusReportLegacyLine -replace ' queries 359 denials 76', ' queries 359 denials 75'
            if ($readStatusReportLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusReportLegacyLine
                $line = $readStatusReportLegacyLine
            }

            $readStatusExportLegacyLine = [regex]::Replace($line, ' denied-drs-export .*? drs-export-unavailable [0-9]+', '')
            $readStatusExportLegacyLine = $readStatusExportLegacyLine -replace ' queries 359 denials 75', ' queries 359 denials 74'
            if ($readStatusExportLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusExportLegacyLine
                $line = $readStatusExportLegacyLine
            }

            $readStatusBufferLegacyLine = [regex]::Replace($line, ' denied-drs-buffer .*? drs-buffer-unavailable [0-9]+', '')
            $readStatusBufferLegacyLine = $readStatusBufferLegacyLine -replace ' queries 359 denials 74', ' queries 359 denials 73'
            if ($readStatusBufferLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusBufferLegacyLine
                $line = $readStatusBufferLegacyLine
            }

            $readStatusGuardLegacyLine = [regex]::Replace($line, ' denied-drs-guard .*? drs-guard-unavailable [0-9]+', '')
            $readStatusGuardLegacyLine = $readStatusGuardLegacyLine -replace ' queries 359 denials 73', ' queries 359 denials 72'
            if ($readStatusGuardLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusGuardLegacyLine
                $line = $readStatusGuardLegacyLine
            }

            $readStatusStableLegacyLine = [regex]::Replace($line, ' denied-drs-stable .*? drs-stable-unavailable [0-9]+', '')
            $readStatusStableLegacyLine = $readStatusStableLegacyLine -replace ' queries 359 denials 72', ' queries 359 denials 71'
            if ($readStatusStableLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusStableLegacyLine
                $line = $readStatusStableLegacyLine
            }

            $readStatusResampleLegacyLine = [regex]::Replace($line, ' denied-drs-resample .*? drs-resample-unavailable [0-9]+', '')
            $readStatusResampleLegacyLine = $readStatusResampleLegacyLine -replace ' queries 359 denials 71', ' queries 359 denials 70'
            if ($readStatusResampleLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusResampleLegacyLine
                $line = $readStatusResampleLegacyLine
            }

            $readStatusClearResultLegacyLine = [regex]::Replace($line, ' denied-drs-clear-result .*? drs-clear-result-unavailable [0-9]+', '')
            $readStatusClearResultLegacyLine = $readStatusClearResultLegacyLine -replace ' queries 359 denials 70', ' queries 359 denials 69'
            if ($readStatusClearResultLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusClearResultLegacyLine
                $line = $readStatusClearResultLegacyLine
            }

            $readStatusClearLegacyLine = [regex]::Replace($line, ' denied-drs-clear .*? drs-clear-unavailable [0-9]+', '')
            $readStatusClearLegacyLine = $readStatusClearLegacyLine -replace ' queries 359 denials 69', ' queries 359 denials 68'
            if ($readStatusClearLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusClearLegacyLine
                $line = $readStatusClearLegacyLine
            }

            $readStatusSampleLegacyLine = [regex]::Replace($line, ' denied-drs-sample .*? drs-sample-unavailable [0-9]+', '')
            $readStatusSampleLegacyLine = $readStatusSampleLegacyLine -replace ' queries 359 denials 68', ' queries 359 denials 67'
            if ($readStatusSampleLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusSampleLegacyLine
                $line = $readStatusSampleLegacyLine
            }

            $readStatusResultLegacyLine = [regex]::Replace($line, ' denied-drs-result .*? drs-result-unavailable [0-9]+', '')
            $readStatusResultLegacyLine = $readStatusResultLegacyLine -replace ' queries 359 denials 67', ' queries 359 denials 66'
            if ($readStatusResultLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusResultLegacyLine
                $line = $readStatusResultLegacyLine
            }

            $readStatusLegacyLine = [regex]::Replace($line, ' denied-drs-status .*? drs-status-unavailable [0-9]+', '')
            $readStatusLegacyLine = $readStatusLegacyLine -replace ' queries 359 denials 66', ' queries 359 denials 65'
            if ($readStatusLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readStatusLegacyLine
                $line = $readStatusLegacyLine
            }

            $readIrqLegacyLine = [regex]::Replace($line, ' denied-drs-irq .*? drs-irq-unavailable [0-9]+', '')
            $readIrqLegacyLine = $readIrqLegacyLine -replace ' queries 359 denials 65', ' queries 359 denials 64'
            if ($readIrqLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readIrqLegacyLine
                $line = $readIrqLegacyLine
            }

            $readDmaLegacyLine = [regex]::Replace($line, ' denied-driver-read-dma .*? driver-read-dma-unavailable [0-9]+', '')
            $readDmaLegacyLine = $readDmaLegacyLine -replace ' queries 359 denials 64', ' queries 359 denials 63'
            if ($readDmaLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readDmaLegacyLine
                $line = $readDmaLegacyLine
            }

            $readIssueLegacyLine = [regex]::Replace($line, ' denied-driver-read-issue .*? driver-read-issue-unavailable [0-9]+', '')
            $readIssueLegacyLine = $readIssueLegacyLine -replace ' queries 359 denials 63', ' queries 359 denials 62'
            if ($readIssueLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readIssueLegacyLine
                $line = $readIssueLegacyLine
            }

            $readBodyLegacyLine = [regex]::Replace($line, ' denied-driver-read-body .*? driver-read-body-unavailable [0-9]+', '')
            $readBodyLegacyLine = $readBodyLegacyLine -replace ' queries 359 denials 62', ' queries 359 denials 61'
            if ($readBodyLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readBodyLegacyLine
                $line = $readBodyLegacyLine
            }

            $readRunLegacyLine = [regex]::Replace($line, ' denied-driver-read-run .*? driver-read-run-unavailable [0-9]+', '')
            $readRunLegacyLine = $readRunLegacyLine -replace ' queries 359 denials 61', ' queries 359 denials 60'
            if ($readRunLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readRunLegacyLine
                $line = $readRunLegacyLine
            }

            $readScheduleLegacyLine = [regex]::Replace($line, ' denied-driver-read-schedule .*? driver-read-schedule-unavailable [0-9]+', '')
            $readScheduleLegacyLine = $readScheduleLegacyLine -replace ' queries 359 denials 60', ' queries 359 denials 59'
            if ($readScheduleLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readScheduleLegacyLine
                $line = $readScheduleLegacyLine
            }

            $readWorkerLegacyLine = [regex]::Replace($line, ' denied-driver-read-worker .*? driver-read-worker-unavailable [0-9]+', '')
            $readWorkerLegacyLine = $readWorkerLegacyLine -replace ' queries 359 denials 59', ' queries 359 denials 58'
            if ($readWorkerLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readWorkerLegacyLine
                $line = $readWorkerLegacyLine
            }

            $readQueueLegacyLine = [regex]::Replace($line, ' denied-driver-read-queue .*? driver-read-queue-unavailable [0-9]+', '')
            $readQueueLegacyLine = $readQueueLegacyLine -replace ' queries 359 denials 58', ' queries 359 denials 57'
            if ($readQueueLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readQueueLegacyLine
                $line = $readQueueLegacyLine
            }

            $readDispatchLegacyLine = [regex]::Replace($line, ' denied-driver-read-dispatch .*? driver-read-dispatch-unavailable [0-9]+', '')
            $readDispatchLegacyLine = $readDispatchLegacyLine -replace ' queries 359 denials 57', ' queries 359 denials 56'
            if ($readDispatchLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readDispatchLegacyLine
                $line = $readDispatchLegacyLine
            }

            $readAuthorizeLegacyLine = [regex]::Replace($line, ' denied-driver-read-authorize .*? driver-read-authorize-unavailable [0-9]+', '')
            $readAuthorizeLegacyLine = $readAuthorizeLegacyLine -replace ' queries 359 denials 56', ' queries 359 denials 55'
            if ($readAuthorizeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readAuthorizeLegacyLine
                $line = $readAuthorizeLegacyLine
            }

            $readFinalizeLegacyLine = [regex]::Replace($line, ' denied-driver-read-finalize .*? driver-read-finalize-unavailable [0-9]+', '')
            $readFinalizeLegacyLine = $readFinalizeLegacyLine -replace ' queries 359 denials 55', ' queries 359 denials 54'
            if ($readFinalizeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readFinalizeLegacyLine
                $line = $readFinalizeLegacyLine
            }

            $readDiscardLegacyLine = [regex]::Replace($line, ' denied-drdc .*? drdc-unavailable [0-9]+', '')
            $readDiscardLegacyLine = $readDiscardLegacyLine -replace ' queries 359 denials 54', ' queries 359 denials 53'
            if ($readDiscardLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readDiscardLegacyLine
                $line = $readDiscardLegacyLine
            }

            $readUnsealLegacyLine = [regex]::Replace($line, ' denied-drul .*? drul-unavailable [0-9]+', '')
            $readUnsealLegacyLine = $readUnsealLegacyLine -replace ' queries 359 denials 53', ' queries 359 denials 52'
            if ($readUnsealLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readUnsealLegacyLine
                $line = $readUnsealLegacyLine
            }

            $readSealLegacyLine = [regex]::Replace($line, ' denied-drsl .*? drsl-unavailable [0-9]+', '')
            $readSealLegacyLine = $readSealLegacyLine -replace ' queries 359 denials 52', ' queries 359 denials 51'
            if ($readSealLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readSealLegacyLine
                $line = $readSealLegacyLine
            }

            $readCloseLegacyLine = [regex]::Replace($line, ' denied-drcl .*? drcl-unavailable [0-9]+', '')
            $readCloseLegacyLine = $readCloseLegacyLine -replace ' queries 359 denials 51', ' queries 359 denials 50'
            if ($readCloseLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readCloseLegacyLine
                $line = $readCloseLegacyLine
            }

            $readAckLegacyLine = [regex]::Replace($line, ' denied-drak .*? drak-unavailable [0-9]+', '')
            $readAckLegacyLine = $readAckLegacyLine -replace ' queries 359 denials 50', ' queries 359 denials 49'
            if ($readAckLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readAckLegacyLine
                $line = $readAckLegacyLine
            }

            $readReceiptLegacyLine = [regex]::Replace($line, ' denied-drrc .*? drrc-unavailable [0-9]+', '')
            $readReceiptLegacyLine = $readReceiptLegacyLine -replace ' queries 359 denials 49', ' queries 359 denials 48'
            if ($readReceiptLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readReceiptLegacyLine
                $line = $readReceiptLegacyLine
            }

            $readReportLegacyLine = [regex]::Replace($line, ' denied-drpt .*? drpt-unavailable [0-9]+', '')
            $readReportLegacyLine = $readReportLegacyLine -replace ' queries 359 denials 48', ' queries 359 denials 47'
            if ($readReportLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readReportLegacyLine
                $line = $readReportLegacyLine
            }

            $readUseLegacyLine = [regex]::Replace($line, ' denied-duse .*? duse-unavailable [0-9]+', '')
            $readUseLegacyLine = $readUseLegacyLine -replace ' queries 359 denials 47', ' queries 359 denials 46'
            if ($readUseLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readUseLegacyLine
                $line = $readUseLegacyLine
            }

            $readLeaseLegacyLine = [regex]::Replace($line, ' denied-dlse .*? dlse-unavailable [0-9]+', '')
            $readLeaseLegacyLine = $readLeaseLegacyLine -replace ' queries 359 denials 46', ' queries 359 denials 45'
            if ($readLeaseLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readLeaseLegacyLine
                $line = $readLeaseLegacyLine
            }

            $readWindowLegacyLine = [regex]::Replace($line, ' denied-dwin .*? dwin-unavailable [0-9]+', '')
            $readWindowLegacyLine = $readWindowLegacyLine -replace ' queries 359 denials 45', ' queries 359 denials 44'
            if ($readWindowLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readWindowLegacyLine
                $line = $readWindowLegacyLine
            }

            $readPermitLegacyLine = [regex]::Replace($line, ' denied-dprm .*? dprm-unavailable [0-9]+', '')
            $readPermitLegacyLine = $readPermitLegacyLine -replace ' queries 359 denials 44', ' queries 359 denials 43'
            if ($readPermitLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readPermitLegacyLine
                $line = $readPermitLegacyLine
            }

            $readRetireLegacyLine = [regex]::Replace($line, ' denied-dret .*? dret-unavailable [0-9]+', '')
            $readRetireLegacyLine = $readRetireLegacyLine -replace ' queries 359 denials 43', ' queries 359 denials 42'
            if ($readRetireLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readRetireLegacyLine
                $line = $readRetireLegacyLine
            }

            $readObserveLegacyLine = [regex]::Replace($line, ' denied-dobs .*? dobs-unavailable [0-9]+', '')
            $readObserveLegacyLine = $readObserveLegacyLine -replace ' queries 359 denials 42', ' queries 359 denials 41'
            if ($readObserveLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readObserveLegacyLine
                $line = $readObserveLegacyLine
            }

            $readSubmitLegacyLine = [regex]::Replace($line, ' denied-dsub .*? dsub-unavailable [0-9]+', '')
            $readSubmitLegacyLine = $readSubmitLegacyLine -replace ' queries 359 denials 41', ' queries 359 denials 40'
            if ($readSubmitLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readSubmitLegacyLine
                $line = $readSubmitLegacyLine
            }

            $readArmLegacyLine = [regex]::Replace($line, ' denied-darm .*? darm-unavailable [0-9]+', '')
            $readArmLegacyLine = $readArmLegacyLine -replace ' queries 359 denials 40', ' queries 359 denials 39'
            if ($readArmLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readArmLegacyLine
                $line = $readArmLegacyLine
            }

            $readActivateLegacyLine = [regex]::Replace($line, ' denied-dact .*? dact-unavailable [0-9]+', '')
            $readActivateLegacyLine = $readActivateLegacyLine -replace ' queries 359 denials 39', ' queries 359 denials 38'
            if ($readActivateLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readActivateLegacyLine
                $line = $readActivateLegacyLine
            }

            $readUpgradeLegacyLine = [regex]::Replace($line, ' denied-dru .*? dru-unavailable [0-9]+', '')
            $readUpgradeLegacyLine = $readUpgradeLegacyLine -replace ' queries 359 denials 38', ' queries 359 denials 37'
            if ($readUpgradeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readUpgradeLegacyLine
                $line = $readUpgradeLegacyLine
            }

            $readAuditLegacyLine = [regex]::Replace($line, ' denied-dra .*? dra-unavailable [0-9]+', '')
            $readAuditLegacyLine = $readAuditLegacyLine -replace ' queries 359 denials 37', ' queries 359 denials 36'
            if ($readAuditLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readAuditLegacyLine
                $line = $readAuditLegacyLine
            }

            $readCommitLegacyLine = [regex]::Replace($line, ' denied-drk .*? drk-unavailable [0-9]+', '')
            $readCommitLegacyLine = $readCommitLegacyLine -replace ' queries 359 denials 36', ' queries 359 denials 35'
            if ($readCommitLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readCommitLegacyLine
                $line = $readCommitLegacyLine
            }

            $readVisibleLegacyLine = [regex]::Replace($line, ' denied-drv .*? drv-unavailable [0-9]+', '')
            $readVisibleLegacyLine = $readVisibleLegacyLine -replace ' queries 359 denials 35', ' queries 359 denials 34'
            if ($readVisibleLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readVisibleLegacyLine
                $line = $readVisibleLegacyLine
            }

            $readDeliveryLegacyLine = [regex]::Replace($line, ' denied-drd .*? drd-unavailable [0-9]+', '')
            $readDeliveryLegacyLine = $readDeliveryLegacyLine -replace ' queries 359 denials 34', ' queries 359 denials 33'
            if ($readDeliveryLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readDeliveryLegacyLine
                $line = $readDeliveryLegacyLine
            }

            $readResponseLegacyLine = [regex]::Replace($line, ' denied-drr .*? drr-unavailable [0-9]+', '')
            $readResponseLegacyLine = $readResponseLegacyLine -replace ' queries 359 denials 33', ' queries 359 denials 32'
            if ($readResponseLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readResponseLegacyLine
                $line = $readResponseLegacyLine
            }

            $tableLegacyLine = [regex]::Replace($line, ' denied-issue-plan .*? handoff-unavailable [0-9]+(?: denied-driver-probe .*? driver-probe-unavailable [0-9]+(?: denied-driver-intent .*? driver-intent-unavailable [0-9]+(?: denied-driver-buffer .*? driver-buffer-unavailable [0-9]+(?: denied-driver-gate .*? driver-gate-unavailable [0-9]+(?: denied-driver-exec .*? driver-exec-unavailable [0-9]+(?: denied-driver-result .*? driver-result-unavailable [0-9]+(?: denied-driver-publish .*? driver-publish-unavailable [0-9]+(?: denied-drg .*? drg-unavailable [0-9]+(?: denied-dmr .*? dmr-unavailable [0-9]+(?: denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?)?)?)?)?)?)?)?)?)?)?', '')
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 32', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 31', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 30', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 29', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 28', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 27', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 26', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 25', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 24', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 23', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 22', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 21', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 20', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 19', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 359 denials 18', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 340 denials 17', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 323 denials 16', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 304 denials 15', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 287 denials 14', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 272 denials 13', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 231 denials 12', ' queries 140 denials 9'
            $tableLegacyLine = $tableLegacyLine -replace ' queries 205 denials 11', ' queries 140 denials 9'
            if ($tableLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $tableLegacyLine
            }

            $issueLegacyLine = [regex]::Replace($line, ' denied-bind-plan .*? handoff-unavailable [0-9]+(?: denied-driver-probe .*? driver-probe-unavailable [0-9]+(?: denied-driver-intent .*? driver-intent-unavailable [0-9]+(?: denied-driver-buffer .*? driver-buffer-unavailable [0-9]+(?: denied-driver-gate .*? driver-gate-unavailable [0-9]+(?: denied-driver-exec .*? driver-exec-unavailable [0-9]+(?: denied-driver-result .*? driver-result-unavailable [0-9]+(?: denied-driver-publish .*? driver-publish-unavailable [0-9]+(?: denied-drg .*? drg-unavailable [0-9]+(?: denied-dmr .*? dmr-unavailable [0-9]+(?: denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?)?)?)?)?)?)?)?)?)?)?', '')
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 32', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 31', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 30', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 29', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 28', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 27', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 26', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 25', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 24', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 23', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 22', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 21', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 20', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 19', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 359 denials 18', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 340 denials 17', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 323 denials 16', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 304 denials 15', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 287 denials 14', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 272 denials 13', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 231 denials 12', ' queries 170 denials 10'
            $issueLegacyLine = $issueLegacyLine -replace ' queries 205 denials 11', ' queries 170 denials 10'
            if ($issueLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $issueLegacyLine
            }

            $readExportLegacyLine = [regex]::Replace($line, ' denied-drx .*? drx-unavailable [0-9]+', '')
            $readExportLegacyLine = $readExportLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 31'
            if ($readExportLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readExportLegacyLine
            }

            $readCapLegacyLine = [regex]::Replace($line, ' denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?', '')
            $readCapLegacyLine = $readCapLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 30'
            $readCapLegacyLine = $readCapLegacyLine -replace ' queries 359 denials 31', ' queries 359 denials 30'
            if ($readCapLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readCapLegacyLine
            }

            $completeLegacyLine = [regex]::Replace($line, ' denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?', '')
            $completeLegacyLine = $completeLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 29'
            $completeLegacyLine = $completeLegacyLine -replace ' queries 359 denials 31', ' queries 359 denials 29'
            $completeLegacyLine = $completeLegacyLine -replace ' queries 359 denials 30', ' queries 359 denials 29'
            if ($completeLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $completeLegacyLine
            }

            $mediaReadLegacyLine = [regex]::Replace($line, ' denied-dmr .*? dmr-unavailable [0-9]+(?: denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?)?', '')
            $mediaReadLegacyLine = $mediaReadLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 28'
            $mediaReadLegacyLine = $mediaReadLegacyLine -replace ' queries 359 denials 31', ' queries 359 denials 28'
            $mediaReadLegacyLine = $mediaReadLegacyLine -replace ' queries 359 denials 30', ' queries 359 denials 28'
            $mediaReadLegacyLine = $mediaReadLegacyLine -replace ' queries 359 denials 29', ' queries 359 denials 28'
            if ($mediaReadLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $mediaReadLegacyLine
            }

            $readGrantLegacyLine = [regex]::Replace($line, ' denied-drg .*? drg-unavailable [0-9]+(?: denied-dmr .*? dmr-unavailable [0-9]+(?: denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?)?)?', '')
            $readGrantLegacyLine = $readGrantLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 27'
            $readGrantLegacyLine = $readGrantLegacyLine -replace ' queries 359 denials 31', ' queries 359 denials 27'
            $readGrantLegacyLine = $readGrantLegacyLine -replace ' queries 359 denials 30', ' queries 359 denials 27'
            $readGrantLegacyLine = $readGrantLegacyLine -replace ' queries 359 denials 29', ' queries 359 denials 27'
            $readGrantLegacyLine = $readGrantLegacyLine -replace ' queries 359 denials 28', ' queries 359 denials 27'
            if ($readGrantLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $readGrantLegacyLine
            }

            $publishLegacyLine = [regex]::Replace($line, ' denied-driver-publish .*? driver-publish-unavailable [0-9]+(?: denied-drg .*? drg-unavailable [0-9]+(?: denied-dmr .*? dmr-unavailable [0-9]+(?: denied-drc .*? drc-unavailable [0-9]+(?: denied-drcap .*? drcap-unavailable [0-9]+(?: denied-drx .*? drx-unavailable [0-9]+)?)?)?)?)?', '')
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 32', ' queries 359 denials 26'
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 31', ' queries 359 denials 26'
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 30', ' queries 359 denials 26'
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 29', ' queries 359 denials 26'
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 28', ' queries 359 denials 26'
            $publishLegacyLine = $publishLegacyLine -replace ' queries 359 denials 27', ' queries 359 denials 26'
            if ($publishLegacyLine -ne $line) {
                $script:LegacyMmioAssertionLines += $publishLegacyLine
            }
        }
    }
}

if ($Architecture -eq "x86") {
    Assert-OutputContains -Lines $outputLines -Pattern 'LimitlessOS kernel core milestone' -Message "x86 kernel banner was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[userspace\] dispatch session-shell' -Message "x86 ring-3 shell did not start."
    Assert-OutputContains -Lines $outputLines -Pattern '\$ apps' -Message "x86 brokered shell/app descriptor script did not run."
    Assert-OutputContains -Lines $outputLines -Pattern 'authority: buffer base' -Message "x86 descriptor authority decoding did not report combined buffer/base rights."
    Assert-OutputContains -Lines $outputLines -Pattern 'bindings: foreground console' -Message "x86 descriptor binding decoding did not report combined foreground/console bindings."
    Assert-OutputContains -Lines $outputLines -Pattern 'authority: buffer base dest pair' -Message "x86 copy descriptor authority decoding did not report multi-right command authority."
    Assert-OutputContains -Lines $outputLines -Pattern 'user-fs-writes' -Message "x86 filesystem telemetry was not observed."
}
elseif ($BootMedia -eq "disk") {
    Assert-OutputContains -Lines $outputLines -Pattern 'LimitlessOS x86_64 scaffold' -Message "x64 BIOS scaffold banner was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] descriptors state 0x0000007F' -Message "x64 descriptor state proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'user-cs 0x00000033 user-ds 0x0000002B' -Message "x64 ring-3 selectors were not reported."
    Assert-OutputContains -Lines $outputLines -Pattern 'star-plan 0x0023001800000000 star 0x0023001800000000 star-ready 1' -Message "x64 native syscall STAR selector proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'user 0 writable 0 supervisor-only 1 validation-only 1' -Message "x64 supervisor-only runtime image protection proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'runtime image map installed 1 .* pages 4 checksum 0x[0-9A-F]+ .* expected 0x36534F4C' -Message "x64 four-page supervisor runtime image mapping proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'plan-bytes 16384 .* map-pages 4 .* payload-size 16384' -Message "x64 launch broker did not report the persistent-shell runtime image geometry."
    Assert-OutputContains -Lines $outputLines -Pattern 'runtime user image map installed 1 .* user 1 writable 0 supervisor-only 0 validation-only 0' -Message "x64 user executable mapping proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'runtime user image map installed 1 .* pages 4 checksum 0x[0-9A-F]+ .* user 1 writable 0 supervisor-only 0 validation-only 0' -Message "x64 four-page user executable mapping proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'runtime user stack map installed 1 .* user 1 writable 1 supervisor-only 0 validation-only 0' -Message "x64 user stack mapping proof is missing."
    Assert-OutputContains -Lines $outputLines -Pattern 'user-entry-state 0x0000002F' -Message "x64 user-entry frame readiness was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user-rip 0x41000010 user-rsp 0x40020000 user-selectors 0x002B0033 user-rflags 0x00000002' -Message "x64 user-entry frame registers were not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user-entry-denial 0 user-transfer-ready 1' -Message "x64 user-entry transfer-ready proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'process-entry-ready 1' -Message "x64 process-owned user-entry readiness was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user transfer probe attempts 1 exits 1 result 0x36534F4C expected 0x36534F4C rip 0x0000000041000010 rsp 0x0000000040020000 cs 0x0000000000000033 ss 0x000000000000002B' -Message "x64 ring-3 transfer and syscall-return proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user preempt probe attempts 1 exits 1 irqs [1-9][0-9]* result 0x5052454D expected 0x5052454D rip 0x00000000410000[0-9A-F]{2} rsp 0x0000000040020000 cs 0x0000000000000033 ss 0x000000000000002B rflags 0x00000202' -Message "x64 interruptible ring-3 timer preemption proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user switch probe attempts 1 exits 1 irqs [1-9][0-9]* switches 1 result 0x53574348 expected 0x53574348 source-rip 0x000000004100008[0-9A-F] source-rsp 0x0000000040020000 target-rip 0x00000000410000C0 target-rsp 0x000000004001FC00 cs 0x0000000000000033 ss 0x000000000000002B rflags 0x00000202' -Message "x64 scheduler-owned ring-3 task switch proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern 'user runqueue probe attempts 1 exits 2 irqs [1-9][0-9]* switches 2 result 0x52514131 source-result 0x52514131 target-result 0x52514232 expected-source 0x52514131 expected-target 0x52514232 source-pid 2 target-pid 4 source-runtime 0x[0-9A-F]+ target-runtime 0x[0-9A-F]+ source-entry-token 0x[0-9A-F]+ target-entry-token 0x[0-9A-F]+ source-rip 0x000000004100010[0-9A-F] source-rsp 0x0000000040020000 target-rip 0x0000000041000140 target-rsp 0x000000004001F800 cs 0x0000000000000033 ss 0x000000000000002B rflags 0x00000202' -Message "x64 process-bound saved-frame run queue proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] fs broker service 0x[0-9A-F]+ denied-open 0xFFFFFFFF root 0x[0-9A-F]+ readme 0x[0-9A-F]+ apps 0x[0-9A-F]+ rights 0x00009A00 owner 0x00000201 wrong-owner 0xFFFFFFFF note 0x[0-9A-F]+ written 32 revoke 1 revoked-read 0xFFFFFFFF' -Message "x64 brokered filesystem capability proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] syscall input keyboard ps2-status 0x[0-9A-F]+ irq [0-9]+ polls [1-9][0-9]* scancodes [1-9][0-9]* bytes [1-9][0-9]* pending [1-9][0-9]* drops 0 last-scancode 0x[0-9A-F]+ last-byte 0x[0-9A-F]+' -Message "x64 PS/2 keyboard input telemetry proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] block broker service 8 cap 0x[0-9A-F]+ available 1 status 0x[0-9A-F]+ denied-read 0xFFFFFFFF read 512 signature 1 reads 1 bytes 512 denials [1-9][0-9]* unavailable 0 lba 0 token 0x(?!00000000)[0-9A-F]{8}' -Message "x64 brokered ATA block read proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pci broker service 9 cap 0x[0-9A-F]+ denied-devices 0xFFFFFFFF denied-mmio-base 0xFFFFFFFF devices [1-9][0-9]* multi [0-9]+ storage [1-9][0-9]* ide [1-9][0-9]* ahci 0 nvme 0 usb [0-9]+ display [0-9]+ ahci-addr 0xFFFFFFFF ahci-vendor-device 0x00000000 ahci-class 0x00000000 ahci-bar5 0x00000000 token 0x(?!00000000)[0-9A-F]{8} mmio-base 0x00000000 mmio-span 0 mmio-flags 0x00000040 mmio-token 0x(?!00000000)[0-9A-F]{8} queries 17 denials 2' -Message "x64 BIOS brokered PCI legacy storage inventory proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pci-ecam drs-pci-ecam-rsdp 0 drs-pci-ecam-mcfg 0 drs-pci-ecam-base 0x0000000000000000 drs-pci-ecam-segment 0 drs-pci-ecam-bus-start 0 drs-pci-ecam-bus-end 0 drs-pci-ecam-active 0 drs-pci-ecam-fallback-io 1 drs-pci-ecam-ahci-found 0' -Message "x64 BIOS PCI ECAM fallback proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-probe denied-drs-nvme-probe 0xFFFFFFFF drs-nvme-probe 0xFFFFFFFF drs-nvme-probe-found 0 drs-nvme-probe-bar0 0x0000000000000000 drs-nvme-probe-ready 0 drs-nvme-probe-identify 0 drs-nvme-probe-model none drs-nvme-probe-firmware none drs-nvme-probe-io-queue 0 drs-nvme-probe-read-authority 0 drs-nvme-probe-fs-authority 0 drs-nvme-probe-unavailable 1' -Message "x64 BIOS NVMe unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-read denied-drs-nvme-read 0xFFFFFFFF drs-nvme-read 0xFFFFFFFF drs-nvme-read-ioq-created 0 drs-nvme-read-issued 0 drs-nvme-read-completed 0 drs-nvme-read-status 0 drs-nvme-read-bytes 0 drs-nvme-read-checksum 0x00000000 fs-authority 0 block-endpoint 0 write-authority 0 unavailable 1' -Message "x64 BIOS NVMe read unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-gpt denied-drs-nvme-gpt 0xFFFFFFFF drs-nvme-gpt 0xFFFFFFFF drs-nvme-gpt-signature 0 drs-nvme-gpt-partitions 0 drs-nvme-gpt-fat32-start 0 drs-nvme-gpt-fat32-sectors 0 drs-nvme-gpt-vbr 0 fs-authority 0 write-authority 0 m5-safe-targets 0 m5-forbidden-targets 0 m5-unknown-targets 0 m5-boot-partition 0 m5-root-partition 0 m5-boot-start 0 m5-root-start 0 m5-forbidden-denied 0 m5-no-write-authority 0 unavailable 1' -Message "x64 BIOS NVMe GPT unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-fat denied-drs-nvme-fat 0xFFFFFFFF drs-nvme-fat 0xFFFFFFFF drs-nvme-fat-bpb 0 drs-nvme-fat-located 0 drs-nvme-fat-read-bytes 0 drs-nvme-fat-checksum 0x00000000 drs-nvme-fat-content-match 0 drs-nvme-fat-bytes-per-sector 0 drs-nvme-fat-sectors-per-cluster 0 drs-nvme-fat-lfn 0 drs-nvme-fat-unicode-lfn 0 drs-nvme-fat-subdir 0 drs-nvme-fat-multicluster 0 drs-nvme-fat-multi-bytes 0 drs-nvme-fat-write-gate 0 drs-nvme-fat-create-cluster 0 drs-nvme-fat-create-readback 0 drs-nvme-fat-create-bytes 0 drs-nvme-fat-create-checksum 0x00000000 drs-nvme-fat-update-cluster 0 drs-nvme-fat-update-readback 0 drs-nvme-fat-update-bytes 0 drs-nvme-fat-update-checksum 0x00000000 drs-nvme-fat-delete-freed 0 drs-nvme-fat-delete-tombstone 0 drs-nvme-fat-flushes 0 fs-delegation 0 block-endpoint 0 write-authority 0 commit-authority 0 unavailable 1 error 1' -Message "x64 BIOS NVMe FAT unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-rw delegated 0 cap 0xFFFFFFFF wrong-owner 0 stale 0 revoked 0 shell-write 0 shell-readback 0 write-bytes 0 write-checksum 0x00000000 persisted 0 audit 0 commits 0 write-authority 0 commit-authority 0 unavailable 1 error [0-9]+' -Message "x64 BIOS NVMe scoped write-authority unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-installer-target drs-installer-target-attempted 0 drs-installer-target-confirmation-token 0 drs-installer-target-classified 0 drs-installer-target-boot-partition 0 drs-installer-target-root-partition 0 drs-installer-target-boot-start 0 drs-installer-target-root-start 0 drs-installer-target-forbidden-denied 0 drs-installer-target-bad-token-denied 0 drs-installer-target-wrong-target-denied 0 drs-installer-target-wrong-owner-denied 0 drs-installer-target-m5-write-cap 0 drs-installer-target-write 0 drs-installer-target-readback 0 drs-installer-target-bytes 0 drs-installer-target-checksum 0x00000000 drs-installer-target-write-denied 0 drs-installer-target-format-denied 1 drs-installer-target-boot-entry-denied 1 drs-installer-target-no-ambient 1 drs-installer-target-unavailable 1 error 0 mode unavailable' -Message "x64 BIOS installer M5 target marker unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-apic drs-apic-madt 0 drs-apic-lapic-base 0x0000000000000000 drs-apic-ioapic-base 0x0000000000000000 drs-apic-pic-disabled 0 drs-apic-timer-ticking 1 drs-apic-keyboard-live 1 drs-apic-enabled 0' -Message "x64 BIOS APIC unavailable/PIC fallback proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-apic-override drs-apic-override-scanned 0 drs-apic-override-count 0 drs-apic-timer-gsi 0 .* drs-apic-keyboard-gsi 0 .* drs-apic-timer-ticking 1 drs-apic-keyboard-live 1' -Message "x64 BIOS APIC override unavailable/PIC fallback proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-xhci drs-xhci-found 0 drs-xhci-bar0 0x0000000000000000 drs-xhci-mapped 0 drs-xhci-cap 0 drs-xhci-ports 0 drs-xhci-ports-scanned 0 drs-xhci-connected 0 drs-xhci-command-ring 0 drs-xhci-dcbaa 0 drs-xhci-event-ring 0 drs-xhci-reset 0 drs-xhci-running 0 drs-xhci-slot-enabled 0 drs-xhci-addressed 0 drs-xhci-config-read 0 drs-xhci-report-desc 0 drs-xhci-endpoint 0 drs-xhci-hid-device 0 drs-xhci-input-live 0 drs-xhci-reports 0 drs-xhci-report-bytes 0 unavailable 1 error 0' -Message "x64 BIOS xHCI unavailable/PIC fallback proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-xhci .* error 0 drs-xhci-extcaps-scanned 0 drs-xhci-legacy-cap 0 drs-xhci-legacy-handoff 0 .* drs-xhci-protocol-caps 0 drs-xhci-usb2-ports 0 drs-xhci-usb3-ports 0 drs-xhci-prefer-usb2 0 .* drs-xhci-reset-wait-ms 100 drs-xhci-settle-ms 50' -Message "x64 BIOS xHCI conservative timing/unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-mouse drs-mouse-found 1 drs-mouse-delta 1 drs-mouse-buttons [01] drs-mouse-packets [1-9][0-9]* .* drs-mouse-ps2-enabled 1' -Message "x64 BIOS brokered mouse input proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-compositor drs-compositor-init 0 drs-compositor-present 0 drs-compositor-cursor 0 drs-compositor-presents 0 drs-compositor-cursors 0' -Message "x64 BIOS compositor unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-font drs-font-init 0 drs-font-glyphs 256 drs-font-render 0 drs-font-renders 0' -Message "x64 BIOS font renderer unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-wm drs-wm-init 0 drs-wm-window-created 0 drs-wm-focus 0 drs-wm-present 0 drs-wm-windows 0 drs-wm-focuses 0 drs-wm-presents 0' -Message "x64 BIOS window manager unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-desktop drs-desktop-init 0 drs-desktop-taskbar 0 drs-desktop-launcher 0 drs-desktop-terminal 0 drs-desktop-fileman 0 drs-desktop-settings 0' -Message "x64 BIOS desktop unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-net drs-net-found 0 drs-net-bar0 0x0000000000000000 drs-net-mapped 0 drs-net-common 0 drs-net-notify 0 drs-net-device-config 0 drs-net-mac 0x0000000000000000 drs-net-mac-nonzero 0 .* drs-net-tx 0 drs-net-rx 0 drs-net-arp-reply 0 .* fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 1 error 0' -Message "x64 BIOS virtio-net unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dhcp drs-dhcp-discover 0 drs-dhcp-offer 0 drs-dhcp-request 0 drs-dhcp-ack 0 drs-dhcp-ip 0x00000000 drs-dhcp-gateway 0x00000000 drs-dhcp-dns 0x00000000 drs-dhcp-lease 0 ambient-authority 0 unavailable 1 error 0' -Message "x64 BIOS DHCP unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dns drs-dns-query 0 drs-dns-response 0 drs-dns-rcode 0 drs-dns-resolved 0x00000000 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 1 error 0' -Message "x64 BIOS DNS unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-http drs-http-connected 0 drs-http-sent 0 drs-http-status 0 drs-http-response-bytes 0 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 1 error 0' -Message "x64 BIOS HTTP unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] mmio planner service 9 .* denied-read-plan 0xFFFFFFFF read-plan 0xFFFFFFFF .* read-staged 0 read-denials 1 read-unavailable 1 denied-cmd-plan 0xFFFFFFFF cmd-plan 0xFFFFFFFF cmd-state 2 cmd-flags 0x0002F801 .* cmd-op 0 .* cmd-armed 0 cmd-issued 0 cmd-dma 0 cmd-staged 0 cmd-denials 1 cmd-unavailable 1 denied-mem-plan 0xFFFFFFFF mem-plan 0xFFFFFFFF mem-state 2 mem-flags 0x0005F801 mem-token 0x00000000 mem-cmd-token 0x00000000 mem-slot 0 mem-pages 0 mem-page-bytes 0 mem-page-virt 0x0000000000000000 mem-page-phys 0x0000000000000000 mem-page-checksum 0x00000000 mem-zeroed 0 mem-materialized 0 .* mem-dma 0 mem-table-written 0 mem-port-programmed 0 mem-armed 0 mem-staged 0 mem-denials 1 mem-unavailable 1 denied-table-plan 0xFFFFFFFF table-plan 0xFFFFFFFF table-state 2 table-flags 0x0013F001 table-token 0x00000000 table-mem-token 0x00000000 table-check-before 0x00000000 table-check-after 0x00000000 table-check-changed 0 table-header-flags 0x00000000 table-prdtl 0 table-prdbc 0 table-ctba-low 0x00000000 table-ctba-high 0x00000000 table-cfis-type 0x00000000 table-cfis-flags 0x00000000 table-cfis-command 0x00000000 table-cfis-device 0x00000000 table-cfis-count 0 table-packet-opcode 0x00000000 table-packet-blocks 0 table-prdt-dba-low 0x00000000 table-prdt-dba-high 0x00000000 table-prdt-dbc 0 table-written 0 table-dma 0 table-port-programmed 0 table-armed 0 table-issued 0 table-staged 0 table-denials 1 table-unavailable 1 map-requests 0 .* queries 140 denials 9' -Message "x64 BIOS brokered MMIO unavailable AHCI table-prep proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] mmio planner service 9 .* denied-issue-plan 0xFFFFFFFF issue-plan 0xFFFFFFFF issue-state 2 issue-flags 0x073F0001 issue-token 0x00000000 .* issue-command-issued 0 issue-armed 0 issue-staged 0 issue-denials 1 issue-unavailable 1 map-requests 0 .* queries 170 denials 10' -Message "x64 BIOS brokered MMIO unavailable AHCI issue-preflight proof was not observed."
    $biosBindPattern = (
        '\[x64\] mmio planner service 9 .* denied-bind-plan 0xFFFFFFFF bind-plan 0xFFFFFFFF bind-state 2 bind-flags 0x1EFF0001 bind-token 0x00000000 .* ' +
        'bind-memory-written 0 bind-dma 0 bind-port-programmed 0 bind-published 0 bind-command-issued 0 bind-armed 0 bind-staged 0 bind-denials 1 bind-unavailable 1 denied-patch-plan 0xFFFFFFFF .* denied-publish-plan 0xFFFFFFFF .* denied-publish-gate 0xFFFFFFFF .* map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosBindPattern -Message "x64 BIOS brokered MMIO unavailable AHCI address-bind proof was not observed."
    $biosPatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-patch-plan 0xFFFFFFFF patch-plan 0xFFFFFFFF patch-state 2 patch-flags 0x03EFE001 patch-token 0x00000000 .* ' +
        'patch-memory-written 0 patch-dma 0 patch-port-programmed 0 patch-published 0 patch-command-issued 0 patch-armed 0 patch-staged 0 patch-denials 1 patch-unavailable 1 denied-publish-plan 0xFFFFFFFF .* denied-publish-gate 0xFFFFFFFF .* map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosPatchPattern -Message "x64 BIOS brokered MMIO unavailable AHCI private address-patch proof was not observed."
    $biosPublishPattern = (
        '\[x64\] mmio planner service 9 .* denied-publish-plan 0xFFFFFFFF publish-plan 0xFFFFFFFF publish-state 2 publish-flags 0x07DFE001 publish-token 0x00000000 .* ' +
        'publish-port 0xFFFFFFFF .* publish-page-check 0x00000000 publish-page-match 0 publish-clb-aligned 0 publish-fis-aligned 0 publish-range-ready 0 publish-below-4g 0 ' +
        'publish-memory-written 0 publish-dma 0 publish-mmio-written 0 publish-port-programmed 0 publish-published 0 publish-command-issued 0 publish-armed 0 publish-staged 0 publish-denials 1 publish-unavailable 1 denied-publish-gate 0xFFFFFFFF publish-gate 0xFFFFFFFF .* map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosPublishPattern -Message "x64 BIOS brokered MMIO unavailable AHCI publish-preflight proof was not observed."
    $biosGatePattern = (
        '\[x64\] mmio planner service 9 .* denied-publish-gate 0xFFFFFFFF publish-gate 0xFFFFFFFF gate-state 2 gate-flags 0x0001FFF9 ' +
        'gate-token 0x00000000 gate-publish-token 0x00000000 gate-live-hardware 1 gate-exclusive 1 gate-revocation-required 1 gate-revocation-satisfied 0 ' +
        'gate-write-window 0 gate-commit-allowed 0 gate-mmio-written 0 gate-port-programmed 0 gate-published 0 gate-command-issued 0 gate-armed 0 ' +
        'gate-staged 0 gate-denials 1 gate-unavailable 1 denied-window-policy 0xFFFFFFFF window-policy 0xFFFFFFFF .* map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosGatePattern -Message "x64 BIOS brokered MMIO AHCI publication gate unavailable proof was not observed."
    $biosWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-window-policy 0xFFFFFFFF window-policy 0xFFFFFFFF window-state 2 window-flags 0x0001FFF1 ' +
        'window-token 0x00000000 window-gate-token 0x00000000 window-publish-token 0x00000000 window-live-hardware 1 window-exclusive 1 ' +
        'window-revocation-required 1 window-revocation-satisfied 0 window-revocation-executed 0 window-write-window 0 window-commit-allowed 0 ' +
        'window-mmio-written 0 window-port-programmed 0 window-published 0 window-command-issued 0 window-armed 0 ' +
        'window-staged 0 window-denials 1 window-unavailable 1 denied-revoke-plan 0xFFFFFFFF revoke-plan 0xFFFFFFFF .* map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosWindowPattern -Message "x64 BIOS brokered MMIO AHCI publish write-window unavailable proof was not observed."
    $biosRevokePattern = (
        '\[x64\] mmio planner service 9 .* denied-revoke-plan 0xFFFFFFFF revoke-plan 0xFFFFFFFF revoke-state 2 revoke-flags 0x0001FFF1 ' +
        'revoke-token 0x00000000 revoke-window-token 0x00000000 revoke-gate-token 0x00000000 revoke-live-before 1 revoke-live-after 1 revoke-exclusive 1 ' +
        'revoke-required 1 revoke-planned 1 revoke-executed 0 revoke-would-revoke 1 revoke-write-window 0 revoke-commit-allowed 0 ' +
        'revoke-mmio-written 0 revoke-port-programmed 0 revoke-published 0 revoke-command-issued 0 revoke-armed 0 ' +
        'revoke-staged 0 revoke-denials 1 revoke-unavailable 1 denied-open-window 0xFFFFFFFF open-window 0xFFFFFFFF open-state 2 open-flags 0x0001FFF1 ' +
        'open-token 0x00000000 open-revoke-token 0x00000000 open-window-token 0x00000000 open-live-hardware 1 open-revocation-required 1 open-revocation-planned 1 open-revocation-executed 0 ' +
        'open-write-window 0 open-allowed 0 open-commit-allowed 0 open-mmio-written 0 open-port-programmed 0 open-published 0 open-command-issued 0 open-armed 0 ' +
        'open-staged 0 open-denials 1 open-unavailable 1 denied-session 0xFFFFFFFF session 0xFFFFFFFF session-state 2 session-flags 0x0001FFF1 ' +
        'session-token 0x00000000 session-open-token 0x00000000 session-revoke-token 0x00000000 session-window-token 0x00000000 session-live-hardware 1 session-revocation-required 1 session-revocation-planned 1 session-revocation-executed 0 ' +
        'session-allowed 0 session-driver-owned 0 session-write-window 0 session-commit-allowed 0 session-mmio-written 0 session-port-programmed 0 session-published 0 session-command-issued 0 session-armed 0 ' +
        'session-staged 0 session-denials 1 session-unavailable 1 denied-drain 0xFFFFFFFF drain 0xFFFFFFFF drain-state 2 drain-flags 0x0001FC71 ' +
        'drain-token 0x00000000 drain-session-token 0x00000000 drain-open-token 0x00000000 drain-revoke-token 0x00000000 drain-window-token 0x00000000 drain-live-before 1 drain-revoked 0 drain-live-after 1 ' +
        'drain-revocation-required 1 drain-revocation-planned 1 drain-revocation-executed 0 drain-write-window 0 drain-commit-allowed 0 drain-mmio-written 0 drain-port-programmed 0 drain-published 0 drain-command-issued 0 drain-armed 0 ' +
        'drain-staged 0 drain-denials 1 drain-unavailable 1 denied-handoff 0xFFFFFFFF handoff 0xFFFFFFFF handoff-state 2 handoff-flags 0x0003F861 ' +
        'handoff-token 0x00000000 handoff-drain-token 0xFFFFFFFF handoff-old-handle 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} handoff-driver-owner 0x00001006 handoff-driver-cap 0xFFFFFFFF ' +
        'handoff-live-before 1 handoff-stale-old-denied 0 handoff-driver-valid 1 handoff-driver-role 0x000000B4 handoff-owner-bound 0 handoff-query-only 0 handoff-live-after 1 ' +
        'handoff-write-window 0 handoff-commit-allowed 0 handoff-mmio-written 0 handoff-port-programmed 0 handoff-published 0 handoff-command-issued 0 handoff-armed 0 ' +
        'handoff-staged 0 handoff-denials 1 handoff-unavailable 1 denied-driver-probe 0xFFFFFFFF driver-probe 0xFFFFFFFF driver-probe-state 2 driver-probe-flags 0x0003FC01 ' +
        'driver-probe-token 0x00000000 driver-probe-handoff-token 0xFFFFFFFF driver-probe-cap 0xFFFFFFFF driver-probe-owner 0x00001006 driver-probe-owner-bound 0 driver-probe-query-only 0 ' +
        'driver-probe-cap-reg 0x00000000 driver-probe-ghc 0x00000000 driver-probe-pi 0x00000000 driver-probe-version 0x00000000 driver-probe-port 0xFFFFFFFF driver-probe-ssts 0x00000000 driver-probe-sig 0x00000000 ' +
        'driver-probe-cmd 0x00000000 driver-probe-tfd 0x00000000 driver-probe-ci 0x00000000 driver-probe-serr 0x00000000 driver-probe-kind 0 driver-probe-read-ready 0 driver-probe-busy 0 driver-probe-drq 0 ' +
        'driver-probe-ci-idle 0 driver-probe-serr-clear 0 driver-probe-op 0 driver-probe-lba 0 driver-probe-blocks 0 driver-probe-bytes 0 driver-probe-mmio-written 0 driver-probe-port-programmed 0 ' +
        'driver-probe-published 0 driver-probe-command-issued 0 driver-probe-dma 0 driver-probe-armed 0 driver-probe-staged 0 driver-probe-denials 1 driver-probe-unavailable 1 denied-driver-intent 0xFFFFFFFF driver-intent 0xFFFFFFFF driver-intent-state 2 driver-intent-flags 0x0003FC01 ' +
        'driver-intent-token 0x00000000 driver-intent-probe-token 0xFFFFFFFF driver-intent-cap 0xFFFFFFFF driver-intent-owner 0x00001006 driver-intent-owner-bound 0 driver-intent-query-only 0 driver-intent-port 0xFFFFFFFF ' +
        'driver-intent-kind 0 driver-intent-op 0 driver-intent-lba 0 driver-intent-blocks 0 driver-intent-bytes 0 driver-intent-read-ready 0 driver-intent-mmio-written 0 driver-intent-port-programmed 0 ' +
        'driver-intent-published 0 driver-intent-command-issued 0 driver-intent-dma 0 driver-intent-armed 0 driver-intent-media-read 0 driver-intent-staged 0 driver-intent-denials 1 driver-intent-unavailable 1 denied-driver-buffer 0xFFFFFFFF driver-buffer 0xFFFFFFFF driver-buffer-state 2 driver-buffer-flags 0x0003FC01 driver-buffer-token 0x00000000 driver-buffer-intent-token 0xFFFFFFFF driver-buffer-cap 0xFFFFFFFF driver-buffer-owner 0x00001006 driver-buffer-owner-bound 0 driver-buffer-query-only 0 driver-buffer-port 0xFFFFFFFF driver-buffer-kind 0 driver-buffer-op 0 driver-buffer-lba 0 driver-buffer-blocks 0 driver-buffer-read-bytes 0 driver-buffer-page-bytes 0 driver-buffer-offset 0 driver-buffer-checksum 0x00000000 driver-buffer-zeroed 0 driver-buffer-read-ready 0 driver-buffer-mmio-written 0 driver-buffer-port-programmed 0 driver-buffer-published 0 driver-buffer-command-issued 0 driver-buffer-dma 0 driver-buffer-armed 0 driver-buffer-media-read 0 driver-buffer-staged 0 driver-buffer-denials 1 driver-buffer-unavailable 1 denied-driver-gate 0xFFFFFFFF driver-gate 0xFFFFFFFF driver-gate-state 2 driver-gate-flags 0x001FFC01 driver-gate-token 0x00000000 driver-gate-buffer-token 0xFFFFFFFF driver-gate-cap 0xFFFFFFFF driver-gate-owner 0x00001006 driver-gate-owner-bound 0 driver-gate-query-only 0 driver-gate-port 0xFFFFFFFF driver-gate-kind 0 driver-gate-op 0 driver-gate-lba 0 driver-gate-blocks 0 driver-gate-read-bytes 0 driver-gate-page-bytes 0 driver-gate-checksum 0x00000000 driver-gate-zeroed 0 driver-gate-read-ready 0 driver-gate-exec-required 1 driver-gate-exec-granted 0 driver-gate-issue-allowed 0 driver-gate-mmio-written 0 driver-gate-port-programmed 0 driver-gate-published 0 driver-gate-command-issued 0 driver-gate-dma 0 driver-gate-armed 0 driver-gate-media-read 0 driver-gate-staged 0 driver-gate-denials 1 driver-gate-unavailable 1 denied-driver-exec 0xFFFFFFFF driver-exec 0xFFFFFFFF driver-exec-state 2 driver-exec-flags 0x00DFFC01 driver-exec-token 0x00000000 driver-exec-gate-token 0xFFFFFFFF driver-exec-cap 0xFFFFFFFF driver-exec-owner 0x00001006 driver-exec-owner-bound 0 driver-exec-query-only 0 driver-exec-port 0xFFFFFFFF driver-exec-kind 0 driver-exec-op 0 driver-exec-lba 0 driver-exec-blocks 0 driver-exec-read-bytes 0 driver-exec-page-bytes 0 driver-exec-checksum 0x00000000 driver-exec-zeroed 0 driver-exec-read-ready 0 driver-exec-attempted 1 driver-exec-required 1 driver-exec-granted 0 driver-exec-issue-allowed 0 driver-exec-issue-denied 1 driver-exec-mmio-written 0 driver-exec-port-programmed 0 driver-exec-published 0 driver-exec-command-issued 0 driver-exec-dma 0 driver-exec-armed 0 driver-exec-media-read 0 driver-exec-staged 0 driver-exec-denials 1 driver-exec-unavailable 1 denied-driver-result 0xFFFFFFFF driver-result 0xFFFFFFFF driver-result-state 2 driver-result-flags 0x01FFFC01 driver-result-token 0x00000000 driver-result-exec-token 0xFFFFFFFF driver-result-cap 0xFFFFFFFF driver-result-owner 0x00001006 driver-result-owner-bound 0 driver-result-query-only 0 driver-result-port 0xFFFFFFFF driver-result-kind 0 driver-result-op 0 driver-result-lba 0 driver-result-blocks 0 driver-result-read-bytes 0 driver-result-page-bytes 0 driver-result-checksum 0x00000000 driver-result-zeroed 0 driver-result-read-ready 0 driver-result-exec-denied 1 driver-result-requested 1 driver-result-granted 0 driver-result-denied 1 driver-result-bytes-available 0 driver-result-block-cap-minted 0 driver-result-fs-minted 0 driver-result-mmio-written 0 driver-result-port-programmed 0 driver-result-published 0 driver-result-command-issued 0 driver-result-dma 0 driver-result-armed 0 driver-result-media-read 0 driver-result-staged 0 driver-result-denials 1 driver-result-unavailable 1 denied-driver-publish 0xFFFFFFFF driver-publish 0xFFFFFFFF driver-publish-state 2 driver-publish-flags 0x0FFFFC01 driver-publish-token 0x00000000 driver-publish-result-token 0xFFFFFFFF driver-publish-cap 0xFFFFFFFF driver-publish-owner 0x00001006 driver-publish-owner-bound 0 driver-publish-query-only 0 driver-publish-port 0xFFFFFFFF driver-publish-kind 0 driver-publish-op 0 driver-publish-lba 0 driver-publish-blocks 0 driver-publish-read-bytes 0 driver-publish-page-bytes 0 driver-publish-checksum 0x00000000 driver-publish-zeroed 0 driver-publish-read-ready 0 driver-publish-exec-denied 1 driver-publish-result-denied 1 driver-publish-bytes-available 0 driver-publish-requested 1 driver-publish-granted 0 driver-publish-denied 1 driver-publish-block-endpoint 0 driver-publish-block-cap-minted 0 driver-publish-fs-minted 0 driver-publish-mmio-written 0 driver-publish-port-programmed 0 driver-publish-published 0 driver-publish-command-issued 0 driver-publish-dma 0 driver-publish-armed 0 driver-publish-media-read 0 driver-publish-media-written 0 driver-publish-staged 0 driver-publish-denials 1 driver-publish-unavailable 1 denied-drg 0xFFFFFFFF drg 0xFFFFFFFF drg-state 2 drg-flags 0x7FFFF801 drg-token 0x00000000 drg-pub-token 0xFFFFFFFF drg-result-token 0xFFFFFFFF drg-cap 0xFFFFFFFF drg-owner 0x00001006 drg-owner-bound 0 drg-qonly 0 drg-port 0xFFFFFFFF drg-kind 0 drg-op 0 drg-lba 0 drg-blocks 0 drg-read-bytes 0 drg-page-bytes 0 drg-checksum 0x00000000 drg-zeroed 0 drg-ready 0 drg-exec-denied 1 drg-result-denied 1 drg-pub-denied 1 drg-bytes 0 drg-requested 1 drg-granted 0 drg-denied 1 drg-media-auth 0 drg-block-endpoint 0 drg-block-cap 0 drg-fs-minted 0 drg-mmio-written 0 drg-port-programmed 0 drg-published 0 drg-command-issued 0 drg-dma 0 drg-armed 0 drg-media-read 0 drg-media-written 0 drg-staged 0 drg-denials 1 drg-unavailable 1 map-requests 0 .* queries 359 denials 28'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosRevokePattern -Message "x64 BIOS brokered MMIO AHCI publish revocation-plan unavailable proof was not observed."
    $biosDriverMediaReadPattern = (
        '\[x64\] mmio planner service 9 .* denied-dmr 0xFFFFFFFF dmr 0xFFFFFFFF dmr-state 2 dmr-flags 0x1FFFFC01 ' +
        'dmr-token 0x00000000 dmr-grant-token 0xFFFFFFFF dmr-cap 0xFFFFFFFF dmr-owner 0x00001006 dmr-owner-bound 0 dmr-qonly 0 ' +
        'dmr-port 0xFFFFFFFF dmr-kind 0 dmr-op 0 dmr-lba 0 dmr-blocks 0 dmr-read-bytes 0 dmr-page-bytes 0 ' +
        'dmr-checksum 0x00000000 dmr-zeroed 0 dmr-ready 0 dmr-drg-denied 1 dmr-auth 0 dmr-attempted 1 dmr-denied 1 ' +
        'dmr-bytes 0 dmr-block-endpoint 0 dmr-block-cap 0 dmr-fs-minted 0 dmr-mmio-written 0 dmr-port-programmed 0 ' +
        'dmr-published 0 dmr-command-issued 0 dmr-dma 0 dmr-armed 0 dmr-media-read 0 dmr-media-written 0 dmr-buffer 1 ' +
        'dmr-staged 0 dmr-denials 1 dmr-unavailable 1 map-requests 0 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverMediaReadPattern -Message "x64 BIOS AHCI denied media-read consumption proof was not observed."
    $biosDriverCompletePattern = (
        '\[x64\] mmio planner service 9 .* denied-drc 0xFFFFFFFF drc 0xFFFFFFFF drc-state 2 drc-flags 0x3FFFFC01 ' +
        'drc-token 0x00000000 drc-dmr-token 0xFFFFFFFF drc-cap 0xFFFFFFFF drc-owner 0x00001006 drc-owner-bound 0 drc-qonly 0 ' +
        'drc-port 0xFFFFFFFF drc-kind 0 drc-op 0 drc-lba 0 drc-blocks 0 drc-read-bytes 0 drc-page-bytes 0 ' +
        'drc-checksum 0x00000000 drc-zeroed 0 drc-ready 0 drc-dmr-denied 1 drc-requested 1 drc-granted 0 drc-denied 1 ' +
        'drc-completed 0 drc-status 0 drc-bytes 0 drc-block-endpoint 0 drc-block-cap 0 drc-fs-minted 0 ' +
        'drc-mmio-written 0 drc-port-programmed 0 drc-published 0 drc-command-issued 0 drc-dma 0 drc-armed 0 ' +
        'drc-media-read 0 drc-media-written 0 drc-buffer 1 drc-staged 0 drc-denials 1 drc-unavailable 1 map-requests 0 .* queries 359 denials 30'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverCompletePattern -Message "x64 BIOS AHCI denied read-completion proof was not observed."
    $biosDriverReadCapPattern = (
        '\[x64\] mmio planner service 9 .* denied-drcap 0xFFFFFFFF drcap 0xFFFFFFFF drcap-state 2 drcap-flags 0x1FFFFC01 ' +
        'drcap-token 0x00000000 drcap-drc-token 0xFFFFFFFF drcap-cap 0xFFFFFFFF drcap-owner 0x00001006 drcap-owner-bound 0 drcap-qonly 0 ' +
        'drcap-port 0xFFFFFFFF drcap-kind 0 drcap-op 0 drcap-lba 0 drcap-blocks 0 drcap-read-bytes 0 drcap-page-bytes 0 ' +
        'drcap-checksum 0x00000000 drcap-zeroed 0 drcap-ready 0 drcap-drc-denied 1 drcap-requested 1 drcap-granted 0 drcap-denied 1 ' +
        'drcap-bytes 0 drcap-block-endpoint 0 drcap-block-cap 0 drcap-fs-minted 0 drcap-mmio-written 0 drcap-port-programmed 0 ' +
        'drcap-published 0 drcap-command-issued 0 drcap-dma 0 drcap-armed 0 drcap-media-read 0 drcap-media-written 0 drcap-buffer 1 ' +
        'drcap-staged 0 drcap-denials 1 drcap-unavailable 1 map-requests 0 .* queries 359 denials 31'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadCapPattern -Message "x64 BIOS AHCI denied read-capability proof was not observed."
    $biosDriverReadExportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drx 0xFFFFFFFF drx 0xFFFFFFFF drx-state 2 drx-flags 0x7FFFFC01 ' +
        'drx-token 0x00000000 drx-drcap-token 0xFFFFFFFF drx-cap 0xFFFFFFFF drx-owner 0x00001006 drx-owner-bound 0 drx-qonly 0 ' +
        'drx-port 0xFFFFFFFF drx-kind 0 drx-op 0 drx-lba 0 drx-blocks 0 drx-read-bytes 0 drx-page-bytes 0 ' +
        'drx-checksum 0x00000000 drx-zeroed 0 drx-ready 0 drx-drcap-denied 1 drx-requested 1 drx-granted 0 drx-denied 1 ' +
        'drx-bytes 0 drx-user-bytes 0 drx-user-buffer 0 drx-block-endpoint 0 drx-block-cap 0 drx-fs-minted 0 ' +
        'drx-mmio-written 0 drx-port-programmed 0 drx-published 0 drx-command-issued 0 drx-dma 0 drx-armed 0 ' +
        'drx-media-read 0 drx-media-written 0 drx-buffer 1 drx-staged 0 drx-denials 1 drx-unavailable 1 map-requests 0 .* queries 359 denials 32'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadExportPattern -Message "x64 BIOS AHCI denied read-export proof was not observed."
    $biosDriverReadResponsePattern = (
        '\[x64\] mmio planner service 9 .* denied-drr 0xFFFFFFFF drr 0xFFFFFFFF drr-state 2 drr-flags 0x7FFFFC01 ' +
        'drr-token 0x00000000 drr-drx-token 0xFFFFFFFF drr-cap 0xFFFFFFFF drr-owner 0x00001006 drr-owner-bound 0 drr-qonly 0 ' +
        'drr-port 0xFFFFFFFF drr-kind 0 drr-op 0 drr-lba 0 drr-blocks 0 drr-read-bytes 0 drr-page-bytes 0 ' +
        'drr-checksum 0x00000000 drr-zeroed 0 drr-ready 0 drr-drx-denied 1 drr-requested 1 drr-granted 0 drr-denied 1 ' +
        'drr-bytes 0 drr-resp-bytes 0 drr-resp-status 0 drr-resp-checksum 0x00000000 drr-block-endpoint 0 drr-block-cap 0 drr-fs-minted 0 ' +
        'drr-mmio-written 0 drr-port-programmed 0 drr-published 0 drr-command-issued 0 drr-dma 0 drr-armed 0 ' +
        'drr-media-read 0 drr-media-written 0 drr-buffer 1 drr-staged 0 drr-denials 1 drr-unavailable 1 map-requests 0 .* queries 359 denials 33'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadResponsePattern -Message "x64 BIOS AHCI denied read-response proof was not observed."
    $biosDriverReadDeliveryPattern = (
        '\[x64\] mmio planner service 9 .* denied-drd 0xFFFFFFFF drd 0xFFFFFFFF drd-state 2 drd-flags 0x7FFFFC01 ' +
        'drd-token 0x00000000 drd-drr-token 0xFFFFFFFF drd-cap 0xFFFFFFFF drd-owner 0x00001006 drd-owner-bound 0 drd-qonly 0 ' +
        'drd-port 0xFFFFFFFF drd-kind 0 drd-op 0 drd-lba 0 drd-blocks 0 drd-read-bytes 0 drd-page-bytes 0 ' +
        'drd-checksum 0x00000000 drd-zeroed 0 drd-ready 0 drd-drr-denied 1 drd-requested 1 drd-granted 0 drd-denied 1 ' +
        'drd-bytes 0 drd-deliv-bytes 0 drd-deliv-status 0 drd-deliv-checksum 0x00000000 drd-block-endpoint 0 drd-block-cap 0 drd-fs-minted 0 ' +
        'drd-mmio-written 0 drd-port-programmed 0 drd-published 0 drd-command-issued 0 drd-dma 0 drd-armed 0 ' +
        'drd-media-read 0 drd-media-written 0 drd-buffer 1 drd-staged 0 drd-denials 1 drd-unavailable 1 map-requests 0 .* queries 359 denials 34'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadDeliveryPattern -Message "x64 BIOS AHCI denied read-delivery proof was not observed."
    $biosDriverReadVisiblePattern = (
        '\[x64\] mmio planner service 9 .* denied-drv 0xFFFFFFFF drv 0xFFFFFFFF drv-state 2 drv-flags 0x7FFFFC01 ' +
        'drv-token 0x00000000 drv-drd-token 0xFFFFFFFF drv-cap 0xFFFFFFFF drv-owner 0x00001006 drv-owner-bound 0 drv-qonly 0 ' +
        'drv-port 0xFFFFFFFF drv-kind 0 drv-op 0 drv-lba 0 drv-blocks 0 drv-read-bytes 0 drv-page-bytes 0 ' +
        'drv-checksum 0x00000000 drv-zeroed 0 drv-ready 0 drv-drd-denied 1 drv-requested 1 drv-granted 0 drv-denied 1 ' +
        'drv-bytes 0 drv-vis-bytes 0 drv-vis-status 0 drv-vis-checksum 0x00000000 drv-block-endpoint 0 drv-block-cap 0 drv-fs-minted 0 ' +
        'drv-mmio-written 0 drv-port-programmed 0 drv-published 0 drv-command-issued 0 drv-dma 0 drv-armed 0 ' +
        'drv-media-read 0 drv-media-written 0 drv-buffer 1 drv-staged 0 drv-denials 1 drv-unavailable 1 map-requests 0 .* queries 359 denials 35'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadVisiblePattern -Message "x64 BIOS AHCI denied read-visibility proof was not observed."
    $biosDriverReadCommitPattern = (
        '\[x64\] mmio planner service 9 .* denied-drk 0xFFFFFFFF drk 0xFFFFFFFF drk-state 2 drk-flags 0x7FFFFC01 ' +
        'drk-token 0x00000000 drk-drv-token 0xFFFFFFFF drk-cap 0xFFFFFFFF drk-owner 0x00001006 drk-owner-bound 0 drk-qonly 0 ' +
        'drk-port 0xFFFFFFFF drk-kind 0 drk-op 0 drk-lba 0 drk-blocks 0 drk-read-bytes 0 drk-page-bytes 0 ' +
        'drk-checksum 0x00000000 drk-zeroed 0 drk-ready 0 drk-drv-denied 1 drk-requested 1 drk-granted 0 drk-denied 1 ' +
        'drk-bytes 0 drk-commit-bytes 0 drk-commit-status 0 drk-commit-checksum 0x00000000 drk-block-endpoint 0 drk-block-cap 0 drk-fs-minted 0 ' +
        'drk-mmio-written 0 drk-port-programmed 0 drk-published 0 drk-command-issued 0 drk-dma 0 drk-armed 0 ' +
        'drk-media-read 0 drk-media-written 0 drk-buffer 1 drk-staged 0 drk-denials 1 drk-unavailable 1 map-requests 0 .* queries 359 denials 36'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadCommitPattern -Message "x64 BIOS AHCI denied read-commit proof was not observed."
    $biosDriverReadAuditPattern = (
        '\[x64\] mmio planner service 9 .* denied-dra 0xFFFFFFFF dra 0xFFFFFFFF dra-state 2 dra-flags 0x7FFFFC01 ' +
        'dra-token 0x00000000 dra-drk-token 0xFFFFFFFF dra-cap 0xFFFFFFFF dra-owner 0x00001006 dra-owner-bound 0 dra-qonly 0 ' +
        'dra-port 0xFFFFFFFF dra-kind 0 dra-op 0 dra-lba 0 dra-blocks 0 dra-read-bytes 0 dra-page-bytes 0 ' +
        'dra-checksum 0x00000000 dra-zeroed 0 dra-ready 0 dra-drk-denied 1 dra-requested 1 dra-granted 0 dra-denied 1 ' +
        'dra-bytes 0 dra-audit-bytes 0 dra-audit-status 0 dra-audit-checksum 0x00000000 dra-block-endpoint 0 dra-block-cap 0 dra-fs-minted 0 ' +
        'dra-mmio-written 0 dra-port-programmed 0 dra-published 0 dra-command-issued 0 dra-dma 0 dra-armed 0 ' +
        'dra-media-read 0 dra-media-written 0 dra-buffer 1 dra-staged 0 dra-denials 1 dra-unavailable 1 map-requests 0 .* queries 359 denials 37'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadAuditPattern -Message "x64 BIOS AHCI denied read-audit proof was not observed."
    $biosDriverReadUpgradePattern = (
        '\[x64\] mmio planner service 9 .* denied-dru 0xFFFFFFFF dru 0xFFFFFFFF dru-state 2 dru-flags 0x7FFFFC01 ' +
        'dru-token 0x00000000 dru-dra-token 0xFFFFFFFF dru-cap 0xFFFFFFFF dru-owner 0x00001006 dru-owner-bound 0 dru-qonly 0 ' +
        'dru-port 0xFFFFFFFF dru-kind 0 dru-op 0 dru-lba 0 dru-blocks 0 dru-read-bytes 0 dru-page-bytes 0 ' +
        'dru-checksum 0x00000000 dru-zeroed 0 dru-ready 0 dru-dra-denied 1 dru-requested 1 dru-granted 0 dru-denied 1 ' +
        'dru-bytes 0 dru-up-cap 0xFFFFFFFF dru-media-auth 0 dru-exec-auth 0 dru-block-endpoint 0 dru-block-cap 0 dru-fs-minted 0 ' +
        'dru-mmio-written 0 dru-port-programmed 0 dru-published 0 dru-command-issued 0 dru-dma 0 dru-armed 0 ' +
        'dru-media-read 0 dru-media-written 0 dru-buffer 1 dru-staged 0 dru-denials 1 dru-unavailable 1 map-requests 0 .* queries 359 denials 38'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadUpgradePattern -Message "x64 BIOS AHCI denied read-upgrade proof was not observed."
    $biosDriverReadActivatePattern = (
        '\[x64\] mmio planner service 9 .* denied-dact 0xFFFFFFFF dact 0xFFFFFFFF dact-state 2 dact-flags 0x7FFFFC01 ' +
        'dact-token 0x00000000 dact-dru-token 0xFFFFFFFF dact-cap 0xFFFFFFFF dact-owner 0x00001006 dact-owner-bound 0 dact-qonly 0 ' +
        'dact-port 0xFFFFFFFF dact-kind 0 dact-op 0 dact-lba 0 dact-blocks 0 dact-read-bytes 0 dact-page-bytes 0 ' +
        'dact-checksum 0x00000000 dact-zeroed 0 dact-ready 0 dact-dru-denied 1 dact-requested 1 dact-granted 0 dact-denied 1 ' +
        'dact-bytes 0 dact-act-cap 0xFFFFFFFF dact-read-auth 0 dact-exec-auth 0 dact-block-endpoint 0 dact-block-cap 0 dact-fs-minted 0 ' +
        'dact-mmio-written 0 dact-port-programmed 0 dact-published 0 dact-command-issued 0 dact-dma 0 dact-armed 0 ' +
        'dact-media-read 0 dact-media-written 0 dact-buffer 1 dact-staged 0 dact-denials 1 dact-unavailable 1 map-requests 0 .* queries 359 denials 39'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadActivatePattern -Message "x64 BIOS AHCI denied read-activation proof was not observed."
    $biosDriverReadArmPattern = (
        '\[x64\] mmio planner service 9 .* denied-darm 0xFFFFFFFF darm 0xFFFFFFFF darm-state 2 darm-flags 0x7FFFFC01 ' +
        'darm-token 0x00000000 darm-dact-token 0xFFFFFFFF darm-cap 0xFFFFFFFF darm-owner 0x00001006 darm-owner-bound 0 darm-qonly 0 ' +
        'darm-port 0xFFFFFFFF darm-kind 0 darm-op 0 darm-lba 0 darm-blocks 0 darm-read-bytes 0 darm-page-bytes 0 ' +
        'darm-checksum 0x00000000 darm-zeroed 0 darm-ready 0 darm-dact-denied 1 darm-requested 1 darm-granted 0 darm-denied 1 ' +
        'darm-bytes 0 darm-arm-cap 0xFFFFFFFF darm-read-auth 0 darm-exec-auth 0 darm-block-endpoint 0 darm-block-cap 0 darm-fs-minted 0 ' +
        'darm-mmio-written 0 darm-port-programmed 0 darm-published 0 darm-command-issued 0 darm-dma 0 darm-armed 0 ' +
        'darm-media-read 0 darm-media-written 0 darm-buffer 1 darm-staged 0 darm-denials 1 darm-unavailable 1 map-requests 0 .* queries 359 denials 40'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadArmPattern -Message "x64 BIOS AHCI denied read-arm proof was not observed."
    $biosDriverReadSubmitPattern = (
        '\[x64\] mmio planner service 9 .* denied-dsub 0xFFFFFFFF dsub 0xFFFFFFFF dsub-state 2 dsub-flags 0x7FFFFC01 ' +
        'dsub-token 0x00000000 dsub-darm-token 0xFFFFFFFF dsub-cap 0xFFFFFFFF dsub-owner 0x00001006 dsub-owner-bound 0 dsub-qonly 0 ' +
        'dsub-port 0xFFFFFFFF dsub-kind 0 dsub-op 0 dsub-lba 0 dsub-blocks 0 dsub-read-bytes 0 dsub-page-bytes 0 ' +
        'dsub-checksum 0x00000000 dsub-zeroed 0 dsub-ready 0 dsub-darm-denied 1 dsub-requested 1 dsub-granted 0 dsub-denied 1 ' +
        'dsub-bytes 0 dsub-submit-cap 0xFFFFFFFF dsub-read-auth 0 dsub-exec-auth 0 dsub-block-endpoint 0 dsub-block-cap 0 dsub-fs-minted 0 ' +
        'dsub-mmio-written 0 dsub-port-programmed 0 dsub-published 0 dsub-command-issued 0 dsub-dma 0 dsub-armed 0 ' +
        'dsub-media-read 0 dsub-media-written 0 dsub-buffer 1 dsub-staged 0 dsub-denials 1 dsub-unavailable 1 map-requests 0 .* queries 359 denials 41'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadSubmitPattern -Message "x64 BIOS AHCI denied read-submit proof was not observed."
    $biosDriverReadObservePattern = (
        '\[x64\] mmio planner service 9 .* denied-dobs 0xFFFFFFFF dobs 0xFFFFFFFF dobs-state 2 dobs-flags 0x7FFFFC01 ' +
        'dobs-token 0x00000000 dobs-dsub-token 0xFFFFFFFF dobs-cap 0xFFFFFFFF dobs-owner 0x00001006 dobs-owner-bound 0 dobs-qonly 0 ' +
        'dobs-port 0xFFFFFFFF dobs-kind 0 dobs-op 0 dobs-lba 0 dobs-blocks 0 dobs-read-bytes 0 dobs-page-bytes 0 ' +
        'dobs-checksum 0x00000000 dobs-zeroed 0 dobs-ready 0 dobs-dsub-denied 1 dobs-requested 1 dobs-granted 0 dobs-denied 1 ' +
        'dobs-bytes 0 dobs-obs-status 0 dobs-obs-bytes 0 dobs-obs-checksum 0x00000000 dobs-read-auth 0 dobs-exec-auth 0 ' +
        'dobs-block-endpoint 0 dobs-block-cap 0 dobs-fs-minted 0 dobs-mmio-written 0 dobs-port-programmed 0 dobs-published 0 ' +
        'dobs-command-issued 0 dobs-dma 0 dobs-armed 0 dobs-media-read 0 dobs-media-written 0 dobs-buffer 1 ' +
        'dobs-staged 0 dobs-denials 1 dobs-unavailable 1 map-requests 0 .* queries 359 denials 42'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadObservePattern -Message "x64 BIOS AHCI denied read-observe proof was not observed."
    $biosDriverReadRetirePattern = (
        '\[x64\] mmio planner service 9 .* denied-dret 0xFFFFFFFF dret 0xFFFFFFFF dret-state 2 dret-flags 0x7FFFFC01 ' +
        'dret-token 0x00000000 dret-dobs-token 0xFFFFFFFF dret-cap 0xFFFFFFFF dret-owner 0x00001006 dret-owner-bound 0 dret-qonly 0 ' +
        'dret-port 0xFFFFFFFF dret-kind 0 dret-op 0 dret-lba 0 dret-blocks 0 dret-read-bytes 0 dret-page-bytes 0 ' +
        'dret-checksum 0x00000000 dret-zeroed 0 dret-ready 0 dret-dobs-denied 1 dret-requested 1 dret-granted 0 dret-denied 1 ' +
        'dret-bytes 0 dret-ret-status 0 dret-ret-bytes 0 dret-ret-checksum 0x00000000 dret-read-auth 0 dret-exec-auth 0 ' +
        'dret-block-endpoint 0 dret-block-cap 0 dret-fs-minted 0 dret-mmio-written 0 dret-port-programmed 0 dret-published 0 ' +
        'dret-command-issued 0 dret-dma 0 dret-armed 0 dret-media-read 0 dret-media-written 0 dret-buffer 1 ' +
        'dret-staged 0 dret-denials 1 dret-unavailable 1 map-requests 0 .* queries 359 denials 43'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadRetirePattern -Message "x64 BIOS AHCI denied read-retire proof was not observed."
    $biosDriverReadPermitPattern = (
        '\[x64\] mmio planner service 9 .* denied-dprm 0xFFFFFFFF dprm 0xFFFFFFFF dprm-state 2 dprm-flags 0x7FFFFC01 ' +
        'dprm-token 0x00000000 dprm-dret-token 0xFFFFFFFF dprm-cap 0xFFFFFFFF dprm-owner 0x00001006 dprm-owner-bound 0 dprm-qonly 0 ' +
        'dprm-port 0xFFFFFFFF dprm-kind 0 dprm-op 0 dprm-lba 0 dprm-blocks 0 dprm-read-bytes 0 dprm-page-bytes 0 ' +
        'dprm-checksum 0x00000000 dprm-zeroed 0 dprm-ready 0 dprm-dret-denied 1 dprm-requested 1 dprm-granted 0 dprm-denied 1 ' +
        'dprm-bytes 0 dprm-permit-cap 0xFFFFFFFF dprm-read-auth 0 dprm-exec-auth 0 dprm-block-endpoint 0 dprm-block-cap 0 dprm-fs-minted 0 ' +
        'dprm-mmio-written 0 dprm-port-programmed 0 dprm-published 0 dprm-command-issued 0 dprm-dma 0 dprm-armed 0 ' +
        'dprm-media-read 0 dprm-media-written 0 dprm-buffer 1 dprm-staged 0 dprm-denials 1 dprm-unavailable 1 map-requests 0 .* queries 359 denials 44'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadPermitPattern -Message "x64 BIOS AHCI denied read-permit proof was not observed."
    $biosDriverReadWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-dwin 0xFFFFFFFF dwin 0xFFFFFFFF dwin-state 2 dwin-flags 0x7FFFFC01 ' +
        'dwin-token 0x00000000 dwin-dprm-token 0xFFFFFFFF dwin-cap 0xFFFFFFFF dwin-owner 0x00001006 dwin-owner-bound 0 dwin-qonly 0 ' +
        'dwin-port 0xFFFFFFFF dwin-kind 0 dwin-op 0 dwin-lba 0 dwin-blocks 0 dwin-read-bytes 0 dwin-page-bytes 0 ' +
        'dwin-checksum 0x00000000 dwin-zeroed 0 dwin-ready 0 dwin-dprm-denied 1 dwin-requested 1 dwin-granted 0 dwin-denied 1 ' +
        'dwin-bytes 0 dwin-window-cap 0xFFFFFFFF dwin-open 0 dwin-read-auth 0 dwin-exec-auth 0 dwin-block-endpoint 0 dwin-block-cap 0 dwin-fs-minted 0 ' +
        'dwin-mmio-written 0 dwin-port-programmed 0 dwin-published 0 dwin-command-issued 0 dwin-dma 0 dwin-armed 0 ' +
        'dwin-media-read 0 dwin-media-written 0 dwin-buffer 1 dwin-staged 0 dwin-denials 1 dwin-unavailable 1 map-requests 0 .* queries 359 denials 45'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadWindowPattern -Message "x64 BIOS AHCI denied read-window proof was not observed."
    $biosDriverReadLeasePattern = (
        '\[x64\] mmio planner service 9 .* denied-dlse 0xFFFFFFFF dlse 0xFFFFFFFF dlse-state 2 dlse-flags 0x7FFFFC01 ' +
        'dlse-token 0x00000000 dlse-dwin-token 0xFFFFFFFF dlse-cap 0xFFFFFFFF dlse-owner 0x00001006 dlse-owner-bound 0 dlse-qonly 0 ' +
        'dlse-port 0xFFFFFFFF dlse-kind 0 dlse-op 0 dlse-lba 0 dlse-blocks 0 dlse-read-bytes 0 dlse-page-bytes 0 ' +
        'dlse-checksum 0x00000000 dlse-zeroed 0 dlse-ready 0 dlse-dwin-denied 1 dlse-requested 1 dlse-granted 0 dlse-denied 1 ' +
        'dlse-bytes 0 dlse-lease-cap 0xFFFFFFFF dlse-active 0 dlse-read-auth 0 dlse-exec-auth 0 dlse-block-endpoint 0 dlse-block-cap 0 dlse-fs-minted 0 ' +
        'dlse-mmio-written 0 dlse-port-programmed 0 dlse-published 0 dlse-command-issued 0 dlse-dma 0 dlse-armed 0 ' +
        'dlse-media-read 0 dlse-media-written 0 dlse-buffer 1 dlse-staged 0 dlse-denials 1 dlse-unavailable 1 map-requests 0 .* queries 359 denials 46'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadLeasePattern -Message "x64 BIOS AHCI denied read-lease proof was not observed."
    $biosDriverReadUsePattern = (
        '\[x64\] mmio planner service 9 .* denied-duse 0xFFFFFFFF duse 0xFFFFFFFF duse-state 2 duse-flags 0x7FFFFC01 ' +
        'duse-token 0x00000000 duse-dlse-token 0xFFFFFFFF duse-cap 0xFFFFFFFF duse-owner 0x00001006 duse-owner-bound 0 duse-qonly 0 ' +
        'duse-port 0xFFFFFFFF duse-kind 0 duse-op 0 duse-lba 0 duse-blocks 0 duse-read-bytes 0 duse-page-bytes 0 ' +
        'duse-checksum 0x00000000 duse-zeroed 0 duse-ready 0 duse-dlse-denied 1 duse-requested 1 duse-granted 0 duse-denied 1 ' +
        'duse-bytes 0 duse-use-cap 0xFFFFFFFF duse-active 0 duse-read-auth 0 duse-exec-auth 0 duse-block-endpoint 0 duse-block-cap 0 duse-fs-minted 0 ' +
        'duse-mmio-written 0 duse-port-programmed 0 duse-published 0 duse-command-issued 0 duse-dma 0 duse-armed 0 ' +
        'duse-media-read 0 duse-media-written 0 duse-buffer 1 duse-staged 0 duse-denials 1 duse-unavailable 1 map-requests 0 .* queries 359 denials 47'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadUsePattern -Message "x64 BIOS AHCI denied read-use proof was not observed."
    $biosDriverReadReportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drpt 0xFFFFFFFF drpt 0xFFFFFFFF drpt-state 2 drpt-flags 0x7FFFFC01 ' +
        'drpt-token 0x00000000 drpt-duse-token 0xFFFFFFFF drpt-cap 0xFFFFFFFF drpt-owner 0x00001006 drpt-owner-bound 0 drpt-qonly 0 ' +
        'drpt-port 0xFFFFFFFF drpt-kind 0 drpt-op 0 drpt-lba 0 drpt-blocks 0 drpt-read-bytes 0 drpt-page-bytes 0 ' +
        'drpt-checksum 0x00000000 drpt-zeroed 0 drpt-ready 0 drpt-duse-denied 1 drpt-requested 1 drpt-granted 0 drpt-denied 1 ' +
        'drpt-bytes 0 drpt-status 0 drpt-report-bytes 0 drpt-report-checksum 0x00000000 drpt-report-cap 0xFFFFFFFF ' +
        'drpt-read-auth 0 drpt-exec-auth 0 drpt-block-endpoint 0 drpt-block-cap 0 drpt-fs-minted 0 ' +
        'drpt-mmio-written 0 drpt-port-programmed 0 drpt-published 0 drpt-command-issued 0 drpt-dma 0 drpt-armed 0 ' +
        'drpt-media-read 0 drpt-media-written 0 drpt-buffer 1 drpt-staged 0 drpt-denials 1 drpt-unavailable 1 map-requests 0 .* queries 359 denials 48'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadReportPattern -Message "x64 BIOS AHCI denied read-report proof was not observed."
    $biosDriverReadReceiptPattern = (
        '\[x64\] mmio planner service 9 .* denied-drrc 0xFFFFFFFF drrc 0xFFFFFFFF drrc-state 2 drrc-flags 0x7FFFFC01 ' +
        'drrc-token 0x00000000 drrc-drpt-token 0xFFFFFFFF drrc-cap 0xFFFFFFFF drrc-owner 0x00001006 drrc-owner-bound 0 drrc-qonly 0 ' +
        'drrc-port 0xFFFFFFFF drrc-kind 0 drrc-op 0 drrc-lba 0 drrc-blocks 0 drrc-read-bytes 0 drrc-page-bytes 0 ' +
        'drrc-checksum 0x00000000 drrc-zeroed 0 drrc-ready 0 drrc-drpt-denied 1 drrc-requested 1 drrc-granted 0 drrc-denied 1 ' +
        'drrc-bytes 0 drrc-status 0 drrc-receipt-bytes 0 drrc-receipt-checksum 0x00000000 drrc-receipt-cap 0xFFFFFFFF ' +
        'drrc-read-auth 0 drrc-exec-auth 0 drrc-block-endpoint 0 drrc-block-cap 0 drrc-fs-minted 0 ' +
        'drrc-mmio-written 0 drrc-port-programmed 0 drrc-published 0 drrc-command-issued 0 drrc-dma 0 drrc-armed 0 ' +
        'drrc-media-read 0 drrc-media-written 0 drrc-buffer 1 drrc-staged 0 drrc-denials 1 drrc-unavailable 1 map-requests 0 .* queries 359 denials 49'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadReceiptPattern -Message "x64 BIOS AHCI denied read-receipt proof was not observed."
    $biosDriverReadAckPattern = (
        '\[x64\] mmio planner service 9 .* denied-drak 0xFFFFFFFF drak 0xFFFFFFFF drak-state 2 drak-flags 0x7FFFFC01 ' +
        'drak-token 0x00000000 drak-drrc-token 0xFFFFFFFF drak-cap 0xFFFFFFFF drak-owner 0x00001006 drak-owner-bound 0 drak-qonly 0 ' +
        'drak-port 0xFFFFFFFF drak-kind 0 drak-op 0 drak-lba 0 drak-blocks 0 drak-read-bytes 0 drak-page-bytes 0 ' +
        'drak-checksum 0x00000000 drak-zeroed 0 drak-ready 0 drak-drrc-denied 1 drak-requested 1 drak-granted 0 drak-denied 1 ' +
        'drak-bytes 0 drak-status 0 drak-ack-bytes 0 drak-ack-checksum 0x00000000 drak-ack-cap 0xFFFFFFFF ' +
        'drak-read-auth 0 drak-exec-auth 0 drak-block-endpoint 0 drak-block-cap 0 drak-fs-minted 0 ' +
        'drak-mmio-written 0 drak-port-programmed 0 drak-published 0 drak-command-issued 0 drak-dma 0 drak-armed 0 ' +
        'drak-media-read 0 drak-media-written 0 drak-buffer 1 drak-staged 0 drak-denials 1 drak-unavailable 1 map-requests 0 .* queries 359 denials 50'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadAckPattern -Message "x64 BIOS AHCI denied read-ack proof was not observed."
    $biosDriverReadClosePattern = (
        '\[x64\] mmio planner service 9 .* denied-drcl 0xFFFFFFFF drcl 0xFFFFFFFF drcl-state 2 drcl-flags 0x7FFFFC01 ' +
        'drcl-token 0x00000000 drcl-drak-token 0xFFFFFFFF drcl-cap 0xFFFFFFFF drcl-owner 0x00001006 drcl-owner-bound 0 drcl-qonly 0 ' +
        'drcl-port 0xFFFFFFFF drcl-kind 0 drcl-op 0 drcl-lba 0 drcl-blocks 0 drcl-read-bytes 0 drcl-page-bytes 0 ' +
        'drcl-checksum 0x00000000 drcl-zeroed 0 drcl-ready 0 drcl-drak-denied 1 drcl-requested 1 drcl-granted 0 drcl-denied 1 ' +
        'drcl-bytes 0 drcl-status 0 drcl-close-bytes 0 drcl-close-checksum 0x00000000 drcl-close-cap 0xFFFFFFFF ' +
        'drcl-read-auth 0 drcl-exec-auth 0 drcl-block-endpoint 0 drcl-block-cap 0 drcl-fs-minted 0 ' +
        'drcl-mmio-written 0 drcl-port-programmed 0 drcl-published 0 drcl-command-issued 0 drcl-dma 0 drcl-armed 0 ' +
        'drcl-media-read 0 drcl-media-written 0 drcl-buffer 1 drcl-staged 0 drcl-denials 1 drcl-unavailable 1 map-requests 0 .* queries 359 denials 51'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadClosePattern -Message "x64 BIOS AHCI denied read-close proof was not observed."
    $biosDriverReadSealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drsl 0xFFFFFFFF drsl 0xFFFFFFFF drsl-state 2 drsl-flags 0x7FFFFC01 ' +
        'drsl-token 0x00000000 drsl-drcl-token 0xFFFFFFFF drsl-cap 0xFFFFFFFF drsl-owner 0x00001006 drsl-owner-bound 0 drsl-qonly 0 ' +
        'drsl-port 0xFFFFFFFF drsl-kind 0 drsl-op 0 drsl-lba 0 drsl-blocks 0 drsl-read-bytes 0 drsl-page-bytes 0 ' +
        'drsl-checksum 0x00000000 drsl-zeroed 0 drsl-ready 0 drsl-drcl-denied 1 drsl-requested 1 drsl-granted 0 drsl-denied 1 ' +
        'drsl-bytes 0 drsl-status 0 drsl-seal-bytes 0 drsl-seal-checksum 0x00000000 drsl-seal-cap 0xFFFFFFFF ' +
        'drsl-read-auth 0 drsl-exec-auth 0 drsl-block-endpoint 0 drsl-block-cap 0 drsl-fs-minted 0 ' +
        'drsl-mmio-written 0 drsl-port-programmed 0 drsl-published 0 drsl-command-issued 0 drsl-dma 0 drsl-armed 0 ' +
        'drsl-media-read 0 drsl-media-written 0 drsl-buffer 1 drsl-staged 0 drsl-denials 1 drsl-unavailable 1 map-requests 0 .* queries 359 denials 52'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadSealPattern -Message "x64 BIOS AHCI denied read-seal proof was not observed."
    $biosDriverReadUnsealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drul 0xFFFFFFFF drul 0xFFFFFFFF drul-state 2 drul-flags 0x7FFFFC01 ' +
        'drul-token 0x00000000 drul-drsl-token 0xFFFFFFFF drul-cap 0xFFFFFFFF drul-owner 0x00001006 drul-owner-bound 0 drul-qonly 0 ' +
        'drul-port 0xFFFFFFFF drul-kind 0 drul-op 0 drul-lba 0 drul-blocks 0 drul-read-bytes 0 drul-page-bytes 0 ' +
        'drul-checksum 0x00000000 drul-zeroed 0 drul-ready 0 drul-drsl-denied 1 drul-requested 1 drul-granted 0 drul-denied 1 ' +
        'drul-bytes 0 drul-status 0 drul-unseal-bytes 0 drul-unseal-checksum 0x00000000 drul-unseal-cap 0xFFFFFFFF ' +
        'drul-read-auth 0 drul-exec-auth 0 drul-block-endpoint 0 drul-block-cap 0 drul-fs-minted 0 ' +
        'drul-mmio-written 0 drul-port-programmed 0 drul-published 0 drul-command-issued 0 drul-dma 0 drul-armed 0 ' +
        'drul-media-read 0 drul-media-written 0 drul-buffer 1 drul-staged 0 drul-denials 1 drul-unavailable 1 map-requests 0 .* queries 359 denials 53'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadUnsealPattern -Message "x64 BIOS AHCI denied read-unseal proof was not observed."
    $biosDriverReadDiscardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drdc 0xFFFFFFFF drdc 0xFFFFFFFF drdc-state 2 drdc-flags 0x7FFFFC01 ' +
        'drdc-token 0x00000000 drdc-drul-token 0xFFFFFFFF drdc-cap 0xFFFFFFFF drdc-owner 0x00001006 drdc-owner-bound 0 drdc-qonly 0 ' +
        'drdc-port 0xFFFFFFFF drdc-kind 0 drdc-op 0 drdc-lba 0 drdc-blocks 0 drdc-read-bytes 0 drdc-page-bytes 0 ' +
        'drdc-checksum 0x00000000 drdc-zeroed 0 drdc-ready 0 drdc-drul-denied 1 drdc-requested 1 drdc-granted 0 drdc-denied 1 ' +
        'drdc-bytes 0 drdc-status 0 drdc-discard-bytes 0 drdc-discard-checksum 0x00000000 drdc-discard-cap 0xFFFFFFFF ' +
        'drdc-read-auth 0 drdc-exec-auth 0 drdc-block-endpoint 0 drdc-block-cap 0 drdc-fs-minted 0 ' +
        'drdc-mmio-written 0 drdc-port-programmed 0 drdc-published 0 drdc-command-issued 0 drdc-dma 0 drdc-armed 0 ' +
        'drdc-media-read 0 drdc-media-written 0 drdc-buffer 1 drdc-staged 0 drdc-denials 1 drdc-unavailable 1 map-requests 0 .* queries 359 denials 54'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadDiscardPattern -Message "x64 BIOS AHCI denied read-discard proof was not observed."
    $biosDriverReadFinalizePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-finalize 0xFFFFFFFF driver-read-finalize 0xFFFFFFFF driver-read-finalize-state 2 driver-read-finalize-flags 0x7FFFFC01 ' +
        'driver-read-finalize-token 0x00000000 driver-read-finalize-read-discard-token 0xFFFFFFFF driver-read-finalize-cap 0xFFFFFFFF driver-read-finalize-owner 0x00001006 driver-read-finalize-owner-bound 0 driver-read-finalize-query-only 0 ' +
        'driver-read-finalize-port 0xFFFFFFFF driver-read-finalize-kind 0 driver-read-finalize-op 0 driver-read-finalize-lba 0 driver-read-finalize-blocks 0 driver-read-finalize-read-bytes 0 driver-read-finalize-page-bytes 0 ' +
        'driver-read-finalize-checksum 0x00000000 driver-read-finalize-zeroed 0 driver-read-finalize-read-ready 0 driver-read-finalize-read-discard-denied 1 driver-read-finalize-requested 1 driver-read-finalize-granted 0 driver-read-finalize-denied 1 ' +
        'driver-read-finalize-bytes-available 0 driver-read-finalize-status 0 driver-read-finalize-finalized-bytes 0 driver-read-finalize-finalize-checksum 0x00000000 driver-read-finalize-finalize-cap 0xFFFFFFFF ' +
        'driver-read-finalize-read-authority 0 driver-read-finalize-execute-authority 0 driver-read-finalize-block-endpoint 0 driver-read-finalize-block-cap-minted 0 driver-read-finalize-fs-minted 0 ' +
        'driver-read-finalize-mmio-written 0 driver-read-finalize-port-programmed 0 driver-read-finalize-published 0 driver-read-finalize-command-issued 0 driver-read-finalize-dma 0 driver-read-finalize-armed 0 ' +
        'driver-read-finalize-media-read 0 driver-read-finalize-media-written 0 driver-read-finalize-buffer-unchanged 1 driver-read-finalize-staged 0 driver-read-finalize-denials 1 driver-read-finalize-unavailable 1 map-requests 0 .* queries 359 denials 55'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadFinalizePattern -Message "x64 BIOS AHCI denied read-finalize proof was not observed."
    $biosDriverReadAuthorizePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-authorize 0xFFFFFFFF driver-read-authorize 0xFFFFFFFF driver-read-authorize-state 2 driver-read-authorize-flags 0x7FFFFC01 ' +
        'driver-read-authorize-token 0x00000000 driver-read-authorize-read-finalize-token 0xFFFFFFFF driver-read-authorize-cap 0xFFFFFFFF driver-read-authorize-owner 0x00001006 driver-read-authorize-owner-bound 0 driver-read-authorize-query-only 0 ' +
        'driver-read-authorize-port 0xFFFFFFFF driver-read-authorize-kind 0 driver-read-authorize-op 0 driver-read-authorize-lba 0 driver-read-authorize-blocks 0 driver-read-authorize-read-bytes 0 driver-read-authorize-page-bytes 0 ' +
        'driver-read-authorize-checksum 0x00000000 driver-read-authorize-zeroed 0 driver-read-authorize-read-ready 0 driver-read-authorize-read-finalize-denied 1 driver-read-authorize-requested 1 driver-read-authorize-granted 0 driver-read-authorize-denied 1 ' +
        'driver-read-authorize-policy-grant 0 driver-read-authorize-issue-authority 0 driver-read-authorize-dma-authority 0 driver-read-authorize-media-read-authority 0 driver-read-authorize-write-authority 0 driver-read-authorize-commit-authority 0 ' +
        'driver-read-authorize-block-endpoint 0 driver-read-authorize-block-cap-minted 0 driver-read-authorize-fs-minted 0 driver-read-authorize-mmio-written 0 driver-read-authorize-port-programmed 0 driver-read-authorize-published 0 ' +
        'driver-read-authorize-command-issued 0 driver-read-authorize-dma 0 driver-read-authorize-armed 0 driver-read-authorize-media-read 0 driver-read-authorize-media-written 0 driver-read-authorize-buffer-unchanged 1 ' +
        'driver-read-authorize-staged 0 driver-read-authorize-denials 1 driver-read-authorize-unavailable 1 map-requests 0 .* queries 359 denials 56'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadAuthorizePattern -Message "x64 BIOS AHCI denied read-authorize proof was not observed."
    $biosDriverReadDispatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-dispatch 0xFFFFFFFF driver-read-dispatch 0xFFFFFFFF driver-read-dispatch-state 2 driver-read-dispatch-flags 0x7FFFFC01 ' +
        'driver-read-dispatch-token 0x00000000 driver-read-dispatch-read-authorize-token 0xFFFFFFFF driver-read-dispatch-cap 0xFFFFFFFF driver-read-dispatch-owner 0x00001006 ' +
        'driver-read-dispatch-owner-bound 0 driver-read-dispatch-query-only 0 driver-read-dispatch-port 0xFFFFFFFF driver-read-dispatch-kind 0 driver-read-dispatch-op 0 ' +
        'driver-read-dispatch-lba 0 driver-read-dispatch-blocks 0 driver-read-dispatch-read-bytes 0 driver-read-dispatch-page-bytes 0 driver-read-dispatch-checksum 0x00000000 ' +
        'driver-read-dispatch-zeroed 0 driver-read-dispatch-read-ready 0 driver-read-dispatch-read-authorize-denied 1 driver-read-dispatch-requested 1 driver-read-dispatch-granted 0 ' +
        'driver-read-dispatch-denied 1 driver-read-dispatch-policy-grant 0 driver-read-dispatch-dispatch-queued 0 driver-read-dispatch-queue-depth 0 driver-read-dispatch-issue-authority 0 ' +
        'driver-read-dispatch-dma-authority 0 driver-read-dispatch-media-read-authority 0 driver-read-dispatch-write-authority 0 driver-read-dispatch-commit-authority 0 ' +
        'driver-read-dispatch-block-endpoint 0 driver-read-dispatch-block-cap-minted 0 driver-read-dispatch-fs-minted 0 driver-read-dispatch-mmio-written 0 driver-read-dispatch-port-programmed 0 ' +
        'driver-read-dispatch-published 0 driver-read-dispatch-command-issued 0 driver-read-dispatch-dma 0 driver-read-dispatch-armed 0 driver-read-dispatch-media-read 0 ' +
        'driver-read-dispatch-media-written 0 driver-read-dispatch-buffer-unchanged 1 driver-read-dispatch-staged 0 driver-read-dispatch-denials 1 driver-read-dispatch-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 57'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadDispatchPattern -Message "x64 BIOS AHCI denied read-dispatch proof was not observed."
    $biosDriverReadQueuePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-queue 0xFFFFFFFF driver-read-queue 0xFFFFFFFF driver-read-queue-state 2 driver-read-queue-flags 0x7FFFFC01 ' +
        'driver-read-queue-token 0x00000000 driver-read-queue-read-dispatch-token 0xFFFFFFFF driver-read-queue-cap 0xFFFFFFFF driver-read-queue-owner 0x00001006 ' +
        'driver-read-queue-owner-bound 0 driver-read-queue-query-only 0 driver-read-queue-port 0xFFFFFFFF driver-read-queue-kind 0 driver-read-queue-op 0 ' +
        'driver-read-queue-lba 0 driver-read-queue-blocks 0 driver-read-queue-read-bytes 0 driver-read-queue-page-bytes 0 driver-read-queue-checksum 0x00000000 ' +
        'driver-read-queue-zeroed 0 driver-read-queue-read-ready 0 driver-read-queue-read-dispatch-denied 1 driver-read-queue-requested 1 driver-read-queue-granted 0 ' +
        'driver-read-queue-denied 1 driver-read-queue-policy-grant 0 driver-read-queue-queue-inserted 0 driver-read-queue-queue-depth 0 driver-read-queue-worker-wake 0 ' +
        'driver-read-queue-issue-authority 0 driver-read-queue-dma-authority 0 driver-read-queue-media-read-authority 0 driver-read-queue-write-authority 0 driver-read-queue-commit-authority 0 ' +
        'driver-read-queue-block-endpoint 0 driver-read-queue-block-cap-minted 0 driver-read-queue-fs-minted 0 driver-read-queue-mmio-written 0 driver-read-queue-port-programmed 0 ' +
        'driver-read-queue-published 0 driver-read-queue-command-issued 0 driver-read-queue-dma 0 driver-read-queue-armed 0 driver-read-queue-media-read 0 ' +
        'driver-read-queue-media-written 0 driver-read-queue-buffer-unchanged 1 driver-read-queue-staged 0 driver-read-queue-denials 1 driver-read-queue-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 58'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadQueuePattern -Message "x64 BIOS AHCI denied read-queue proof was not observed."
    $biosDriverReadWorkerPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-worker 0xFFFFFFFF driver-read-worker 0xFFFFFFFF driver-read-worker-state 2 driver-read-worker-flags 0x7FFFFC01 ' +
        'driver-read-worker-token 0x00000000 driver-read-worker-read-queue-token 0xFFFFFFFF driver-read-worker-cap 0xFFFFFFFF driver-read-worker-owner 0x00001006 ' +
        'driver-read-worker-owner-bound 0 driver-read-worker-query-only 0 driver-read-worker-port 0xFFFFFFFF driver-read-worker-kind 0 driver-read-worker-op 0 ' +
        'driver-read-worker-lba 0 driver-read-worker-blocks 0 driver-read-worker-read-bytes 0 driver-read-worker-page-bytes 0 driver-read-worker-checksum 0x00000000 ' +
        'driver-read-worker-zeroed 0 driver-read-worker-read-ready 0 driver-read-worker-read-queue-denied 1 driver-read-worker-requested 1 driver-read-worker-granted 0 ' +
        'driver-read-worker-denied 1 driver-read-worker-policy-grant 0 driver-read-worker-queue-inserted 0 driver-read-worker-queue-depth 0 driver-read-worker-worker-wake 0 ' +
        'driver-read-worker-worker-dequeued 0 driver-read-worker-issue-authority 0 driver-read-worker-dma-authority 0 driver-read-worker-media-read-authority 0 ' +
        'driver-read-worker-write-authority 0 driver-read-worker-commit-authority 0 driver-read-worker-block-endpoint 0 driver-read-worker-block-cap-minted 0 ' +
        'driver-read-worker-fs-minted 0 driver-read-worker-mmio-written 0 driver-read-worker-port-programmed 0 driver-read-worker-published 0 ' +
        'driver-read-worker-command-issued 0 driver-read-worker-dma 0 driver-read-worker-armed 0 driver-read-worker-media-read 0 driver-read-worker-media-written 0 ' +
        'driver-read-worker-buffer-unchanged 1 driver-read-worker-staged 0 driver-read-worker-denials 1 driver-read-worker-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 59'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadWorkerPattern -Message "x64 BIOS AHCI denied read-worker proof was not observed."
    $biosDriverReadSchedulePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-schedule 0xFFFFFFFF driver-read-schedule 0xFFFFFFFF driver-read-schedule-state 2 driver-read-schedule-flags 0x7FFFFC01 ' +
        'driver-read-schedule-token 0x00000000 driver-read-schedule-read-worker-token 0xFFFFFFFF driver-read-schedule-cap 0xFFFFFFFF driver-read-schedule-owner 0x00001006 ' +
        'driver-read-schedule-owner-bound 0 driver-read-schedule-query-only 0 driver-read-schedule-port 0xFFFFFFFF driver-read-schedule-kind 0 driver-read-schedule-op 0 ' +
        'driver-read-schedule-lba 0 driver-read-schedule-blocks 0 driver-read-schedule-read-bytes 0 driver-read-schedule-page-bytes 0 driver-read-schedule-checksum 0x00000000 ' +
        'driver-read-schedule-zeroed 0 driver-read-schedule-read-ready 0 driver-read-schedule-read-worker-denied 1 driver-read-schedule-requested 1 driver-read-schedule-granted 0 ' +
        'driver-read-schedule-denied 1 driver-read-schedule-policy-grant 0 driver-read-schedule-queue-inserted 0 driver-read-schedule-queue-depth 0 driver-read-schedule-worker-wake 0 ' +
        'driver-read-schedule-worker-dequeued 0 driver-read-schedule-worker-runnable 0 driver-read-schedule-worker-scheduled 0 driver-read-schedule-issue-authority 0 ' +
        'driver-read-schedule-dma-authority 0 driver-read-schedule-media-read-authority 0 driver-read-schedule-write-authority 0 driver-read-schedule-commit-authority 0 ' +
        'driver-read-schedule-block-endpoint 0 driver-read-schedule-block-cap-minted 0 driver-read-schedule-fs-minted 0 driver-read-schedule-mmio-written 0 ' +
        'driver-read-schedule-port-programmed 0 driver-read-schedule-published 0 driver-read-schedule-command-issued 0 driver-read-schedule-dma 0 driver-read-schedule-armed 0 ' +
        'driver-read-schedule-media-read 0 driver-read-schedule-media-written 0 driver-read-schedule-buffer-unchanged 1 driver-read-schedule-staged 0 ' +
        'driver-read-schedule-denials 1 driver-read-schedule-unavailable 1 map-requests 0 .* queries 359 denials 60'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadSchedulePattern -Message "x64 BIOS AHCI denied read-schedule proof was not observed."
    $biosDriverReadRunPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-run 0xFFFFFFFF driver-read-run 0xFFFFFFFF driver-read-run-state 2 driver-read-run-flags 0x7FFFFC01 ' +
        'driver-read-run-token 0x00000000 driver-read-run-read-schedule-token 0xFFFFFFFF driver-read-run-cap 0xFFFFFFFF driver-read-run-owner 0x00001006 ' +
        'driver-read-run-owner-bound 0 driver-read-run-query-only 0 driver-read-run-port 0xFFFFFFFF driver-read-run-kind 0 driver-read-run-op 0 ' +
        'driver-read-run-lba 0 driver-read-run-blocks 0 driver-read-run-read-bytes 0 driver-read-run-page-bytes 0 driver-read-run-checksum 0x00000000 ' +
        'driver-read-run-zeroed 0 driver-read-run-read-ready 0 driver-read-run-read-schedule-denied 1 driver-read-run-requested 1 driver-read-run-granted 0 ' +
        'driver-read-run-denied 1 driver-read-run-policy-grant 0 driver-read-run-queue-inserted 0 driver-read-run-queue-depth 0 driver-read-run-worker-wake 0 ' +
        'driver-read-run-worker-dequeued 0 driver-read-run-worker-runnable 0 driver-read-run-worker-scheduled 0 driver-read-run-worker-run 0 driver-read-run-worker-executed 0 ' +
        'driver-read-run-issue-authority 0 driver-read-run-dma-authority 0 driver-read-run-media-read-authority 0 driver-read-run-write-authority 0 driver-read-run-commit-authority 0 ' +
        'driver-read-run-block-endpoint 0 driver-read-run-block-cap-minted 0 driver-read-run-fs-minted 0 driver-read-run-mmio-written 0 driver-read-run-port-programmed 0 ' +
        'driver-read-run-published 0 driver-read-run-command-issued 0 driver-read-run-dma 0 driver-read-run-armed 0 driver-read-run-media-read 0 ' +
        'driver-read-run-media-written 0 driver-read-run-buffer-unchanged 1 driver-read-run-staged 0 driver-read-run-denials 1 driver-read-run-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 61'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadRunPattern -Message "x64 BIOS AHCI denied read-run proof was not observed."
    $biosDriverReadBodyPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-body 0xFFFFFFFF driver-read-body 0xFFFFFFFF driver-read-body-state 2 driver-read-body-flags 0x7FFFFC01 ' +
        'driver-read-body-token 0x00000000 driver-read-body-read-run-token 0xFFFFFFFF driver-read-body-cap 0xFFFFFFFF driver-read-body-owner 0x00001006 ' +
        'driver-read-body-owner-bound 0 driver-read-body-query-only 0 driver-read-body-port 0xFFFFFFFF driver-read-body-kind 0 driver-read-body-op 0 ' +
        'driver-read-body-lba 0 driver-read-body-blocks 0 driver-read-body-read-bytes 0 driver-read-body-page-bytes 0 driver-read-body-checksum 0x00000000 ' +
        'driver-read-body-zeroed 0 driver-read-body-read-ready 0 driver-read-body-read-run-denied 1 driver-read-body-requested 1 driver-read-body-granted 0 ' +
        'driver-read-body-denied 1 driver-read-body-policy-grant 0 driver-read-body-queue-inserted 0 driver-read-body-queue-depth 0 driver-read-body-worker-wake 0 ' +
        'driver-read-body-worker-dequeued 0 driver-read-body-worker-runnable 0 driver-read-body-worker-scheduled 0 driver-read-body-worker-run 0 driver-read-body-worker-executed 0 ' +
        'driver-read-body-body-entered 0 driver-read-body-body-completed 0 driver-read-body-issue-authority 0 driver-read-body-dma-authority 0 ' +
        'driver-read-body-media-read-authority 0 driver-read-body-write-authority 0 driver-read-body-commit-authority 0 driver-read-body-block-endpoint 0 ' +
        'driver-read-body-block-cap-minted 0 driver-read-body-fs-minted 0 driver-read-body-mmio-written 0 driver-read-body-port-programmed 0 ' +
        'driver-read-body-published 0 driver-read-body-command-issued 0 driver-read-body-dma 0 driver-read-body-armed 0 driver-read-body-media-read 0 ' +
        'driver-read-body-media-written 0 driver-read-body-buffer-unchanged 1 driver-read-body-staged 0 driver-read-body-denials 1 driver-read-body-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 62'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadBodyPattern -Message "x64 BIOS AHCI denied read-body proof was not observed."
    $biosDriverReadIssuePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-issue 0xFFFFFFFF driver-read-issue 0xFFFFFFFF driver-read-issue-state 2 driver-read-issue-flags 0x7FFFFC01 ' +
        'driver-read-issue-token 0x00000000 driver-read-issue-read-body-token 0xFFFFFFFF driver-read-issue-cap 0xFFFFFFFF driver-read-issue-owner 0x00001006 ' +
        'driver-read-issue-owner-bound 0 driver-read-issue-query-only 0 driver-read-issue-port 0xFFFFFFFF driver-read-issue-kind 0 driver-read-issue-op 0 ' +
        'driver-read-issue-lba 0 driver-read-issue-blocks 0 driver-read-issue-read-bytes 0 driver-read-issue-page-bytes 0 driver-read-issue-checksum 0x00000000 ' +
        'driver-read-issue-zeroed 0 driver-read-issue-read-ready 0 driver-read-issue-read-body-denied 1 driver-read-issue-requested 1 driver-read-issue-granted 0 ' +
        'driver-read-issue-denied 1 driver-read-issue-policy-grant 0 driver-read-issue-queue-inserted 0 driver-read-issue-queue-depth 0 driver-read-issue-worker-wake 0 ' +
        'driver-read-issue-worker-dequeued 0 driver-read-issue-worker-runnable 0 driver-read-issue-worker-scheduled 0 driver-read-issue-worker-run 0 driver-read-issue-worker-executed 0 ' +
        'driver-read-issue-issue-entered 0 driver-read-issue-issue-completed 0 driver-read-issue-issue-authority 0 driver-read-issue-dma-authority 0 ' +
        'driver-read-issue-media-read-authority 0 driver-read-issue-write-authority 0 driver-read-issue-commit-authority 0 driver-read-issue-block-endpoint 0 ' +
        'driver-read-issue-block-cap-minted 0 driver-read-issue-fs-minted 0 driver-read-issue-mmio-written 0 driver-read-issue-port-programmed 0 ' +
        'driver-read-issue-published 0 driver-read-issue-command-issued 0 driver-read-issue-dma 0 driver-read-issue-armed 0 driver-read-issue-media-read 0 ' +
        'driver-read-issue-media-written 0 driver-read-issue-buffer-unchanged 1 driver-read-issue-staged 0 driver-read-issue-denials 1 driver-read-issue-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 63'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadIssuePattern -Message "x64 BIOS AHCI denied read-issue proof was not observed."
    $biosDriverReadDmaPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-dma 0xFFFFFFFF driver-read-dma 0xFFFFFFFF driver-read-dma-state 2 driver-read-dma-flags 0x7FFFFC01 ' +
        'driver-read-dma-token 0x00000000 driver-read-dma-read-issue-token 0xFFFFFFFF driver-read-dma-cap 0xFFFFFFFF driver-read-dma-owner 0x00001006 ' +
        'driver-read-dma-owner-bound 0 driver-read-dma-query-only 0 driver-read-dma-port 0xFFFFFFFF driver-read-dma-kind 0 driver-read-dma-op 0 ' +
        'driver-read-dma-lba 0 driver-read-dma-blocks 0 driver-read-dma-read-bytes 0 driver-read-dma-page-bytes 0 driver-read-dma-checksum 0x00000000 ' +
        'driver-read-dma-zeroed 0 driver-read-dma-read-ready 0 driver-read-dma-read-issue-denied 1 driver-read-dma-requested 1 driver-read-dma-granted 0 ' +
        'driver-read-dma-denied 1 driver-read-dma-policy-grant 0 driver-read-dma-bytes-available 0 driver-read-dma-window-cap 0xFFFFFFFF ' +
        'driver-read-dma-window-open 0 driver-read-dma-entered 0 driver-read-dma-completed 0 driver-read-dma-issue-authority 0 ' +
        'driver-read-dma-dma-authority 0 driver-read-dma-media-read-authority 0 driver-read-dma-write-authority 0 driver-read-dma-commit-authority 0 ' +
        'driver-read-dma-block-endpoint 0 driver-read-dma-block-cap-minted 0 driver-read-dma-fs-minted 0 driver-read-dma-mmio-written 0 ' +
        'driver-read-dma-port-programmed 0 driver-read-dma-published 0 driver-read-dma-command-issued 0 driver-read-dma-dma 0 driver-read-dma-armed 0 ' +
        'driver-read-dma-media-read 0 driver-read-dma-media-written 0 driver-read-dma-buffer-unchanged 1 driver-read-dma-staged 0 ' +
        'driver-read-dma-denials 1 driver-read-dma-unavailable 1 map-requests 0 .* queries 359 denials 64'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadDmaPattern -Message "x64 BIOS AHCI denied read-DMA proof was not observed."
    $biosDriverReadIrqPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-irq 0xFFFFFFFF drs-irq 0xFFFFFFFF drs-irq-state 2 drs-irq-flags 0x7FFFFC01 ' +
        'drs-irq-token 0x00000000 drs-irq-dma-token 0xFFFFFFFF drs-irq-cap 0xFFFFFFFF drs-irq-owner 0x00001006 ' +
        'drs-irq-owner-bound 0 drs-irq-qonly 0 drs-irq-port 0xFFFFFFFF drs-irq-kind 0 drs-irq-op 0 ' +
        'drs-irq-lba 0 drs-irq-blocks 0 drs-irq-read-bytes 0 drs-irq-page-bytes 0 drs-irq-checksum 0x00000000 ' +
        'drs-irq-zeroed 0 drs-irq-ready 0 drs-irq-dma-denied 1 drs-irq-requested 1 drs-irq-granted 0 ' +
        'drs-irq-denied 1 drs-irq-policy-grant 0 drs-irq-bytes 0 drs-irq-wait 0 drs-irq-fired 0 ' +
        'drs-irq-cstatus 0 drs-irq-cbytes 0 drs-irq-cchecksum 0x00000000 drs-irq-issue-auth 0 ' +
        'drs-irq-dma-auth 0 drs-irq-read-auth 0 drs-irq-write-auth 0 drs-irq-commit-auth 0 ' +
        'drs-irq-block-endpoint 0 drs-irq-block-cap 0 drs-irq-fs-minted 0 drs-irq-mmio-written 0 ' +
        'drs-irq-port-programmed 0 drs-irq-published 0 drs-irq-command-issued 0 drs-irq-dma 0 drs-irq-armed 0 ' +
        'drs-irq-media-read 0 drs-irq-media-written 0 drs-irq-buffer 1 drs-irq-staged 0 ' +
        'drs-irq-denials 1 drs-irq-unavailable 1 map-requests 0 .* queries 359 denials 65'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadIrqPattern -Message "x64 BIOS AHCI denied read-IRQ proof was not observed."
    $biosDriverReadStatusPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-status 0xFFFFFFFF drs-status 0xFFFFFFFF drs-status-state 2 drs-status-flags 0x7FFFFC01 ' +
        'drs-status-token 0x00000000 drs-status-irq-token 0xFFFFFFFF drs-status-cap 0xFFFFFFFF drs-status-owner 0x00001006 ' +
        'drs-status-owner-bound 0 drs-status-qonly 0 drs-status-port 0xFFFFFFFF drs-status-kind 0 drs-status-op 0 ' +
        'drs-status-lba 0 drs-status-blocks 0 drs-status-read-bytes 0 drs-status-page-bytes 0 drs-status-checksum 0x00000000 ' +
        'drs-status-zeroed 0 drs-status-ready 0 drs-status-irq-denied 1 drs-status-requested 1 drs-status-granted 0 ' +
        'drs-status-denied 1 drs-status-policy-grant 0 drs-status-bytes 0 drs-status-poll 0 drs-status-sready 0 ' +
        'drs-status-pxis 0x00000000 drs-status-ci 0x00000000 drs-status-tfd 0x00000000 drs-status-serr 0x00000000 drs-status-irq-clear 0 ' +
        'drs-status-cstatus 0 drs-status-cbytes 0 drs-status-cchecksum 0x00000000 drs-status-issue-auth 0 ' +
        'drs-status-dma-auth 0 drs-status-read-auth 0 drs-status-write-auth 0 drs-status-commit-auth 0 ' +
        'drs-status-block-endpoint 0 drs-status-block-cap 0 drs-status-fs-minted 0 drs-status-mmio-written 0 ' +
        'drs-status-port-programmed 0 drs-status-published 0 drs-status-command-issued 0 drs-status-dma 0 drs-status-armed 0 ' +
        'drs-status-media-read 0 drs-status-media-written 0 drs-status-buffer 1 drs-status-staged 0 ' +
        'drs-status-denials 1 drs-status-unavailable 1 map-requests 0 .* queries 359 denials 66'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusPattern -Message "x64 BIOS AHCI denied read-status proof was not observed."
    $biosDriverReadStatusResultPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-result 0xFFFFFFFF drs-result 0xFFFFFFFF drs-result-state 2 drs-result-flags 0x7FFFFC01 ' +
        'drs-result-token 0x00000000 drs-result-status-token 0xFFFFFFFF drs-result-cap 0xFFFFFFFF drs-result-owner 0x00001006 ' +
        'drs-result-owner-bound 0 drs-result-qonly 0 drs-result-port 0xFFFFFFFF drs-result-kind 0 drs-result-op 0 ' +
        'drs-result-lba 0 drs-result-blocks 0 drs-result-read-bytes 0 drs-result-page-bytes 0 drs-result-checksum 0x00000000 ' +
        'drs-result-zeroed 0 drs-result-ready 0 drs-result-status-denied 1 drs-result-requested 1 drs-result-granted 0 ' +
        'drs-result-denied 1 drs-result-policy-grant 0 drs-result-bytes 0 drs-result-rstatus 0 drs-result-rbytes 0 ' +
        'drs-result-rchecksum 0x00000000 drs-result-issue-auth 0 drs-result-dma-auth 0 drs-result-read-auth 0 ' +
        'drs-result-write-auth 0 drs-result-commit-auth 0 drs-result-block-endpoint 0 drs-result-block-cap 0 ' +
        'drs-result-fs-minted 0 drs-result-mmio-written 0 drs-result-port-programmed 0 drs-result-published 0 ' +
        'drs-result-command-issued 0 drs-result-dma 0 drs-result-armed 0 drs-result-media-read 0 drs-result-media-written 0 ' +
        'drs-result-buffer 1 drs-result-staged 0 drs-result-denials 1 drs-result-unavailable 1 map-requests 0 .* queries 359 denials 67'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusResultPattern -Message "x64 BIOS AHCI denied read-status-result proof was not observed."
    $biosDriverReadStatusSamplePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-sample 0xFFFFFFFF drs-sample 0xFFFFFFFF drs-sample-state 2 drs-sample-flags 0x7FF00C01 ' +
        'drs-sample-token 0x00000000 drs-sample-result-token 0xFFFFFFFF drs-sample-cap 0xFFFFFFFF drs-sample-owner 0x00001006 ' +
        'drs-sample-owner-bound 0 drs-sample-qonly 0 drs-sample-port 0xFFFFFFFF drs-sample-kind 0 drs-sample-op 0 ' +
        'drs-sample-lba 0 drs-sample-blocks 0 drs-sample-read-bytes 0 drs-sample-page-bytes 0 drs-sample-checksum 0x00000000 ' +
        'drs-sample-zeroed 0 drs-sample-ready 0 drs-sample-result-denied 1 drs-sample-requested 1 ' +
        'drs-sample-granted 0 drs-sample-denied 1 drs-sample-policy-grant 0 drs-sample-bytes 0 ' +
        'drs-sample-pxis 0x00000000 drs-sample-ci 0x00000000 drs-sample-tfd 0x00000000 drs-sample-serr 0x00000000 ' +
        'drs-sample-tfd-ready 0 drs-sample-ci-idle 0 drs-sample-serr-clear 0 drs-sample-irq-clear 0 ' +
        'drs-sample-rstatus 0 drs-sample-rbytes 0 drs-sample-rchecksum 0x00000000 drs-sample-issue-auth 0 ' +
        'drs-sample-dma-auth 0 drs-sample-read-auth 0 drs-sample-write-auth 0 drs-sample-commit-auth 0 ' +
        'drs-sample-block-endpoint 0 drs-sample-block-cap 0 drs-sample-fs-minted 0 drs-sample-mmio-written 0 ' +
        'drs-sample-port-programmed 0 drs-sample-published 0 drs-sample-command-issued 0 drs-sample-dma 0 drs-sample-armed 0 ' +
        'drs-sample-media-read 0 drs-sample-media-written 0 drs-sample-buffer 1 drs-sample-staged 0 ' +
        'drs-sample-denials 1 drs-sample-unavailable 1 map-requests 0 .* queries 359 denials 68'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusSamplePattern -Message "x64 BIOS AHCI read-only status-sample unavailable proof was not observed."
    $biosDriverReadStatusClearPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-clear 0xFFFFFFFF drs-clear 0xFFFFFFFF drs-clear-state 2 drs-clear-flags 0x7FF03801 ' +
        'drs-clear-token 0x00000000 drs-clear-sample-token 0xFFFFFFFF drs-clear-cap 0xFFFFFFFF drs-clear-owner 0x00001006 ' +
        'drs-clear-owner-bound 0 drs-clear-qonly 0 drs-clear-port 0xFFFFFFFF drs-clear-kind 0 drs-clear-op 0 ' +
        'drs-clear-lba 0 drs-clear-blocks 0 drs-clear-read-bytes 0 drs-clear-page-bytes 0 drs-clear-checksum 0x00000000 ' +
        'drs-clear-zeroed 0 drs-clear-ready 0 drs-clear-sample-ready 0 drs-clear-sample-bound 0 ' +
        'drs-clear-requested 1 drs-clear-granted 0 drs-clear-denied 1 drs-clear-policy-grant 0 ' +
        'drs-clear-bytes 0 drs-clear-pxis-b 0x00000000 drs-clear-pxis-a 0x00000000 drs-clear-pxis-same 0 ' +
        'drs-clear-ci 0x00000000 drs-clear-tfd 0x00000000 drs-clear-serr 0x00000000 drs-clear-tfd-ready 0 ' +
        'drs-clear-ci-idle 0 drs-clear-serr-clear 0 drs-clear-clear-requested 1 drs-clear-clear-granted 0 ' +
        'drs-clear-clear-denied 1 drs-clear-clear-value 0x00000000 drs-clear-irq-clear 0 drs-clear-rstatus 0 ' +
        'drs-clear-rbytes 0 drs-clear-rchecksum 0x00000000 drs-clear-issue-auth 0 drs-clear-dma-auth 0 ' +
        'drs-clear-read-auth 0 drs-clear-write-auth 0 drs-clear-commit-auth 0 drs-clear-block-endpoint 0 ' +
        'drs-clear-block-cap 0 drs-clear-fs-minted 0 drs-clear-mmio-written 0 drs-clear-port-programmed 0 ' +
        'drs-clear-published 0 drs-clear-command-issued 0 drs-clear-dma 0 drs-clear-armed 0 drs-clear-media-read 0 ' +
        'drs-clear-media-written 0 drs-clear-buffer 1 drs-clear-staged 0 drs-clear-denials 1 ' +
        'drs-clear-unavailable 1 map-requests 0 .* queries 359 denials 69'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusClearPattern -Message "x64 BIOS AHCI denied status-clear unavailable proof was not observed."
    $biosDriverReadStatusClearResultPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-clear-result 0xFFFFFFFF drs-clear-result 0xFFFFFFFF ' +
        'drs-clear-result-state 2 drs-clear-result-flags 0x7FF0E401 .* ' +
        'drs-clear-result-owner 0x00001006 drs-clear-result-owner-bound 0 drs-clear-result-qonly 0 ' +
        'drs-clear-result-port 0xFFFFFFFF drs-clear-result-kind 0 drs-clear-result-op 0 .* ' +
        'drs-clear-result-clear-denied 1 drs-clear-result-requested 1 drs-clear-result-granted 0 ' +
        'drs-clear-result-denied 1 drs-clear-result-policy-grant 0 drs-clear-result-bytes 0 .* ' +
        'drs-clear-result-clear-requested 0 drs-clear-result-clear-granted 0 drs-clear-result-pxis-clear-denied 0 .* ' +
        'drs-clear-result-result-requested 1 drs-clear-result-result-granted 0 drs-clear-result-result-denied 1 ' +
        'drs-clear-result-result-status 0 drs-clear-result-result-bytes 0 drs-clear-result-result-checksum 0x00000000 .* ' +
        'drs-clear-result-block-endpoint 0 drs-clear-result-block-cap 0 drs-clear-result-fs-minted 0 ' +
        'drs-clear-result-mmio-written 0 drs-clear-result-port-programmed 0 drs-clear-result-published 0 ' +
        'drs-clear-result-command-issued 0 drs-clear-result-dma 0 drs-clear-result-armed 0 ' +
        'drs-clear-result-media-read 0 drs-clear-result-media-written 0 drs-clear-result-buffer 1 ' +
        'drs-clear-result-staged 0 drs-clear-result-denials 1 drs-clear-result-unavailable 1 map-requests 0 .* queries 359 denials 70'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusClearResultPattern -Message "x64 BIOS AHCI denied status-clear-result unavailable proof was not observed."
    $biosDriverReadStatusResamplePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-resample 0xFFFFFFFF drs-resample 0xFFFFFFFF ' +
        'drs-resample-state 2 drs-resample-flags 0x7FC01C01 ' +
        'drs-resample-token 0x00000000 drs-resample-clear-result-token 0xFFFFFFFF ' +
        'drs-resample-cap 0xFFFFFFFF drs-resample-owner 0x00001006 drs-resample-owner-bound 0 ' +
        'drs-resample-qonly 0 drs-resample-port 0xFFFFFFFF drs-resample-kind 0 drs-resample-op 0 ' +
        'drs-resample-lba 0 drs-resample-blocks 0 drs-resample-read-bytes 0 drs-resample-page-bytes 0 ' +
        'drs-resample-checksum 0x00000000 drs-resample-zeroed 0 drs-resample-ready 0 ' +
        'drs-resample-clear-result-denied 1 drs-resample-requested 1 drs-resample-granted 0 ' +
        'drs-resample-denied 1 drs-resample-policy-grant 0 drs-resample-bytes 0 ' +
        'drs-resample-pxis-b 0x00000000 drs-resample-pxis-a 0x00000000 drs-resample-pxis-same 0 ' +
        'drs-resample-ci 0x00000000 drs-resample-tfd 0x00000000 drs-resample-serr 0x00000000 ' +
        'drs-resample-tfd-ready 0 drs-resample-ci-idle 0 drs-resample-serr-clear 0 drs-resample-irq-clear 0 ' +
        'drs-resample-result-status 0 drs-resample-result-bytes 0 drs-resample-result-checksum 0x00000000 ' +
        'drs-resample-issue-auth 0 drs-resample-dma-auth 0 drs-resample-read-auth 0 ' +
        'drs-resample-write-auth 0 drs-resample-commit-auth 0 drs-resample-block-endpoint 0 ' +
        'drs-resample-block-cap 0 drs-resample-fs-minted 0 drs-resample-mmio-written 0 ' +
        'drs-resample-port-programmed 0 drs-resample-published 0 drs-resample-command-issued 0 ' +
        'drs-resample-dma 0 drs-resample-armed 0 drs-resample-media-read 0 ' +
        'drs-resample-media-written 0 drs-resample-buffer 1 drs-resample-staged 0 ' +
        'drs-resample-denials 1 drs-resample-unavailable 1 map-requests 0 .* queries 359 denials 71'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusResamplePattern -Message "x64 BIOS AHCI read-status resample unavailable proof was not observed."
    $biosDriverReadStatusStablePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-stable 0xFFFFFFFF drs-stable 0xFFFFFFFF ' +
        'drs-stable-state 2 drs-stable-flags 0x7FE03401 drs-stable-token 0x00000000 ' +
        'drs-stable-resample-token 0xFFFFFFFF drs-stable-cap 0xFFFFFFFF drs-stable-owner 0x00001006 ' +
        'drs-stable-owner-bound 0 drs-stable-qonly 0 drs-stable-port 0xFFFFFFFF drs-stable-kind 0 drs-stable-op 0 ' +
        'drs-stable-lba 0 drs-stable-blocks 0 drs-stable-read-bytes 0 drs-stable-page-bytes 0 ' +
        'drs-stable-checksum 0x00000000 drs-stable-zeroed 0 drs-stable-ready 0 ' +
        'drs-stable-clear-result-denied 1 drs-stable-resample-read-only 0 drs-stable-requested 1 drs-stable-granted 0 ' +
        'drs-stable-denied 1 drs-stable-policy-grant 0 drs-stable-bytes 0 ' +
        'drs-stable-pxis-b 0x00000000 drs-stable-pxis-a 0x00000000 drs-stable-pxis-stable 0 ' +
        'drs-stable-ci-b 0x00000000 drs-stable-ci-a 0x00000000 drs-stable-ci-stable 0 ' +
        'drs-stable-tfd-b 0x00000000 drs-stable-tfd-a 0x00000000 drs-stable-tfd-stable 0 ' +
        'drs-stable-serr-b 0x00000000 drs-stable-serr-a 0x00000000 drs-stable-serr-stable 0 ' +
        'drs-stable-tfd-ready 0 drs-stable-ci-idle 0 drs-stable-serr-clear 0 drs-stable-irq-clear 0 ' +
        'drs-stable-result-status 0 drs-stable-result-bytes 0 drs-stable-result-checksum 0x00000000 ' +
        'drs-stable-issue-auth 0 drs-stable-dma-auth 0 drs-stable-read-auth 0 ' +
        'drs-stable-write-auth 0 drs-stable-commit-auth 0 drs-stable-block-endpoint 0 ' +
        'drs-stable-block-cap 0 drs-stable-fs-minted 0 drs-stable-mmio-written 0 ' +
        'drs-stable-port-programmed 0 drs-stable-published 0 drs-stable-command-issued 0 ' +
        'drs-stable-dma 0 drs-stable-armed 0 drs-stable-media-read 0 ' +
        'drs-stable-media-written 0 drs-stable-buffer 1 drs-stable-staged 0 ' +
        'drs-stable-denials 1 drs-stable-unavailable 1 map-requests 0 .* queries 359 denials 72'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusStablePattern -Message "x64 BIOS AHCI read-status stable unavailable proof was not observed."
    $biosDriverReadStatusGuardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-guard 0xFFFFFFFF drs-guard 0xFFFFFFFF ' +
        'drs-guard-state 2 drs-guard-flags 0x7FFC0001 drs-guard-token 0x00000000 ' +
        'drs-guard-stable-token 0xFFFFFFFF drs-guard-cap 0xFFFFFFFF drs-guard-owner 0x00001006 ' +
        'drs-guard-owner-bound 0 drs-guard-qonly 0 drs-guard-port 0xFFFFFFFF drs-guard-kind 0 drs-guard-op 0 ' +
        'drs-guard-lba 0 drs-guard-blocks 0 drs-guard-read-bytes 0 drs-guard-page-bytes 0 ' +
        'drs-guard-checksum 0x00000000 drs-guard-zeroed 0 drs-guard-ready 0 ' +
        'drs-guard-pxis-b 0x00000000 drs-guard-pxis-a 0x00000000 drs-guard-pxis-stable 0 ' +
        'drs-guard-ci-b 0x00000000 drs-guard-ci-a 0x00000000 drs-guard-ci-stable 0 ' +
        'drs-guard-tfd-b 0x00000000 drs-guard-tfd-a 0x00000000 drs-guard-tfd-stable 0 ' +
        'drs-guard-serr-b 0x00000000 drs-guard-serr-a 0x00000000 drs-guard-serr-stable 0 ' +
        'drs-guard-tfd-ready 0 drs-guard-ci-idle 0 drs-guard-serr-clear 0 ' +
        'drs-guard-requested 1 drs-guard-issue-ok 0 drs-guard-issue-denied 1 ' +
        'drs-guard-dma-ok 0 drs-guard-dma-denied 1 drs-guard-read-auth 0 ' +
        'drs-guard-read-denied 1 drs-guard-write-auth 0 drs-guard-write-denied 1 ' +
        'drs-guard-commit-auth 0 drs-guard-commit-denied 1 drs-guard-irq-clear 0 ' +
        'drs-guard-result-status 0 drs-guard-result-bytes 0 drs-guard-result-checksum 0x00000000 ' +
        'drs-guard-block-endpoint 0 drs-guard-block-cap 0 drs-guard-fs-minted 0 ' +
        'drs-guard-mmio-written 0 drs-guard-port-programmed 0 drs-guard-published 0 ' +
        'drs-guard-command-issued 0 drs-guard-dma 0 drs-guard-armed 0 ' +
        'drs-guard-media-read 0 drs-guard-media-written 0 drs-guard-buffer 1 ' +
        'drs-guard-staged 0 drs-guard-denials 1 drs-guard-unavailable 1 map-requests 0 .* queries 359 denials 73'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusGuardPattern -Message "x64 BIOS AHCI read-status guard unavailable proof was not observed."
    $biosDriverReadStatusBufferPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-buffer 0xFFFFFFFF drs-buffer 0xFFFFFFFF ' +
        'drs-buffer-state 2 drs-buffer-flags 0x7FFFF801 drs-buffer-token 0x00000000 ' +
        'drs-buffer-guard-token 0xFFFFFFFF drs-buffer-cap 0xFFFFFFFF drs-buffer-owner 0x00001006 ' +
        'drs-buffer-owner-bound 0 drs-buffer-qonly 0 drs-buffer-port 0xFFFFFFFF drs-buffer-kind 0 ' +
        'drs-buffer-op 0 drs-buffer-lba 0 drs-buffer-blocks 0 drs-buffer-read-bytes 0 ' +
        'drs-buffer-page-bytes 0 drs-buffer-checksum 0x00000000 drs-buffer-zeroed 0 drs-buffer-ready 0 ' +
        'drs-buffer-view-requested 1 drs-buffer-view-granted 0 drs-buffer-view-denied 1 ' +
        'drs-buffer-result-status 0 drs-buffer-result-bytes 0 drs-buffer-result-checksum 0x00000000 ' +
        'drs-buffer-read-auth 0 drs-buffer-write-auth 0 drs-buffer-commit-auth 0 ' +
        'drs-buffer-block-endpoint 0 drs-buffer-block-cap 0 drs-buffer-fs-minted 0 ' +
        'drs-buffer-mmio-written 0 drs-buffer-port-programmed 0 drs-buffer-published 0 ' +
        'drs-buffer-command-issued 0 drs-buffer-dma 0 drs-buffer-armed 0 ' +
        'drs-buffer-media-read 0 drs-buffer-media-written 0 drs-buffer-buffer 1 ' +
        'drs-buffer-staged 0 drs-buffer-denials 1 drs-buffer-unavailable 1 .* map-requests 0 .* queries 359 denials 75'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusBufferPattern -Message "x64 BIOS AHCI read-status buffer unavailable proof was not observed."
    $biosDriverReadStatusExportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-export 0xFFFFFFFF drs-export 0xFFFFFFFF ' +
        'drs-export-state 2 drs-export-flags 0x7FFFF001 drs-export-token 0x00000000 ' +
        'drs-export-buffer-token 0xFFFFFFFF drs-export-cap 0xFFFFFFFF drs-export-owner 0x00001006 ' +
        'drs-export-owner-bound 0 drs-export-qonly 0 drs-export-port 0xFFFFFFFF drs-export-kind 0 ' +
        'drs-export-op 0 drs-export-lba 0 drs-export-blocks 0 drs-export-read-bytes 0 ' +
        'drs-export-checksum 0x00000000 ' +
        'drs-export-sealed 0 drs-export-requested 1 drs-export-granted 0 drs-export-denied 1 ' +
        'drs-export-user-copy 0x00000000 drs-export-authority 0x00000000 drs-export-effects 0x00000000 ' +
        'drs-export-buffer 1 drs-export-staged 0 ' +
        'drs-export-denials 1 drs-export-unavailable 1 map-requests 0 .* queries 359 denials 75'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusExportPattern -Message "x64 BIOS AHCI read-status export unavailable proof was not observed."
    $biosDriverReadStatusReportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-report 0xFFFFFFFF drs-report 0xFFFFFFFF ' +
        'drs-report-state 2 drs-report-flags 0x7FFFFE01 ' +
        'drs-report-owner 0x00001006 drs-report-qonly 0 ' +
        'drs-report-checksum 0x00000000 drs-report-export-denied 0 ' +
        'drs-report-report 0x00000000 drs-report-user-copy 0x00000000 ' +
        'drs-report-authority 0x00000000 drs-report-effects 0x00000000 ' +
        'drs-report-buffer 1 drs-report-staged 0 ' +
        'drs-report-denials 1 drs-report-unavailable 1 map-requests 0 .* queries 359 denials 76'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusReportPattern -Message "x64 BIOS AHCI read-status report unavailable proof was not observed."
    $biosDriverReadStatusReceiptPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-receipt 0xFFFFFFFF drs-receipt 0xFFFFFFFF ' +
        'drs-receipt-state 2 drs-receipt-flags 0x7FFFFF01 ' +
        'drs-receipt-owner 0x00001006 drs-receipt-qonly 0 ' +
        'drs-receipt-checksum 0x00000000 drs-receipt-report-denied 0 ' +
        'drs-receipt-receipt 0x00000000 drs-receipt-user-copy 0x00000000 ' +
        'drs-receipt-authority 0x00000000 drs-receipt-effects 0x00000000 ' +
        'drs-receipt-buffer 1 drs-receipt-staged 0 ' +
        'drs-receipt-denials 1 drs-receipt-unavailable 1 map-requests 0 .* queries 359 denials 77'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusReceiptPattern -Message "x64 BIOS AHCI read-status receipt unavailable proof was not observed."
    $biosDriverReadStatusAckPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-ack 0xFFFFFFFF drs-ack 0xFFFFFFFF ' +
        'drs-ack-state 2 drs-ack-flags 0x7FFFFF01 ' +
        'drs-ack-owner 0x00001006 drs-ack-qonly 0 ' +
        'drs-ack-checksum 0x00000000 drs-ack-receipt-denied 0 ' +
        'drs-ack-ack 0x00000000 drs-ack-user-copy 0x00000000 ' +
        'drs-ack-authority 0x00000000 drs-ack-effects 0x00000000 ' +
        'drs-ack-buffer 1 drs-ack-staged 0 ' +
        'drs-ack-denials 1 drs-ack-unavailable 1 map-requests 0 .* queries 359 denials 78'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusAckPattern -Message "x64 BIOS AHCI read-status ack unavailable proof was not observed."
    $biosDriverReadStatusClosePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-close 0xFFFFFFFF drs-close 0xFFFFFFFF ' +
        'drs-close-state 2 drs-close-flags 0x7FFFFF01 ' +
        'drs-close-owner 0x00001006 drs-close-qonly 0 ' +
        'drs-close-checksum 0x00000000 drs-close-ack-denied 0 ' +
        'drs-close-close 0x00000000 drs-close-user-copy 0x00000000 ' +
        'drs-close-authority 0x00000000 drs-close-effects 0x00000000 ' +
        'drs-close-buffer 1 drs-close-staged 0 ' +
        'drs-close-denials 1 drs-close-unavailable 1 map-requests 0 .* queries 359 denials 79'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusClosePattern -Message "x64 BIOS AHCI read-status close unavailable proof was not observed."
    $biosDriverReadStatusSealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-seal 0xFFFFFFFF drs-seal 0xFFFFFFFF ' +
        'drs-seal-state 2 drs-seal-flags 0x7FFFFF01 ' +
        'drs-seal-owner 0x00001006 drs-seal-qonly 0 ' +
        'drs-seal-checksum 0x00000000 drs-seal-close-denied 0 ' +
        'drs-seal-seal 0x00000000 drs-seal-user-copy 0x00000000 ' +
        'drs-seal-authority 0x00000000 drs-seal-effects 0x00000000 ' +
        'drs-seal-buffer 1 drs-seal-staged 0 ' +
        'drs-seal-denials 1 drs-seal-unavailable 1 map-requests 0 .* queries 359 denials 80'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusSealPattern -Message "x64 BIOS AHCI read-status seal unavailable proof was not observed."
    $biosDriverReadStatusUnsealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-unseal 0xFFFFFFFF drs-unseal 0xFFFFFFFF ' +
        'drs-unseal-state 2 drs-unseal-flags 0x7FFFFF01 ' +
        'drs-unseal-owner 0x00001006 drs-unseal-qonly 0 ' +
        'drs-unseal-checksum 0x00000000 drs-unseal-seal-denied 0 ' +
        'drs-unseal-unseal 0x00000000 drs-unseal-user-copy 0x00000000 ' +
        'drs-unseal-authority 0x00000000 drs-unseal-effects 0x00000000 ' +
        'drs-unseal-buffer 1 drs-unseal-staged 0 ' +
        'drs-unseal-denials 1 drs-unseal-unavailable 1 map-requests 0 .* queries 359 denials 81'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusUnsealPattern -Message "x64 BIOS AHCI read-status unseal unavailable proof was not observed."
    $biosDriverReadStatusDiscardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-discard 0xFFFFFFFF drs-discard 0xFFFFFFFF ' +
        'drs-discard-state 2 drs-discard-flags 0x7FFFFF01 ' +
        'drs-discard-owner 0x00001006 drs-discard-qonly 0 ' +
        'drs-discard-checksum 0x00000000 drs-discard-unseal-denied 0 ' +
        'drs-discard-discard 0x00000000 drs-discard-user-copy 0x00000000 ' +
        'drs-discard-authority 0x00000000 drs-discard-effects 0x00000000 ' +
        'drs-discard-buffer 1 drs-discard-staged 0 ' +
        'drs-discard-denials 1 drs-discard-unavailable 1 map-requests 0 .* queries 359 denials 82'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusDiscardPattern -Message "x64 BIOS AHCI read-status discard unavailable proof was not observed."
    $biosDriverReadStatusFinalizePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-final 0xFFFFFFFF drs-final 0xFFFFFFFF ' +
        'drs-final-state 2 drs-final-flags 0x7FFFFF01 ' +
        'drs-final-owner 0x00001006 drs-final-qonly 0 ' +
        'drs-final-checksum 0x00000000 drs-final-discard-denied 0 ' +
        'drs-final-finish 0x00000000 drs-final-user-copy 0x00000000 ' +
        'drs-final-authority 0x00000000 drs-final-effects 0x00000000 ' +
        'drs-final-buffer 1 drs-final-staged 0 ' +
        'drs-final-denials 1 drs-final-unavailable 1 map-requests 0 .* queries 359 denials 83'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusFinalizePattern -Message "x64 BIOS AHCI read-status finalize unavailable proof was not observed."
    $biosDriverReadStatusAuthorizePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-authz 0xFFFFFFFF drs-authz 0xFFFFFFFF ' +
        'drs-authz-state 2 drs-authz-flags 0x7FFFFF01 ' +
        'drs-authz-owner 0x00001006 drs-authz-qonly 0 ' +
        'drs-authz-checksum 0x00000000 drs-authz-final-denied 0 ' +
        'drs-authz-grant 0x00000000 drs-authz-user-copy 0x00000000 ' +
        'drs-authz-authority 0x00000000 drs-authz-effects 0x00000000 ' +
        'drs-authz-buffer 1 drs-authz-staged 0 ' +
        'drs-authz-denials 1 drs-authz-unavailable 1 map-requests 0 .* queries 359 denials 84'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusAuthorizePattern -Message "x64 BIOS AHCI read-status authorize unavailable proof was not observed."
    $biosDriverReadStatusDispatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dispatch 0xFFFFFFFF drs-dispatch 0xFFFFFFFF ' +
        'drs-dispatch-state 2 drs-dispatch-flags 0x7FFFFF01 ' +
        'drs-dispatch-owner 0x00001006 drs-dispatch-qonly 0 ' +
        'drs-dispatch-checksum 0x00000000 drs-dispatch-authz-denied 0 ' +
        'drs-dispatch-safety 0x00000000 drs-dispatch-buffer 1 drs-dispatch-staged 0 ' +
        'drs-dispatch-denials 1 drs-dispatch-unavailable 1 map-requests 0 .* queries 359 denials 85'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusDispatchPattern -Message "x64 BIOS AHCI read-status dispatch unavailable proof was not observed."
    $biosDriverReadStatusQueuePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-queue 0xFFFFFFFF drs-queue 0xFFFFFFFF ' +
        'drs-queue-state 2 drs-queue-flags 0x7FFFFF01 ' +
        'drs-queue-owner 0x00001006 drs-queue-qonly 0 ' +
        'drs-queue-checksum 0x00000000 drs-queue-dispatch-denied 0 ' +
        'drs-queue-safety 0x00000000 drs-queue-depth 0 drs-queue-admit 0 ' +
        'drs-queue-worker 0 drs-queue-runnable 0 drs-queue-schedule 0 drs-queue-buffer 1 ' +
        'drs-queue-staged 0 drs-queue-denials 1 drs-queue-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 86'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusQueuePattern -Message "x64 BIOS AHCI read-status queue unavailable proof was not observed."
    $biosDriverReadStatusWorkerPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-w 0xFFFFFFFF drs-w 0xFFFFFFFF ' +
        'drs-w-state 2 drs-w-flags 0x7FFFFF01 ' +
        'drs-w-owner 0x00001006 drs-w-qonly 0 ' +
        'drs-w-checksum 0x00000000 drs-w-queue-denied 0 ' +
        'drs-w-safety 0x00000000 drs-w-dequeue 0 drs-w-admit 0 ' +
        'drs-w-wake 0 drs-w-runnable 0 drs-w-sched 0 drs-w-run 0 drs-w-exec 0 ' +
        'drs-w-buffer 1 drs-w-staged 0 ' +
        'drs-w-denials 1 drs-w-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 87'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusWorkerPattern -Message "x64 BIOS AHCI read-status worker unavailable proof was not observed."
    $biosDriverReadStatusReadAuthorityPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-rauth 0xFFFFFFFF drs-rauth 0xFFFFFFFF ' +
        'drs-rauth-state 2 drs-rauth-flags 0x7FFFF901 ' +
        'drs-rauth-owner 0x00001006 drs-rauth-qonly 0 ' +
        'drs-rauth-checksum 0x00000000 drs-rauth-worker-denied 0 ' +
        'drs-rauth-policy 0 drs-rauth-read 0 drs-rauth-issue 0 ' +
        'drs-rauth-dma-auth 0 drs-rauth-media-auth 0 drs-rauth-write 0 drs-rauth-commit 0 ' +
        'drs-rauth-block-endpoint 0 drs-rauth-block-cap 0 drs-rauth-fs-minted 0 ' +
        'drs-rauth-safety 0x00000000 drs-rauth-dequeue 0 drs-rauth-admit 0 ' +
        'drs-rauth-wake 0 drs-rauth-runnable 0 drs-rauth-sched 0 drs-rauth-run 0 ' +
        'drs-rauth-exec 0 drs-rauth-buffer 1 drs-rauth-staged 0 ' +
        'drs-rauth-denials 1 drs-rauth-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 88'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusReadAuthorityPattern -Message "x64 BIOS AHCI read-authority unavailable proof was not observed."
    $biosDriverReadStatusDescriptorPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-desc 0xFFFFFFFF drs-desc 0xFFFFFFFF ' +
        'drs-desc-state 2 drs-desc-flags 0x7FFFF901 ' +
        'drs-desc-owner 0x00001006 drs-desc-qonly 0 ' +
        'drs-desc-checksum 0x00000000 drs-desc-rauth 0 drs-desc-shaped 0 drs-desc-read 0 ' +
        'drs-desc-port 0xFFFFFFFF drs-desc-kind 0 drs-desc-op 0 drs-desc-lba 0 ' +
        'drs-desc-blocks 0 drs-desc-read-bytes 0 drs-desc-page-bytes 0 ' +
        'drs-desc-slot 0 drs-desc-header 0 drs-desc-table 0 drs-desc-cfis 0 ' +
        'drs-desc-prdt 0 drs-desc-prdt-bytes 0 drs-desc-packet 0 ' +
        'drs-desc-opcode 0x00000000 drs-desc-packet-op 0x00000000 drs-desc-transfer 0 ' +
        'drs-desc-issue 0 drs-desc-dma-auth 0 drs-desc-media-auth 0 drs-desc-write 0 ' +
        'drs-desc-commit 0 drs-desc-block-endpoint 0 drs-desc-block-cap 0 ' +
        'drs-desc-fs-minted 0 drs-desc-safety 0x00000000 drs-desc-dequeue 0 ' +
        'drs-desc-admit 0 drs-desc-wake 0 drs-desc-runnable 0 drs-desc-sched 0 ' +
        'drs-desc-run 0 drs-desc-exec 0 drs-desc-buffer 1 drs-desc-staged 0 ' +
        'drs-desc-denials 1 drs-desc-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 89'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusDescriptorPattern -Message "x64 BIOS AHCI read-status descriptor unavailable proof was not observed."
    $biosDriverReadStatusCommandTablePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-ctab 0xFFFFFFFF drs-ctab 0xFFFFFFFF ' +
        'drs-ctab-state 2 drs-ctab-flags 0x7FFFF901 ' +
        'drs-ctab-owner 0x00001006 drs-ctab-qonly 0 ' +
        'drs-ctab-checksum 0x00000000 drs-ctab-desc 0 drs-ctab-mat 0 drs-ctab-ready 0 ' +
        'drs-ctab-port 0xFFFFFFFF drs-ctab-kind 0 drs-ctab-op 0 drs-ctab-lba 0 ' +
        'drs-ctab-blocks 0 drs-ctab-read-bytes 0 drs-ctab-page-bytes 0 ' +
        'drs-ctab-before 0x00000000 drs-ctab-after 0x00000000 drs-ctab-changed 0 ' +
        'drs-ctab-hdr 0x00000000 drs-ctab-cfis 0x00000000 drs-ctab-packet 0x00000000 ' +
        'drs-ctab-dbc 0 drs-ctab-written 0 drs-ctab-issue 0 drs-ctab-dma-auth 0 ' +
        'drs-ctab-media-auth 0 drs-ctab-write 0 drs-ctab-commit 0 ' +
        'drs-ctab-block-endpoint 0 drs-ctab-block-cap 0 drs-ctab-fs-minted 0 ' +
        'drs-ctab-safety 0x00000000 drs-ctab-mmio 0 drs-ctab-portw 0 ' +
        'drs-ctab-cmd 0 drs-ctab-dma 0 drs-ctab-media 0 drs-ctab-buffer 1 ' +
        'drs-ctab-staged 0 drs-ctab-denials 1 drs-ctab-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 90'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusCommandTablePattern -Message "x64 BIOS AHCI read-status command-table unavailable proof was not observed."
    $biosDriverReadStatusCommandIssuePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-issue 0xFFFFFFFF drs-issue 0xFFFFFFFF ' +
        'drs-issue-state 2 drs-issue-flags 0x7FFFF901 ' +
        'drs-issue-owner 0x00001006 drs-issue-qonly 0 drs-issue-checksum 0x00000000 ' +
        'drs-issue-ctab 0 drs-issue-ready 0 drs-issue-request 1 drs-issue-grant 0 drs-issue-denied 0 ' +
        'drs-issue-port 0xFFFFFFFF drs-issue-kind 0 drs-issue-op 0 drs-issue-lba 0 ' +
        'drs-issue-blocks 0 drs-issue-read-bytes 0 drs-issue-page-bytes 0 ' +
        'drs-issue-ci 0x00000000 drs-issue-mask 0x00000000 drs-issue-slot-idle 0 ' +
        'drs-issue-tfd 0 drs-issue-serr 0 drs-issue-table-check 0x00000000 ' +
        'drs-issue-expected 0x00000000 drs-issue-match 0 ' +
        'drs-issue-issue-auth 0 drs-issue-dma-auth 0 drs-issue-media-auth 0 ' +
        'drs-issue-write 0 drs-issue-commit 0 drs-issue-block-endpoint 0 ' +
        'drs-issue-block-cap 0 drs-issue-fs-minted 0 drs-issue-safety 0x00000000 ' +
        'drs-issue-mmio 0 drs-issue-portw 0 drs-issue-cmd 0 drs-issue-dma 0 ' +
        'drs-issue-media 0 drs-issue-buffer 1 drs-issue-staged 0 ' +
        'drs-issue-denials 1 drs-issue-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 91'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusCommandIssuePattern -Message "x64 BIOS AHCI read-status command-issue unavailable proof was not observed."
    $biosDriverReadStatusIssueGrantPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-grant 0xFFFFFFFF drs-grant 0xFFFFFFFF ' +
        'drs-grant-state 2 drs-grant-flags 0x7FFFF901 ' +
        'drs-grant-owner 0x00001006 drs-grant-qonly 0 drs-grant-checksum 0x00000000 ' +
        'drs-grant-issue 0 drs-grant-ready 0 drs-grant-request 1 drs-grant-grant 0 drs-grant-denied 0 ' +
        'drs-grant-port 0xFFFFFFFF drs-grant-kind 0 drs-grant-op 0 drs-grant-lba 0 ' +
        'drs-grant-blocks 0 drs-grant-read-bytes 0 drs-grant-page-bytes 0 ' +
        'drs-grant-ci 0x00000000 drs-grant-mask 0x00000000 drs-grant-slot-idle 0 ' +
        'drs-grant-tfd 0 drs-grant-serr 0 drs-grant-table-check 0x00000000 ' +
        'drs-grant-expected 0x00000000 drs-grant-match 0 ' +
        'drs-grant-issue-auth 0 drs-grant-dma-auth 0 drs-grant-media-auth 0 ' +
        'drs-grant-write 0 drs-grant-commit 0 drs-grant-block-endpoint 0 ' +
        'drs-grant-block-cap 0 drs-grant-fs-minted 0 drs-grant-safety 0x00000000 ' +
        'drs-grant-mmio 0 drs-grant-portw 0 drs-grant-cmd 0 drs-grant-dma 0 ' +
        'drs-grant-media 0 drs-grant-buffer 1 drs-grant-staged 0 ' +
        'drs-grant-denials 1 drs-grant-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 92'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusIssueGrantPattern -Message "x64 BIOS AHCI read-status issue-grant unavailable proof was not observed."
    $biosDriverReadStatusArmPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-arm 0xFFFFFFFF drs-arm 0xFFFFFFFF ' +
        'drs-arm-state 2 drs-arm-flags 0x7FFFF901 ' +
        'drs-arm-owner 0x00001006 drs-arm-qonly 0 drs-arm-checksum 0x00000000 ' +
        'drs-arm-grant 0 drs-arm-ready 0 drs-arm-request 1 drs-arm-arm 0 drs-arm-denied 0 ' +
        'drs-arm-port 0xFFFFFFFF drs-arm-kind 0 drs-arm-op 0 drs-arm-lba 0 ' +
        'drs-arm-blocks 0 drs-arm-read-bytes 0 drs-arm-page-bytes 0 ' +
        'drs-arm-ci 0x00000000 drs-arm-mask 0x00000000 drs-arm-slot-idle 0 ' +
        'drs-arm-tfd 0 drs-arm-serr 0 drs-arm-table-check 0x00000000 ' +
        'drs-arm-expected 0x00000000 drs-arm-match 0 ' +
        'drs-arm-issue-auth 0 drs-arm-dma-auth 0 drs-arm-media-auth 0 ' +
        'drs-arm-write 0 drs-arm-commit 0 drs-arm-block-endpoint 0 ' +
        'drs-arm-block-cap 0 drs-arm-fs-minted 0 drs-arm-safety 0x00000000 ' +
        'drs-arm-mmio 0 drs-arm-portw 0 drs-arm-cmd 0 drs-arm-dma 0 ' +
        'drs-arm-media 0 drs-arm-buffer 1 drs-arm-staged 0 ' +
        'drs-arm-denials 1 drs-arm-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 93'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusArmPattern -Message "x64 BIOS AHCI read-status arm unavailable proof was not observed."
    $biosDriverReadStatusExecPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-exec 0xFFFFFFFF drs-exec 0xFFFFFFFF ' +
        'drs-exec-state 2 drs-exec-flags 0x7FFFF901 ' +
        'drs-exec-owner 0x00001006 drs-exec-qonly 0 drs-exec-checksum 0x00000000 ' +
        'drs-exec-arm 0 drs-exec-ready 0 drs-exec-request 1 drs-exec-exec 0 drs-exec-denied 0 ' +
        'drs-exec-port 0xFFFFFFFF drs-exec-kind 0 drs-exec-op 0 drs-exec-lba 0 ' +
        'drs-exec-blocks 0 drs-exec-read-bytes 0 drs-exec-page-bytes 0 ' +
        'drs-exec-ci 0x00000000 drs-exec-mask 0x00000000 drs-exec-slot-idle 0 ' +
        'drs-exec-tfd 0 drs-exec-serr 0 drs-exec-table-check 0x00000000 ' +
        'drs-exec-expected 0x00000000 drs-exec-match 0 ' +
        'drs-exec-issue-auth 0 drs-exec-dma-auth 0 drs-exec-media-auth 0 ' +
        'drs-exec-write 0 drs-exec-commit 0 drs-exec-block-endpoint 0 ' +
        'drs-exec-block-cap 0 drs-exec-fs-minted 0 drs-exec-safety 0x00000000 ' +
        'drs-exec-mmio 0 drs-exec-portw 0 drs-exec-cmd 0 drs-exec-dma 0 ' +
        'drs-exec-media 0 drs-exec-buffer 1 drs-exec-staged 0 ' +
        'drs-exec-denials 1 drs-exec-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 94'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusExecPattern -Message "x64 BIOS AHCI read-status exec unavailable proof was not observed."
    $biosDriverReadStatusDmaPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dma 0xFFFFFFFF drs-dma 0xFFFFFFFF ' +
        'drs-dma-state 2 drs-dma-flags 0x7FFFF901 ' +
        'drs-dma-owner 0x00001006 drs-dma-qonly 0 drs-dma-checksum 0x00000000 ' +
        'drs-dma-exec 0 drs-dma-ready 0 drs-dma-request 1 drs-dma-grant 0 drs-dma-denied 0 ' +
        'drs-dma-port 0xFFFFFFFF drs-dma-kind 0 drs-dma-op 0 drs-dma-lba 0 ' +
        'drs-dma-blocks 0 drs-dma-read-bytes 0 drs-dma-page-bytes 0 ' +
        'drs-dma-ci 0x00000000 drs-dma-mask 0x00000000 drs-dma-slot-idle 0 ' +
        'drs-dma-tfd 0 drs-dma-serr 0 drs-dma-table-check 0x00000000 ' +
        'drs-dma-expected 0x00000000 drs-dma-match 0 ' +
        'drs-dma-issue-auth 0 drs-dma-dma-auth 0 drs-dma-media-auth 0 ' +
        'drs-dma-write 0 drs-dma-commit 0 drs-dma-block-endpoint 0 ' +
        'drs-dma-block-cap 0 drs-dma-fs-minted 0 drs-dma-safety 0x00000000 ' +
        'drs-dma-mmio 0 drs-dma-portw 0 drs-dma-cmd 0 drs-dma-dma 0 ' +
        'drs-dma-media 0 drs-dma-buffer 1 drs-dma-staged 0 ' +
        'drs-dma-denials 1 drs-dma-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 95'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusDmaPattern -Message "x64 BIOS AHCI read-status DMA unavailable proof was not observed."
    $biosDriverReadStatusMmioPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-mmio 0xFFFFFFFF stale-drs-mmio 0xFFFFFFFF drs-mmio 0xFFFFFFFF ' +
        'drs-mmio-state 2 drs-mmio-flags 0x7FF00101 ' +
        'drs-mmio-owner 0x00001006 drs-mmio-qonly 0 drs-mmio-checksum 0x00000000 ' +
        'drs-mmio-dma-bound 0 drs-mmio-ready 0 drs-mmio-request 1 drs-mmio-grant 0 drs-mmio-denied 0 ' +
        'drs-mmio-port 0x00000000 drs-mmio-kind 0 drs-mmio-op 0 drs-mmio-lba 0 ' +
        'drs-mmio-blocks 0 drs-mmio-read-bytes 0 drs-mmio-page-bytes 0 ' +
        'drs-mmio-ci 0x00000000 drs-mmio-mask 0x00000000 drs-mmio-slot-idle 0 ' +
        'drs-mmio-tfd 0 drs-mmio-serr 0 drs-mmio-table-check 0x00000000 ' +
        'drs-mmio-expected 0x00000000 drs-mmio-match 0 ' +
        'drs-mmio-reg 0x00000000 drs-mmio-value 0x00000000 ' +
        'drs-mmio-pxis-b 0x00000000 drs-mmio-pxis-a 0x00000000 drs-mmio-pxis-same 0 ' +
        'drs-mmio-rollback-required 0 drs-mmio-rollback-done 0 drs-mmio-teardown 0 drs-mmio-stale-denied 0 ' +
        'drs-mmio-issue-auth 0 drs-mmio-dma-auth 0 drs-mmio-media-auth 0 ' +
        'drs-mmio-write 0 drs-mmio-commit 0 drs-mmio-block-endpoint 0 ' +
        'drs-mmio-block-cap 0 drs-mmio-fs-minted 0 drs-mmio-safety 0x00000000 ' +
        'drs-mmio-mmio 0 drs-mmio-portw 0 drs-mmio-cmd 0 drs-mmio-dma 0 ' +
        'drs-mmio-media 0 drs-mmio-media-write 0 drs-mmio-buffer 1 drs-mmio-staged 0 ' +
        'drs-mmio-denials 1 drs-mmio-unavailable 1 ' +
        'map-requests 0 .* queries 359 denials 96'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusMmioPattern -Message "x64 BIOS AHCI read-status MMIO unavailable proof was not observed."
    $biosDriverReadStatusDmaWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dwin 0xFFFFFFFF stale-drs-dwin 0xFFFFFFFF drs-dwin 0xFFFFFFFF ' +
        'drs-dwin-state 2 drs-dwin-flags 0x7FC80001 ' +
        'drs-dwin-owner 0x00001006 drs-dwin-qonly 0 drs-dwin-checksum 0x00000000 ' +
        'drs-dwin-mmio-bound 0 drs-dwin-ready 0 drs-dwin-request 1 drs-dwin-grant 0 drs-dwin-denied 0 ' +
        'drs-dwin-port 0x00000000 drs-dwin-kind 0 drs-dwin-op 0 drs-dwin-lba 0 ' +
        'drs-dwin-blocks 0 drs-dwin-read-bytes 0 drs-dwin-page-bytes 0 ' +
        'drs-dwin-ci 0x00000000 drs-dwin-mask 0x00000000 drs-dwin-slot-idle 0 ' +
        'drs-dwin-tfd 0 drs-dwin-serr 0 drs-dwin-table-check 0x00000000 ' +
        'drs-dwin-expected 0x00000000 drs-dwin-match 0 ' +
        'drs-dwin-page-low 0x00000000 drs-dwin-page-high 0x00000000 ' +
        'drs-dwin-bounce-low 0x00000000 drs-dwin-bounce-high 0x00000000 ' +
        'drs-dwin-bounce-bytes 0 drs-dwin-offset 0 drs-dwin-range-end 0 ' +
        'drs-dwin-single-page 0 drs-dwin-broker 0 drs-dwin-bounds 0 ' +
        'drs-dwin-confined 0 drs-dwin-below4g 0 drs-dwin-iommu 0 drs-dwin-identity 0 ' +
        'drs-dwin-non-user 0 drs-dwin-alias-safe 0 drs-dwin-opened 0 drs-dwin-closed 0 drs-dwin-active 0 ' +
        'drs-dwin-revoke-required 1 drs-dwin-revoke-done 0 drs-dwin-stale-denied 0 ' +
        'drs-dwin-issue-auth 0 drs-dwin-dma-auth 0 drs-dwin-media-auth 0 ' +
        'drs-dwin-write 0 drs-dwin-commit 0 drs-dwin-block-endpoint 0 ' +
        'drs-dwin-block-cap 0 drs-dwin-fs-minted 0 drs-dwin-safety 0x00000000 ' +
        'drs-dwin-mmio 0 drs-dwin-portw 0 drs-dwin-cmd 0 drs-dwin-dma 0 ' +
        'drs-dwin-media 0 drs-dwin-media-write 0 drs-dwin-buffer 1 drs-dwin-staged 0 ' +
        'drs-dwin-denials 1 drs-dwin-unavailable 1 ' +
        '.* map-requests 0 .* queries 359 denials 97'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusDmaWindowPattern -Message "x64 BIOS AHCI read-status DMA-window unavailable proof was not observed."
    $biosDriverReadStatusReadPattern = (
        '\[x64\] mmio planner service 9 .* drs-read 0xFFFFFFFF drs-read-state 2 drs-read-flags 0x801F0001 ' +
        'drs-read-owner 0x00001006 drs-read-qonly 0 drs-read-dwin-bound 0 drs-read-ready 0 drs-read-request 1 ' +
        'drs-read-issued 0 drs-read-completed 0 drs-read-bytes 0 drs-read-checksum 0x00000000 drs-read-error 1 ' +
        'drs-read-port 0xFFFFFFFF drs-read-kind 0 drs-read-op 0 drs-read-lba 0 drs-read-blocks 0 ' +
        'drs-read-page-low 0x00000000 drs-read-bounce-low 0x00000000 drs-read-table-before 0x00000000 drs-read-table-after 0x00000000 ' +
        'drs-read-prdbc 0 drs-read-ci-b 0x00000000 drs-read-ci-a 0x00000000 drs-read-tfd-b 0x00000000 drs-read-tfd-a 0x00000000 ' +
        'drs-read-polls 0 drs-read-mmio 0 drs-read-portw 0 drs-read-dma 0 drs-read-media 0 drs-read-write 0 ' +
        'drs-read-commit 0 drs-read-block-endpoint 0 drs-read-fs-minted 0 drs-read-active 0 drs-read-staged 0 ' +
        'drs-read-denials 0 drs-read-unavailable 1 '
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusReadPattern -Message "x64 BIOS AHCI drs-read unavailable proof was not observed."
    $biosDriverReadStatusBlockPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-block 0xFFFFFFFF stale-drs-block 0xFFFFFFFF ' +
        'drs-block 0xFFFFFFFF drs-block-state 2 drs-block-flags 0x800E0001 ' +
        'drs-block-owner 0x00001006 drs-block-read-bound 0 drs-block-endpoint 0 drs-block-cap-minted 0 ' +
        'drs-block-cap 0xFFFFFFFF drs-block-read-only 0 drs-block-delegated-cap 0 drs-block-read-routed 0 ' +
        'drs-block-bytes 0 drs-block-checksum 0x00000000 drs-block-read-checksum 0x00000000 ' +
        'drs-block-checksum-match 0 drs-block-wrong-owner 0 drs-block-stale-denied 0 ' +
        'drs-block-write 0 drs-block-commit 0 drs-block-fs-minted 0 drs-block-lba 0 drs-block-blocks 0 ' +
        'drs-block-buffer 0 drs-block-staged 0 drs-block-denials 1 drs-block-unavailable 1 ' +
        'drs-fs 0xFFFFFFFF drs-fs-state 2 drs-fs-flags 0x8001C001 drs-fs-owner 0x00001006 ' +
        'drs-fs-block-bound 0 drs-fs-block-cap 0xFFFFFFFF drs-fs-block-read-only 0 drs-fs-block-route 0 ' +
        'drs-fs-pvd 0 drs-fs-pvd-checksum 0x00000000 drs-fs-root-lba 0 drs-fs-root-bytes 0 ' +
        'drs-fs-root-read 0 drs-fs-located 0 drs-fs-file-lba 0 drs-fs-read-bytes 0 ' +
        'drs-fs-checksum 0x00000000 drs-fs-expected-checksum 0x(?!00000000)[0-9A-F]{8} drs-fs-content-match 0 ' +
        'drs-fs-write 0 drs-fs-commit 0 drs-fs-fs-minted 0 drs-fs-staged 0 drs-fs-denials 0 drs-fs-unavailable 1 ' +
        'drs-fs-user 0xFFFFFFFF drs-fs-user-path /APPS/LS.APP drs-fs-user-state 2 drs-fs-user-flags 0x801C0001 ' +
        'drs-fs-user-owner 0x00001006 drs-fs-user-user-owner 0x00000201 drs-fs-user-fs-bound 0 ' +
        'drs-fs-user-cap-minted 0 drs-fs-user-delegated 0 drs-fs-user-read-routed 0 drs-fs-user-root-read 0 ' +
        'drs-fs-user-apps-lba 0 drs-fs-user-apps-bytes 0 drs-fs-user-file-lba 0 drs-fs-user-bytes 0 ' +
        'drs-fs-user-checksum 0x00000000 drs-fs-user-expected-checksum 0xFDB1F751 drs-fs-user-content-match 0 ' +
        'drs-fs-user-wrong-owner 0 drs-fs-user-stale 0 drs-fs-user-write 0 drs-fs-user-commit 0 ' +
        'drs-fs-user-additional-fs-caps 0 drs-fs-user-user-buffer 0 drs-fs-user-staged 0 ' +
        'drs-fs-user-denials 0 drs-fs-user-unavailable 1 ' +
        'drs-fs-shell 0xFFFFFFFF drs-fs-shell-state 2 drs-fs-shell-flags 0x8000E001 ' +
        'drs-fs-shell-owner 0x00001006 drs-fs-shell-user-owner 0x00000201 drs-fs-shell-fs-user-bound 0 ' +
        'drs-fs-shell-delegated 0 drs-fs-shell-descriptors-read 0 drs-fs-shell-descriptors-parsed 0 ' +
        'drs-fs-shell-scan-dynamic 0 ' +
        'drs-fs-shell-ls-dispatched 0 drs-fs-shell-cat-dispatched 0 drs-fs-shell-stat-dispatched 0 ' +
        'drs-fs-shell-ramfs-route 0 drs-fs-shell-iso-route 0 drs-fs-shell-write 0 drs-fs-shell-commit 0 ' +
        'drs-fs-shell-additional-fs-caps 0 drs-fs-shell-staged 0 drs-fs-shell-denials 0 drs-fs-shell-unavailable 1 '
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusBlockPattern -Message "x64 BIOS AHCI drs-block unavailable proof was not observed."
    $biosDriverReadStatusLoadPattern = (
        '\[x64\] drs-load 0xFFFFFFFF drs-load-state 2 drs-load-flags 0x80000701 ' +
        'drs-load-owner 0x00001006 drs-load-user-owner 0x00000201 drs-load-fs-shell-bound 0 ' +
        'drs-load-binary-read 0 drs-load-checksum-verified 0 drs-load-mapped 0 ' +
        'drs-load-launched 0 drs-load-ls-completed 0 drs-load-source unavailable drs-load-bytes 0 ' +
        'drs-load-checksum 0x00000000 drs-load-expected-checksum 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-load-mapped-bytes 0 drs-load-entry-rip 0x00000000 drs-load-exit-result 0x00000000 ' +
        'drs-load-ls-bytes 0 drs-load-write 0 drs-load-commit 0 drs-load-additional-fs-caps 0 ' +
        'drs-load-staged 0 drs-load-denials 0 drs-load-unavailable 1'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusLoadPattern -Message "x64 BIOS AHCI drs-load unavailable proof was not observed."
    $biosDriverReadStatusLoadFullPattern = (
        '\[x64\] drs-load-full 0xFFFFFFFF drs-load-full-state 2 drs-load-full-flags 0x80001C01 ' +
        'drs-load-full-owner 0x00001006 drs-load-full-user-owner 0x00000201 ' +
        'drs-load-full-load-bound 0 drs-load-full-fs-shell-bound 0 drs-load-full-binaries 0 ' +
        'drs-load-full-verified 0 drs-load-full-registered 0 drs-load-full-cat 0 ' +
        'drs-load-full-mkdir 0 drs-load-full-write 0 drs-load-full-rename 0 ' +
        'drs-load-full-move 0 drs-load-full-source unavailable ' +
        'drs-load-full-exit-result 0x00000000 drs-load-full-exit-aux 0 ' +
        'drs-load-full-write-escalation 0 drs-load-full-commit 0 drs-load-full-additional-fs-caps 0 ' +
        'drs-load-full-staged 0 drs-load-full-denials 0 drs-load-full-unavailable 1'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $biosDriverReadStatusLoadFullPattern -Message "x64 BIOS AHCI drs-load-full unavailable proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] fs readme bytes [1-9][0-9]* stat-bytes [1-9][0-9]* text LimitlessOS bootstrap ramfs' -Message "x64 RAMFS read/stat proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] fs internal apps bytes [1-9][0-9]* internal-listing .*ECHO\.APP.*LS\.APP.*CAT\.APP' -Message "x64 RAMFS internal directory listing proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] fs note bytes 32 text x64 capability filesystem online live [1-9][0-9]* opens 3 creates 1 lists 1 reads 2 writes 1 stats 1 revokes 1 denials 3 stale-denials 0' -Message "x64 RAMFS create/write/revoke telemetry proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] user fs probe attempts 2 exits 2 result 0x46535233 recorded 0x46535233 expected 0x46535233 read-bytes 32 rip 0x0000000041000180 rsp 0x0000000040020000 cs 0x0000000000000033 ss 0x000000000000002B' -Message "x64 ring-3 filesystem syscall proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] brokered keyboard read cap 0x[0-9A-F]+ result 1 first-byte 0x[0-9A-F]+ pending-before [1-9][0-9]* pending-after [0-9]+ keyboard-reads 1 keyboard-read-bytes 1' -Message "x64 brokered keyboard read syscall proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] user second-page probe attempts [1-9][0-9]* exits [1-9][0-9]* result 0x32504752 recorded 0x32504752 expected 0x32504752 mapped-bytes 16384 page-count 4 offset 0x00001ED0 note-path-bytes 11 note-bytes 15 fs-creates 2 fs-writes 2 fs-reads [1-9][0-9]* display-pixels [0-9]+ display-draws [0-9]+ display-denials 0 display-unavailable [0-9]+ display-token 0x[0-9A-F]+ display-available [0-1] display-text-writes [0-9]+ display-text-bytes [0-9]+ display-clears [0-9]+ display-console-writes [0-9]+ display-console-bytes [0-9]+ display-console-line-clears [0-9]+ display-console-wraps [0-9]+ display-console-scrolls [0-9]+ rip 0x0000000041001ED0 rsp 0x0000000040020000 cs 0x0000000000000033 ss 0x000000000000002B' -Message "x64 ring-3 second-page create/write/read/display proof was not observed."
}
else {
    Assert-OutputContains -Lines $outputLines -Pattern 'LimitlessOS x86_64 UEFI scaffold' -Message "x64 UEFI scaffold banner was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] firmware boot active' -Message "x64 UEFI firmware handoff was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] package archive v2' -Message "x64 UEFI package archive summary was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] services 11 console 4 ramfs 5 input 6 display 7 block 8 hardware 9 network 10' -Message "x64 UEFI service namespace summary was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] gop framebuffer mode [0-9]+ max [1-9][0-9]* [1-9][0-9]*x[1-9][0-9]* ppsl [1-9][0-9]* format (rgb|bgr) base 0x[0-9A-F]+ bytes 0x[0-9A-F]+' -Message "x64 UEFI GOP framebuffer geometry was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] gop draw pixels [1-9][0-9]* token 0x[0-9A-F]+ status 1' -Message "x64 UEFI GOP framebuffer draw proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] boot media read README\.TXT bytes 66 token 0xDAF085B1 prefix 1 status 0x0000000000000000' -Message "x64 UEFI boot-media file read proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] boot manifest read BOOTMAN\.TXT bytes [1-9][0-9]* token 0x[0-9A-F]+ valid 1 kernel-bytes [1-9][0-9]* kernel-checksum 0x[0-9A-F]+ status 0x0000000000000000' -Message "x64 UEFI loader manifest proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] loader payload read KERNEL64\.BIN bytes [1-9][0-9]* token 0x[0-9A-F]+ expected-bytes [1-9][0-9]* expected-token 0x[0-9A-F]+ match 1 status 0x0000000000000000' -Message "x64 UEFI loader payload checksum match was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] loader buffer base 0x[0-9A-F]+ capacity 2097152 loaded [1-9][0-9]* pages [1-9][0-9]* token 0x[0-9A-F]+ match 1 status 0x0000000000000000' -Message "x64 UEFI loader handoff buffer proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] memory map descriptors [1-9][0-9]* desc-size [1-9][0-9]* key 0x[0-9A-F]+ version [0-9]+ total-pages [1-9][0-9]* conventional-pages [1-9][0-9]* loader-pages [0-9]+ boot-pages [0-9]+ runtime-pages [0-9]+ largest-conv 0x[0-9A-F]+/[1-9][0-9]* status 0x0000000000000000' -Message "x64 UEFI memory map summary was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] kernel placement planned 1 request 0x[0-9A-F]+ base 0x[0-9A-F]+ bytes [1-9][0-9]* pages [1-9][0-9]* source 0x[0-9A-F]+ region 0x[0-9A-F]+/[1-9][0-9]* align 2097152 allocated 1 copied 1 token 0x[0-9A-F]+ match 1 status 0x0000000000000000' -Message "x64 UEFI firmware-backed kernel placement proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] linked kernel placement planned 1 request 0x0000000000010000 base 0x[0-9A-F]+ allocation-base 0x[0-9A-F]+ allocation-pages [1-9][0-9]* bytes [1-9][0-9]* pages [1-9][0-9]* entry 0xFFFFFFFF80010000 boot-info 0x[0-9A-F]+ page-root 0x[0-9A-F]+ identity 16777216 source 0x[0-9A-F]+ fixed-ok [01] fixed-status 0x[0-9A-F]+ conflict-type [0-9]+ fallback-attempted [01] fallback-base 0x[0-9A-F]+ fallback-used [01] allocated 1 copied 1 token 0x[0-9A-F]+ match 1 status 0x0000000000000000' -Message "x64 UEFI dynamic linked-kernel placement proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] boot handoff tables planned 1 request 0x0000000000001000 base 0x[0-9A-F]+ pages 15 pml4 0x[0-9A-F]+ pdpt 0x[0-9A-F]+ pd 0x[0-9A-F]+ high-pdpt 0x[0-9A-F]+ framebuffer-pd 0x[0-9A-F]+ kernel-pd 0x[0-9A-F]+ kernel-pt 0x[0-9A-F]+ boot-info 0x[0-9A-F]+ trampoline 0x[0-9A-F]+ tramp-bytes [1-9][0-9]* tramp-ready 1 identity 16777216 kernel-bytes [1-9][0-9]* sectors [1-9][0-9]* entries 8 flags 0x0000003F fb-base 0x[0-9A-F]+ fb-bytes 0x[0-9A-F]+ fb-geometry [1-9][0-9]*x[1-9][0-9]* fb-ppsl [1-9][0-9]* fb-format [0-1] fb-map-pdpt [0-9]+ fb-map-start [0-9]+ fb-map-entries [1-9][0-9]* fb-map-bytes [1-9][0-9]* fb-mapped 1 fb-token 0x[0-9A-F]+ token 0x[0-9A-F]+ fixed-ok [01] fixed-status 0x[0-9A-F]+ conflict-type [0-9]+ fallback-attempted [01] fallback-base 0x[0-9A-F]+ fallback-used [01] allocated 1 built 1 ready 1 status 0x0000000000000000' -Message "x64 UEFI dynamic boot handoff tables and framebuffer map proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] handoff memory map descriptors [1-9][0-9]* desc-size [1-9][0-9]* key 0x[0-9A-F]+ version [0-9]+ total-pages [1-9][0-9]* conventional-pages [1-9][0-9]* loader-pages [1-9][0-9]* boot-pages [0-9]+ runtime-pages [0-9]+ largest-conv 0x[0-9A-F]+/[1-9][0-9]* status 0x0000000000000000' -Message "x64 UEFI post-placement handoff memory map was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] exit boot services status 0x0000000000000000 key 0x[0-9A-F]+ descriptors [1-9][0-9]* desc-size [1-9][0-9]* kernel-base 0x[0-9A-F]+ kernel-bytes [1-9][0-9]* kernel-pages [1-9][0-9]* placement-match 1 firmware-offline 1 handoff-ready 1' -Message "x64 UEFI ExitBootServices handoff proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] kernel entry guard linked-base 0x[0-9A-F]+ linked-match 1 entry 0xFFFFFFFF80010000 boot-info 0x[0-9A-F]+ page-root 0x[0-9A-F]+ identity 16777216 tables-ready 1 jump-ready 1 reason handoff-ready' -Message "x64 UEFI dynamic kernel-entry guard proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[uefi\] firmware services offline; jumping to x64 kernel entry' -Message "x64 UEFI post-ExitBootServices kernel jump marker was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '^LimitlessOS x86_64 scaffold$' -Message "x64 UEFI handoff did not reach the linked kernel scaffold."
    Assert-OutputContains -Lines $outputLines -Pattern '\[boot\] arch 64 flags 0x0000003F' -Message "x64 UEFI framebuffer boot flag was not preserved into the kernel."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] framebuffer handoff base 0x[0-9A-F]+ bytes 0x[0-9A-F]+ [1-9][0-9]*x[1-9][0-9]* ppsl [1-9][0-9]* format [0-1] firmware-token 0x[0-9A-F]+ kernel-draw-pixels [1-9][0-9]* kernel-token 0x[0-9A-F]+ status 1' -Message "x64 kernel-owned framebuffer handoff draw proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] descriptors state 0x0000007F .* cs 0x00000018 .* star-ready 1' -Message "x64 UEFI kernel descriptor reload proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pci broker service 9 cap 0x[0-9A-F]+ denied-devices 0xFFFFFFFF denied-mmio-base 0xFFFFFFFF devices [1-9][0-9]* multi [0-9]+ storage [1-9][0-9]* ide [0-9]+ ahci [1-9][0-9]* nvme [1-9][0-9]* raid [0-9]+ other-storage [0-9]+ intel-system [0-9]+ vmd [0-9]+ usb [0-9]+ display [0-9]+ ahci-addr 0x(?!FFFFFFFF)[0-9A-F]{8} ahci-vendor-device 0x(?!00000000)[0-9A-F]{8} ahci-class 0x0106[0-9A-F]{4} ahci-bar5 0x(?!00000000)[0-9A-F]{8} nvme-addr 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-vendor-device 0x(?!00000000)[0-9A-F]{8} nvme-class 0x0108[0-9A-F]{4} nvme-bar0 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-bar1 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-low 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-high 0x(?!FFFFFFFF)[0-9A-F]{8} nvme-mmio-span [1-9][0-9]* nvme-mmio-flags 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-mmio-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} nvme-candidate-source 1 nvme-candidate-deferred 0 nvme-candidate-bdf 0xFFFFFFFF nvme-candidate-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} other-storage-addr 0x[0-9A-F]{8} other-storage-vendor-device 0x[0-9A-F]{8} other-storage-class 0x[0-9A-F]{8} other-storage-bar0 0x[0-9A-F]{8} other-storage-bar1 0x[0-9A-F]{8} intel-system-addr 0x[0-9A-F]{8} intel-system-vendor-device 0x[0-9A-F]{8} intel-system-class 0x[0-9A-F]{8} intel-system-bar0 0x[0-9A-F]{8} intel-system-bar1 0x[0-9A-F]{8} vmd-addr 0x[0-9A-F]{8} vmd-vendor-device 0x[0-9A-F]{8} vmd-class 0x[0-9A-F]{8} vmd-bar0 0x[0-9A-F]{8} vmd-bar1 0x[0-9A-F]{8} vmd-mmio-low 0x[0-9A-F]{8} vmd-mmio-high 0x[0-9A-F]{8} vmd-mmio-span [0-9]+ vmd-mmio-flags 0x[0-9A-F]{8} vmd-mmio-token 0x[0-9A-F]{8} vmd-nested-plan [0-9]+ vmd-nested-enum [0-9]+ vmd-nested-nvme [0-9]+ vmd-nested-status [0-9]+ vmd-nested-token 0x[0-9A-F]{8} vmd-nested-pci 0x[0-9A-F]{8} vmd-nested-vendor-device 0x[0-9A-F]{8} vmd-nested-class 0x[0-9A-F]{8} vmd-nested-bar0 0x[0-9A-F]{8} vmd-nested-bar1 0x[0-9A-F]{8} vmd-nested-scan-buses [0-9]+ vmd-nested-scan-devices [0-9]+ vmd-nested-scan-functions [0-9]+ vmd-nested-scan-windows [0-9]+ vmd-nested-scan-truncated [01] vmd-nested-mmio-low 0x[0-9A-F]{8} vmd-nested-mmio-high 0x[0-9A-F]{8} vmd-nested-mmio-span [0-9]+ vmd-nested-mmio-flags 0x[0-9A-F]{8} vmd-nested-mmio-token 0x[0-9A-F]{8} vmd-nested-bind-ready [0-9]+ vmd-nested-bind-status [0-9]+ vmd-nested-bind-token 0x[0-9A-F]{8} vmd-nested-register-candidate [0-9]+ vmd-nested-register-status [0-9]+ vmd-nested-register-token 0x[0-9A-F]{8} vmd-nested-driver-plan-result 0x[0-9A-F]{8} vmd-nested-driver-plan-state [0-9]+ vmd-nested-driver-plan-flags 0x[0-9A-F]{8} vmd-nested-driver-plan-token 0x[0-9A-F]{8} vmd-nested-driver-plan-stage-count [0-9]+ vmd-nested-driver-plan-denials [0-9]+ vmd-nested-driver-plan-unavailable [0-9]+ token 0x(?!00000000)[0-9A-F]{8} mmio-base 0x(?!00000000)[0-9A-F]{8} mmio-span 8192 mmio-flags 0x0000007F mmio-token 0x(?!00000000)[0-9A-F]{8} queries [0-9]+ denials 2' -Message "x64 UEFI brokered PCI/AHCI/NVMe inventory and MMIO candidate proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pci-ecam drs-pci-ecam-rsdp 1 drs-pci-ecam-mcfg 1 drs-pci-ecam-base 0x(?!0000000000000000)[0-9A-F]{16} drs-pci-ecam-segment 0 drs-pci-ecam-bus-start 0 drs-pci-ecam-bus-end [0-9]+ drs-pci-ecam-active 1 drs-pci-ecam-fallback-io 0 drs-pci-ecam-ahci-found 1' -Message "x64 UEFI PCI ECAM AHCI discovery proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-probe denied-drs-nvme-probe 0xFFFFFFFF drs-nvme-probe 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-nvme-probe-found 1 drs-nvme-probe-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-nvme-candidate-source 1 drs-nvme-candidate-deferred 0 drs-nvme-candidate-bdf 0xFFFFFFFF drs-nvme-candidate-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-nvme-probe-ready 1 drs-nvme-probe-identify 1 drs-nvme-probe-model (?!none)[A-Za-z0-9_.-]+ drs-nvme-probe-firmware (?!none)[A-Za-z0-9_.-]+ drs-nvme-probe-io-queue 0 drs-nvme-probe-read-authority 0 drs-nvme-probe-fs-authority 0 drs-nvme-probe-unavailable 0' -Message "x64 UEFI NVMe admin identify proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-read denied-drs-nvme-read 0xFFFFFFFF drs-nvme-read 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-nvme-read-ioq-created 1 drs-nvme-read-issued 1 drs-nvme-read-completed 1 drs-nvme-read-status 0 drs-nvme-read-bytes 4096 drs-nvme-read-checksum 0x(?!00000000)[0-9A-F]{8} fs-authority 0 block-endpoint 0 write-authority 0 unavailable 0 error 0' -Message "x64 UEFI NVMe IO read proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-gpt denied-drs-nvme-gpt 0xFFFFFFFF drs-nvme-gpt 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-nvme-gpt-signature 1 drs-nvme-gpt-partitions 6 drs-nvme-gpt-fat32-start 2048 drs-nvme-gpt-fat32-sectors [1-9][0-9]* drs-nvme-gpt-vbr 1 fs-authority 0 write-authority 0 m5-safe-targets 2 m5-forbidden-targets 4 m5-unknown-targets 0 m5-boot-partition 5 m5-root-partition 6 m5-boot-start 16384 m5-root-start 20480 m5-forbidden-denied 1 m5-no-write-authority 1 unavailable 0 error 0' -Message "x64 UEFI NVMe GPT partition/classification proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-fat denied-drs-nvme-fat 0xFFFFFFFF drs-nvme-fat 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-nvme-fat-bpb 1 drs-nvme-fat-located 1 drs-nvme-fat-read-bytes 30 drs-nvme-fat-checksum 0xD4F8C331 drs-nvme-fat-content-match 1 drs-nvme-fat-bytes-per-sector 512 drs-nvme-fat-sectors-per-cluster 2 drs-nvme-fat-lfn 1 drs-nvme-fat-unicode-lfn 1 drs-nvme-fat-subdir 1 drs-nvme-fat-multicluster 1 drs-nvme-fat-multi-bytes 2500 drs-nvme-fat-write-gate 1 drs-nvme-fat-create-cluster 12 drs-nvme-fat-create-readback 1 drs-nvme-fat-create-bytes 47 drs-nvme-fat-create-checksum 0x8B4D45D8 drs-nvme-fat-update-cluster 13 drs-nvme-fat-update-readback 1 drs-nvme-fat-update-bytes 1800 drs-nvme-fat-update-checksum 0x4E5F0AAE drs-nvme-fat-delete-freed 1 drs-nvme-fat-delete-tombstone 1 drs-nvme-fat-flushes [1-9][0-9]* fs-delegation 0 block-endpoint 0 write-authority 0 commit-authority 0 unavailable 0 error 0' -Message "x64 UEFI NVMe FAT file-read/write proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-nvme-rw delegated 1 cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} wrong-owner 1 stale 1 revoked 1 shell-write 1 shell-readback 1 write-bytes [1-9][0-9]* write-checksum 0x(?!00000000)[0-9A-F]{8} persisted 0 audit [1-9][0-9]* commits [1-9][0-9]* write-authority 1 commit-authority 1 unavailable 0 error 0' -Message "x64 UEFI NVMe scoped shell write-authority proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-installer-commit drs-installer-commit-attempted 1 drs-installer-commit-runtime-fat-target 1 drs-installer-commit-confirmation-token 1 drs-installer-commit-scoped-write-cap 1 drs-installer-commit-bad-token-denied 1 drs-installer-commit-wrong-owner-denied 1 drs-installer-commit-write 1 drs-installer-commit-readback 1 drs-installer-commit-bytes 42 drs-installer-commit-checksum 0x(?!00000000)[0-9A-F]{8} drs-installer-commit-audit [1-9][0-9]* drs-installer-commit-no-ambient 1 drs-installer-commit-unavailable 0 error 0 mode nvme-fat-marker-only' -Message "x64 UEFI scoped installer commit marker proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-installer-target drs-installer-target-attempted 1 drs-installer-target-confirmation-token 1 drs-installer-target-classified 1 drs-installer-target-boot-partition 5 drs-installer-target-root-partition 6 drs-installer-target-boot-start 16384 drs-installer-target-root-start 20480 drs-installer-target-forbidden-denied 1 drs-installer-target-bad-token-denied 1 drs-installer-target-wrong-target-denied 1 drs-installer-target-wrong-owner-denied 1 drs-installer-target-m5-write-cap 1 drs-installer-target-write 1 drs-installer-target-readback 1 drs-installer-target-bytes 34 drs-installer-target-checksum 0x(?!00000000)[0-9A-F]{8} drs-installer-target-write-denied 1 drs-installer-target-format-denied 1 drs-installer-target-boot-entry-denied 1 drs-installer-target-no-ambient 1 drs-installer-target-unavailable 0 error 0 mode m5-boot-marker-write-only' -Message "x64 UEFI installer M5 scoped boot-marker write/readback proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-apic drs-apic-madt 1 drs-apic-lapic-base 0x(?!0000000000000000)[0-9A-F]{16} drs-apic-ioapic-base 0x(?!0000000000000000)[0-9A-F]{16} drs-apic-pic-disabled 1 drs-apic-timer-ticking 1 drs-apic-keyboard-live 1 drs-apic-enabled 1' -Message "x64 UEFI APIC MADT/LAPIC/IOAPIC switchover proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-apic-override drs-apic-override-scanned 1 drs-apic-override-count [1-9][0-9]* drs-apic-timer-gsi (?!0 )[0-9]+ .* drs-apic-keyboard-gsi [0-9]+ .* drs-apic-timer-ticking 1 drs-apic-keyboard-live 1' -Message "x64 UEFI APIC interrupt-source-override routing proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-xhci drs-xhci-found 1 drs-xhci-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-xhci-mapped 1 drs-xhci-cap [1-9][0-9]* drs-xhci-ports [1-9][0-9]* drs-xhci-ports-scanned [1-9][0-9]* drs-xhci-connected [1-9][0-9]* drs-xhci-command-ring 1 drs-xhci-dcbaa 1 drs-xhci-event-ring 1 drs-xhci-reset 1 drs-xhci-running 1 drs-xhci-slot-enabled 1 drs-xhci-addressed 1 drs-xhci-config-read 1 drs-xhci-report-desc 1 drs-xhci-endpoint 1 drs-xhci-hid-device 1 drs-xhci-input-live 1 drs-xhci-reports [1-9][0-9]* drs-xhci-report-bytes [1-9][0-9]* unavailable 0 error 0' -Message "x64 UEFI xHCI HID keyboard proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-xhci .* unavailable 0 error 0 drs-xhci-extcaps-scanned [1-9][0-9]* drs-xhci-legacy-cap [0-1] drs-xhci-legacy-handoff 1 .* drs-xhci-bios-owned-clear 1 .* drs-xhci-protocol-caps [1-9][0-9]* drs-xhci-usb2-ports [1-9][0-9]* drs-xhci-usb3-ports [1-9][0-9]* drs-xhci-prefer-usb2 1 .* drs-xhci-reset-wait-ms 100 drs-xhci-settle-ms 50' -Message "x64 UEFI xHCI handoff/protocol/timing proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-mouse drs-mouse-found 1 drs-mouse-delta 1 drs-mouse-buttons [01] drs-mouse-packets [1-9][0-9]* .* drs-mouse-usb-device [0-1]' -Message "x64 UEFI brokered mouse input proof was not observed."
    if ($BuildProfile -eq "Experimental") {
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] build-profile Experimental product 0 experimental 1 experimental-runtime 1' -Message "x64 Experimental build-profile marker was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] experimental-runtime enabled proof-surface 1 not-product-path 1' -Message "x64 Experimental runtime label was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-compositor drs-compositor-init 1 drs-compositor-present 1 drs-compositor-cursor 1 drs-compositor-presents [1-9][0-9]* drs-compositor-cursors [1-9][0-9]*' -Message "x64 UEFI compositor proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-font drs-font-init 1 drs-font-glyphs 256 drs-font-render 1 drs-font-renders [1-9][0-9]*' -Message "x64 UEFI font renderer proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-wm drs-wm-init 1 drs-wm-window-created 1 drs-wm-focus 1 drs-wm-present 1 drs-wm-windows [1-9][0-9]* drs-wm-focuses [1-9][0-9]* drs-wm-presents [1-9][0-9]*' -Message "x64 UEFI window manager proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-desktop drs-desktop-init 1 drs-desktop-taskbar 1 drs-desktop-launcher 1 drs-desktop-terminal 1 drs-desktop-fileman 1 drs-desktop-settings 1' -Message "x64 UEFI desktop environment proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-gui drs-gui-interactive 1 drs-gui-click-hittest 1 drs-gui-launcher-opened 1 drs-gui-terminal-opened 1 drs-gui-drag-completed [01] drs-gui-keyboard-routed 1 drs-gui-close-completed [01] drs-gui-taskbar-focus [01] drs-gui-fileman-opened 1 drs-gui-settings-opened [01] drs-gui-installer-opened [01] drs-gui-right-click [0-9]+ drs-gui-context-action [0-9]+ wm-resize [0-9]+ wm-minimize [0-9]+ wm-restore [0-9]+ wm-zorder [0-9]+ drs-gui-scroll [1-9][0-9]* terminal-actions [1-9][0-9]* terminal-scroll [1-9][0-9]* terminal-scroll-offset [0-9]+ terminal-selection [1-9][0-9]* terminal-copy [1-9][0-9]* terminal-copied-bytes [1-9][0-9]* terminal-cursor [1-9][0-9]* fileman-actions [1-9][0-9]* fileman-refresh [1-9][0-9]* .* fileman-write [1-9][0-9]* .* fileman-mkdir [1-9][0-9]* .* fileman-edit [1-9][0-9]* fileman-edit-commit [1-9][0-9]* .* drs-gui-unfocused-key-denied [0-9]+ drs-gui-no-ambient-input 1 drs-gui-no-ambient-display 1 drs-gui-no-ambient-fs 1 .* target-window [0-9]+ .* key-target-window [0-9]+ unfocused-key-denials [0-9]+ input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041' -Message "x64 UEFI GUI input-routed interactive proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-gui .* drs-gui-close-completed 1 drs-gui-taskbar-focus 1 .* drs-gui-right-click [1-9][0-9]* drs-gui-context-action [1-9][0-9]* wm-resize [1-9][0-9]* wm-minimize [1-9][0-9]* wm-restore [1-9][0-9]* wm-zorder [1-9][0-9]*' -Message "x64 UEFI M132 window-manager/context/taskbar proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern 'settings-actions [1-9][0-9]* settings-load [0-9]+ settings-save [1-9][0-9]* settings-save-denial 0 settings-export [1-9][0-9]* settings-export-denial 0 settings-theme [01] settings-pointer [123] settings-keyrepeat [01]' -Message "x64 UEFI Settings persisted workflow proof was not observed."
        if ($NetworkDevice -eq "virtio") {
            Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-net drs-net-found 1 drs-net-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-net-mapped 1 drs-net-common 1 drs-net-notify 1 drs-net-device-config 1 drs-net-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-net-mac-nonzero 1 drs-net-status-ack 1 drs-net-status-driver 1 drs-net-features-ok 1 drs-net-driver-ok 1 drs-net-rx-queue 1 drs-net-tx-queue 1 drs-net-rx-buffers [1-9][0-9]* drs-net-tx 1 drs-net-rx 1 drs-net-arp-reply 1 drs-net-arp-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-net-arp-ip 0x0A000202 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 UEFI virtio-net brokered ARP proof was not observed."
        }
        else {
            Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-e1000 drs-e1000-found 1 drs-e1000-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-e1000-mapped 1 drs-e1000-reset 1 drs-e1000-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-e1000-mac-nonzero 1 drs-e1000-link-up [0-1] drs-e1000-rx-queue 1 drs-e1000-tx-queue 1 drs-e1000-rx-buffers [1-9][0-9]* drs-e1000-tx 1 drs-e1000-rx 1 drs-e1000-dhcp 1 drs-e1000-dns 1 drs-e1000-http 200 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 UEFI e1000e brokered network proof was not observed."
        }
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dhcp drs-dhcp-discover 1 drs-dhcp-offer 1 drs-dhcp-request 1 drs-dhcp-ack 1 drs-dhcp-ip 0x(?!00000000)[0-9A-F]{8} drs-dhcp-gateway 0x0A000202 drs-dhcp-dns 0x[0-9A-F]{8} drs-dhcp-lease [1-9][0-9]* ambient-authority 0 unavailable 0 error 0' -Message "x64 UEFI DHCP lease proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dns drs-dns-query 1 drs-dns-response 1 drs-dns-rcode 0 drs-dns-resolved 0x(?!00000000)[0-9A-F]{8} fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 UEFI DNS A-record proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-http drs-http-connected 1 drs-http-sent 1 drs-http-status [1-9][0-9]* drs-http-response-bytes [1-9][0-9]* fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 UEFI HTTP-over-TCP proof was not observed."
    }
    else {
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] build-profile Product product 1 experimental 0 experimental-runtime 0' -Message "x64 Product build-profile marker was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] experimental-runtime disabled proof-surface 0 gui product-gated network product-gated ai unavailable installer unavailable package-manager unavailable' -Message "x64 Product experimental-quarantine marker was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-compositor drs-compositor-init 1 drs-compositor-present 1 drs-compositor-cursor 1 drs-compositor-presents [1-9][0-9]* drs-compositor-cursors [1-9][0-9]*' -Message "x64 Product compositor proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-font drs-font-init 1 drs-font-glyphs 256 drs-font-render 1 drs-font-renders [1-9][0-9]*' -Message "x64 Product font renderer proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-wm drs-wm-init 1 drs-wm-window-created 1 drs-wm-focus 1 drs-wm-present 1 drs-wm-windows [1-9][0-9]* drs-wm-focuses [1-9][0-9]* drs-wm-presents [1-9][0-9]*' -Message "x64 Product window-manager proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-desktop drs-desktop-init 1 drs-desktop-taskbar 1 drs-desktop-launcher 1 drs-desktop-terminal 1 drs-desktop-fileman 1 drs-desktop-settings 1' -Message "x64 Product desktop proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-gui drs-gui-interactive 1 drs-gui-click-hittest 1 drs-gui-launcher-opened 1 drs-gui-terminal-opened 1 drs-gui-drag-completed [01] drs-gui-keyboard-routed 1 drs-gui-close-completed [01] drs-gui-taskbar-focus [01] drs-gui-fileman-opened 1 drs-gui-settings-opened [01] drs-gui-installer-opened [01] drs-gui-right-click [0-9]+ drs-gui-context-action [0-9]+ wm-resize [0-9]+ wm-minimize [0-9]+ wm-restore [0-9]+ wm-zorder [0-9]+ drs-gui-scroll [1-9][0-9]* terminal-actions [1-9][0-9]* terminal-scroll [1-9][0-9]* terminal-scroll-offset [0-9]+ terminal-selection [1-9][0-9]* terminal-copy [1-9][0-9]* terminal-copied-bytes [1-9][0-9]* terminal-cursor [1-9][0-9]* fileman-actions [1-9][0-9]* fileman-refresh [1-9][0-9]* .* fileman-write [1-9][0-9]* .* fileman-mkdir [1-9][0-9]* .* fileman-edit [1-9][0-9]* fileman-edit-commit [1-9][0-9]* .* drs-gui-unfocused-key-denied [0-9]+ drs-gui-no-ambient-input 1 drs-gui-no-ambient-display 1 drs-gui-no-ambient-fs 1 .* target-window [0-9]+ .* key-target-window [0-9]+ unfocused-key-denials [0-9]+ input-token 0x494E5054 display-token 0x44495350 fs-token 0x46535041' -Message "x64 Product GUI input-routed interactive proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-gui .* drs-gui-close-completed 1 drs-gui-taskbar-focus 1 .* drs-gui-right-click [1-9][0-9]* drs-gui-context-action [1-9][0-9]* wm-resize [1-9][0-9]* wm-minimize [1-9][0-9]* wm-restore [1-9][0-9]* wm-zorder [1-9][0-9]*' -Message "x64 Product M132 window-manager/context/taskbar proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern 'settings-actions [1-9][0-9]* settings-load [0-9]+ settings-save [1-9][0-9]* settings-save-denial 0 settings-export [1-9][0-9]* settings-export-denial 0 settings-theme [01] settings-pointer [123] settings-keyrepeat [01]' -Message "x64 Product Settings persisted workflow proof was not observed."
        if ($NetworkDevice -eq "virtio") {
            Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-net drs-net-found 1 drs-net-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-net-mapped 1 drs-net-common 1 drs-net-notify 1 drs-net-device-config 1 drs-net-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-net-mac-nonzero 1 drs-net-status-ack 1 drs-net-status-driver 1 drs-net-features-ok 1 drs-net-driver-ok 1 drs-net-rx-queue 1 drs-net-tx-queue 1 drs-net-rx-buffers [1-9][0-9]* drs-net-tx 1 drs-net-rx 1 drs-net-arp-reply 1 drs-net-arp-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-net-arp-ip 0x0A000202 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 Product virtio-net brokered ARP proof was not observed."
        }
        else {
            Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-e1000 drs-e1000-found 1 drs-e1000-bar0 0x(?!0000000000000000)[0-9A-F]{16} drs-e1000-mapped 1 drs-e1000-reset 1 drs-e1000-mac 0x(?!0000000000000000)[0-9A-F]{16} drs-e1000-mac-nonzero 1 drs-e1000-link-up [0-1] drs-e1000-rx-queue 1 drs-e1000-tx-queue 1 drs-e1000-rx-buffers [1-9][0-9]* drs-e1000-tx 1 drs-e1000-rx 1 drs-e1000-dhcp 1 drs-e1000-dns 1 drs-e1000-http 200 fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 Product e1000e brokered network proof was not observed."
        }
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dhcp drs-dhcp-discover 1 drs-dhcp-offer 1 drs-dhcp-request 1 drs-dhcp-ack 1 drs-dhcp-ip 0x(?!00000000)[0-9A-F]{8} drs-dhcp-gateway 0x0A000202 drs-dhcp-dns 0x[0-9A-F]{8} drs-dhcp-lease [1-9][0-9]* ambient-authority 0 unavailable 0 error 0' -Message "x64 Product DHCP lease proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-dns drs-dns-query 1 drs-dns-response 1 drs-dns-rcode 0 drs-dns-resolved 0x(?!00000000)[0-9A-F]{8} fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 Product DNS A-record proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-http drs-http-connected 1 drs-http-sent 1 drs-http-status [1-9][0-9]* drs-http-response-bytes [1-9][0-9]* fs-authority 0 storage-authority 0 ambient-authority 0 unavailable 0 error 0' -Message "x64 Product HTTP-over-TCP proof was not observed."
    }
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] mmio planner service 9 .* map-request 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} plans 1 .* map-installed 1 .* port-state 3 .* policy-ready 1 .* denied-read-plan 0xFFFFFFFF read-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} read-state 3 read-flags 0x0000BFFF .* read-staged 1 read-denials 1 read-unavailable 0 denied-cmd-plan 0xFFFFFFFF cmd-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} cmd-state 3 cmd-flags 0x0001FFFF cmd-token 0x(?!00000000)[0-9A-F]{8} cmd-read-token 0x(?!00000000)[0-9A-F]{8} cmd-op 2 cmd-slot 0 cmd-header 32 cmd-table 144 cmd-cfis 20 cmd-cfis-dwords 5 cmd-prdt 1 cmd-prdt-bytes 16 cmd-atapi-packet 12 cmd-opcode 0x000000A0 cmd-packet-opcode 0x00000028 cmd-transfer 2048 cmd-armed 0 cmd-issued 0 cmd-dma 0 cmd-staged 1 cmd-denials 1 cmd-unavailable 0 denied-mem-plan 0xFFFFFFFF mem-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} mem-state 3 mem-flags 0x007BFFFF mem-token 0x(?!00000000)[0-9A-F]{8} mem-cmd-token 0x(?!00000000)[0-9A-F]{8} mem-slot 0 mem-pages 1 mem-page-bytes 4096 mem-page-virt 0x[0-9A-F]{13}000 mem-page-phys 0x[0-9A-F]{13}000 mem-page-checksum 0x76EFDDC5 mem-zeroed 1 mem-materialized 1 mem-list-off 0 mem-list-bytes 1024 mem-header-off 0 mem-header-bytes 32 mem-table-off 1024 mem-table-bytes 144 mem-prdt-off 1152 mem-prdt-bytes 16 mem-bounce-off 2048 mem-bounce-bytes 2048 mem-prdt-dbc 2047 mem-dma-low 0x00000000 mem-dma-high 0x00000000 mem-dma 0 mem-table-written 0 mem-port-programmed 0 mem-armed 0 mem-staged 1 mem-denials 1 mem-unavailable 0 denied-table-plan 0xFFFFFFFF table-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} table-state 3 table-flags 0x006FFFFF table-token 0x(?!00000000)[0-9A-F]{8} table-mem-token 0x(?!00000000)[0-9A-F]{8} table-check-before 0x76EFDDC5 table-check-after 0x3FBFAF45 table-check-changed 1 table-header-flags 0x00010025 table-prdtl 1 table-prdbc 0 table-ctba-low 0x00000000 table-ctba-high 0x00000000 table-cfis-type 0x00000027 table-cfis-flags 0x00000080 table-cfis-command 0x000000A0 table-cfis-device 0x00000040 table-cfis-count 0 table-packet-opcode 0x00000028 table-packet-blocks 1 table-prdt-dba-low 0x00000000 table-prdt-dba-high 0x00000000 table-prdt-dbc 2047 table-written 1 table-dma 0 table-port-programmed 0 table-armed 0 table-issued 0 table-staged 1 table-denials 1 table-unavailable 0 map-requests 1 .* queries 140 denials 9' -Message "x64 UEFI brokered MMIO AHCI table-prep proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] mmio planner service 9 .* denied-issue-plan 0xFFFFFFFF issue-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} issue-state 3 issue-flags 0x06FFFFFF issue-token 0x(?!00000000)[0-9A-F]{8} issue-table-token 0x(?!00000000)[0-9A-F]{8} issue-mem-token 0x(?!00000000)[0-9A-F]{8} issue-cmd-token 0x(?!00000000)[0-9A-F]{8} issue-read-token 0x(?!00000000)[0-9A-F]{8} issue-port 0x(?!FFFFFFFF)[0-9A-F]{8} issue-slot 0 issue-ci 0x00000000 issue-slot-mask 0x00000001 issue-slot-idle 1 issue-tfd-ready 1 issue-serr-clear 1 issue-policy-ready 1 issue-engine-st 0 issue-engine-fre 0 issue-engine-fr 0 issue-engine-cr 0 issue-stop-required 0 issue-start-required 1 issue-timeout 100 issue-poll-budget 10000 issue-table-check 0x3FBFAF45 issue-expected-check 0x3FBFAF45 issue-check-match 1 issue-dma 0 issue-port-programmed 0 issue-command-issued 0 issue-armed 0 issue-staged 1 issue-denials 1 issue-unavailable 0 map-requests 1 .* queries 170 denials 10' -Message "x64 UEFI brokered MMIO AHCI issue-preflight proof was not observed."
    $uefiBindPattern = (
        '\[x64\] mmio planner service 9 .* denied-bind-plan 0xFFFFFFFF bind-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} bind-state 3 bind-flags 0x0FFFFFFF bind-token 0x(?!00000000)[0-9A-F]{8} ' +
        'bind-issue-token 0x(?!00000000)[0-9A-F]{8} bind-table-token 0x(?!00000000)[0-9A-F]{8} bind-mem-token 0x(?!00000000)[0-9A-F]{8} bind-cmd-token 0x(?!00000000)[0-9A-F]{8} bind-read-token 0x(?!00000000)[0-9A-F]{8} ' +
        'bind-page-low 0x[0-9A-F]{8} bind-page-high 0x00000000 bind-list-low 0x[0-9A-F]{8} bind-list-high 0x00000000 bind-table-low 0x[0-9A-F]{8} bind-table-high 0x00000000 bind-bounce-low 0x[0-9A-F]{8} bind-bounce-high 0x00000000 ' +
        'bind-header-ctba-low 0x[0-9A-F]{8} bind-header-ctba-high 0x00000000 bind-prdt-dba-low 0x[0-9A-F]{8} bind-prdt-dba-high 0x00000000 bind-prdt-dbc 2047 bind-header-patch 8 bind-prdt-patch 1152 ' +
        'bind-check-before 0x3FBFAF45 bind-check-predicted 0x[0-9A-F]{8} bind-check-changed 1 bind-aligned 1 bind-range-ready 1 bind-below-4g 1 ' +
        'bind-memory-written 0 bind-dma 0 bind-port-programmed 0 bind-published 0 bind-command-issued 0 bind-armed 0 bind-staged 1 bind-denials 1 bind-unavailable 0 denied-patch-plan 0xFFFFFFFF .* denied-publish-plan 0xFFFFFFFF .* denied-publish-gate 0xFFFFFFFF .* map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiBindPattern -Message "x64 UEFI brokered MMIO AHCI address-bind proof was not observed."
    $uefiPatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-patch-plan 0xFFFFFFFF patch-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} patch-state 3 patch-flags 0x0DFFFFFF patch-token 0x(?!00000000)[0-9A-F]{8} ' +
        'patch-bind-token 0x(?!00000000)[0-9A-F]{8} patch-issue-token 0x(?!00000000)[0-9A-F]{8} patch-table-token 0x(?!00000000)[0-9A-F]{8} patch-mem-token 0x(?!00000000)[0-9A-F]{8} patch-cmd-token 0x(?!00000000)[0-9A-F]{8} patch-read-token 0x(?!00000000)[0-9A-F]{8} ' +
        'patch-header-patch 8 patch-prdt-patch 1152 patch-header-ctba-low 0x[0-9A-F]{8} patch-header-ctba-high 0x00000000 patch-prdt-dba-low 0x[0-9A-F]{8} patch-prdt-dba-high 0x00000000 ' +
        'patch-check-before 0x3FBFAF45 patch-check-expected 0x[0-9A-F]{8} patch-check-after 0x[0-9A-F]{8} patch-check-match 1 patch-check-changed 1 ' +
        'patch-memory-written 1 patch-dma 0 patch-port-programmed 0 patch-published 0 patch-command-issued 0 patch-armed 0 patch-staged 1 patch-denials 1 patch-unavailable 0 denied-publish-plan 0xFFFFFFFF .* denied-publish-gate 0xFFFFFFFF .* map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiPatchPattern -Message "x64 UEFI brokered MMIO AHCI private address-patch proof was not observed."
    $uefiPublishPattern = (
        '\[x64\] mmio planner service 9 .* denied-publish-plan 0xFFFFFFFF publish-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} publish-state 3 publish-flags 0x03FFFFFF publish-token 0x(?!00000000)[0-9A-F]{8} ' +
        'publish-patch-token 0x(?!00000000)[0-9A-F]{8} publish-bind-token 0x(?!00000000)[0-9A-F]{8} publish-issue-token 0x(?!00000000)[0-9A-F]{8} publish-table-token 0x(?!00000000)[0-9A-F]{8} publish-mem-token 0x(?!00000000)[0-9A-F]{8} publish-cmd-token 0x(?!00000000)[0-9A-F]{8} publish-read-token 0x(?!00000000)[0-9A-F]{8} ' +
        'publish-port 0x(?!FFFFFFFF)[0-9A-F]{8} publish-port-base [1-9][0-9]* publish-list-low 0x[0-9A-F]{8} publish-list-high 0x00000000 publish-fis-low 0x[0-9A-F]{8} publish-fis-high 0x00000000 ' +
        'publish-clb-off [1-9][0-9]* publish-clbu-off [1-9][0-9]* publish-fb-off [1-9][0-9]* publish-fbu-off [1-9][0-9]* publish-cmd-off [1-9][0-9]* publish-ci-off [1-9][0-9]* ' +
        'publish-clb-low 0x[0-9A-F]{8} publish-clb-high 0x00000000 publish-fb-low 0x[0-9A-F]{8} publish-fb-high 0x00000000 publish-fis-off 1536 publish-fis-bytes 256 publish-page-check 0x[0-9A-F]{8} publish-page-match 1 ' +
        'publish-clb-aligned 1 publish-fis-aligned 1 publish-range-ready 1 publish-below-4g 1 publish-memory-written 0 publish-dma 0 publish-mmio-written 0 publish-port-programmed 0 publish-published 0 publish-command-issued 0 publish-armed 0 publish-staged 1 publish-denials 1 publish-unavailable 0 denied-publish-gate 0xFFFFFFFF publish-gate 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} .* map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiPublishPattern -Message "x64 UEFI brokered MMIO AHCI publish-preflight proof was not observed."
    $uefiGatePattern = (
        '\[x64\] mmio planner service 9 .* denied-publish-gate 0xFFFFFFFF publish-gate 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} gate-state 3 gate-flags 0x0000FFFF ' +
        'gate-token 0x(?!00000000)[0-9A-F]{8} gate-publish-token 0x(?!00000000)[0-9A-F]{8} gate-live-hardware 1 gate-exclusive 1 gate-revocation-required 1 gate-revocation-satisfied 0 ' +
        'gate-write-window 0 gate-commit-allowed 0 gate-mmio-written 0 gate-port-programmed 0 gate-published 0 gate-command-issued 0 gate-armed 0 ' +
        'gate-staged 1 gate-denials 1 gate-unavailable 0 denied-window-policy 0xFFFFFFFF window-policy 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} .* map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiGatePattern -Message "x64 UEFI brokered MMIO AHCI publication gate proof was not observed."
    $uefiWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-window-policy 0xFFFFFFFF window-policy 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} window-state 3 window-flags 0x0000FFFF ' +
        'window-token 0x(?!00000000)[0-9A-F]{8} window-gate-token 0x(?!00000000)[0-9A-F]{8} window-publish-token 0x(?!00000000)[0-9A-F]{8} window-live-hardware 1 window-exclusive 1 ' +
        'window-revocation-required 1 window-revocation-satisfied 0 window-revocation-executed 0 window-write-window 0 window-commit-allowed 0 ' +
        'window-mmio-written 0 window-port-programmed 0 window-published 0 window-command-issued 0 window-armed 0 ' +
        'window-staged 1 window-denials 1 window-unavailable 0 denied-revoke-plan 0xFFFFFFFF revoke-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} .* map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiWindowPattern -Message "x64 UEFI brokered MMIO AHCI publish write-window policy proof was not observed."
    $uefiRevokePattern = (
        '\[x64\] mmio planner service 9 .* denied-revoke-plan 0xFFFFFFFF revoke-plan 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} revoke-state 3 revoke-flags 0x0000FFFF ' +
        'revoke-token 0x(?!00000000)[0-9A-F]{8} revoke-window-token 0x(?!00000000)[0-9A-F]{8} revoke-gate-token 0x(?!00000000)[0-9A-F]{8} revoke-live-before 1 revoke-live-after 1 revoke-exclusive 1 ' +
        'revoke-required 1 revoke-planned 1 revoke-executed 0 revoke-would-revoke 1 revoke-write-window 0 revoke-commit-allowed 0 ' +
        'revoke-mmio-written 0 revoke-port-programmed 0 revoke-published 0 revoke-command-issued 0 revoke-armed 0 ' +
        'revoke-staged 1 revoke-denials 1 revoke-unavailable 0 denied-open-window 0xFFFFFFFF open-window 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} open-state 3 open-flags 0x0000FFFF ' +
        'open-token 0x(?!00000000)[0-9A-F]{8} open-revoke-token 0x(?!00000000)[0-9A-F]{8} open-window-token 0x(?!00000000)[0-9A-F]{8} open-live-hardware 1 open-revocation-required 1 open-revocation-planned 1 open-revocation-executed 0 ' +
        'open-write-window 0 open-allowed 0 open-commit-allowed 0 open-mmio-written 0 open-port-programmed 0 open-published 0 open-command-issued 0 open-armed 0 ' +
        'open-staged 1 open-denials 1 open-unavailable 0 denied-session 0xFFFFFFFF session 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} session-state 3 session-flags 0x0000FFFF ' +
        'session-token 0x(?!00000000)[0-9A-F]{8} session-open-token 0x(?!00000000)[0-9A-F]{8} session-revoke-token 0x(?!00000000)[0-9A-F]{8} session-window-token 0x(?!00000000)[0-9A-F]{8} session-live-hardware 1 session-revocation-required 1 session-revocation-planned 1 session-revocation-executed 0 ' +
        'session-allowed 0 session-driver-owned 0 session-write-window 0 session-commit-allowed 0 session-mmio-written 0 session-port-programmed 0 session-published 0 session-command-issued 0 session-armed 0 ' +
        'session-staged 1 session-denials 1 session-unavailable 0 denied-drain 0xFFFFFFFF drain 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drain-state 3 drain-flags 0x0000FFFF ' +
        'drain-token 0x(?!00000000)[0-9A-F]{8} drain-session-token 0x(?!00000000)[0-9A-F]{8} drain-open-token 0x(?!00000000)[0-9A-F]{8} drain-revoke-token 0x(?!00000000)[0-9A-F]{8} drain-window-token 0x(?!00000000)[0-9A-F]{8} drain-live-before 1 drain-revoked 1 drain-live-after 0 ' +
        'drain-revocation-required 1 drain-revocation-planned 1 drain-revocation-executed 1 drain-write-window 0 drain-commit-allowed 0 drain-mmio-written 0 drain-port-programmed 0 drain-published 0 drain-command-issued 0 drain-armed 0 ' +
        'drain-staged 1 drain-denials 1 drain-unavailable 0 denied-handoff 0xFFFFFFFF handoff 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} handoff-state 3 handoff-flags 0x0001FFFF ' +
        'handoff-token 0x(?!00000000)[0-9A-F]{8} handoff-drain-token 0x(?!00000000)[0-9A-F]{8} handoff-old-handle 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} handoff-driver-owner 0x00001006 handoff-driver-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'handoff-live-before 0 handoff-stale-old-denied 1 handoff-driver-valid 1 handoff-driver-role 0x000000B4 handoff-owner-bound 1 handoff-query-only 1 handoff-live-after 1 ' +
        'handoff-write-window 0 handoff-commit-allowed 0 handoff-mmio-written 0 handoff-port-programmed 0 handoff-published 0 handoff-command-issued 0 handoff-armed 0 ' +
        'handoff-staged 1 handoff-denials 1 handoff-unavailable 0 denied-driver-probe 0xFFFFFFFF driver-probe 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-probe-state 3 driver-probe-flags 0x0001FFFF ' +
        'driver-probe-token 0x(?!00000000)[0-9A-F]{8} driver-probe-handoff-token 0x(?!00000000)[0-9A-F]{8} driver-probe-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-probe-owner 0x00001006 driver-probe-owner-bound 1 driver-probe-query-only 1 ' +
        'driver-probe-cap-reg 0x(?!00000000)[0-9A-F]{8} driver-probe-ghc 0x80000000 driver-probe-pi 0x(?!00000000)[0-9A-F]{8} driver-probe-version 0x00010000 driver-probe-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-probe-ssts 0x00000113 driver-probe-sig 0xEB140101 ' +
        'driver-probe-cmd 0x03000006 driver-probe-tfd 0x00000050 driver-probe-ci 0x00000000 driver-probe-serr 0x00000000 driver-probe-kind 2 driver-probe-read-ready 1 driver-probe-busy 0 driver-probe-drq 0 ' +
        'driver-probe-ci-idle 1 driver-probe-serr-clear 1 driver-probe-op 2 driver-probe-lba 0 driver-probe-blocks 1 driver-probe-bytes 2048 driver-probe-mmio-written 0 driver-probe-port-programmed 0 ' +
        'driver-probe-published 0 driver-probe-command-issued 0 driver-probe-dma 0 driver-probe-armed 0 driver-probe-staged 1 driver-probe-denials 1 driver-probe-unavailable 0 denied-driver-intent 0xFFFFFFFF driver-intent 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-intent-state 3 driver-intent-flags 0x0001FFFF ' +
        'driver-intent-token 0x(?!00000000)[0-9A-F]{8} driver-intent-probe-token 0x(?!00000000)[0-9A-F]{8} driver-intent-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-intent-owner 0x00001006 driver-intent-owner-bound 1 driver-intent-query-only 1 ' +
        'driver-intent-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-intent-kind 2 driver-intent-op 2 driver-intent-lba 0 driver-intent-blocks 1 driver-intent-bytes 2048 driver-intent-read-ready 1 driver-intent-mmio-written 0 driver-intent-port-programmed 0 ' +
        'driver-intent-published 0 driver-intent-command-issued 0 driver-intent-dma 0 driver-intent-armed 0 driver-intent-media-read 0 driver-intent-staged 1 driver-intent-denials 1 driver-intent-unavailable 0 denied-driver-buffer 0xFFFFFFFF driver-buffer 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-buffer-state 3 driver-buffer-flags 0x0001FFFF driver-buffer-token 0x(?!00000000)[0-9A-F]{8} driver-buffer-intent-token 0x(?!00000000)[0-9A-F]{8} driver-buffer-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-buffer-owner 0x00001006 driver-buffer-owner-bound 1 driver-buffer-query-only 1 driver-buffer-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-buffer-kind 2 driver-buffer-op 2 driver-buffer-lba 0 driver-buffer-blocks 1 driver-buffer-read-bytes 2048 driver-buffer-page-bytes 4096 driver-buffer-offset 0 driver-buffer-checksum 0x76EFDDC5 driver-buffer-zeroed 1 driver-buffer-read-ready 1 driver-buffer-mmio-written 0 driver-buffer-port-programmed 0 driver-buffer-published 0 driver-buffer-command-issued 0 driver-buffer-dma 0 driver-buffer-armed 0 driver-buffer-media-read 0 driver-buffer-staged 1 driver-buffer-denials 1 driver-buffer-unavailable 0 denied-driver-gate 0xFFFFFFFF driver-gate 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-gate-state 3 driver-gate-flags 0x000FFFFF driver-gate-token 0x(?!00000000)[0-9A-F]{8} driver-gate-buffer-token 0x(?!00000000)[0-9A-F]{8} driver-gate-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-gate-owner 0x00001006 driver-gate-owner-bound 1 driver-gate-query-only 1 driver-gate-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-gate-kind 2 driver-gate-op 2 driver-gate-lba 0 driver-gate-blocks 1 driver-gate-read-bytes 2048 driver-gate-page-bytes 4096 driver-gate-checksum 0x76EFDDC5 driver-gate-zeroed 1 driver-gate-read-ready 1 driver-gate-exec-required 1 driver-gate-exec-granted 0 driver-gate-issue-allowed 0 driver-gate-mmio-written 0 driver-gate-port-programmed 0 driver-gate-published 0 driver-gate-command-issued 0 driver-gate-dma 0 driver-gate-armed 0 driver-gate-media-read 0 driver-gate-staged 1 driver-gate-denials 1 driver-gate-unavailable 0 denied-driver-exec 0xFFFFFFFF driver-exec 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-exec-state 3 driver-exec-flags 0x007FFFFF driver-exec-token 0x(?!00000000)[0-9A-F]{8} driver-exec-gate-token 0x(?!00000000)[0-9A-F]{8} driver-exec-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-exec-owner 0x00001006 driver-exec-owner-bound 1 driver-exec-query-only 1 driver-exec-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-exec-kind 2 driver-exec-op 2 driver-exec-lba 0 driver-exec-blocks 1 driver-exec-read-bytes 2048 driver-exec-page-bytes 4096 driver-exec-checksum 0x76EFDDC5 driver-exec-zeroed 1 driver-exec-read-ready 1 driver-exec-attempted 1 driver-exec-required 1 driver-exec-granted 0 driver-exec-issue-allowed 0 driver-exec-issue-denied 1 driver-exec-mmio-written 0 driver-exec-port-programmed 0 driver-exec-published 0 driver-exec-command-issued 0 driver-exec-dma 0 driver-exec-armed 0 driver-exec-media-read 0 driver-exec-staged 1 driver-exec-denials 1 driver-exec-unavailable 0 denied-driver-result 0xFFFFFFFF driver-result 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-result-state 3 driver-result-flags 0x00FFFFFF driver-result-token 0x(?!00000000)[0-9A-F]{8} driver-result-exec-token 0x(?!00000000)[0-9A-F]{8} driver-result-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-result-owner 0x00001006 driver-result-owner-bound 1 driver-result-query-only 1 driver-result-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-result-kind 2 driver-result-op 2 driver-result-lba 0 driver-result-blocks 1 driver-result-read-bytes 2048 driver-result-page-bytes 4096 driver-result-checksum 0x76EFDDC5 driver-result-zeroed 1 driver-result-read-ready 1 driver-result-exec-denied 1 driver-result-requested 1 driver-result-granted 0 driver-result-denied 1 driver-result-bytes-available 0 driver-result-block-cap-minted 0 driver-result-fs-minted 0 driver-result-mmio-written 0 driver-result-port-programmed 0 driver-result-published 0 driver-result-command-issued 0 driver-result-dma 0 driver-result-armed 0 driver-result-media-read 0 driver-result-staged 1 driver-result-denials 1 driver-result-unavailable 0 map-requests 1 .* queries 359 denials 26'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiRevokePattern -Message "x64 UEFI brokered MMIO AHCI publish revocation-plan proof was not observed."
    $uefiDriverPublishPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-publish 0xFFFFFFFF driver-publish 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-publish-state 3 driver-publish-flags 0x07FFFFFF ' +
        'driver-publish-token 0x(?!00000000)[0-9A-F]{8} driver-publish-result-token 0x(?!00000000)[0-9A-F]{8} driver-publish-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-publish-owner 0x00001006 driver-publish-owner-bound 1 driver-publish-query-only 1 driver-publish-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-publish-kind 2 driver-publish-op 2 ' +
        'driver-publish-lba 0 driver-publish-blocks 1 driver-publish-read-bytes 2048 driver-publish-page-bytes 4096 driver-publish-checksum 0x76EFDDC5 driver-publish-zeroed 1 ' +
        'driver-publish-read-ready 1 driver-publish-exec-denied 1 driver-publish-result-denied 1 driver-publish-bytes-available 0 driver-publish-requested 1 driver-publish-granted 0 ' +
        'driver-publish-denied 1 driver-publish-block-endpoint 0 driver-publish-block-cap-minted 0 driver-publish-fs-minted 0 driver-publish-mmio-written 0 driver-publish-port-programmed 0 ' +
        'driver-publish-published 0 driver-publish-command-issued 0 driver-publish-dma 0 driver-publish-armed 0 driver-publish-media-read 0 driver-publish-media-written 0 ' +
        'driver-publish-staged 1 driver-publish-denials 1 driver-publish-unavailable 0 map-requests 1 .* queries 359 denials 27'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverPublishPattern -Message "x64 UEFI brokered MMIO AHCI driver block-publication denial proof was not observed."
    $uefiDriverReadGrantPattern = (
        '\[x64\] mmio planner service 9 .* denied-drg 0xFFFFFFFF drg 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drg-state 3 drg-flags 0x3FFFFFFF ' +
        'drg-token 0x(?!00000000)[0-9A-F]{8} drg-pub-token 0x(?!00000000)[0-9A-F]{8} drg-result-token 0x(?!00000000)[0-9A-F]{8} ' +
        'drg-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drg-owner 0x00001006 drg-owner-bound 1 drg-qonly 1 ' +
        'drg-port 0x(?!FFFFFFFF)[0-9A-F]{8} drg-kind 2 drg-op 2 drg-lba 0 drg-blocks 1 ' +
        'drg-read-bytes 2048 drg-page-bytes 4096 drg-checksum 0x76EFDDC5 drg-zeroed 1 drg-ready 1 ' +
        'drg-exec-denied 1 drg-result-denied 1 drg-pub-denied 1 drg-bytes 0 ' +
        'drg-requested 1 drg-granted 0 drg-denied 1 drg-media-auth 0 drg-block-endpoint 0 ' +
        'drg-block-cap 0 drg-fs-minted 0 drg-mmio-written 0 drg-port-programmed 0 drg-published 0 ' +
        'drg-command-issued 0 drg-dma 0 drg-armed 0 drg-media-read 0 drg-media-written 0 ' +
        'drg-staged 1 drg-denials 1 drg-unavailable 0 map-requests 1 .* queries 359 denials 28'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadGrantPattern -Message "x64 UEFI brokered MMIO AHCI driver read-authority denial proof was not observed."
    $uefiDriverMediaReadPattern = (
        '\[x64\] mmio planner service 9 .* denied-dmr 0xFFFFFFFF dmr 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dmr-state 3 dmr-flags 0x0FFFFFFF ' +
        'dmr-token 0x(?!00000000)[0-9A-F]{8} dmr-grant-token 0x(?!00000000)[0-9A-F]{8} dmr-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dmr-owner 0x00001006 dmr-owner-bound 1 dmr-qonly 1 dmr-port 0x(?!FFFFFFFF)[0-9A-F]{8} dmr-kind 2 dmr-op 2 ' +
        'dmr-lba 0 dmr-blocks 1 dmr-read-bytes 2048 dmr-page-bytes 4096 dmr-checksum 0x76EFDDC5 dmr-zeroed 1 ' +
        'dmr-ready 1 dmr-drg-denied 1 dmr-auth 0 dmr-attempted 1 dmr-denied 1 dmr-bytes 0 ' +
        'dmr-block-endpoint 0 dmr-block-cap 0 dmr-fs-minted 0 dmr-mmio-written 0 dmr-port-programmed 0 ' +
        'dmr-published 0 dmr-command-issued 0 dmr-dma 0 dmr-armed 0 dmr-media-read 0 dmr-media-written 0 ' +
        'dmr-buffer 1 dmr-staged 1 dmr-denials 1 dmr-unavailable 0 map-requests 1 .* queries 359 denials 29'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverMediaReadPattern -Message "x64 UEFI AHCI denied media-read consumption proof was not observed."
    $uefiDriverCompletePattern = (
        '\[x64\] mmio planner service 9 .* denied-drc 0xFFFFFFFF drc 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drc-state 3 drc-flags 0x1FFFFFFF ' +
        'drc-token 0x(?!00000000)[0-9A-F]{8} drc-dmr-token 0x(?!00000000)[0-9A-F]{8} drc-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drc-owner 0x00001006 drc-owner-bound 1 drc-qonly 1 drc-port 0x(?!FFFFFFFF)[0-9A-F]{8} drc-kind 2 drc-op 2 ' +
        'drc-lba 0 drc-blocks 1 drc-read-bytes 2048 drc-page-bytes 4096 drc-checksum 0x76EFDDC5 drc-zeroed 1 ' +
        'drc-ready 1 drc-dmr-denied 1 drc-requested 1 drc-granted 0 drc-denied 1 drc-completed 0 drc-status 0 ' +
        'drc-bytes 0 drc-block-endpoint 0 drc-block-cap 0 drc-fs-minted 0 drc-mmio-written 0 drc-port-programmed 0 ' +
        'drc-published 0 drc-command-issued 0 drc-dma 0 drc-armed 0 drc-media-read 0 drc-media-written 0 ' +
        'drc-buffer 1 drc-staged 1 drc-denials 1 drc-unavailable 0 map-requests 1 .* queries 359 denials 30'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverCompletePattern -Message "x64 UEFI AHCI denied read-completion proof was not observed."
    $uefiDriverReadCapPattern = (
        '\[x64\] mmio planner service 9 .* denied-drcap 0xFFFFFFFF drcap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drcap-state 3 drcap-flags 0x0FFFFFFF ' +
        'drcap-token 0x(?!00000000)[0-9A-F]{8} drcap-drc-token 0x(?!00000000)[0-9A-F]{8} drcap-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drcap-owner 0x00001006 drcap-owner-bound 1 drcap-qonly 1 drcap-port 0x(?!FFFFFFFF)[0-9A-F]{8} drcap-kind 2 drcap-op 2 ' +
        'drcap-lba 0 drcap-blocks 1 drcap-read-bytes 2048 drcap-page-bytes 4096 drcap-checksum 0x76EFDDC5 drcap-zeroed 1 ' +
        'drcap-ready 1 drcap-drc-denied 1 drcap-requested 1 drcap-granted 0 drcap-denied 1 drcap-bytes 0 ' +
        'drcap-block-endpoint 0 drcap-block-cap 0 drcap-fs-minted 0 drcap-mmio-written 0 drcap-port-programmed 0 ' +
        'drcap-published 0 drcap-command-issued 0 drcap-dma 0 drcap-armed 0 drcap-media-read 0 drcap-media-written 0 ' +
        'drcap-buffer 1 drcap-staged 1 drcap-denials 1 drcap-unavailable 0 map-requests 1 .* queries 359 denials 31'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadCapPattern -Message "x64 UEFI AHCI denied read-capability proof was not observed."
    $uefiDriverReadExportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drx 0xFFFFFFFF drx 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drx-state 3 drx-flags 0x3FFFFFFF ' +
        'drx-token 0x(?!00000000)[0-9A-F]{8} drx-drcap-token 0x(?!00000000)[0-9A-F]{8} drx-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drx-owner 0x00001006 drx-owner-bound 1 drx-qonly 1 drx-port 0x(?!FFFFFFFF)[0-9A-F]{8} drx-kind 2 drx-op 2 ' +
        'drx-lba 0 drx-blocks 1 drx-read-bytes 2048 drx-page-bytes 4096 drx-checksum 0x76EFDDC5 drx-zeroed 1 ' +
        'drx-ready 1 drx-drcap-denied 1 drx-requested 1 drx-granted 0 drx-denied 1 drx-bytes 0 ' +
        'drx-user-bytes 0 drx-user-buffer 0 drx-block-endpoint 0 drx-block-cap 0 drx-fs-minted 0 ' +
        'drx-mmio-written 0 drx-port-programmed 0 drx-published 0 drx-command-issued 0 drx-dma 0 drx-armed 0 ' +
        'drx-media-read 0 drx-media-written 0 drx-buffer 1 drx-staged 1 drx-denials 1 drx-unavailable 0 map-requests 1 .* queries 359 denials 32'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadExportPattern -Message "x64 UEFI AHCI denied read-export proof was not observed."
    $uefiDriverReadResponsePattern = (
        '\[x64\] mmio planner service 9 .* denied-drr 0xFFFFFFFF drr 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drr-state 3 drr-flags 0x3FFFFFFF ' +
        'drr-token 0x(?!00000000)[0-9A-F]{8} drr-drx-token 0x(?!00000000)[0-9A-F]{8} drr-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drr-owner 0x00001006 drr-owner-bound 1 drr-qonly 1 drr-port 0x(?!FFFFFFFF)[0-9A-F]{8} drr-kind 2 drr-op 2 ' +
        'drr-lba 0 drr-blocks 1 drr-read-bytes 2048 drr-page-bytes 4096 drr-checksum 0x76EFDDC5 drr-zeroed 1 ' +
        'drr-ready 1 drr-drx-denied 1 drr-requested 1 drr-granted 0 drr-denied 1 drr-bytes 0 ' +
        'drr-resp-bytes 0 drr-resp-status 0 drr-resp-checksum 0x00000000 drr-block-endpoint 0 drr-block-cap 0 drr-fs-minted 0 ' +
        'drr-mmio-written 0 drr-port-programmed 0 drr-published 0 drr-command-issued 0 drr-dma 0 drr-armed 0 ' +
        'drr-media-read 0 drr-media-written 0 drr-buffer 1 drr-staged 1 drr-denials 1 drr-unavailable 0 map-requests 1 .* queries 359 denials 33'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadResponsePattern -Message "x64 UEFI AHCI denied read-response proof was not observed."
    $uefiDriverReadDeliveryPattern = (
        '\[x64\] mmio planner service 9 .* denied-drd 0xFFFFFFFF drd 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drd-state 3 drd-flags 0x3FFFFFFF ' +
        'drd-token 0x(?!00000000)[0-9A-F]{8} drd-drr-token 0x(?!00000000)[0-9A-F]{8} drd-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drd-owner 0x00001006 drd-owner-bound 1 drd-qonly 1 drd-port 0x(?!FFFFFFFF)[0-9A-F]{8} drd-kind 2 drd-op 2 ' +
        'drd-lba 0 drd-blocks 1 drd-read-bytes 2048 drd-page-bytes 4096 drd-checksum 0x76EFDDC5 drd-zeroed 1 ' +
        'drd-ready 1 drd-drr-denied 1 drd-requested 1 drd-granted 0 drd-denied 1 drd-bytes 0 ' +
        'drd-deliv-bytes 0 drd-deliv-status 0 drd-deliv-checksum 0x00000000 drd-block-endpoint 0 drd-block-cap 0 drd-fs-minted 0 ' +
        'drd-mmio-written 0 drd-port-programmed 0 drd-published 0 drd-command-issued 0 drd-dma 0 drd-armed 0 ' +
        'drd-media-read 0 drd-media-written 0 drd-buffer 1 drd-staged 1 drd-denials 1 drd-unavailable 0 map-requests 1 .* queries 359 denials 34'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadDeliveryPattern -Message "x64 UEFI AHCI denied read-delivery proof was not observed."
    $uefiDriverReadVisiblePattern = (
        '\[x64\] mmio planner service 9 .* denied-drv 0xFFFFFFFF drv 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drv-state 3 drv-flags 0x3FFFFFFF ' +
        'drv-token 0x(?!00000000)[0-9A-F]{8} drv-drd-token 0x(?!00000000)[0-9A-F]{8} drv-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drv-owner 0x00001006 drv-owner-bound 1 drv-qonly 1 drv-port 0x(?!FFFFFFFF)[0-9A-F]{8} drv-kind 2 drv-op 2 ' +
        'drv-lba 0 drv-blocks 1 drv-read-bytes 2048 drv-page-bytes 4096 drv-checksum 0x76EFDDC5 drv-zeroed 1 ' +
        'drv-ready 1 drv-drd-denied 1 drv-requested 1 drv-granted 0 drv-denied 1 drv-bytes 0 ' +
        'drv-vis-bytes 0 drv-vis-status 0 drv-vis-checksum 0x00000000 drv-block-endpoint 0 drv-block-cap 0 drv-fs-minted 0 ' +
        'drv-mmio-written 0 drv-port-programmed 0 drv-published 0 drv-command-issued 0 drv-dma 0 drv-armed 0 ' +
        'drv-media-read 0 drv-media-written 0 drv-buffer 1 drv-staged 1 drv-denials 1 drv-unavailable 0 map-requests 1 .* queries 359 denials 35'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadVisiblePattern -Message "x64 UEFI AHCI denied read-visibility proof was not observed."
    $uefiDriverReadCommitPattern = (
        '\[x64\] mmio planner service 9 .* denied-drk 0xFFFFFFFF drk 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drk-state 3 drk-flags 0x3FFFFFFF ' +
        'drk-token 0x(?!00000000)[0-9A-F]{8} drk-drv-token 0x(?!00000000)[0-9A-F]{8} drk-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drk-owner 0x00001006 drk-owner-bound 1 drk-qonly 1 drk-port 0x(?!FFFFFFFF)[0-9A-F]{8} drk-kind 2 drk-op 2 ' +
        'drk-lba 0 drk-blocks 1 drk-read-bytes 2048 drk-page-bytes 4096 drk-checksum 0x76EFDDC5 drk-zeroed 1 ' +
        'drk-ready 1 drk-drv-denied 1 drk-requested 1 drk-granted 0 drk-denied 1 drk-bytes 0 ' +
        'drk-commit-bytes 0 drk-commit-status 0 drk-commit-checksum 0x00000000 drk-block-endpoint 0 drk-block-cap 0 drk-fs-minted 0 ' +
        'drk-mmio-written 0 drk-port-programmed 0 drk-published 0 drk-command-issued 0 drk-dma 0 drk-armed 0 ' +
        'drk-media-read 0 drk-media-written 0 drk-buffer 1 drk-staged 1 drk-denials 1 drk-unavailable 0 map-requests 1 .* queries 359 denials 36'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadCommitPattern -Message "x64 UEFI AHCI denied read-commit proof was not observed."
    $uefiDriverReadAuditPattern = (
        '\[x64\] mmio planner service 9 .* denied-dra 0xFFFFFFFF dra 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dra-state 3 dra-flags 0x3FFFFFFF ' +
        'dra-token 0x(?!00000000)[0-9A-F]{8} dra-drk-token 0x(?!00000000)[0-9A-F]{8} dra-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dra-owner 0x00001006 dra-owner-bound 1 dra-qonly 1 dra-port 0x(?!FFFFFFFF)[0-9A-F]{8} dra-kind 2 dra-op 2 ' +
        'dra-lba 0 dra-blocks 1 dra-read-bytes 2048 dra-page-bytes 4096 dra-checksum 0x76EFDDC5 dra-zeroed 1 ' +
        'dra-ready 1 dra-drk-denied 1 dra-requested 1 dra-granted 0 dra-denied 1 dra-bytes 0 ' +
        'dra-audit-bytes 0 dra-audit-status 0 dra-audit-checksum 0x00000000 dra-block-endpoint 0 dra-block-cap 0 dra-fs-minted 0 ' +
        'dra-mmio-written 0 dra-port-programmed 0 dra-published 0 dra-command-issued 0 dra-dma 0 dra-armed 0 ' +
        'dra-media-read 0 dra-media-written 0 dra-buffer 1 dra-staged 1 dra-denials 1 dra-unavailable 0 map-requests 1 .* queries 359 denials 37'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadAuditPattern -Message "x64 UEFI AHCI denied read-audit proof was not observed."
    $uefiDriverReadUpgradePattern = (
        '\[x64\] mmio planner service 9 .* denied-dru 0xFFFFFFFF dru 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dru-state 3 dru-flags 0x3FFFFFFF ' +
        'dru-token 0x(?!00000000)[0-9A-F]{8} dru-dra-token 0x(?!00000000)[0-9A-F]{8} dru-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dru-owner 0x00001006 dru-owner-bound 1 dru-qonly 1 dru-port 0x(?!FFFFFFFF)[0-9A-F]{8} dru-kind 2 dru-op 2 ' +
        'dru-lba 0 dru-blocks 1 dru-read-bytes 2048 dru-page-bytes 4096 dru-checksum 0x76EFDDC5 dru-zeroed 1 ' +
        'dru-ready 1 dru-dra-denied 1 dru-requested 1 dru-granted 0 dru-denied 1 dru-bytes 0 ' +
        'dru-up-cap 0xFFFFFFFF dru-media-auth 0 dru-exec-auth 0 dru-block-endpoint 0 dru-block-cap 0 dru-fs-minted 0 ' +
        'dru-mmio-written 0 dru-port-programmed 0 dru-published 0 dru-command-issued 0 dru-dma 0 dru-armed 0 ' +
        'dru-media-read 0 dru-media-written 0 dru-buffer 1 dru-staged 1 dru-denials 1 dru-unavailable 0 map-requests 1 .* queries 359 denials 38'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadUpgradePattern -Message "x64 UEFI AHCI denied read-upgrade proof was not observed."
    $uefiDriverReadActivatePattern = (
        '\[x64\] mmio planner service 9 .* denied-dact 0xFFFFFFFF dact 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dact-state 3 dact-flags 0x3FFFFFFF ' +
        'dact-token 0x(?!00000000)[0-9A-F]{8} dact-dru-token 0x(?!00000000)[0-9A-F]{8} dact-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dact-owner 0x00001006 dact-owner-bound 1 dact-qonly 1 dact-port 0x(?!FFFFFFFF)[0-9A-F]{8} dact-kind 2 dact-op 2 ' +
        'dact-lba 0 dact-blocks 1 dact-read-bytes 2048 dact-page-bytes 4096 dact-checksum 0x76EFDDC5 dact-zeroed 1 ' +
        'dact-ready 1 dact-dru-denied 1 dact-requested 1 dact-granted 0 dact-denied 1 dact-bytes 0 ' +
        'dact-act-cap 0xFFFFFFFF dact-read-auth 0 dact-exec-auth 0 dact-block-endpoint 0 dact-block-cap 0 dact-fs-minted 0 ' +
        'dact-mmio-written 0 dact-port-programmed 0 dact-published 0 dact-command-issued 0 dact-dma 0 dact-armed 0 ' +
        'dact-media-read 0 dact-media-written 0 dact-buffer 1 dact-staged 1 dact-denials 1 dact-unavailable 0 map-requests 1 .* queries 359 denials 39'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadActivatePattern -Message "x64 UEFI AHCI denied read-activation proof was not observed."
    $uefiDriverReadArmPattern = (
        '\[x64\] mmio planner service 9 .* denied-darm 0xFFFFFFFF darm 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} darm-state 3 darm-flags 0x3FFFFFFF ' +
        'darm-token 0x(?!00000000)[0-9A-F]{8} darm-dact-token 0x(?!00000000)[0-9A-F]{8} darm-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'darm-owner 0x00001006 darm-owner-bound 1 darm-qonly 1 darm-port 0x(?!FFFFFFFF)[0-9A-F]{8} darm-kind 2 darm-op 2 ' +
        'darm-lba 0 darm-blocks 1 darm-read-bytes 2048 darm-page-bytes 4096 darm-checksum 0x76EFDDC5 darm-zeroed 1 ' +
        'darm-ready 1 darm-dact-denied 1 darm-requested 1 darm-granted 0 darm-denied 1 darm-bytes 0 ' +
        'darm-arm-cap 0xFFFFFFFF darm-read-auth 0 darm-exec-auth 0 darm-block-endpoint 0 darm-block-cap 0 darm-fs-minted 0 ' +
        'darm-mmio-written 0 darm-port-programmed 0 darm-published 0 darm-command-issued 0 darm-dma 0 darm-armed 0 ' +
        'darm-media-read 0 darm-media-written 0 darm-buffer 1 darm-staged 1 darm-denials 1 darm-unavailable 0 map-requests 1 .* queries 359 denials 40'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadArmPattern -Message "x64 UEFI AHCI denied read-arm proof was not observed."
    $uefiDriverReadSubmitPattern = (
        '\[x64\] mmio planner service 9 .* denied-dsub 0xFFFFFFFF dsub 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dsub-state 3 dsub-flags 0x3FFFFFFF ' +
        'dsub-token 0x(?!00000000)[0-9A-F]{8} dsub-darm-token 0x(?!00000000)[0-9A-F]{8} dsub-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dsub-owner 0x00001006 dsub-owner-bound 1 dsub-qonly 1 dsub-port 0x(?!FFFFFFFF)[0-9A-F]{8} dsub-kind 2 dsub-op 2 ' +
        'dsub-lba 0 dsub-blocks 1 dsub-read-bytes 2048 dsub-page-bytes 4096 dsub-checksum 0x76EFDDC5 dsub-zeroed 1 ' +
        'dsub-ready 1 dsub-darm-denied 1 dsub-requested 1 dsub-granted 0 dsub-denied 1 dsub-bytes 0 ' +
        'dsub-submit-cap 0xFFFFFFFF dsub-read-auth 0 dsub-exec-auth 0 dsub-block-endpoint 0 dsub-block-cap 0 dsub-fs-minted 0 ' +
        'dsub-mmio-written 0 dsub-port-programmed 0 dsub-published 0 dsub-command-issued 0 dsub-dma 0 dsub-armed 0 ' +
        'dsub-media-read 0 dsub-media-written 0 dsub-buffer 1 dsub-staged 1 dsub-denials 1 dsub-unavailable 0 map-requests 1 .* queries 359 denials 41'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadSubmitPattern -Message "x64 UEFI AHCI denied read-submit proof was not observed."
    $uefiDriverReadObservePattern = (
        '\[x64\] mmio planner service 9 .* denied-dobs 0xFFFFFFFF dobs 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dobs-state 3 dobs-flags 0x3FFFFFFF ' +
        'dobs-token 0x(?!00000000)[0-9A-F]{8} dobs-dsub-token 0x(?!00000000)[0-9A-F]{8} dobs-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dobs-owner 0x00001006 dobs-owner-bound 1 dobs-qonly 1 dobs-port 0x(?!FFFFFFFF)[0-9A-F]{8} dobs-kind 2 dobs-op 2 ' +
        'dobs-lba 0 dobs-blocks 1 dobs-read-bytes 2048 dobs-page-bytes 4096 dobs-checksum 0x76EFDDC5 dobs-zeroed 1 ' +
        'dobs-ready 1 dobs-dsub-denied 1 dobs-requested 1 dobs-granted 0 dobs-denied 1 dobs-bytes 0 ' +
        'dobs-obs-status 0 dobs-obs-bytes 0 dobs-obs-checksum 0x00000000 dobs-read-auth 0 dobs-exec-auth 0 ' +
        'dobs-block-endpoint 0 dobs-block-cap 0 dobs-fs-minted 0 dobs-mmio-written 0 dobs-port-programmed 0 dobs-published 0 ' +
        'dobs-command-issued 0 dobs-dma 0 dobs-armed 0 dobs-media-read 0 dobs-media-written 0 dobs-buffer 1 ' +
        'dobs-staged 1 dobs-denials 1 dobs-unavailable 0 map-requests 1 .* queries 359 denials 42'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadObservePattern -Message "x64 UEFI AHCI denied read-observe proof was not observed."
    $uefiDriverReadRetirePattern = (
        '\[x64\] mmio planner service 9 .* denied-dret 0xFFFFFFFF dret 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dret-state 3 dret-flags 0x3FFFFFFF ' +
        'dret-token 0x(?!00000000)[0-9A-F]{8} dret-dobs-token 0x(?!00000000)[0-9A-F]{8} dret-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dret-owner 0x00001006 dret-owner-bound 1 dret-qonly 1 dret-port 0x(?!FFFFFFFF)[0-9A-F]{8} dret-kind 2 dret-op 2 ' +
        'dret-lba 0 dret-blocks 1 dret-read-bytes 2048 dret-page-bytes 4096 dret-checksum 0x76EFDDC5 dret-zeroed 1 ' +
        'dret-ready 1 dret-dobs-denied 1 dret-requested 1 dret-granted 0 dret-denied 1 dret-bytes 0 ' +
        'dret-ret-status 0 dret-ret-bytes 0 dret-ret-checksum 0x00000000 dret-read-auth 0 dret-exec-auth 0 ' +
        'dret-block-endpoint 0 dret-block-cap 0 dret-fs-minted 0 dret-mmio-written 0 dret-port-programmed 0 dret-published 0 ' +
        'dret-command-issued 0 dret-dma 0 dret-armed 0 dret-media-read 0 dret-media-written 0 dret-buffer 1 ' +
        'dret-staged 1 dret-denials 1 dret-unavailable 0 map-requests 1 .* queries 359 denials 43'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadRetirePattern -Message "x64 UEFI AHCI denied read-retire proof was not observed."
    $uefiDriverReadPermitPattern = (
        '\[x64\] mmio planner service 9 .* denied-dprm 0xFFFFFFFF dprm 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dprm-state 3 dprm-flags 0x3FFFFFFF ' +
        'dprm-token 0x(?!00000000)[0-9A-F]{8} dprm-dret-token 0x(?!00000000)[0-9A-F]{8} dprm-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dprm-owner 0x00001006 dprm-owner-bound 1 dprm-qonly 1 dprm-port 0x(?!FFFFFFFF)[0-9A-F]{8} dprm-kind 2 dprm-op 2 ' +
        'dprm-lba 0 dprm-blocks 1 dprm-read-bytes 2048 dprm-page-bytes 4096 dprm-checksum 0x76EFDDC5 dprm-zeroed 1 ' +
        'dprm-ready 1 dprm-dret-denied 1 dprm-requested 1 dprm-granted 0 dprm-denied 1 dprm-bytes 0 ' +
        'dprm-permit-cap 0xFFFFFFFF dprm-read-auth 0 dprm-exec-auth 0 dprm-block-endpoint 0 dprm-block-cap 0 ' +
        'dprm-fs-minted 0 dprm-mmio-written 0 dprm-port-programmed 0 dprm-published 0 dprm-command-issued 0 dprm-dma 0 ' +
        'dprm-armed 0 dprm-media-read 0 dprm-media-written 0 dprm-buffer 1 dprm-staged 1 dprm-denials 1 ' +
        'dprm-unavailable 0 map-requests 1 .* queries 359 denials 44'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadPermitPattern -Message "x64 UEFI AHCI denied read-permit proof was not observed."
    $uefiDriverReadWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-dwin 0xFFFFFFFF dwin 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dwin-state 3 dwin-flags 0x3FFFFFFF ' +
        'dwin-token 0x(?!00000000)[0-9A-F]{8} dwin-dprm-token 0x(?!00000000)[0-9A-F]{8} dwin-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dwin-owner 0x00001006 dwin-owner-bound 1 dwin-qonly 1 dwin-port 0x(?!FFFFFFFF)[0-9A-F]{8} dwin-kind 2 dwin-op 2 ' +
        'dwin-lba 0 dwin-blocks 1 dwin-read-bytes 2048 dwin-page-bytes 4096 dwin-checksum 0x76EFDDC5 dwin-zeroed 1 ' +
        'dwin-ready 1 dwin-dprm-denied 1 dwin-requested 1 dwin-granted 0 dwin-denied 1 dwin-bytes 0 ' +
        'dwin-window-cap 0xFFFFFFFF dwin-open 0 dwin-read-auth 0 dwin-exec-auth 0 dwin-block-endpoint 0 dwin-block-cap 0 ' +
        'dwin-fs-minted 0 dwin-mmio-written 0 dwin-port-programmed 0 dwin-published 0 dwin-command-issued 0 dwin-dma 0 ' +
        'dwin-armed 0 dwin-media-read 0 dwin-media-written 0 dwin-buffer 1 dwin-staged 1 dwin-denials 1 ' +
        'dwin-unavailable 0 map-requests 1 .* queries 359 denials 45'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadWindowPattern -Message "x64 UEFI AHCI denied read-window proof was not observed."
    $uefiDriverReadLeasePattern = (
        '\[x64\] mmio planner service 9 .* denied-dlse 0xFFFFFFFF dlse 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dlse-state 3 dlse-flags 0x3FFFFFFF ' +
        'dlse-token 0x(?!00000000)[0-9A-F]{8} dlse-dwin-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} dlse-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'dlse-owner 0x00001006 dlse-owner-bound 1 dlse-qonly 1 dlse-port 0x(?!FFFFFFFF)[0-9A-F]{8} dlse-kind 2 dlse-op 2 ' +
        'dlse-lba 0 dlse-blocks 1 dlse-read-bytes 2048 dlse-page-bytes 4096 dlse-checksum 0x76EFDDC5 dlse-zeroed 1 ' +
        'dlse-ready 1 dlse-dwin-denied 1 dlse-requested 1 dlse-granted 0 dlse-denied 1 dlse-bytes 0 ' +
        'dlse-lease-cap 0xFFFFFFFF dlse-active 0 dlse-read-auth 0 dlse-exec-auth 0 dlse-block-endpoint 0 dlse-block-cap 0 ' +
        'dlse-fs-minted 0 dlse-mmio-written 0 dlse-port-programmed 0 dlse-published 0 dlse-command-issued 0 dlse-dma 0 ' +
        'dlse-armed 0 dlse-media-read 0 dlse-media-written 0 dlse-buffer 1 dlse-staged 1 dlse-denials 1 ' +
        'dlse-unavailable 0 map-requests 1 .* queries 359 denials 46'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadLeasePattern -Message "x64 UEFI AHCI denied read-lease proof was not observed."
    $uefiDriverReadUsePattern = (
        '\[x64\] mmio planner service 9 .* denied-duse 0xFFFFFFFF duse 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} duse-state 3 duse-flags 0x3FFFFFFF ' +
        'duse-token 0x(?!00000000)[0-9A-F]{8} duse-dlse-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} duse-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'duse-owner 0x00001006 duse-owner-bound 1 duse-qonly 1 duse-port 0x(?!FFFFFFFF)[0-9A-F]{8} duse-kind 2 duse-op 2 ' +
        'duse-lba 0 duse-blocks 1 duse-read-bytes 2048 duse-page-bytes 4096 duse-checksum 0x76EFDDC5 duse-zeroed 1 ' +
        'duse-ready 1 duse-dlse-denied 1 duse-requested 1 duse-granted 0 duse-denied 1 duse-bytes 0 ' +
        'duse-use-cap 0xFFFFFFFF duse-active 0 duse-read-auth 0 duse-exec-auth 0 duse-block-endpoint 0 duse-block-cap 0 ' +
        'duse-fs-minted 0 duse-mmio-written 0 duse-port-programmed 0 duse-published 0 duse-command-issued 0 duse-dma 0 ' +
        'duse-armed 0 duse-media-read 0 duse-media-written 0 duse-buffer 1 duse-staged 1 duse-denials 1 ' +
        'duse-unavailable 0 map-requests 1 .* queries 359 denials 47'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadUsePattern -Message "x64 UEFI AHCI denied read-use proof was not observed."
    $uefiDriverReadReportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drpt 0xFFFFFFFF drpt 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drpt-state 3 drpt-flags 0x3FFFFFFF ' +
        'drpt-token 0x(?!00000000)[0-9A-F]{8} drpt-duse-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drpt-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drpt-owner 0x00001006 drpt-owner-bound 1 drpt-qonly 1 drpt-port 0x(?!FFFFFFFF)[0-9A-F]{8} drpt-kind 2 drpt-op 2 ' +
        'drpt-lba 0 drpt-blocks 1 drpt-read-bytes 2048 drpt-page-bytes 4096 drpt-checksum 0x76EFDDC5 drpt-zeroed 1 ' +
        'drpt-ready 1 drpt-duse-denied 1 drpt-requested 1 drpt-granted 0 drpt-denied 1 drpt-bytes 0 ' +
        'drpt-status 0 drpt-report-bytes 0 drpt-report-checksum 0x00000000 drpt-report-cap 0xFFFFFFFF drpt-read-auth 0 ' +
        'drpt-exec-auth 0 drpt-block-endpoint 0 drpt-block-cap 0 drpt-fs-minted 0 drpt-mmio-written 0 ' +
        'drpt-port-programmed 0 drpt-published 0 drpt-command-issued 0 drpt-dma 0 drpt-armed 0 ' +
        'drpt-media-read 0 drpt-media-written 0 drpt-buffer 1 drpt-staged 1 drpt-denials 1 ' +
        'drpt-unavailable 0 map-requests 1 .* queries 359 denials 48'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadReportPattern -Message "x64 UEFI AHCI denied read-report proof was not observed."
    $uefiDriverReadReceiptPattern = (
        '\[x64\] mmio planner service 9 .* denied-drrc 0xFFFFFFFF drrc 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drrc-state 3 drrc-flags 0x3FFFFFFF ' +
        'drrc-token 0x(?!00000000)[0-9A-F]{8} drrc-drpt-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drrc-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drrc-owner 0x00001006 drrc-owner-bound 1 drrc-qonly 1 drrc-port 0x(?!FFFFFFFF)[0-9A-F]{8} drrc-kind 2 drrc-op 2 ' +
        'drrc-lba 0 drrc-blocks 1 drrc-read-bytes 2048 drrc-page-bytes 4096 drrc-checksum 0x76EFDDC5 drrc-zeroed 1 ' +
        'drrc-ready 1 drrc-drpt-denied 1 drrc-requested 1 drrc-granted 0 drrc-denied 1 drrc-bytes 0 ' +
        'drrc-status 0 drrc-receipt-bytes 0 drrc-receipt-checksum 0x00000000 drrc-receipt-cap 0xFFFFFFFF drrc-read-auth 0 ' +
        'drrc-exec-auth 0 drrc-block-endpoint 0 drrc-block-cap 0 drrc-fs-minted 0 drrc-mmio-written 0 ' +
        'drrc-port-programmed 0 drrc-published 0 drrc-command-issued 0 drrc-dma 0 drrc-armed 0 ' +
        'drrc-media-read 0 drrc-media-written 0 drrc-buffer 1 drrc-staged 1 drrc-denials 1 ' +
        'drrc-unavailable 0 map-requests 1 .* queries 359 denials 49'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadReceiptPattern -Message "x64 UEFI AHCI denied read-receipt proof was not observed."
    $uefiDriverReadAckPattern = (
        '\[x64\] mmio planner service 9 .* denied-drak 0xFFFFFFFF drak 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drak-state 3 drak-flags 0x3FFFFFFF ' +
        'drak-token 0x(?!00000000)[0-9A-F]{8} drak-drrc-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drak-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drak-owner 0x00001006 drak-owner-bound 1 drak-qonly 1 drak-port 0x(?!FFFFFFFF)[0-9A-F]{8} drak-kind 2 drak-op 2 ' +
        'drak-lba 0 drak-blocks 1 drak-read-bytes 2048 drak-page-bytes 4096 drak-checksum 0x76EFDDC5 drak-zeroed 1 ' +
        'drak-ready 1 drak-drrc-denied 1 drak-requested 1 drak-granted 0 drak-denied 1 drak-bytes 0 ' +
        'drak-status 0 drak-ack-bytes 0 drak-ack-checksum 0x00000000 drak-ack-cap 0xFFFFFFFF drak-read-auth 0 ' +
        'drak-exec-auth 0 drak-block-endpoint 0 drak-block-cap 0 drak-fs-minted 0 drak-mmio-written 0 ' +
        'drak-port-programmed 0 drak-published 0 drak-command-issued 0 drak-dma 0 drak-armed 0 ' +
        'drak-media-read 0 drak-media-written 0 drak-buffer 1 drak-staged 1 drak-denials 1 ' +
        'drak-unavailable 0 map-requests 1 .* queries 359 denials 50'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadAckPattern -Message "x64 UEFI AHCI denied read-ack proof was not observed."
    $uefiDriverReadClosePattern = (
        '\[x64\] mmio planner service 9 .* denied-drcl 0xFFFFFFFF drcl 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drcl-state 3 drcl-flags 0x3FFFFFFF ' +
        'drcl-token 0x(?!00000000)[0-9A-F]{8} drcl-drak-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drcl-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drcl-owner 0x00001006 drcl-owner-bound 1 drcl-qonly 1 drcl-port 0x(?!FFFFFFFF)[0-9A-F]{8} drcl-kind 2 drcl-op 2 ' +
        'drcl-lba 0 drcl-blocks 1 drcl-read-bytes 2048 drcl-page-bytes 4096 drcl-checksum 0x76EFDDC5 drcl-zeroed 1 ' +
        'drcl-ready 1 drcl-drak-denied 1 drcl-requested 1 drcl-granted 0 drcl-denied 1 drcl-bytes 0 ' +
        'drcl-status 0 drcl-close-bytes 0 drcl-close-checksum 0x00000000 drcl-close-cap 0xFFFFFFFF drcl-read-auth 0 ' +
        'drcl-exec-auth 0 drcl-block-endpoint 0 drcl-block-cap 0 drcl-fs-minted 0 drcl-mmio-written 0 ' +
        'drcl-port-programmed 0 drcl-published 0 drcl-command-issued 0 drcl-dma 0 drcl-armed 0 ' +
        'drcl-media-read 0 drcl-media-written 0 drcl-buffer 1 drcl-staged 1 drcl-denials 1 ' +
        'drcl-unavailable 0 map-requests 1 .* queries 359 denials 51'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadClosePattern -Message "x64 UEFI AHCI denied read-close proof was not observed."
    $uefiDriverReadSealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drsl 0xFFFFFFFF drsl 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drsl-state 3 drsl-flags 0x3FFFFFFF ' +
        'drsl-token 0x(?!00000000)[0-9A-F]{8} drsl-drcl-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drsl-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drsl-owner 0x00001006 drsl-owner-bound 1 drsl-qonly 1 drsl-port 0x(?!FFFFFFFF)[0-9A-F]{8} drsl-kind 2 drsl-op 2 ' +
        'drsl-lba 0 drsl-blocks 1 drsl-read-bytes 2048 drsl-page-bytes 4096 drsl-checksum 0x76EFDDC5 drsl-zeroed 1 ' +
        'drsl-ready 1 drsl-drcl-denied 1 drsl-requested 1 drsl-granted 0 drsl-denied 1 drsl-bytes 0 ' +
        'drsl-status 0 drsl-seal-bytes 0 drsl-seal-checksum 0x00000000 drsl-seal-cap 0xFFFFFFFF drsl-read-auth 0 ' +
        'drsl-exec-auth 0 drsl-block-endpoint 0 drsl-block-cap 0 drsl-fs-minted 0 drsl-mmio-written 0 ' +
        'drsl-port-programmed 0 drsl-published 0 drsl-command-issued 0 drsl-dma 0 drsl-armed 0 ' +
        'drsl-media-read 0 drsl-media-written 0 drsl-buffer 1 drsl-staged 1 drsl-denials 1 ' +
        'drsl-unavailable 0 map-requests 1 .* queries 359 denials 52'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadSealPattern -Message "x64 UEFI AHCI denied read-seal proof was not observed."
    $uefiDriverReadUnsealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drul 0xFFFFFFFF drul 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drul-state 3 drul-flags 0x3FFFFFFF ' +
        'drul-token 0x(?!00000000)[0-9A-F]{8} drul-drsl-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drul-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drul-owner 0x00001006 drul-owner-bound 1 drul-qonly 1 drul-port 0x(?!FFFFFFFF)[0-9A-F]{8} drul-kind 2 drul-op 2 ' +
        'drul-lba 0 drul-blocks 1 drul-read-bytes 2048 drul-page-bytes 4096 drul-checksum 0x76EFDDC5 drul-zeroed 1 ' +
        'drul-ready 1 drul-drsl-denied 1 drul-requested 1 drul-granted 0 drul-denied 1 drul-bytes 0 ' +
        'drul-status 0 drul-unseal-bytes 0 drul-unseal-checksum 0x00000000 drul-unseal-cap 0xFFFFFFFF drul-read-auth 0 ' +
        'drul-exec-auth 0 drul-block-endpoint 0 drul-block-cap 0 drul-fs-minted 0 drul-mmio-written 0 ' +
        'drul-port-programmed 0 drul-published 0 drul-command-issued 0 drul-dma 0 drul-armed 0 ' +
        'drul-media-read 0 drul-media-written 0 drul-buffer 1 drul-staged 1 drul-denials 1 ' +
        'drul-unavailable 0 map-requests 1 .* queries 359 denials 53'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadUnsealPattern -Message "x64 UEFI AHCI denied read-unseal proof was not observed."
    $uefiDriverReadDiscardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drdc 0xFFFFFFFF drdc 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drdc-state 3 drdc-flags 0x3FFFFFFF ' +
        'drdc-token 0x(?!00000000)[0-9A-F]{8} drdc-drul-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drdc-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drdc-owner 0x00001006 drdc-owner-bound 1 drdc-qonly 1 drdc-port 0x(?!FFFFFFFF)[0-9A-F]{8} drdc-kind 2 drdc-op 2 ' +
        'drdc-lba 0 drdc-blocks 1 drdc-read-bytes 2048 drdc-page-bytes 4096 drdc-checksum 0x76EFDDC5 drdc-zeroed 1 ' +
        'drdc-ready 1 drdc-drul-denied 1 drdc-requested 1 drdc-granted 0 drdc-denied 1 drdc-bytes 0 ' +
        'drdc-status 0 drdc-discard-bytes 0 drdc-discard-checksum 0x00000000 drdc-discard-cap 0xFFFFFFFF drdc-read-auth 0 ' +
        'drdc-exec-auth 0 drdc-block-endpoint 0 drdc-block-cap 0 drdc-fs-minted 0 drdc-mmio-written 0 ' +
        'drdc-port-programmed 0 drdc-published 0 drdc-command-issued 0 drdc-dma 0 drdc-armed 0 ' +
        'drdc-media-read 0 drdc-media-written 0 drdc-buffer 1 drdc-staged 1 drdc-denials 1 ' +
        'drdc-unavailable 0 map-requests 1 .* queries 359 denials 54'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadDiscardPattern -Message "x64 UEFI AHCI denied read-discard proof was not observed."
    $uefiDriverReadFinalizePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-finalize 0xFFFFFFFF driver-read-finalize 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-finalize-state 3 driver-read-finalize-flags 0x3FFFFFFF ' +
        'driver-read-finalize-token 0x(?!00000000)[0-9A-F]{8} driver-read-finalize-read-discard-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-finalize-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-finalize-owner 0x00001006 driver-read-finalize-owner-bound 1 driver-read-finalize-query-only 1 driver-read-finalize-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-finalize-kind 2 driver-read-finalize-op 2 ' +
        'driver-read-finalize-lba 0 driver-read-finalize-blocks 1 driver-read-finalize-read-bytes 2048 driver-read-finalize-page-bytes 4096 driver-read-finalize-checksum 0x76EFDDC5 driver-read-finalize-zeroed 1 ' +
        'driver-read-finalize-read-ready 1 driver-read-finalize-read-discard-denied 1 driver-read-finalize-requested 1 driver-read-finalize-granted 0 driver-read-finalize-denied 1 driver-read-finalize-bytes-available 0 ' +
        'driver-read-finalize-status 0 driver-read-finalize-finalized-bytes 0 driver-read-finalize-finalize-checksum 0x00000000 driver-read-finalize-finalize-cap 0xFFFFFFFF driver-read-finalize-read-authority 0 ' +
        'driver-read-finalize-execute-authority 0 driver-read-finalize-block-endpoint 0 driver-read-finalize-block-cap-minted 0 driver-read-finalize-fs-minted 0 driver-read-finalize-mmio-written 0 ' +
        'driver-read-finalize-port-programmed 0 driver-read-finalize-published 0 driver-read-finalize-command-issued 0 driver-read-finalize-dma 0 driver-read-finalize-armed 0 ' +
        'driver-read-finalize-media-read 0 driver-read-finalize-media-written 0 driver-read-finalize-buffer-unchanged 1 driver-read-finalize-staged 1 driver-read-finalize-denials 1 ' +
        'driver-read-finalize-unavailable 0 map-requests 1 .* queries 359 denials 55'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadFinalizePattern -Message "x64 UEFI AHCI denied read-finalize proof was not observed."
    $uefiDriverReadAuthorizePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-authorize 0xFFFFFFFF driver-read-authorize 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-authorize-state 3 driver-read-authorize-flags 0x3FFFFFFF ' +
        'driver-read-authorize-token 0x(?!00000000)[0-9A-F]{8} driver-read-authorize-read-finalize-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-authorize-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-authorize-owner 0x00001006 driver-read-authorize-owner-bound 1 driver-read-authorize-query-only 1 driver-read-authorize-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-authorize-kind 2 driver-read-authorize-op 2 ' +
        'driver-read-authorize-lba 0 driver-read-authorize-blocks 1 driver-read-authorize-read-bytes 2048 driver-read-authorize-page-bytes 4096 driver-read-authorize-checksum 0x76EFDDC5 driver-read-authorize-zeroed 1 ' +
        'driver-read-authorize-read-ready 1 driver-read-authorize-read-finalize-denied 1 driver-read-authorize-requested 1 driver-read-authorize-granted 0 driver-read-authorize-denied 1 driver-read-authorize-policy-grant 0 ' +
        'driver-read-authorize-issue-authority 0 driver-read-authorize-dma-authority 0 driver-read-authorize-media-read-authority 0 driver-read-authorize-write-authority 0 driver-read-authorize-commit-authority 0 ' +
        'driver-read-authorize-block-endpoint 0 driver-read-authorize-block-cap-minted 0 driver-read-authorize-fs-minted 0 driver-read-authorize-mmio-written 0 driver-read-authorize-port-programmed 0 ' +
        'driver-read-authorize-published 0 driver-read-authorize-command-issued 0 driver-read-authorize-dma 0 driver-read-authorize-armed 0 driver-read-authorize-media-read 0 driver-read-authorize-media-written 0 ' +
        'driver-read-authorize-buffer-unchanged 1 driver-read-authorize-staged 1 driver-read-authorize-denials 1 driver-read-authorize-unavailable 0 map-requests 1 .* queries 359 denials 56'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadAuthorizePattern -Message "x64 UEFI AHCI denied read-authorize proof was not observed."
    $uefiDriverReadDispatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-dispatch 0xFFFFFFFF driver-read-dispatch 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-dispatch-state 3 driver-read-dispatch-flags 0x3FFFFFFF ' +
        'driver-read-dispatch-token 0x(?!00000000)[0-9A-F]{8} driver-read-dispatch-read-authorize-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-dispatch-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-dispatch-owner 0x00001006 driver-read-dispatch-owner-bound 1 driver-read-dispatch-query-only 1 driver-read-dispatch-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-dispatch-kind 2 driver-read-dispatch-op 2 ' +
        'driver-read-dispatch-lba 0 driver-read-dispatch-blocks 1 driver-read-dispatch-read-bytes 2048 driver-read-dispatch-page-bytes 4096 driver-read-dispatch-checksum 0x76EFDDC5 driver-read-dispatch-zeroed 1 ' +
        'driver-read-dispatch-read-ready 1 driver-read-dispatch-read-authorize-denied 1 driver-read-dispatch-requested 1 driver-read-dispatch-granted 0 driver-read-dispatch-denied 1 driver-read-dispatch-policy-grant 0 ' +
        'driver-read-dispatch-dispatch-queued 0 driver-read-dispatch-queue-depth 0 driver-read-dispatch-issue-authority 0 driver-read-dispatch-dma-authority 0 driver-read-dispatch-media-read-authority 0 ' +
        'driver-read-dispatch-write-authority 0 driver-read-dispatch-commit-authority 0 driver-read-dispatch-block-endpoint 0 driver-read-dispatch-block-cap-minted 0 driver-read-dispatch-fs-minted 0 ' +
        'driver-read-dispatch-mmio-written 0 driver-read-dispatch-port-programmed 0 driver-read-dispatch-published 0 driver-read-dispatch-command-issued 0 driver-read-dispatch-dma 0 driver-read-dispatch-armed 0 ' +
        'driver-read-dispatch-media-read 0 driver-read-dispatch-media-written 0 driver-read-dispatch-buffer-unchanged 1 driver-read-dispatch-staged 1 driver-read-dispatch-denials 1 ' +
        'driver-read-dispatch-unavailable 0 map-requests 1 .* queries 359 denials 57'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadDispatchPattern -Message "x64 UEFI AHCI denied read-dispatch proof was not observed."
    $uefiDriverReadQueuePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-queue 0xFFFFFFFF driver-read-queue 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-queue-state 3 driver-read-queue-flags 0x3FFFFFFF ' +
        'driver-read-queue-token 0x(?!00000000)[0-9A-F]{8} driver-read-queue-read-dispatch-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-queue-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-queue-owner 0x00001006 driver-read-queue-owner-bound 1 driver-read-queue-query-only 1 driver-read-queue-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-queue-kind 2 driver-read-queue-op 2 ' +
        'driver-read-queue-lba 0 driver-read-queue-blocks 1 driver-read-queue-read-bytes 2048 driver-read-queue-page-bytes 4096 driver-read-queue-checksum 0x76EFDDC5 driver-read-queue-zeroed 1 ' +
        'driver-read-queue-read-ready 1 driver-read-queue-read-dispatch-denied 1 driver-read-queue-requested 1 driver-read-queue-granted 0 driver-read-queue-denied 1 driver-read-queue-policy-grant 0 ' +
        'driver-read-queue-queue-inserted 0 driver-read-queue-queue-depth 0 driver-read-queue-worker-wake 0 driver-read-queue-issue-authority 0 driver-read-queue-dma-authority 0 ' +
        'driver-read-queue-media-read-authority 0 driver-read-queue-write-authority 0 driver-read-queue-commit-authority 0 driver-read-queue-block-endpoint 0 driver-read-queue-block-cap-minted 0 ' +
        'driver-read-queue-fs-minted 0 driver-read-queue-mmio-written 0 driver-read-queue-port-programmed 0 driver-read-queue-published 0 driver-read-queue-command-issued 0 ' +
        'driver-read-queue-dma 0 driver-read-queue-armed 0 driver-read-queue-media-read 0 driver-read-queue-media-written 0 driver-read-queue-buffer-unchanged 1 driver-read-queue-staged 1 ' +
        'driver-read-queue-denials 1 driver-read-queue-unavailable 0 map-requests 1 .* queries 359 denials 58'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadQueuePattern -Message "x64 UEFI AHCI denied read-queue proof was not observed."
    $uefiDriverReadWorkerPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-worker 0xFFFFFFFF driver-read-worker 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-worker-state 3 driver-read-worker-flags 0x3FFFFFFF ' +
        'driver-read-worker-token 0x(?!00000000)[0-9A-F]{8} driver-read-worker-read-queue-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-worker-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-worker-owner 0x00001006 driver-read-worker-owner-bound 1 driver-read-worker-query-only 1 driver-read-worker-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-worker-kind 2 driver-read-worker-op 2 ' +
        'driver-read-worker-lba 0 driver-read-worker-blocks 1 driver-read-worker-read-bytes 2048 driver-read-worker-page-bytes 4096 driver-read-worker-checksum 0x76EFDDC5 driver-read-worker-zeroed 1 ' +
        'driver-read-worker-read-ready 1 driver-read-worker-read-queue-denied 1 driver-read-worker-requested 1 driver-read-worker-granted 0 driver-read-worker-denied 1 driver-read-worker-policy-grant 0 ' +
        'driver-read-worker-queue-inserted 0 driver-read-worker-queue-depth 0 driver-read-worker-worker-wake 0 driver-read-worker-worker-dequeued 0 driver-read-worker-issue-authority 0 ' +
        'driver-read-worker-dma-authority 0 driver-read-worker-media-read-authority 0 driver-read-worker-write-authority 0 driver-read-worker-commit-authority 0 driver-read-worker-block-endpoint 0 ' +
        'driver-read-worker-block-cap-minted 0 driver-read-worker-fs-minted 0 driver-read-worker-mmio-written 0 driver-read-worker-port-programmed 0 driver-read-worker-published 0 ' +
        'driver-read-worker-command-issued 0 driver-read-worker-dma 0 driver-read-worker-armed 0 driver-read-worker-media-read 0 driver-read-worker-media-written 0 ' +
        'driver-read-worker-buffer-unchanged 1 driver-read-worker-staged 1 driver-read-worker-denials 1 driver-read-worker-unavailable 0 map-requests 1 .* queries 359 denials 59'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadWorkerPattern -Message "x64 UEFI AHCI denied read-worker proof was not observed."
    $uefiDriverReadSchedulePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-schedule 0xFFFFFFFF driver-read-schedule 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-schedule-state 3 driver-read-schedule-flags 0x3FFFFFFF ' +
        'driver-read-schedule-token 0x(?!00000000)[0-9A-F]{8} driver-read-schedule-read-worker-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-schedule-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-schedule-owner 0x00001006 driver-read-schedule-owner-bound 1 driver-read-schedule-query-only 1 driver-read-schedule-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-schedule-kind 2 driver-read-schedule-op 2 ' +
        'driver-read-schedule-lba 0 driver-read-schedule-blocks 1 driver-read-schedule-read-bytes 2048 driver-read-schedule-page-bytes 4096 driver-read-schedule-checksum 0x76EFDDC5 driver-read-schedule-zeroed 1 ' +
        'driver-read-schedule-read-ready 1 driver-read-schedule-read-worker-denied 1 driver-read-schedule-requested 1 driver-read-schedule-granted 0 driver-read-schedule-denied 1 ' +
        'driver-read-schedule-policy-grant 0 driver-read-schedule-queue-inserted 0 driver-read-schedule-queue-depth 0 driver-read-schedule-worker-wake 0 driver-read-schedule-worker-dequeued 0 ' +
        'driver-read-schedule-worker-runnable 0 driver-read-schedule-worker-scheduled 0 driver-read-schedule-issue-authority 0 driver-read-schedule-dma-authority 0 ' +
        'driver-read-schedule-media-read-authority 0 driver-read-schedule-write-authority 0 driver-read-schedule-commit-authority 0 driver-read-schedule-block-endpoint 0 ' +
        'driver-read-schedule-block-cap-minted 0 driver-read-schedule-fs-minted 0 driver-read-schedule-mmio-written 0 driver-read-schedule-port-programmed 0 ' +
        'driver-read-schedule-published 0 driver-read-schedule-command-issued 0 driver-read-schedule-dma 0 driver-read-schedule-armed 0 driver-read-schedule-media-read 0 ' +
        'driver-read-schedule-media-written 0 driver-read-schedule-buffer-unchanged 1 driver-read-schedule-staged 1 driver-read-schedule-denials 1 ' +
        'driver-read-schedule-unavailable 0 map-requests 1 .* queries 359 denials 60'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadSchedulePattern -Message "x64 UEFI AHCI denied read-schedule proof was not observed."
    $uefiDriverReadRunPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-run 0xFFFFFFFF driver-read-run 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-run-state 3 driver-read-run-flags 0x3FFFFFFF ' +
        'driver-read-run-token 0x(?!00000000)[0-9A-F]{8} driver-read-run-read-schedule-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-run-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-run-owner 0x00001006 driver-read-run-owner-bound 1 driver-read-run-query-only 1 driver-read-run-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-run-kind 2 driver-read-run-op 2 ' +
        'driver-read-run-lba 0 driver-read-run-blocks 1 driver-read-run-read-bytes 2048 driver-read-run-page-bytes 4096 driver-read-run-checksum 0x76EFDDC5 driver-read-run-zeroed 1 ' +
        'driver-read-run-read-ready 1 driver-read-run-read-schedule-denied 1 driver-read-run-requested 1 driver-read-run-granted 0 driver-read-run-denied 1 driver-read-run-policy-grant 0 ' +
        'driver-read-run-queue-inserted 0 driver-read-run-queue-depth 0 driver-read-run-worker-wake 0 driver-read-run-worker-dequeued 0 driver-read-run-worker-runnable 0 ' +
        'driver-read-run-worker-scheduled 0 driver-read-run-worker-run 0 driver-read-run-worker-executed 0 driver-read-run-issue-authority 0 driver-read-run-dma-authority 0 ' +
        'driver-read-run-media-read-authority 0 driver-read-run-write-authority 0 driver-read-run-commit-authority 0 driver-read-run-block-endpoint 0 ' +
        'driver-read-run-block-cap-minted 0 driver-read-run-fs-minted 0 driver-read-run-mmio-written 0 driver-read-run-port-programmed 0 ' +
        'driver-read-run-published 0 driver-read-run-command-issued 0 driver-read-run-dma 0 driver-read-run-armed 0 driver-read-run-media-read 0 ' +
        'driver-read-run-media-written 0 driver-read-run-buffer-unchanged 1 driver-read-run-staged 1 driver-read-run-denials 1 ' +
        'driver-read-run-unavailable 0 map-requests 1 .* queries 359 denials 61'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadRunPattern -Message "x64 UEFI AHCI denied read-run proof was not observed."
    $uefiDriverReadBodyPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-body 0xFFFFFFFF driver-read-body 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-body-state 3 driver-read-body-flags 0x3FFFFFFF ' +
        'driver-read-body-token 0x(?!00000000)[0-9A-F]{8} driver-read-body-read-run-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-body-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-body-owner 0x00001006 driver-read-body-owner-bound 1 driver-read-body-query-only 1 driver-read-body-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-body-kind 2 driver-read-body-op 2 ' +
        'driver-read-body-lba 0 driver-read-body-blocks 1 driver-read-body-read-bytes 2048 driver-read-body-page-bytes 4096 driver-read-body-checksum 0x76EFDDC5 driver-read-body-zeroed 1 ' +
        'driver-read-body-read-ready 1 driver-read-body-read-run-denied 1 driver-read-body-requested 1 driver-read-body-granted 0 driver-read-body-denied 1 driver-read-body-policy-grant 0 ' +
        'driver-read-body-queue-inserted 0 driver-read-body-queue-depth 0 driver-read-body-worker-wake 0 driver-read-body-worker-dequeued 0 driver-read-body-worker-runnable 0 ' +
        'driver-read-body-worker-scheduled 0 driver-read-body-worker-run 0 driver-read-body-worker-executed 0 driver-read-body-body-entered 0 driver-read-body-body-completed 0 ' +
        'driver-read-body-issue-authority 0 driver-read-body-dma-authority 0 driver-read-body-media-read-authority 0 driver-read-body-write-authority 0 driver-read-body-commit-authority 0 ' +
        'driver-read-body-block-endpoint 0 driver-read-body-block-cap-minted 0 driver-read-body-fs-minted 0 driver-read-body-mmio-written 0 driver-read-body-port-programmed 0 ' +
        'driver-read-body-published 0 driver-read-body-command-issued 0 driver-read-body-dma 0 driver-read-body-armed 0 driver-read-body-media-read 0 ' +
        'driver-read-body-media-written 0 driver-read-body-buffer-unchanged 1 driver-read-body-staged 1 driver-read-body-denials 1 ' +
        'driver-read-body-unavailable 0 map-requests 1 .* queries 359 denials 62'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadBodyPattern -Message "x64 UEFI AHCI denied read-body proof was not observed."
    $uefiDriverReadIssuePattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-issue 0xFFFFFFFF driver-read-issue 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-issue-state 3 driver-read-issue-flags 0x3FFFFFFF ' +
        'driver-read-issue-token 0x(?!00000000)[0-9A-F]{8} driver-read-issue-read-body-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-issue-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-issue-owner 0x00001006 driver-read-issue-owner-bound 1 driver-read-issue-query-only 1 driver-read-issue-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-issue-kind 2 driver-read-issue-op 2 ' +
        'driver-read-issue-lba 0 driver-read-issue-blocks 1 driver-read-issue-read-bytes 2048 driver-read-issue-page-bytes 4096 driver-read-issue-checksum 0x76EFDDC5 driver-read-issue-zeroed 1 ' +
        'driver-read-issue-read-ready 1 driver-read-issue-read-body-denied 1 driver-read-issue-requested 1 driver-read-issue-granted 0 driver-read-issue-denied 1 driver-read-issue-policy-grant 0 ' +
        'driver-read-issue-queue-inserted 0 driver-read-issue-queue-depth 0 driver-read-issue-worker-wake 0 driver-read-issue-worker-dequeued 0 driver-read-issue-worker-runnable 0 ' +
        'driver-read-issue-worker-scheduled 0 driver-read-issue-worker-run 0 driver-read-issue-worker-executed 0 driver-read-issue-issue-entered 0 driver-read-issue-issue-completed 0 ' +
        'driver-read-issue-issue-authority 0 driver-read-issue-dma-authority 0 driver-read-issue-media-read-authority 0 driver-read-issue-write-authority 0 driver-read-issue-commit-authority 0 ' +
        'driver-read-issue-block-endpoint 0 driver-read-issue-block-cap-minted 0 driver-read-issue-fs-minted 0 driver-read-issue-mmio-written 0 driver-read-issue-port-programmed 0 ' +
        'driver-read-issue-published 0 driver-read-issue-command-issued 0 driver-read-issue-dma 0 driver-read-issue-armed 0 driver-read-issue-media-read 0 ' +
        'driver-read-issue-media-written 0 driver-read-issue-buffer-unchanged 1 driver-read-issue-staged 1 driver-read-issue-denials 1 ' +
        'driver-read-issue-unavailable 0 map-requests 1 .* queries 359 denials 63'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadIssuePattern -Message "x64 UEFI AHCI denied read-issue proof was not observed."
    $uefiDriverReadDmaPattern = (
        '\[x64\] mmio planner service 9 .* denied-driver-read-dma 0xFFFFFFFF driver-read-dma 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-dma-state 3 driver-read-dma-flags 0x3FFFFFFF ' +
        'driver-read-dma-token 0x(?!00000000)[0-9A-F]{8} driver-read-dma-read-issue-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} driver-read-dma-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'driver-read-dma-owner 0x00001006 driver-read-dma-owner-bound 1 driver-read-dma-query-only 1 driver-read-dma-port 0x(?!FFFFFFFF)[0-9A-F]{8} driver-read-dma-kind 2 driver-read-dma-op 2 ' +
        'driver-read-dma-lba 0 driver-read-dma-blocks 1 driver-read-dma-read-bytes 2048 driver-read-dma-page-bytes 4096 driver-read-dma-checksum 0x76EFDDC5 ' +
        'driver-read-dma-zeroed 1 driver-read-dma-read-ready 1 driver-read-dma-read-issue-denied 1 driver-read-dma-requested 1 driver-read-dma-granted 0 ' +
        'driver-read-dma-denied 1 driver-read-dma-policy-grant 0 driver-read-dma-bytes-available 0 driver-read-dma-window-cap 0xFFFFFFFF ' +
        'driver-read-dma-window-open 0 driver-read-dma-entered 0 driver-read-dma-completed 0 driver-read-dma-issue-authority 0 ' +
        'driver-read-dma-dma-authority 0 driver-read-dma-media-read-authority 0 driver-read-dma-write-authority 0 driver-read-dma-commit-authority 0 ' +
        'driver-read-dma-block-endpoint 0 driver-read-dma-block-cap-minted 0 driver-read-dma-fs-minted 0 driver-read-dma-mmio-written 0 ' +
        'driver-read-dma-port-programmed 0 driver-read-dma-published 0 driver-read-dma-command-issued 0 driver-read-dma-dma 0 driver-read-dma-armed 0 ' +
        'driver-read-dma-media-read 0 driver-read-dma-media-written 0 driver-read-dma-buffer-unchanged 1 driver-read-dma-staged 1 ' +
        'driver-read-dma-denials 1 driver-read-dma-unavailable 0 map-requests 1 .* queries 359 denials 64'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadDmaPattern -Message "x64 UEFI AHCI denied read-DMA proof was not observed."
    $uefiDriverReadIrqPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-irq 0xFFFFFFFF drs-irq 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-irq-state 3 drs-irq-flags 0x3FFFFFFF ' +
        'drs-irq-token 0x(?!00000000)[0-9A-F]{8} drs-irq-dma-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-irq-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-irq-owner 0x00001006 drs-irq-owner-bound 1 drs-irq-qonly 1 drs-irq-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-irq-kind 2 drs-irq-op 2 ' +
        'drs-irq-lba 0 drs-irq-blocks 1 drs-irq-read-bytes 2048 drs-irq-page-bytes 4096 drs-irq-checksum 0x76EFDDC5 ' +
        'drs-irq-zeroed 1 drs-irq-ready 1 drs-irq-dma-denied 1 drs-irq-requested 1 drs-irq-granted 0 ' +
        'drs-irq-denied 1 drs-irq-policy-grant 0 drs-irq-bytes 0 drs-irq-wait 0 drs-irq-fired 0 ' +
        'drs-irq-cstatus 0 drs-irq-cbytes 0 drs-irq-cchecksum 0x00000000 drs-irq-issue-auth 0 ' +
        'drs-irq-dma-auth 0 drs-irq-read-auth 0 drs-irq-write-auth 0 drs-irq-commit-auth 0 ' +
        'drs-irq-block-endpoint 0 drs-irq-block-cap 0 drs-irq-fs-minted 0 drs-irq-mmio-written 0 ' +
        'drs-irq-port-programmed 0 drs-irq-published 0 drs-irq-command-issued 0 drs-irq-dma 0 drs-irq-armed 0 ' +
        'drs-irq-media-read 0 drs-irq-media-written 0 drs-irq-buffer 1 drs-irq-staged 1 ' +
        'drs-irq-denials 1 drs-irq-unavailable 0 map-requests 1 .* queries 359 denials 65'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadIrqPattern -Message "x64 UEFI AHCI denied read-IRQ proof was not observed."
    $uefiDriverReadStatusPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-status 0xFFFFFFFF drs-status 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-status-state 3 drs-status-flags 0x3FFFFFFF ' +
        'drs-status-token 0x(?!00000000)[0-9A-F]{8} drs-status-irq-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-status-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-status-owner 0x00001006 drs-status-owner-bound 1 drs-status-qonly 1 drs-status-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-status-kind 2 drs-status-op 2 ' +
        'drs-status-lba 0 drs-status-blocks 1 drs-status-read-bytes 2048 drs-status-page-bytes 4096 drs-status-checksum 0x76EFDDC5 ' +
        'drs-status-zeroed 1 drs-status-ready 1 drs-status-irq-denied 1 drs-status-requested 1 drs-status-granted 0 ' +
        'drs-status-denied 1 drs-status-policy-grant 0 drs-status-bytes 0 drs-status-poll 0 drs-status-sready 0 ' +
        'drs-status-pxis 0x00000000 drs-status-ci 0x00000000 drs-status-tfd 0x00000000 drs-status-serr 0x00000000 drs-status-irq-clear 0 ' +
        'drs-status-cstatus 0 drs-status-cbytes 0 drs-status-cchecksum 0x00000000 drs-status-issue-auth 0 ' +
        'drs-status-dma-auth 0 drs-status-read-auth 0 drs-status-write-auth 0 drs-status-commit-auth 0 ' +
        'drs-status-block-endpoint 0 drs-status-block-cap 0 drs-status-fs-minted 0 drs-status-mmio-written 0 ' +
        'drs-status-port-programmed 0 drs-status-published 0 drs-status-command-issued 0 drs-status-dma 0 drs-status-armed 0 ' +
        'drs-status-media-read 0 drs-status-media-written 0 drs-status-buffer 1 drs-status-staged 1 ' +
        'drs-status-denials 1 drs-status-unavailable 0 map-requests 1 .* queries 359 denials 66'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusPattern -Message "x64 UEFI AHCI denied read-status proof was not observed."
    $uefiDriverReadStatusResultPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-result 0xFFFFFFFF drs-result 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-result-state 3 drs-result-flags 0x3FFFFFFF ' +
        'drs-result-token 0x(?!00000000)[0-9A-F]{8} drs-result-status-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-result-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-result-owner 0x00001006 drs-result-owner-bound 1 drs-result-qonly 1 drs-result-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-result-kind 2 ' +
        'drs-result-op 2 drs-result-lba 0 drs-result-blocks 1 drs-result-read-bytes 2048 drs-result-page-bytes 4096 ' +
        'drs-result-checksum 0x76EFDDC5 drs-result-zeroed 1 drs-result-ready 1 drs-result-status-denied 1 ' +
        'drs-result-requested 1 drs-result-granted 0 drs-result-denied 1 drs-result-policy-grant 0 drs-result-bytes 0 ' +
        'drs-result-rstatus 0 drs-result-rbytes 0 drs-result-rchecksum 0x00000000 drs-result-issue-auth 0 ' +
        'drs-result-dma-auth 0 drs-result-read-auth 0 drs-result-write-auth 0 drs-result-commit-auth 0 ' +
        'drs-result-block-endpoint 0 drs-result-block-cap 0 drs-result-fs-minted 0 drs-result-mmio-written 0 ' +
        'drs-result-port-programmed 0 drs-result-published 0 drs-result-command-issued 0 drs-result-dma 0 drs-result-armed 0 ' +
        'drs-result-media-read 0 drs-result-media-written 0 drs-result-buffer 1 drs-result-staged 1 ' +
        'drs-result-denials 1 drs-result-unavailable 0 map-requests 1 .* queries 359 denials 67'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusResultPattern -Message "x64 UEFI AHCI denied read-status-result proof was not observed."
    $uefiDriverReadStatusSamplePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-sample 0xFFFFFFFF drs-sample 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-sample-state 3 drs-sample-flags 0x3FFFFFFF ' +
        'drs-sample-token 0x(?!00000000)[0-9A-F]{8} drs-sample-result-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-sample-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-sample-owner 0x00001006 drs-sample-owner-bound 1 drs-sample-qonly 1 drs-sample-port 0x(?!FFFFFFFF)[0-9A-F]{8} ' +
        'drs-sample-kind 2 drs-sample-op 2 drs-sample-lba 0 drs-sample-blocks 1 drs-sample-read-bytes 2048 ' +
        'drs-sample-page-bytes 4096 drs-sample-checksum 0x76EFDDC5 drs-sample-zeroed 1 drs-sample-ready 1 ' +
        'drs-sample-result-denied 1 drs-sample-requested 1 drs-sample-granted 0 drs-sample-denied 1 ' +
        'drs-sample-policy-grant 0 drs-sample-bytes 0 drs-sample-pxis 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-sample-ci 0x00000000 drs-sample-tfd 0x00000050 drs-sample-serr 0x00000000 drs-sample-tfd-ready 1 ' +
        'drs-sample-ci-idle 1 drs-sample-serr-clear 1 drs-sample-irq-clear 0 drs-sample-rstatus 0 ' +
        'drs-sample-rbytes 0 drs-sample-rchecksum 0x00000000 drs-sample-issue-auth 0 drs-sample-dma-auth 0 ' +
        'drs-sample-read-auth 0 drs-sample-write-auth 0 drs-sample-commit-auth 0 drs-sample-block-endpoint 0 ' +
        'drs-sample-block-cap 0 drs-sample-fs-minted 0 drs-sample-mmio-written 0 drs-sample-port-programmed 0 ' +
        'drs-sample-published 0 drs-sample-command-issued 0 drs-sample-dma 0 drs-sample-armed 0 drs-sample-media-read 0 ' +
        'drs-sample-media-written 0 drs-sample-buffer 1 drs-sample-staged 1 drs-sample-denials 1 ' +
        'drs-sample-unavailable 0 map-requests 1 .* queries 359 denials 68'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusSamplePattern -Message "x64 UEFI AHCI read-only status-sample proof was not observed."
    $uefiDriverReadStatusClearPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-clear 0xFFFFFFFF drs-clear 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-clear-state 3 drs-clear-flags 0x3FFFFFFF ' +
        'drs-clear-token 0x(?!00000000)[0-9A-F]{8} drs-clear-sample-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-clear-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-clear-owner 0x00001006 drs-clear-owner-bound 1 drs-clear-qonly 1 drs-clear-port 0x(?!FFFFFFFF)[0-9A-F]{8} ' +
        'drs-clear-kind 2 drs-clear-op 2 drs-clear-lba 0 drs-clear-blocks 1 drs-clear-read-bytes 2048 ' +
        'drs-clear-page-bytes 4096 drs-clear-checksum 0x76EFDDC5 drs-clear-zeroed 1 drs-clear-ready 1 ' +
        'drs-clear-sample-ready 1 drs-clear-sample-bound 1 drs-clear-requested 1 drs-clear-granted 0 ' +
        'drs-clear-denied 1 drs-clear-policy-grant 0 drs-clear-bytes 0 drs-clear-pxis-b 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-clear-pxis-a 0x(?!00000000)[0-9A-F]{8} drs-clear-pxis-same 1 drs-clear-ci 0x00000000 ' +
        'drs-clear-tfd 0x00000050 drs-clear-serr 0x00000000 drs-clear-tfd-ready 1 drs-clear-ci-idle 1 ' +
        'drs-clear-serr-clear 1 drs-clear-clear-requested 1 drs-clear-clear-granted 0 drs-clear-clear-denied 1 ' +
        'drs-clear-clear-value 0x00000000 drs-clear-irq-clear 0 drs-clear-rstatus 0 drs-clear-rbytes 0 ' +
        'drs-clear-rchecksum 0x00000000 drs-clear-issue-auth 0 drs-clear-dma-auth 0 ' +
        'drs-clear-read-auth 0 drs-clear-write-auth 0 drs-clear-commit-auth 0 drs-clear-block-endpoint 0 ' +
        'drs-clear-block-cap 0 drs-clear-fs-minted 0 drs-clear-mmio-written 0 drs-clear-port-programmed 0 ' +
        'drs-clear-published 0 drs-clear-command-issued 0 drs-clear-dma 0 drs-clear-armed 0 drs-clear-media-read 0 ' +
        'drs-clear-media-written 0 drs-clear-buffer 1 drs-clear-staged 1 drs-clear-denials 1 ' +
        'drs-clear-unavailable 0 map-requests 1 .* queries 359 denials 69'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusClearPattern -Message "x64 UEFI AHCI denied status-clear proof was not observed."
    $uefiDriverReadStatusClearResultPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-clear-result 0xFFFFFFFF drs-clear-result 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-clear-result-state 3 drs-clear-result-flags 0x3FFFFFFF ' +
        'drs-clear-result-token 0x(?!00000000)[0-9A-F]{8} drs-clear-result-clear-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-clear-result-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-clear-result-owner 0x00001006 ' +
        'drs-clear-result-owner-bound 1 drs-clear-result-qonly 1 drs-clear-result-port 0x(?!FFFFFFFF)[0-9A-F]{8} ' +
        'drs-clear-result-kind 2 drs-clear-result-op 2 drs-clear-result-lba 0 drs-clear-result-blocks 1 ' +
        'drs-clear-result-read-bytes 2048 drs-clear-result-page-bytes 4096 drs-clear-result-checksum 0x76EFDDC5 ' +
        'drs-clear-result-zeroed 1 drs-clear-result-ready 1 drs-clear-result-clear-denied 1 ' +
        'drs-clear-result-requested 1 drs-clear-result-granted 0 drs-clear-result-denied 1 ' +
        'drs-clear-result-policy-grant 0 drs-clear-result-bytes 0 drs-clear-result-pxis-b 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-clear-result-pxis-a 0x(?!00000000)[0-9A-F]{8} drs-clear-result-pxis-same 1 ' +
        'drs-clear-result-ci 0x00000000 drs-clear-result-tfd 0x00000050 drs-clear-result-serr 0x00000000 ' +
        'drs-clear-result-tfd-ready 1 drs-clear-result-ci-idle 1 drs-clear-result-serr-clear 1 ' +
        'drs-clear-result-clear-requested 1 drs-clear-result-clear-granted 0 drs-clear-result-pxis-clear-denied 1 ' +
        'drs-clear-result-clear-value 0x00000000 drs-clear-result-irq-clear 0 ' +
        'drs-clear-result-result-requested 1 drs-clear-result-result-granted 0 drs-clear-result-result-denied 1 ' +
        'drs-clear-result-result-status 0 drs-clear-result-result-bytes 0 drs-clear-result-result-checksum 0x00000000 .* ' +
        'drs-clear-result-block-endpoint 0 drs-clear-result-block-cap 0 drs-clear-result-fs-minted 0 ' +
        'drs-clear-result-mmio-written 0 drs-clear-result-port-programmed 0 drs-clear-result-published 0 ' +
        'drs-clear-result-command-issued 0 drs-clear-result-dma 0 drs-clear-result-armed 0 ' +
        'drs-clear-result-media-read 0 drs-clear-result-media-written 0 drs-clear-result-buffer 1 ' +
        'drs-clear-result-staged 1 drs-clear-result-denials 1 drs-clear-result-unavailable 0 map-requests 1 .* queries 359 denials 70'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusClearResultPattern -Message "x64 UEFI AHCI denied status-clear-result proof was not observed."
    $uefiDriverReadStatusResamplePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-resample 0xFFFFFFFF drs-resample 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-resample-state 3 drs-resample-flags 0x3FFFFFFF ' +
        'drs-resample-token 0x(?!00000000)[0-9A-F]{8} drs-resample-clear-result-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-resample-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-resample-owner 0x00001006 drs-resample-owner-bound 1 ' +
        'drs-resample-qonly 1 drs-resample-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-resample-kind 2 drs-resample-op 2 ' +
        'drs-resample-lba 0 drs-resample-blocks 1 drs-resample-read-bytes 2048 drs-resample-page-bytes 4096 ' +
        'drs-resample-checksum 0x76EFDDC5 drs-resample-zeroed 1 drs-resample-ready 1 ' +
        'drs-resample-clear-result-denied 1 drs-resample-requested 1 drs-resample-granted 0 ' +
        'drs-resample-denied 1 drs-resample-policy-grant 0 drs-resample-bytes 0 ' +
        'drs-resample-pxis-b 0x(?!00000000)[0-9A-F]{8} drs-resample-pxis-a 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-resample-pxis-same 1 drs-resample-ci 0x00000000 drs-resample-tfd 0x00000050 ' +
        'drs-resample-serr 0x00000000 drs-resample-tfd-ready 1 drs-resample-ci-idle 1 ' +
        'drs-resample-serr-clear 1 drs-resample-irq-clear 0 drs-resample-result-status 0 ' +
        'drs-resample-result-bytes 0 drs-resample-result-checksum 0x00000000 drs-resample-issue-auth 0 ' +
        'drs-resample-dma-auth 0 drs-resample-read-auth 0 drs-resample-write-auth 0 ' +
        'drs-resample-commit-auth 0 drs-resample-block-endpoint 0 drs-resample-block-cap 0 ' +
        'drs-resample-fs-minted 0 drs-resample-mmio-written 0 drs-resample-port-programmed 0 ' +
        'drs-resample-published 0 drs-resample-command-issued 0 drs-resample-dma 0 ' +
        'drs-resample-armed 0 drs-resample-media-read 0 drs-resample-media-written 0 ' +
        'drs-resample-buffer 1 drs-resample-staged 1 drs-resample-denials 1 ' +
        'drs-resample-unavailable 0 map-requests 1 .* queries 359 denials 71'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusResamplePattern -Message "x64 UEFI AHCI read-only status-resample proof was not observed."
    $uefiDriverReadStatusStablePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-stable 0xFFFFFFFF drs-stable 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-stable-state 3 drs-stable-flags 0x3FFFFFFF drs-stable-token 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-stable-resample-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-stable-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-stable-owner 0x00001006 drs-stable-owner-bound 1 drs-stable-qonly 1 ' +
        'drs-stable-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-stable-kind 2 drs-stable-op 2 ' +
        'drs-stable-lba 0 drs-stable-blocks 1 drs-stable-read-bytes 2048 drs-stable-page-bytes 4096 ' +
        'drs-stable-checksum 0x76EFDDC5 drs-stable-zeroed 1 drs-stable-ready 1 ' +
        'drs-stable-clear-result-denied 1 drs-stable-resample-read-only 1 drs-stable-requested 1 ' +
        'drs-stable-granted 0 drs-stable-denied 1 drs-stable-policy-grant 0 drs-stable-bytes 0 ' +
        'drs-stable-pxis-b 0x(?!00000000)[0-9A-F]{8} drs-stable-pxis-a 0x(?!00000000)[0-9A-F]{8} drs-stable-pxis-stable 1 ' +
        'drs-stable-ci-b 0x00000000 drs-stable-ci-a 0x00000000 drs-stable-ci-stable 1 ' +
        'drs-stable-tfd-b 0x00000050 drs-stable-tfd-a 0x00000050 drs-stable-tfd-stable 1 ' +
        'drs-stable-serr-b 0x00000000 drs-stable-serr-a 0x00000000 drs-stable-serr-stable 1 ' +
        'drs-stable-tfd-ready 1 drs-stable-ci-idle 1 drs-stable-serr-clear 1 drs-stable-irq-clear 0 ' +
        'drs-stable-result-status 0 drs-stable-result-bytes 0 drs-stable-result-checksum 0x00000000 ' +
        'drs-stable-issue-auth 0 drs-stable-dma-auth 0 drs-stable-read-auth 0 ' +
        'drs-stable-write-auth 0 drs-stable-commit-auth 0 drs-stable-block-endpoint 0 ' +
        'drs-stable-block-cap 0 drs-stable-fs-minted 0 drs-stable-mmio-written 0 ' +
        'drs-stable-port-programmed 0 drs-stable-published 0 drs-stable-command-issued 0 ' +
        'drs-stable-dma 0 drs-stable-armed 0 drs-stable-media-read 0 ' +
        'drs-stable-media-written 0 drs-stable-buffer 1 drs-stable-staged 1 ' +
        'drs-stable-denials 1 drs-stable-unavailable 0 map-requests 1 .* queries 359 denials 72'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusStablePattern -Message "x64 UEFI AHCI read-only status-stable proof was not observed."
    $uefiDriverReadStatusGuardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-guard 0xFFFFFFFF drs-guard 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-guard-state 3 drs-guard-flags 0x3FFFFFFF drs-guard-token 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-guard-stable-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-guard-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-guard-owner 0x00001006 drs-guard-owner-bound 1 drs-guard-qonly 1 ' +
        'drs-guard-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-guard-kind 2 drs-guard-op 2 ' +
        'drs-guard-lba 0 drs-guard-blocks 1 drs-guard-read-bytes 2048 drs-guard-page-bytes 4096 ' +
        'drs-guard-checksum 0x76EFDDC5 drs-guard-zeroed 1 drs-guard-ready 1 ' +
        'drs-guard-pxis-b 0x(?!00000000)[0-9A-F]{8} drs-guard-pxis-a 0x(?!00000000)[0-9A-F]{8} drs-guard-pxis-stable 1 ' +
        'drs-guard-ci-b 0x00000000 drs-guard-ci-a 0x00000000 drs-guard-ci-stable 1 ' +
        'drs-guard-tfd-b 0x00000050 drs-guard-tfd-a 0x00000050 drs-guard-tfd-stable 1 ' +
        'drs-guard-serr-b 0x00000000 drs-guard-serr-a 0x00000000 drs-guard-serr-stable 1 ' +
        'drs-guard-tfd-ready 1 drs-guard-ci-idle 1 drs-guard-serr-clear 1 ' +
        'drs-guard-requested 1 drs-guard-issue-ok 0 drs-guard-issue-denied 1 ' +
        'drs-guard-dma-ok 0 drs-guard-dma-denied 1 drs-guard-read-auth 0 ' +
        'drs-guard-read-denied 1 drs-guard-write-auth 0 drs-guard-write-denied 1 ' +
        'drs-guard-commit-auth 0 drs-guard-commit-denied 1 drs-guard-irq-clear 0 ' +
        'drs-guard-result-status 0 drs-guard-result-bytes 0 drs-guard-result-checksum 0x00000000 ' +
        'drs-guard-block-endpoint 0 drs-guard-block-cap 0 drs-guard-fs-minted 0 ' +
        'drs-guard-mmio-written 0 drs-guard-port-programmed 0 drs-guard-published 0 ' +
        'drs-guard-command-issued 0 drs-guard-dma 0 drs-guard-armed 0 ' +
        'drs-guard-media-read 0 drs-guard-media-written 0 drs-guard-buffer 1 ' +
        'drs-guard-staged 1 drs-guard-denials 1 drs-guard-unavailable 0 map-requests 1 .* queries 359 denials 73'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusGuardPattern -Message "x64 UEFI AHCI read-status guard proof was not observed."
    $uefiDriverReadStatusBufferPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-buffer 0xFFFFFFFF drs-buffer 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-buffer-state 3 drs-buffer-flags 0x3FFFFFFF drs-buffer-token 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-buffer-guard-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-buffer-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-buffer-owner 0x00001006 drs-buffer-owner-bound 1 drs-buffer-qonly 1 ' +
        'drs-buffer-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-buffer-kind 2 drs-buffer-op 2 ' +
        'drs-buffer-lba 0 drs-buffer-blocks 1 drs-buffer-read-bytes 2048 drs-buffer-page-bytes 4096 ' +
        'drs-buffer-checksum 0x76EFDDC5 drs-buffer-zeroed 1 drs-buffer-ready 1 ' +
        'drs-buffer-view-requested 1 drs-buffer-view-granted 0 drs-buffer-view-denied 1 ' +
        'drs-buffer-result-status 0 drs-buffer-result-bytes 0 drs-buffer-result-checksum 0x00000000 ' +
        'drs-buffer-read-auth 0 drs-buffer-write-auth 0 drs-buffer-commit-auth 0 ' +
        'drs-buffer-block-endpoint 0 drs-buffer-block-cap 0 drs-buffer-fs-minted 0 ' +
        'drs-buffer-mmio-written 0 drs-buffer-port-programmed 0 drs-buffer-published 0 ' +
        'drs-buffer-command-issued 0 drs-buffer-dma 0 drs-buffer-armed 0 ' +
        'drs-buffer-media-read 0 drs-buffer-media-written 0 drs-buffer-buffer 1 ' +
        'drs-buffer-staged 1 drs-buffer-denials 1 drs-buffer-unavailable 0 .* map-requests 1 .* queries 359 denials 75'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusBufferPattern -Message "x64 UEFI AHCI read-status buffer proof was not observed."
    $uefiDriverReadStatusExportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-export 0xFFFFFFFF drs-export 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-export-state 3 drs-export-flags 0x3FFFFFFF drs-export-token 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-export-buffer-token 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-export-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-export-owner 0x00001006 drs-export-owner-bound 1 drs-export-qonly 1 ' +
        'drs-export-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-export-kind 2 drs-export-op 2 ' +
        'drs-export-lba 0 drs-export-blocks 1 drs-export-read-bytes 2048 ' +
        'drs-export-checksum 0x76EFDDC5 ' +
        'drs-export-sealed 1 drs-export-requested 1 drs-export-granted 0 drs-export-denied 1 ' +
        'drs-export-user-copy 0x00000000 drs-export-authority 0x00000000 drs-export-effects 0x00000000 ' +
        'drs-export-buffer 1 drs-export-staged 1 ' +
        'drs-export-denials 1 drs-export-unavailable 0 map-requests 1 .* queries 359 denials 75'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusExportPattern -Message "x64 UEFI AHCI read-status export denial proof was not observed."
    $uefiDriverReadStatusReportPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-report 0xFFFFFFFF drs-report 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-report-state 3 drs-report-flags 0x3FFFFFFF ' +
        'drs-report-owner 0x00001006 drs-report-qonly 1 ' +
        'drs-report-checksum 0x76EFDDC5 drs-report-export-denied 1 ' +
        'drs-report-report 0x00000000 drs-report-user-copy 0x00000000 ' +
        'drs-report-authority 0x00000000 drs-report-effects 0x00000000 ' +
        'drs-report-buffer 1 drs-report-staged 1 ' +
        'drs-report-denials 1 drs-report-unavailable 0 map-requests 1 .* queries 359 denials 76'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusReportPattern -Message "x64 UEFI AHCI read-status report denial proof was not observed."
    $uefiDriverReadStatusReceiptPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-receipt 0xFFFFFFFF drs-receipt 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-receipt-state 3 drs-receipt-flags 0x3FFFFFFF ' +
        'drs-receipt-owner 0x00001006 drs-receipt-qonly 1 ' +
        'drs-receipt-checksum 0x76EFDDC5 drs-receipt-report-denied 1 ' +
        'drs-receipt-receipt 0x00000000 drs-receipt-user-copy 0x00000000 ' +
        'drs-receipt-authority 0x00000000 drs-receipt-effects 0x00000000 ' +
        'drs-receipt-buffer 1 drs-receipt-staged 1 ' +
        'drs-receipt-denials 1 drs-receipt-unavailable 0 map-requests 1 .* queries 359 denials 77'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusReceiptPattern -Message "x64 UEFI AHCI read-status receipt denial proof was not observed."
    $uefiDriverReadStatusAckPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-ack 0xFFFFFFFF drs-ack 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-ack-state 3 drs-ack-flags 0x3FFFFFFF ' +
        'drs-ack-owner 0x00001006 drs-ack-qonly 1 ' +
        'drs-ack-checksum 0x76EFDDC5 drs-ack-receipt-denied 1 ' +
        'drs-ack-ack 0x00000000 drs-ack-user-copy 0x00000000 ' +
        'drs-ack-authority 0x00000000 drs-ack-effects 0x00000000 ' +
        'drs-ack-buffer 1 drs-ack-staged 1 ' +
        'drs-ack-denials 1 drs-ack-unavailable 0 map-requests 1 .* queries 359 denials 78'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusAckPattern -Message "x64 UEFI AHCI read-status ack denial proof was not observed."
    $uefiDriverReadStatusClosePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-close 0xFFFFFFFF drs-close 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-close-state 3 drs-close-flags 0x3FFFFFFF ' +
        'drs-close-owner 0x00001006 drs-close-qonly 1 ' +
        'drs-close-checksum 0x76EFDDC5 drs-close-ack-denied 1 ' +
        'drs-close-close 0x00000000 drs-close-user-copy 0x00000000 ' +
        'drs-close-authority 0x00000000 drs-close-effects 0x00000000 ' +
        'drs-close-buffer 1 drs-close-staged 1 ' +
        'drs-close-denials 1 drs-close-unavailable 0 map-requests 1 .* queries 359 denials 79'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusClosePattern -Message "x64 UEFI AHCI read-status close denial proof was not observed."
    $uefiDriverReadStatusSealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-seal 0xFFFFFFFF drs-seal 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-seal-state 3 drs-seal-flags 0x3FFFFFFF ' +
        'drs-seal-owner 0x00001006 drs-seal-qonly 1 ' +
        'drs-seal-checksum 0x76EFDDC5 drs-seal-close-denied 1 ' +
        'drs-seal-seal 0x00000000 drs-seal-user-copy 0x00000000 ' +
        'drs-seal-authority 0x00000000 drs-seal-effects 0x00000000 ' +
        'drs-seal-buffer 1 drs-seal-staged 1 ' +
        'drs-seal-denials 1 drs-seal-unavailable 0 map-requests 1 .* queries 359 denials 80'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusSealPattern -Message "x64 UEFI AHCI read-status seal denial proof was not observed."
    $uefiDriverReadStatusUnsealPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-unseal 0xFFFFFFFF drs-unseal 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-unseal-state 3 drs-unseal-flags 0x3FFFFFFF ' +
        'drs-unseal-owner 0x00001006 drs-unseal-qonly 1 ' +
        'drs-unseal-checksum 0x76EFDDC5 drs-unseal-seal-denied 1 ' +
        'drs-unseal-unseal 0x00000000 drs-unseal-user-copy 0x00000000 ' +
        'drs-unseal-authority 0x00000000 drs-unseal-effects 0x00000000 ' +
        'drs-unseal-buffer 1 drs-unseal-staged 1 ' +
        'drs-unseal-denials 1 drs-unseal-unavailable 0 map-requests 1 .* queries 359 denials 81'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusUnsealPattern -Message "x64 UEFI AHCI read-status unseal denial proof was not observed."
    $uefiDriverReadStatusDiscardPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-discard 0xFFFFFFFF drs-discard 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-discard-state 3 drs-discard-flags 0x3FFFFFFF ' +
        'drs-discard-owner 0x00001006 drs-discard-qonly 1 ' +
        'drs-discard-checksum 0x76EFDDC5 drs-discard-unseal-denied 1 ' +
        'drs-discard-discard 0x00000000 drs-discard-user-copy 0x00000000 ' +
        'drs-discard-authority 0x00000000 drs-discard-effects 0x00000000 ' +
        'drs-discard-buffer 1 drs-discard-staged 1 ' +
        'drs-discard-denials 1 drs-discard-unavailable 0 map-requests 1 .* queries 359 denials 82'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusDiscardPattern -Message "x64 UEFI AHCI read-status discard denial proof was not observed."
    $uefiDriverReadStatusFinalizePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-final 0xFFFFFFFF drs-final 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-final-state 3 drs-final-flags 0x3FFFFFFF ' +
        'drs-final-owner 0x00001006 drs-final-qonly 1 ' +
        'drs-final-checksum 0x76EFDDC5 drs-final-discard-denied 1 ' +
        'drs-final-finish 0x00000000 drs-final-user-copy 0x00000000 ' +
        'drs-final-authority 0x00000000 drs-final-effects 0x00000000 ' +
        'drs-final-buffer 1 drs-final-staged 1 ' +
        'drs-final-denials 1 drs-final-unavailable 0 map-requests 1 .* queries 359 denials 83'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusFinalizePattern -Message "x64 UEFI AHCI read-status finalize denial proof was not observed."
    $uefiDriverReadStatusAuthorizePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-authz 0xFFFFFFFF drs-authz 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-authz-state 3 drs-authz-flags 0x3FFFFFFF ' +
        'drs-authz-owner 0x00001006 drs-authz-qonly 1 ' +
        'drs-authz-checksum 0x76EFDDC5 drs-authz-final-denied 1 ' +
        'drs-authz-grant 0x00000000 drs-authz-user-copy 0x00000000 ' +
        'drs-authz-authority 0x00000000 drs-authz-effects 0x00000000 ' +
        'drs-authz-buffer 1 drs-authz-staged 1 ' +
        'drs-authz-denials 1 drs-authz-unavailable 0 map-requests 1 .* queries 359 denials 84'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusAuthorizePattern -Message "x64 UEFI AHCI read-status authorize denial proof was not observed."
    $uefiDriverReadStatusDispatchPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dispatch 0xFFFFFFFF drs-dispatch 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-dispatch-state 3 drs-dispatch-flags 0x3FFFFFFF ' +
        'drs-dispatch-owner 0x00001006 drs-dispatch-qonly 1 ' +
        'drs-dispatch-checksum 0x76EFDDC5 drs-dispatch-authz-denied 1 ' +
        'drs-dispatch-safety 0x00000000 drs-dispatch-buffer 1 drs-dispatch-staged 1 ' +
        'drs-dispatch-denials 1 drs-dispatch-unavailable 0 map-requests 1 .* queries 359 denials 85'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusDispatchPattern -Message "x64 UEFI AHCI read-status dispatch denial proof was not observed."
    $uefiDriverReadStatusQueuePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-queue 0xFFFFFFFF drs-queue 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-queue-state 3 drs-queue-flags 0x3FFFFFFF ' +
        'drs-queue-owner 0x00001006 drs-queue-qonly 1 ' +
        'drs-queue-checksum 0x76EFDDC5 drs-queue-dispatch-denied 1 ' +
        'drs-queue-safety 0x00000000 drs-queue-depth 0 drs-queue-admit 0 ' +
        'drs-queue-worker 0 drs-queue-runnable 0 drs-queue-schedule 0 drs-queue-buffer 1 ' +
        'drs-queue-staged 1 drs-queue-denials 1 drs-queue-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 86'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusQueuePattern -Message "x64 UEFI AHCI read-status queue denial proof was not observed."
    $uefiDriverReadStatusWorkerPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-w 0xFFFFFFFF drs-w 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-w-state 3 drs-w-flags 0x3FFFFFFF ' +
        'drs-w-owner 0x00001006 drs-w-qonly 1 ' +
        'drs-w-checksum 0x76EFDDC5 drs-w-queue-denied 1 ' +
        'drs-w-safety 0x00000000 drs-w-dequeue 0 drs-w-admit 0 ' +
        'drs-w-wake 0 drs-w-runnable 0 drs-w-sched 0 drs-w-run 0 drs-w-exec 0 ' +
        'drs-w-buffer 1 drs-w-staged 1 ' +
        'drs-w-denials 1 drs-w-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 87'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusWorkerPattern -Message "x64 UEFI AHCI read-status worker denial proof was not observed."
    $uefiDriverReadStatusReadAuthorityPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-rauth 0xFFFFFFFF drs-rauth 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-rauth-state 3 drs-rauth-flags 0x3FFFFFFF ' +
        'drs-rauth-owner 0x00001006 drs-rauth-qonly 1 ' +
        'drs-rauth-checksum 0x76EFDDC5 drs-rauth-worker-denied 1 ' +
        'drs-rauth-policy 1 drs-rauth-read 1 drs-rauth-issue 0 ' +
        'drs-rauth-dma-auth 0 drs-rauth-media-auth 0 drs-rauth-write 0 drs-rauth-commit 0 ' +
        'drs-rauth-block-endpoint 0 drs-rauth-block-cap 0 drs-rauth-fs-minted 0 ' +
        'drs-rauth-safety 0x00000000 drs-rauth-dequeue 0 drs-rauth-admit 0 ' +
        'drs-rauth-wake 0 drs-rauth-runnable 0 drs-rauth-sched 0 drs-rauth-run 0 ' +
        'drs-rauth-exec 0 drs-rauth-buffer 1 drs-rauth-staged 1 ' +
        'drs-rauth-denials 1 drs-rauth-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 88'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusReadAuthorityPattern -Message "x64 UEFI AHCI read-authority grant proof was not observed."
    $uefiDriverReadStatusDescriptorPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-desc 0xFFFFFFFF drs-desc 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-desc-state 3 drs-desc-flags 0x3FFFFFFF ' +
        'drs-desc-owner 0x00001006 drs-desc-qonly 1 ' +
        'drs-desc-checksum 0x76EFDDC5 drs-desc-rauth 1 drs-desc-shaped 1 drs-desc-read 1 ' +
        'drs-desc-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-desc-kind 2 drs-desc-op 2 drs-desc-lba 0 ' +
        'drs-desc-blocks 1 drs-desc-read-bytes 2048 drs-desc-page-bytes 4096 ' +
        'drs-desc-slot 0 drs-desc-header 32 drs-desc-table 144 drs-desc-cfis 20 ' +
        'drs-desc-prdt 1 drs-desc-prdt-bytes 16 drs-desc-packet 12 ' +
        'drs-desc-opcode 0x000000A0 drs-desc-packet-op 0x00000028 drs-desc-transfer 2048 ' +
        'drs-desc-issue 0 drs-desc-dma-auth 0 drs-desc-media-auth 0 drs-desc-write 0 ' +
        'drs-desc-commit 0 drs-desc-block-endpoint 0 drs-desc-block-cap 0 ' +
        'drs-desc-fs-minted 0 drs-desc-safety 0x00000000 drs-desc-dequeue 0 ' +
        'drs-desc-admit 0 drs-desc-wake 0 drs-desc-runnable 0 drs-desc-sched 0 ' +
        'drs-desc-run 0 drs-desc-exec 0 drs-desc-buffer 1 drs-desc-staged 1 ' +
        'drs-desc-denials 1 drs-desc-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 89'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusDescriptorPattern -Message "x64 UEFI AHCI read-status descriptor proof was not observed."
    $uefiDriverReadStatusCommandTablePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-ctab 0xFFFFFFFF drs-ctab 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-ctab-state 3 drs-ctab-flags 0x3FFFFFFF ' +
        'drs-ctab-owner 0x00001006 drs-ctab-qonly 1 ' +
        'drs-ctab-checksum 0x76EFDDC5 drs-ctab-desc 1 drs-ctab-mat 1 drs-ctab-ready 1 ' +
        'drs-ctab-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-ctab-kind 2 drs-ctab-op 2 drs-ctab-lba 0 ' +
        'drs-ctab-blocks 1 drs-ctab-read-bytes 2048 drs-ctab-page-bytes 4096 ' +
        'drs-ctab-before 0x76EFDDC5 drs-ctab-after 0x3FBFAF45 drs-ctab-changed 1 ' +
        'drs-ctab-hdr 0x00010025 drs-ctab-cfis 0x000000A0 drs-ctab-packet 0x00000028 ' +
        'drs-ctab-dbc 2047 drs-ctab-written 1 drs-ctab-issue 0 drs-ctab-dma-auth 0 ' +
        'drs-ctab-media-auth 0 drs-ctab-write 0 drs-ctab-commit 0 ' +
        'drs-ctab-block-endpoint 0 drs-ctab-block-cap 0 drs-ctab-fs-minted 0 ' +
        'drs-ctab-safety 0x00000000 drs-ctab-mmio 0 drs-ctab-portw 0 ' +
        'drs-ctab-cmd 0 drs-ctab-dma 0 drs-ctab-media 0 drs-ctab-buffer 1 ' +
        'drs-ctab-staged 1 drs-ctab-denials 1 drs-ctab-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 90'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusCommandTablePattern -Message "x64 UEFI AHCI read-status command-table proof was not observed."
    $uefiDriverReadStatusCommandIssuePattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-issue 0xFFFFFFFF drs-issue 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-issue-state 3 drs-issue-flags 0x3FFFFFFF ' +
        'drs-issue-owner 0x00001006 drs-issue-qonly 1 drs-issue-checksum 0x76EFDDC5 ' +
        'drs-issue-ctab 1 drs-issue-ready 1 drs-issue-request 1 drs-issue-grant 0 drs-issue-denied 1 ' +
        'drs-issue-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-issue-kind 2 drs-issue-op 2 drs-issue-lba 0 ' +
        'drs-issue-blocks 1 drs-issue-read-bytes 2048 drs-issue-page-bytes 4096 ' +
        'drs-issue-ci 0x00000000 drs-issue-mask 0x00000001 drs-issue-slot-idle 1 ' +
        'drs-issue-tfd 1 drs-issue-serr 1 drs-issue-table-check 0x3FBFAF45 ' +
        'drs-issue-expected 0x3FBFAF45 drs-issue-match 1 ' +
        'drs-issue-issue-auth 0 drs-issue-dma-auth 0 drs-issue-media-auth 0 ' +
        'drs-issue-write 0 drs-issue-commit 0 drs-issue-block-endpoint 0 ' +
        'drs-issue-block-cap 0 drs-issue-fs-minted 0 drs-issue-safety 0x00000000 ' +
        'drs-issue-mmio 0 drs-issue-portw 0 drs-issue-cmd 0 drs-issue-dma 0 ' +
        'drs-issue-media 0 drs-issue-buffer 1 drs-issue-staged 1 ' +
        'drs-issue-denials 1 drs-issue-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 91'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusCommandIssuePattern -Message "x64 UEFI AHCI read-status command-issue denial proof was not observed."
    $uefiDriverReadStatusIssueGrantPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-grant 0xFFFFFFFF drs-grant 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-grant-state 3 drs-grant-flags 0x3FFFFFFF ' +
        'drs-grant-owner 0x00001006 drs-grant-qonly 1 drs-grant-checksum 0x76EFDDC5 ' +
        'drs-grant-issue 1 drs-grant-ready 1 drs-grant-request 1 drs-grant-grant 0 drs-grant-denied 1 ' +
        'drs-grant-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-grant-kind 2 drs-grant-op 2 drs-grant-lba 0 ' +
        'drs-grant-blocks 1 drs-grant-read-bytes 2048 drs-grant-page-bytes 4096 ' +
        'drs-grant-ci 0x00000000 drs-grant-mask 0x00000001 drs-grant-slot-idle 1 ' +
        'drs-grant-tfd 1 drs-grant-serr 1 drs-grant-table-check 0x3FBFAF45 ' +
        'drs-grant-expected 0x3FBFAF45 drs-grant-match 1 ' +
        'drs-grant-issue-auth 0 drs-grant-dma-auth 0 drs-grant-media-auth 0 ' +
        'drs-grant-write 0 drs-grant-commit 0 drs-grant-block-endpoint 0 ' +
        'drs-grant-block-cap 0 drs-grant-fs-minted 0 drs-grant-safety 0x00000000 ' +
        'drs-grant-mmio 0 drs-grant-portw 0 drs-grant-cmd 0 drs-grant-dma 0 ' +
        'drs-grant-media 0 drs-grant-buffer 1 drs-grant-staged 1 ' +
        'drs-grant-denials 1 drs-grant-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 92'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusIssueGrantPattern -Message "x64 UEFI AHCI read-status issue-grant denial proof was not observed."
    $uefiDriverReadStatusArmPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-arm 0xFFFFFFFF drs-arm 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-arm-state 3 drs-arm-flags 0x3FFFFFFF ' +
        'drs-arm-owner 0x00001006 drs-arm-qonly 1 drs-arm-checksum 0x76EFDDC5 ' +
        'drs-arm-grant 1 drs-arm-ready 1 drs-arm-request 1 drs-arm-arm 0 drs-arm-denied 1 ' +
        'drs-arm-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-arm-kind 2 drs-arm-op 2 drs-arm-lba 0 ' +
        'drs-arm-blocks 1 drs-arm-read-bytes 2048 drs-arm-page-bytes 4096 ' +
        'drs-arm-ci 0x00000000 drs-arm-mask 0x00000001 drs-arm-slot-idle 1 ' +
        'drs-arm-tfd 1 drs-arm-serr 1 drs-arm-table-check 0x3FBFAF45 ' +
        'drs-arm-expected 0x3FBFAF45 drs-arm-match 1 ' +
        'drs-arm-issue-auth 0 drs-arm-dma-auth 0 drs-arm-media-auth 0 ' +
        'drs-arm-write 0 drs-arm-commit 0 drs-arm-block-endpoint 0 ' +
        'drs-arm-block-cap 0 drs-arm-fs-minted 0 drs-arm-safety 0x00000000 ' +
        'drs-arm-mmio 0 drs-arm-portw 0 drs-arm-cmd 0 drs-arm-dma 0 ' +
        'drs-arm-media 0 drs-arm-buffer 1 drs-arm-staged 1 ' +
        'drs-arm-denials 1 drs-arm-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 93'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusArmPattern -Message "x64 UEFI AHCI read-status arm denial proof was not observed."
    $uefiDriverReadStatusExecPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-exec 0xFFFFFFFF drs-exec 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-exec-state 3 drs-exec-flags 0x3FFFFFFF ' +
        'drs-exec-owner 0x00001006 drs-exec-qonly 1 drs-exec-checksum 0x76EFDDC5 ' +
        'drs-exec-arm 1 drs-exec-ready 1 drs-exec-request 1 drs-exec-exec 0 drs-exec-denied 1 ' +
        'drs-exec-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-exec-kind 2 drs-exec-op 2 drs-exec-lba 0 ' +
        'drs-exec-blocks 1 drs-exec-read-bytes 2048 drs-exec-page-bytes 4096 ' +
        'drs-exec-ci 0x00000000 drs-exec-mask 0x00000001 drs-exec-slot-idle 1 ' +
        'drs-exec-tfd 1 drs-exec-serr 1 drs-exec-table-check 0x3FBFAF45 ' +
        'drs-exec-expected 0x3FBFAF45 drs-exec-match 1 ' +
        'drs-exec-issue-auth 0 drs-exec-dma-auth 0 drs-exec-media-auth 0 ' +
        'drs-exec-write 0 drs-exec-commit 0 drs-exec-block-endpoint 0 ' +
        'drs-exec-block-cap 0 drs-exec-fs-minted 0 drs-exec-safety 0x00000000 ' +
        'drs-exec-mmio 0 drs-exec-portw 0 drs-exec-cmd 0 drs-exec-dma 0 ' +
        'drs-exec-media 0 drs-exec-buffer 1 drs-exec-staged 1 ' +
        'drs-exec-denials 1 drs-exec-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 94'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusExecPattern -Message "x64 UEFI AHCI read-status exec denial proof was not observed."
    $uefiDriverReadStatusDmaPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dma 0xFFFFFFFF drs-dma 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-dma-state 3 drs-dma-flags 0x3FFFFFFF ' +
        'drs-dma-owner 0x00001006 drs-dma-qonly 1 drs-dma-checksum 0x76EFDDC5 ' +
        'drs-dma-exec 1 drs-dma-ready 1 drs-dma-request 1 drs-dma-grant 0 drs-dma-denied 1 ' +
        'drs-dma-port 0x(?!FFFFFFFF)[0-9A-F]{8} drs-dma-kind 2 drs-dma-op 2 drs-dma-lba 0 ' +
        'drs-dma-blocks 1 drs-dma-read-bytes 2048 drs-dma-page-bytes 4096 ' +
        'drs-dma-ci 0x00000000 drs-dma-mask 0x00000001 drs-dma-slot-idle 1 ' +
        'drs-dma-tfd 1 drs-dma-serr 1 drs-dma-table-check 0x3FBFAF45 ' +
        'drs-dma-expected 0x3FBFAF45 drs-dma-match 1 ' +
        'drs-dma-issue-auth 0 drs-dma-dma-auth 0 drs-dma-media-auth 0 ' +
        'drs-dma-write 0 drs-dma-commit 0 drs-dma-block-endpoint 0 ' +
        'drs-dma-block-cap 0 drs-dma-fs-minted 0 drs-dma-safety 0x00000000 ' +
        'drs-dma-mmio 0 drs-dma-portw 0 drs-dma-cmd 0 drs-dma-dma 0 ' +
        'drs-dma-media 0 drs-dma-buffer 1 drs-dma-staged 1 ' +
        'drs-dma-denials 1 drs-dma-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 95'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusDmaPattern -Message "x64 UEFI AHCI read-status DMA denial proof was not observed."
    $uefiDriverReadStatusMmioPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-mmio 0xFFFFFFFF stale-drs-mmio 0xFFFFFFFF drs-mmio 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-mmio-state 3 drs-mmio-flags 0x3FFFFFFF ' +
        'drs-mmio-owner 0x00001006 drs-mmio-qonly 1 drs-mmio-checksum 0x76EFDDC5 ' +
        'drs-mmio-dma-bound 1 drs-mmio-ready 1 drs-mmio-request 1 drs-mmio-grant 1 drs-mmio-denied 0 ' +
        'drs-mmio-port 0x0000000[0-5] drs-mmio-kind 2 drs-mmio-op 2 drs-mmio-lba 0 ' +
        'drs-mmio-blocks 1 drs-mmio-read-bytes 2048 drs-mmio-page-bytes 4096 ' +
        'drs-mmio-ci 0x00000000 drs-mmio-mask 0x00000001 drs-mmio-slot-idle 1 ' +
        'drs-mmio-tfd 1 drs-mmio-serr 1 drs-mmio-table-check 0x3FBFAF45 ' +
        'drs-mmio-expected 0x3FBFAF45 drs-mmio-match 1 ' +
        'drs-mmio-reg 0x00000(?:110|190|210|290|310|390) drs-mmio-value 0x00000000 ' +
        'drs-mmio-pxis-b 0x00000003 drs-mmio-pxis-a 0x00000003 drs-mmio-pxis-same 1 ' +
        'drs-mmio-rollback-required 0 drs-mmio-rollback-done 1 drs-mmio-teardown 1 drs-mmio-stale-denied 1 ' +
        'drs-mmio-issue-auth 0 drs-mmio-dma-auth 0 drs-mmio-media-auth 0 ' +
        'drs-mmio-write 0 drs-mmio-commit 0 drs-mmio-block-endpoint 0 ' +
        'drs-mmio-block-cap 0 drs-mmio-fs-minted 0 drs-mmio-safety 0x00000000 ' +
        'drs-mmio-mmio 1 drs-mmio-portw 0 drs-mmio-cmd 0 drs-mmio-dma 0 ' +
        'drs-mmio-media 0 drs-mmio-media-write 0 drs-mmio-buffer 1 drs-mmio-staged 1 ' +
        'drs-mmio-denials 2 drs-mmio-unavailable 0 ' +
        'map-requests 1 .* queries 359 denials 97'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusMmioPattern -Message "x64 UEFI AHCI read-status MMIO write proof was not observed."
    $uefiDriverReadStatusDmaWindowPattern = (
        '\[x64\] mmio planner service 9 .* denied-drs-dwin 0xFFFFFFFF stale-drs-dwin 0xFFFFFFFF drs-dwin 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-dwin-state 3 drs-dwin-flags 0x3FFFFFFF ' +
        'drs-dwin-owner 0x00001006 drs-dwin-qonly 1 drs-dwin-checksum 0x76EFDDC5 ' +
        'drs-dwin-mmio-bound 1 drs-dwin-ready 1 drs-dwin-request 1 drs-dwin-grant 1 drs-dwin-denied 0 ' +
        'drs-dwin-port 0x0000000[0-5] drs-dwin-kind 2 drs-dwin-op 2 drs-dwin-lba 0 ' +
        'drs-dwin-blocks 1 drs-dwin-read-bytes 2048 drs-dwin-page-bytes 4096 ' +
        'drs-dwin-ci 0x00000000 drs-dwin-mask 0x00000001 drs-dwin-slot-idle 1 ' +
        'drs-dwin-tfd 1 drs-dwin-serr 1 drs-dwin-table-check 0x3FBFAF45 ' +
        'drs-dwin-expected 0x3FBFAF45 drs-dwin-match 1 ' +
        'drs-dwin-page-low 0x[0-9A-F]{5}000 drs-dwin-page-high 0x00000000 ' +
        'drs-dwin-bounce-low 0x[0-9A-F]{5}800 drs-dwin-bounce-high 0x00000000 ' +
        'drs-dwin-bounce-bytes 2048 drs-dwin-offset 2048 drs-dwin-range-end 4096 ' +
        'drs-dwin-single-page 1 drs-dwin-broker 1 drs-dwin-bounds 1 ' +
        'drs-dwin-confined 1 drs-dwin-below4g 1 drs-dwin-iommu 0 drs-dwin-identity 1 ' +
        'drs-dwin-non-user 1 drs-dwin-alias-safe 1 drs-dwin-opened 1 drs-dwin-closed 1 drs-dwin-active 0 ' +
        'drs-dwin-revoke-required 1 drs-dwin-revoke-done 1 drs-dwin-stale-denied 1 ' +
        'drs-dwin-issue-auth 0 drs-dwin-dma-auth 0 drs-dwin-media-auth 0 ' +
        'drs-dwin-write 0 drs-dwin-commit 0 drs-dwin-block-endpoint 0 ' +
        'drs-dwin-block-cap 0 drs-dwin-fs-minted 0 drs-dwin-safety 0x00000000 ' +
        'drs-dwin-mmio 0 drs-dwin-portw 0 drs-dwin-cmd 0 drs-dwin-dma 0 ' +
        'drs-dwin-media 0 drs-dwin-media-write 0 drs-dwin-buffer 1 drs-dwin-staged 1 ' +
        'drs-dwin-denials 2 drs-dwin-unavailable 0 ' +
        '.* map-requests 1 .* queries 359 denials 99'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusDmaWindowPattern -Message "x64 UEFI AHCI read-status DMA-window proof was not observed."
    $uefiDriverReadStatusReadPattern = (
        '\[x64\] mmio planner service 9 .* drs-read 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-read-state 3 drs-read-flags 0x007FFFFF ' +
        'drs-read-owner 0x00001006 drs-read-qonly 1 drs-read-dwin-bound 1 drs-read-ready 1 drs-read-request 1 ' +
        'drs-read-issued 1 drs-read-completed 1 drs-read-bytes 2048 drs-read-checksum 0x(?!00000000)[0-9A-F]{8} drs-read-error 0 ' +
        'drs-read-port 0x0000000[0-5] drs-read-kind 2 drs-read-op 2 drs-read-lba 0 drs-read-blocks 1 ' +
        'drs-read-page-low 0x[0-9A-F]{5}000 drs-read-bounce-low 0x[0-9A-F]{5}800 ' +
        'drs-read-table-before 0x(?!00000000)[0-9A-F]{8} drs-read-table-after 0x(?!00000000)[0-9A-F]{8} ' +
        'drs-read-prdbc [0-9]+ drs-read-ci-b 0x00000000 drs-read-ci-a 0x00000000 ' +
        'drs-read-tfd-b 0x[0-9A-F]{8} drs-read-tfd-a 0x[0-9A-F]{8} drs-read-polls [1-9][0-9]* ' +
        'drs-read-mmio 1 drs-read-portw 1 drs-read-dma 1 drs-read-media 1 drs-read-write 0 ' +
        'drs-read-commit 0 drs-read-block-endpoint 0 drs-read-fs-minted 0 drs-read-active 0 ' +
        'drs-read-staged 1 drs-read-denials 0 drs-read-unavailable 0 '
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusReadPattern -Message "x64 UEFI AHCI drs-read positive read proof was not observed."
    $uefiDriverReadStatusBlockPattern = (
        '\[x64\] mmio planner service 9 .* drs-read .* drs-read-checksum (0x(?!00000000)[0-9A-F]{8}) .* ' +
        'denied-drs-block 0xFFFFFFFF stale-drs-block 0xFFFFFFFF drs-block 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-block-state 3 drs-block-flags 0x000FFFFF drs-block-owner 0x00001006 drs-block-read-bound 1 ' +
        'drs-block-endpoint 1 drs-block-cap-minted 1 drs-block-cap 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} ' +
        'drs-block-read-only 1 drs-block-delegated-cap 1 drs-block-read-routed 1 drs-block-bytes 2048 ' +
        'drs-block-checksum \1 drs-block-read-checksum \1 drs-block-checksum-match 1 ' +
        'drs-block-wrong-owner 1 drs-block-stale-denied 1 drs-block-write 0 drs-block-commit 0 ' +
        'drs-block-fs-minted 0 drs-block-lba 0 drs-block-blocks 1 drs-block-buffer 1 ' +
        'drs-block-staged 1 drs-block-denials 2 drs-block-unavailable 0 '
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusBlockPattern -Message "x64 UEFI AHCI drs-block read-only block publication proof was not observed."
    $uefiDriverReadStatusLoadPattern = (
        '\[x64\] drs-load 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-load-state 3 drs-load-flags 0x000007FF ' +
        'drs-load-owner 0x00001006 drs-load-user-owner 0x00000201 drs-load-fs-shell-bound 1 ' +
        'drs-load-binary-read 1 drs-load-checksum-verified 1 drs-load-mapped 1 ' +
        'drs-load-launched 1 drs-load-ls-completed 1 drs-load-source disk drs-load-bytes 170 ' +
        'drs-load-checksum (0x(?!00000000)[0-9A-F]{8}) drs-load-expected-checksum \1 ' +
        'drs-load-mapped-bytes 4096 drs-load-entry-rip 0x43000010 drs-load-exit-result 0x44524C31 ' +
        'drs-load-ls-bytes [1-9][0-9]* drs-load-write 0 drs-load-commit 0 drs-load-additional-fs-caps 0 ' +
        'drs-load-staged 1 drs-load-denials 0 drs-load-unavailable 0'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusLoadPattern -Message "x64 UEFI AHCI drs-load disk-sourced launch proof was not observed."
    $uefiDriverReadStatusLoadFullPattern = (
        '\[x64\] drs-load-full 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-load-full-state 3 ' +
        'drs-load-full-flags 0x00007FFF drs-load-full-owner 0x00001006 ' +
        'drs-load-full-user-owner 0x00000201 drs-load-full-load-bound 1 drs-load-full-fs-shell-bound 1 ' +
        'drs-load-full-binaries 10 drs-load-full-verified 10 drs-load-full-registered 10 ' +
        'drs-load-full-cat 1 drs-load-full-mkdir 1 drs-load-full-write 1 ' +
        'drs-load-full-rename 1 drs-load-full-move 1 drs-load-full-source disk ' +
        'drs-load-full-exit-result 0x44524C56 drs-load-full-exit-aux [1-9][0-9]* ' +
        'drs-load-full-write-escalation 0 drs-load-full-commit 0 drs-load-full-additional-fs-caps 0 ' +
        'drs-load-full-staged 1 drs-load-full-denials 0 drs-load-full-unavailable 0'
    )
    Assert-OutputContains -Lines $outputLines -Pattern $uefiDriverReadStatusLoadFullPattern -Message "x64 UEFI AHCI drs-load-full disk-sourced utility set proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] syscall input keyboard ps2-status 0x[0-9A-F]+ irq [0-9]+ polls [1-9][0-9]* scancodes [1-9][0-9]* bytes [0-9]+ pending [0-9]+ drops 0 last-scancode 0x[0-9A-F]+ last-byte 0x[0-9A-F]+' -Message "x64 UEFI PS/2 keyboard input telemetry proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] brokered keyboard read cap 0x[0-9A-F]+ result 1 first-byte 0x[0-9A-F]+ pending-before [1-9][0-9]* pending-after [0-9]+ keyboard-reads [1-9][0-9]* keyboard-read-bytes [1-9][0-9]*' -Message "x64 UEFI brokered keyboard read syscall proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] user second-page probe attempts [1-9][0-9]* exits [1-9][0-9]* result 0x32504752 recorded 0x32504752 expected 0x32504752' -Message "x64 UEFI kernel second-page userspace proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] user second-page probe attempts [1-9][0-9]* exits [1-9][0-9]* result 0x32504752 recorded 0x32504752 expected 0x32504752 .* display-pixels [1-9][0-9]* display-draws [1-9][0-9]* display-denials 0 display-unavailable 0 display-token 0x[0-9A-F]+ display-available 1 display-text-writes [1-9][0-9]* display-text-bytes [1-9][0-9]* display-clears [1-9][0-9]* display-console-writes [0-9]+ display-console-bytes [0-9]+ display-console-line-clears [0-9]+ display-console-wraps [0-9]+ display-console-scrolls [0-9]+' -Message "x64 UEFI brokered display text proof was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vfs-G4 proc maps-fd [3-9][0-9]* read [1-9][0-9]* regions [1-9][0-9]* bytes [1-9][0-9]* base 1 exe [3-9][0-9]*/13/1 fd-dir [3-9][0-9]*/1/[1-9][0-9]* fd-link [3-9][0-9]*/15/1 status [1-9][0-9]* cmd-env 16/12 deny 1 proc-io [1-9][0-9]*/[1-9][0-9]* cleanup 7/7/1/1 positive 1' -Message "x64 UEFI Linux VFS G.4 /proc/self checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vfs-G5 meminfo fd [3-9][0-9]* read [1-9][0-9]* total [1-9][0-9]* free [1-9][0-9]* available [1-9][0-9]* claimed [0-9]+ lines [5-9][0-9]* labels 1/1/1/1 parse 1 deny 1 cleanup 1/1 positive 1' -Message "x64 UEFI Linux VFS G.5 /proc/meminfo checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vfs-G6 tmp create-fd [3-9][0-9]* write 7 read-fd [3-9][0-9]* read 7 checksum 0x(?!00000000)[0-9A-F]{8} match 1 dir [3-9][0-9]*/1/2 entries 1 found 1 delete 1 post-deny 1 counts 1/1/[1-9][0-9]* backend [1-9][0-9]* ns 1 cleanup 1/1/1/1/1/1 positive 1' -Message "x64 UEFI Linux VFS G.6 /tmp writable namespace checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vfs-G7 symlink make 1 target-fd [3-9][0-9]* write 11 readlink 8/1 lstat 1/1/1 nofollow 1 follow-fd [3-9][0-9]* read 11 checksum 0x(?!00000000)[0-9A-F]{8} match 1 dir-link 1 counts 1/[1-9][0-9]*/1/1/1 target-bytes 8 cleanup 1/1/1/1/1/1/1/1 positive 1' -Message "x64 UEFI Linux VFS G.7 symlink path-walk checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vfs-G8 stat file 1/1/1/1/1 dir 1/1/1 link 1/1/1 follow 1/1/1 dev 1/1/0x000021B6 proc 1/1/1/0x(?!0000000000000000)[0-9A-F]{16} deny 1 cleanup 1/1 positive 1' -Message "x64 UEFI Linux VFS G.8 stat/lstat provider checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pipe-C6 block direct 1 block 0xFFFFFFFE tasks 0/0/4/1/2 write 5 wake 1/1/1/4294967295 back 1/0/2 read 5 checksum 0x(?!00000000)[0-9A-F]{8} match 1 avail 5/0 counts 1/1/1/1/0 cleanup 1/1/1/3/0 positive 1' -Message "x64 UEFI pipe C.6 scheduler-backed blocking checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F25 futex w 0x0000000000000000 k 0x0000000000000001 p 1 f 0 t [0-3]/[0-3]/4/1/1/1/0 v 42 e 0xFFFFFFFFFFFFFFF5 x 0xFFFFFFFFFFFFFFF2 b 0xFFFFFFFFFFFFFFEA n 0xFFFFFFFFFFFFFFEA/22 r 11/14/22 a [0-9]+/[0-9]+ d 1/1/1/1/2/1 l [0-9]+/0x00000000441C0080/42/1/[0-3] c 1 positive 1' -Message "x64 UEFI Linux ABI F.25 scheduler-backed futex checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F25b futex-timeout wait 0x0000000000000000 resume 0xFFFFFFFFFFFFFF92 states 4/2 waiters 1/0 pending 1/0 switch 1/1 ticks [1-9][0-9]*/[3-9][0-9]* sched 1/1 sleep 1/1 zero 0xFFFFFFFFFFFFFF92 invalid 0xFFFFFFFFFFFFFFEA fault 0xFFFFFFFFFFFFFFF2 counts 1/2/1/1/1 audit [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ records 110/110/22/14 last 4294967295/0/110 cleanup 1 positive 1' -Message "x64 UEFI Linux ABI F.25b timed futex checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F27 exec ret 0x0000000000000000 bytes 256 err 0 fd 5/1/4 vma 1/1/3 args 2/1 ready 1 rip 0x00000000441F0080 rsp 0x00000000441E[0-9A-F]{4} prot 0x00000005/0x00000003 clo 1/0 deny 0xFFFFFFFFFFFFFFF2/0xFFFFFFFFFFFFFFEA audit [0-9]+/[0-9]+ d 1/2/1 cleanup 3/1/1 positive 1' -Message "x64 UEFI Linux ABI F.27 execve/execveat checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F28 wait4 ret 0x00000000000020[0-9A-F]{2} child [0-9]+ status 0x00000700 exit 0x0000000000000000 pc 8/8/7 zombie 1/4/0 last 1/[0-9]+/7/0x00000700/1 rel 1/1 deny 0xFFFFFFFFFFFFFFF2/0xFFFFFFFFFFFFFFF6 audit [0-9]+/[0-9]+ d 1/1/2/1 c 1 positive 1' -Message "x64 UEFI Linux ABI F.28 wait4 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F29 kill ret 0x0000000000000000 tkill 0x0000000000000000 bad 0xFFFFFFFFFFFFFFEA entries 1/1 audit [0-9]+/[0-9]+ d 1/1/0/1 last 200/1/0/0 events 3/3/3 positive 1' -Message "x64 UEFI Linux ABI F.29 kill/tkill pending checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F30 getrandom ret 0x0000000000000010/0x0000000000000010 bad 0xFFFFFFFFFFFFFFEA fault 0xFFFFFFFFFFFFFFF2 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} different 1 entry 1 audit [0-9]+/[0-9]+ d 2/32/2/1 last 16/0x(?!00000000)[0-9A-F]{8}/0/0 events 3/3/3 cleanup 1 positive 1' -Message "x64 UEFI Linux ABI F.30 getrandom checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F31 positioned ret 0x0000000000000005/0x0000000000000006/0x0000000000000006 bad 0xFFFFFFFFFFFFFFEA fault 0xFFFFFFFFFFFFFFF2 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} match 1/1 offsets 0x0000000000000005/0x0000000000000005/0x0000000000000005/0x0000000000000005 preserved 1 entries 1/1 audit [0-9]+/[0-9]+ d 2/1/11/6/1/1 last 17/[3-9][0-9]*/0/0x000000000000000A/14 events 3/3/3/3/3 cleanup 1/1/1/1 positive 1' -Message "x64 UEFI Linux ABI F.31 pread64/pwrite64 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F32 vectored ret 0x000000000000000C/0x0000000000000008 bad 0xFFFFFFFFFFFFFFEA fault 0xFFFFFFFFFFFFFFF2 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} match 1/1 offsets 0x000000000000000C/0x0000000000000014/0x000000000000001C entries 1/1 audit [0-9]+/[0-9]+ d 1/1/12/8/1/1 last 19/[3-9][0-9]*/1/0/14 events 3/3/3/3 cleanup 1/1/1/1 positive 1' -Message "x64 UEFI Linux ABI F.32 readv/writev checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-abi-F33 poll ret 0x0000000000000000/0x0000000000000001 ppoll 0x0000000000000001 nval 0x0000000000000001 bad 0xFFFFFFFFFFFFFFEA fault 0xFFFFFFFFFFFFFFF2 revents 0x00000000/0x00000001/0x00000001/0x00000020 bytes 5 map 1/8 entries 1/1 audit [0-9]+/[0-9]+ d 3/1/3/2/1 last 7/1/0/0/14 events 3/3/3/3 cleanup 1/1/1 live ([0-9]+)/\1 positive 1' -Message "x64 UEFI Linux ABI F.33 poll/ppoll checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vdso-H1 map 1 validate 1 base 0x00000000441C0000 pte 1 prot 0x00000005 elf 1/2/1/3/62 phdr 2 seg 1/1 names 1/1/1 checksum 0x(?!00000000)[0-9A-F]{8} deny 1/1 fast 0x0000000000000000 tv 0x[0-9A-F]{16}/0x[0-9A-F]{16} counts 1/1/0/0 cleanup 1/1/0 positive 1' -Message "x64 UEFI Linux VDSO H.1 page checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-vdso-H2 aux 1 value 0x00000000441C0000 entries 19/19 validate 1 pte 1 prot 0x00000005 elf 1/2/1/3/62 phdr 2 stack 1/1 cleanup 1 positive 1' -Message "x64 UEFI Linux VDSO H.2 auxv mapping checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-signal-I1 sigaction-bytes 32 slots 64 pending 0x0000000000000000 mask 0x0000000000000000 zero 64/64/64 invalid 1 positive 1' -Message "x64 UEFI Linux signal I.1 context checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-signal-I2 rt-sigaction ret 0x0000000000000000/0x0000000000000000 deny 0xFFFFFFFFFFFFFFEA/0xFFFFFFFFFFFFFFF2 entry 1 signal 10 handler 0x0000000040123450/0x0000000040123450 old 0x0000000000000000 mask 0x0000000000000400/0x0000000000000400 flags 0x0000000001000000/0x0000000001000000 counts 0/2 query 0/1 denial 0/2 fault 0/1 audit [0-9]+/[0-9]+ events 3/3/3/3 results 0/0/22/14 cleanup 1 positive 1' -Message "x64 UEFI Linux signal I.2 rt_sigaction checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-signal-I3 rt-sigprocmask ret 0x0000000000000000/0x0000000000000000/0x0000000000000000 deny 0xFFFFFFFFFFFFFFEA/0xFFFFFFFFFFFFFFF2 entry 1 masks 0x0000000000000000/0x0000000000000200/0x0000000000000200/0x0000000000000200/0x0000000000000000 set 0x0000000000000300/0x0000000000000200 counts 0/3 query 0/1 denial 0/2 fault 0/1 audit [0-9]+/[0-9]+ events 3/3/3/3/3 results 0/0/0/22/14 last 0/14 cleanup 1 positive 1' -Message "x64 UEFI Linux signal I.3 rt_sigprocmask checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-signal-I4 inject kill 0x0000000000000000 deliver 1 masked 0 fault 0 pending 0x0000000000000000/0x0000000000000200/0x0000000000000000/0x0000000000000200/0x0000000000000000 mask 0x0000000000000000/0x0000000000000600/0x0000000000000000 handler 10/0x0000000040123450 frame 0x0000000044030[0-9A-F]{3} saved 0x00000000441000A4/0x0000000044030F00/0x0000000000000000 args 1/1/1 align 1 counts 0/1/0/1/0/1/0/1 audit [0-9]+/[0-9]+ events 3/3/3 results 0/0/14 last 10/14 cleanup 1 positive 1' -Message "x64 UEFI Linux signal I.4 injection checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-signal-I5 rt-sigreturn ret 0x00000000000000B5 fault 0xFFFFFFFFFFFFFFF2 invalid 0xFFFFFFFFFFFFFFEA entry 1 frame 0x0000000044030[0-9A-F]{3} restored 1/1/1 rip 0x00000000441000B5 rsp 0x0000000044030F00 mask 0x0000000000000600/0x0000000000000000/0x0000000000000000 rax 0x00000000000000B5 counts 0/1 denial 0/2 fault-count 0/1 audit [0-9]+/[0-9]+ events 3/3/3 results 0/14/22 last 0 cleanup 1 positive 1' -Message "x64 UEFI Linux signal I.5 rt_sigreturn checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-seh-I6 dispatch 1 invalid 0 pid [1-9][0-9]* bind 1/2 teb 0x0000000044240000 record 0x0000000044240100 handler 0x0000000044250180 frame 0x0000000044240[0-9A-F]{3} args 1/1/1/1/1 counts 0/1 denial 0/1 unhandled 0/0 audit 0/2 events 5/5 codes 0/0 results 0x00000000/0xC000000D/0xC000000D cleanup 1/1/1/1/0/1/1 positive 1' -Message "x64 UEFI Windows SEH I.6 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J1 header parse 1 machine 0x00008664 sections 3 entry 0x00001000 image-base 0x0000000140000000 image-size 0x00005000 headers 0x00000400 optional 240/0x0000020B peoff 0x00000080 dirs 16 deny 0/3 0/6 0/8 positive 1' -Message "x64 UEFI PE J.1 header parser checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J2 sections parse 1 count 3 names 1 attrs 1 ranges 1 text 0x00001000/0x00000200/0x00000400/0x00000200/0x60000020 rdata 0x00002000/0x00000100/0x00000600/0x00000200/0x40000040 data 0x00003000/0x00000100/0x00000800/0x00000200/0xC0000040 prot 5/1/3 totals 0x0000000000000600/0x0000000000000400 range 0x00001000/0x00003100 flags 1/3/1/1/2/0 checksum 0x(?!00000000)[0-9A-F]{8} deny 0/15 0/10 positive 1' -Message "x64 UEFI PE J.2 section parser checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J3 section-map 1 sections 3 bytes 0x0000000000003000 file 0x0000000000000500 bss 0x0000000000000100 base 0x0000000044200000 text-pte 1 text-prot 0x00000005 rdata-pte 1 rdata-prot 0x00000001 data-pte 1 data-prot 0x00000003 first 0x00000090/0x00000052/0x000000D0 bss-zero 1 source 0x(?!00000000)[0-9A-F]{8} mapped 0x(?!00000000)[0-9A-F]{8} denied 0 err 17 cleanup 3 positive 1' -Message "x64 UEFI PE J.3 section mapper checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J4 reloc 1 blocks 2 entries 4 applied 2 skipped 2 preferred 0x0000000044200000 actual 0x0000000044300000 delta 0x0000000000100000 section 0x00004000/0x00000040 fixups 0x00002000/0x00003000 values 0x0000000044202220/0x0000000044302220 0x0000000044203330/0x0000000044303330 prot 0x00000001/0x00000003 checksum 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} deny 0/21 cleanup 4 positive 1' -Message "x64 UEFI PE J.4 base relocation checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J5 imports 1 desc 1 thunks 1 names 1 ordinals 0 resolved 1 dir 0x00002000/0x00000080 dll 0x00002060 iat 0x00002080/0x00002080 value 0x0000000000002070/0x0000000077001234 funcs 0x0000000077001234/0x0000000077001234 prot 0x00000001 checksum 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} deny 0/32 cleanup 3 positive 1' -Message "x64 UEFI PE J.5 import resolver checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J6 tls 1 dir 0x00002020/0x00000028 raw 0x0000000044503000/0x0000000044503010 index 0x0000000044503020/7 callbacks 0x0000000044502048 block 0x0000000044504000/0x0000000000001000 bytes 16/8 first 0x33323130 zero 1 cb 1/1 cbva 0x0000000044501000/0x0000000044501000 reason 1 order 1/2/1 prot 0x00000001/0x00000003 checksum 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} deny 0/42 cleanup 4 positive 1' -Message "x64 UEFI PE J.6 TLS directory checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J7 pdata 1 dir 0x00002000/0x00000018 funcs 2/2 table 0x0000000044602000/0x0000000000000018 range 0x00001000/0x000010A0 unwind 0x00002040/0x00002048 first 0x00001000/0x00001050/0x00002040 ctx 1/1/2 prot 0x00000005/0x00000001 checksum 0x(?!00000000)[0-9A-F]{8} deny 0/48 cleanup 1/1 positive 1' -Message "x64 UEFI PE J.7 exception directory checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J8 teb 1 base 0x0000000044240000 peb 0x0000000044240800 stack 0x0000000044280000/0x0000000044270000 self 0x0000000044240000 gs (0x[0-9A-F]{16})/0x0000000044240000/0x0000000044240000/\1 fields 0xFFFFFFFFFFFFFFFF/0x0000000044240700/0x0000000044240800 ctx 1/1/2 pte 1/0x00000003 checksum 0x(?!00000000)[0-9A-F]{8} deny 0/50 cleanup 1/1 positive 1' -Message "x64 UEFI PE J.8 TEB setup checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J9 peb 1 image 0x0000000044700000/0x0000000044700000 peb 0x0000000044240800 gs 0x0000000044240800/0x0000000044700000 params 0x0000000044240A00/0x0000000044240A00 os 10/0/22621 nt 0x00000000 strings [1-9][0-9]*/[1-9][0-9]*/[1-9][0-9]* checksums 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} ctx 1/1/2 map 3 deny 0/55 cleanup 1/1 positive 1' -Message "x64 UEFI PE J.9 minimal PEB checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J10 kuser 1 base 0x000000007FFE0000 bytes 4096 time 0x[0-9A-F]{8}/0x[0-9A-F]{8} ticks [0-9]+/[1-9][0-9]* product 1 prot 1/0x00000001 root [1-9][0-9]* checksums 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} ctx 1/1/[2-9][0-9]* high 1 advance 1 deny 0/58 cleanup 1/1 positive 1' -Message "x64 UEFI PE J.10 KUSER_SHARED_DATA checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J11 cookie 1 dir 0x00002040/0x00000060 field 0x0000000044802098/0x0000000044803020 before 0x0000000000000000 after 0x(?!0000000000000000)[0-9A-F]{16} second 0x(?!0000000000000000)[0-9A-F]{16} checksums 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} prot 0x00000003 ctx 1/1/2 unique 1 deny 0/63 cleanup 1/1 second-cleanup 1/1 positive 1' -Message "x64 UEFI PE J.11 security cookie checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] pe-J12 entry 1 rip 0x0000000044A01000/0x0000000044A01000 rsp 0x0000000044B00FE0 stack 0x0000000044B00000/0x0000000044B01000 args 0x0000000000000001/0x0000000044A00000/0x0000000000000000 pages 1/0x00000005/1/0x00000003 transfer 1/1/0x50453132/0x00000007 selectors 0x002B0033 rflags 0x00000002 ctx 1/1/2 deny 0/72 ntdll 1/0x0000000044B81000 pte 1/0x00000005/1/0x00000005 exe 1 rip 0x0000000044A01000/0x0000000044B81000 args 0x0000000044A01000/0x0000000044A00000/0x0000000044B03000 ready 1/1 pages 1/0x00000005/1/0x00000003 result 0x50453132/0x00000007 ctx 1/1 err 0 cleanup 1/1 positive 1' -Message "x64 UEFI PE J.12 entry transfer checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K1 table 1 size 512 unimpl 492 entries 0/0/0/0/0/0/0/0/0/0/0/0/0/0/0/0 bind 1/2/1 ret 0xC0000002 invalid 0xC000001C audit 0/1/2 record 2/7/0xC0000002/2 rip 0x00000000F2000007 counts 2/1/1 last 1/512/0xC000001C/0x00000000F2000200 cleanup 1/1/1/1 positive 1' -Message "x64 UEFI Windows ABI K.1 switchboard checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K2 write ret 0x00000000 handle 4 bytes 3/3 iosb 0x00000000/0x0000000000000003 console [0-9]+/[0-9]+ bytes-total [0-9]+/[0-9]+ audit 0/1/2/3 record 3/8/0x00000000/2 bad 0xC0000008/0xC0000008 fault 0xC0000005 counts 1/1/1 cap 0x(?!FFFFFFFF)[0-9A-F]{8} last 4/0/0xFFFFFFFF/0xC0000005 checksum 0x(?!00000000)[0-9A-F]{8} cleanup 1/1/1/0/1/1 positive 1' -Message "x64 UEFI Windows ABI K.2 NtWriteFile checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K3 read ret 0x00000000 handle 12 bytes [0-9]+/[0-9]+ iosb 0x00000000/0x[0-9A-F]{16} input [0-9]+/[0-9]+ bytes-total [0-9]+/[0-9]+ audit 0/1/2/3 record 3/6/0x00000000/2 bad 0xC0000008/0xC0000008 fault 0xC0000005 counts 1/1/1 cap 0x(?!FFFFFFFF)[0-9A-F]{8} last 12/0/0xFFFFFFFF/0xC0000005 checksum 0x(?!00000000)[0-9A-F]{8} match [01] cleanup 1/1/1/0/1/1 positive [01]' -Message "x64 UEFI Windows ABI K.3 NtReadFile checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-handle-K4 init 1/1 handle 0x0000000000000040 dup 0x0000000000000044 child 0x0000000000000040 cap 0x(?!FFFFFFFF)[0-9A-F]{8} match 1/1/1/1 live 1/2/1 ref 1/2/2/1 close-dup 1/1 inherit 1/1 child-state 1/0x00000004/1/1 pseudo 1/5/1/1/6/1 deny 0/1/2 bad 0/0xC0000008 protect 0/0xC0000008/1 audit 0/1/2 records 4/75/0xC0000008/4/75/0xC0000008 close 1/1/1/1 release 1/0 cleanup 1/1/1/1/1/1/1/1 counts 2/2/3/2/2 high 0x0000000000000044 last 0x0000000000000040/0x(?!FFFFFFFF)[0-9A-F]{8}/0x00000000 positive 1' -Message "x64 UEFI Windows handle K.4 object table checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-K5 create ret 0x00000000 handle 0x0000000000000040 type 1 rights 0x00000005 ref 1 live 1 cap 0x(?!FFFFFFFF)[0-9A-F]{8} owner 1 route 0x[0-9A-F]{8}/0x[0-9A-F]{8} iosb 0x00000000/0x0000000000000001 audit 0/1/2/3 record 3/85/0x00000000/2 bad 0xC0000034/0xC0000034 fault 0xC0000005 counts 1/1/1 vfs 1/1 last 0xC0000005/0x00000000/0/0 vfs-last 0xC0000034/0x(?!00000000)[0-9A-F]{8}/35/0/0x00000000 close 1 release 0 cleanup 1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows VFS K.5 NtCreateFile checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K6 alloc ret 0x00000000 entry 1 base 0x0000000045001000/0x0000000045001000 size 0x0000000000001000 pte 1 prot 0x00000003 regions 2 mapped 0x0000000000002000 audit 0/1/2/3 record 3/24/0x00000000/2 deny 0xC0000002/0xC0000002 fault 0xC0000005/0xC0000005 counts 1/1/1 bytes 4096 last 0xC0000005/0x0000000000000000/0x0000000000000000/0x00000000/0x00000000 cleanup 1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.6 NtAllocateVirtualMemory checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K7 free ret 0x00000000 entry 1 base 0x0000000045021000/0x0000000045021000 size 0x0000000000001000 pte 1/0 regions 2/1 mapped 0x0000000000002000/0x0000000000001000 audit 0/1/2/3 record 3/30/0x00000000/2 deny 0xC0000002/0xC0000002 fault 0xC0000005/0xC0000005 counts 1/1/1 bytes 4096 last 0xC0000005/0x0000000000000000/0x0000000000000000/0x00000000 cleanup 1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.7 NtFreeVirtualMemory checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K8 protect ret 0x00000000 entry 1 base 0x0000000045041000/0x0000000045041000 size 0x0000000000001000 old 0x00000004 pte 1/1 prot 0x00000003/0x00000005 regions 2/2 mapped 0x0000000000002000/0x0000000000002000 audit 0/1/2/3 record 3/80/0x00000000/2 deny 0xC0000002/0xC0000002 fault 0xC0000005/0xC0000005 counts 1/1/1 bytes 4096 last 0xC0000005/0x0000000000000000/0x0000000000000000/0x00000000/0x00000000 cleanup 1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.8 NtProtectVirtualMemory checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K9 events create 0x00000000/0x00000000 entries 1/1/1 handles 0x0000000000000040/0x0000000000000044 types 3/3 rights 0x00000005/0x00000005 ref 1/1 manual 0/1 state 0/1/0/1 set 0x00000000 prev 0 wait 0x00000000/0x00000000 deny 0xC0000008/0xC0000008 fault 0xC0000005/0xC0000005 audit 0/1/2/3/4/5/6/7 record 3/72/0x00000000/2 seq 226/4/72/4 counts 2/1/2/1/1 handle-counts 2/1/2/1 live 2/2/2/0 last 0/0/0/0xC0000005 cleanup 1/1/0/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.9 event syscall checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K10 mutant create 0x00000000 entries 1/1/1 handles 0x0000000000000040/0x0000000000000040 types 4/4 rights 0x00000005/0x00000005 ref 1/1 states 1/1/1/0/1/1/1/0 wait 0x00000102/0x00000000 release 0x00000000/0x00000000 prev 1/1 deny 0xC0000046/0xC0000046 audit 0/1/0/1/2/2/3/3 record 3/87/0x00000000/2 seq 4/172/4/172 counts 1/2/2/1/0 handle-counts 1/2/2/1 live 1/0 last 64/0/4294967295/0/0xC0000046 cleanup 1/1/0/0/1/1/1/1/1/1/1/1/0/0/1/1 positive 1' -Message "x64 UEFI Windows ABI K.10 mutant syscall checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K10b wait-block direct 0xC000000D wait 0x00000000 release 0x00000000/0x00000000 tasks [0-9]+/[0-9]+/4/2/1/[0-9]+/2 sched 1/1/0 owner 1/1/1/0 prev 1/1 audit [0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+/[0-9]+ records 4/0xC000000D/4/0x00000000/172/172 counts 2/2/1/2/2/0 cleanup 1 positive 1' -Message "x64 UEFI Windows ABI K.10b wait-block checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K10c wait-timeout 0x00000000/0x00000000/0x0000000000000102/0xC0000002 s [0-9]+/4/2/4294967295 p 1/0/[3-9][0-9]* q 1/1/1/1 c 1/1/1 l [0-9]+/3/0x00000102 a [0-9]+/[0-9]+/[0-9]+ r 4/0x00000102/0xC0000002 x 1/1' -Message "x64 UEFI Windows ABI K.10c timed wait checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K11 query basic 0x00000000 entry 1 peb 0x0000000044240800/0x0000000044240800 pid [0-9]+/[0-9]+ parent 0 ret 48/8/92 debug 0x00000000/0x0000000000000000 image 0x00000000 len 74/76 buf 0x0000000045080190 checksum 0xC70F0AE4 deny 0xC0000008/0xC0000008 audit 0/1/2/3/4 records 3/25/0x00000000/4/0xC0000008 counts 3/1/0 last 27/0x00000000/0x0000000044240800/92 cleanup 1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.11 query-process syscall checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-abi-K12 system basic 0x00000000 entry 1 page 4096 pages [1-9][0-9]*/[0-9]+ gran 0x00010000 user 0x0000000000010000/0x00007FFFFFFFFFFF cpu 1/0x0000000000000001 processor 0x00000000/9/[1-9][0-9]*/[0-9]+/1/0x(?!00000000)[0-9A-F]{8} perf 0x00000000 pages [0-9]+/1/[1-9][0-9]*/1 vma 1/[1-9][0-9]* psize 4096 ret 64/12/312 checksum 0x(?!00000000)[0-9A-F]{8} deny 0xC0000003/0xC0000003 audit 0/1/2/3/4 records 3/54/0x00000000/3/0xC0000003 counts 3/1/0 last 2/0x00000000/312/4096/1/[1-9][0-9]*/[0-9]+ cleanup 1/1/1/0/1 positive 1' -Message "x64 UEFI Windows ABI K.12 query-system syscall checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K13 ntdll load 1 base 0x0000000044C00000 image 12288/1536 sections 2/2 symbols 29 checksum 0x(?!00000000)[0-9A-F]{8} text 1/0x00000005/0x(?!00000000)[0-9A-F]{8} rdata 1/0x00000001/0x(?!00000000)[0-9A-F]{8} exports 0x0000000044C01000/0x0000000044C01060/0x0000000044C01158/0x0000000000000000 registry 1/1/29 ctx 1/1 handoff 1/1 rip 0x0000000044C01000 exe 1/0x00000005 deny 0/3 counts 1/1 last 0x0000000044C00000 cleanup 1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.13 ntdll checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K13b ntdll-syscall load 1 export 0x0000000044C01158 maps 0x0000000044D00000/0x0000000044D01000 copy 1/0x00000007/1 run 0x4B313342/29 task [0-9]+/1/[1-9][0-9]* ret 0x0000000000000000 iosb 0x00000000/0x0000000000000004/0x0000000000000004 console 1/4 native 1 persona 1/1 write 1 audit 1/1/3/8/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 match 1/1 cleanup 1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.13b ntdll syscall checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K14 kernel32 load 1 dep 1/1 base 0x0000000044F00000 image 12288/3584 sections 2/2 symbols 27 checksum 0x(?!00000000)[0-9A-F]{8} text 1/0x00000005/0x(?!00000000)[0-9A-F]{8} rdata 1/0x00000001/0x(?!00000000)[0-9A-F]{8} exports 0x0000000044F01000/0x0000000044F01040/0x0000000044F01120/0x0000000000000000 registry 1/2/29/27 ctx 1/1 bridge 0x00000FFF live 16/11 import 1/1 iat 0x0000000000002070/0x0000000044F01040 deny 0/10 import-deny 0/32 counts 1/1 last 0x0000000044F00000 cleanup 1/1/1/1/1/1/1/1/1/0 positive 1' -Message "x64 UEFI Windows shim K.14 kernel32 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K14b kernel32-console maps 0x0000000044DC0000/0x0000000044DD0000 copy 1/0x00000007/1 run 0x4B313442/16 task [0-9]+/1/[1-9][0-9]* handle 0x0000000000000004 ret 0x0000000000000001/4 console 1/4 native 1 persona 1/1 write 1 audit 1/1/3/8/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 match 1/1 cleanup 1/1/1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.14b kernel32 console checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K14c imported-pe launch base 0x0000000044DC0000/0x0000000044DD0000 deps 1/1/1 parse 1/1/1/1/1/1 imports 1/2/2 iat 0x00000000000020A0/0x0000000044D81020/0x00000000000020C0/0x0000000044D81040 run 0x4B313443/2 task [0-9]+/1/[1-9][0-9]* ret 0x0000000000000004/0x0000000000000001/4 console 1/4 native 1 persona 1/1 write 1 audit 1/1/3/8/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 match 1/1 cleanup 1/1/1/1/1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.14c imported PE launch checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K14d kernel32-vmheap maps 0x0000000044DE0000/0x0000000044DF0000 deps 1/1 run 0x44DF0000/16 task [0-9]+/1/[1-9][0-9]* vm 0x(?!0000000000000000)[0-9A-F]{16}/0x00000004/0x0000000000000001/0x0000000000000001 heap 0x0000000048EE0001/0x(?!0000000000000000)[0-9A-F]{16}/0x(?!0000000000000000)[0-9A-F]{16}/0x1122334455667788/0x0000000000000001 deny 0x0000000000000000 sys 7/7/7 vmcalls 3/1/3 audit 7/1/3/30/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 match 1/1 cleanup 1/1/1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.14d kernel32 VM/heap checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K16 real-pe launch base 0x0000000044E40000/0x0000000044EC0000 deps 1/1/1 parse 1/1/1/1/1/0/0/0 sections 5/5 imports 1/3/3 iat 0x0000000000005068/0x0000000044D81000/0x0000000000005076/0x0000000044D81020/0x0000000000005086/0x0000000044D81040 run 0x4B313658/0 task [0-9]+/1/[1-9][0-9]* console 1/4 native 2 persona 2/2 calls 1/1 audit 2/1/3/44/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 term [1-9][0-9]*/0x00000000/0x00000000 match 1/1 cleanup 1/1/1/1/1/1/1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.16 real PE launch checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K17 heap-pe launch base 0x0000000045100000/0x0000000045180000 deps 1/1/1 parse 1/1/1/1/1/0/0/0 sections 5/5 imports 2/9/9 iat 1/0x0000000044F01100/0x0000000044F01330/0x0000000044C011D0 run 0x4B313658/0 task [0-9]+/1/[1-9][0-9]* console 1/9 native 7 persona 7/7 calls 1/1/1/0/1/1/1/1/1 handles [0-9]+/[0-9]+/0 audit 7/1/3/44/0x00000000 last [1-9][0-9]*/2/0x0000000000000000 close 64/0x00000000 set 14/4/0x00000000 term [1-9][0-9]*/0x00000000/0x00000000 match 1/1/1 cleanup 1/1/1/1/1/1/1/1/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows shim K.17 heap PE launch checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-shim-K15 crt load 1 dep 1/1/1 base 0x0000000045400000 image 12288/2560 sections 2/2 symbols 21 checksum 0x(?!00000000)[0-9A-F]{8} text 1/0x00000005/0x(?!00000000)[0-9A-F]{8} rdata 1/0x00000001/0x(?!00000000)[0-9A-F]{8} exports 0x0000000045401000/0x0000000045401020/0x0000000045401080/0x0000000045401180/0x0000000045401240/0x0000000000000000 registry 1/4/29/27/21/21 crt-reg 1/2/21/21 ctx 1/1 bridge 0x000000FF live 0/21 parse 1/1/1/0/0/0 import 1/2/2 iat 0x0000000000002080/0x0000000045401000/0x0000000000002090/0x0000000045401240 deny 0/10 import-deny 0/32 counts 1/1 last 0x0000000045400000 cleanup 1/1/1/1/1/1/1/1/1/1/1/0 positive 1' -Message "x64 UEFI Windows shim K.15 CRT checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-L1 route bind 1 c 0x00000000/1/0x(?!00000000)[0-9A-F]{8}/35/5 unc 0xC0000002/2/1/0x(?!00000000)[0-9A-F]{8}/30/10 con 0x00000000/3/1/12/4 null 0x00000000/4/2/9 bad 0xC0000034/0 wrong 0xC000000D/0 counts 4/3 last 0/0x00000000/0/0/0 cleanup 1/0 positive 1' -Message "x64 UEFI Windows VFS L.1 NT namespace router checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-L2 system32 bind 1/1 ok 15 miss 0xC0000034 ids 0x04030201 masks 15/15/15 counts 4/1/5/0 live 4/0 last 0xC0000034/0 cleanup 0/1/1/1 positive 1' -Message "x64 UEFI Windows VFS L.2 System32 shim mapping checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-L3 users bind 1/1 create 0x00000000/1/1/1 node [1-9][0-9]*/[1-9][0-9]* mask 0x000000FF deny 0xC0000022 counts 1/1/1/1/2/1 last 0xC0000022/5 live 1/0 cleanup 1/0/1/1/1 positive 1' -Message "x64 UEFI Windows VFS L.3 user profile namespace checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-L4 temp bind 1/1 create 0x00000000/1/1/1 io 0x00000000/7/0x00000000/7/0x(?!00000000)[0-9A-F]{8}/1 delete 0x00000000/0xC0000034 node [1-9][0-9]*/[1-9][0-9]* mask 0x00000003 counts 1/1/1/1/1/1/1/3/0 last 0xC0000034/6 live 1/0 cleanup 1/0/1/1/1 positive 1' -Message "x64 UEFI Windows VFS L.4 temp namespace checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] windows-vfs-L5 registry entries 1/1/1 open 0x00000000/0x00000040/2 query 0x00000000/1/10/22/22/0x(?!00000000)[0-9A-F]{8}/1 create 0x00000000/2/16 miss 0xC0000034 handles 7/7 caps 1/1 live 2/0 counts 1/1/1/1/0 reg 1/1/1/1/1 last 23/0xC0000034/2/0/0 audit 0/4 record 4/23/0xC0000034/2 cleanup 1/1/0/1/1/1/1/0/1 positive 1' -Message "x64 UEFI Windows VFS L.5 registry checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M1 header parse 1 magic 0xFEEDFACF cpu 0x01000007 subtype 3 filetype 2 cmds 3 size 0x00000100 range 0x00000020/0x00000120 flags 0x00200085 deny 0/3 0/4 0/5 0/6 0/9 positive 1' -Message "x64 UEFI Mach-O M.1 header parser checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M2 fat slice 1 magic 0xCAFEBABE arches 2 index 1 cpu 0x01000007 subtype 3 off 0x00000100 size 0x00000120 align 12 thin 1/6/2/0x00000080 deny 0/11 0/12 0/13 0/16 0/15 positive 1' -Message "x64 UEFI Mach-O M.2 fat binary slicer checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M3 segment-map 1 segments 3/3 bytes 0x0000000000003000 file 0x00000000000000D0 bss 0x0000000000002F30 text 1/1/0x00000005/0x000000A0 data 1/1/0x00000003/0x000000B0 linkedit 1/1/0x00000001/0x000000C0 bss-zero 1 source 0x(?!00000000)[0-9A-F]{8} mapped 0x(?!00000000)[0-9A-F]{8} denied 0 err 21 cleanup 3/1/1 positive 1' -Message "x64 UEFI Mach-O M.3 LC_SEGMENT_64 mapper checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M4 lc-main 1 entry 0x0000000046100010 off 0x0000000000000010 text 1/0x0000000046100000/0x0000000000001000/1 page 1/0x00000005 stack 1/1/0x0000000000800000/0x0000000000001000 base 0x0000000046A00000 top 0x0000000047200000 rsp 0x0000000047200000 spage 1/0x00000003 deny 0/28 cleanup 1/1 positive 1' -Message "x64 UEFI Mach-O M.4 LC_MAIN entry checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M5 dylib walk 1 cmds 2/1 record 1/1/1/0 first 1/0x(?!00000000)[0-9A-F]{8} dep 1/0/1/1/24/26/0x12345678/0x00010000/0x00010000 path 1 deny 0/34/1/1 positive 1' -Message "x64 UEFI Mach-O M.5 LC_LOAD_DYLIB checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M6 dyld fixups 1 info 1/1 reb 1/0x0000000046101020/0x0000000046100000/0x0000000046101000/0x0000000046101000 bind 1/1/1/5/0x(?!00000000)[0-9A-F]{8}/0x0000000046101028/0x0000000047301000/0x0000000047301000 ranges 0x00003020/0x00000008/0x00003040/0x00000010/0x00003060/0x00000004 deny 0/43/0 positive 1' -Message "x64 UEFI Mach-O M.6 dyld rebase/bind checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M7 tls 1 sections 3/1/1/1 vars 0x0000000046210000/0x0000000000000018 regular 0x0000000046210020/0x0000000000000010 zero 0x0000000046210030/0x0000000000000008 block 0x0000000046220000/0x0000000000001000 template 0x0000000046220008/0x0000000000000010/0x33323130 gs 0x[0-9A-F]{16}/0x0000000046220000/0x0000000046220000/0x0000000046220000/0x[0-9A-F]{16} checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} pte 1/0x00000003 ctx 1/1/3 deny 0/46 cleanup 1/1/1 positive 1' -Message "x64 UEFI Mach-O M.7 TLS checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macho-M8 stack 1 rsp 0x0000000046230F10 layout 240 strings 110 slots 16 counts 2/2/2/3 ptrs 0x0000000046230F92/0x0000000046230FA9/0x0000000046230FCD/0x0000000046230FE8/0x0000000046230F60 reads 0x0000000000000002/0x0000000046230F92/0x0000000046230FA9/0x0000000046230FCD/0x0000000000000001/0x0000000046100010 nulls 1/1/1/1 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} page 1/0x00000003 ctx 1/3 deny 0/52 cleanup 1/1/1 prefix 1 positive 1' -Message "x64 UEFI Mach-O M.8 initial stack checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macos-abi-N1 table 512/499 entries 13 ctx 1 setup [0-9]+/1/1/1/1/1 getpid 0x[0-9A-F]{16} write 0x0000000000000003 console 1/3 open 0x0000000000000003 read 0x0000000000000004/0x(?!00000000)[0-9A-F]{8} stat 0x0000000000000000/0x(?!0000000000000000)[0-9A-F]{16}/0x(?!00000000)[0-9A-F]{8} fstat 0x0000000000000000 close 0x0000000000000000/1 mmap 0x0000000046250000 pte 1/0x00000003 protect 0x0000000000000000/0x00000001 munmap 0x0000000000000000/0 clock 0x0000000000000000/0x[0-9A-F]{16}/0x[0-9A-F]{16} sysctl 0x0000000000000000/12/1/1/1/12 deny 0xFFFFFFFFFFFFFFEA unimpl 0xFFFFFFFFFFFFFFB2 audit 0/14/1/2/511/78/3 exit 0x0000000000000000/1/3/1/1 cleanup 1/1 positive 1' -Message "x64 UEFI macOS ABI N.1 switchboard checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macos-mach-N2 table 64/59 entries 5 setup [0-9]+/1/1/1/1 ports 0x000000000000A200/0x000000000000A201/0x000000000000A202/0x000000000000A203 live 4/0 kinds 1/2/3 reply-rights 0x0000000D backing 1/1/0x(?!00000000)[0-9A-F]{8} msg 0x0000000000000000/0x0000000000000000 pending 1/0 recv 0x00004D32/28/0x0000A203/0x0000A203/0x(?!00000000)[0-9A-F]{8} counts 1/1/1 deny 0x0000000010000003 unimpl 0x0000000000000004 audit 0/8/1/2/63/4/3 last 63/0x00000004/3/1 cleanup 4/1/1/1/1/1 positive 1' -Message "x64 UEFI macOS Mach N.2 trap table checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macos-shim-N3 load 1/0 err 0/3 syms 16/16 addrs 0x0000000047301000/0x00000000473011C0/0x0000000000000000 map 2/1/1 prot 0x00000005/0x00000001 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} printf 13/1/13 strlen 13 mem 0x0000000047340000/0x00000000473400C0/1/0 mmap 0x0000000046280000/1/0x00000001/0x0000000000000000 open 0x0000000000000003/0x0000000000000000 clock 0x0000000000000000/1 bad 6 counts 1/13/7/7/2/0 audit [0-9]+/[0-9]+ last 0/6/0x0000000000000000 cleanup 3/0/3/1/1/0/1/1 positive 1' -Message "x64 UEFI macOS libSystem N.3 shim checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macos-dyld-N4 load 1/0 err 0/3 syms 3/3 addrs 0x0000000047311000/0x0000000047311020/0x0000000047311040/0x0000000000000000 map 2/1/1/1 prot 0x00000005/0x00000001 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} bind 0x0000000046290040/0x0000000047301000/0x0000000047301000/8/0x(?!00000000)[0-9A-F]{8} images 3/0x0000000047312100/30/1/0x(?!00000000)[0-9A-F]{8} bad 6/6 counts 1/3/1/2/3/0 audit [0-9]+/[0-9]+ last 0/6/0x0000000000000000 cleanup 2/1/1/1/0/1/1 positive 1' -Message "x64 UEFI macOS libdyld N.4 shim checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] macos-cf-N5 load 1/0 lib 1 err 0/3 syms 6/6 addrs 0x0000000047321000/0x0000000047321060/0x0000000047321080/0x00000000473210A0/0x0000000000000000 map 2/1/1/1 prot 0x00000005/0x00000001 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} alloc 0x0000000047322000 create 0x0000000047323000/18/0x(?!00000000)[0-9A-F]{8} retain 0x0000000047323000/2 get 1/18/1/1 show 18/1/18 release 1/0 live 1/1/1/0 bad 6 counts 1/7/1/1/1/1/2/2/0/1/1/1 audit [0-9]+/[0-9]+ last 0/6/0x0000000000000000 cleanup 3/2/1/3/1/1/0/1/1 positive 1' -Message "x64 UEFI macOS CoreFoundation N.5 shim checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o1 cap-tag pids 1/2 bind 1/1 masks 0x00000002/0x00000004 caps 0x(?!FFFFFFFF)[0-9A-F]{8}/0x(?!FFFFFFFF)[0-9A-F]{8} delegates 0x(?!FFFFFFFF)[0-9A-F]{8}/0xFFFFFFFF/0x(?!FFFFFFFF)[0-9A-F]{8} tags 1/0x00000002/1/0x00000002/255/0x00000000 routes 4/4 denials 1/1 audit [0-9]+/[0-9]+/1/4/4/1/2 cleanup 1/1/1/1/1/1 positive 1' -Message "x64 UEFI persona O.1 capability attenuation checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o2 syscall-audit bind 1/1 map 1 calls 1/1/1/1/1 count 0/5/5 reads 1/1/1/1/1 events 3/3/3/3/3 codes 39/186/158/218/228 persona 1/1/1/1/1 results 0/0/0/0/0 ops 3/3/3/3/4 names 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} match 1/1/1/1/1 time 1 last 0x(?!00000000)[0-9A-F]{8}/4 cleanup 1/1/1/1 positive 1' -Message "x64 UEFI persona O.2 syscall audit metadata checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o3 budgets bind 1/1/1/1 defaults 16384/1024/64 config 1/1 vma 0x00000000472C0000/0xFFFFFFFFFFFFFFF4 pages 1 vdeny 1/2/1 fd 0x0000000000000003/0xFFFFFFFFFFFFFFE8 live 4 fdeny 2/5/4 pipe 0x0000000000000000/0xFFFFFFFFFFFFFFE8 fds 4/5 count 1/0 pdeny 3/2/1 denials 3 audit 0/6 cleanup 1/1/1/1/3/1/1/0/1 positive 1' -Message "x64 UEFI persona O.3 resource budget checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o4 isolation bind 1/1/1 groups [1-9][0-9]*/[1-9][0-9]* ret 0x0000000000000000/0xFFFFFFFFFFFFFFFD audit 0/2/1 record 4/62/3/1/1 last [1-9][0-9]*/[1-9][0-9]*/[1-9][0-9]*/[1-9][0-9]*/1 denials 1 types 1/2 cleanup 1/1/1/1/1/1 positive 1' -Message "x64 UEFI persona O.4 process-group isolation checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o5 unavailable bind 1/1/1/1/1/1 ret 0xFFFFFFFFFFFFFFDA/0x00000000C0000002/0xFFFFFFFFFFFFFFB2 expected 0xFFFFFFFFFFFFFFDA/0x00000000C0000002/0xFFFFFFFFFFFFFFB2 abi 0x00000026/0xC0000002/0x0000004E audit 0/1/1 0/1/1 0/1/1 records 2/511/0x00000026/1/255 2/7/0xC0000002/2/255 2/511/0x0000004E/3/255 deltas 1/1/1 cleanup 1/1/1/1/1/1/1/1/1/1 positive 1' -Message "x64 UEFI persona O.5 truthful unavailability checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persona-o6 crash-report bind 1/1/1 record 1/0 audit 0/1/1 crash 0/1/1 event 5/11/11/1/0x0000000044600BAD frame 14/0x0000000000000004/0x0000000044600BAD/0x0000000047600F00/0x0000000000000000 vma 1/0x0000000000001000/0x0000000047600000/0x0000000047601000/0x00000003/0 cleanup 1/1/1/0/1/1 positive 1' -Message "x64 UEFI persona O.6 crash reporting checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-dynamic-P1 ld init 1 prepare 1/0 counts 1/1/1/1 interp 3/1/1/10/0x(?!00000000)[0-9A-F]{8} prot 0x00000005/0x00000005 aux 0x0000000047800000/0x0000000047900200/1/1 deps 0/1/1/8 audit 0/1/1/4/8 ctx 1/1/1 cleanup 3/1/1/1/0/1/1 positive 1' -Message "x64 UEFI Linux dynamic linker P.1 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2 shim init 1 prepare 1 counts 1/1/1/1/2 needed 1/1/0/1 libc 1/1/3/1/1/67/33/3/5/4/4/1/0/0x(?!00000000)[0-9A-F]{8} prot 0x00000005 exports 0x0000000047811020/0x0000000047811240/0x0000000047811480/0x0000000047811400/1/1 deps 1/0 deny 0/3 audit 0/1/1/4/3 ctx 1 fd 1/[3-9][0-9]* cleanup 5/1/1/1/0/1/1 positive 1' -Message "x64 UEFI Linux libc shim P.2 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-pthread-P3 alias init 1 prepare 1/0 counts 1/1/1/1/1/1 needed 2/2/0/1/1 needok 1 checks 0x(?!00000000)[0-9A-F]{8}/0x(?!00000000)[0-9A-F]{8} pthread 1/1/1/1/1/0 libc 1/1/67/1/1/1/2/5 exports 1 ctx 1 deny 8/1/1/1 audit 0/1/1/4/8 cleanup 5/1/1/1/0/1/1 positive 1' -Message "x64 UEFI Linux libpthread alias P.3 checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2b helpers maps 0x00000000479A0000/0x00000000479B0000 symbols 5/0 exec 0x00000007 run 0x50324231/5 match 1/1/1/1/1/1/1 rets 0x00000000479B0020/0x00000000479B0060/0x00000000FFFFFFFF/0x0000000000000000/0x00000000FFFFFFFF/0x00000000479B00A2 checksum 0x(?!00000000)[0-9A-F]{8} copy 1/1 cleanup 1/1/0/0 positive 1' -Message "x64 UEFI Linux libc P.2b string/memory helper checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2c puts maps 0x00000000479C0000/0x00000000479D0000 fd 1 symbols 4/0 run 0x50324331/4/[1-9][0-9]* ret 0x0000000000000009 console 2/9 native 2 persona 2/2 write 2 audit 2/1/3/1/0 last [1-9][0-9]*/1/0x0000000000000001 match 1/1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2c puts/helper syscall-routing checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2d printf maps 0x00000000479E0000/0x00000000479F0000 fd 1 symbols 4/0 run 0x50324431/4/[1-9][0-9]* ret 0x000000000000000B/0xFFFFFFFFFFFFFFDA console 1/11 native 1 persona 1/1 write 1 audit 1/1/3/1/0 last [1-9][0-9]*/1/0x000000000000000B match 1/1/1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2d literal printf checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2e malloc maps 0x00000000479A0000/0x00000000479B0000/0x00000000479C0000 load 1 symbols 4/0 copy 1/0x00000007/1 run 0x50324531/4/[1-9][0-9]* ptr 0x00000000479D0000/0x00000000479D0000 free 0x0000000000000000 magic 0x3265706165682D70 native 2 persona 2/2 mmap 1/4096 munmap 1/4096 audit 2/1/3/11/0 last [1-9][0-9]*/1/0x0000000000000000 match 1/1/1/1/1/1 cleanup 1/1/1/0/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2e malloc/free checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2f calloc-realloc maps 0x00000000479A0000/0x00000000479B0000/0x00000000479C0000 symbols 4/0 copy 1/0x00000007 run 0x50324631/4/[1-9][0-9]* ptr 0x00000000479D0000/0x00000000479E0000 free 0x0000000000000000 zero 0x0000000000000004 magic 0x2170616568663270 native 4 persona 4/4 mmap 2/8192 munmap 2/8192 audit 4/1/3/11/0 last [1-9][0-9]*/1/0x0000000000000000 match 1/1/1/1/1/1/1/1 cleanup 1/1/1/0/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2f calloc/realloc checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2g fputs-fwrite maps 0x00000000479A0000/0x00000000479B0000 fd 1 symbols 4/0 copy 1/0x00000007/1 run 0x50324731/4/[1-9][0-9]* ret 0x000000000000000A/0x000000000000000B console 2/21 native 2 persona 2/2 write 2 audit 2/1/3/1/0 last [1-9][0-9]*/1/0x000000000000000B match 1/1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2g fputs/fwrite checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2h getenv maps 0x00000000479A0000/0x00000000479B0000 symbols 2/0 bind 1/0x(?!0000000000000000)[0-9A-F]{16}/4 copy 1/0x00000007/1 run 0x50324831/2/[1-9][0-9]* ret 0x(?!0000000000000000)[0-9A-F]{16}/0x0000000000000000/0x0000000000000000/0x(?!0000000000000000)[0-9A-F]{16}/0xFFFFFFFFFFFFFFF4 native 0 persona 0/0 write 0 audit 0 match 1/1/1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2h getenv/setenv checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2i errno maps 0x00000000479A0000/0x00000000479B0000 symbols 1/0 cell 0x0000000047811460/0x0000000047812000/0x0000000047812000 prot 0x00000007/0x00000003 copy 1 run 0x50324931/1/[1-9][0-9]* value 0x0000000000000016 native 0 persona 0/0 write 0 audit 0 match 1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2i errno-location checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2k mutex maps 0x00000000479A0000/0x00000000479B0000 symbols 2/0 copy 1/0x00000007/1 run 0x50324B31/2/[1-9][0-9]* ret 0x0000000000000000/0x0000000000000000/0x0000000000000016/0x0000000000000016/0x0000000000000016 word 0x0000000000000001/0x0000000000000000 native 1 persona 1/1 futex 1/0/0 audit 1/1/3/202/0 last [1-9][0-9]*/1/0x0000000000000000 match 1/1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2k pthread mutex checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2n cond maps 0x00000000479A0000/0x00000000479B0000 symbols 5/0 copy 1/0x00000007/1 run 0x50324E31/5/[1-9][0-9]* ret 0x0000000000000000/0x0000000000000000/0x0000000000000016 native 2 persona 2/2 futex 2/0/0 audit 2/1/3/202/0 last [1-9][0-9]*/1/0x0000000000000000 match 1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2n pthread condition signal checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2o cond-wait maps 0x00000000479A0000/0x00000000479B0000 symbols 5/0 copy 1/0x00000007/1 run 0x50324F32/[1-9][0-9]*/[1-9][0-9]* tasks 3/3/0x50324F32/0x50324F33 words 0x00000000/0x00000001 wait 0x0000000000000000/0x0000000000000000 sched 1/1/2 native 3 persona 3/3 futex 1/2/0 audit 3 match 1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2o pthread condition blocking checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2p pthread-tls maps 0x00000000479A0000/0x00000000479B0000 symbols 3/0 copy 1/0x00000007/1 run 0x50325031/3/[1-9][0-9]* ret 0x0000000000000000/0x0000000000000000/0x0000000000000000/0x1122334455667788/0x000000000000000B/0x0000000000000016 keys 0x00000000/0x5A5A5A5A native 2 persona 2/2 audit 2/1/3/186/0 match 1/1/1 cleanup 1/1/0/0/1 positive 1' -Message "x64 UEFI Linux libc P.2p pthread TLS key checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2l pthread-create maps 0x0000000044180000/0x0000000044181000/0x0000000044182000 load 1 symbols 1/0 copy 1/0x00000007/1 run 0x50324C31/1/[1-9][0-9]* ret 0x0000000000000016/0x0000000000000026/0x0000000000000000/0x(?!0000000000000000)[0-9A-F]{16} task [1-9][0-9]*/1/[1-9][0-9]*/1/0x000000004419[0-9A-F]{4}/0x(?!0000000000000000)[0-9A-F]{16} stack 0x(?!0000000000000000)[0-9A-F]{16}/0x(?!0000000000000000)[0-9A-F]{16} shared 1/1/1 native 2 persona 2/2 mmap 1/16384 clone 1/1/1 audit 2/1/3/56/0 last [1-9][0-9]*/1/0x(?!0000000000000000)[0-9A-F]{16} match 1/1/1/1 cleanup 1/1/1/1/1/0/0/0/0/1 release 2/3/1/1/0/1/1 positive 1' -Message "x64 UEFI Linux libc P.2l pthread_create checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2m pthread-join maps 0x00000000441D0000/0x00000000441D1000/0x00000000441D2000/0x00000000441D3000 load 1 symbols 1/0 copy 1/0x00000007/1 clone 0x(?!FFFFFFFFFFFFFFFF)[0-9A-F]{16}/[1-9][0-9]*/[0-3]/[1-9][0-9]*/1/0x00000000441D4000 exit 0/45 run 0x50324D31/1/[1-9][0-9]* ret 0x0000000000000016/0x0000000000000026/0x0000000000000000 wait 1/1/11520/0/1/1 native 1 persona 1/1 audit 1/1/3/61/0 last [1-9][0-9]*/1/0x(?!0000000000000000)[0-9A-F]{16} match 1/1/1/1/1 cleanup 1/1/1/1/0/0/0/0/1 release 2/3/1/1/0/1/1 positive 1' -Message "x64 UEFI Linux libc P.2m pthread_join checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-libc-P2j abort maps 0x0000000047B30000/0x0000000047B40000/0x0000000047B50000 load 1 symbols 1/0 fn 0x0000000047B01400 copy 1/0x00000007 run 0x50324A31/1/[1-9][0-9]* ret 0x0000000000000000 native 1 persona 1/1 exit 1/1/134 last [1-9][0-9]*/134/0/0/1/0/1 detach 1 audit 1/[1-9][0-9]* match 1/1/1 cleanup 1/[5-9][0-9]*/1/1/1/1 positive 1' -Message "x64 UEFI Linux libc P.2j abort checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] linux-Q1 musl-static elf size 2816 entry 0x0000000052001270 load 1/0/2/3/0/0 mapped 3/0x0000000052000000/0x0000000053000000 ready 1/1/1 run 0x4C513158/0 probe 1/1 task [0-9]+/1/[1-9][0-9]* console 1/6 native 4 persona 4/4 calls 1/0/1/1/1/0/0/0/0 bytes 6 last [1-9][0-9]*/1/0x0000000000000000 exit 1/0/[1-9][0-9]*/0/0/0/1/0/1/1 cleanup 1/[5-9][0-9]*/1/[3-9][0-9]*/1/[1-9][0-9]*/1/1/1/1 match 1/1/1 positive 1' -Message "x64 UEFI Linux Q.1 real musl-static ELF checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] sched-block start 1 block 1/4/1 switch 1/2 wake 1/1 back 1/2 counts 1/1 deny 1/1/2 positive 1' -Message "x64 UEFI BLOCKED task scheduler checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] sched-sleep start 1 sleep 1/4/1 switch 1/2 wake 1/0/2/0 ticks 3/[3-9][0-9]*/[1-9][0-9]* counts 1/1/1/1 deny 0/2/1 last 0 positive 1' -Message "x64 UEFI scheduler sleep queue checkpoint was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] bootstrap halt' -Message "x64 UEFI kernel did not reach the normal scaffold halt."
}

if ($Architecture -eq "x86_64") {
    Assert-OutputNotContains -Lines $outputLines -Pattern '\[x64:live\]' -Message "x64 diagnostic live bridge was used instead of the persistent ring-3 shell."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] persistent ring3 shell default' -Message "x64 did not hand post-scaffold input to the persistent ring-3 shell."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64:shell\] persistent ring3 shell online' -Message "x64 persistent ring-3 shell banner was not observed."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ ls' -Message "x64 persistent shell did not accept a live ls command."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ ls apps' -Message "x64 persistent shell did not normalize and list a lowercase APPS path."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ cat readme\.txt' -Message "x64 persistent shell did not accept a lowercase README path."
    Assert-OutputContains -Lines $outputLines -Pattern 'This userspace shell is reading files through capability-checked handles\.' -Message "x64 persistent shell did not print README.TXT through the brokered filesystem path."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ stat readme\.txt' -Message "x64 persistent shell did not stat a lowercase README path."
    Assert-OutputContains -Lines $outputLines -Pattern 'type=file size=102' -Message "x64 persistent shell did not return README.TXT stat output."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ net' -Message "x64 persistent shell did not accept the Product net command."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ net curl example\.com' -Message "x64 persistent shell did not accept the scoped Product net curl command."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ pkginfo' -Message "x64 persistent shell did not accept the Product pkginfo command."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ hwval' -Message "x64 persistent shell did not accept the Product hwval command."
    Assert-OutputContains -Lines $outputLines -Pattern '^hardware validation: read-only Product mode$' -Message "x64 hwval did not report read-only Product mode."
    Assert-OutputContains -Lines $outputLines -Pattern '^installer dry-run: awaiting hardware evidence; writes disabled$' -Message "x64 hwval did not report installer dry-run as awaiting hardware evidence with writes disabled."
    Assert-OutputContains -Lines $outputLines -Pattern '^internal writes: disabled by default$' -Message "x64 hwval did not report internal writes disabled."
    Assert-OutputContains -Lines $outputLines -Pattern '^real install: not approved$' -Message "x64 hwval did not report real install as unapproved."
    Assert-OutputContains -Lines $outputLines -Pattern '^authority: read-only scoped validation; no ambient storage, installer, network, update, or install authority$' -Message "x64 hwval did not report scoped read-only authority with user-facing wording."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-hwval drs-hwval-product 1 drs-hwval-readonly 1 drs-hwval-no-internal-write 1 drs-hwval-no-format 1 drs-hwval-no-nvram 1 drs-hwval-storage-enumeration-scoped 1 drs-hwval-network-status-scoped 1 drs-hwval-package-status-scoped 1 drs-hwval-installer-dryrun-only 1 drs-hwval-msi-checklist-present 1 .* real-install-approved 0' -Message "x64 M9 hardware-validation read-only proof was not observed."
    if (($BootMedia -ne "disk") -and ($BuildProfile -eq "Product")) {
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-hardware-registry hardware-registry 1 refresh [1-9][0-9]* limit 32 inventory [1-9][0-9]* pci-enumerated [1-9][0-9]* pci-query-denial 0 .* driver-bound [1-9][0-9]* .* driver-failed 0 overflow 0 token 0x[0-9A-F]{8}' -Message "x64 M106 hardware registry proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-login drs-login-screen 1 drs-login-auth-success 1 drs-login-wrong-password-denied 0 drs-login-rate-limited 0 drs-session-lock 1 drs-session-unlock 1 drs-session-authority-scoped 1 .* user-store-nvme 1 user-store-persistent 1 .* login-display-only 1 login-input-only 1 desktop-blocked-pre-auth 1 failures 0 lockout-seconds 0 input-waits [1-9][0-9]* hardware-fallbacks [0-9]+ hardware-recovery [0-9]+ lock-unavailable [0-9]+ login-present [1-9][0-9]* login-setup-visible [0-9]+ login-lock-visible [0-9]+ login-unlock-visible [0-9]+ login-recovery-visible [0-9]+ login-wait-visible [1-9][0-9]* login-safe-path [1-9][0-9]* login-last-state [1-9][0-9]* user limitless home /HOME/LIMITLESS profile local-console' -Message "x64 UEFI M131 login/auth/session polish proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-identity drs-identity-foundation 1 drs-identity-local-active 1 drs-identity-personal-unavailable 1 drs-identity-enterprise-unavailable 1 drs-identity-settings-panel 1 drs-identity-status-readonly 1 drs-identity-mutation-denied 1 drs-vault-foundation 1 drs-vault-secret-read-denied 1 drs-vault-secret-write-denied 1 drs-vault-no-plaintext-token 1 drs-cloud-association-unavailable 1 drs-no-ambient-identity 1 drs-no-ambient-secret 1 encrypted-vault 0 secret-storage 0 account-type local account-id local:limitless display limitless association local-active network offline-capable credential bcrypt-local vault metadata-only' -Message "x64 UEFI M11 identity/vault foundation proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-idtransport drs-idtransport-product 1 drs-idtransport-provider-descriptor 1 drs-idtransport-descriptor-verified 1 drs-idtransport-descriptor-missing-sig-denied 1 drs-idtransport-descriptor-invalid-sig-denied 1 drs-idtransport-descriptor-wrong-key-denied 1 drs-idtransport-descriptor-tamper-denied 1 drs-idtransport-descriptor-rollback-denied 1 drs-idtransport-descriptor-version-denied 1 drs-idtransport-network-scoped 1 drs-idtransport-no-network-cap-denied 1 drs-idtransport-plaintext-credential-denied 1 drs-idtransport-unverified-endpoint-denied 1 drs-idtransport-token-storage-denied 1 drs-idtransport-personal-unavailable 1 drs-idtransport-enterprise-unavailable 1 drs-idtransport-cloud-association-unavailable 1 drs-idtransport-settings-panel 1 drs-idtransport-status-readonly 1 drs-idtransport-trusted-time-status 1 drs-no-ambient-idtransport-network 1 drs-no-ambient-idtransport-identity 1 drs-no-ambient-idtransport-secret 1 drs-idtransport-encrypted-channel-unavailable 1 drs-idtransport-credential-transport-unavailable 1 mode mode-b-descriptor-foundation provider personal\.fixture\.limitless provider-type personal endpoint descriptor-verified online offline-fixture encrypted unavailable credential denied token-storage denied trusted-time unavailable' -Message "x64 UEFI M12 identity transport foundation proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-account drs-account-association-product 1 drs-account-local-active 1 drs-account-personal-unavailable 1 drs-account-enterprise-unavailable 1 drs-account-cloud-unavailable 1 drs-account-security-key-unavailable 1 drs-account-settings-panel 1 drs-account-status-readonly 1 drs-account-mutation-denied 1 drs-account-unlink-denied 1 drs-account-token-storage-denied 1 drs-account-credential-transport-denied 1 drs-account-enterprise-policy-unavailable 1 drs-account-remote-no-ambient-authority 1 drs-no-ambient-account-identity 1 drs-no-ambient-account-network 1 drs-no-ambient-account-secret 1 mode mode-b-status-only local active personal planned-unavailable enterprise planned-unavailable cloud planned-unavailable security-key planned-unavailable enterprise-policy unavailable encrypted unavailable token-storage denied trusted-time unavailable remote-login unavailable local-user local:limitless provider personal\.fixture\.limitless' -Message "x64 UEFI M13 account association Mode B proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-cloud drs-cloud-broker-product 1 drs-cloud-provider-descriptor 1 drs-cloud-provider-verified 1 drs-cloud-provider-missing-sig-denied 1 drs-cloud-provider-invalid-sig-denied 1 drs-cloud-provider-wrong-key-denied 1 drs-cloud-provider-tamper-denied 1 drs-cloud-provider-rollback-denied 1 drs-cloud-provider-version-denied 1 drs-cloud-provider-malformed-denied 1 drs-cloud-association-unavailable 1 drs-cloud-account-unavailable 1 drs-cloud-token-storage-denied 1 drs-cloud-encrypted-transport-unavailable 1 drs-cloud-upload-denied 1 drs-cloud-download-denied 1 drs-cloud-sync-denied 1 drs-cloud-auto-upload-unavailable 1 drs-cloud-auto-download-unavailable 1 drs-cloud-ai-access-unavailable 1 drs-cloud-app-direct-denied 1 drs-cloud-settings-panel 1 drs-cloud-settings-readonly 1 drs-cloud-fileman-status 1 drs-cloud-fileman-mutation-denied 1 drs-no-ambient-cloud 1 drs-no-ambient-cloud-fs 1 drs-no-ambient-cloud-network 1 drs-no-ambient-cloud-identity 1 drs-no-ambient-cloud-secret 1 mode foundation-active storage-mode unavailable-policy-only provider cloud\.fixture\.limitless descriptor signed-local-fixture-verified account unavailable-planned association unavailable-planned token-storage denied-vault-mode-b encrypted unavailable sync unavailable upload denied download denied offline-cache planned-unavailable ai unavailable app-direct denied' -Message "x64 UEFI M14 cloud storage broker foundation proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ lock' -Message "x64 persistent shell did not accept the Product lock command."
        Assert-OutputContains -Lines $outputLines -Pattern '^(session unlocked|lock unavailable on this boot path)$' -Message "x64 persistent shell did not report a truthful Product lock command result."
    }
    if ($BootMedia -eq "disk") {
        Assert-OutputContains -Lines $outputLines -Pattern '^no network$' -Message "x64 persistent shell did not report clean network unavailability on disk/BIOS media."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pkg unavailable bios-checksum-only 1' -Message "x64 BIOS package-signing surface did not report checksum-only fallback."
        Assert-OutputContains -Lines $outputLines -Pattern '^package system: BIOS checksum-only fallback$' -Message "x64 BIOS pkginfo did not report checksum-only fallback."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud storage broker: unavailable on BIOS fallback$' -Message "x64 BIOS pkginfo did not report cloud storage as unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai policy broker: unavailable on BIOS fallback$' -Message "x64 BIOS pkginfo did not report AI policy as unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai actions: unavailable$' -Message "x64 BIOS pkginfo did not report AI actions unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai assistant: unavailable( on BIOS fallback)?$' -Message "x64 BIOS pkginfo did not report AI assistant unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pkg-status unavailable bios-checksum-only 1 .*drs-pkg-status-no-auto-install-visible 1 .*drs-pkg-status-public-fetch-unavailable 1 .*drs-pkg-status-trusted-time-unavailable 1 .*drs-pkg-install-action-unavailable 1 .*drs-pkg-update-apply-unavailable 1' -Message "x64 BIOS package trust UX fallback proof was not observed."
    }
    else {
        Assert-OutputContains -Lines $outputLines -Pattern '^network: online$' -Message "x64 persistent shell did not report an active Product network lease."
        Assert-OutputContains -Lines $outputLines -Pattern '^gateway: 10\.0\.2\.2$' -Message "x64 persistent shell did not report the DHCP gateway."
        Assert-OutputContains -Lines $outputLines -Pattern '^dns: 10\.0\.2\.[0-9]+$' -Message "x64 persistent shell did not report the DHCP DNS server."
        Assert-OutputContains -Lines $outputLines -Pattern '^socket api: brokered tcp-client foundation$' -Message "x64 persistent shell did not report the brokered socket API foundation."
        Assert-OutputContains -Lines $outputLines -Pattern '^socket http status: 200$' -Message "x64 persistent shell did not report the brokered socket HTTP status."
        Assert-OutputContains -Lines $outputLines -Pattern '^socket denied: raw packet, listen, send without broker data-plane authority$' -Message "x64 persistent shell did not report socket denial boundaries."
        Assert-OutputContains -Lines $outputLines -Pattern '^authority: brokered$' -Message "x64 persistent shell did not label network authority as brokered."
        Assert-OutputContains -Lines $outputLines -Pattern '^HTTP/1\.[01] 200' -Message "x64 net curl did not print the brokered HTTP response status line."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-socket drs-socket-api 1 drs-socket-service 1 drs-socket-cap-required 1 drs-socket-cap-minted 1 drs-socket-no-cap-denied 1 drs-socket-wrong-owner-denied 1 drs-socket-raw-denied 1 drs-socket-listen-denied 1 drs-socket-send-denied 1 drs-socket-connect-attempt 1 drs-socket-connect-granted 1 drs-socket-connect-unavailable 0 drs-socket-recv-status 1 drs-socket-close 1 socket-count 0 http-status 200 response-bytes [1-9][0-9]* fs-authority 0 storage-authority 0 ambient-authority 0' -Message "x64 M19 brokered socket API foundation proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] net-N1 dns 1 connect 1 http-get 1 response-bytes [1-9][0-9]* truncated [0-1] close 1 cap-minted 1 cap-destroyed 1 nondelegable-denied 1 socket-count 0 fs-authority 0 storage-authority 0 ambient-authority 0 url-denied 0 error 0' -Message "x64 net-N1 scoped TCP client curl proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '^hello from user app$' -Message "x64 M21 native user app did not print through brokered console authority."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-app-m21 0x(?!00000000|FFFFFFFF)[0-9A-F]{8} drs-app-m21-state 4 drs-app-m21-flags 0x[0-9A-F]{8} drs-app-m21-owner 0x00000201 drs-app-m21-name-token 0x(?!00000000)[0-9A-F]{8} drs-app-m21-executable-id 20 drs-app-m21-authority-mask 0x00000060 drs-app-m21-capability-mask 0x00000003 drs-app-m21-payload-slot 13 drs-app-m21-entry-result 0x4E484530 drs-app-m21-success-result 0x4E484531 drs-app-m21-binary-path-verified 1 drs-app-m21-descriptor-read 1 drs-app-m21-descriptor-parsed 1 drs-app-m21-descriptor-bytes [1-9][0-9]* drs-app-m21-binary-read 1 drs-app-m21-checksum-verified 1 drs-app-m21-binary-bytes [1-9][0-9]* drs-app-m21-checksum 0x[0-9A-F]{8} drs-app-m21-expected-checksum 0x[0-9A-F]{8} drs-app-m21-mapped 1 drs-app-m21-mapped-bytes 4096 drs-app-m21-entry-rip 0x43000010 drs-app-m21-entry-rsp 0x40020000 drs-app-m21-entry-selectors 0x002B0033 drs-app-m21-entry-rflags 0x00000002 drs-app-m21-launched 1 drs-app-m21-hello 1 drs-app-m21-syscall-bridge 1 drs-app-m21-network-cap-requested 1 drs-app-m21-network-cap-granted 1 drs-app-m21-socket-open 1 drs-app-m21-recv-status 1 drs-app-m21-send-denied 1 drs-app-m21-close 1 drs-app-m21-fs-denied 1 drs-app-m21-storage-denied 1 drs-app-m21-exit-result 0x4E484531 drs-app-m21-exit-aux [1-9][0-9]* fs-authority 0 storage-authority 0 ambient-authority 0' -Message "x64 M21 native app SDK loader/capability/socket verification was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '^package system: enabled on UEFI Product$' -Message "x64 UEFI pkginfo did not report the signed-package Product path."
        Assert-OutputContains -Lines $outputLines -Pattern '^uefi package mode: Ed25519 verified$' -Message "x64 UEFI pkginfo did not report Ed25519 verification."
        Assert-OutputContains -Lines $outputLines -Pattern '^trusted public key fingerprint: [0-9A-F]{64}$' -Message "x64 UEFI pkginfo did not expose the signer fingerprint."
        Assert-OutputContains -Lines $outputLines -Pattern '^capability requests: visible; policy enforced$' -Message "x64 UEFI pkginfo did not expose package capability policy status."
        Assert-OutputContains -Lines $outputLines -Pattern '^auto-install: unavailable$' -Message "x64 UEFI pkginfo did not report auto-install as unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^public update fetch: unavailable$' -Message "x64 UEFI pkginfo did not report public update fetch as unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^trusted-time expiry: unavailable$' -Message "x64 UEFI pkginfo did not report trusted-time expiry as unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^identity descriptor: signed local provider verified$' -Message "x64 UEFI pkginfo did not report identity descriptor verification."
        Assert-OutputContains -Lines $outputLines -Pattern '^account association mode: status only$' -Message "x64 UEFI pkginfo did not report M13 account association status-only mode."
        Assert-OutputContains -Lines $outputLines -Pattern '^local association: active and offline-capable$' -Message "x64 UEFI pkginfo did not report local association active."
        Assert-OutputContains -Lines $outputLines -Pattern '^personal association: unavailable$' -Message "x64 UEFI pkginfo did not report personal association unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^enterprise association: unavailable$' -Message "x64 UEFI pkginfo did not report enterprise association unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud association: unavailable$' -Message "x64 UEFI pkginfo did not report cloud association unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^security key login: unavailable$' -Message "x64 UEFI pkginfo did not report security key login unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^remote account authority: none$' -Message "x64 UEFI pkginfo did not report no remote account authority."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud storage broker: foundation active$' -Message "x64 UEFI pkginfo did not report M14 cloud storage broker active."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud provider descriptor: signed local provider verified$' -Message "x64 UEFI pkginfo did not report cloud provider descriptor verification."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud storage mode: policy only; sync unavailable$' -Message "x64 UEFI pkginfo did not report cloud storage mode as policy-only."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud token storage: denied$' -Message "x64 UEFI pkginfo did not report cloud token storage denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud encrypted transport: unavailable$' -Message "x64 UEFI pkginfo did not report cloud encrypted transport unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud sync: unavailable$' -Message "x64 UEFI pkginfo did not report cloud sync unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud transfers: denied$' -Message "x64 UEFI pkginfo did not report cloud transfer denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud automatic transfers: unavailable$' -Message "x64 UEFI pkginfo did not report cloud automatic transfers unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud AI access: unavailable$' -Message "x64 UEFI pkginfo did not report cloud AI access unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^cloud app direct authority: denied$' -Message "x64 UEFI pkginfo did not report app direct cloud authority denied."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai policy broker: foundation active$' -Message "x64 UEFI pkginfo did not report M16 AI policy broker active."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai principal: request-only no default capabilities$' -Message "x64 UEFI pkginfo did not report the request-only AI principal."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai action request: modeled$' -Message "x64 UEFI pkginfo did not report AI action requests as modeled."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai consent: required no auto-approve$' -Message "x64 UEFI pkginfo did not report consent as required."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai actions: (unavailable|consent-scoped templates only)$' -Message "x64 UEFI pkginfo did not report AI action availability truthfully."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai action broker: deterministic templates only$' -Message "x64 UEFI pkginfo did not report M18 AI action broker status."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai action templates: assistant-note-write installer-dryrun open-settings-panel package-trust-status$' -Message "x64 UEFI pkginfo did not report M18 allowed action templates."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai action audit: request consent grant result revocation recorded$' -Message "x64 UEFI pkginfo did not report M18 action audit status."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai audit: immutable queryable settings-visible$' -Message "x64 UEFI pkginfo did not report the AI audit log surface."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai filesystem access: denied$' -Message "x64 UEFI pkginfo did not report AI filesystem denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai network access: denied$' -Message "x64 UEFI pkginfo did not report AI network denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai settings access: denied$' -Message "x64 UEFI pkginfo did not report AI settings denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai package access: denied$' -Message "x64 UEFI pkginfo did not report AI package denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai secret access: denied$' -Message "x64 UEFI pkginfo did not report AI secret denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai cloud access: denied$' -Message "x64 UEFI pkginfo did not report AI cloud denial."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai assistant: (unavailable|host active; inference unavailable)$' -Message "x64 UEFI pkginfo did not report AI assistant status truthfully."
        Assert-OutputContains -Lines $outputLines -Pattern '^ai automation: unavailable$' -Message "x64 UEFI pkginfo did not report AI automation unavailable."
        Assert-OutputContains -Lines $outputLines -Pattern '^authority: no ambient install, update, network, cloud, file, identity, secret, or AI access$' -Message "x64 UEFI pkginfo did not report no ambient install, update, network, cloud, file, identity, secret, or AI authority."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pkg drs-pkg-signed 1 drs-pkg-verified 1 drs-pkg-invalid-denied 1 drs-pkg-missing-sig-denied 1 drs-pkg-wrong-key-denied 1 drs-pkg-manifest-tamper-denied 1 drs-pkg-payload-tamper-denied 1 drs-pkg-checksum-mismatch-denied 1 drs-pkg-unsupported-version-denied 1 drs-pkg-duplicate-denied 1 drs-pkg-downgrade-denied 1 drs-pkg-wrong-owner-denied 1 drs-pkg-stale-token-denied 1 drs-pkg-cap-policy-denied 1 drs-pkg-malformed-denied 1 drs-pkg-oversized-denied 1 drs-pkg-install-no-cap-denied 1 drs-pkg-install-scoped 1 drs-pkg-update-check 1 drs-pkg-update-index-verified 1 drs-pkg-update-index-unsigned-denied 1 drs-pkg-update-index-tamper-denied 1 drs-pkg-update-index-wrong-key-denied 1 drs-pkg-update-index-rollback-denied 1 drs-pkg-update-index-replay-handled 1 drs-pkg-update-no-network-cap-denied 1 drs-pkg-update-apply-no-install-cap-denied 1 drs-pkg-update-no-ambient 1 drs-pkg-update-no-auto-install 1' -Message "x64 UEFI M7.1 signed package/update negative fixture proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-pkg-status drs-pkg-settings-panel 1 drs-pkg-settings-readonly 1 drs-pkg-status-visible 1 drs-pkg-status-signer-visible 1 drs-pkg-status-capabilities-visible 1 drs-pkg-status-update-index-visible 1 drs-pkg-status-no-auto-install-visible 1 drs-pkg-status-public-fetch-unavailable 1 drs-pkg-status-trusted-time-unavailable 1 drs-pkg-status-no-ambient-install 1 drs-pkg-status-no-ambient-update 1 drs-pkg-status-no-ambient-network 1 drs-pkg-settings-write-denied 1 drs-pkg-install-action-unavailable 1 drs-pkg-update-apply-unavailable 1 signer-key 0x[0-9A-F]+ signed-packages [1-9][0-9]* settings-panels [1-9][0-9]*' -Message "x64 UEFI M8 package trust UX/status proof was not observed."
        Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] drs-ai drs-ai-principal 1 drs-ai-request-created 1 drs-ai-consent-required 1 drs-ai-denied-no-consent 1 drs-ai-scope-validated 1 drs-ai-invalid-scope-denied 1 drs-ai-audit-recorded 1 drs-ai-settings-panel 1 drs-ai-settings-readonly 1 drs-ai-no-ambient-authority 1 drs-ai-no-filesystem-access 1 drs-ai-no-network-access 1 drs-ai-no-settings-access 1 drs-ai-no-package-access 1 drs-ai-no-secret-access 1 drs-ai-no-cloud-access 1 .* default-caps 0 actions-executed 0 audit-records [1-9][0-9]* mode request-deny-audit-only principal request-only-no-default-capabilities action read-file resource /README\.TXT capability fs-read scope file decision deny result denied-no-consent assistant unavailable automation unavailable cloud-ai unavailable' -Message "x64 UEFI M16 AI policy request/deny/audit proof was not observed."
    }
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ write w\.txt ok' -Message "x64 persistent shell did not accept a live write command with explicit text."
    Assert-OutputContains -Lines $outputLines -Pattern '\[x64\] \$ cat w\.txt' -Message "x64 persistent shell did not accept a live cat command for the written file."
    Assert-OutputContains -Lines $outputLines -Pattern '^ok$' -Message "x64 persistent shell did not print the written file contents."
    Assert-X64M6ServiceSessionSurface -Lines $outputLines
    Assert-X64M1RuntimeSurface -Lines $outputLines -LoginExpected:(($BootMedia -ne "disk") -and ($BuildProfile -eq "Product")) -BootMedia $BootMedia
}

$outputLines
