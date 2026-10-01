param(
    [string]$Implementation = '<WORKSPACE>\outgoing\meridian-baseline\client\Data\Platform\Distribution\RuntimeDependencies\SkyrimPlatformImpl.dll',
    [string]$Entry = '<WORKSPACE>\outgoing\meridian-baseline\client\Data\SKSE\Plugins\SkyrimPlatform.dll',
    [switch]$RequireNoLegacyCef
)
$ErrorActionPreference = 'Stop'
$objdump = 'C:\MinGW64\bin\objdump.exe'
$results = @()
foreach ($file in @($Entry, $Implementation)) {
    $output = & $objdump -p $file
    if ($LASTEXITCODE -ne 0) { throw "PE inspection failed: $file" }
    $imports = @($output | ForEach-Object { if ($_ -match '^\s*DLL Name:\s*(.+)$') { $Matches[1].Trim() } })
    $exports = @($output | ForEach-Object { if ($_ -match '^\s*\[\s*\d+\]\s+(SKSEPlugin_\w+|SkyrimPlatform_\w+)\s*$') { $Matches[1] } })
    $required = if ($file -eq $Entry) {
        @('SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version', 'SkyrimPlatform_IpcSend', 'SkyrimPlatform_IpcSubscribe', 'SkyrimPlatform_IpcUnsubscribe')
    } else {
        @('SKSEPlugin_Load_Impl', 'SkyrimPlatform_IpcSend_Impl', 'SkyrimPlatform_IpcSubscribe_Impl', 'SkyrimPlatform_IpcUnsubscribe_Impl')
    }
    foreach ($symbol in $required) { if ($symbol -notin $exports) { throw "Missing ABI export $symbol in $file" } }
    if (@($imports | Where-Object { $_ -match '^(MSVCP[0-9_]+D|VCRUNTIME[0-9_]+D|ucrtbased)\.dll$' }).Count -gt 0) { throw "Debug CRT dependency: $file" }
    if ($RequireNoLegacyCef -and @($imports | Where-Object { $_ -match '^(libcef|chrome_elf)\.dll$' }).Count -gt 0) { throw "Legacy CEF import: $file" }
    $results += [pscustomobject]@{
        path = $file
        sha256 = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
        fileVersion = (Get-Item -LiteralPath $file).VersionInfo.FileVersion
        imports = $imports
        exports = $exports
        requiredExportsPresent = $true
        directLegacyCefImport = ('libcef.dll' -in $imports)
        coverage = 'static direct imports and export names only; not recursive dependency/ABI/runtime validation'
    }
}
$results | ConvertTo-Json -Depth 5

