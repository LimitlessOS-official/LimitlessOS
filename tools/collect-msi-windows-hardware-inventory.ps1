param(
    [string]$OutputDir = ".\dist\msi-windows-hardware-inventory",
    [switch]$IncludeSetupApiTail,
    [switch]$IncludeMsInfo,
    [switch]$IncludeRegistrySnapshot,
    [switch]$IncludeAllPnpProperties
)

$ErrorActionPreference = "Stop"

$collector = Join-Path $PSScriptRoot "collect-windows-hardware-inventory.ps1"
& $collector -OutputDir $OutputDir -IncludeSetupApiTail:$IncludeSetupApiTail -IncludeMsInfo:$IncludeMsInfo -IncludeRegistrySnapshot:$IncludeRegistrySnapshot -IncludeAllPnpProperties:$IncludeAllPnpProperties
