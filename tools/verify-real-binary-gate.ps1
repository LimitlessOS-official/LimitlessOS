param(
    [Parameter(Mandatory = $true)]
    [string]$BusyBoxPath,

    [string]$BusyBoxSource = "",
    [string]$BusyBoxVersion = "",
    [string]$LowAddressBusyBoxPath = "",

    [switch]$RequireLowAddressNegative,
    [switch]$RequireShellApplets,
    [switch]$RequireShellCwdLoop,
    [switch]$RequireRelativePathProof,
    [switch]$RequireProcSymlinkProof,
    [switch]$RequireProcFdProof,
    [switch]$RequireProcSelfProof,
    [switch]$TraceShellForkBoundary,
    [switch]$SkipBuild,
    [switch]$SkipNegativeTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$resolvedBusyBox = (Resolve-Path $BusyBoxPath).Path
$buildDir = Join-Path $root "build"
$distDir = Join-Path $root "dist"
$provenancePath = Join-Path $buildDir "real-binary-gate-provenance.txt"
$nvmeImagePath = Join-Path $distDir "limitlessos-x86_64-nvme-gpt.img"
$commandLine = "linux /APPS/BUSYBOX echo limitless-real-binary"
$catCommandLine = "linux /APPS/BUSYBOX cat /proc/meminfo"
$nvmeCatCommandLine = "linux /APPS/BUSYBOX cat /nvme/apps/data/file.txt"
$relativeNvmeCatCommandLine = "linux /APPS/BUSYBOX cat nvme/apps/./data/../data/file.txt"
$lsCommandLine = "linux /APPS/BUSYBOX ls /nvme/apps"
$relativeLsCommandLine = "linux /APPS/BUSYBOX ls -l nvme/apps/./data/.."
$procSymlinkCommandLine = "linux /APPS/BUSYBOX ls -l /proc/self/exe"
$procFdCommandLine = "linux /APPS/BUSYBOX ls -l /proc/self/fd"
$procSelfCommandLine = "linux /APPS/BUSYBOX ls -l /proc/self"
$shellCommandLine = "linux /APPS/BUSYBOX sh"
$shellLoopInputLines = if ($RequireShellApplets -or $TraceShellForkBoundary) {
    @(
        $shellCommandLine,
        "echo shellloop",
        "true",
        "ls /nvme/apps",
        "cat /nvme/apps/data/file.txt"
    )
}
elseif ($RequireShellCwdLoop) {
    @(
        $shellCommandLine,
        "echo shellloop",
        "pwd",
        "cd /nvme/apps",
        "pwd",
        "cd /",
        "pwd"
    )
}
else {
    @(
        $shellCommandLine,
        "echo shellloop",
        "true",
        "echo aftertrue"
    )
}
$verifyQemuPath = Join-Path $root "tools\verify-qemu.ps1"
$realLaunchBufferBytes = 4194304
$missingFileFailure = ""
$dynamicElfFailure = ""
$lowAddressFailure = ""
$lowAddressArtifact = ""
$lowAddressNegativeRequired = [bool]$RequireLowAddressNegative
$oversizedFailure = ""
$biosUnavailable = ""
$relativeNvmeConsoleLine = ""
$relativeNvmeSummaryLine = ""
$relativeNvmeRealbinLines = @()
$relativeLsConsoleLine = ""
$relativeLsBusyboxLine = ""
$relativeLsDataLine = ""
$relativeLsSummaryLine = ""
$relativeLsRealbinLines = @()
$procSymlinkConsoleLine = ""
$procSymlinkSummaryLine = ""
$procSymlinkSyscallLine = ""
$procSymlinkRealbinLines = @()
$procFdConsoleLine = ""
$procFdFd0Line = ""
$procFdFd1Line = ""
$procFdFd2Line = ""
$procFdSummaryLine = ""
$procFdSyscallLine = ""
$procFdRealbinLines = @()
$procSelfConsoleLine = ""
$procSelfMapsLine = ""
$procSelfExeLine = ""
$procSelfFdLine = ""
$procSelfStatusLine = ""
$procSelfCmdlineLine = ""
$procSelfEnvironLine = ""
$procSelfSummaryLine = ""
$procSelfSyscallLine = ""
$procSelfRealbinLines = @()

if ($SkipNegativeTests -and $RequireLowAddressNegative) {
    throw "Real-binary gate verifier: -RequireLowAddressNegative cannot be combined with -SkipNegativeTests."
}
if ($RequireShellApplets -and $TraceShellForkBoundary) {
    throw "Real-binary gate verifier: -RequireShellApplets and -TraceShellForkBoundary are mutually exclusive."
}
if ($RequireShellApplets -and $RequireShellCwdLoop) {
    throw "Real-binary gate verifier: -RequireShellApplets and -RequireShellCwdLoop are mutually exclusive."
}
if ($RequireShellCwdLoop -and $TraceShellForkBoundary) {
    throw "Real-binary gate verifier: -RequireShellCwdLoop and -TraceShellForkBoundary are mutually exclusive."
}

function Invoke-Capture
{
    param(
        [string]$Tool,
        [string[]]$Arguments
    )

    $command = Get-Command $Tool -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Real-binary gate verifier requires '$Tool' on PATH."
    }

    $output = & $command.Source @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Real-binary gate verifier: '$Tool $($Arguments -join ' ')' failed."
    }

    return @($output)
}

function Set-Le16
{
    param(
        [byte[]]$Bytes,
        [int]$Offset,
        [UInt16]$Value
    )

    $encoded = [BitConverter]::GetBytes($Value)
    [Array]::Copy($encoded, 0, $Bytes, $Offset, 2)
}

function Set-Le32
{
    param(
        [byte[]]$Bytes,
        [int]$Offset,
        [UInt32]$Value
    )

    $encoded = [BitConverter]::GetBytes($Value)
    [Array]::Copy($encoded, 0, $Bytes, $Offset, 4)
}

function Set-Le64
{
    param(
        [byte[]]$Bytes,
        [int]$Offset,
        [UInt64]$Value
    )

    $encoded = [BitConverter]::GetBytes($Value)
    [Array]::Copy($encoded, 0, $Bytes, $Offset, 8)
}

