param(
    [ValidateSet("x86", "x86_64")]
    [string]$Architecture = "x86",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$distDir = Join-Path $root "dist"
$generatedDir = Join-Path $buildDir "generated"
$bootSource = Join-Path $root "boot\\boot.asm"
$kernelEntry = Join-Path $root "kernel\\arch\\x86\\entry.asm"
$interruptStubSource = Join-Path $root "kernel\\arch\\x86\\interrupts.asm"
$usermodeAsmSource = Join-Path $root "kernel\\arch\\x86\\usermode.asm"
$kernelLinker = Join-Path $root "kernel\\linker.ld"
$includeDir = Join-Path $root "kernel\\include"
$packageStoreSpec = Join-Path $root "packages\\bootstrap-store.json"
$packageStoreGenerator = Join-Path $root "tools\\generate-package-store.ps1"
$runtimeImageGenerator = Join-Path $root "tools\\generate-x64-runtime-image.ps1"
$isoGenerator = Join-Path $root "tools\\generate-iso.ps1"
$uefiFatGenerator = Join-Path $root "tools\\generate-uefi-fat-image.ps1"
$nvmeImageGenerator = Join-Path $root "tools\\generate-nvme-image.ps1"
$m1ProductionGate = Join-Path $root "tools\\assert-m1-production-slice.ps1"
$cryptBlowfishDir = Join-Path $root "third_party\\crypt_blowfish"
$packageStoreHeader = Join-Path $generatedDir "package_store_generated.h"
$packageStoreSignatureHeader = Join-Path $generatedDir "package_store_signatures_generated.h"
$archBuildHeader = Join-Path $generatedDir "arch_build.h"
$runtimeImageAsm = Join-Path $root "kernel\\arch\\x86_64\\runtime_image_user.asm"
$runtimeImageBin = Join-Path $buildDir "runtime-image-x86_64.bin"
$runtimeImageHeader = Join-Path $generatedDir "runtime_image_x64_generated.h"
$diskLsImageAsm = Join-Path $root "kernel\\arch\\x86_64\\ls_bin_user.asm"
$diskLsImageBin = Join-Path $buildDir "ls-x86_64.bin"
$diskUtilityImageAsm = Join-Path $root "kernel\\arch\\x86_64\\utility_bin_user.asm"
$nvmeGptImagePath = Join-Path $distDir "limitlessos-x86_64-nvme-gpt.img"
$diskFlatBinarySpecs = @(
    @{ Name = "CAT"; Slot = [uint32]3; Define = "UTILITY_CAT"; Bin = (Join-Path $buildDir "cat-x86_64.bin") },
    @{ Name = "STAT"; Slot = [uint32]4; Define = "UTILITY_STAT"; Bin = (Join-Path $buildDir "stat-x86_64.bin") },
    @{ Name = "MKDIR"; Slot = [uint32]5; Define = "UTILITY_MKDIR"; Bin = (Join-Path $buildDir "mkdir-x86_64.bin") },
    @{ Name = "WRITE"; Slot = [uint32]6; Define = "UTILITY_WRITE"; Bin = (Join-Path $buildDir "write-x86_64.bin") },
    @{ Name = "TOUCH"; Slot = [uint32]7; Define = "UTILITY_TOUCH"; Bin = (Join-Path $buildDir "touch-x86_64.bin") },
    @{ Name = "APPEND"; Slot = [uint32]8; Define = "UTILITY_APPEND"; Bin = (Join-Path $buildDir "append-x86_64.bin") },
    @{ Name = "COPY"; Slot = [uint32]9; Define = "UTILITY_COPY"; Bin = (Join-Path $buildDir "copy-x86_64.bin") },
    @{ Name = "DELETE"; Slot = [uint32]10; Define = "UTILITY_DELETE"; Bin = (Join-Path $buildDir "delete-x86_64.bin") },
    @{ Name = "RENAME"; Slot = [uint32]11; Define = "UTILITY_RENAME"; Bin = (Join-Path $buildDir "rename-x86_64.bin") },
    @{ Name = "MOVE"; Slot = [uint32]12; Define = "UTILITY_MOVE"; Bin = (Join-Path $buildDir "move-x86_64.bin") }
)

function Write-ArchBuildHeader
{
    param(
        [string]$OutputPath,
        [string]$TargetArchitecture,
        [string]$TargetBuildProfile
    )

    $bits = if ($TargetArchitecture -eq "x86") { 32 } else { 64 }
    $isX86 = if ($TargetArchitecture -eq "x86") { 1 } else { 0 }
    $isX64 = if ($TargetArchitecture -eq "x86_64") { 1 } else { 0 }
    $bootstrapKind = if ($TargetArchitecture -eq "x86") { "bios32-boot-image" } else { "bios64-long-mode-image" }
    $isProduct = if ($TargetBuildProfile -eq "Product") { 1 } else { 0 }
    $isExperimental = if ($TargetBuildProfile -eq "Experimental") { 1 } else { 0 }
    $experimentalRuntime = $isExperimental

    $header = @"
#ifndef LIMITLESS_ARCH_BUILD_H
#define LIMITLESS_ARCH_BUILD_H

#define LIMITLESS_ARCH_NAME "$TargetArchitecture"
#define LIMITLESS_ARCH_BITS $bits
#define LIMITLESS_ARCH_X86 $isX86
#define LIMITLESS_ARCH_X86_64 $isX64
#define LIMITLESS_ARCH_BOOTSTRAP_KIND "$bootstrapKind"
#define LIMITLESS_BUILD_PROFILE_NAME "$TargetBuildProfile"
#define LIMITLESS_BUILD_PROFILE_PRODUCT $isProduct
#define LIMITLESS_BUILD_PROFILE_EXPERIMENTAL $isExperimental
#define LIMITLESS_EXPERIMENTAL_RUNTIME_ENABLED $experimentalRuntime

#endif
"@

    Set-Content -Path $OutputPath -Value $header -Encoding Ascii
}

function Get-Fnv1aDataChecksum
{
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    [uint32]$hash = 2166136261

    for ($i = 0; $i -lt $Bytes.Length; $i++) {
        [uint32]$value = $Bytes[$i]
        $hash = [uint32](($hash -bxor $value) -band 0xFFFFFFFF)
        $hash = [uint32](([uint64]$hash * [uint64]16777619) % [uint64]4294967296)
    }

    return $hash
}

function Get-BinutilsSectionSizes
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $lines = @(size -A $Path)
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to inspect section sizes for $Path."
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
        Path = $Path
        Name = (Split-Path -Leaf $Path)
        Text = $text
        Rodata = $rodata
        Data = $data
        Bss = $bss
        Total = ($text + $rodata + $data + $bss)
    }
}

function ConvertTo-RepoRelativePath
{
    param([Parameter(Mandatory = $true)][string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $rootPath = [System.IO.Path]::GetFullPath($root).TrimEnd('\')
    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length + 1)
    }

    return $fullPath
}

function Build-X86
{
    $bootSource = Join-Path $root "boot\\boot.asm"
    $kernelEntry = Join-Path $root "kernel\\arch\\x86\\entry.asm"
    $interruptStubSource = Join-Path $root "kernel\\arch\\x86\\interrupts.asm"
    $usermodeAsmSource = Join-Path $root "kernel\\arch\\x86\\usermode.asm"
    $kernelLinker = Join-Path $root "kernel\\linker.ld"
    $includeDir = Join-Path $root "kernel\\include"

    $bootBin = Join-Path $buildDir "boot.bin"
    $entryObj = Join-Path $buildDir "entry.o"
    $interruptsAsmObj = Join-Path $buildDir "interrupts-asm.o"
    $usermodeAsmObj = Join-Path $buildDir "usermode-asm.o"
    $kernelPe = Join-Path $buildDir "kernel.pe"
    $kernelBin = Join-Path $buildDir "kernel.bin"
    $imagePath = Join-Path $distDir "limitlessos.img"
    $isoPath = Join-Path $distDir "limitlessos.iso"
    $kernelOut = Join-Path $distDir "limitlessos.kernel.bin"

    Write-Host "Assembling boot sector"
    & nasm -f bin $bootSource -o $bootBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble boot sector."
    }

    Write-Host "Assembling kernel entry"
    & nasm -f win32 $kernelEntry -o $entryObj
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble kernel entry."
    }

    Write-Host "Assembling interrupt stubs"
    & nasm -f win32 $interruptStubSource -o $interruptsAsmObj
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble interrupt stubs."
    }

    Write-Host "Assembling usermode helpers"
    & nasm -f win32 $usermodeAsmSource -o $usermodeAsmObj
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble usermode helpers."
    }

    $cFlags = @(
        "-m32",
        "-march=i386",
        "-mno-mmx",
        "-mno-sse",
        "-mno-sse2",
        "-mno-80387",
        "-O2",
        "-ffreestanding",
        "-fno-pic",
        "-fno-pie",
        "-fno-ident",
        "-fmerge-all-constants",
        "-fno-jump-tables",
        "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables",
        "-fno-builtin",
        "-fno-tree-vectorize",
        "-nostdlib",
        "-Wall",
        "-Wextra",
        "-I$includeDir",
        "-I$generatedDir"
    )

    Write-Host "Compiling kernel sources"
    $objectFiles = @($entryObj, $interruptsAsmObj, $usermodeAsmObj)
    $cSources = Get-ChildItem -Path @(
        (Join-Path $root "kernel\\core\\*.c"),
        (Join-Path $root "kernel\\arch\\x86\\*.c")
    )

    foreach ($source in $cSources) {
        $objectPath = Join-Path $buildDir ($source.BaseName + ".o")
        & gcc @cFlags -c $source.FullName -o $objectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compile $($source.Name)."
        }

        $objectFiles += $objectPath
    }

    Write-Host "Linking kernel"
    & ld -m i386pe --image-base 0 --disable-reloc-section -T $kernelLinker -nostdlib -o $kernelPe @objectFiles
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to link kernel."
    }

    Write-Host "Converting kernel to raw binary"
    & objcopy -O binary $kernelPe $kernelBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert kernel to raw binary."
    }

    [byte[]]$bootBytes = [System.IO.File]::ReadAllBytes($bootBin)
    [byte[]]$kernelBytes = [System.IO.File]::ReadAllBytes($kernelBin)

    if ($bootBytes.Length -ne 512) {
        throw "Boot sector must be exactly 512 bytes. Actual length: $($bootBytes.Length)"
    }

    $sectorCount = [int][Math]::Ceiling($kernelBytes.Length / 512.0)
    if ($sectorCount -le 0) {
        throw "Kernel image is empty."
    }

    $marker = [System.Text.Encoding]::ASCII.GetBytes("KSCT")
    $markerIndex = -1

    for ($i = 0; $i -le ($bootBytes.Length - $marker.Length - 2); $i++) {
        $found = $true
        for ($j = 0; $j -lt $marker.Length; $j++) {
            if ($bootBytes[$i + $j] -ne $marker[$j]) {
                $found = $false
                break
            }
        }

        if ($found) {
            $markerIndex = $i
            break
        }
    }

    if ($markerIndex -lt 0) {
        throw "Could not locate kernel sector marker in boot sector."
    }

    $countOffset = $markerIndex + $marker.Length
    $bootBytes[$countOffset] = [byte]($sectorCount -band 0xFF)
    $bootBytes[$countOffset + 1] = [byte](($sectorCount -shr 8) -band 0xFF)

    [System.IO.File]::WriteAllBytes($bootBin, $bootBytes)

    $imageSize = 1474560
    $imageStream = [System.IO.File]::Open($imagePath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $imageStream.SetLength($imageSize)
        $imageStream.Write($bootBytes, 0, $bootBytes.Length)
        $imageStream.Seek(512, [System.IO.SeekOrigin]::Begin) | Out-Null
        $imageStream.Write($kernelBytes, 0, $kernelBytes.Length)
    }
    finally {
        $imageStream.Dispose()
    }

    [System.IO.File]::WriteAllBytes($kernelOut, $kernelBytes)

    Write-Host "Packaging bootable ISO"
    & $isoGenerator -InputImagePath $imagePath -OutputIsoPath $isoPath -Architecture x86
    if (-not $?) {
        throw "Failed to generate x86 ISO image."
    }

    Write-Host ""
    Write-Host "Build complete"
    Write-Host "  architecture  : x86"
    Write-Host "  kernel size   : $($kernelBytes.Length) bytes"
    Write-Host "  kernel sectors: $sectorCount"
    Write-Host "  image         : $imagePath"
    Write-Host "  iso           : $isoPath"
}

