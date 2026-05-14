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

function Assert-M14Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M14 cloud storage verifier failed: expected '$Label 1' in drs-cloud telemetry."
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

$cloudLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-cloud ' } | Select-Object -First 1
if (-not $cloudLine) {
    throw "M14 cloud storage verifier failed: no x64 drs-cloud telemetry line was observed."
}

foreach ($label in @(
    "drs-cloud-broker-product",
    "drs-cloud-provider-descriptor",
    "drs-cloud-provider-verified",
    "drs-cloud-provider-missing-sig-denied",
    "drs-cloud-provider-invalid-sig-denied",
    "drs-cloud-provider-wrong-key-denied",
    "drs-cloud-provider-tamper-denied",
    "drs-cloud-provider-rollback-denied",
    "drs-cloud-provider-version-denied",
    "drs-cloud-provider-malformed-denied",
    "drs-cloud-association-unavailable",
    "drs-cloud-account-unavailable",
    "drs-cloud-token-storage-denied",
    "drs-cloud-encrypted-transport-unavailable",
    "drs-cloud-upload-denied",
    "drs-cloud-download-denied",
    "drs-cloud-sync-denied",
    "drs-cloud-auto-upload-unavailable",
    "drs-cloud-auto-download-unavailable",
    "drs-cloud-ai-access-unavailable",
    "drs-cloud-app-direct-denied",
    "drs-cloud-settings-panel",
    "drs-cloud-settings-readonly",
    "drs-cloud-fileman-status",
    "drs-cloud-fileman-mutation-denied",
    "drs-no-ambient-cloud",
    "drs-no-ambient-cloud-fs",
    "drs-no-ambient-cloud-network",
    "drs-no-ambient-cloud-identity",
    "drs-no-ambient-cloud-secret"
)) {
    Assert-M14Label -Line $cloudLine -Label $label
}

if ($cloudLine -notmatch ' mode foundation-active storage-mode unavailable-policy-only provider cloud\.fixture\.limitless descriptor signed-local-fixture-verified account unavailable-planned association unavailable-planned token-storage denied-vault-mode-b encrypted unavailable sync unavailable upload denied download denied offline-cache planned-unavailable ai unavailable app-direct denied') {
    throw "M14 cloud storage verifier failed: cloud broker foundation status/details did not match the Product contract."
}

Assert-Line -Lines $outputLines -Pattern '^Cloud storage status: Settings/File Manager; unavailable/planned; no sync$' -Message "M14 cloud storage verifier failed: apps output did not expose truthful cloud storage status."
Assert-Line -Lines $outputLines -Pattern '^cloud storage broker: foundation active$' -Message "M14 cloud storage verifier failed: pkginfo did not report cloud broker foundation status."
Assert-Line -Lines $outputLines -Pattern '^cloud provider descriptor: signed local fixture verified$' -Message "M14 cloud storage verifier failed: pkginfo did not report signed cloud-provider descriptor verification."
Assert-Line -Lines $outputLines -Pattern '^cloud storage mode: unavailable/planned$' -Message "M14 cloud storage verifier failed: pkginfo did not report cloud storage mode unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud token storage: denied while vault Mode B$' -Message "M14 cloud storage verifier failed: pkginfo did not report token storage denial."
Assert-Line -Lines $outputLines -Pattern '^cloud encrypted transport: unavailable$' -Message "M14 cloud storage verifier failed: pkginfo did not report encrypted transport unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud sync: unavailable$' -Message "M14 cloud storage verifier failed: pkginfo did not report cloud sync unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud upload/download: denied$' -Message "M14 cloud storage verifier failed: pkginfo did not report upload/download denial."
Assert-Line -Lines $outputLines -Pattern '^cloud auto-upload/download: unavailable$' -Message "M14 cloud storage verifier failed: pkginfo did not report automatic upload/download unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud AI access: unavailable$' -Message "M14 cloud storage verifier failed: pkginfo did not report AI cloud access unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud app direct authority: denied$' -Message "M14 cloud storage verifier failed: pkginfo did not report app direct cloud authority denied."
Assert-Line -Lines $outputLines -Pattern '^no ambient install/update/network/cloud/fs/identity/secret(/ai)?$' -Message "M14 cloud storage verifier failed: pkginfo did not report no ambient cloud/filesystem/network/identity/secret authority."

Write-Host "M14 cloud storage verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