function New-DynamicElfNegativeFixture
{
    param(
        [string]$Path
    )

    $bytes = New-Object byte[] 512
    $interp = [System.Text.Encoding]::ASCII.GetBytes("/lib64/ld-linux-x86-64.so.2`0")
    $interpOffset = 0x100
    $loadBase = [UInt64]0x52000000

    $bytes[0] = 0x7F
    $bytes[1] = [byte][char]'E'
    $bytes[2] = [byte][char]'L'
    $bytes[3] = [byte][char]'F'
    $bytes[4] = 2
    $bytes[5] = 1
    $bytes[6] = 1
    $bytes[7] = 3

    Set-Le16 -Bytes $bytes -Offset 16 -Value 2
    Set-Le16 -Bytes $bytes -Offset 18 -Value 62
    Set-Le32 -Bytes $bytes -Offset 20 -Value 1
    Set-Le64 -Bytes $bytes -Offset 24 -Value ($loadBase + [UInt64]0x80)
    Set-Le64 -Bytes $bytes -Offset 32 -Value 0x40
    Set-Le32 -Bytes $bytes -Offset 48 -Value 0
    Set-Le16 -Bytes $bytes -Offset 52 -Value 64
    Set-Le16 -Bytes $bytes -Offset 54 -Value 56
    Set-Le16 -Bytes $bytes -Offset 56 -Value 2
    Set-Le16 -Bytes $bytes -Offset 58 -Value 0
    Set-Le16 -Bytes $bytes -Offset 60 -Value 0
    Set-Le16 -Bytes $bytes -Offset 62 -Value 0

    Set-Le32 -Bytes $bytes -Offset 0x40 -Value 1
    Set-Le32 -Bytes $bytes -Offset 0x44 -Value 5
    Set-Le64 -Bytes $bytes -Offset 0x48 -Value 0
    Set-Le64 -Bytes $bytes -Offset 0x50 -Value $loadBase
    Set-Le64 -Bytes $bytes -Offset 0x58 -Value $loadBase
    Set-Le64 -Bytes $bytes -Offset 0x60 -Value $bytes.Length
    Set-Le64 -Bytes $bytes -Offset 0x68 -Value $bytes.Length
    Set-Le64 -Bytes $bytes -Offset 0x70 -Value 0x1000

    Set-Le32 -Bytes $bytes -Offset 0x78 -Value 3
    Set-Le32 -Bytes $bytes -Offset 0x7C -Value 4
    Set-Le64 -Bytes $bytes -Offset 0x80 -Value $interpOffset
    Set-Le64 -Bytes $bytes -Offset 0x88 -Value ($loadBase + [UInt64]$interpOffset)
    Set-Le64 -Bytes $bytes -Offset 0x90 -Value ($loadBase + [UInt64]$interpOffset)
    Set-Le64 -Bytes $bytes -Offset 0x98 -Value $interp.Length
    Set-Le64 -Bytes $bytes -Offset 0xA0 -Value $interp.Length
    Set-Le64 -Bytes $bytes -Offset 0xA8 -Value 1

    [Array]::Copy($interp, 0, $bytes, $interpOffset, $interp.Length)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Assert-RealBinaryFitsLaunchBuffer
{
    param(
        [string]$Path
    )

    $length = (Get-Item $Path).Length
    if ($length -gt $realLaunchBufferBytes) {
        throw "Real-binary gate requires BusyBox to fit in the 4 MiB UEFI real-launch buffer."
    }
    return $length
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$busyBoxHash = (Get-FileHash -Algorithm SHA256 -Path $resolvedBusyBox).Hash.ToLowerInvariant()
$busyBoxLength = Assert-RealBinaryFitsLaunchBuffer -Path $resolvedBusyBox
$readelfHeader = Invoke-Capture -Tool "readelf" -Arguments @("-h", $resolvedBusyBox)
$readelfProgramHeaders = Invoke-Capture -Tool "readelf" -Arguments @("-l", $resolvedBusyBox)
$fileCommand = Get-Command "file" -ErrorAction SilentlyContinue
if ($fileCommand) {
    $fileOutput = & $fileCommand.Source $resolvedBusyBox 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Real-binary gate verifier: 'file $resolvedBusyBox' failed."
    }
    $fileOutput = @($fileOutput)
}
else {
    $fileOutput = @("file: unavailable on PATH; readelf validation used")
}

if ($fileCommand -and (($fileOutput -join "`n") -notmatch "statically linked")) {
    throw "Real-binary gate requires a statically linked BusyBox binary."
}
if (($readelfHeader -join "`n") -notmatch "Type:\s+EXEC") {
    throw "Real-binary gate v1 requires an ET_EXEC BusyBox binary."
}
if (($readelfProgramHeaders -join "`n") -match "INTERP|DYNAMIC") {
    throw "Real-binary gate v1 rejects PT_INTERP/PT_DYNAMIC."
}

if (-not $SkipNegativeTests) {
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -ExtraShellLine $commandLine
    if (-not $?) {
        throw "Real-binary gate verifier: missing-file negative QEMU run failed."
    }

    $negativeLogPath = Join-Path $buildDir "qemu-x86_64-uefi-debug.log"
    if (-not (Test-Path $negativeLogPath)) {
        throw "Real-binary gate verifier: missing-file negative QEMU log not found."
    }
    $negativeRealbinLines = @(Get-Content $negativeLogPath | Where-Object { $_ -match "drs-realbin" })
    $missingFileFailure = $negativeRealbinLines |
        Where-Object { $_ -match "drs-realbin-fail path /APPS/BUSYBOX stage read " } |
        Select-Object -First 1
    if (-not $missingFileFailure) {
        Write-Host "Real-binary missing-file negative telemetry:"
        if ($negativeRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $negativeRealbinLines) {
                Write-Host "  $line"
            }
        }
        throw "Real-binary gate verifier: missing /APPS/BUSYBOX did not fail at read stage."
    }
    Write-Host "Real-binary missing-file negative passed."
    Write-Host "  $missingFileFailure"

    $dynamicFixturePath = Join-Path $buildDir "real-binary-negative-pt-interp.elf"
    New-DynamicElfNegativeFixture -Path $dynamicFixturePath
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $dynamicFixturePath `
        -BusyBoxSource "negative:pt-interp" `
        -BusyBoxVersion "dynamic-pt-interp-fixture" `
        -ExtraShellLine $commandLine
    if (-not $?) {
        throw "Real-binary gate verifier: dynamic-ELF negative QEMU run failed."
    }

    if (-not (Test-Path $negativeLogPath)) {
        throw "Real-binary gate verifier: dynamic-ELF negative QEMU log not found."
    }
    $negativeRealbinLines = @(Get-Content $negativeLogPath | Where-Object { $_ -match "drs-realbin" })
    $dynamicElfFailure = $negativeRealbinLines |
        Where-Object { $_ -match "drs-realbin-fail path /APPS/BUSYBOX stage static code 8 " } |
        Select-Object -First 1
    if (-not $dynamicElfFailure) {
        Write-Host "Real-binary dynamic-ELF negative telemetry:"
        if ($negativeRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $negativeRealbinLines) {
                Write-Host "  $line"
            }
        }
        throw "Real-binary gate verifier: dynamic ELF was not rejected at static stage."
    }
    Write-Host "Real-binary dynamic-ELF negative passed."
    Write-Host "  $dynamicElfFailure"

    if ([string]::IsNullOrWhiteSpace($LowAddressBusyBoxPath)) {
        $candidateLowAddressBusyBoxPath = Join-Path $root "external\busybox-1.35.0-x86_64-linux-musl"
        if (Test-Path $candidateLowAddressBusyBoxPath) {
            $LowAddressBusyBoxPath = $candidateLowAddressBusyBoxPath
            $lowAddressNegativeRequired = $true
        }
    }
    if ([string]::IsNullOrWhiteSpace($LowAddressBusyBoxPath)) {
        if ($lowAddressNegativeRequired) {
            throw "Real-binary gate verifier: -RequireLowAddressNegative was set, but no low-address BusyBox artifact was provided or found at external\busybox-1.35.0-x86_64-linux-musl."
        }
        $lowAddressFailure = "skipped: no upstream-default-address BusyBox artifact provided"
    }
    else {
        $resolvedLowAddressBusyBox = (Resolve-Path $LowAddressBusyBoxPath).Path
        $lowAddressArtifact = $resolvedLowAddressBusyBox
        $lowAddressNegativeRequired = $true
        $lowAddressHeader = Invoke-Capture -Tool "readelf" -Arguments @("-h", $resolvedLowAddressBusyBox)
        $lowAddressProgramHeaders = Invoke-Capture -Tool "readelf" -Arguments @("-l", $resolvedLowAddressBusyBox)
        if (($lowAddressHeader -join "`n") -notmatch "Type:\s+EXEC") {
            throw "Real-binary gate verifier: low-address negative requires an ET_EXEC binary."
        }
        if (($lowAddressProgramHeaders -join "`n") -notmatch "0x0000000000400000") {
            throw "Real-binary gate verifier: low-address negative requires a binary with a 0x400000 LOAD segment."
        }

        & $verifyQemuPath `
            -Architecture x86_64 `
            -BootMedia uefi `
            -BuildProfile Product `
            -RealBinaryGate `
            -BusyBoxPath $resolvedLowAddressBusyBox `
            -BusyBoxSource "negative:upstream-default-address" `
            -BusyBoxVersion "default-low-address" `
            -ExtraShellLine $commandLine
        if (-not $?) {
            throw "Real-binary gate verifier: low-address ET_EXEC negative QEMU run failed."
        }

        if (-not (Test-Path $negativeLogPath)) {
            throw "Real-binary gate verifier: low-address ET_EXEC negative QEMU log not found."
        }
        $negativeRealbinLines = @(Get-Content $negativeLogPath | Where-Object { $_ -match "drs-realbin" })
        $lowAddressFailure = $negativeRealbinLines |
            Where-Object {
                $_ -match "drs-realbin-fail path /APPS/BUSYBOX stage static code 20 " `
                    -and $_ -match "load-first 0x0000000000400000" `
                    -and $_ -match "low-kernel-limit 0x0000000001000000"
            } |
            Select-Object -First 1
        if (-not $lowAddressFailure) {
            Write-Host "Real-binary low-address ET_EXEC negative telemetry:"
            if ($negativeRealbinLines.Count -eq 0) {
                Write-Host "  <none>"
            }
            else {
                foreach ($line in $negativeRealbinLines) {
                    Write-Host "  $line"
                }
            }
            throw "Real-binary gate verifier: low-address ET_EXEC was not rejected at the static load-address stage."
        }
        Write-Host "Real-binary low-address ET_EXEC negative passed."
        Write-Host "  $lowAddressFailure"
    }

    $oversizedPath = Join-Path $buildDir "real-binary-negative-oversized.bin"
    $oversizedStream = [System.IO.File]::Open($oversizedPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $oversizedStream.SetLength([int64]($realLaunchBufferBytes + 1))
    }
    finally {
        $oversizedStream.Dispose()
    }
    try {
        $null = Assert-RealBinaryFitsLaunchBuffer -Path $oversizedPath
    }
    catch {
        $oversizedFailure = $_.Exception.Message
    }
    if ($oversizedFailure -notmatch "4 MiB UEFI real-launch buffer") {
        throw "Real-binary gate verifier: oversized binary was not rejected before staging."
    }
    Write-Host "Real-binary oversized negative passed."
    Write-Host "  $oversizedFailure"

    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia disk `
        -BuildProfile Product `
        -RealBinaryGate `
        -ExtraShellLine $commandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BIOS unavailable negative QEMU run failed."
    }

    $biosLogPath = Join-Path $buildDir "qemu-x86_64-disk-debug.log"
    if (-not (Test-Path $biosLogPath)) {
        throw "Real-binary gate verifier: BIOS unavailable QEMU log not found."
    }
    $biosUnavailable = Get-Content $biosLogPath |
        Where-Object { $_ -match "drs-realbin-unavailable bios 1 nvme 0" } |
        Select-Object -First 1
    if (-not $biosUnavailable) {
        Write-Host "Real-binary BIOS unavailable telemetry:"
        $biosRealbinLines = @(Get-Content $biosLogPath | Where-Object { $_ -match "drs-realbin" })
        if ($biosRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $biosRealbinLines) {
                Write-Host "  $line"
            }
        }
        throw "Real-binary gate verifier: BIOS path did not report unavailable telemetry."
    }
    Write-Host "Real-binary BIOS unavailable negative passed."
    Write-Host "  $biosUnavailable"
}
else {
    $missingFileFailure = "skipped"
    $dynamicElfFailure = "skipped"
    $lowAddressFailure = "skipped"
    $oversizedFailure = "skipped"
    $biosUnavailable = "skipped"
}

$generatorPath = Join-Path $root "tools\generate-nvme-image.ps1"
& $generatorPath `
    -OutputPath $nvmeImagePath `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion
if (-not $?) {
    throw "Real-binary gate verifier: failed to stage BusyBox into the NVMe image."
}

if (-not $SkipBuild) {
    & (Join-Path $root "tools\build.ps1") -Architecture x86_64 -BuildProfile Product
    if (-not $?) {
        throw "Real-binary gate verifier: build failed."
    }
}

$qemuLogPath = Join-Path $buildDir "qemu-x86_64-uefi-debug.log"

if ($RequireProcSymlinkProof) {
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $resolvedBusyBox `
        -BusyBoxSource $BusyBoxSource `
        -BusyBoxVersion $BusyBoxVersion `
        -ExtraShellLine $procSymlinkCommandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BusyBox proc symlink QEMU run failed."
    }

    if (-not (Test-Path $qemuLogPath)) {
        throw "Real-binary gate verifier: BusyBox proc symlink QEMU debug log not found."
    }

    $procSymlinkQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
    $procSymlinkRealbinLines = @($procSymlinkQemuLines | Where-Object { $_ -match "drs-realbin" })
    $procSymlinkConsoleLine = $procSymlinkQemuLines |
        Where-Object { $_ -match "^lrwxrwxrwx\s+1\s+14\s+/proc/self/exe\s+->\s+/proc/self/exe$" } |
        Select-Object -First 1
    $procSymlinkSummaryLine = $procSymlinkRealbinLines |
        Where-Object {
            $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
                -and $_ -match "stat [1-9][0-9]*" `
                -and $_ -match "stat-denial 0" `
                -and $_ -match "stat-fault 0" `
                -and $_ -match "readlink [1-9][0-9]*" `
                -and $_ -match "readlink-bytes 14" `
                -and $_ -match "readlink-denial 0" `
                -and $_ -match "readlink-fault 0" `
                -and $_ -match "readlink-last-result 14"
        } |
        Select-Object -First 1
    $procSymlinkSyscallLine = $procSymlinkRealbinLines |
        Where-Object { $_ -match "drs-realbin-syscall-last .* unimplemented 0" } |
        Select-Object -First 1

    if ((-not $procSymlinkConsoleLine) -or (-not $procSymlinkSummaryLine) -or (-not $procSymlinkSyscallLine)) {
        Write-Host "Real-binary BusyBox proc symlink telemetry:"
        if ($procSymlinkRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $procSymlinkRealbinLines) {
                Write-Host "  $line"
            }
        }
        $procSymlinkFirstFailure = $procSymlinkRealbinLines |
            Where-Object { $_ -match "drs-realbin-fail" } |
            Select-Object -First 1
        if ($procSymlinkFirstFailure) {
            throw "Real-binary gate verifier: BusyBox proc symlink did not reach exit 0; first failure: $procSymlinkFirstFailure"
        }
        if (-not $procSymlinkConsoleLine) {
            throw "Real-binary gate verifier: BusyBox proc symlink output was not observed."
        }
        if (-not $procSymlinkSyscallLine) {
            throw "Real-binary gate verifier: BusyBox proc symlink still reported an unimplemented syscall."
        }
        throw "Real-binary gate verifier: successful BusyBox proc symlink drs-realbin telemetry was not observed."
    }
}

if ($RequireProcFdProof) {
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $resolvedBusyBox `
        -BusyBoxSource $BusyBoxSource `
        -BusyBoxVersion $BusyBoxVersion `
        -ExtraShellLine $procFdCommandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BusyBox proc fd QEMU run failed."
    }

    if (-not (Test-Path $qemuLogPath)) {
        throw "Real-binary gate verifier: BusyBox proc fd QEMU debug log not found."
    }

    $procFdQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
    $procFdRealbinLines = @($procFdQemuLines | Where-Object { $_ -match "drs-realbin" })
    $procFdFd0Line = $procFdQemuLines |
        Where-Object { $_ -match "^lrwxrwxrwx\s+1\s+11\s+0\s+->\s+anon:\[fd 0\]$" } |
        Select-Object -First 1
    $procFdFd1Line = $procFdQemuLines |
        Where-Object { $_ -match "^lrwxrwxrwx\s+1\s+11\s+1\s+->\s+anon:\[fd 1\]$" } |
        Select-Object -First 1
    $procFdFd2Line = $procFdQemuLines |
        Where-Object { $_ -match "^lrwxrwxrwx\s+1\s+11\s+2\s+->\s+anon:\[fd 2\]$" } |
        Select-Object -First 1
    $procFdErrorLine = $procFdQemuLines |
        Where-Object { $_ -match "cannot read link" } |
        Select-Object -First 1
    if ($procFdFd0Line -and $procFdFd1Line -and $procFdFd2Line) {
        $procFdConsoleLine = "$procFdFd2Line / $procFdFd1Line / $procFdFd0Line"
    }
    $procFdSummaryLine = $procFdRealbinLines |
        Where-Object {
            $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
                -and $_ -match "getdents64 [1-9][0-9]*" `
                -and $_ -match "getdents64-entries 3" `
                -and $_ -match "getdents64-bytes 72" `
                -and $_ -match "stat 4" `
                -and $_ -match "stat-denial 0" `
                -and $_ -match "stat-fault 0" `
                -and $_ -match "readlink 3" `
                -and $_ -match "readlink-bytes 33" `
                -and $_ -match "readlink-denial 0" `
                -and $_ -match "readlink-fault 0" `
                -and $_ -match "readlink-last-result 11" `
                -and $_ -match "writev 3" `
                -and $_ -match "writev-bytes 129"
        } |
        Select-Object -First 1
    $procFdSyscallLine = $procFdRealbinLines |
        Where-Object { $_ -match "drs-realbin-syscall-last .* unimplemented 0" } |
        Select-Object -First 1

    if ((-not $procFdConsoleLine) -or $procFdErrorLine -or (-not $procFdSummaryLine) -or (-not $procFdSyscallLine)) {
        Write-Host "Real-binary BusyBox proc fd telemetry:"
        if ($procFdRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $procFdRealbinLines) {
                Write-Host "  $line"
            }
        }
        $procFdFirstFailure = $procFdRealbinLines |
            Where-Object { $_ -match "drs-realbin-fail" } |
            Select-Object -First 1
        if ($procFdFirstFailure) {
            throw "Real-binary gate verifier: BusyBox proc fd did not reach exit 0; first failure: $procFdFirstFailure"
        }
        if (-not $procFdConsoleLine) {
            throw "Real-binary gate verifier: BusyBox proc fd output was not observed."
        }
        if ($procFdErrorLine) {
            throw "Real-binary gate verifier: BusyBox proc fd reported a readlink error: $procFdErrorLine"
        }
        if (-not $procFdSyscallLine) {
            throw "Real-binary gate verifier: BusyBox proc fd reported an unimplemented syscall."
        }
        throw "Real-binary gate verifier: successful BusyBox proc fd drs-realbin telemetry was not observed."
    }
}

