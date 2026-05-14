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

function Assert-M18Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M18 AI action verifier failed: expected '$Label 1' in drs-ai-action telemetry."
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

$actionLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-ai-action ' } | Select-Object -First 1
if (-not $actionLine) {
    throw "M18 AI action verifier failed: no x64 drs-ai-action telemetry line was observed."
}

foreach ($label in @(
    "drs-ai-action-mode-product",
    "drs-ai-action-request-created",
    "drs-ai-action-consent-required",
    "drs-ai-action-denied-no-effect",
    "drs-ai-action-approved-scoped-cap",
    "drs-ai-action-note-write",
    "drs-ai-action-note-commit",
    "drs-ai-action-note-readback",
    "drs-ai-action-arbitrary-write-denied",
    "drs-ai-action-path-traversal-denied",
    "drs-ai-action-stale-grant-denied",
    "drs-ai-action-wrong-session-denied",
    "drs-ai-action-installer-dryrun",
    "drs-ai-action-installer-dryrun-no-writes",
    "drs-ai-action-open-settings",
    "drs-ai-action-package-status",
    "drs-ai-action-settings-mutation-denied",
    "drs-ai-action-package-install-denied",
    "drs-ai-action-update-apply-denied",
    "drs-ai-action-cloud-enable-denied",
    "drs-ai-action-secret-denied",
    "drs-ai-action-self-modification-denied",
    "drs-ai-action-audit-recorded",
    "drs-ai-action-no-autonomy",
    "drs-ai-action-no-model-call",
    "drs-ai-action-no-fake-response",
    "drs-no-ambient-ai-action-fs",
    "drs-no-ambient-ai-action-installer",
    "drs-no-ambient-ai-action-settings",
    "drs-no-ambient-ai-action-package",
    "drs-no-ambient-ai-action-cloud",
    "drs-no-ambient-ai-action-secret",
    "drs-no-ambient-ai-action-network"
)) {
    Assert-M18Label -Line $actionLine -Label $label
}

if ($actionLine -notmatch ' action-id 18 note-bytes [1-9][0-9]* audit-records [1-9][0-9]* mode mode-b-deterministic-action-templates templates assistant-note-write,installer-dryrun,open-settings-panel,package-trust-status forbidden package-install,package-update,settings-mutation,cloud-enable,secret-token,model-transport,self-modification note-path /HOME/ASSIST/NOTE\.TXT consent allow-once-write-readonly-session-dryrun-deny grant session-bound-action-id-target-bound-expired result note-committed-readback-dryrun-status-opened-audited') {
    throw "M18 AI action verifier failed: action broker detail did not match the Mode B scoped-action contract."
}

Assert-Line -Lines $outputLines -Pattern '^Product AI assistant: launcher/Settings/pkginfo show consent-scoped action templates; inference unavailable$' -Message "M18 AI action verifier failed: help output did not describe action templates truthfully."
Assert-Line -Lines $outputLines -Pattern '^AI Assistant: launcher/Settings/pkginfo; consent-scoped action templates; inference unavailable$' -Message "M18 AI action verifier failed: apps output did not expose action templates truthfully."
Assert-Line -Lines $outputLines -Pattern '^ai actions: consent-scoped templates only$' -Message "M18 AI action verifier failed: pkginfo did not expose consent-scoped action status."
Assert-Line -Lines $outputLines -Pattern '^ai action broker: Mode B deterministic templates$' -Message "M18 AI action verifier failed: pkginfo did not expose Mode B action broker status."
Assert-Line -Lines $outputLines -Pattern '^ai action templates: assistant-note-write installer-dryrun open-settings-panel package-trust-status$' -Message "M18 AI action verifier failed: allowed action templates were not visible."
Assert-Line -Lines $outputLines -Pattern '^ai forbidden actions: package-install package-update settings-mutation cloud-enable secret-token model-transport self-modification$' -Message "M18 AI action verifier failed: forbidden action templates were not visible."
Assert-Line -Lines $outputLines -Pattern '^ai note action: /HOME/ASSIST/NOTE\.TXT committed readback verified$' -Message "M18 AI action verifier failed: assistant note action status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai action consent: allow once required for write; read-only session allowed for status$' -Message "M18 AI action verifier failed: action consent status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai action grant: scoped session-bound action-bound target-bound expired$' -Message "M18 AI action verifier failed: action grant scoping was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai action audit: request consent grant result revocation recorded$' -Message "M18 AI action verifier failed: action audit status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai autonomous actions: unavailable$' -Message "M18 AI action verifier failed: no-autonomy status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai model call: none$' -Message "M18 AI action verifier failed: no-model-call status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai scripted response: none$' -Message "M18 AI action verifier failed: scripted-response absence was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai self-modification: denied$' -Message "M18 AI action verifier failed: self-modification denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^no ambient install/update/network/cloud/fs/identity/secret/ai$' -Message "M18 AI action verifier failed: no-ambient AI authority status was not visible."

Write-Host "M18 AI consent-scoped action verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
