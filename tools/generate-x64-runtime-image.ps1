param(
    [Parameter(Mandatory = $true)]
    [string]$InputAsmPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputBinPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputHeaderPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$outputDir = Split-Path -Parent $OutputBinPath
$headerDir = Split-Path -Parent $OutputHeaderPath
New-Item -ItemType Directory -Force -Path $outputDir, $headerDir | Out-Null

$maximumImageBytes = 16384

& nasm -f bin $InputAsmPath -o $OutputBinPath
if ($LASTEXITCODE -ne 0) {
    throw "Failed to assemble x86_64 sealed runtime image."
}

[byte[]]$bytes = [System.IO.File]::ReadAllBytes($OutputBinPath)
if (($bytes.Length -le 0) -or (($bytes.Length % 4096) -ne 0) -or ($bytes.Length -gt $maximumImageBytes)) {
    throw "x86_64 sealed runtime image must be non-empty, page-aligned, and no larger than $maximumImageBytes bytes. Actual length: $($bytes.Length)"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("#ifndef LIMITLESS_RUNTIME_IMAGE_X64_GENERATED_H")
$lines.Add("#define LIMITLESS_RUNTIME_IMAGE_X64_GENERATED_H")
$lines.Add("")
$lines.Add("static const u8 g_runtime64_transfer_image[RUNTIME64_TRANSFER_IMAGE_BYTES] __attribute__((aligned(4096))) = {")

for ($offset = 0; $offset -lt $bytes.Length; $offset += 12) {
    $count = [Math]::Min(12, $bytes.Length - $offset)
    $items = @()
    for ($index = 0; $index -lt $count; ++$index) {
        $items += ("0x{0:X2}u" -f $bytes[$offset + $index])
    }

    $suffix = if (($offset + $count) -lt $bytes.Length) { "," } else { "" }
    $lines.Add("    " + ($items -join ", ") + $suffix)
}

$lines.Add("};")
$lines.Add("")
$lines.Add("#endif")

Set-Content -Path $OutputHeaderPath -Value $lines -Encoding Ascii

Write-Host "Generated x86_64 sealed runtime image"
Write-Host "  input asm : $InputAsmPath"
Write-Host "  output bin: $OutputBinPath"
Write-Host "  header    : $OutputHeaderPath"
Write-Host "  size      : $($bytes.Length) bytes"