if ($RequireProcSelfProof) {
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $resolvedBusyBox `
        -BusyBoxSource $BusyBoxSource `
        -BusyBoxVersion $BusyBoxVersion `
        -ExtraShellLine $procSelfCommandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BusyBox proc self QEMU run failed."
    }

    if (-not (Test-Path $qemuLogPath)) {
        throw "Real-binary gate verifier: BusyBox proc self QEMU debug log not found."
    }

    $procSelfQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
    $procSelfRealbinLines = @($procSelfQemuLines | Where-Object { $_ -match "drs-realbin" })
    $procSelfEnvironLine = $procSelfQemuLines |
        Where-Object { $_ -match "^-r--r--r--\s+1\s+0\s+environ$" } |
        Select-Object -First 1
    $procSelfCmdlineLine = $procSelfQemuLines |
        Where-Object { $_ -match "^-r--r--r--\s+1\s+0\s+cmdline$" } |
        Select-Object -First 1
    $procSelfStatusLine = $procSelfQemuLines |
        Where-Object { $_ -match "^-r--r--r--\s+1\s+70\s+status$" } |
        Select-Object -First 1
    $procSelfFdLine = $procSelfQemuLines |
        Where-Object { $_ -match "^dr-xr-xr-x\s+2\s+0\s+fd$" } |
        Select-Object -First 1
    $procSelfExeLine = $procSelfQemuLines |
        Where-Object { $_ -match "^lrwxrwxrwx\s+1\s+14\s+exe\s+->\s+/proc/self/exe$" } |
        Select-Object -First 1
    $procSelfMapsLine = $procSelfQemuLines |
        Where-Object { $_ -match "^-r--r--r--\s+1\s+384\s+maps$" } |
        Select-Object -First 1
    if ($procSelfEnvironLine -and $procSelfCmdlineLine -and $procSelfStatusLine -and $procSelfFdLine -and $procSelfExeLine -and $procSelfMapsLine) {
        $procSelfConsoleLine = "$procSelfEnvironLine / $procSelfCmdlineLine / $procSelfStatusLine / $procSelfFdLine / $procSelfExeLine / $procSelfMapsLine"
    }
    $procSelfSummaryLine = $procSelfRealbinLines |
        Where-Object {
            $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
                -and $_ -match "getdents64 [1-9][0-9]*" `
                -and $_ -match "getdents64-entries 6" `
                -and $_ -match "getdents64-bytes 168" `
                -and $_ -match "stat 7" `
                -and $_ -match "stat-denial 0" `
                -and $_ -match "stat-fault 0" `
                -and $_ -match "readlink 1" `
                -and $_ -match "readlink-bytes 14" `
                -and $_ -match "readlink-denial 0" `
                -and $_ -match "readlink-fault 0" `
                -and $_ -match "readlink-last-result 14" `
                -and $_ -match "writev 6" `
                -and $_ -match "writev-bytes 209"
        } |
        Select-Object -First 1
    $procSelfSyscallLine = $procSelfRealbinLines |
        Where-Object { $_ -match "drs-realbin-syscall-last .* unimplemented 0" } |
        Select-Object -First 1

    if ((-not $procSelfConsoleLine) -or (-not $procSelfSummaryLine) -or (-not $procSelfSyscallLine)) {
        Write-Host "Real-binary BusyBox proc self telemetry:"
        if ($procSelfRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $procSelfRealbinLines) {
                Write-Host "  $line"
            }
        }
        $procSelfFirstFailure = $procSelfRealbinLines |
            Where-Object { $_ -match "drs-realbin-fail" } |
            Select-Object -First 1
        if ($procSelfFirstFailure) {
            throw "Real-binary gate verifier: BusyBox proc self did not reach exit 0; first failure: $procSelfFirstFailure"
        }
        if (-not $procSelfConsoleLine) {
            throw "Real-binary gate verifier: BusyBox proc self output was not observed."
        }
        if (-not $procSelfSyscallLine) {
            throw "Real-binary gate verifier: BusyBox proc self reported an unimplemented syscall."
        }
        throw "Real-binary gate verifier: successful BusyBox proc self drs-realbin telemetry was not observed."
    }
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion `
    -ExtraShellLine $commandLine
if (-not $?) {
    throw "Real-binary gate verifier: QEMU run failed."
}

if (-not (Test-Path $qemuLogPath)) {
    throw "Real-binary gate verifier: QEMU debug log not found."
}

$qemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
$realbinLines = @($qemuLines | Where-Object { $_ -match "drs-realbin" })
$consoleLine = $qemuLines | Where-Object { $_ -match "^limitless-real-binary$" } | Select-Object -First 1
$summaryLine = $realbinLines |
    Where-Object {
        $_ -match "drs-realbin path /APPS/BUSYBOX .* exit 0 cleanup 1" `
            -and $_ -match "vfs-nvme-reads 0" `
            -and $_ -match "vfs-nvme-readdirs 0" `
            -and $_ -match "vfs-nvme-dirents 0" `
            -and $_ -match "vfs-nvme-bytes 0"
    } |
    Select-Object -First 1

if ((-not $consoleLine) -or (-not $summaryLine)) {
    Write-Host "Real-binary gate telemetry:"
    if ($realbinLines.Count -eq 0) {
        Write-Host "  <none>"
    }
    else {
        foreach ($line in $realbinLines) {
            Write-Host "  $line"
        }
    }
    $firstFailure = $realbinLines | Where-Object { $_ -match "drs-realbin-fail" } | Select-Object -First 1
    if ($firstFailure) {
        throw "Real-binary gate verifier: BusyBox did not reach exit 0; first failure: $firstFailure"
    }
    if (-not $consoleLine) {
        throw "Real-binary gate verifier: BusyBox output was not observed."
    }
    throw "Real-binary gate verifier: successful echo drs-realbin telemetry with zero NVMe VFS deltas was not observed."
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion `
    -ExtraShellLine $catCommandLine
if (-not $?) {
    throw "Real-binary gate verifier: BusyBox cat /proc/meminfo QEMU run failed."
}

if (-not (Test-Path $qemuLogPath)) {
    throw "Real-binary gate verifier: BusyBox cat /proc/meminfo QEMU debug log not found."
}

$catQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
$catRealbinLines = @($catQemuLines | Where-Object { $_ -match "drs-realbin" })
$catConsoleLine = $catQemuLines | Where-Object { $_ -match "^MemTotal:\s+[0-9]+ kB$" } | Select-Object -First 1
$catSummaryLine = $catRealbinLines |
    Where-Object {
        $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
            -and $_ -match "vfs-nvme-reads 0" `
            -and $_ -match "vfs-nvme-readdirs 0" `
            -and $_ -match "vfs-nvme-dirents 0" `
            -and $_ -match "vfs-nvme-bytes 0"
    } |
    Select-Object -First 1

if ((-not $catConsoleLine) -or (-not $catSummaryLine)) {
    Write-Host "Real-binary BusyBox cat telemetry:"
    if ($catRealbinLines.Count -eq 0) {
        Write-Host "  <none>"
    }
    else {
        foreach ($line in $catRealbinLines) {
            Write-Host "  $line"
        }
    }
    $catFirstFailure = $catRealbinLines | Where-Object { $_ -match "drs-realbin-fail" } | Select-Object -First 1
    if ($catFirstFailure) {
        throw "Real-binary gate verifier: BusyBox cat did not reach exit 0; first failure: $catFirstFailure"
    }
    if (-not $catConsoleLine) {
        throw "Real-binary gate verifier: BusyBox cat /proc/meminfo output was not observed."
    }
    throw "Real-binary gate verifier: successful BusyBox cat /proc/meminfo drs-realbin telemetry with zero NVMe VFS deltas was not observed."
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion `
    -ExtraShellLine $nvmeCatCommandLine
if (-not $?) {
    throw "Real-binary gate verifier: BusyBox cat /nvme/apps/data/file.txt QEMU run failed."
}

if (-not (Test-Path $qemuLogPath)) {
    throw "Real-binary gate verifier: BusyBox cat /nvme/apps/data/file.txt QEMU debug log not found."
}

$nvmeQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
$nvmeRealbinLines = @($nvmeQemuLines | Where-Object { $_ -match "drs-realbin" })
$nvmeConsoleLine = $nvmeQemuLines | Where-Object { $_ -match "^Nested FAT32 path fixture$" } | Select-Object -First 1
$nvmeSummaryLine = $nvmeRealbinLines |
    Where-Object {
        $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
            -and $_ -match "vfs-nvme-bind 1" `
            -and $_ -match "vfs-nvme-release 1" `
            -and $_ -match "vfs-nvme-reads [1-9][0-9]*" `
            -and $_ -match "vfs-nvme-bytes 27"
    } |
    Select-Object -First 1

if ((-not $nvmeConsoleLine) -or (-not $nvmeSummaryLine)) {
    Write-Host "Real-binary BusyBox cat NVMe telemetry:"
    if ($nvmeRealbinLines.Count -eq 0) {
        Write-Host "  <none>"
    }
    else {
        foreach ($line in $nvmeRealbinLines) {
            Write-Host "  $line"
        }
    }
    $nvmeFirstFailure = $nvmeRealbinLines | Where-Object { $_ -match "drs-realbin-fail" } | Select-Object -First 1
    if ($nvmeFirstFailure) {
        throw "Real-binary gate verifier: BusyBox cat /nvme/apps/data/file.txt did not reach exit 0; first failure: $nvmeFirstFailure"
    }
    if (-not $nvmeConsoleLine) {
        throw "Real-binary gate verifier: BusyBox cat /nvme/apps/data/file.txt output was not observed."
    }
    throw "Real-binary gate verifier: successful BusyBox cat NVMe drs-realbin telemetry was not observed."
}

if ($RequireRelativePathProof) {
    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $resolvedBusyBox `
        -BusyBoxSource $BusyBoxSource `
        -BusyBoxVersion $BusyBoxVersion `
        -ExtraShellLine $relativeNvmeCatCommandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BusyBox relative cat QEMU run failed."
    }

    if (-not (Test-Path $qemuLogPath)) {
        throw "Real-binary gate verifier: BusyBox relative cat QEMU debug log not found."
    }

    $relativeNvmeQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
    $relativeNvmeRealbinLines = @($relativeNvmeQemuLines | Where-Object { $_ -match "drs-realbin" })
    $relativeNvmeConsoleLine = $relativeNvmeQemuLines |
        Where-Object { $_ -match "^Nested FAT32 path fixture$" } |
        Select-Object -First 1
    $relativeNvmeSummaryLine = $relativeNvmeRealbinLines |
        Where-Object {
            $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
                -and $_ -match "path-relative [1-9][0-9]*" `
                -and $_ -match "path-dot [1-9][0-9]*" `
                -and $_ -match "path-dotdot [1-9][0-9]*" `
                -and $_ -match "path-fault 0" `
                -and $_ -match "vfs-nvme-bind 1" `
                -and $_ -match "vfs-nvme-release 1" `
                -and $_ -match "vfs-nvme-reads [1-9][0-9]*" `
                -and $_ -match "vfs-nvme-bytes 27"
        } |
        Select-Object -First 1

    if ((-not $relativeNvmeConsoleLine) -or (-not $relativeNvmeSummaryLine)) {
        Write-Host "Real-binary BusyBox relative cat telemetry:"
        if ($relativeNvmeRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $relativeNvmeRealbinLines) {
                Write-Host "  $line"
            }
        }
        $relativeNvmeFirstFailure = $relativeNvmeRealbinLines |
            Where-Object { $_ -match "drs-realbin-fail" } |
            Select-Object -First 1
        if ($relativeNvmeFirstFailure) {
            throw "Real-binary gate verifier: BusyBox relative cat did not reach exit 0; first failure: $relativeNvmeFirstFailure"
        }
        if (-not $relativeNvmeConsoleLine) {
            throw "Real-binary gate verifier: BusyBox relative cat output was not observed."
        }
        throw "Real-binary gate verifier: successful BusyBox relative cat drs-realbin telemetry was not observed."
    }

    & $verifyQemuPath `
        -Architecture x86_64 `
        -BootMedia uefi `
        -BuildProfile Product `
        -RealBinaryGate `
        -BusyBoxPath $resolvedBusyBox `
        -BusyBoxSource $BusyBoxSource `
        -BusyBoxVersion $BusyBoxVersion `
        -ExtraShellLine $relativeLsCommandLine
    if (-not $?) {
        throw "Real-binary gate verifier: BusyBox relative ls QEMU run failed."
    }

    if (-not (Test-Path $qemuLogPath)) {
        throw "Real-binary gate verifier: BusyBox relative ls QEMU debug log not found."
    }

    $relativeLsQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
    $relativeLsRealbinLines = @($relativeLsQemuLines | Where-Object { $_ -match "drs-realbin" })
    $relativeLsBusyboxLine = $relativeLsQemuLines |
        Where-Object { $_ -match "^-r--r--r--\s+1\s+145264\s+busybox$" } |
        Select-Object -First 1
    $relativeLsDataLine = $relativeLsQemuLines |
        Where-Object { $_ -match "^dr-xr-xr-x\s+2\s+0\s+data$" } |
        Select-Object -First 1
    if ($relativeLsBusyboxLine -and $relativeLsDataLine) {
        $relativeLsConsoleLine = "$relativeLsBusyboxLine / $relativeLsDataLine"
    }
    $relativeLsSummaryLine = $relativeLsRealbinLines |
        Where-Object {
            $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
                -and $_ -match "getdents64 [1-9][0-9]*" `
                -and $_ -match "getdents64-entries 2" `
                -and $_ -match "stat [1-9][0-9]*" `
                -and $_ -match "stat-denial 0" `
                -and $_ -match "stat-fault 0" `
                -and $_ -match "path-relative [1-9][0-9]*" `
                -and $_ -match "path-dot [1-9][0-9]*" `
                -and $_ -match "path-dotdot [1-9][0-9]*" `
                -and $_ -match "path-fault 0" `
                -and $_ -match "vfs-nvme-bind 1" `
                -and $_ -match "vfs-nvme-release 1" `
                -and $_ -match "vfs-nvme-readdirs [1-9][0-9]*" `
                -and $_ -match "vfs-nvme-dirents 2"
        } |
        Select-Object -First 1

    if ((-not $relativeLsBusyboxLine) -or (-not $relativeLsDataLine) -or (-not $relativeLsSummaryLine)) {
        Write-Host "Real-binary BusyBox relative ls telemetry:"
        if ($relativeLsRealbinLines.Count -eq 0) {
            Write-Host "  <none>"
        }
        else {
            foreach ($line in $relativeLsRealbinLines) {
                Write-Host "  $line"
            }
        }
        $relativeLsFirstFailure = $relativeLsRealbinLines |
            Where-Object { $_ -match "drs-realbin-fail" } |
            Select-Object -First 1
        if ($relativeLsFirstFailure) {
            throw "Real-binary gate verifier: BusyBox relative ls did not reach exit 0; first failure: $relativeLsFirstFailure"
        }
        if ((-not $relativeLsBusyboxLine) -or (-not $relativeLsDataLine)) {
            throw "Real-binary gate verifier: BusyBox relative ls -l output was not observed."
        }
        throw "Real-binary gate verifier: successful BusyBox relative ls -l drs-realbin telemetry was not observed."
    }
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion `
    -ExtraShellLine $lsCommandLine
if (-not $?) {
    throw "Real-binary gate verifier: BusyBox ls /nvme/apps QEMU run failed."
}

if (-not (Test-Path $qemuLogPath)) {
    throw "Real-binary gate verifier: BusyBox ls /nvme/apps QEMU debug log not found."
}

$lsQemuLines = @(Get-Content $qemuLogPath | Where-Object { $_.Trim().Length -gt 0 })
$lsRealbinLines = @($lsQemuLines | Where-Object { $_ -match "drs-realbin" })
$lsConsoleLine = $lsQemuLines | Where-Object { $_ -match "^busybox\s+data$" } | Select-Object -First 1
$lsSummaryLine = $lsRealbinLines |
    Where-Object {
        $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]* exit 0 cleanup 1" `
            -and $_ -match "getdents64 [1-9][0-9]*" `
            -and $_ -match "getdents64-entries 2" `
            -and $_ -match "getdents64-bytes [1-9][0-9]*" `
            -and $_ -match "vfs-nvme-readdirs [1-9][0-9]*" `
            -and $_ -match "vfs-nvme-dirents 2"
    } |
    Select-Object -First 1
