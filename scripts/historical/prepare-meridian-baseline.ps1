param([string]$Workspace = '<WORKSPACE>')
$ErrorActionPreference = 'Stop'
$runtime = Join-Path $Workspace 'aetherius-runtime-meridian'
$baseline = Join-Path $Workspace 'outgoing\meridian-baseline'
$zipPath = Join-Path $Workspace 'outgoing\client-release-v0.8.1\client-v0.8.1.zip'
New-Item -ItemType Directory -Force -Path $baseline | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($zipPath)
$entries = @()
try {
    foreach ($entry in $archive.Entries) {
        if ($entry.FullName.EndsWith('/')) { continue }
        $stream = $entry.Open()
        try { $hash = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream)) }
        finally { $stream.Dispose() }
        $entries += [pscustomobject]@{ path = $entry.FullName; bytes = $entry.Length; sha256 = $hash }
        if ($entry.FullName -match '^client/(Data/(SKSE/Plugins/SkyrimPlatform[^/]*|Platform/Distribution/RuntimeDependencies/[^/]+)|modOrganizer/profiles/[^/]+/[^/]+)$') {
            $destination = [IO.Path]::GetFullPath((Join-Path $baseline $entry.FullName))
            if (-not $destination.StartsWith([IO.Path]::GetFullPath($baseline) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'ZIP path outside baseline' }
            New-Item -ItemType Directory -Force -Path ([IO.Path]::GetDirectoryName($destination)) | Out-Null
            if (Test-Path -LiteralPath $destination) {
                if ((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $hash) { throw "Existing baseline differs: $destination" }
            } else { [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $destination, $false) }
        }
    }
} finally { $archive.Dispose() }
$record = [ordered]@{
    schema = 1
    createdUtc = [DateTime]::UtcNow.ToString('o')
    clientTag = 'v0.8.1'
    clientCommit = '113f10ef479dbd12a182b8763cee319f80a3d00e'
    clientZipSha256 = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
    nativeCommit = (& git -C $runtime rev-parse HEAD).Trim()
    nativeBranch = (& git -C $runtime branch --show-current).Trim()
    vcpkgCommit = 'cb2981c4e03d421fa03b9bb5044cd1986180e7e4'
    vcpkgPinOrigin = 'upstream main gitlink inspected 2026-09-30; original Aetherius build pin is absent/unconfirmed'
    deployment = 'none; production game and MO2 untouched'
    entries = $entries
}
$record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $baseline 'manifest.json') -Encoding utf8
Write-Output "Baseline recorded: $($entries.Count) ZIP entries; $baseline"

