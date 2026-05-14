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

function Assert-M16Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M16 AI policy verifier failed: expected '$Label 1' in drs-ai telemetry."
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

$aiLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-ai ' } | Select-Object -First 1
if (-not $aiLine) {
    throw "M16 AI policy verifier failed: no x64 drs-ai telemetry line was observed."
}

foreach ($label in @(
    "drs-ai-principal",
    "drs-ai-request-created",
    "drs-ai-consent-required",
    "drs-ai-denied-no-consent",
    "drs-ai-scope-validated",
    "drs-ai-invalid-scope-denied",
    "drs-ai-audit-recorded",
    "drs-ai-settings-panel",
    "drs-ai-settings-readonly",
    "drs-ai-no-ambient-authority",
    "drs-ai-no-filesystem-access",
    "drs-ai-no-network-access",
    "drs-ai-no-settings-access",
    "drs-ai-no-package-access",
    "drs-ai-no-secret-access",
    "drs-ai-no-cloud-access"
)) {
    Assert-M16Label -Line $aiLine -Label $label
}

if ($aiLine -notmatch ' default-caps 0 actions-executed 0 audit-records [1-9][0-9]* mode request-deny-audit-only principal request-only-no-default-capabilities action read-file resource /README\.TXT capability fs-read scope file decision deny result denied-no-consent assistant unavailable automation unavailable cloud-ai unavailable') {
    throw "M16 AI policy verifier failed: AI request/deny/audit detail did not match the no-action Product contract."
}

Assert-Line -Lines $outputLines -Pattern '^Product AI policy: Settings/pkginfo show request-deny-audit only; AI actions unavailable$' -Message "M16 AI policy verifier failed: help output did not describe request/deny/audit-only AI policy."
Assert-Line -Lines $outputLines -Pattern '^AI policy: Settings/pkginfo; request-deny-audit only; no actions$' -Message "M16 AI policy verifier failed: apps output did not label AI policy as no-action."
Assert-Line -Lines $outputLines -Pattern '^ai policy broker: foundation active$' -Message "M16 AI policy verifier failed: pkginfo did not expose AI policy broker status."
Assert-Line -Lines $outputLines -Pattern '^ai principal: request-only no default capabilities$' -Message "M16 AI policy verifier failed: pkginfo did not expose the request-only principal."
Assert-Line -Lines $outputLines -Pattern '^ai action request: modeled$' -Message "M16 AI policy verifier failed: pkginfo did not expose the action request model."
Assert-Line -Lines $outputLines -Pattern '^ai consent: required no auto-approve$' -Message "M16 AI policy verifier failed: pkginfo did not expose consent requirement."
Assert-Line -Lines $outputLines -Pattern '^ai actions: unavailable$' -Message "M16 AI policy verifier failed: pkginfo did not label AI actions unavailable."
Assert-Line -Lines $outputLines -Pattern '^ai audit: immutable queryable settings-visible$' -Message "M16 AI policy verifier failed: pkginfo did not expose audit visibility."
Assert-Line -Lines $outputLines -Pattern '^ai filesystem access: denied$' -Message "M16 AI policy verifier failed: AI filesystem denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai network access: denied$' -Message "M16 AI policy verifier failed: AI network denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai settings access: denied$' -Message "M16 AI policy verifier failed: AI settings denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai package access: denied$' -Message "M16 AI policy verifier failed: AI package denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai secret access: denied$' -Message "M16 AI policy verifier failed: AI secret denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai cloud access: denied$' -Message "M16 AI policy verifier failed: AI cloud denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai assistant: unavailable$' -Message "M16 AI policy verifier failed: AI assistant was not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^ai automation: unavailable$' -Message "M16 AI policy verifier failed: AI automation was not labeled unavailable."
Assert-Line -Lines $outputLines -Pattern '^no ambient install/update/network/cloud/fs/identity/secret/ai$' -Message "M16 AI policy verifier failed: no-ambient AI authority status was not visible."

Write-Host "M16 AI policy verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