$lsSyscallLine = $lsRealbinLines |
    Where-Object { $_ -match "drs-realbin-syscall-last .* unimplemented 0" } |
    Select-Object -First 1

if ((-not $lsConsoleLine) -or (-not $lsSummaryLine) -or (-not $lsSyscallLine)) {
    Write-Host "Real-binary BusyBox ls telemetry:"
    if ($lsRealbinLines.Count -eq 0) {
        Write-Host "  <none>"
    }
    else {
        foreach ($line in $lsRealbinLines) {
            Write-Host "  $line"
        }
    }
    $lsFirstFailure = $lsRealbinLines | Where-Object { $_ -match "drs-realbin-fail" } | Select-Object -First 1
    if ($lsFirstFailure) {
        throw "Real-binary gate verifier: BusyBox ls /nvme/apps did not reach exit 0; first failure: $lsFirstFailure"
    }
    if (-not $lsConsoleLine) {
        throw "Real-binary gate verifier: BusyBox ls /nvme/apps output was not observed."
    }
    if (-not $lsSyscallLine) {
        throw "Real-binary gate verifier: BusyBox ls /nvme/apps reported an unimplemented syscall."
    }
    throw "Real-binary gate verifier: successful BusyBox ls /nvme/apps drs-realbin telemetry was not observed."
}

