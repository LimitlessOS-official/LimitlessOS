param(
    [string]$SourceDir = "",
    [string]$SeedBuildDir = "",
    [string]$BuildDir = "",
    [string]$OutputPath = "",
    [switch]$ForceClean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $root "external\src\busybox-1.35.0"
}
if ([string]::IsNullOrWhiteSpace($SeedBuildDir)) {
    $SeedBuildDir = Join-Path $root "external\build\busybox-ls-build"
}
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $root "external\build\busybox-standalone-sh-build"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root "external\build\busybox-1.35.0-x86_64-linux-musl-0x52000000-standalone-sh"
}

function Resolve-ExistingPath
{
    param([string]$Path)

    return (Resolve-Path -LiteralPath $Path).Path
}

function Assert-ExternalBuildPath
{
    param([string]$Path)

    $externalBuildRoot = Join-Path $root "external\build"
    $fullExternalBuildRoot = [System.IO.Path]::GetFullPath($externalBuildRoot)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if (-not $fullPath.StartsWith($fullExternalBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "BusyBox standalone build path must stay under external\build: $Path"
    }
}

function ConvertTo-MsysPath
{
    param([string]$Path)

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath.Length -lt 3 -or $fullPath[1] -ne ':') {
        throw "Cannot convert path to MSYS form: $Path"
    }

    return ("/" + $fullPath[0].ToString().ToLowerInvariant() + $fullPath.Substring(2).Replace('\', '/'))
}

function Set-ConfigLine
{
    param(
        [string[]]$Lines,
        [string]$Name,
        [string]$Value
    )

    $setLine = "$Name=$Value"
    $unsetLine = "# $Name is not set"
    $updated = New-Object System.Collections.Generic.List[string]
    $found = $false

    foreach ($line in $Lines) {
        if (($line -eq $unsetLine) -or ($line -match "^$([regex]::Escape($Name))=")) {
            if (-not $found) {
                $updated.Add($setLine)
                $found = $true
            }
        }
        else {
            $updated.Add($line)
        }
    }

    if (-not $found) {
        $updated.Add($setLine)
    }

    return $updated.ToArray()
}

function Invoke-Checked
{
    param(
        [string]$Tool,
        [string[]]$Arguments,
        [string]$LogPath,
        [string]$FailureMessage
    )

    $output = & $Tool @Arguments 2>&1
    Set-Content -Path $LogPath -Value @($output) -Encoding Ascii
    if ($LASTEXITCODE -ne 0) {
        Get-Content -Path $LogPath -Tail 160
        throw $FailureMessage
    }
}

$sourcePath = Resolve-ExistingPath $SourceDir
$seedPath = Resolve-ExistingPath $SeedBuildDir
Assert-ExternalBuildPath -Path $BuildDir
Assert-ExternalBuildPath -Path $OutputPath

$msysBin = Join-Path $root "external\tools\msys2-base\usr\bin"
$muslBin = Join-Path $root "external\tools\gcc-linux-musl-x86_64\bin"
$gitUsrBin = Join-Path $root "external\tools\git-usr-bin"
if (-not (Test-Path (Join-Path $gitUsrBin "cmp.exe"))) {
    $gitUsrBin = "C:\Program Files\Git\usr\bin"
}

$makePath = Join-Path $msysBin "make.exe"
if (-not (Test-Path $makePath)) {
    throw "BusyBox standalone build requires bundled MSYS2 make at external\tools\msys2-base\usr\bin\make.exe."
}
if (-not (Test-Path (Join-Path $muslBin "x86_64-linux-musl-gcc.exe"))) {
    throw "BusyBox standalone build requires musl-cross x86_64-linux-musl-gcc under external\tools\gcc-linux-musl-x86_64\bin."
}

$nativeGcc = Get-Command "gcc" -ErrorAction SilentlyContinue
if (-not $nativeGcc) {
    throw "BusyBox standalone build requires a native host gcc on PATH for BusyBox host generators."
}

New-Item -ItemType Directory -Force -Path (Join-Path $root "build") | Out-Null
if ((Test-Path $BuildDir) -and $ForceClean) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
if (-not (Test-Path $BuildDir)) {
    Copy-Item -LiteralPath $seedPath -Destination $BuildDir -Recurse
}

$configPath = Join-Path $BuildDir ".config"
if (-not (Test-Path $configPath)) {
    throw "BusyBox seed build directory does not contain .config: $SeedBuildDir"
}

$configLines = Get-Content -LiteralPath $configPath
$configLines = Set-ConfigLine -Lines $configLines -Name "CONFIG_FEATURE_PREFER_APPLETS" -Value "y"
$configLines = Set-ConfigLine -Lines $configLines -Name "CONFIG_FEATURE_SH_STANDALONE" -Value "y"
$configLines = Set-ConfigLine -Lines $configLines -Name "CONFIG_FEATURE_SH_NOFORK" -Value "y"
$configLines = Set-ConfigLine -Lines $configLines -Name "CONFIG_STATIC" -Value "y"
$configLines = Set-ConfigLine -Lines $configLines -Name "CONFIG_EXTRA_LDFLAGS" -Value '"-static -no-pie -Wl,-Ttext-segment=0x52000000"'
Set-Content -LiteralPath $configPath -Value $configLines -Encoding Ascii

$catSourcePath = Join-Path $sourcePath "coreutils\cat.c"
$catAppletSourceLine = "//applet:IF_CAT(APPLET(cat, BB_DIR_BIN, BB_SUID_DROP))"
$catNoexecSourceLine = "//applet:IF_CAT(APPLET_NOEXEC(cat, cat, BB_DIR_BIN, BB_SUID_DROP, cat))"
if (-not (Test-Path $catSourcePath)) {
    throw "BusyBox source tree does not contain coreutils\cat.c."
}
$catSource = Get-Content -LiteralPath $catSourcePath -Raw
if ($catSource.Contains($catAppletSourceLine)) {
    $catSource = $catSource.Replace($catAppletSourceLine, $catNoexecSourceLine)
    Set-Content -LiteralPath $catSourcePath -Value $catSource -Encoding Ascii
}
elseif (-not $catSource.Contains($catNoexecSourceLine)) {
    throw "BusyBox cat.c does not contain the expected applet declaration."
}

New-Item -ItemType Directory -Force -Path (Join-Path $sourcePath "include\asm-x86_64") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $BuildDir "include2") | Out-Null
Remove-Item -LiteralPath (Join-Path $BuildDir ".config.old") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir ".tmpconfig.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include\autoconf.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include\bbconfigopts.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include\bbconfigopts_bz2.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include\applet_tables.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include\NUM_APPLETS.h") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "applets\applet_tables.exe") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "applets\applet_tables") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "applets\.applet_tables.cmd") -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include2\asm") -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $BuildDir "include2\asm.lnk") -Force -ErrorAction SilentlyContinue

