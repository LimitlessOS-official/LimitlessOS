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

function Assert-M17Label
{
    param(
        [Parameter(Mandatory = $true)][string]$Line,
        [Parameter(Mandatory = $true)][string]$Label
    )

    if ($Line -notmatch ("(^| )" + [regex]::Escape($Label) + " 1( |$)")) {
        throw "M17 AI Assistant verifier failed: expected '$Label 1' in drs-ai-assistant telemetry."
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

$assistantLine = $outputLines | Where-Object { $_ -match '^\[x64\] drs-ai-assistant ' } | Select-Object -First 1
if (-not $assistantLine) {
    throw "M17 AI Assistant verifier failed: no x64 drs-ai-assistant telemetry line was observed."
}

foreach ($label in @(
    "drs-ai-assistant-product",
    "drs-ai-assistant-app-opened",
    "drs-ai-assistant-blocked-preauth",
    "drs-ai-assistant-backend-mode",
    "drs-ai-assistant-zero-default-caps",
    "drs-ai-context-request",
    "drs-ai-context-consent-required",
    "drs-ai-context-denied-no-data",
    "drs-ai-context-allowed-scoped-read",
    "drs-ai-context-invalid-scope-denied",
    "drs-ai-context-broad-fs-denied",
    "drs-ai-context-secret-denied",
    "drs-ai-context-cloud-denied",
    "drs-ai-file-write-denied",
    "drs-ai-settings-mutation-denied",
    "drs-ai-package-mutation-denied",
    "drs-ai-network-denied-or-scoped",
    "drs-ai-stale-grant-denied",
    "drs-ai-wrong-session-denied",
    "drs-ai-audit-query",
    "drs-ai-settings-panel",
    "drs-ai-actions-unavailable",
    "drs-ai-automation-unavailable",
    "drs-ai-cloud-memory-unavailable",
    "drs-ai-self-modification-denied",
    "drs-ai-package-integrity",
    "drs-no-ambient-ai-fs",
    "drs-no-ambient-ai-network",
    "drs-no-ambient-ai-settings",
    "drs-no-ambient-ai-package",
    "drs-no-ambient-ai-secret",
    "drs-no-ambient-ai-cloud",
    "drs-ai-inference-unavailable",
    "drs-ai-no-model-call",
    "drs-ai-no-fake-response"
)) {
    Assert-M17Label -Line $assistantLine -Label $label
}

if ($assistantLine -notmatch ' default-caps 0 actions-executed 0 request-id 17 allowed-bytes [1-9][0-9]* denied-bytes 0 audit-records [1-9][0-9]* backend mode-b-host-consent-context-only inference unavailable-no-model-call context system-status resource settings-ai-policy-summary scope session-readonly-status-only reason explain-current-ai-safety-state capability status-read decision allow-once-readonly-fixture-and-deny-sensitive result scoped-read-visible-denied-sensitive-no-action egress none-no-backend package signed-product-component-integrity-checked selfmod denied cloud-memory unavailable') {
    throw "M17 AI Assistant verifier failed: Assistant Mode B context/consent/audit detail did not match the Product read-only contract."
}

Assert-Line -Lines $outputLines -Pattern '^Product AI assistant: launcher/Settings/pkginfo show (read-only consent flow|consent-scoped action templates); inference unavailable$' -Message "M17 AI Assistant verifier failed: help output did not describe read-only Assistant Mode B."
Assert-Line -Lines $outputLines -Pattern '^AI Assistant: launcher/Settings/pkginfo; (read-only consent flow|consent-scoped action templates); inference unavailable$' -Message "M17 AI Assistant verifier failed: apps output did not expose Assistant truthfully."
Assert-Line -Lines $outputLines -Pattern '^GUI desktop: Terminal File Manager Settings Installer Assistant$' -Message "M17 AI Assistant verifier failed: Assistant was not listed in Product GUI apps."
Assert-Line -Lines $outputLines -Pattern '^ai assistant: host active; inference unavailable$' -Message "M17 AI Assistant verifier failed: pkginfo did not expose Assistant host status."
Assert-Line -Lines $outputLines -Pattern '^ai backend mode: consent host only; inference unavailable$' -Message "M17 AI Assistant verifier failed: pkginfo did not expose consent-host backend status."
Assert-Line -Lines $outputLines -Pattern '^ai context request: read-only system status scoped$' -Message "M17 AI Assistant verifier failed: pkginfo did not expose context request status."
Assert-Line -Lines $outputLines -Pattern '^ai context consent: allow once/read-only session/deny$' -Message "M17 AI Assistant verifier failed: pkginfo did not expose consent choices."
Assert-Line -Lines $outputLines -Pattern '^ai denied request data: 0$' -Message "M17 AI Assistant verifier failed: denied request data count was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai allowed context: scoped read-only status only$' -Message "M17 AI Assistant verifier failed: scoped read-only grant was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai model call: none$' -Message "M17 AI Assistant verifier failed: no-model-call status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai scripted response: none$' -Message "M17 AI Assistant verifier failed: no scripted-response status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai package integrity: signed Product component$' -Message "M17 AI Assistant verifier failed: Assistant package integrity status was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai self-modification: denied$' -Message "M17 AI Assistant verifier failed: Assistant self-modification denial was not visible."
Assert-Line -Lines $outputLines -Pattern '^ai cloud memory: unavailable$' -Message "M17 AI Assistant verifier failed: cloud memory unavailability was not visible."
Assert-Line -Lines $outputLines -Pattern '^authority: no ambient install, update, network, cloud, file, identity, secret, or AI access$' -Message "M17 AI Assistant verifier failed: no-ambient AI authority status was not visible."

Write-Host "M17 AI Assistant read-only verifier passed for $Architecture $BootMedia ($BuildProfile profile)."