& $verifyQemuPath `
    -Architecture x86_64 `
    -BootMedia uefi `
    -BuildProfile Product `
    -RealBinaryGate `
    -BusyBoxPath $resolvedBusyBox `
    -BusyBoxSource $BusyBoxSource `
    -BusyBoxVersion $BusyBoxVersion `
    -ExtraShellLine $shellLoopInputLines
if (-not $?) {
    throw "Real-binary gate verifier: BusyBox sh QEMU run failed."
}

if (-not (Test-Path $qemuLogPath)) {
    throw "Real-binary gate verifier: BusyBox sh QEMU debug log not found."
}

$shellQemuText = Get-Content $qemuLogPath -Raw
$shellQemuLines = @($shellQemuText -split '\r?\n' | Where-Object { $_.Trim().Length -gt 0 })
$shellRealbinLines = @($shellQemuLines | Where-Object { $_ -match "drs-realbin" })
$shellBannerLine = $shellQemuLines |
    Where-Object { $_ -match "^BusyBox v[0-9]+\.[0-9]+\.[0-9]+ .* built-in shell \(ash\)$" } |
    Select-Object -First 1
$shellPromptLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ drs-realbin path /APPS/BUSYBOX ' } |
    Select-Object -First 1
$shellLoopEchoLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ shellloop$' } |
    Select-Object -First 1
$shellLoopAfterTrueLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ (\$ )?aftertrue$' } |
    Select-Object -First 1
$shellLoopLsLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ (\$ )?busybox\s+data$' } |
    Select-Object -First 1
$shellLoopCatLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ (\$ )?Nested FAT32 path fixture$' } |
    Select-Object -First 1
$shellCwdRootLines = @($shellQemuLines |
    Where-Object { $_ -match '^\$ (\$ )?/$' })
$shellCwdNvmeLine = $shellQemuLines |
    Where-Object { $_ -match '^\$ (\$ )?/nvme/apps$' } |
    Select-Object -First 1
$shellForkErrorLines = @($shellQemuLines |
    Where-Object { $_ -match "sh: can't fork: Function not implemented" })
$shellSummaryLine = $shellRealbinLines |
    Where-Object {
        $_ -match "drs-realbin path /APPS/BUSYBOX .* console-bytes [1-9][0-9]*" `
            -and (
                (($TraceShellForkBoundary -and $_ -match " exit 2 cleanup 1") `
                    -or ((-not $TraceShellForkBoundary) -and $_ -match " exit 0 cleanup 1"))
            ) `
            -and (
                (((-not $RequireShellApplets) -and (-not $RequireShellCwdLoop) -and (-not $TraceShellForkBoundary)) `
                    -and $_ -match "getdents64 0" `
                    -and $_ -match "getdents64-entries 0" `
                    -and $_ -match "vfs-nvme-reads 0" `
                    -and $_ -match "vfs-nvme-readdirs 0" `
                    -and $_ -match "vfs-nvme-dirents 0" `
                    -and $_ -match "vfs-nvme-bytes 0") `
                -or ($TraceShellForkBoundary `
                    -and $_ -match "getdents64 0" `
                    -and $_ -match "getdents64-entries 0" `
                    -and $_ -match "vfs-nvme-reads 0" `
                    -and $_ -match "vfs-nvme-readdirs 0" `
                    -and $_ -match "vfs-nvme-dirents 0" `
                    -and $_ -match "vfs-nvme-bytes 0" `
                    -and $_ -match "getcwd 0" `
                    -and $_ -match "chdir 0" `
                    -and $_ -match "fork 2" `
                    -and $_ -match "fork-enosys 2" `
                    -and $_ -match "fork-denial 0" `
                    -and $_ -match "fork-last-rip 0x(?!0000000000000000)[0-9A-F]+") `
                -or ($RequireShellCwdLoop `
                    -and $_ -match "getdents64 0" `
                    -and $_ -match "getdents64-entries 0" `
                    -and $_ -match "vfs-nvme-reads 0" `
                    -and $_ -match "vfs-nvme-readdirs 0" `
                    -and $_ -match "vfs-nvme-dirents 0" `
                    -and $_ -match "vfs-nvme-bytes 0" `
                    -and $_ -match "getcwd [1-9][0-9]*" `
                    -and $_ -match "getcwd-bytes [1-9][0-9]*" `
                    -and $_ -match "getcwd-denial 0" `
                    -and $_ -match "getcwd-fault 0" `
                    -and $_ -match "chdir 2" `
                    -and $_ -match "fchdir 0" `
                    -and $_ -match "chdir-denial 0" `
                    -and $_ -match "chdir-fault 0" `
                    -and $_ -match "fork 0" `
                    -and $_ -match "fork-enosys 0" `
                    -and $_ -match "fork-denial 0") `
                -or ($RequireShellApplets `
                    -and $_ -match "getdents64 [1-9][0-9]*" `
                    -and $_ -match "getdents64-entries 2" `
                    -and $_ -match "vfs-nvme-reads [1-9][0-9]*" `
                    -and $_ -match "vfs-nvme-readdirs [1-9][0-9]*" `
                    -and $_ -match "vfs-nvme-dirents 2" `
                    -and $_ -match "vfs-nvme-bytes 27")
            ) `
            -and $_ -match " read [1-9][0-9]* " `
            -and $_ -match " read-bytes [1-9][0-9]*" `
            -and $_ -match " writev [1-9][0-9]*" `
            -and $_ -match " writev-bytes [1-9][0-9]*" `
            -and $_ -match " ioctl-tty [1-9][0-9]*" `
            -and $_ -match " ioctl-enotty 0" `
            -and $_ -match " ioctl-enosys 0"
    } |
    Select-Object -First 1
