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

function Assert-M11Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M11 identity verifier failed: expected '$Label 1' in drs-identity telemetry."
    }
}

$outputLines = @(& $verifyQemu -Architecture $Architecture -BootMedia $BootMedia -BuildProfile $BuildProfile 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$identityLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-identity ' } | Select-Object -First 1
if (-not $identityLine) {
    throw "M11 identity verifier failed: no x64 drs-identity telemetry line was observed."
}

foreach ($label in @(
    "drs-identity-foundation",
    "drs-identity-local-active",
    "drs-identity-personal-unavailable",
    "drs-identity-enterprise-unavailable",
    "drs-identity-settings-panel",
    "drs-identity-status-readonly",
    "drs-identity-mutation-denied",
    "drs-vault-foundation",
    "drs-vault-secret-read-denied",
    "drs-vault-secret-write-denied",
    "drs-vault-no-plaintext-token",
    "drs-cloud-association-unavailable",
    "drs-no-ambient-identity",
    "drs-no-ambient-secret"
)) {
    Assert-M11Label -Line $identityLine -Label $label
}

if ($identityLine -notmatch ' encrypted-vault 0 secret-storage 0 account-type local account-id local:limitless display limitless association local-active network offline-capable credential bcrypt-local vault metadata-only') {
    throw "M11 identity verifier failed: identity/vault state did not match the M11 Product contract."
}

Write-Host "M11 identity/vault verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
