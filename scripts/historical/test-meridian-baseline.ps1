$ErrorActionPreference = 'Stop'
$workspace = '<WORKSPACE>'
$baseline = Join-Path $workspace 'outgoing\meridian-baseline'
$manifest = Get-Content -LiteralPath (Join-Path $baseline 'manifest.json') -Raw | ConvertFrom-Json
$zipPath = Join-Path $workspace 'outgoing\client-release-v0.8.1\client-v0.8.1.zip'
if ((Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash -ne $manifest.clientZipSha256) { throw 'Baseline ZIP changed' }
$profiles = @('AETHERIUS - GRAFICO - QUALIDADE', 'AETHERIUS - PERFORMANCE')
$filenames = @('archives.txt', 'initweaks.ini', 'loadorder.txt', 'lockedorder.txt', 'modlist.txt', 'plugins.txt', 'settings.ini')
$results = @()
foreach ($profile in $profiles) {
    foreach ($filename in $filenames) {
        $relative = "client/modOrganizer/profiles/$profile/$filename"
        $entry = @($manifest.entries | Where-Object path -EQ $relative)
        if ($entry.Count -ne 1) { throw "Missing or duplicate ZIP entry: $relative" }
        $backupFile = Join-Path $baseline $relative
        $sourceFile = Join-Path 'C:\Modding\MO2\profiles' (Join-Path $profile $filename)
        $backupHash = (Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash
        if ($backupHash -ne $entry[0].sha256) { throw "Backup mismatch: $relative" }
        $sourceHash = (Get-FileHash -LiteralPath $sourceFile -Algorithm SHA256).Hash
        $results += [pscustomobject]@{ file = $relative; backupMatchesZip = $true; sourceMatchesBaseline = ($sourceHash -eq $backupHash) }
    }
}
foreach ($relative in @('client/Data/SKSE/Plugins/SkyrimPlatform.dll', 'client/Data/Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll')) {
    $entry = @($manifest.entries | Where-Object path -EQ $relative)
    if ($entry.Count -ne 1) { throw "Missing native baseline: $relative" }
    $backupFile = Join-Path $baseline $relative
    $installed = Join-Path '<GAME>' $relative.Substring('client/'.Length)
    if ((Get-FileHash -LiteralPath $backupFile -Algorithm SHA256).Hash -ne $entry[0].sha256) { throw "Native backup mismatch: $relative" }
    if ((Get-FileHash -LiteralPath $installed -Algorithm SHA256).Hash -ne $entry[0].sha256) { throw "Installed native differs: $relative" }
}
foreach ($entry in $manifest.entries) {
    if ($entry.path -notmatch '^client/' -or $entry.path -match '(^|/)\.\.(/|$)|^/|^[a-zA-Z]:|\\') { throw "Invalid ZIP path: $($entry.path)" }
}
if (@($manifest.entries | Group-Object path | Where-Object Count -GT 1).Count -ne 0) { throw 'Duplicate ZIP paths' }
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $baseline 'verification.json') -Encoding utf8
$results | Format-Table -AutoSize
Write-Output 'PASS: ZIP/backup hashes, official profile inventory, installed native DLLs, safe paths and duplicates.'
if (@($results | Where-Object sourceMatchesBaseline -EQ $false).Count -gt 0) { Write-Warning 'The current source profiles have changed since v0.8.1; review before final packaging.' }