$shellSyscallLine = $shellRealbinLines |
    Where-Object { $_ -match "drs-realbin-syscall-last .* unimplemented 0" } |
    Select-Object -First 1

if ((-not $shellBannerLine) `
    -or (-not $shellPromptLine) `
    -or (-not $shellLoopEchoLine) `
    -or (((-not $RequireShellApplets) -and (-not $RequireShellCwdLoop) -and (-not $TraceShellForkBoundary) -and (-not $shellLoopAfterTrueLine))) `
    -or (($RequireShellCwdLoop -and (($shellCwdRootLines.Count -lt 2) -or (-not $shellCwdNvmeLine)))) `
    -or (($TraceShellForkBoundary -and ($shellForkErrorLines.Count -lt 2))) `
    -or (($RequireShellApplets -and ((-not $shellLoopLsLine) -or (-not $shellLoopCatLine)))) `
    -or (-not $shellSummaryLine) `
    -or (-not $shellSyscallLine)) {
    Write-Host "Real-binary BusyBox sh telemetry:"
    if ($shellRealbinLines.Count -eq 0) {
        Write-Host "  <none>"
    }
    else {
        foreach ($line in $shellRealbinLines) {
            Write-Host "  $line"
        }
    }
    $shellFirstFailure = $shellRealbinLines | Where-Object { $_ -match "drs-realbin-fail" } | Select-Object -First 1
    if ($shellFirstFailure -and (-not $TraceShellForkBoundary)) {
        throw "Real-binary gate verifier: BusyBox sh did not reach exit 0; first failure: $shellFirstFailure"
    }
    if (-not $shellBannerLine) {
        throw "Real-binary gate verifier: BusyBox sh banner was not observed."
    }
    if (-not $shellPromptLine) {
        throw "Real-binary gate verifier: BusyBox sh prompt was not observed."
    }
    if (-not $shellLoopEchoLine) {
        throw "Real-binary gate verifier: BusyBox sh loop echo output was not observed."
    }
    if ((-not $RequireShellApplets) -and (-not $RequireShellCwdLoop) -and (-not $TraceShellForkBoundary) -and (-not $shellLoopAfterTrueLine)) {
        throw "Real-binary gate verifier: BusyBox sh loop did not continue after true."
    }
    if ($RequireShellCwdLoop -and ($shellCwdRootLines.Count -lt 2)) {
        throw "Real-binary gate verifier: BusyBox sh cwd loop did not show root before and after cd."
    }
    if ($RequireShellCwdLoop -and (-not $shellCwdNvmeLine)) {
        throw "Real-binary gate verifier: BusyBox sh cwd loop did not show /nvme/apps."
    }
    if ($TraceShellForkBoundary -and ($shellForkErrorLines.Count -lt 2)) {
        throw "Real-binary gate verifier: BusyBox sh fork-boundary trace did not show two fork failures."
    }
    if ($RequireShellApplets -and (-not $shellLoopLsLine)) {
        throw "Real-binary gate verifier: BusyBox sh applet loop ls output was not observed."
    }
    if ($RequireShellApplets -and (-not $shellLoopCatLine)) {
        throw "Real-binary gate verifier: BusyBox sh applet loop cat output was not observed."
    }
    if (-not $shellSyscallLine) {
        throw "Real-binary gate verifier: BusyBox sh reported an unimplemented syscall."
    }
    throw "Real-binary gate verifier: successful BusyBox sh drs-realbin telemetry was not observed."
}