function Build-X64Scaffold
{
    $bootSource = Join-Path $root "boot\\boot64.asm"
    $kernelEntry = Join-Path $root "kernel\\arch\\x86_64\\entry.asm"
    $interruptStubSource = Join-Path $root "kernel\\arch\\x86_64\\interrupts.asm"
    $kernelLinker = Join-Path $root "kernel\\linker-x86_64.ld"
    $includeDir = Join-Path $root "kernel\\include"
    $ed25519Dir = Join-Path $root "third_party\\ed25519_ref10"

    $bootBin = Join-Path $buildDir "boot64.bin"
    $entryObj = Join-Path $buildDir "entry-x86_64.o"
    $interruptsAsmObj = Join-Path $buildDir "interrupts-asm-x86_64.o"
    $kernelPe = Join-Path $buildDir "kernel-x86_64-bios.pe"
    $kernelBin = Join-Path $buildDir "kernel-x86_64-bios.bin"
    $uefiKernelPe = Join-Path $buildDir "kernel-x86_64-uefi.pe"
    $uefiKernelBin = Join-Path $buildDir "kernel-x86_64-uefi.bin"
    $uefiObject = Join-Path $buildDir "uefi-app-x86_64.o"
    $uefiPe = Join-Path $buildDir "limitlessos-x86_64.efi"
    $imagePath = Join-Path $distDir "limitlessos-x86_64.img"
    $isoPath = Join-Path $distDir "limitlessos-x86_64.iso"
    $kernelOut = Join-Path $distDir "KERNEL64-BIOS.BIN"
    $uefiKernelOut = Join-Path $distDir "KERNEL64.BIN"
    $uefiArtifact = Join-Path $distDir "limitlessos-x86_64.efi"
    $uefiImage = Join-Path $distDir "limitlessos-x86_64-uefi.img"
    $uefiStageDir = Join-Path $distDir "limitlessos-x86_64-uefi"
    $uefiBootDir = Join-Path $uefiStageDir "EFI\\BOOT"
    $uefiBootPath = Join-Path $uefiBootDir "BOOTX64.EFI"
    $uefiReadmePath = Join-Path $uefiStageDir "README.TXT"
    $uefiManifestPath = Join-Path $uefiStageDir "BOOTMAN.TXT"
    $uefiKernelPath = Join-Path $uefiStageDir "KERNEL64.BIN"
    $uefiAppsDir = Join-Path $uefiStageDir "APPS"
    $uefiLsAppPath = Join-Path $uefiAppsDir "LS.APP"
    $uefiLsBinPath = Join-Path $uefiAppsDir "LS.BIN"
    $uefiCatAppPath = Join-Path $uefiAppsDir "CAT.APP"
    $uefiStatAppPath = Join-Path $uefiAppsDir "STAT.APP"
    $artifactPe = Join-Path $distDir "limitlessos-x86_64-bios.pe"
    $uefiArtifactPe = Join-Path $distDir "limitlessos-x86_64-uefi-kernel.pe"
    $artifactBin = Join-Path $distDir "limitlessos-x86_64.scaffold.bin"
    $uefiArtifactBin = Join-Path $distDir "limitlessos-x86_64.uefi-kernel.bin"
    $reportPath = Join-Path $distDir "limitlessos-x86_64.scaffold.txt"
    $sizeReportPath = Join-Path $distDir "limitlessos-x86_64.size.txt"

    Write-Host "Assembling x86_64 boot sector"
    & nasm -f bin $bootSource -o $bootBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble x86_64 boot sector."
    }

    Write-Host "Assembling x86_64 kernel entry"
    & nasm -f win64 $kernelEntry -o $entryObj
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble x86_64 kernel entry."
    }

    Write-Host "Assembling x86_64 interrupt stubs"
    & nasm -f win64 $interruptStubSource -o $interruptsAsmObj
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble x86_64 interrupt stubs."
    }

    $cFlags = @(
        "-m64",
        "-march=x86-64",
        "-mno-red-zone",
        "-mno-stack-arg-probe",
        "-Oz",
        "-ffreestanding",
        "-fno-pic",
        "-fno-pie",
        "-fno-ident",
        "-fno-stack-protector",
        "-fno-asynchronous-unwind-tables",
        "-fno-builtin",
        "-nostdlib",
        "-Wall",
        "-Wextra",
        "-I$includeDir",
        "-I$generatedDir"
    )
    $uefiOnlyCFlags = @($cFlags + "-I$ed25519Dir")

    Write-Host "Compiling x86_64 BIOS Product kernel sources"
    $commonSources = @()
    $commonSources += Get-Item (Join-Path $root "kernel\\core\\ramfs.c")
    $commonSources += Get-ChildItem -Path (Join-Path $root "kernel\\arch\\x86_64\\*.c") |
        Where-Object { $_.Name -ne "uefi_app.c" }
    $commonSources = @($commonSources | Where-Object { $_.Name -ne "network_disabled.c" })
    $commonSources = @($commonSources | Where-Object { $_.Name -ne "package_signing.c" })
    $commonSources = @($commonSources | Where-Object { $_.Name -ne "identity.c" })
    $commonSources = @($commonSources | Where-Object { $_.Name -ne "identity_transport.c" })
    $commonSources = @($commonSources | Where-Object { $_.Name -ne "account_association.c" })
    $biosSources = @($commonSources | Where-Object {
            ($_.Name -ne "virtio_net.c") -and ($_.Name -ne "e1000e.c")
        })
    $biosSources += Get-Item (Join-Path $root "kernel\\arch\\x86_64\\network_disabled.c")
    $uefiSources = @($commonSources)
    $uefiSources += Get-Item (Join-Path $root "kernel\\arch\\x86_64\\identity.c")
    $uefiSources += Get-Item (Join-Path $root "kernel\\arch\\x86_64\\identity_transport.c")
    $uefiSources += Get-Item (Join-Path $root "kernel\\arch\\x86_64\\account_association.c")
    $uefiSources += Get-Item (Join-Path $root "kernel\\arch\\x86_64\\package_signing.c")
    $uefiSources += @(
        "fe.c",
        "ge.c",
        "open.c",
        "sc_reduce.c",
        "sha512.c",
        "verify.c"
    ) | ForEach-Object { Get-Item (Join-Path $ed25519Dir $_) }
    $uefiSources += Get-Item (Join-Path $cryptBlowfishDir "crypt_blowfish.c")
    $biosCFlags = @($cFlags + "-DLIMITLESS_X64_BIOS_KERNEL=1")
    $uefiCFlags = @($uefiOnlyCFlags + "-DLIMITLESS_X64_UEFI_KERNEL=1")
    $objectFiles = @($entryObj, $interruptsAsmObj)

    foreach ($source in $biosSources) {
        $objectPath = Join-Path $buildDir ($source.BaseName + "-x86_64-bios.o")
        & gcc @biosCFlags -c $source.FullName -o $objectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compile BIOS $($source.Name)."
        }

        $objectFiles += $objectPath
    }

    Write-Host "Compiling x86_64 UEFI Product kernel sources"
    $uefiObjectFiles = @($entryObj, $interruptsAsmObj)
    foreach ($source in $uefiSources) {
        $objectPath = Join-Path $buildDir ($source.BaseName + "-x86_64-uefi.o")
        & gcc @uefiCFlags -c $source.FullName -o $objectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compile UEFI $($source.Name)."
        }

        $uefiObjectFiles += $objectPath
    }

    $uefiSource = Join-Path $root "kernel\\arch\\x86_64\\uefi_app.c"
    $uefiSupportObjects = @()
    Write-Host "Compiling x86_64 UEFI app"
    & gcc @uefiCFlags -c $uefiSource -o $uefiObject
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to compile x86_64 UEFI app."
    }

    $uefiSupportObjects = @($uefiObjectFiles | Where-Object { $_ -like "*services-x86_64-uefi.o" })

    Write-Host "Linking x86_64 scaffold"
    & ld -m i386pep --image-base 0 --disable-reloc-section -T $kernelLinker -nostdlib -o $kernelPe @objectFiles
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to link x86_64 scaffold."
    }

    Write-Host "Converting x86_64 scaffold to raw binary"
    & objcopy -O binary $kernelPe $kernelBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert x86_64 scaffold."
    }

    Write-Host "Linking x86_64 UEFI Product kernel"
    & ld -m i386pep --image-base 0 --disable-reloc-section -T $kernelLinker -nostdlib -o $uefiKernelPe @uefiObjectFiles
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to link x86_64 UEFI Product kernel."
    }

    Write-Host "Converting x86_64 UEFI Product kernel to raw binary"
    & objcopy -O binary $uefiKernelPe $uefiKernelBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert x86_64 UEFI Product kernel."
    }

    Write-Host "Linking x86_64 UEFI app"
    & ld -m i386pep --subsystem 10 --image-base 0 --entry efi_main -nostdlib -o $uefiPe $uefiObject @uefiSupportObjects
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to link x86_64 UEFI app."
    }

    [byte[]]$bootBytes = [System.IO.File]::ReadAllBytes($bootBin)
    [byte[]]$kernelBytes = [System.IO.File]::ReadAllBytes($kernelBin)
    [byte[]]$uefiKernelBytes = [System.IO.File]::ReadAllBytes($uefiKernelBin)
    $loaderSectorLimit = 1024
    $loaderReserveWarning = 128
    $loaderReserveHardMinimum = 96
    $uefiKernelByteLimit = 2 * 1024 * 1024
    $sectorCount = [int][Math]::Ceiling($kernelBytes.Length / 512.0)
    $loaderSectorReserve = $loaderSectorLimit - $sectorCount
    $kernelSizeMap = Get-BinutilsSectionSizes -Path $kernelPe
    $uefiKernelSizeMap = Get-BinutilsSectionSizes -Path $uefiKernelPe
    $objectSizeMaps = @($objectFiles | ForEach-Object { Get-BinutilsSectionSizes -Path $_ })
    $uefiObjectSizeMaps = @($uefiObjectFiles | ForEach-Object { Get-BinutilsSectionSizes -Path $_ })
    $topObjectSizeMaps = @($objectSizeMaps |
        Sort-Object -Property @{ Expression = { $_.Total }; Descending = $true }, Name |
        Select-Object -First 3)
    $uefiTopObjectSizeMaps = @($uefiObjectSizeMaps |
        Sort-Object -Property @{ Expression = { $_.Total }; Descending = $true }, Name |
        Select-Object -First 3)
    $topSizeObject = $topObjectSizeMaps[0]
    $uefiTopSizeObject = $uefiTopObjectSizeMaps[0]
    $marker = [System.Text.Encoding]::ASCII.GetBytes("KS64")
    $markerIndex = -1

    if ($bootBytes.Length -ne 512) {
        throw "x86_64 boot sector must be exactly 512 bytes. Actual length: $($bootBytes.Length)"
    }

    if ($sectorCount -le 0) {
        throw "x86_64 scaffold image is empty."
    }

    if ($sectorCount -gt $loaderSectorLimit) {
        throw "x86_64 scaffold exceeds the chunked BIOS loader budget of $loaderSectorLimit sectors."
    }
    if ($loaderSectorReserve -lt $loaderReserveHardMinimum) {
        throw "x86_64 scaffold BIOS loader reserve $loaderSectorReserve is below the hard minimum of $loaderReserveHardMinimum sectors."
    }
    if ($loaderSectorReserve -lt $loaderReserveWarning) {
        Write-Warning "x86_64 scaffold BIOS loader reserve $loaderSectorReserve is below the warning threshold of $loaderReserveWarning sectors."
    }
    if ($uefiKernelBytes.Length -gt $uefiKernelByteLimit) {
        throw "x86_64 UEFI payload exceeds the $uefiKernelByteLimit-byte UEFI kernel file contract."
    }

    for ($i = 0; $i -le ($bootBytes.Length - $marker.Length - 2); $i++) {
        $found = $true
        for ($j = 0; $j -lt $marker.Length; $j++) {
            if ($bootBytes[$i + $j] -ne $marker[$j]) {
                $found = $false
                break
            }
        }

        if ($found) {
            $markerIndex = $i
            break
        }
    }

    if ($markerIndex -lt 0) {
        throw "Could not locate x86_64 kernel sector marker in boot sector."
    }

    $countOffset = $markerIndex + $marker.Length
    $bootBytes[$countOffset] = [byte]($sectorCount -band 0xFF)
    $bootBytes[$countOffset + 1] = [byte](($sectorCount -shr 8) -band 0xFF)
    [System.IO.File]::WriteAllBytes($bootBin, $bootBytes)

    $imageSize = 1474560
    $imageStream = [System.IO.File]::Open($imagePath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $imageStream.SetLength($imageSize)
        $imageStream.Write($bootBytes, 0, $bootBytes.Length)
        $imageStream.Seek(512, [System.IO.SeekOrigin]::Begin) | Out-Null
        $imageStream.Write($kernelBytes, 0, $kernelBytes.Length)
    }
    finally {
        $imageStream.Dispose()
    }

    [System.IO.File]::WriteAllBytes($kernelOut, $kernelBytes)
    [System.IO.File]::WriteAllBytes($uefiKernelOut, $uefiKernelBytes)
    [System.IO.File]::WriteAllBytes($artifactBin, $kernelBytes)
    [System.IO.File]::WriteAllBytes($uefiArtifactBin, $uefiKernelBytes)
    Copy-Item -Force $kernelPe $artifactPe
    Copy-Item -Force $uefiKernelPe $uefiArtifactPe
    Copy-Item -Force $uefiPe $uefiArtifact

    $sizeReportLines = @(
        "LimitlessOS x86_64 size map",
        "kernel-bytes=$($kernelBytes.Length)",
        "bios-kernel-sectors=$sectorCount",
        "bios-sector-limit=$loaderSectorLimit",
        "bios-sector-reserve=$loaderSectorReserve",
        "uefi-kernel-bytes=$($uefiKernelBytes.Length)",
        "uefi-kernel-byte-limit=$uefiKernelByteLimit",
        "uefi-kernel-byte-reserve=$($uefiKernelByteLimit - $uefiKernelBytes.Length)",
        "section-text=$($kernelSizeMap.Text)",
        "section-rodata=$($kernelSizeMap.Rodata)",
        "section-data=$($kernelSizeMap.Data)",
        "section-bss=$($kernelSizeMap.Bss)",
        "uefi-section-text=$($uefiKernelSizeMap.Text)",
        "uefi-section-rodata=$($uefiKernelSizeMap.Rodata)",
        "uefi-section-data=$($uefiKernelSizeMap.Data)",
        "uefi-section-bss=$($uefiKernelSizeMap.Bss)"
    )
    foreach ($objectSizeMap in $topObjectSizeMaps) {
        $sizeReportLines += "top-object=$($objectSizeMap.Name) text=$($objectSizeMap.Text) rodata=$($objectSizeMap.Rodata) data=$($objectSizeMap.Data) bss=$($objectSizeMap.Bss) total=$($objectSizeMap.Total)"
    }
    foreach ($objectSizeMap in $uefiTopObjectSizeMaps) {
        $sizeReportLines += "uefi-top-object=$($objectSizeMap.Name) text=$($objectSizeMap.Text) rodata=$($objectSizeMap.Rodata) data=$($objectSizeMap.Data) bss=$($objectSizeMap.Bss) total=$($objectSizeMap.Total)"
    }
    Set-Content -Path $sizeReportPath -Value $sizeReportLines -Encoding Ascii

    if (Test-Path $uefiStageDir) {
        Remove-Item -Recurse -Force $uefiStageDir
    }

    New-Item -ItemType Directory -Force -Path $uefiBootDir | Out-Null
    New-Item -ItemType Directory -Force -Path $uefiAppsDir | Out-Null
    Copy-Item -Force $uefiPe $uefiBootPath
    [System.IO.File]::WriteAllBytes($uefiKernelPath, $uefiKernelBytes)
    $uefiLsAppText = "3`n3`n2`n3`nls [path] - list directory entries from cwd or a given path`nfilesystem`n"
    $uefiCatAppText = "4`n3`n2`n3`ncat <path> - print file contents`nfilesystem`n"
    $uefiStatAppText = "7`n3`n2`n3`nstat <path> - show file or directory metadata`nfilesystem`n"
    $uefiMkdirAppText = "5`n3`n2`n1`nmkdir <path> - create a directory tree`nfilesystem`n"
    $uefiWriteAppText = "6`n11`n3`n1`nwrite <path> <text> - replace file contents`nfilesystem`n"
    $uefiTouchAppText = "14`n3`n2`n1`ntouch <path> - create an empty file if missing`nfilesystem`n"
    $uefiAppendAppText = "9`n11`n3`n1`nappend <path> <text> - append text to a file`nfilesystem`n"
    $uefiCopyAppText = "15`n23`n5`n1`ncopy <source> <dest> - copy a file across directories`nfilesystem`n"
    $uefiDeleteAppText = "10`n3`n2`n1`ndelete <path> - remove a file or empty directory`nfilesystem`n"
    $uefiRenameAppText = "8`n19`n3`n1`nrename <from> <to> - rename within one directory`nfilesystem`n"
    $uefiMoveAppText = "11`n23`n5`n1`nmove <source> <dest> - move across directories`nfilesystem`n"
    [System.IO.File]::WriteAllBytes($uefiLsAppPath, [System.Text.Encoding]::ASCII.GetBytes($uefiLsAppText))
    Copy-Item -Force $diskLsImageBin $uefiLsBinPath
    [System.IO.File]::WriteAllBytes($uefiCatAppPath, [System.Text.Encoding]::ASCII.GetBytes($uefiCatAppText))
    [System.IO.File]::WriteAllBytes($uefiStatAppPath, [System.Text.Encoding]::ASCII.GetBytes($uefiStatAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "MKDIR.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiMkdirAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "WRITE.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiWriteAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "TOUCH.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiTouchAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "APPEND.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiAppendAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "COPY.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiCopyAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "DELETE.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiDeleteAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "RENAME.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiRenameAppText))
    [System.IO.File]::WriteAllBytes((Join-Path $uefiAppsDir "MOVE.APP"), [System.Text.Encoding]::ASCII.GetBytes($uefiMoveAppText))
    foreach ($spec in $diskFlatBinarySpecs) {
        Copy-Item -Force $spec.Bin (Join-Path $uefiAppsDir "$($spec.Name).BIN")
    }
    $kernelChecksum = Get-Fnv1aDataChecksum -Bytes $uefiKernelBytes
    $kernelChecksumHex = "0x{0:X8}" -f $kernelChecksum
    $profileStatusLines = if ($BuildProfile -eq "Product") {
        "- build profile: Product`n- experimental runtime surfaces are quarantined: GUI/window-manager/desktop, AI, installer, and package-manager behavior are unavailable and must not be presented as product-path"
    }
    else {
        "- build profile: Experimental`n- experimental proof/runtime surfaces may initialize, but verifier-visible logs must label them experimental or proof-only and they are not product-path behavior"
    }
    $networkStatusLine = if ($BuildProfile -eq "Product") {
        "- Product profile initializes broker-private virtio-net or e1000e networking only when supported hardware is present, exposes status through the shell net command, and reports truthful unavailable telemetry without sockets or ambient network authority when hardware or DHCP is absent"
    }
    else {
        "- Experimental UEFI media paths attach a modern virtio-net/e1000e PCI device, discover it through ECAM vendor/device matching, parse network device configuration, complete broker-private ARP/DHCP/DNS/HTTP proof exchanges, and still grant no filesystem, storage, or ambient network authority"
    }
    Set-Content -Path $uefiReadmePath -Encoding Ascii -Value @"
LimitlessOS x86_64 UEFI stage

This directory is the removable-media layout for the first UEFI scaffold lane.
The fallback path is EFI\BOOT\BOOTX64.EFI.

Current status:
$profileStatusLines
- removable UEFI image is bootable under OVMF in QEMU with GOP framebuffer, boot-info framebuffer mapping, kernel framebuffer draw, brokered display marker/panel/text proof, line-cleared scrolling brokered console-to-framebuffer mirror proof, brokered PS/2 plus xHCI HID keyboard event telemetry, profile-labeled network availability/quarantine telemetry, explicit broker-side keyboard-read proof, compact sealed bootstrap proof, real-media storage proofs, disk-sourced descriptor/binary launch proofs, boot-media file-read, loader-buffer, kernel-placement, linked-base placement, boot-handoff table, kernel-jump, and x64 userspace proofs
- UEFI ISO is bootable under OVMF in QEMU with GOP framebuffer, boot-info framebuffer mapping, kernel framebuffer draw, brokered display marker/panel/text proof, line-cleared scrolling brokered console-to-framebuffer mirror proof, brokered PS/2 plus xHCI HID keyboard event telemetry, profile-labeled network availability/quarantine telemetry, explicit broker-side keyboard-read proof, compact sealed bootstrap proof, real-media storage proofs, disk-sourced descriptor/binary launch proofs, boot-media file-read, loader-buffer, kernel-placement, linked-base placement, boot-handoff table, kernel-jump, and x64 userspace proofs
- both UEFI media paths read and verify the staged root README.TXT from the boot volume
- both UEFI media paths read BOOTMAN.TXT, load KERNEL64.BIN into a bounded handoff buffer, and report a checksum match
- both UEFI media paths allocate exact firmware-owned loader pages from conventional memory, copy/check KERNEL64.BIN there, and prove the linked x64 scaffold image can be copied at physical 0x10000 for the higher-half kernel entry
- both UEFI media paths build the low boot-handoff page tables, dedicated framebuffer page-directory, and trampoline, take a final silent memory-map key, exit firmware boot services, jump into the x64 kernel, draw/log a kernel-owned framebuffer marker, and reach the compact sealed bootstrap, second-page userspace display/filesystem, real-media storage, and disk-sourced utility launch proofs
- brokered keyboard proofs still read staged PS/2 bytes through scoped input authority, auto-fall back from set-2 to set-1 scancode decoding when the media path exposes set-1 release scancodes, and keep hardware input separate from command execution now sourced through disk-backed APPS descriptors and flat binaries
- UEFI media paths also attach a qemu-xhci USB keyboard, discover the xHCI controller through ECAM class 0x0C/subclass 0x03/progif 0x30, map BAR0 through a broker-private kernel MMIO aperture, stage page-aligned DCBAA/command/event/control/interrupt-ring memory, reset and run the controller, enable and address a device slot, read USB configuration plus HID report descriptors, configure the boot keyboard interrupt endpoint, and prove drs-xhci by routing bounded boot-protocol HID reports into the same brokered input queue without creating ambient input authority
$networkStatusLine
- the hardware-inventory/MMIO proofs now keep AHCI access behind brokered query authority, deny wrong-owner map/snapshot/classification requests, discover PCI through ACPI MCFG/ECAM on UEFI media when available, fall back to legacy I/O-port config on BIOS/no-MCFG media, install a kernel-only/read-only/no-deref MMIO page-table view for Q35 AHCI candidates with cache-disabled/NX entry flags, and only read narrow HBA plus port-state snapshots and derived readiness policy until a real driver policy exists
- the AHCI command-issue preflight now binds prepared table, memory, command, and read-plan tokens, records slot/controller readiness plus timeout policy, proves checksum match, and still reports zero DMA, MMIO writes, port programming, and command issue
- the AHCI address-bind preflight now computes future command-list/table/bounce physical addresses and a predicted checksum without writing memory, mapping DMA, publishing ports, or issuing commands
- the AHCI private address-patch stage now writes predicted CTBA/PRDT addresses only into the broker-owned command page and proves zero DMA, MMIO, port publication, arm, or command issue side effects
- the AHCI controller-publication preflight now computes future CLB/FB register offsets and values from the private page while still proving zero MMIO writes, port programming, controller publication, arm, or command issue side effects
- the AHCI write-window open attempt now binds the non-destructive revocation preflight to the write-window token and proves the broker still refuses to enable write access until revocation is actually executed
- the AHCI capability-drain checkpoint now executes the broker-side hardware capability revocation after the blocked exclusive-session proof while still proving zero MMIO, port publication, arm, or command issue side effects
- the AHCI driver-handoff checkpoint now proves the old hardware handle is stale after drain and regrants only a fresh query-only hardware capability to the block-worker principal, with zero MMIO, port publication, arm, or command issue side effects
- the AHCI driver-owned read-only probe now consumes the fresh post-handoff query-only capability to reread controller/ATAPI port readiness while proving zero MMIO writes, port programming, DMA, publication, arm, or command issue side effects
- the AHCI driver-owned read-intent and read-buffer checkpoints bind that post-handoff probe into explicit one-block read metadata and a zeroed private buffer descriptor while still proving zero MMIO writes, port programming, DMA, publication, arm, command issue, or media-read side effects
- the AHCI driver-owned read-status-clear checkpoint now consumes a hardware-backed read-only status sample only as a denied PxIS clear request, proving PxIS remains unchanged and zero MMIO writes, port programming, command issue, DMA, arm, media-read, or media-write side effects occur
- the AHCI driver read-execution gate now binds that read-buffer token into a locked future-read policy object, proving query-only authority cannot issue the read while keeping MMIO, port, DMA, command, arm, media-read, and storage-write side effects at zero
- the AHCI driver read-execution attempt now consumes that locked gate only to prove the broker still denies issue under query-only authority, with UEFI/ISO reporting driver-exec-state 3 and BIOS/no-AHCI reporting explicit unavailable telemetry
- the AHCI driver read-result/export checkpoint now consumes the denied execution token only to prove no media bytes, block capability, or filesystem authority can be exported under query-only authority, with UEFI/ISO reporting driver-result-state 3 and BIOS/no-AHCI reporting explicit unavailable telemetry
- the AHCI driver block-publication checkpoint now consumes the denied result token only to prove no externally visible block endpoint, block capability, filesystem authority, media read, or media write can appear under query-only authority, with UEFI/ISO reporting driver-publish-state 3 and BIOS/no-AHCI reporting explicit unavailable telemetry
- the AHCI driver read-authority checkpoint now consumes the denied block-publication token only to prove no media-read authority or storage surface can be granted under query-only authority, with UEFI/ISO reporting drg-state 3 and BIOS/no-AHCI reporting explicit unavailable telemetry
- the AHCI driver media-read checkpoint now consumes the denied read-authority token only as an explicit denied media-read attempt, proving no media bytes, block endpoint, block capability, filesystem authority, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-completion checkpoint now consumes the denied media-read token only as an explicit completion denial, proving no completion status, media bytes, block endpoint, block capability, filesystem authority, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-capability checkpoint now consumes the denied completion token only as an explicit read-result capability denial, proving no read-result capability, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-export checkpoint now consumes the denied read-result capability token only as an explicit caller-visible export denial, proving no user-visible bytes, user-buffer write, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-response checkpoint now consumes the denied read-export token only as an explicit block/read response denial, proving no response bytes, response status, response checksum, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-delivery checkpoint now consumes the denied read-response token only as an explicit block-service delivery denial, proving no delivered bytes, delivery status, delivery checksum, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-visibility checkpoint now consumes the denied read-delivery token only as an explicit service/client-visible result denial, proving no visible bytes, visibility status, visibility checksum, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-commit checkpoint now consumes the denied read-visibility token only as an explicit committed-result denial, proving no committed bytes, commit status, commit checksum, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-audit checkpoint now consumes the denied read-commit token only as an explicit audit/result-finalization denial, proving no audited bytes, audit status, audit checksum, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-activate checkpoint now consumes the denied read-upgrade token only as an explicit controller-activation denial, proving no activated capability, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-arm checkpoint now consumes the denied read-activation token only as an explicit controller-arm denial, proving no armed capability, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-submit checkpoint now consumes the denied read-arm token only as an explicit command-submission denial, proving no submitted command capability, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-observe checkpoint now consumes the denied read-submit token only as an explicit completion-observation denial, proving no observed status, observed bytes, observed checksum, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-retire checkpoint now consumes the denied read-observe token only as an explicit completion-retirement denial, proving no retired status, retired bytes, retired checksum, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-permit checkpoint now consumes the denied read-retire token only as an explicit read-permission denial, proving no permit capability, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-window checkpoint now consumes the denied read-permit token only as an explicit read-window-open denial, proving no read-window capability, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- the AHCI driver read-lease checkpoint now consumes the denied read-window token only as an explicit reusable-read-lease denial, proving no read-lease capability, active read lease, read authority, execute authority, block endpoint, block capability, filesystem authority, media bytes, MMIO write, port program, publish, command issue, DMA, arm transition, media read, or media write can appear under query-only authority
- raw x86_64 BIOS image remains the main kernel scaffold path for kernel-core convergence
"@
    Set-Content -Path $uefiManifestPath -Encoding Ascii -Value @"
LimitlessOS boot manifest v1
architecture=x86_64
kernel=KERNEL64.BIN
kernel-bytes=$($uefiKernelBytes.Length)
kernel-byte-limit=$uefiKernelByteLimit
kernel-byte-reserve=$($uefiKernelByteLimit - $uefiKernelBytes.Length)
kernel-checksum=$kernelChecksumHex
handoff=uefi-loader-proof
boot-contract=uefi-kernel-file
"@

    Write-Host "Packaging x86_64 UEFI FAT image"
    & $uefiFatGenerator `
        -InputEfiPath $uefiArtifact `
        -OutputImagePath $uefiImage `
        -BootManifestPath $uefiManifestPath `
        -KernelPayloadPath $uefiKernelPath
    if (-not $?) {
        throw "Failed to generate x86_64 UEFI FAT image."
    }

    Write-Host "Packaging x86_64 UEFI ISO"
    & $isoGenerator `
        -InputImagePath $imagePath `
        -BootImagePath $uefiImage `
        -StageSourcePath $uefiStageDir `
        -OutputIsoPath $isoPath `
        -Architecture x86_64 `
        -BootMode uefi
    if (-not $?) {
        throw "Failed to generate x86_64 ISO image."
    }

    $runtimeImageAsmReport = ConvertTo-RepoRelativePath $runtimeImageAsm
    $uefiArtifactReport = ConvertTo-RepoRelativePath $uefiArtifact
    $uefiImageReport = ConvertTo-RepoRelativePath $uefiImage
    $uefiStageReport = ConvertTo-RepoRelativePath $uefiStageDir
    $isoReport = ConvertTo-RepoRelativePath $isoPath
    $imageReport = ConvertTo-RepoRelativePath $imagePath
    $artifactPeReport = ConvertTo-RepoRelativePath $artifactPe
    $artifactBinReport = ConvertTo-RepoRelativePath $artifactBin
    $uefiArtifactPeReport = ConvertTo-RepoRelativePath $uefiArtifactPe
    $uefiArtifactBinReport = ConvertTo-RepoRelativePath $uefiArtifactBin
    $sizeReportPathReport = ConvertTo-RepoRelativePath $sizeReportPath

$report = @"
LimitlessOS x86_64 scaffold
state: bootable bios long-mode scaffold
arch: x86_64
build-profile: $BuildProfile
experimental-runtime-enabled: $(if ($BuildProfile -eq "Experimental") { 1 } else { 0 })
bootstrap: bios64-long-mode-image
entry: _start
paging: active 4-level long mode with 16 MiB identity/high-half alias map and higher-half kernel execution
loader: bounded chunked BIOS loader reads the scaffold in 127-sector chunks and supports images up to 1024 sectors before the low-memory stack window
loader-budget: bios-sector-limit $loaderSectorLimit current-sectors $sectorCount reserve-sectors $loaderSectorReserve enforced 1
uefi-loader-budget: kernel-byte-limit $uefiKernelByteLimit current-bytes $($uefiKernelBytes.Length) reserve-bytes $($uefiKernelByteLimit - $uefiKernelBytes.Length) checksum $kernelChecksumHex enforced 1
size-map: text-bytes $($kernelSizeMap.Text) rodata-bytes $($kernelSizeMap.Rodata) data-bytes $($kernelSizeMap.Data) bss-bytes $($kernelSizeMap.Bss) top-object $($topSizeObject.Name) top-object-total $($topSizeObject.Total)
uefi-size-map: text-bytes $($uefiKernelSizeMap.Text) rodata-bytes $($uefiKernelSizeMap.Rodata) data-bytes $($uefiKernelSizeMap.Data) bss-bytes $($uefiKernelSizeMap.Bss) top-object $($uefiTopSizeObject.Name) top-object-total $($uefiTopSizeObject.Total)
boot-info: shared x86/x64 handoff contract active
package-archive: shared bootstrap package catalog v2 summary visible on BIOS and UEFI x64 paths with kernel-service manifests and payload offset/size/checksum metadata included for sealed x64 services
runtime-image-source: sealed x64 ring3 transfer image assembled from $runtimeImageAsmReport into a generated page-aligned 16 KiB persistent-shell bootstrap payload before package metadata and kernel compilation
principals: x64 principal registry exposes active system, policy, console, and service-worker identities through int 0x80 and native syscall queries
processes: x64 bootstrap process registry binds service PIDs to principals, endpoints, sealed state, scheduler classes, capability budgets, verified package manifests, and operation-aware launch/quiesce/drain/restart lifecycle records plus compact lifecycle phase, runtime-identity, runtime-image, runtime-image-plan, real supervisor-only image-map installation, separate user executable and stack map proofs, protection-token, install-token, entry-transfer, process-owned user-entry RIP/RSP/selectors/RFLAGS, and runtime-payload offset/size/checksum queries through int 0x80 and native syscall paths
launch: x64 manifest verifier validates the shared archive, accepts kernel-service-authority manifests, ignores user-app manifests, starts each sealed service exactly once through an init-authorized launch request log, proves quiesce preflight succeeds only with zero live target capabilities, proves brokered capability drain revokes live target handles before post-drain quiesce, denies restart while only drained, proves restart only after quiesce-ready, rekeys the service runtime generation/token on approved restart, refreshes a broker-computed runtime image token from verified package payload offset/size/checksum metadata, derives a sealed runtime image plan with base, entry, mapped bytes, rights, and a plan token, installs real four-page supervisor-only and user read/execute mappings for a sealed 16 KiB persistent-shell bootstrap transfer image, records page count, PML4/PDPT/PD indexes, map token, protection token, install token, source checksum, controlled entry probe result, and explicit user/protection proofs, proves controlled ring3 transfer, IF-enabled user IRQ frame capture, scheduler-owned task-frame switching, second-page filesystem/display mutation, disk-sourced descriptor/binary launch handoff, and a two-task scheduler_x64 saved-frame run queue with runtime/user-entry token binding, mirrors runtime, image, plan, map, protection, install, transfer, and payload identity into approved requests, binds post-restart service capabilities to that new runtime identity, rejects old runtime tokens, exposes drained/quiesce-ready/restart service counts and per-manifest lifecycle phases, proves payload metadata and image-map geometry stay stable across restart while plan/map/transfer tokens rekey with the runtime image, and audits protected-service stop requests with pending, approved, denied, and completed telemetry
user-entry: x64 broker derives a tokenized ring-3 entry frame with RIP 0x41000010, RSP 0x40020000, selectors 0x33/0x2B, interrupt-masked RFLAGS 0x00000002, denial 0, and transfer-ready telemetry once separate user executable and stack views are installed without weakening the supervisor-only validation mapping, verifies controlled iretq entry, IF-enabled PIT frame capture, scheduler-owned switching, reusable scheduler_x64 saved-frame dispatch, brokered fs_read and CLI-shaped probes, line-oriented shell-stream dispatch, and second-page filesystem/display mutation at RIP 0x41001ED0, with command execution now sourced from disk-backed APPS descriptors and flat binaries through the launch broker
descriptors: x64 kernel installs its own long-mode GDT and TSS, exposes kernel selectors 0x18/0x20, user selectors 0x33/0x2B, task register 0x38, TSS rsp0, descriptor tokens, and native syscall STAR readiness through direct, int 0x80, and native syscall telemetry
services: shared x64 service namespace scaffold with package-aware query ABI visible on BIOS and UEFI x64 paths, including policy, console, RAMFS, input, display, block, and hardware-inventory endpoints
capabilities: x64 service handles are principal-scoped and runtime-bound, can grant, delegate with short leases and attenuation, route, query live target exposure, revoke with child cascade, drain by authorized launch-broker request, and reject unknown-principal, wrong-owner, expired, stale, stale-runtime, second-hop, or over-broad authority through int 0x80 and native syscall proofs
console: x64 brokered console syscall surface accepts only principal-scoped console service capabilities, validates user image or stack buffers before reading bytes, writes to the boot/debug console path, and reports write/byte/denial telemetry through int 0x80
input: x64 brokered input syscall surface accepts only principal-scoped input service capabilities, validates writable user-stack or kernel-high buffers before copying command bytes, keeps the original byte-stream read path for compatibility, adds a line-oriented read syscall for shell command boundaries, normalizes backspace/delete bytes in line reads before copying command text to userspace, stages IRQ1-backed PS/2 keyboard input in a bounded broker-owned queue with set-1 scancode translation on BIOS boots, set-2 decoding support for framebuffer/UEFI handoff, and automatic set-1 fallback when high-bit set-1 release scancodes appear after a UEFI handoff, translates basic extended cursor/delete escape sequences, actively polls during authorized keyboard reads, clears stale pending bytes on scancode interpretation switches, discards overlong stale fragments before copying later lines, adds explicit keyboard byte-read and hardware-line-read syscalls that consume staged hardware bytes only after the caller presents scoped input authority, keeps hardware-keyboard bytes separate from the deterministic seeded startup stream until a live shell mode explicitly consumes them, seeds the command stream as cat README.TXT followed by a typo-corrected help line, line-delimited help ls, help cat, help stat, help mkdir, help write, write SHELL.TXT, cat SHELL.TXT, apps with M3-labeled product inventory, pwd, ls /, ls APPS with the same M3-labeled inventory, info ls, info cat, info stat, info mkdir, info write, cat README.TXT, stat README.TXT, helpX, and noop, and reports read/line/byte/edit/denial/eof plus keyboard IRQ/poll/scancode/translated-byte/pending/read/line/drop/last-key telemetry through int 0x80
disk-sourced-shell: brokered shell startup now scans real ISO /APPS descriptors through the scoped read-only filesystem delegation, prefers checksum-verified disk flat binaries over sealed fallback code, and keeps keyboard input, console, RAMFS, block, and ISO filesystem authority separately scoped
display: x64 brokered display syscall surface accepts only principal-scoped display service capabilities, consumes UEFI GOP framebuffer metadata from boot-info, draws bounded markers, clears a kernel-bounded text panel, renders tiny 5x7-font text only when framebuffer geometry is available, mirrors successful brokered console writes into a bounded line-cleared scrolling framebuffer viewport without exposing direct framebuffer access to shell processes, reports draw/pixel/clear/text/console-mirror/line-clear/wrap/scroll/denial/unavailable/token telemetry, and degrades to an explicit unavailable count on raw BIOS boots instead of bypassing the capability model
block: x64 brokered block syscall surface exposes read-only sector-read paths only through principal-scoped block service capabilities, rejects wrong-owner calls before touching disk data, reports availability/status/read/byte/denial/unavailable/token telemetry, proves raw BIOS boot-media LBA0 reads with the 0x55AA boot signature, proves the UEFI/ISO AHCI drs-block route can publish a read-only block-worker-owned endpoint over a sealed drs-read result, lets the kernel storage layer consume that block capability for a broker-private ISO9660 README.TXT read, delegates one scoped read-only drs-fs-user filesystem capability to ring3 for /APPS/LS.APP, keeps one persistent drs-fs-shell delegation for a dynamic disk-sourced /APPS descriptor scan, and keeps NVMe IO reads broker-private without block publication, write, format, commit, or additional filesystem authority
pci-inventory: x64 reads PCI configuration space only after a caller presents query authority for the hardware-inventory service, proves wrong-owner denial for inventory and MMIO-plan queries, proves legacy IDE discovery on BIOS media, proves AHCI and NVMe controller discovery on Q35 UEFI media, exports AHCI MMIO base/span/flags/token to the brokered MMIO planner, exports NVMe BAR0/BAR1 shape only to the admin-probe path, and does not perform storage writes
pci-ecam: x64 UEFI handoff now records minimal ACPI RSDP/XSDT/MCFG metadata so q35 UEFI and ISO media scan PCI through segment-0 ECAM config reads when MCFG is present, report drs-pci-ecam-rsdp 1, drs-pci-ecam-mcfg 1, a nonzero ECAM base, AHCI found through ECAM, and NVMe enumerated through the same ECAM scanner, while BIOS/disk media report MCFG unavailable and cleanly fall back to legacy 0xCF8/0xCFC config-space reads without adding uncontrolled controller programming, DMA, block, filesystem, write, or commit authority
nvme-probe: x64 UEFI and ISO media now prove drs-nvme-probe by finding a class 0x01/0x08 NVMe controller through ECAM, mapping its 64-bit BAR0 through broker-private MMIO, initializing only admin SQ/CQ pages plus a 4 KiB Identify buffer, enabling the controller with bounded CSTS.RDY polling, submitting one Identify Controller command, recording model/firmware strings, and reporting zero IO queue, zero read authority, zero filesystem authority, zero write authority, and zero commit authority; BIOS/disk media report unavailable
nvme-read: x64 UEFI and ISO media now prove drs-nvme-read by binding to the drs-nvme-probe token, creating broker-private IO completion/submission queues through bounded admin commands, submitting one NVM Read command for namespace 1 LBA0 into a kernel-owned 4 KiB PRP buffer, polling the IO completion queue with phase/CID/status checks, reporting issued/completed/status 0/4096 bytes/nonzero checksum, and still publishing no block endpoint, filesystem authority, write authority, format authority, or commit authority; BIOS/disk media report unavailable
nvme-gpt: x64 UEFI and ISO media now prove drs-nvme-gpt by booting with a deterministic GPT/FAT32 NVMe sidecar image, binding to the sealed drs-nvme-read token, reading GPT header LBA1 and partition entries at LBA2 through the existing IO queue, validating EFI PART/revision/header CRC, discovering the FAT32/basic-data partition at LBA 2048, reading its VBR, and still publishing zero filesystem authority and zero write authority; BIOS/disk media report unavailable
nvme-fat: x64 UEFI and ISO media now prove drs-nvme-fat by binding to the sealed drs-nvme-gpt token, parsing the FAT32 BPB from the already validated VBR, reconstructing ASCII and UTF-16 LFN entries, matching UTF-8 path input for the Caf\u00E9.txt fixture, traversing APPS/DATA subdirectories, following multi-cluster FAT chains, reading NVME.TXT plus long-name, Unicode-name, nested, and multi-cluster fixtures through the existing IO queue, matching staged content checksums, then exercising a broker-private mutation gate that allocates cluster 12 for a new LFN file, extends NVME.TXT through cluster 13, tombstones and frees REMOVE.ME on cluster 14, flushes every dirty FAT/data/directory sector through NVMe writes, and reads each result back while still publishing zero filesystem delegation, zero block endpoint, zero caller write authority, and zero commit authority; BIOS/disk media report unavailable
apic: x64 UEFI handoff now records minimal ACPI MADT metadata so q35 UEFI and ISO media can enable the Local APIC, map the IOAPIC through a dedicated supervisor-only APIC page table, mask the 8259 PIC after bounded redirection setup, report drs-apic-madt 1, nonzero LAPIC/IOAPIC bases, drs-apic-pic-disabled 1, timer ticking 1, keyboard live 1, and keep the full storage chain green; drs-apic-override additionally records MADT interrupt source overrides for ISA IRQs 0-15, routes PIT/keyboard/AHCI through the remapped GSI/polarity/trigger data when present, proves QEMU q35 timer delivery through GSI 2, and leaves BIOS/disk media on the PIC fallback with no override scan
xhci-input: x64 UEFI and ISO media now prove drs-xhci by finding an ECAM-discovered class 0x0C/subclass 0x03/progif 0x30 xHCI controller, mapping its BAR0 through a broker-private kernel MMIO aperture, staging page-aligned DCBAA, command-ring, event-ring, control-transfer, and interrupt-transfer memory, resetting and running the controller, enabling and addressing a USB device slot, reading USB configuration plus HID report descriptors, configuring a boot-protocol keyboard interrupt endpoint, and routing bounded 8-byte HID reports into the existing brokered input queue; BIOS/disk media report unavailable. This checkpoint proves the real controller enumeration path while preserving PS/2 fallback and creates no ambient input authority or user-visible USB device authority.
network-profile: $networkStatusLine
mmio-planner: x64 promotes discovered AHCI BAR metadata into a brokered MMIO planning surface that requires hardware-inventory query authority, proves wrong-owner denial for base queries plus map/controller-snapshot/port-snapshot/port-policy/read-plan/command-plan/memory-plan/table-prep/issue/bind/patch/publish/gate/write-window/revoke/open/session/drain/handoff/driver-probe/driver-intent/driver-buffer/driver-gate/driver-exec/driver-result/driver-publish/drg/dmr/drc/drcap/drx/drr/drd/drv/drk/dra/dru/dact/darm/dsub/dobs/dret/dprm/dwin/dlse/duse requests, classifies no-device BIOS boots as safe-no-touch/unmapped with unavailable controller, port, policy, read-plan, command-plan, memory-plan, table-prep, issue, bind, patch, publish, gate, write-window, revoke, open-attempt, session, drain, handoff, driver-probe, driver-intent, driver-buffer, driver-gate, driver-exec, driver-result, driver-publish, drg, dmr, drc, drcap, drx, drr, drd, drv, drk, dra, dru, dact, darm, dsub, dobs, dret, dprm, dwin, dlse, and duse telemetry, classifies Q35 UEFI AHCI as a bounded candidate, installs a kernel-only/read-only/no-deref page-table view at the reserved high-half MMIO window, reports page count, map-installed 1, table indices 511/510/128/0, cache-disabled/NX entry flags 0x8000000000000019, reads only CAP/GHC/PI/VS plus selected implemented/active port SSTS/signature/command/task-file/command-issue/error through brokered read-only snapshots, derives device kind/link/busy/DRQ/CI-idle/read-eligibility policy from those snapshots, stages non-executing AHCI read-plan, command-layout, one-page command-memory, broker-private table-prep, non-issuing issue-preflight, predicted address-bind, private address-patch, controller-publication preflight, revocation-gated publication, write-window policy, non-destructive revocation-preflight, denied write-window-open, blocked session, executed drain, post-drain handoff, driver-owned read-only probe, driver-owned read-intent, read-buffer, read-execution gate, denied execution attempt, result/export denial, block-publication denial, read-authority denial, denied media-read, read-completion denial, read-capability denial, read-export denial, read-response denial, read-delivery denial, read-visibility denial, read-commit denial, read-audit denial, read-upgrade denial, read-activation denial, read-arm denial, read-submit denial, read-observation denial, read-retirement denial, read-permission denial, read-window-open denial, read-lease denial, and read-use denial tokens that bind selected port, policy/read-plan/command-plan/memory-plan/table/issue/bind/patch/publish/gate/window/revoke/session/drain/handoff/probe/intent/buffer/gate/exec/result/publish/read-grant/media-read/complete/read-cap/read-export/read-response/read-delivery/read-visible/read-commit/read-audit/read-upgrade/read-activate/read-arm/read-submit/read-observe/read-retire/read-permit/read-window/read-lease tokens, LBA, block count, operation kind, command header/table sizes, CFIS/PRDT geometry, command opcode, ATAPI packet opcode, transfer hint, command-list/header/table/PRDT/bounce/FIS offsets, PRDT byte count, held-zero DMA address, live hardware-handle count, query-only driver ownership, read-only probe readiness, read-intent/buffer/result/publish/read-grant/media-read/complete/read-cap/read-export/read-response/read-delivery/read-visible/read-commit/read-audit/read-upgrade/read-activate/read-arm/read-submit/read-observe/read-retire/read-permit/read-window/read-lease/read-use shape, and checksum transition while proving all storage hardware mutation side effects plus media-read are zero, completion, read-capability, read-export, read-response, read-delivery, read-visible, read-commit, read-audit, read-upgrade, read-activate, read-arm, read-submit, read-observe, read-retire, read-permit, read-window, read-lease, and read-use requests produce no bytes, response/delivery/visibility/commit/audit/observed/retired status, upgraded, activated, armed, submitted, observed, retired, permit, read-window, read-lease, or read-use capability, or authority, and still performs no AHCI command, DMA mapping, controller-visible register write, user-visible read export, block-read response delivery, read-result visibility, block capability mint, filesystem authority mint, media read, or disk write
mmio-command-page: x64 now materializes the AHCI command-memory preflight as a broker-owned page-aligned zeroed kernel page, reports its virtual and physical page addresses, checksum 0x76EFDDC5, mem-zeroed 1, and mem-materialized 1 on Q35 AHCI media, then prepares a non-issuing broker-private command table with checksum 0x3FBFAF45, header flags 0x00010025, CFIS command 0xA0, packet opcode 0x28, PRDT byte count 2047, and table-written 1 while keeping BIOS/no-AHCI media unavailable with zero page authority and still exposing no user mapping, no DMA address, no port programming, and no command issue
mmio-drs-read: x64 now consumes drs-dwin into the first positive broker-private AHCI/ATAPI read: it patches the PRDT with the single-page bounce buffer physical address, records command-table checksum 0x14EC2F71 -> 0xD8BD95B6, programs only the selected port CLB/FB registers through the temporary kernel-only MMIO aperture, sets FRE/ST, clears PxIS/PxSERR, issues PxCI slot 0, polls bounded completion, seals 2048 bytes in the broker bounce buffer with a nonzero checksum, and still publishes no block endpoint, filesystem authority, user-visible read result, reusable read lease, write authority, commit authority, or media write
mmio-drs-block: x64 now consumes drs-read into the first read-only AHCI-backed block route: UEFI and ISO AHCI paths report drs-block-state 3, drs-block-flags 0x000FFFFF, owner 0x00001006, read-token binding 1, block endpoint 1, block cap minted 1, read-only 1, delegated cap 1, routed read 1, bytes 2048, checksum matching drs-read, wrong-owner denial 1, stale-handle denial 1, and zero write authority, commit authority, filesystem mint, format authority, or media write; BIOS/no-AHCI media reports unavailable drs-block-flags 0x800E0001
mmio-drs-fs-user: x64 now consumes drs-block into a broker-private ISO9660 read, then binds drs-fs into the first scoped read-only filesystem delegation to ring3: UEFI and ISO AHCI paths report drs-fs-state 3, drs-fs-flags 0x0001FFFF, PVD CD001 validation 1, README.TXT content-match 1, plus drs-fs-user-state 3, drs-fs-user-flags 0x001FFFFF, path /APPS/LS.APP, delegated 1, read-routed 1, root-read 1, bytes 79, checksum 0xFDB1F751, content-match 1, wrong-owner 1, stale 1, user-buffer 1, and zero write authority, commit authority, or additional filesystem capability mint; BIOS/no-AHCI media reports unavailable drs-fs-flags 0x8001C001 and drs-fs-user-flags 0x801C0001
mmio-drs-fs-shell: x64 now binds the scoped drs-fs-user delegation into the shell startup path so APPS descriptors can be sourced from a dynamic real-ISO /APPS scan: UEFI and ISO AHCI paths report drs-fs-shell-state 3, drs-fs-shell-flags 0x0001FFFF, owner 0x00001006, user owner 0x00000201, fs-user-bound 1, delegated 1, descriptors-read 11, parsed 1, scan-dynamic 1, ls/cat/stat dispatched 1, RAMFS and ISO routes 1, and zero write authority, commit authority, or additional filesystem capability mint; BIOS/no-AHCI media reports unavailable drs-fs-shell-flags 0x8000E001
mmio-drs-load-full: x64 now expands disk-sourced flat-binary utility launch through the same delegated read-only ISO filesystem route: UEFI and ISO AHCI paths report drs-load-full-state 3, drs-load-full-flags 0x00007FFF, owner 0x00001006, user owner 0x00000201, load-bound 1, fs-shell-bound 1, binaries 10, verified 10, registered 10, cat/mkdir/write/rename/move completed 1, source disk, write-escalation 0, commit 0, additional-fs-caps 0, staged 1, denials 0, and unavailable 0; BIOS/no-AHCI media reports unavailable drs-load-full-flags 0x80001C01
filesystem: x64 brokered RAMFS syscall surface reuses the shared ramfs implementation behind service-capability authorization, mints owner-scoped node capabilities for opened or created files and directories, enforces read/write/list/stat/create/revoke rights, rejects wrong-owner and revoked handles, reads README.TXT, lists APPS, writes NOTES.TXT, reports open/create/list/read/write/stat/revoke/denial telemetry through int 0x80, and is now exercised from sealed ring-3 user probes including CLI-shaped cat README.TXT paths, input-backed cat command receipt, a line-oriented shell loop, a second-page ring3 probe that creates USRNOTE.TXT and reads it back, and disk-sourced flat-binary utilities that can source APPS descriptors from the ISO-backed drs-fs-shell route while keeping RAMFS authority for mutable files, revokes temporary RAMFS handles, and keeps exact-length near misses and unknown input on non-fatal console-only branches
mmio-issue-preflight: x64 now proves a brokered AHCI command-issue preflight without issuing the command: UEFI and ISO AHCI paths report issue-state 3, issue-flags 0x06FFFFFF, bound table/memory/command/read-plan tokens, idle slot/CI, ready TFD and SERR policy, checksum match 0x3FBFAF45, stop/start plus timeout policy, and zero DMA/MMIO/port/command side effects, while BIOS/no-AHCI media reports the unavailable issue-flags 0x073F0001 path
mmio-address-bind: x64 computes a predicted-only AHCI address-bind plan with command-list/table/bounce physical addresses, predicted CTBA/PRDT fields, a page-address-derived checksum, and zero memory-write/DMA/MMIO/port/publish/issue side effects
mmio-private-patch: x64 now applies the AHCI address bind only inside the broker-owned command page: UEFI and ISO AHCI paths report patch-state 3, patch-flags 0x0DFFFFFF, page-checksum match, patch-memory-written 1, and zero DMA/MMIO/port/publish/arm/issue side effects, while BIOS/no-AHCI media reports unavailable patch-flags 0x03EFE001
mmio-publish-preflight: x64 now computes controller-publication values without committing them: UEFI and ISO AHCI paths report publish-state 3, publish-flags 0x03FFFFFF, patch/bind/issue/table/memory/command/read token binding, command-list and receive-FIS CLB/FB values, receive-FIS geometry 1536/256, alignment/range/below-4G readiness, and zero memory-write/DMA/MMIO-write/port-program/publish/arm/issue side effects, while BIOS/no-AHCI media reports unavailable publish-flags 0x07DFE001
mmio-publish-gate: x64 now requires a revocation-gated controller-publication gate after the non-writing publish preflight: UEFI and ISO AHCI paths report gate-state 3, gate-flags 0x0000FFFF, publish-token binding, exclusive live hardware handle, revocation required but not satisfied, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable gate-flags 0x0001FFF9
mmio-publish-window: x64 now adds a reusable write-window policy checkpoint after the publication gate: UEFI and ISO AHCI paths report window-state 3, window-flags 0x0000FFFF, gate-token and publish-token binding, exclusive live hardware handle, revocation required but not executed, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable window-flags 0x0001FFF1
mmio-publish-revoke: x64 now adds a non-destructive revocation preflight after the write-window policy: UEFI and ISO AHCI paths report revoke-state 3, revoke-flags 0x0000FFFF, window-token and gate-token binding, live-before/live-after 1, revocation planned but not executed, would-revoke 1, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable revoke-flags 0x0001FFF1
mmio-publish-open: x64 now attempts to open the AHCI publish write window only to prove the broker refuses it until revocation is executed: UEFI and ISO AHCI paths report open-state 3, open-flags 0x0000FFFF, revoke-token and window-token binding, live hardware handles present, open-allowed 0, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable open-flags 0x0001FFF1
mmio-publish-session: x64 now turns the denied AHCI publish write-window-open result into a blocked exclusive driver-session checkpoint: UEFI and ISO AHCI paths report session-state 3, session-flags 0x0000FFFF, open/revoke/window token binding, live hardware handles present, revocation required and planned but not executed, session-allowed 0, driver-owned 0, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable session-flags 0x0001FFF1
mmio-publish-drain: x64 now executes the AHCI publish capability-drain after the blocked exclusive-session checkpoint: UEFI and ISO AHCI paths report drain-state 3, drain-flags 0x0000FFFF, session/open/revoke/window token binding, live-before 1, revoked 1, live-after 0, revocation executed 1, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable drain-flags 0x0001FC71
mmio-publish-handoff: x64 now proves a post-drain AHCI driver handoff without hardware mutation: UEFI and ISO AHCI paths report handoff-state 3, handoff-flags 0x0001FFFF, drain-token binding, stale denial of the old hardware handle, active driver principal role 0x000000B4, a fresh block-worker-owned query-only hardware capability, live-before 0, live-after 1, write window disabled, commit denied, and zero MMIO/port/publish/command/arm side effects, while BIOS/no-AHCI media reports unavailable handoff-flags 0x0003F861
mmio-driver-probe: x64 now consumes the fresh post-drain block-worker query-only hardware capability for a driver-owned read-only AHCI readiness probe: UEFI and ISO AHCI paths report driver-probe-state 3, driver-probe-flags 0x0001FFFF, handoff-token binding, owner 0x00001006, query-only 1, controller and selected ATAPI port register reads, one-block 2048-byte read-shape metadata, and zero MMIO-write/port-program/publish/command/DMA/arm side effects, while BIOS/no-AHCI media reports unavailable driver-probe-flags 0x0003FC01
mmio-driver-intent: x64 now binds the post-handoff driver probe into a driver-owned read-intent descriptor without media access: UEFI and ISO AHCI paths report driver-intent-state 3, driver-intent-flags 0x0001FFFF, probe-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, 2048 bytes, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read side effects, while BIOS/no-AHCI media reports unavailable driver-intent-flags 0x0003FC01
mmio-driver-buffer: x64 now binds the driver read-intent token to a driver-owned zeroed read-buffer descriptor without media access: UEFI and ISO AHCI paths report driver-buffer-state 3, driver-buffer-flags 0x0001FFFF, intent-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, zeroed 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read side effects, while BIOS/no-AHCI media reports unavailable driver-buffer-flags 0x0003FC01
mmio-driver-gate: x64 now binds the driver read-buffer token into a locked read-execution gate without media access: UEFI and ISO AHCI paths report driver-gate-state 3, driver-gate-flags 0x000FFFFF, buffer-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, exec-required 1, exec-granted 0, issue-allowed 0, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read side effects, while BIOS/no-AHCI media reports unavailable driver-gate-flags 0x001FFC01
mmio-driver-exec: x64 now consumes the locked driver read-execution gate only as an explicit denied execution attempt: UEFI and ISO AHCI paths report driver-exec-state 3, driver-exec-flags 0x007FFFFF, gate-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, exec-attempted 1, exec-required 1, exec-granted 0, issue-allowed 0, issue-denied 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read side effects, while BIOS/no-AHCI media reports unavailable driver-exec-flags 0x00DFFC01
mmio-driver-result: x64 now consumes the denied driver execution token only as an explicit read-result/export denial: UEFI and ISO AHCI paths report driver-result-state 3, driver-result-flags 0x00FFFFFF, exec-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, zeroed 1, read-ready 1, granted 0, denied 1, bytes-available 0, block-cap-minted 0, fs-minted 0, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read side effects, while BIOS/no-AHCI media reports unavailable driver-result-flags 0x01FFFC01
mmio-driver-publish: x64 now consumes the denied driver result token only as an explicit block-publication denial: UEFI and ISO AHCI paths report driver-publish-state 3, driver-publish-flags 0x07FFFFFF, result-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, zeroed 1, read-ready 1, result-denied 1, bytes-available 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-publish-flags 0x0FFFFC01
mmio-drg: x64 now consumes the denied driver block-publication token only as an explicit read-authority denial: UEFI and ISO AHCI paths report drg-state 3, drg-flags 0x3FFFFFFF, publish-token and result-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, zeroed 1, read-ready 1, media-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drg-flags 0x7FFFF801
mmio-dmr: x64 now consumes the denied driver read-authority token only as an explicit media-read denial: UEFI and ISO AHCI paths report dmr-state 3, dmr-flags 0x0FFFFFFF, read-grant token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, grant-denied 1, authority 0, attempted 1, denied 1, bytes-available 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dmr-flags 0x1FFFFC01
mmio-drc: x64 now consumes the denied driver media-read token only as an explicit read-completion denial: UEFI and ISO AHCI paths report drc-state 3, drc-flags 0x1FFFFFFF, media-read token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, media-read-denied 1, requested 1, granted 0, denied 1, completed 0, status 0, bytes-available 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drc-flags 0x3FFFFC01
mmio-drcap: x64 now consumes the denied driver completion token only as an explicit read-result capability denial: UEFI and ISO AHCI paths report drcap-state 3, drcap-flags 0x0FFFFFFF, complete-token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, complete-denied 1, requested 1, granted 0, denied 1, bytes-available 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drcap-flags 0x1FFFFC01
mmio-drx: x64 now consumes the denied driver read-capability token only as an explicit caller-visible read-export denial: UEFI and ISO AHCI paths report drx-state 3, drx-flags 0x3FFFFFFF, read-cap token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-cap-denied 1, requested 1, granted 0, denied 1, bytes-available 0, user-bytes-copied 0, user-buffer-written 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drx-flags 0x7FFFFC01
mmio-drr: x64 now consumes the denied driver read-export token only as an explicit block/read response denial: UEFI and ISO AHCI paths report drr-state 3, drr-flags 0x3FFFFFFF, read-export token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-export-denied 1, requested 1, granted 0, denied 1, bytes-available 0, response-bytes 0, response-status 0, response-checksum 0x00000000, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drr-flags 0x7FFFFC01
mmio-drd: x64 now consumes the denied driver read-response token only as an explicit block-service delivery denial: UEFI and ISO AHCI paths report drd-state 3, drd-flags 0x3FFFFFFF, read-response token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-response-denied 1, requested 1, granted 0, denied 1, bytes-available 0, delivered-bytes 0, delivery-status 0, delivery-checksum 0x00000000, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drd-flags 0x7FFFFC01
mmio-drv: x64 now consumes the denied driver read-delivery token only as an explicit service/client-visible result denial: UEFI and ISO AHCI paths report drv-state 3, drv-flags 0x3FFFFFFF, read-delivery token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-delivery-denied 1, requested 1, granted 0, denied 1, bytes-available 0, visible-bytes 0, visibility-status 0, visibility-checksum 0x00000000, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drv-flags 0x7FFFFC01
mmio-drk: x64 now consumes the denied driver read-visibility token only as an explicit committed-result denial: UEFI and ISO AHCI paths report drk-state 3, drk-flags 0x3FFFFFFF, drk-drv-token binding, drk-owner 0x00001006, drk-qonly 1, selected ATAPI port, drk-op 2, drk-lba 0, drk-blocks 1, drk-read-bytes 2048, drk-page-bytes 4096, drk-checksum 0x76EFDDC5, drk-drv-denied 1, drk-requested 1, drk-granted 0, drk-denied 1, drk-bytes 0, drk-commit-bytes 0, drk-commit-status 0, drk-commit-checksum 0x00000000, drk-block-endpoint 0, drk-block-cap 0, drk-fs-minted 0, drk-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drk-flags 0x7FFFFC01
mmio-dra: x64 now consumes the denied driver read-commit token only as an explicit audit/result-finalization denial: UEFI and ISO AHCI paths report dra-state 3, dra-flags 0x3FFFFFFF, dra-drk-token binding, dra-owner 0x00001006, dra-qonly 1, selected ATAPI port, dra-op 2, dra-lba 0, dra-blocks 1, dra-read-bytes 2048, dra-page-bytes 4096, dra-checksum 0x76EFDDC5, dra-drk-denied 1, dra-requested 1, dra-granted 0, dra-denied 1, dra-bytes 0, dra-audit-bytes 0, dra-audit-status 0, dra-audit-checksum 0x00000000, dra-block-endpoint 0, dra-block-cap 0, dra-fs-minted 0, dra-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dra-flags 0x7FFFFC01
mmio-dru: x64 now consumes the denied driver read-audit token only as an explicit authority-upgrade denial: UEFI and ISO AHCI paths report dru-state 3, dru-flags 0x3FFFFFFF, dru-dra-token binding, dru-owner 0x00001006, dru-qonly 1, selected ATAPI port, dru-op 2, dru-lba 0, dru-blocks 1, dru-read-bytes 2048, dru-page-bytes 4096, dru-checksum 0x76EFDDC5, dru-dra-denied 1, dru-requested 1, dru-granted 0, dru-denied 1, dru-bytes 0, dru-up-cap 0xFFFFFFFF, dru-media-auth 0, dru-exec-auth 0, dru-block-endpoint 0, dru-block-cap 0, dru-fs-minted 0, dru-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dru-flags 0x7FFFFC01
mmio-dact: x64 now consumes the denied driver read-upgrade token only as an explicit controller-activation denial: UEFI and ISO AHCI paths report dact-state 3, dact-flags 0x3FFFFFFF, dact-dru-token binding, dact-owner 0x00001006, dact-qonly 1, selected ATAPI port, dact-op 2, dact-lba 0, dact-blocks 1, dact-read-bytes 2048, dact-page-bytes 4096, dact-checksum 0x76EFDDC5, dact-dru-denied 1, dact-requested 1, dact-granted 0, dact-denied 1, dact-bytes 0, dact-act-cap 0xFFFFFFFF, dact-read-auth 0, dact-exec-auth 0, dact-block-endpoint 0, dact-block-cap 0, dact-fs-minted 0, dact-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dact-flags 0x7FFFFC01
mmio-darm: x64 now consumes the denied driver read-activate token only as an explicit controller-arm denial: UEFI and ISO AHCI paths report darm-state 3, darm-flags 0x3FFFFFFF, darm-dact-token binding, darm-owner 0x00001006, darm-qonly 1, selected ATAPI port, darm-op 2, darm-lba 0, darm-blocks 1, darm-read-bytes 2048, darm-page-bytes 4096, darm-checksum 0x76EFDDC5, darm-dact-denied 1, darm-requested 1, darm-granted 0, darm-denied 1, darm-bytes 0, darm-arm-cap 0xFFFFFFFF, darm-read-auth 0, darm-exec-auth 0, darm-block-endpoint 0, darm-block-cap 0, darm-fs-minted 0, darm-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable darm-flags 0x7FFFFC01
mmio-dsub: x64 now consumes the denied driver read-arm token only as an explicit command-submission denial: UEFI and ISO AHCI paths report dsub-state 3, dsub-flags 0x3FFFFFFF, dsub-darm-token binding, dsub-owner 0x00001006, dsub-qonly 1, selected ATAPI port, dsub-op 2, dsub-lba 0, dsub-blocks 1, dsub-read-bytes 2048, dsub-page-bytes 4096, dsub-checksum 0x76EFDDC5, dsub-darm-denied 1, dsub-requested 1, dsub-granted 0, dsub-denied 1, dsub-bytes 0, dsub-submit-cap 0xFFFFFFFF, dsub-read-auth 0, dsub-exec-auth 0, dsub-block-endpoint 0, dsub-block-cap 0, dsub-fs-minted 0, dsub-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dsub-flags 0x7FFFFC01
mmio-dobs: x64 now consumes the denied driver read-submit token only as an explicit completion-observation denial: UEFI and ISO AHCI paths report dobs-state 3, dobs-flags 0x3FFFFFFF, dobs-dsub-token binding, dobs-owner 0x00001006, dobs-qonly 1, selected ATAPI port, dobs-op 2, dobs-lba 0, dobs-blocks 1, dobs-read-bytes 2048, dobs-page-bytes 4096, dobs-checksum 0x76EFDDC5, dobs-dsub-denied 1, dobs-requested 1, dobs-granted 0, dobs-denied 1, dobs-bytes 0, dobs-obs-status 0, dobs-obs-bytes 0, dobs-obs-checksum 0x00000000, dobs-read-auth 0, dobs-exec-auth 0, dobs-block-endpoint 0, dobs-block-cap 0, dobs-fs-minted 0, dobs-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dobs-flags 0x7FFFFC01
mmio-dret: x64 now consumes the denied driver read-observe token only as an explicit completion-retirement denial: UEFI and ISO AHCI paths report dret-state 3, dret-flags 0x3FFFFFFF, dret-dobs-token binding, dret-owner 0x00001006, dret-qonly 1, selected ATAPI port, dret-op 2, dret-lba 0, dret-blocks 1, dret-read-bytes 2048, dret-page-bytes 4096, dret-checksum 0x76EFDDC5, dret-dobs-denied 1, dret-requested 1, dret-granted 0, dret-denied 1, dret-bytes 0, dret-ret-status 0, dret-ret-bytes 0, dret-ret-checksum 0x00000000, dret-read-auth 0, dret-exec-auth 0, dret-block-endpoint 0, dret-block-cap 0, dret-fs-minted 0, dret-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dret-flags 0x7FFFFC01
mmio-dprm: x64 now consumes the denied driver read-retire token only as an explicit read-permission denial: UEFI and ISO AHCI paths report dprm-state 3, dprm-flags 0x3FFFFFFF, dprm-dret-token binding, dprm-owner 0x00001006, dprm-qonly 1, selected ATAPI port, dprm-op 2, dprm-lba 0, dprm-blocks 1, dprm-read-bytes 2048, dprm-page-bytes 4096, dprm-checksum 0x76EFDDC5, dprm-dret-denied 1, dprm-requested 1, dprm-granted 0, dprm-denied 1, dprm-bytes 0, dprm-permit-cap 0xFFFFFFFF, dprm-read-auth 0, dprm-exec-auth 0, dprm-block-endpoint 0, dprm-block-cap 0, dprm-fs-minted 0, dprm-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dprm-flags 0x7FFFFC01
mmio-dwin: x64 now consumes the denied driver read-permit token only as an explicit read-window-open denial: UEFI and ISO AHCI paths report dwin-state 3, dwin-flags 0x3FFFFFFF, dwin-dprm-token binding, dwin-owner 0x00001006, dwin-qonly 1, selected ATAPI dwin-port, dwin-op 2, dwin-lba 0, dwin-blocks 1, dwin-read-bytes 2048, dwin-page-bytes 4096, dwin-checksum 0x76EFDDC5, dwin-dprm-denied 1, dwin-requested 1, dwin-granted 0, dwin-denied 1, dwin-bytes 0, dwin-window-cap 0xFFFFFFFF, dwin-open 0, dwin-read-auth 0, dwin-exec-auth 0, dwin-block-endpoint 0, dwin-block-cap 0, dwin-fs-minted 0, dwin-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dwin-flags 0x7FFFFC01
mmio-dlse: x64 now consumes the denied driver read-window token only as an explicit reusable-read-lease denial: UEFI and ISO AHCI paths report dlse-state 3, dlse-flags 0x3FFFFFFF, dlse-dwin-token binding, dlse-owner 0x00001006, dlse-qonly 1, selected ATAPI dlse-port, dlse-op 2, dlse-lba 0, dlse-blocks 1, dlse-read-bytes 2048, dlse-page-bytes 4096, dlse-checksum 0x76EFDDC5, dlse-dwin-denied 1, dlse-requested 1, dlse-granted 0, dlse-denied 1, dlse-bytes 0, dlse-lease-cap 0xFFFFFFFF, dlse-active 0, dlse-read-auth 0, dlse-exec-auth 0, dlse-block-endpoint 0, dlse-block-cap 0, dlse-fs-minted 0, dlse-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable dlse-flags 0x7FFFFC01
mmio-duse: x64 now consumes the denied driver read-lease token only as an explicit lease-use denial: UEFI and ISO AHCI paths report duse-state 3, duse-flags 0x3FFFFFFF, duse-dlse-token binding, duse-owner 0x00001006, duse-qonly 1, selected ATAPI duse-port, duse-op 2, duse-lba 0, duse-blocks 1, duse-read-bytes 2048, duse-page-bytes 4096, duse-checksum 0x76EFDDC5, duse-dlse-denied 1, duse-requested 1, duse-granted 0, duse-denied 1, duse-bytes 0, duse-use-cap 0xFFFFFFFF, duse-active 0, duse-read-auth 0, duse-exec-auth 0, duse-block-endpoint 0, duse-block-cap 0, duse-fs-minted 0, duse-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable duse-flags 0x7FFFFC01
mmio-drpt: x64 now consumes the denied driver read-use token only as an explicit post-use report denial: UEFI and ISO AHCI paths report drpt-state 3, drpt-flags 0x3FFFFFFF, drpt-duse-token binding, drpt-owner 0x00001006, drpt-qonly 1, selected ATAPI drpt-port, drpt-op 2, drpt-lba 0, drpt-blocks 1, drpt-read-bytes 2048, drpt-page-bytes 4096, drpt-checksum 0x76EFDDC5, drpt-duse-denied 1, drpt-requested 1, drpt-granted 0, drpt-denied 1, drpt-bytes 0, drpt-status 0, drpt-report-bytes 0, drpt-report-checksum 0x00000000, drpt-report-cap 0xFFFFFFFF, drpt-read-auth 0, drpt-exec-auth 0, drpt-block-endpoint 0, drpt-block-cap 0, drpt-fs-minted 0, drpt-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drpt-flags 0x7FFFFC01
mmio-drrc: x64 now consumes the denied driver read-report token only as an explicit no-success-receipt denial: UEFI and ISO AHCI paths report drrc-state 3, drrc-flags 0x3FFFFFFF, drrc-drpt-token binding, drrc-owner 0x00001006, drrc-qonly 1, selected ATAPI drrc-port, drrc-op 2, drrc-lba 0, drrc-blocks 1, drrc-read-bytes 2048, drrc-page-bytes 4096, drrc-checksum 0x76EFDDC5, drrc-drpt-denied 1, drrc-requested 1, drrc-granted 0, drrc-denied 1, drrc-bytes 0, drrc-status 0, drrc-receipt-bytes 0, drrc-receipt-checksum 0x00000000, drrc-receipt-cap 0xFFFFFFFF, drrc-read-auth 0, drrc-exec-auth 0, drrc-block-endpoint 0, drrc-block-cap 0, drrc-fs-minted 0, drrc-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drrc-flags 0x7FFFFC01
mmio-drak: x64 now consumes the denied driver read-receipt token only as an explicit no-success-ack denial: UEFI and ISO AHCI paths report drak-state 3, drak-flags 0x3FFFFFFF, drak-drrc-token binding, drak-owner 0x00001006, drak-qonly 1, selected ATAPI drak-port, drak-op 2, drak-lba 0, drak-blocks 1, drak-read-bytes 2048, drak-page-bytes 4096, drak-checksum 0x76EFDDC5, drak-drrc-denied 1, drak-requested 1, drak-granted 0, drak-denied 1, drak-bytes 0, drak-status 0, drak-ack-bytes 0, drak-ack-checksum 0x00000000, drak-ack-cap 0xFFFFFFFF, drak-read-auth 0, drak-exec-auth 0, drak-block-endpoint 0, drak-block-cap 0, drak-fs-minted 0, drak-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drak-flags 0x7FFFFC01
mmio-drcl: x64 now consumes the denied driver read-ack token only as an explicit no-success-close denial: UEFI and ISO AHCI paths report drcl-state 3, drcl-flags 0x3FFFFFFF, drcl-drak-token binding, drcl-owner 0x00001006, drcl-qonly 1, selected ATAPI drcl-port, drcl-op 2, drcl-lba 0, drcl-blocks 1, drcl-read-bytes 2048, drcl-page-bytes 4096, drcl-checksum 0x76EFDDC5, drcl-drak-denied 1, drcl-requested 1, drcl-granted 0, drcl-denied 1, drcl-bytes 0, drcl-status 0, drcl-close-bytes 0, drcl-close-checksum 0x00000000, drcl-close-cap 0xFFFFFFFF, drcl-read-auth 0, drcl-exec-auth 0, drcl-block-endpoint 0, drcl-block-cap 0, drcl-fs-minted 0, drcl-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drcl-flags 0x7FFFFC01
mmio-drsl: x64 now consumes the denied driver read-close token only as an explicit no-success-seal denial: UEFI and ISO AHCI paths report drsl-state 3, drsl-flags 0x3FFFFFFF, drsl-drcl-token binding, drsl-owner 0x00001006, drsl-qonly 1, selected ATAPI drsl-port, drsl-op 2, drsl-lba 0, drsl-blocks 1, drsl-read-bytes 2048, drsl-page-bytes 4096, drsl-checksum 0x76EFDDC5, drsl-drcl-denied 1, drsl-requested 1, drsl-granted 0, drsl-denied 1, drsl-bytes 0, drsl-status 0, drsl-seal-bytes 0, drsl-seal-checksum 0x00000000, drsl-seal-cap 0xFFFFFFFF, drsl-read-auth 0, drsl-exec-auth 0, drsl-block-endpoint 0, drsl-block-cap 0, drsl-fs-minted 0, drsl-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drsl-flags 0x7FFFFC01
mmio-drul: x64 now consumes the denied driver read-seal token only as an explicit no-success-unseal denial: UEFI and ISO AHCI paths report drul-state 3, drul-flags 0x3FFFFFFF, drul-drsl-token binding, drul-owner 0x00001006, drul-qonly 1, selected ATAPI drul-port, drul-op 2, drul-lba 0, drul-blocks 1, drul-read-bytes 2048, drul-page-bytes 4096, drul-checksum 0x76EFDDC5, drul-drsl-denied 1, drul-requested 1, drul-granted 0, drul-denied 1, drul-bytes 0, drul-status 0, drul-unseal-bytes 0, drul-unseal-checksum 0x00000000, drul-unseal-cap 0xFFFFFFFF, drul-read-auth 0, drul-exec-auth 0, drul-block-endpoint 0, drul-block-cap 0, drul-fs-minted 0, drul-buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drul-flags 0x7FFFFC01
mmio-drdc: x64 now consumes the denied driver read-unseal token only as an explicit no-success-discard denial: UEFI and ISO AHCI paths report drdc-state 3, drdc-flags 0x3FFFFFFF, read-unseal token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-unseal-denied 1, requested 1, granted 0, denied 1, bytes-available 0, discard-status 0, discarded-bytes 0, discard-checksum 0x00000000, discard-cap 0xFFFFFFFF, read-authority 0, execute-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drdc-flags 0x7FFFFC01
mmio-driver-read-finalize: x64 now consumes the denied driver read-discard token only as an explicit no-success-finalize denial: UEFI and ISO AHCI paths report driver-read-finalize-state 3, driver-read-finalize-flags 0x3FFFFFFF, read-discard token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-discard-denied 1, requested 1, granted 0, denied 1, bytes-available 0, finalize-status 0, finalized-bytes 0, finalize-checksum 0x00000000, finalize-cap 0xFFFFFFFF, read-authority 0, execute-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-finalize-flags 0x7FFFFC01
mmio-driver-read-authorize: x64 now consumes the denied driver read-finalize token only as an explicit policy authorization denial: UEFI and ISO AHCI paths report driver-read-authorize-state 3, driver-read-authorize-flags 0x3FFFFFFF, read-finalize token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-finalize-denied 1, requested 1, granted 0, denied 1, policy-grant 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-authorize-flags 0x7FFFFC01
mmio-driver-read-dispatch: x64 now consumes the denied driver read-authorize token only as an explicit driver dispatch/queue denial: UEFI and ISO AHCI paths report driver-read-dispatch-state 3, driver-read-dispatch-flags 0x3FFFFFFF, read-authorize token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-authorize-denied 1, requested 1, granted 0, denied 1, policy-grant 0, dispatch-queued 0, queue-depth 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-dispatch-flags 0x7FFFFC01
mmio-driver-read-queue: x64 now consumes the denied driver read-dispatch token only as an explicit driver queue-admission denial: UEFI and ISO AHCI paths report driver-read-queue-state 3, driver-read-queue-flags 0x3FFFFFFF, read-dispatch token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-dispatch-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-queue-flags 0x7FFFFC01
mmio-driver-read-worker: x64 now consumes the denied driver read-queue token only as an explicit block-worker admission denial: UEFI and ISO AHCI paths report driver-read-worker-state 3, driver-read-worker-flags 0x3FFFFFFF, read-queue token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-queue-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, worker-dequeued 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-worker-flags 0x7FFFFC01
mmio-driver-read-schedule: x64 now consumes the denied driver read-worker token only as an explicit scheduler/run-queue admission denial: UEFI and ISO AHCI paths report driver-read-schedule-state 3, driver-read-schedule-flags 0x3FFFFFFF, read-worker token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-worker-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, worker-dequeued 0, worker-runnable 0, worker-scheduled 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-schedule-flags 0x7FFFFC01
mmio-driver-read-run: x64 now consumes the denied driver read-schedule token only as an explicit block-worker run-entry denial: UEFI and ISO AHCI paths report driver-read-run-state 3, driver-read-run-flags 0x3FFFFFFF, read-schedule token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-schedule-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, worker-dequeued 0, worker-runnable 0, worker-scheduled 0, worker-run 0, worker-executed 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-run-flags 0x7FFFFC01
mmio-driver-read-body: x64 now consumes the denied driver read-run token only as an explicit block-worker body-entry denial: UEFI and ISO AHCI paths report driver-read-body-state 3, driver-read-body-flags 0x3FFFFFFF, read-run token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-run-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, worker-dequeued 0, worker-runnable 0, worker-scheduled 0, worker-run 0, worker-executed 0, body-entered 0, body-completed 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-body-flags 0x7FFFFC01
mmio-driver-read-issue: x64 now consumes the denied driver read-body token only as an explicit command-issue entry denial: UEFI and ISO AHCI paths report driver-read-issue-state 3, driver-read-issue-flags 0x3FFFFFFF, read-body token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-body-denied 1, requested 1, granted 0, denied 1, policy-grant 0, queue-inserted 0, queue-depth 0, worker-wake 0, worker-dequeued 0, worker-runnable 0, worker-scheduled 0, worker-run 0, worker-executed 0, issue-entered 0, issue-completed 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-issue-flags 0x7FFFFC01
mmio-driver-read-dma: x64 now consumes the denied driver read-issue token only as an explicit DMA-window denial: UEFI and ISO AHCI paths report driver-read-dma-state 3, driver-read-dma-flags 0x3FFFFFFF, read-issue token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-issue-denied 1, requested 1, granted 0, denied 1, policy-grant 0, bytes-available 0, window-cap 0xFFFFFFFF, window-open 0, dma-entered 0, dma-completed 0, issue-authority 0, dma-authority 0, media-read-authority 0, write-authority 0, commit-authority 0, block-endpoint 0, block-cap-minted 0, fs-minted 0, buffer-unchanged 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable driver-read-dma-flags 0x7FFFFC01
mmio-driver-read-irq: x64 now consumes the denied driver read-DMA token only as an explicit IRQ/completion observation denial using compact drs-irq telemetry: UEFI and ISO AHCI paths report drs-irq-state 3, drs-irq-flags 0x3FFFFFFF, read-DMA token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-DMA-denied 1, requested 1, granted 0, denied 1, policy-grant 0, bytes 0, irq-wait 0, irq-fired 0, result status/bytes/checksum all zero, issue-auth 0, dma-auth 0, read-auth 0, write-auth 0, commit-auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-irq-flags 0x7FFFFC01
mmio-driver-read-status: x64 now consumes the denied driver read-IRQ token only as an explicit status/completion polling denial using compact drs-status telemetry: UEFI and ISO AHCI paths report drs-status-state 3, drs-status-flags 0x3FFFFFFF, read-IRQ token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, read-IRQ-denied 1, requested 1, granted 0, denied 1, policy-grant 0, bytes 0, poll 0, status-ready 0, pxis/ci/tfd/serr all 0, irq-clear 0, result status/bytes/checksum all zero, issue-auth 0, dma-auth 0, read-auth 0, write-auth 0, commit-auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-status-flags 0x7FFFFC01
mmio-driver-read-status-result: x64 now consumes the denied driver read-status token only as an explicit read-result denial using compact drs-result telemetry: UEFI and ISO AHCI paths report drs-result-state 3, drs-result-flags 0x3FFFFFFF, read-status token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, status-denied 1, requested 1, granted 0, denied 1, policy-grant 0, bytes 0, result-status 0, result-bytes 0, result-checksum 0x00000000, issue-auth 0, dma-auth 0, read-auth 0, write-auth 0, commit-auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-result-flags 0x7FFFFC01
mmio-driver-read-status-sample: x64 now consumes the denied driver read-status-result token through a driver-owned query-only capability to perform a read-only AHCI port status sample using compact drs-sample telemetry: UEFI and ISO AHCI paths report drs-sample-state 3, drs-sample-flags 0x3FFFFFFF, read-status-result token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, result-denied 1, requested 1, granted 0, denied 1, policy-grant 0, bytes 0, hardware-backed PxIS/PxCI/PxTFD/PxSERR samples, tfd-ready 1, ci-idle 1, serr-clear 1, irq-clear 0, result-status 0, result-bytes 0, result-checksum 0x00000000, issue-auth 0, dma-auth 0, read-auth 0, write-auth 0, commit-auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-sample-flags 0x7FF00C01
mmio-driver-read-status-clear: x64 now consumes the read-only status-sample token through the driver-owned query-only capability to prove AHCI PxIS clear remains denied using compact drs-clear telemetry: UEFI and ISO AHCI paths report drs-clear-state 3, drs-clear-flags 0x3FFFFFFF, sample token binding, owner 0x00001006, query-only 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, sample-ready 1, sample-bound 1, requested 1, granted 0, denied 1, policy-grant 0, PxIS before/after 0x00000003 unchanged 1, CI 0x00000000, TFD 0x00000050, SERR 0x00000000, tfd-ready 1, ci-idle 1, serr-clear 1, clear-requested 1, clear-granted 0, clear-denied 1, clear-value 0x00000000, irq-clear 0, result-status 0, result-bytes 0, result-checksum 0x00000000, issue-auth 0, dma-auth 0, read-auth 0, write-auth 0, commit-auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-clear-flags 0x7FF03801
mmio-drs-guard: x64 now consumes the stable read-status token only as a guarded future-read shape using compact drs-guard telemetry: UEFI and ISO AHCI paths report drs-guard-state 3, drs-guard-flags 0x3FFFFFFF, stable-token binding, owner 0x00001006, qonly 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, stable PxIS/CI/TFD/SERR preconditions, issue-denied 1, dma-denied 1, read-denied 1, write-denied 1, commit-denied 1, result-status 0, result-bytes 0, result-checksum 0x00000000, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-guard-flags 0x7FFC0001
mmio-drs-buffer: x64 now consumes the read-status guard token only as a sealed no-result buffer view using compact drs-buffer telemetry: UEFI and ISO AHCI paths report drs-buffer-state 3, drs-buffer-flags 0x3FFFFFFF, guard-token binding, owner 0x00001006, qonly 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, page-bytes 4096, checksum 0x76EFDDC5, view-requested 1, view-granted 0, view-denied 1, result-status 0, result-bytes 0, result-checksum 0x00000000, read/write/commit auth 0, block-endpoint 0, block-cap 0, fs-minted 0, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-buffer-flags 0x7FFFF801
mmio-drs-export: x64 now consumes the sealed read-status buffer token only as an explicit caller-visible export denial using compact drs-export telemetry: UEFI and ISO AHCI paths report drs-export-state 3, drs-export-flags 0x3FFFFFFF, status-buffer token binding, owner 0x00001006, qonly 1, selected ATAPI port, operation 2, LBA 0, one block, read-bytes 2048, checksum 0x76EFDDC5, sealed 1, requested 1, granted 0, denied 1, user-copy 0x00000000, authority 0x00000000, effects 0x00000000, and buffer 1, while BIOS/no-AHCI media reports unavailable drs-export-flags 0x7FFFF001
mmio-drs-report: x64 now consumes the denied read-status export token only as a broker/report-facing no-result proof using compact drs-report telemetry: UEFI and ISO AHCI paths report drs-report-state 3, drs-report-flags 0x3FFFFFFF, owner 0x00001006, qonly 1, checksum 0x76EFDDC5, export-denied 1, report/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-report-flags 0x7FFFFE01
mmio-drs-receipt: x64 now consumes the denied read-status report token only as an explicit no-success receipt proof using compact drs-receipt telemetry: UEFI and ISO AHCI paths report drs-receipt-state 3, drs-receipt-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, report-denied 1, receipt/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-receipt-flags 0x7FFFFF01
mmio-drs-ack: x64 now consumes the denied read-status receipt token only as an explicit no-success acknowledgment proof using compact drs-ack telemetry: UEFI and ISO AHCI paths report drs-ack-state 3, drs-ack-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, receipt-denied 1, ack/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-ack-flags 0x7FFFFF01
mmio-drs-close: x64 now consumes the denied read-status ack token only as an explicit no-success close proof using compact drs-close telemetry: UEFI and ISO AHCI paths report drs-close-state 3, drs-close-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, ack-denied 1, close/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-close-flags 0x7FFFFF01
mmio-drs-seal: x64 now consumes the denied read-status close token only as an explicit no-success seal proof using compact drs-seal telemetry: UEFI and ISO AHCI paths report drs-seal-state 3, drs-seal-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, close-denied 1, seal/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-seal-flags 0x7FFFFF01
mmio-driver-read-status-unseal: x64 now consumes the denied read-status seal token only as an explicit no-success unseal proof using compact drs-unseal telemetry: UEFI and ISO AHCI paths report drs-unseal-state 3, drs-unseal-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, seal-denied 1, unseal/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-unseal-flags 0x7FFFFF01
mmio-driver-read-status-discard: x64 now consumes the denied read-status unseal token only as an explicit no-success discard proof using compact drs-discard telemetry: UEFI and ISO AHCI paths report drs-discard-state 3, drs-discard-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, unseal-denied 1, discard/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-discard-flags 0x7FFFFF01
mmio-driver-read-status-finalize: x64 now consumes the denied read-status discard token only as an explicit no-success finalize proof using compact drs-final telemetry: UEFI and ISO AHCI paths report drs-final-state 3, drs-final-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, discard-denied 1, finish/user-copy/authority/effects all 0x00000000, buffer 1, and zero MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-final-flags 0x7FFFFF01
mmio-driver-read-status-authorize: x64 now consumes the denied read-status finalize token only as an explicit no-authority authorization proof: UEFI and ISO AHCI paths report drs-authz-state 3, drs-authz-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, final-denied 1, grant/user-copy/authority/effects all 0x00000000, buffer 1, and zero policy-grant/issue-authority/DMA-authority/media-read-authority/MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-authz-flags 0x7FFFFF01
mmio-driver-read-status-dispatch: x64 now consumes the denied read-status authorize token only as an explicit no-dispatch proof: UEFI and ISO AHCI paths report drs-dispatch-state 3, drs-dispatch-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, drs-dispatch-authz-denied 1, drs-dispatch-safety 0x00000000, drs-dispatch-buffer 1, and zero dispatch-handle/queue-entry/worker-admit/schedule/issue-authority/DMA-authority/media-read-authority/MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-dispatch-flags 0x7FFFFF01
mmio-driver-read-status-queue: x64 now consumes the denied read-status dispatch token only as an explicit queue-closure proof: UEFI and ISO AHCI paths report drs-queue-state 3, drs-queue-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, drs-queue-dispatch-denied 1, drs-queue-safety 0x00000000, queue depth/admit/worker/runnable/schedule all 0, buffer-unchanged 1, and zero queue-ticket/queue-entry/worker-wake/worker-runnable/issue-authority/DMA-authority/media-read-authority/write-authority/commit-authority/MMIO-write/port-program/publish/command/DMA/arm/media-read/media-write side effects, while BIOS/no-AHCI media reports unavailable drs-queue-flags 0x7FFFFF01
mmio-driver-read-status-worker: x64 now consumes the denied read-status queue token only as an explicit worker-admission denial proof: UEFI and ISO AHCI paths report drs-w-state 3, drs-w-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, drs-w-queue-denied 1, drs-w-safety 0x00000000, drs-w-dequeue/admit/wake/runnable/sched/run/exec all 0, drs-w-buffer 1, and zero queue-dequeue/worker-admit/worker-wake/worker-runnable/worker-schedule/worker-run/worker-execute/issue-authority/DMA-authority/media-read-authority/write-authority/commit-authority/MMIO-write/port-program/DMA/media-read side effects, while BIOS/no-AHCI media reports unavailable drs-w-flags 0x7FFFFF01
mmio-driver-read-status-read-authority: x64 now consumes the denied read-status worker token as the first positive sealed read-authority primitive while keeping execution closed: UEFI and ISO AHCI paths report drs-rauth-state 3, drs-rauth-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, drs-rauth-worker-denied 1, drs-rauth-policy 1, drs-rauth-read 1, drs-rauth-issue 0, drs-rauth-dma-auth 0, drs-rauth-media-auth 0, drs-rauth-write 0, drs-rauth-commit 0, drs-rauth-block-endpoint 0, drs-rauth-block-cap 0, drs-rauth-fs-minted 0, drs-rauth-safety 0x00000000, drs-rauth-dequeue/admit/wake/runnable/sched/run/exec all 0, drs-rauth-buffer 1, and zero queue dequeue, worker execution, issue/DMA/media-read/write/commit, block publication, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write side effects, while BIOS/no-AHCI media reports unavailable drs-rauth-flags 0x7FFFF901
mmio-driver-read-status-descriptor: x64 now consumes the sealed drs-rauth token into the first driver-owned non-executing AHCI/ATAPI read descriptor shape: UEFI and ISO AHCI paths report drs-desc-state 3, drs-desc-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, read-authority bound 1, descriptor-shaped 1, read-descriptor 1, ATAPI port/kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, slot 0, header 32, table 144, CFIS 20, PRDT 1/16 bytes, ATAPI packet 12, command opcode 0xA0, packet opcode 0x28, transfer 2048, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, queue dequeue, worker execution, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-desc-flags 0x7FFFF901
mmio-driver-read-status-command-table: x64 now consumes the sealed drs-desc token into a driver-owned AHCI/ATAPI command-table materialization proof without issuing it: UEFI and ISO AHCI paths report drs-ctab-state 3, drs-ctab-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, descriptor-bound 1, table-materialized 1, table-ready 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, checksum transition 0x76EFDDC5 -> 0x3FBFAF45, header flags 0x00010025, CFIS command 0xA0, packet opcode 0x28, PRDT DBC 2047, table-written 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-ctab-flags 0x7FFFF901
mmio-driver-read-status-command-issue: x64 now consumes the sealed drs-ctab token into a driver-owned AHCI/ATAPI command-issue boundary proof while still denying issue authority: UEFI and ISO AHCI paths report drs-issue-state 3, drs-issue-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, command-table-bound 1, issue-ready 1, issue-request 1, issue-grant 0, issue-denied 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, CI 0, slot-mask 1, slot-idle 1, TFD/SERR ready, table checksum 0x3FBFAF45, checksum-match 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-issue-flags 0x7FFFF901
mmio-driver-read-status-issue-grant: x64 now consumes the sealed drs-issue token into a separate driver-owned issue-grant policy proof while still denying issue authority: UEFI and ISO AHCI paths report drs-grant-state 3, drs-grant-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, command-issue-bound 1, grant-ready 1, grant-request 1, grant-grant 0, grant-denied 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, CI 0, slot-mask 1, slot-idle 1, TFD/SERR ready, table checksum 0x3FBFAF45, checksum-match 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-grant-flags 0x7FFFF901
mmio-driver-read-status-arm: x64 now consumes the sealed drs-grant token into a driver-owned arm/doorbell preflight proof while still denying arm and issue authority: UEFI and ISO AHCI paths report drs-arm-state 3, drs-arm-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, issue-grant-bound 1, arm-ready 1, arm-request 1, arm-grant 0, arm-denied 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, CI 0, slot-mask 1, slot-idle 1, TFD/SERR ready, table checksum 0x3FBFAF45, checksum-match 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-arm-flags 0x7FFFF901
mmio-driver-read-status-exec: x64 now consumes the sealed drs-arm token into a driver-owned execution/doorbell preflight proof while still denying execution and preserving zero unsafe side effects: UEFI and ISO AHCI paths report drs-exec-state 3, drs-exec-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, arm-bound 1, exec-ready 1, exec-request 1, exec-grant 0, exec-denied 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, CI 0, slot-mask 1, slot-idle 1, TFD/SERR ready, table checksum 0x3FBFAF45, checksum-match 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-exec-flags 0x7FFFF901
mmio-driver-read-status-dma: x64 now consumes the sealed drs-exec token into a driver-owned DMA-window preflight proof while still denying DMA mapping and preserving zero unsafe side effects: UEFI and ISO AHCI paths report drs-dma-state 3, drs-dma-flags 0x3FFFFFFF, owner 0x00001006, query-only 1, checksum 0x76EFDDC5, exec-bound 1, dma-ready 1, dma-request 1, dma-grant 0, dma-denied 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, CI 0, slot-mask 1, slot-idle 1, TFD/SERR ready, table checksum 0x3FBFAF45, checksum-match 1, and zero issue/DMA/media-read/write/commit, block endpoint/capability, filesystem mint, MMIO-write, port-program, command, DMA, media-read, or media-write authority, while BIOS/no-AHCI media reports unavailable drs-dma-flags 0x7FFFF901
mmio-driver-read-status-mmio: x64 now consumes the denied drs-dma token into the first broker-owned AHCI MMIO side-effect proof without command issue or DMA. UEFI and ISO AHCI paths report drs-mmio-state 3, drs-mmio-flags 0x3FFFFFFF, owner 0x00001006, query-only binding, checksum 0x76EFDDC5, DMA-token binding, exact selected-port PxIS register offset (0x00000210 for UEFI removable media and 0x00000390 for ISO in QEMU), exact write value 0x00000000, PxIS before/after 0x00000003 unchanged, rollback-required 0, teardown 1, stale-token denial 1, MMIO-write 1, and zero issue authority, DMA authority, media-read authority, write/commit authority, block endpoint/capability, filesystem mint, port-program, command issue, DMA, media-read, or media-write side effects; BIOS/no-AHCI media reports unavailable drs-mmio-flags 0x7FF00101
mmio-driver-read-status-dwin: x64 now consumes the drs-mmio token into the first broker-owned single-page DMA-window lifetime proof without command issue, device DMA execution, or media-byte exposure. UEFI and ISO AHCI paths report drs-dwin-state 3, drs-dwin-flags 0x3FFFFFFF, owner 0x00001006, query-only binding, checksum 0x76EFDDC5, MMIO-token binding, selected ATAPI port, kind/op 2, LBA 0, blocks 1, read-bytes 2048, page-bytes 4096, table checksum 0x3FBFAF45, checksum-match 1, page/bounce physical addresses, bounce offset 2048, range-end 4096, single-page 1, broker-owned 1, bounds-enforced 1, confined 1, below4g 1, non-user 1, alias-safe 1, opened 1, closed 1, revoke-required 1, revoke-done 1, stale-denied 1, active 0, and zero issue/DMA-execution/media-read/write/commit authority, block endpoint/capability, filesystem mint, MMIO-write, port-program, command issue, DMA, media-read, or media-write side effects; BIOS/no-AHCI media reports unavailable drs-dwin-flags 0x7FC80001
mmio-driver-read-status-read: x64 now consumes drs-dwin into the first positive bounded hardware-backed AHCI/ATAPI read while preserving broker-private sealing. UEFI and ISO AHCI paths report drs-read-state 3, drs-read-flags 0x007FFFFF, owner 0x00001006, query-only binding, dwin-bound 1, selected ATAPI port, kind/op 2, LBA 0, blocks 1, table checksum transition 0x14EC2F71 -> 0xD8BD95B6, issued 1, completed 1, bytes 2048, nonzero checksum, error 0, PRDBC 2048, CI idle before/after, bounded polls, MMIO/port/DMA/media telemetry 1, and zero write/commit authority, block endpoint/capability, filesystem mint, active lease, or media write; BIOS/no-AHCI media reports unavailable drs-read-flags 0x801F0001
mmio-driver-read-status-block: x64 now consumes drs-read into a read-only AHCI-backed block route while preserving write closure, consumes that block capability into the first broker-private ISO9660 read, delegates one scoped read-only filesystem capability to ring3 through drs-fs-user, and keeps one persistent drs-fs-shell delegation for a dynamic disk-sourced /APPS descriptor scan. UEFI and ISO AHCI paths report drs-block-state 3, drs-block-flags 0x000FFFFF, owner 0x00001006, read routed 1, bytes 2048, checksum equal to drs-read, wrong-owner and stale-handle denial, plus drs-fs-state 3, PVD CD001 validation, README.TXT location/read, checksum 0x4ABDFAAA, drs-fs-user path /APPS/LS.APP, bytes 79, checksum 0xFDB1F751, delegated 1, wrong-owner 1, stale 1, drs-fs-shell descriptors-read 11, scan-dynamic 1, ls/cat/stat dispatch 1, and zero write authority, commit authority, additional filesystem mint, format authority, reusable lease, or media write; BIOS/no-AHCI media reports unavailable drs-block-flags 0x800E0001, drs-fs-flags 0x8001C001, drs-fs-user-flags 0x801C0001, and drs-fs-shell-flags 0x8000E001
input-keyboard: x64 brokered keyboard input now has a 256-byte pending queue and bounded multi-scancode PS/2 controller drains so future live-shell verification has headroom beyond the compact scripted command stream while still reporting drops 0, QMP verification sends split press/release events with an 80 ms hold, 180 ms per-key pacing, 1300 ms post-line settling, and a 52000 ms probe window on disk media, while UEFI and ISO media use 210 ms per-key pacing, 1600 ms post-line settling, and a 56000 ms probe window so the compact x64 scaffold keeps command lines distinct without granting ambient input authority
interrupts: IDT online with proof vector 0x30, syscall vector 0x80, PIT IRQ wakeups, and IRQ1 keyboard dispatch
syscall: scaffold int 0x80 query ABI, brokered filesystem, console, input, and display syscall surfaces, and native syscall entry online
faults: breakpoint, invalid-op, and page-fault proof plus last-fault telemetry online
optical-media: IMAPI2-packaged UEFI ISO generated from the verified removable-media boot image and verified under OVMF QEMU via BootMedia iso
uefi-graphics: UEFI app locates GOP through firmware boot services, reports framebuffer mode/geometry/base/size, draws a bounded firmware pixel pattern, reads the pixels back into a nonzero token, and requires that proof for both removable UEFI image and UEFI ISO verification
uefi-framebuffer-handoff: UEFI records GOP framebuffer base/size/geometry in boot-info, maps it through a dedicated 0xB000 page-directory in the boot handoff, sets the framebuffer boot flag so UEFI boots report flags 0x3F, the x64 kernel draws/logs a kernel-owned marker before runtime mappings begin, ring3 userspace can draw a bounded marker plus a clearable text panel only through delegated display authority, and successful ring3 console writes are mirrored by the console service into a bounded line-cleared scrolling framebuffer viewport with dedicated console-mirror, line-clear, wrap, and scroll telemetry
uefi-media-read: UEFI app resolves its loaded-image device, opens the boot volume through Simple File System, reads the staged root README.TXT, and verifies 66 bytes with prefix proof and checksum 0xDAF085B1 on both removable UEFI image and UEFI ISO verification
uefi-loader-manifest: UEFI app reads BOOTMAN.TXT from the same boot volume, parses the declared KERNEL64.BIN byte count and FNV-1a checksum, and requires manifest-valid telemetry before payload verification
uefi-loader-payload: UEFI app loads KERNEL64.BIN from boot media into an aligned 2 MiB handoff buffer and verifies $($uefiKernelBytes.Length) bytes with checksum $kernelChecksumHex before reporting loader match 1 on both removable UEFI image and UEFI ISO verification, preserving verified loader headroom for upcoming x64 storage checkpoints without adding disk, MMIO, DMA, or filesystem authority
uefi-kernel-placement: UEFI app selects a 2 MiB-aligned address inside the largest conventional firmware region, allocates exact EfiLoaderData pages with AllocatePages, copies the verified kernel payload into that allocation, zeroes page padding, rechecks the copied bytes before reporting placement match 1, and then separately proves the linked x64 scaffold can be allocated and copied at physical 0x10000 with entry 0xFFFFFFFF80010000, boot-info 0x9000, and page-root 0x1000 recorded for the guarded kernel-entry handoff
uefi-memory-map: UEFI app captures the firmware memory map after payload load and again after kernel placement, reporting descriptor count, descriptor size, map key, page totals, conventional/loader/boot/runtime page classes, and largest conventional region before taking the final silent map key used by ExitBootServices
uefi-boot-handoff: UEFI app allocates low handoff pages at 0x1000, builds the 16 MiB identity/high-half page-table substrate plus framebuffer page-directory at 0xB000, reserves 0xC000 for the broker-installed kernel MMIO page table, writes boot-info at 0x9000, copies a trampoline at 0xA000, and reports ready 1 with jump-ready 1 before leaving firmware
uefi-exit-boot-services: UEFI app captures one final silent memory map after all console output, calls ExitBootServices with that fresh map key, emits only direct serial/debug output after success, reports firmware-offline 1 and handoff-ready 1, emits a kernel-entry guard proving linked-base match 1 plus jump-ready 1, jumps through the handoff trampoline into the x64 kernel, reloads the kernel descriptor state, draws/logs the kernel-owned framebuffer marker, and reaches the compact bootstrap, second-page filesystem/display, real-media storage, and disk-sourced launch proofs before bootstrap halt
uefi-app: $uefiArtifactReport
uefi-image: $uefiImageReport (verified under OVMF QEMU via BootMedia uefi with GOP framebuffer, boot-info framebuffer mapping, kernel framebuffer draw, brokered display marker/panel/text proof, line-cleared scrolling brokered console-to-framebuffer mirror proof, brokered PS/2 plus xHCI HID keyboard event telemetry, explicit keyboard-read syscall proof, compact sealed bootstrap proof, real-media storage proofs, disk-sourced descriptor/binary launch proofs, boot-media file-read, loader buffer, kernel-placement, linked-base placement, boot-handoff, kernel-jump, and x64 userspace proofs)
uefi-stage: $uefiStageReport
uefi-iso: $isoReport (verified under OVMF QEMU via BootMedia iso with GOP framebuffer, boot-info framebuffer mapping, kernel framebuffer draw, brokered display marker/panel/text proof, line-cleared scrolling brokered console-to-framebuffer mirror proof, brokered PS/2 plus xHCI HID keyboard event telemetry, explicit keyboard-read syscall proof, compact sealed bootstrap proof, real-media storage proofs, disk-sourced descriptor/binary launch proofs, boot-media file-read, loader buffer, kernel-placement, linked-base placement, boot-handoff, kernel-jump, and x64 userspace proofs)
compat32: planned userspace compatibility lane
artifact-image: $imageReport
artifact-iso: $isoReport
artifact-pe: $artifactPeReport
artifact-bin: $artifactBinReport
artifact-uefi-pe: $uefiArtifactPeReport
artifact-uefi-bin: $uefiArtifactBinReport
artifact-size-map: $sizeReportPathReport
"@
    Set-Content -Path $reportPath -Value $report -Encoding Ascii

    Write-Host ""
    Write-Host "Build complete"
    Write-Host "  architecture : x86_64"
    Write-Host "  build kind   : bios long-mode scaffold"
    Write-Host "  bios kernel  : $($kernelBytes.Length) bytes"
    Write-Host "  uefi kernel  : $($uefiKernelBytes.Length) bytes"
    Write-Host "  bios sectors : $sectorCount / $loaderSectorLimit sectors ($loaderSectorReserve reserve)"
    Write-Host "  uefi budget  : $($uefiKernelBytes.Length) / $uefiKernelByteLimit bytes ($($uefiKernelByteLimit - $uefiKernelBytes.Length) reserve)"
    Write-Host "  section map  : text $($kernelSizeMap.Text), rodata $($kernelSizeMap.Rodata), data $($kernelSizeMap.Data), bss $($kernelSizeMap.Bss)"
    Write-Host "  top object   : $($topSizeObject.Name) ($($topSizeObject.Total) bytes)"
    Write-Host "  uefi section : text $($uefiKernelSizeMap.Text), rodata $($uefiKernelSizeMap.Rodata), data $($uefiKernelSizeMap.Data), bss $($uefiKernelSizeMap.Bss)"
    Write-Host "  uefi top obj : $($uefiTopSizeObject.Name) ($($uefiTopSizeObject.Total) bytes)"
    Write-Host "  image        : $imagePath"
    Write-Host "  iso          : $isoPath"
    Write-Host "  uefi app     : $uefiArtifact"
    Write-Host "  uefi image   : $uefiImage"
    Write-Host "  uefi stage   : $uefiStageDir"
    Write-Host "  uefi manifest: $uefiManifestPath"
    Write-Host "  bios payload : $kernelOut ($($kernelBytes.Length) bytes)"
    Write-Host "  uefi payload : $uefiKernelPath ($($uefiKernelBytes.Length) bytes checksum $kernelChecksumHex)"
    Write-Host "  artifact pe  : $artifactPe"
    Write-Host "  artifact bin : $artifactBin"
    Write-Host "  uefi pe      : $uefiArtifactPe"
    Write-Host "  uefi bin     : $uefiArtifactBin"
    Write-Host "  size map     : $sizeReportPath"
    Write-Host "  report       : $reportPath"
}

if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}

