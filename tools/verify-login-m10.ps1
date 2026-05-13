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

function Assert-M10Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M10 login verifier failed: expected '$Label 1' in drs-login telemetry."
    }
}

$outputLines = @(& $verifyQemu -Architecture $Architecture -BootMedia $BootMedia -BuildProfile $BuildProfile 2>&1)
$exitCode = $LASTEXITCODE
$outputLines | ForEach-Object { $_ }
if ($exitCode -ne 0) {
    exit $exitCode
}

$loginLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-login ' } | Select-Object -First 1
if (-not $loginLine) {
    throw "M10 login verifier failed: no x64 drs-login telemetry line was observed."
}

foreach ($label in @(
    "drs-login-screen",
    "drs-login-auth-success",
    "drs-login-wrong-password-denied",
    "drs-login-rate-limited",
    "drs-session-lock",
    "drs-session-unlock",
    "drs-session-authority-scoped",
    "user-store-nvme",
    "user-store-persistent",
    "bcrypt-hash",
    "login-display-only",
    "login-input-only",
    "desktop-blocked-pre-auth"
)) {
    Assert-M10Label -Line $loginLine -Label $label
}

if ($loginLine -notmatch ' user limitless home /HOME/LIMITLESS profile local-console') {
    throw "M10 login verifier failed: authenticated local-console principal/home grant was not observed."
}

if (-not ($outputLines | Where-Object { $_ -match '^session unlocked$' } | Select-Object -First 1)) {
    throw "M10 login verifier failed: lock command did not unlock and resume the session."
}

Write-Host "M10 login verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
