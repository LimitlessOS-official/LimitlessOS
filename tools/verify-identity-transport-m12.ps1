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

function Assert-M12Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M12 identity transport verifier failed: expected '$Label 1' in drs-idtransport telemetry."
    }
}

$outputLines = @(& $verifyQemu -Architecture $Architecture -BootMedia $BootMedia -BuildProfile $BuildProfile 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$transportLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-idtransport ' } | Select-Object -First 1
if (-not $transportLine) {
    throw "M12 identity transport verifier failed: no x64 drs-idtransport telemetry line was observed."
}

foreach ($label in @(
    "drs-idtransport-product",
    "drs-idtransport-provider-descriptor",
    "drs-idtransport-descriptor-verified",
    "drs-idtransport-descriptor-missing-sig-denied",
    "drs-idtransport-descriptor-invalid-sig-denied",
    "drs-idtransport-descriptor-wrong-key-denied",
    "drs-idtransport-descriptor-tamper-denied",
    "drs-idtransport-descriptor-rollback-denied",
    "drs-idtransport-descriptor-version-denied",
    "drs-idtransport-network-scoped",
    "drs-idtransport-no-network-cap-denied",
    "drs-idtransport-plaintext-credential-denied",
    "drs-idtransport-unverified-endpoint-denied",
    "drs-idtransport-token-storage-denied",
    "drs-idtransport-personal-unavailable",
    "drs-idtransport-enterprise-unavailable",
    "drs-idtransport-cloud-association-unavailable",
    "drs-idtransport-settings-panel",
    "drs-idtransport-status-readonly",
    "drs-idtransport-trusted-time-status",
    "drs-no-ambient-idtransport-network",
    "drs-no-ambient-idtransport-identity",
    "drs-no-ambient-idtransport-secret",
    "drs-idtransport-encrypted-channel-unavailable",
    "drs-idtransport-credential-transport-unavailable"
)) {
    Assert-M12Label -Line $transportLine -Label $label
}

if ($transportLine -notmatch ' mode mode-b-descriptor-foundation provider personal\.fixture\.limitless provider-type personal endpoint descriptor-verified online offline-fixture encrypted unavailable credential denied token-storage denied trusted-time unavailable') {
    throw "M12 identity transport verifier failed: Mode B status/details did not match the Product contract."
}

Write-Host "M12 identity transport verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
