# Locates the LimitlessOS host toolchain for the current PowerShell process.
#
# Dot-source this file from build and verification scripts. It leaves PATH
# untouched when the tools are already resolvable, and otherwise prepends the
# standard MSYS2 MINGW64 and QEMU install directories. Nothing is persisted.
#
# Required tools: gcc, ld, objcopy, nm, size (mingw-w64 binutils), nasm.
# QEMU (qemu-system-x86_64 plus share\edk2-x86_64-code.fd) is needed only by
# the verifiers. See docs\development.md for install commands.

$limitlessToolchainCandidates = @(@(
    $env:LIMITLESS_MINGW_BIN,
    "C:\msys64\mingw64\bin"
) | Where-Object { $_ -and (Test-Path (Join-Path $_ "gcc.exe")) })

if (-not (Get-Command gcc -ErrorAction SilentlyContinue) -and $limitlessToolchainCandidates) {
    $env:PATH = "$($limitlessToolchainCandidates[0]);$env:PATH"
}

$limitlessQemuDir = "C:\Program Files\qemu"
if (-not (Get-Command qemu-system-x86_64 -ErrorAction SilentlyContinue) -and (Test-Path (Join-Path $limitlessQemuDir "qemu-system-x86_64.exe"))) {
    $env:PATH = "$limitlessQemuDir;$env:PATH"
}

function Assert-LimitlessToolchain
{
    $missing = @("gcc", "ld", "objcopy", "nm", "size", "nasm") |
        Where-Object { -not (Get-Command $_ -ErrorAction SilentlyContinue) }
    if ($missing) {
        throw ("LimitlessOS toolchain is incomplete; missing: {0}. Install MSYS2 and run 'pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils mingw-w64-x86_64-nasm' (see docs\development.md)." -f ($missing -join ", "))
    }
}