$env:Path = $msysBin + ";" + $gitUsrBin + ";" + $muslBin + ";" + $env:Path

$sourceMsys = ConvertTo-MsysPath -Path $sourcePath
$buildMsys = ConvertTo-MsysPath -Path $BuildDir
$oldConfigArgs = @(
    "-C",
    $sourceMsys,
    "O=$buildMsys",
    "ARCH=x86_64",
    "HOSTCC=gcc",
    "CC=x86_64-linux-musl-gcc",
    "oldconfig"
)
Invoke-Checked `
    -Tool $makePath `
    -Arguments $oldConfigArgs `
    -LogPath (Join-Path $root "build\busybox-standalone-oldconfig.log") `
    -FailureMessage "BusyBox standalone build failed while regenerating config headers."

$autoconfPath = Join-Path $BuildDir "include\autoconf.h"
$tmpAutoconfPath = Join-Path $BuildDir ".tmpconfig.h"
if ((-not (Test-Path $autoconfPath)) -and (Test-Path $tmpAutoconfPath)) {
    Copy-Item -LiteralPath $tmpAutoconfPath -Destination $autoconfPath -Force
}
if (-not (Test-Path $autoconfPath)) {
    throw "BusyBox oldconfig did not produce include\autoconf.h."
}
$autoconfText = Get-Content -LiteralPath $autoconfPath -Raw
if ($autoconfText -notmatch "#define CONFIG_FEATURE_SH_STANDALONE 1") {
    throw "BusyBox generated config did not enable CONFIG_FEATURE_SH_STANDALONE."
}
if ($autoconfText -notmatch "#define CONFIG_FEATURE_SH_NOFORK 1") {
    throw "BusyBox generated config did not enable CONFIG_FEATURE_SH_NOFORK."
}
if ($autoconfText -notmatch "#define CONFIG_FEATURE_PREFER_APPLETS 1") {
    throw "BusyBox generated config did not enable CONFIG_FEATURE_PREFER_APPLETS."
}