New-Item -ItemType Directory -Force $buildDir, $distDir, $generatedDir | Out-Null

if ($Architecture -eq "x86_64") {
    Write-Host "Generating x86_64 NVMe GPT fixture image"
    & $nvmeImageGenerator -OutputPath $nvmeGptImagePath
    if (-not $?) {
        throw "Failed to generate x86_64 NVMe GPT fixture image."
    }

    Write-Host "Generating x86_64 sealed runtime image"
    & $runtimeImageGenerator -InputAsmPath $runtimeImageAsm -OutputBinPath $runtimeImageBin -OutputHeaderPath $runtimeImageHeader
    if (-not $?) {
        throw "Failed to generate x86_64 sealed runtime image."
    }

    Write-Host "Assembling x86_64 disk-sourced LS flat binary"
    & nasm -f bin $diskLsImageAsm -o $diskLsImageBin
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to assemble x86_64 disk-sourced LS flat binary."
    }

    foreach ($spec in $diskFlatBinarySpecs) {
        Write-Host "Assembling x86_64 disk-sourced $($spec.Name) flat binary"
        & nasm -f bin "-D$($spec.Define)" $diskUtilityImageAsm -o $spec.Bin
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to assemble x86_64 disk-sourced $($spec.Name) flat binary."
        }
    }
}

