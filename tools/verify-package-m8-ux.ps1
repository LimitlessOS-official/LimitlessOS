param(
    [ValidateSet("uefi", "iso")]
    [string]$BootMedia = "uefi"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$verifyQemu = Join-Path $root "tools\verify-qemu.ps1"

function Assert-Line
{
    param(
        [string[]]$Lines,
        [string]$Pattern,
        [string]$Message
    )

    foreach ($line in $Lines) {
        if ($line -match $Pattern) {
            return
        }
    }

    throw $Message
}

$lines = @(& $verifyQemu -Architecture x86_64 -BootMedia $BootMedia -BuildProfile Product)
if ($LASTEXITCODE -ne 0) {
    throw "M8 package UX verifier failed: verify-qemu exited with $LASTEXITCODE."
}

$statusLine = $lines | Where-Object { $_ -match '^\[x64\] drs-pkg-status ' } | Select-Object -First 1
if (-not $statusLine) {
    throw "M8 package UX verifier failed: no drs-pkg-status telemetry line was observed."
}

foreach ($label in @(
    "drs-pkg-settings-panel",
    "drs-pkg-settings-readonly",
    "drs-pkg-status-visible",
    "drs-pkg-status-signer-visible",
    "drs-pkg-status-capabilities-visible",
    "drs-pkg-status-update-index-visible",
    "drs-pkg-status-no-auto-install-visible",
    "drs-pkg-status-public-fetch-unavailable",
    "drs-pkg-status-trusted-time-unavailable",
    "drs-pkg-status-no-ambient-install",
    "drs-pkg-status-no-ambient-update",
    "drs-pkg-status-no-ambient-network",
    "drs-pkg-settings-write-denied",
    "drs-pkg-install-action-unavailable",
    "drs-pkg-update-apply-unavailable"
)) {
    if ($statusLine -notmatch ("{0} 1" -f [regex]::Escape($label))) {
        throw "M8 package UX verifier failed: expected '$label 1' in drs-pkg-status telemetry."
    }
}

Assert-Line -Lines $lines -Pattern '^\[x64\] \$ pkginfo$' -Message "M8 package UX verifier failed: pkginfo command was not observed."
Assert-Line -Lines $lines -Pattern '^package system: enabled on UEFI Product$' -Message "M8 package UX verifier failed: UEFI package system status was not visible."
Assert-Line -Lines $lines -Pattern '^trusted public key fingerprint: [0-9A-F]{64}$' -Message "M8 package UX verifier failed: signer fingerprint was not visible."
Assert-Line -Lines $lines -Pattern '^capability requests: visible; policy enforced$' -Message "M8 package UX verifier failed: capability request status was not visible."
Assert-Line -Lines $lines -Pattern '^update-index: local signed fixture verified$' -Message "M8 package UX verifier failed: update-index status was not visible."
Assert-Line -Lines $lines -Pattern '^auto-install: unavailable$' -Message "M8 package UX verifier failed: no-auto-install status was not visible."
Assert-Line -Lines $lines -Pattern '^public update fetch: unavailable/non-product$' -Message "M8 package UX verifier failed: public update fetch status was not visible."
Assert-Line -Lines $lines -Pattern '^trusted-time expiry: unavailable/non-product$' -Message "M8 package UX verifier failed: trusted-time status was not visible."
Assert-Line -Lines $lines -Pattern '^install authority: disabled in M(8|9|10|11); scoped capability required$' -Message "M8 package UX verifier failed: install authority status was not visible."
Assert-Line -Lines $lines -Pattern '^update-apply authority: disabled in M(8|9|10|11); scoped install required$' -Message "M8 package UX verifier failed: update-apply authority status was not visible."
Assert-Line -Lines $lines -Pattern '^no ambient install/update/network$' -Message "M8 package UX verifier failed: no ambient authority status was not visible."

Write-Host "M8 package UX verifier passed for $BootMedia."