$appletsHeaderPath = Join-Path $BuildDir "include\applets.h"
if (-not (Test-Path $appletsHeaderPath)) {
    throw "BusyBox oldconfig did not produce include\applets.h."
}
$appletsHeader = Get-Content -LiteralPath $appletsHeaderPath
$catAppletLine = "IF_CAT(APPLET(cat, BB_DIR_BIN, BB_SUID_DROP))"
$catNoexecLine = "IF_CAT(APPLET_NOEXEC(cat, cat, BB_DIR_BIN, BB_SUID_DROP, cat))"
$catAppletPatched = $false
$patchedAppletsHeader = New-Object System.Collections.Generic.List[string]
foreach ($line in $appletsHeader) {
    if ($line -eq $catAppletLine) {
        $patchedAppletsHeader.Add($catNoexecLine)
        $catAppletPatched = $true
    }
    else {
        $patchedAppletsHeader.Add($line)
    }
}
if (-not $catAppletPatched) {
    if (($appletsHeader -join "`n") -notmatch [regex]::Escape($catNoexecLine)) {
        throw "BusyBox generated applets.h does not contain the expected cat applet line."
    }
}
Set-Content -LiteralPath $appletsHeaderPath -Value $patchedAppletsHeader.ToArray() -Encoding Ascii

$appletTablesPath = Join-Path $BuildDir "applets\applet_tables.exe"
if (-not (Test-Path $appletTablesPath)) {
    Push-Location $BuildDir
    try {
        $appletTablesSource = Join-Path $sourcePath "applets\applet_tables.c"
        $gccArgs = @(
            "-Wp,-MD,applets/.applet_tables.d",
            "-Iapplets",
            "-Wall",
            "-Wstrict-prototypes",
            "-O2",
            "-fomit-frame-pointer",
            "-o",
            "applets/applet_tables",
            $appletTablesSource
        )
        Invoke-Checked `
            -Tool $nativeGcc.Source `
            -Arguments $gccArgs `
            -LogPath (Join-Path $root "build\busybox-standalone-applet-tables.log") `
            -FailureMessage "BusyBox standalone build failed while compiling applets/applet_tables.exe."
    }
    finally {
        Pop-Location
    }
}

$makeArgs = @(
    "-C",
    $sourceMsys,
    "O=$buildMsys",
    "ARCH=x86_64",
    "HOSTCC=gcc",
    "CC=x86_64-linux-musl-gcc",
    "-j1",
    "busybox"
)
Invoke-Checked `
    -Tool $makePath `
    -Arguments $makeArgs `
    -LogPath (Join-Path $root "build\busybox-standalone-build.log") `
    -FailureMessage "BusyBox standalone build failed."

$builtBusyBox = Join-Path $BuildDir "busybox"
if (-not (Test-Path $builtBusyBox)) {
    throw "BusyBox standalone build completed but did not produce $builtBusyBox."
}
Copy-Item -LiteralPath $builtBusyBox -Destination $OutputPath -Force

$hash = (Get-FileHash -Algorithm SHA256 -Path $OutputPath).Hash.ToLowerInvariant()
$readelf = Get-Command "readelf" -ErrorAction SilentlyContinue
if ($readelf) {
    $header = & $readelf.Source -h $OutputPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "readelf -h failed for $OutputPath"
    }
    $programHeaders = & $readelf.Source -l $OutputPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "readelf -l failed for $OutputPath"
    }
    if (($header -join "`n") -notmatch "Type:\s+EXEC") {
        throw "Standalone BusyBox artifact is not ET_EXEC."
    }
    if (($programHeaders -join "`n") -notmatch "0x0000000052000000") {
        throw "Standalone BusyBox artifact is not linked at 0x52000000."
    }
    if (($programHeaders -join "`n") -match "INTERP|DYNAMIC") {
        throw "Standalone BusyBox artifact must remain static and must not contain PT_INTERP/PT_DYNAMIC."
    }
}

Write-Host "Built standalone-shell BusyBox artifact."
Write-Host "  output : $OutputPath"
Write-Host "  bytes  : $((Get-Item $OutputPath).Length)"
Write-Host "  sha256 : $hash"
Write-Host "  config : FEATURE_PREFER_APPLETS=y FEATURE_SH_STANDALONE=y FEATURE_SH_NOFORK=y CAT_NOEXEC=patched"
Write-Host "  link   : static non-PIE ET_EXEC at 0x52000000"
