param(
    [ValidateSet("x86_64")]
    [string]$Architecture = "x86_64",

    [ValidateSet("uefi", "iso")]
    [string]$BootMedia = "uefi",

    [ValidateSet("Product", "Experimental")]
    [string]$BuildProfile = "Product"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$verifyQemu = Join-Path $scriptDir "verify-qemu.ps1"

function Assert-M15Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M15 installer UX verifier failed: expected '$Label 1' in drs-installer-ux telemetry."
    }
}

function Assert-Line
{
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Message
    )

    foreach ($line in $Lines) {
        if ($line -match $Pattern) {
            return
        }
    }

    throw $Message
}

$outputLines = @(& $verifyQemu -Architecture $Architecture -BootMedia $BootMedia -BuildProfile $BuildProfile 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$installerLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-installer-ux ' } | Select-Object -First 1
if (-not $installerLine) {
    throw "M15 installer UX verifier failed: no x64 drs-installer-ux telemetry line was observed."
}

foreach ($label in @(
    "drs-installer-ux-product",
    "drs-installer-welcome",
    "drs-installer-beginner-mode",
    "drs-installer-advanced-mode",
    "drs-installer-hardware-summary",
    "drs-installer-recommendation",
    "drs-installer-component-selection",
    "drs-installer-unavailable-components-labeled",
    "drs-installer-account-page",
    "drs-installer-personal-unavailable",
    "drs-installer-enterprise-unavailable",
    "drs-installer-cloud-page",
    "drs-installer-cloud-sync-unavailable",
    "drs-installer-ai-page",
    "drs-installer-ai-setup-unavailable",
    "drs-installer-plan-generated",
    "drs-installer-dryrun-no-writes",
    "drs-installer-forbidden-target-denied",
    "drs-installer-write-action-denied",
    "drs-installer-format-action-denied",
    "drs-installer-boot-entry-denied",
    "drs-installer-package-install-denied",
    "drs-installer-cloud-enable-denied",
    "drs-installer-ai-enable-denied",
    "drs-no-ambient-installer",
    "drs-no-ambient-installer-storage",
    "drs-no-ambient-installer-firmware",
    "drs-no-ambient-installer-package",
    "drs-no-ambient-installer-identity-cloud-secret"
)) {
    Assert-M15Label -Line $installerLine -Label $label
}

if ($installerLine -notmatch ' writes-planned 0 formats-planned 0 boot-entry-planned 0 package-ops-planned 0 real-install-approved 0 mode planning-dry-run-only profile general-use recommendation general-use-safe-profile components product-components-selected-unavailable-labeled account local-only-personal-enterprise-unavailable cloud cloud-sync-unavailable ai ai-assisted-setup-unavailable plan generated-zero-write-plan dryrun validated-no-writes') {
    throw "M15 installer UX verifier failed: install-plan status did not match the zero-write Product contract."
}

Assert-Line -Lines $outputLines -Pattern '^GUI desktop: Terminal File Manager Settings Installer( Assistant)?$' -Message "M15 installer UX verifier failed: apps output did not expose the Installer GUI entry."
Assert-Line -Lines $outputLines -Pattern '^Installer UX: launcher and Settings show dry-run planning; writes disabled$' -Message "M15 installer UX verifier failed: apps output did not describe dry-run installer planning."
Assert-Line -Lines $outputLines -Pattern '^Product installer UX: launcher and Settings show dry-run planning; writes, formatting, and boot-entry changes disabled$' -Message "M15 installer UX verifier failed: help output did not describe installer UX."
Assert-Line -Lines $outputLines -Pattern '^AI-assisted setup$' -Message "M15 installer UX verifier failed: AI-assisted setup was not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^Real internal installation and write access$' -Message "M15 installer UX verifier failed: real internal installation and write access was not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^Formatting$' -Message "M15 installer UX verifier failed: formatting was not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^Boot entry changes$' -Message "M15 installer UX verifier failed: boot-entry changes were not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^installer ux: planning and dry-run only$' -Message "M15 installer UX verifier failed: pkginfo did not expose installer UX status."
Assert-Line -Lines $outputLines -Pattern '^installer selected profile: general-use$' -Message "M15 installer UX verifier failed: pkginfo did not expose the selected plan profile."
Assert-Line -Lines $outputLines -Pattern '^installer writes planned: 0$' -Message "M15 installer UX verifier failed: pkginfo did not report zero write operations."
Assert-Line -Lines $outputLines -Pattern '^installer formats planned: 0$' -Message "M15 installer UX verifier failed: pkginfo did not report zero format operations."
Assert-Line -Lines $outputLines -Pattern '^installer boot entries planned: 0$' -Message "M15 installer UX verifier failed: pkginfo did not report zero boot-entry operations."
Assert-Line -Lines $outputLines -Pattern '^installer real install: not approved$' -Message "M15 installer UX verifier failed: pkginfo did not report real install as not approved."

Write-Host "M15 installer UX verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
