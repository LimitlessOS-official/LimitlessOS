param(
    [ValidateSet("x86_64")]
    [string]$Architecture = "x86_64",

    [ValidateSet("disk", "uefi", "iso")]
    [string]$BootMedia = "uefi",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$verifyQemu = Join-Path $PSScriptRoot "verify-qemu.ps1"
$output = & $verifyQemu -Architecture $Architecture -BootMedia $BootMedia -BuildProfile $BuildProfile
if (-not $?) {
    throw "M9 hardware-validation verifier failed: verify-qemu failed."
}

$lines = @($output)
function Assert-Line
{
    param(
        [string]$Pattern,
        [string]$Message
    )

    if (-not ($lines | Where-Object { $_ -match $Pattern } | Select-Object -First 1)) {
        throw "M9 hardware-validation verifier failed: $Message"
    }
}

Assert-Line -Pattern '^\[x64\] \$ hwval$' -Message "hwval command was not observed."
Assert-Line -Pattern '^hardware validation: read-only Product mode$' -Message "hwval did not report read-only Product mode."
Assert-Line -Pattern '^authority: read-only scoped validation; no ambient storage/installer/network/update/install$' -Message "hwval did not report scoped read-only authority."
Assert-Line -Pattern '^internal writes: disabled by default$' -Message "internal writes were not reported disabled."
Assert-Line -Pattern '^format authority: unavailable$' -Message "format authority was not reported unavailable."
Assert-Line -Pattern '^nvram boot-entry authority: unavailable$' -Message "NVRAM authority was not reported unavailable."
Assert-Line -Pattern '^real install: not approved$' -Message "real install approval was not reported as not approved."
Assert-Line -Pattern '\[x64\] drs-hwval drs-hwval-product 1 drs-hwval-readonly 1 drs-hwval-no-internal-write 1 drs-hwval-no-format 1 drs-hwval-no-nvram 1 drs-hwval-storage-enumeration-scoped 1 drs-hwval-network-status-scoped 1 drs-hwval-package-status-scoped 1 drs-hwval-installer-dryrun-only 1 drs-hwval-msi-checklist-present 1' -Message "drs-hwval Product/read-only proof was missing."

Write-Host "M9 hardware-validation verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
$lines