$provenance = @(
    "source=$BusyBoxSource",
    "version=$BusyBoxVersion",
    "sha256=$busyBoxHash",
    "bytes=$busyBoxLength",
    "staged-path=/APPS/BUSYBOX",
    "command=$commandLine",
    "console-output=limitless-real-binary",
    "exit-code=0",
    "cat-command=$catCommandLine",
    "cat-console-output=$catConsoleLine",
    "cat-exit-code=0",
    "nvme-cat-command=$nvmeCatCommandLine",
    "nvme-cat-console-output=$nvmeConsoleLine",
    "nvme-cat-exit-code=0",
    "relative-path-proof-required=$([bool]$RequireRelativePathProof)",
    "relative-nvme-cat-command=$relativeNvmeCatCommandLine",
    "relative-nvme-cat-console-output=$relativeNvmeConsoleLine",
    "relative-nvme-cat-exit-code=$(if ($RequireRelativePathProof) { 0 } else { '' })",
    "ls-command=$lsCommandLine",
    "ls-console-output=$lsConsoleLine",
    "ls-exit-code=0",
    "relative-ls-command=$relativeLsCommandLine",
    "relative-ls-console-output=$relativeLsConsoleLine",
    "relative-ls-busybox-output=$relativeLsBusyboxLine",
    "relative-ls-data-output=$relativeLsDataLine",
    "relative-ls-exit-code=$(if ($RequireRelativePathProof) { 0 } else { '' })",
    "proc-symlink-proof-required=$([bool]$RequireProcSymlinkProof)",
    "proc-symlink-command=$procSymlinkCommandLine",
    "proc-symlink-console-output=$procSymlinkConsoleLine",
    "proc-symlink-exit-code=$(if ($RequireProcSymlinkProof) { 0 } else { '' })",
    "proc-fd-proof-required=$([bool]$RequireProcFdProof)",
    "proc-fd-command=$procFdCommandLine",
    "proc-fd-console-output=$procFdConsoleLine",
    "proc-fd-fd0-output=$procFdFd0Line",
    "proc-fd-fd1-output=$procFdFd1Line",
    "proc-fd-fd2-output=$procFdFd2Line",
    "proc-fd-exit-code=$(if ($RequireProcFdProof) { 0 } else { '' })",
    "proc-self-proof-required=$([bool]$RequireProcSelfProof)",
    "proc-self-command=$procSelfCommandLine",
    "proc-self-console-output=$procSelfConsoleLine",
    "proc-self-environ-output=$procSelfEnvironLine",
    "proc-self-cmdline-output=$procSelfCmdlineLine",
    "proc-self-status-output=$procSelfStatusLine",
    "proc-self-fd-output=$procSelfFdLine",
    "proc-self-exe-output=$procSelfExeLine",
    "proc-self-maps-output=$procSelfMapsLine",
    "proc-self-exit-code=$(if ($RequireProcSelfProof) { 0 } else { '' })",
    "shell-command=$shellCommandLine",
    "shell-standalone-applets-required=$([bool]$RequireShellApplets)",
    "shell-cwd-loop-required=$([bool]$RequireShellCwdLoop)",
    "shell-fork-boundary-trace=$([bool]$TraceShellForkBoundary)",
    "shell-console-output=$shellBannerLine",
    'shell-prompt=$',
    "shell-loop-echo-output=$shellLoopEchoLine",
    "shell-loop-after-true-output=$shellLoopAfterTrueLine",
    "shell-loop-ls-output=$shellLoopLsLine",
    "shell-loop-cat-output=$shellLoopCatLine",
    "shell-cwd-root-output=$($shellCwdRootLines -join ' / ')",
    "shell-cwd-nvme-output=$shellCwdNvmeLine",
    "shell-fork-errors=$($shellForkErrorLines.Count)",
    "shell-exit-code=$(if ($TraceShellForkBoundary) { 2 } else { 0 })",
    "",
    "[file]",
    $fileOutput,
    "",
    "[readelf -h]",
    $readelfHeader,
    "",
    "[readelf -l]",
    $readelfProgramHeaders,
    "",
    "[negative missing /APPS/BUSYBOX]",
    $missingFileFailure,
    "",
    "[negative dynamic PT_INTERP]",
    $dynamicElfFailure,
    "",
    "[negative low-address ET_EXEC]",
    "required=$lowAddressNegativeRequired",
    "artifact=$lowAddressArtifact",
    $lowAddressFailure,
    "",
    "[negative oversized]",
    $oversizedFailure,
    "",
    "[negative BIOS unavailable]",
    $biosUnavailable,
    "",
    "[drs-realbin]",
    $realbinLines,
    "",
    "[drs-realbin cat /proc/meminfo]",
    $catRealbinLines,
    "",
    "[drs-realbin cat /nvme/apps/data/file.txt]",
    $nvmeRealbinLines,
    "",
    "[drs-realbin cat nvme/apps/./data/../data/file.txt]",
    $relativeNvmeRealbinLines,
    "",
    "[drs-realbin ls -l nvme/apps/./data/..]",
    $relativeLsRealbinLines,
    "",
    "[drs-realbin ls -l /proc/self/exe]",
    $procSymlinkRealbinLines,
    "",
    "[drs-realbin ls -l /proc/self/fd]",
    $procFdRealbinLines,
    "",
    "[drs-realbin ls -l /proc/self]",
    $procSelfRealbinLines,
    "",
    "[drs-realbin ls /nvme/apps]",
    $lsRealbinLines,
    "",
    "[drs-realbin sh]",
    $shellRealbinLines
)
Set-Content -Path $provenancePath -Value $provenance -Encoding Ascii

Write-Host "Real-binary gate passed."
Write-Host "  busybox : $resolvedBusyBox"
Write-Host "  sha256  : $busyBoxHash"
Write-Host "  output  : limitless-real-binary"
Write-Host "  cat     : $catConsoleLine"
Write-Host "  nvme cat: $nvmeConsoleLine"
if ($RequireRelativePathProof) {
    Write-Host "  rel cat : $relativeNvmeConsoleLine"
    Write-Host "  rel ls  : $relativeLsConsoleLine"
}
if ($RequireProcSymlinkProof) {
    Write-Host "  proc ln : $procSymlinkConsoleLine"
}
if ($RequireProcFdProof) {
    Write-Host "  proc fd : $procFdConsoleLine"
}
if ($RequireProcSelfProof) {
    Write-Host "  proc    : $procSelfConsoleLine"
}
Write-Host "  ls      : $lsConsoleLine"
Write-Host "  shell   : $shellBannerLine"
Write-Host '  prompt  : $'
if ($RequireShellApplets) {
    Write-Host "  loop    : $shellLoopEchoLine / $shellLoopLsLine / $shellLoopCatLine"
}
elseif ($RequireShellCwdLoop) {
    Write-Host "  loop    : $shellLoopEchoLine / $($shellCwdRootLines -join ' / ') / $shellCwdNvmeLine"
}
elseif ($TraceShellForkBoundary) {
    Write-Host "  loop    : $shellLoopEchoLine / fork-errors $($shellForkErrorLines.Count)"
}
else {
    Write-Host "  loop    : $shellLoopEchoLine / $shellLoopAfterTrueLine"
}
Write-Host "  lowaddr : $lowAddressFailure"
Write-Host "  proof   : $provenancePath"
