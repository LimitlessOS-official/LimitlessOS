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

function Assert-M13Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M13 account association verifier failed: expected '$Label 1' in drs-account telemetry."
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

$accountLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-account ' } | Select-Object -First 1
if (-not $accountLine) {
    throw "M13 account association verifier failed: no x64 drs-account telemetry line was observed."
}

foreach ($label in @(
    "drs-account-association-product",
    "drs-account-local-active",
    "drs-account-personal-unavailable",
    "drs-account-enterprise-unavailable",
    "drs-account-cloud-unavailable",
    "drs-account-security-key-unavailable",
    "drs-account-settings-panel",
    "drs-account-status-readonly",
    "drs-account-mutation-denied",
    "drs-account-unlink-denied",
    "drs-account-token-storage-denied",
    "drs-account-credential-transport-denied",
    "drs-account-enterprise-policy-unavailable",
    "drs-account-remote-no-ambient-authority",
    "drs-no-ambient-account-identity",
    "drs-no-ambient-account-network",
    "drs-no-ambient-account-secret"
)) {
    Assert-M13Label -Line $accountLine -Label $label
}

if ($accountLine -notmatch ' mode mode-b-status-only local active personal planned-unavailable enterprise planned-unavailable cloud planned-unavailable security-key planned-unavailable enterprise-policy unavailable encrypted unavailable token-storage denied trusted-time unavailable remote-login unavailable local-user local:limitless provider personal\.fixture\.limitless') {
    throw "M13 account association verifier failed: Mode B account association status/details did not match the Product contract."
}

Assert-Line -Lines $outputLines -Pattern '^account association mode: status only$' -Message "M13 account association verifier failed: pkginfo did not report status-only mode."
Assert-Line -Lines $outputLines -Pattern '^local association: active/offline-capable$' -Message "M13 account association verifier failed: pkginfo did not report local association active."
Assert-Line -Lines $outputLines -Pattern '^personal association: unavailable$' -Message "M13 account association verifier failed: pkginfo did not report personal association unavailable."
Assert-Line -Lines $outputLines -Pattern '^enterprise association: unavailable$' -Message "M13 account association verifier failed: pkginfo did not report enterprise association unavailable."
Assert-Line -Lines $outputLines -Pattern '^cloud association: unavailable$' -Message "M13 account association verifier failed: pkginfo did not report cloud association unavailable."
Assert-Line -Lines $outputLines -Pattern '^security key login: unavailable$' -Message "M13 account association verifier failed: pkginfo did not report security-key status."
Assert-Line -Lines $outputLines -Pattern '^remote account authority: none$' -Message "M13 account association verifier failed: pkginfo did not report no remote account authority."
Assert-Line -Lines $outputLines -Pattern '^Identity/account/vault/transport status: Settings; local only; no secret storage$' -Message "M13 account association verifier failed: apps output did not expose truthful account status."

Write-Host "M13 account association verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
