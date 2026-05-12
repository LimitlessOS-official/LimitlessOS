param(
    [string]$EvidenceDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $scriptDir

$scanRoots = @(
    (Join-Path $root "README.md"),
    (Join-Path $root "docs"),
    (Join-Path $root "dist"),
    (Join-Path $root "build\generated\package_store_generated.h"),
    (Join-Path $root "build\generated\package_store_signatures_generated.h")
)

if (-not [string]::IsNullOrWhiteSpace($EvidenceDir)) {
    $scanRoots += $EvidenceDir
}

$secretMarkers = @(
    "-----BEGIN PRIVATE KEY-----",
    "-----BEGIN OPENSSH PRIVATE KEY-----",
    "-----BEGIN ED25519 PRIVATE KEY-----",
    "Ed25519PrivateKey.generate",
    "private_key =",
    "privateKeyPem",
    "private-key-material"
)

$candidateFiles = New-Object System.Collections.Generic.List[string]
foreach ($rootPath in $scanRoots) {
    if (-not (Test-Path -LiteralPath $rootPath)) {
        continue
    }

    $item = Get-Item -LiteralPath $rootPath
    if ($item.PSIsContainer) {
        Get-ChildItem -LiteralPath $item.FullName -Recurse -File | ForEach-Object {
            $candidateFiles.Add($_.FullName)
        }
    }
    else {
        $candidateFiles.Add($item.FullName)
    }
}

$violations = New-Object System.Collections.Generic.List[string]
foreach ($path in ($candidateFiles | Sort-Object -Unique)) {
    $relative = Resolve-Path -LiteralPath $path -Relative
    if ($relative -match '\\dist\\m7-evidence-' -and $relative -notmatch '\\dist\\m7\.1-evidence-') {
        continue
    }
    if ($relative -match 'verify-product-private-key-artifacts\.(stdout|stderr|output)\.txt$') {
        continue
    }

    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($path)
    $latin1 = [System.Text.Encoding]::GetEncoding(28591).GetString($bytes)
    foreach ($marker in $secretMarkers) {
        if ($latin1.Contains($marker)) {
            $violations.Add("$relative contains forbidden private-key marker '$marker'")
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Error $_ }
    throw "private-key artifact scan failed."
}

Write-Host "Private-key artifact scan passed: $($candidateFiles.Count) files checked."