Write-Host "Generating bootstrap package archive"
$packageStoreArgs = @{
    InputPath = $packageStoreSpec
    OutputPath = $packageStoreHeader
}

if ($Architecture -eq "x86_64") {
    $packageStoreArgs.PayloadImagePath = $runtimeImageBin
    $packageStoreArgs.PayloadSlot = 1
    $packageStoreArgs.FlatBinaryImagePath = @($diskLsImageBin) + @($diskFlatBinarySpecs | ForEach-Object { $_.Bin })
    $packageStoreArgs.FlatBinaryPayloadSlot = @([uint32]2) + @($diskFlatBinarySpecs | ForEach-Object { [uint32]$_.Slot })
    $packageStoreArgs.OutputSignaturePath = $packageStoreSignatureHeader
}

& $packageStoreGenerator @packageStoreArgs
if (-not $?) {
    throw "Failed to generate bootstrap package archive."
}

Write-Host "Generating architecture build header"
Write-ArchBuildHeader -OutputPath $archBuildHeader -TargetArchitecture $Architecture -TargetBuildProfile $BuildProfile

if ($Architecture -eq "x86") {
    Build-X86
}
else {
    Build-X64Scaffold
    Write-Host "Running M1 production-slice gate"
    & $m1ProductionGate -Architecture x86_64 -BuildProfile $BuildProfile -WriteInventory
    if (-not $?) {
        throw "M1 production-slice gate failed."
    }
}





