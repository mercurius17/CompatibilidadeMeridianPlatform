param(
  [Parameter(Mandatory)][string]$BaselineZip,
  [Parameter(Mandatory)][string]$CandidateDirectory,
  [string]$Label=([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($Label -notmatch '^\d{8}-\d{6}$') { throw 'Timestamp label required' }
$baselineHash='4BD9615AB3211805A51A34F901FB92B0DFD98AF82A1B98DDAAD5DCC0BB18B2C7'
if ((Get-FileHash -LiteralPath $BaselineZip).Hash -ne $baselineHash) { throw 'Not the frozen v0.8.1 baseline; review a new baseline before using it' }
$manifest=Get-Content -LiteralPath (Join-Path $CandidateDirectory 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.status -ne 'EXPERIMENTAL - NOT HOMOLOGATED - DO NOT PUBLISH' -or $manifest.schema -ne 1 -or @($manifest.entries).Count -ne 8) { throw 'Unexpected overlay manifest' }
$allow=@('SKSE/Plugins/SkyrimPlatform.dll','SKSE/Plugins/SkyrimPlatform.ini','Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll','Platform/Distribution/RuntimeDependencies/libnode.dll','Platform/Plugins/skymp5-client.js','MeridianUI/aetherius-client/index.html','MeridianUI/aetherius-client/bootstrap.js','MeridianUI/aetherius-client/widgets.js')
$inputs=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $manifest.entries) {
  if ($entry.path -cnotin $allow -or $entry.sha256 -notmatch '^[A-Fa-f0-9]{64}$') { throw 'Unreviewed overlay entry' }
  $file=Join-Path (Join-Path $CandidateDirectory 'Aetherius - Meridian - TESTE') $entry.path
  if ((Get-FileHash -LiteralPath $file).Hash -ne $entry.sha256) { throw 'Overlay digest mismatch' }
  $inputs.Add(('client/Data/'+$entry.path),$file)
}
function Get-EntryHash($Entry) {
  $stream=$Entry.Open(); $sha=[Security.Cryptography.SHA256]::Create()
  try { return [Convert]::ToHexString($sha.ComputeHash($stream)) }
  finally { $stream.Dispose(); $sha.Dispose() }
}
$baseline=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($BaselineZip))
try {
  $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  $files=@($baseline.Entries | Where-Object { -not $_.FullName.EndsWith('/') })
  foreach ($entry in $baseline.Entries) {
    if ($entry.FullName -notmatch '^client/' -or $entry.FullName -match '\\|:|(^|/)\.\.(/|$)' -or -not $seen.Add($entry.FullName)) { throw 'Unsafe or duplicate baseline path' }
  }
  foreach ($path in @('client/modOrganizer/ModOrganizer.exe','client/Data/SKSE/Plugins/MpClientPlugin.dll')) { if (-not $seen.Contains($path)) { throw "Baseline required file absent: $path" } }
  foreach ($profile in @('AETHERIUS - GRAFICO - QUALIDADE','AETHERIUS - PERFORMANCE')) {
    foreach ($name in @('archives.txt','initweaks.ini','loadorder.txt','lockedorder.txt','modlist.txt','plugins.txt','settings.ini')) {
      if (-not $seen.Contains("client/modOrganizer/profiles/$profile/$name")) { throw 'Baseline profile incomplete' }
    }
  }
  $output=Join-Path $root "packages/client-preview-$Label"
  if (Test-Path -LiteralPath $output) { throw 'Never overwrite an earlier preview' }
  New-Item -ItemType Directory -Path $output | Out-Null
  # Deliberately not client-v<version>.zip: never feed this unapproved preview
  # into the release selector. Its internal client/ topology matches the launcher.
  $zipPath=Join-Path $output 'client-meridian-EXPERIMENTAL.zip'
  $archive=[IO.Compression.ZipFile]::Open($zipPath,[IO.Compression.ZipArchiveMode]::Create)
  $rows=[Collections.Generic.List[object]]::new()
  try {
    foreach ($entry in $files) {
      $oldHash=Get-EntryHash $entry
      $newEntry=$archive.CreateEntry($entry.FullName,[IO.Compression.CompressionLevel]::Optimal)
      $destination=$newEntry.Open()
      if ($inputs.ContainsKey($entry.FullName)) {
        $source=[IO.File]::OpenRead($inputs[$entry.FullName])
        $hash=(Get-FileHash -LiteralPath $inputs[$entry.FullName]).Hash
        $bytes=$source.Length
      } else { $source=$entry.Open(); $hash=$oldHash; $bytes=$entry.Length }
      try { $source.CopyTo($destination) } finally { $source.Dispose(); $destination.Dispose() }
      $rows.Add([pscustomobject]@{path=$entry.FullName;bytes=$bytes;sha256=$hash;baselineSha256=$oldHash;change=if($hash -eq $oldHash){'unchanged'}else{'replaced'}})
    }
    foreach ($path in $inputs.Keys) {
      if ($seen.Contains($path)) { continue }
      [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$inputs[$path],$path,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
      $rows.Add([pscustomobject]@{path=$path;bytes=(Get-Item -LiteralPath $inputs[$path]).Length;sha256=(Get-FileHash -LiteralPath $inputs[$path]).Hash;baselineSha256=$null;change='added'})
    }
  } finally { $archive.Dispose() }
} finally { $baseline.Dispose() }
$expected=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach ($row in $rows) { $expected.Add($row.path,$row) }
$check=[IO.Compression.ZipFile]::OpenRead($zipPath)
try {
  if ($check.Entries.Count -ne $expected.Count) { throw 'ZIP file count differs' }
  $unique=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  foreach ($entry in $check.Entries) {
    if (-not $expected.ContainsKey($entry.FullName) -or -not $unique.Add($entry.FullName)) { throw 'Unexpected or duplicate preview entry' }
    $row=$expected[$entry.FullName]
    if ($entry.Length -ne $row.bytes -or (Get-EntryHash $entry) -ne $row.sha256) { throw 'Preview entry failed byte/hash validation' }
  }
} finally { $check.Dispose() }
$changed=@($rows | Where-Object change -NE 'unchanged')
if (@($changed | Where-Object { $_.path -notin $inputs.Keys }).Count) { throw 'Change outside the eight-file overlay allowlist' }
$profileChanges=@($changed | Where-Object path -Like 'client/modOrganizer/profiles/*')
if ($profileChanges.Count) { throw 'Profiles must remain byte-identical to frozen baseline' }
$legacy=@($rows | Where-Object { $_.path -like 'client/Data/Platform/Distribution/CEF/*' -or $_.path -match '/RuntimeDependencies/(libcef\.dll|chrome_elf\.dll|SkyrimPlatformCEF\.exe\.hidden)$' } | ForEach-Object path)
$report=[ordered]@{schema=1;label=$Label;status='EXPERIMENTAL PREVIEW - NOT HOMOLOGATED - DO NOT PUBLISH';baselineZipSha256=$baselineHash;candidateLabel=$manifest.label;zipFile='client-meridian-EXPERIMENTAL.zip';zipSha256=(Get-FileHash -LiteralPath $zipPath).Hash;entryCount=$rows.Count;unchangedCount=@($rows | Where-Object change -EQ 'unchanged').Count;changes=$changed;profiles='Frozen baseline profiles byte-identical; not synchronized from live MO2';legacyCefAssetsRetained=$legacy;limitations=@('Historical baseline, not verified against latest official release','Legacy CEF removal requires consumer review; nothing removed in this preview','No installation, launcher update, production version, upload or release','Login, voice, input, complete profile/clean installation validation outstanding');gameWrites=$false;mo2Writes=$false;release='none'}
$rows | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $output 'entry-hashes.json') -Encoding utf8NoBOM
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding utf8NoBOM
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root "reports/client-preview-$Label.json") -Encoding utf8NoBOM
Write-Output "VERIFIED LOCAL PREVIEW ONLY: $zipPath"
