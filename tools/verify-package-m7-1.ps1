param(
    [ValidateSet("Package", "Update", "All")]
    [string]$Mode = "All",

    [ValidateSet("virtio", "e1000e", "e1000")]
    [string]$NetworkDevice = "virtio"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$verifyQemu = Join-Path $scriptDir "verify-qemu.ps1"

function Assert-M7Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M7.1 fixture verifier failed: expected '$Label 1' in drs-pkg telemetry."
    }
}

$outputLines = @(& $verifyQemu -Architecture x86_64 -BootMedia uefi -NetworkDevice $NetworkDevice -BuildProfile Product 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$pkgLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-pkg ' } | Select-Object -First 1
if (-not $pkgLine) {
    throw "M7.1 fixture verifier failed: no x64 drs-pkg telemetry line was observed."
}

$packageLabels = @(
    "drs-pkg-signed",
    "drs-pkg-verified",
    "drs-pkg-invalid-denied",
    "drs-pkg-missing-sig-denied",
    "drs-pkg-wrong-key-denied",
    "drs-pkg-manifest-tamper-denied",
    "drs-pkg-payload-tamper-denied",
    "drs-pkg-checksum-mismatch-denied",
    "drs-pkg-unsupported-version-denied",
    "drs-pkg-duplicate-denied",
    "drs-pkg-downgrade-denied",
    "drs-pkg-wrong-owner-denied",
    "drs-pkg-stale-token-denied",
    "drs-pkg-cap-policy-denied",
    "drs-pkg-malformed-denied",
    "drs-pkg-oversized-denied",
    "drs-pkg-install-no-cap-denied",
    "drs-pkg-install-scoped"
)

$updateLabels = @(
    "drs-pkg-update-check",
    "drs-pkg-update-index-verified",
    "drs-pkg-update-index-unsigned-denied",
    "drs-pkg-update-index-tamper-denied",
    "drs-pkg-update-index-wrong-key-denied",
    "drs-pkg-update-index-rollback-denied",
    "drs-pkg-update-index-replay-handled",
    "drs-pkg-update-no-network-cap-denied",
    "drs-pkg-update-apply-no-install-cap-denied",
    "drs-pkg-update-no-ambient",
    "drs-pkg-update-no-auto-install"
)

if (($Mode -eq "Package") -or ($Mode -eq "All")) {
    foreach ($label in $packageLabels) {
        Assert-M7Label -Line $pkgLine -Label $label
    }
}

if (($Mode -eq "Update") -or ($Mode -eq "All")) {
    foreach ($label in $updateLabels) {
        Assert-M7Label -Line $pkgLine -Label $label
    }
}

Write-Host "M7.1 $Mode negative fixture verifier passed."
