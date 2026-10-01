param(
  [Parameter(Mandatory)][switch]$PublicationApproved,
  [Parameter(Mandatory)][string]$BaselineZip,
  [Parameter(Mandatory)][string]$CandidateDirectory
)
$ErrorActionPreference='Stop'
if (-not $PublicationApproved) { throw 'Explicit publication approval is required' }
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$modName='AETHERIUS - MERIDIAN - COMPATIBILIDADE'
$modAsset='AETHERIUS.-.MERIDIAN.-.COMPATIBILIDADE.zip'
$baselineHash='4BD9615AB3211805A51A34F901FB92B0DFD98AF82A1B98DDAAD5DCC0BB18B2C7'
if ((Get-FileHash -LiteralPath $BaselineZip).Hash -ne $baselineHash) { throw 'Baseline does not match the verified remote v0.8.1 asset' }
$candidate=Get-Content -LiteralPath (Join-Path $CandidateDirectory 'manifest.json') -Raw | ConvertFrom-Json
$allow=@('SKSE/Plugins/SkyrimPlatform.dll','SKSE/Plugins/SkyrimPlatform.ini','Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll','Platform/Distribution/RuntimeDependencies/libnode.dll','Platform/Plugins/skymp5-client.js','MeridianUI/aetherius-client/index.html','MeridianUI/aetherius-client/bootstrap.js','MeridianUI/aetherius-client/widgets.js')
if ($candidate.schema -ne 1 -or $candidate.label -ne '20261001-090924' -or @($candidate.entries).Count -ne 8) { throw 'Unexpected candidate' }
$inputs=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
$modInputs=[Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($entry in $candidate.entries) {
  if ($entry.path -cnotin $allow) { throw 'Unapproved overlay path' }
  $file=Join-Path (Join-Path $CandidateDirectory 'Aetherius - Meridian - TESTE') $entry.path
  if ((Get-FileHash -LiteralPath $file).Hash -ne $entry.sha256) { throw 'Candidate content changed' }
  $inputs.Add(('client/Data/'+$entry.path),$file)
  $modInputs.Add(($modName+'/'+$entry.path),$file)
}
$notices=@{ 'LICENSE.txt'=(Join-Path $root 'LICENSE'); 'THIRD_PARTY_NOTICES.md'=(Join-Path $root 'THIRD_PARTY_NOTICES.md'); 'licenses/NODE-LICENSE-v23.2.0.txt'=(Join-Path $root 'licenses/NODE-LICENSE-v23.2.0.txt'); 'licenses/Meridian-SDK-LICENSE-MIT.txt'=(Join-Path $root 'source/runtime/skyrim-platform/src/platform_se/skyrim_platform/MeridianUIAPI/LICENSE-MIT'); 'PUBLICACAO.md'=(Join-Path $root 'docs/PUBLICACAO-20261001.md') }
foreach($item in $notices.GetEnumerator()) { $modInputs.Add(($modName+'/'+$item.Key),$item.Value) }
foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'licenses/dependencies') -File -Recurse) {
  $rel=[IO.Path]::GetRelativePath((Join-Path $root 'licenses/dependencies'),$file.FullName).Replace('\','/')
  $modInputs.Add(($modName+'/licenses/dependencies/'+$rel),$file.FullName)
}
foreach($path in $modInputs.Keys) { $inputs.Add(('client/modOrganizer/mods/'+$path),$modInputs[$path]) }
$profiles=@('AETHERIUS - GRAFICO - QUALIDADE','AETHERIUS - PERFORMANCE')
foreach($profile in $profiles) {
  $inputs.Add("client/modOrganizer/profiles/$profile/modlist.txt",(Join-Path $root "source/client/client/profiles/$profile/modlist.txt"))
}
function Get-EntryHash($Entry) {
  $s=$Entry.Open(); $sha=[Security.Cryptography.SHA256]::Create()
  try { return [Convert]::ToHexString($sha.ComputeHash($s)) } finally { $s.Dispose();$sha.Dispose() }
}
function Assert-Path([string]$Path,[string]$Prefix) {
  if (-not $Path.StartsWith($Prefix,[StringComparison]::Ordinal) -or $Path -match '\\|:|(^|/)\.\.(/|$)') { throw "Unsafe archive path: $Path" }
}
$baseline=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($BaselineZip))
$out=Join-Path $root 'packages/release-v0.8.2'
try {
  $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  foreach($entry in $baseline.Entries) {
    Assert-Path $entry.FullName 'client/'
    if (-not $seen.Add($entry.FullName)) { throw 'Duplicate baseline path' }
  }
  foreach($profile in $profiles) {
    foreach($name in @('archives.txt','initweaks.ini','loadorder.txt','lockedorder.txt','modlist.txt','plugins.txt','settings.ini')) {
      $entry=$baseline.GetEntry("client/modOrganizer/profiles/$profile/$name")
      if ($null -eq $entry) { throw 'Baseline profile missing' }
      $source=Join-Path $root "source/client/client/profiles/$profile/$name"
      if ($name -eq 'modlist.txt') {
        $r=[IO.StreamReader]::new($entry.Open()); try{$old=$r.ReadToEnd()}finally{$r.Dispose()}
        $new=[IO.File]::ReadAllText($source)
        $added="+$modName`n"
        if (($new.Replace("`r`n","`n").Replace($added,'')) -cne $old.Replace("`r`n","`n")) { throw 'Profile changed beyond the one approved mod' }
        if (@($new -split '\r?\n' | Where-Object {$_ -ceq "+$modName"}).Count -ne 1 -or ($new -split '\r?\n')[1] -cne "+$modName") { throw 'Overlay must appear once at highest priority' }
        # Verify bytes too, not just normalized line endings.
        $bytes=[IO.File]::ReadAllBytes($source)
        $removed=[Text.UTF8Encoding]::new($false).GetBytes($new.Replace(("+$modName"+ $(if($new.Contains("`r`n")){"`r`n"}else{"`n"})),''))
        $sha=[Security.Cryptography.SHA256]::Create()
        try{if([Convert]::ToHexString($sha.ComputeHash($removed)) -ne (Get-EntryHash $entry)){throw 'Profile encoding/line endings changed'}}finally{$sha.Dispose()}
      } else {
        # Git may normalize repository text; the package itself copies these
        # unmodified entries directly from the ZIP and validates their raw hash.
        $r=[IO.StreamReader]::new($entry.Open());try{$old=$r.ReadToEnd()}finally{$r.Dispose()}
        if([IO.File]::ReadAllText($source).Replace("`r`n","`n") -cne $old.Replace("`r`n","`n")){throw 'Unapproved profile content change'}
      }
    }
  }
  if (Test-Path -LiteralPath $out) { throw 'Never overwrite release staging' }
  New-Item -ItemType Directory -Path $out | Out-Null
  $rows=[Collections.Generic.List[object]]::new()
  $clientZip=Join-Path $out 'client-v0.8.2.zip'
  $archive=[IO.Compression.ZipFile]::Open($clientZip,[IO.Compression.ZipArchiveMode]::Create)
  try {
    foreach($entry in $baseline.Entries | Where-Object {-not $_.FullName.EndsWith('/')}) {
      $oldHash=Get-EntryHash $entry
      $target=$archive.CreateEntry($entry.FullName,[IO.Compression.CompressionLevel]::Optimal)
      $target.ExternalAttributes=$entry.ExternalAttributes; $target.LastWriteTime=$entry.LastWriteTime
      $dest=$target.Open()
      if($inputs.ContainsKey($entry.FullName)){$s=[IO.File]::OpenRead($inputs[$entry.FullName]);$hash=(Get-FileHash -LiteralPath $inputs[$entry.FullName]).Hash;$len=$s.Length}
      else{$s=$entry.Open();$hash=$oldHash;$len=$entry.Length}
      try{$s.CopyTo($dest)}finally{$s.Dispose();$dest.Dispose()}
      $rows.Add([pscustomobject]@{path=$entry.FullName;bytes=$len;sha256=$hash;baselineSha256=$oldHash;change=if($hash -eq $oldHash){'unchanged'}else{'replaced'}})
    }
    foreach($path in $inputs.Keys) {
      Assert-Path $path 'client/'
      if($seen.Contains($path)){continue}
      [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$inputs[$path],$path,[IO.Compression.CompressionLevel]::Optimal)|Out-Null
      $rows.Add([pscustomobject]@{path=$path;bytes=(Get-Item -LiteralPath $inputs[$path]).Length;sha256=(Get-FileHash -LiteralPath $inputs[$path]).Hash;baselineSha256=$null;change='added'})
    }
  }finally{$archive.Dispose()}
}finally{$baseline.Dispose()}
function Test-Zip($Zip,$Expected,$Prefix) {
  $z=[IO.Compression.ZipFile]::OpenRead($Zip)
  try{
    if($z.Entries.Count -ne $Expected.Count){throw 'ZIP count mismatch'}
    $unique=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($e in $z.Entries){
      Assert-Path $e.FullName $Prefix
      if(-not $unique.Add($e.FullName) -or -not $Expected.ContainsKey($e.FullName)){throw 'Unexpected ZIP entry'}
      $row=$Expected[$e.FullName]
      if($e.Length -ne $row.bytes -or (Get-EntryHash $e) -ne $row.sha256){throw 'ZIP content mismatch'}
    }
  }finally{$z.Dispose()}
}
$expected=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
foreach($row in $rows){$expected.Add($row.path,$row)}
Test-Zip $clientZip $expected 'client/'
$modZip=Join-Path $out $modAsset
$modRows=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::OrdinalIgnoreCase)
$z=[IO.Compression.ZipFile]::Open($modZip,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($path in $modInputs.Keys){
  [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z,$modInputs[$path],$path,[IO.Compression.CompressionLevel]::Optimal)|Out-Null
  $modRows.Add($path,[pscustomobject]@{path=$path;bytes=(Get-Item -LiteralPath $modInputs[$path]).Length;sha256=(Get-FileHash -LiteralPath $modInputs[$path]).Hash})
}}finally{$z.Dispose()}
Test-Zip $modZip $modRows ($modName+'/')
$changes=@($rows|Where-Object change -NE 'unchanged')
if(@($changes|Where-Object {$_.path -notin $inputs.Keys}).Count){throw 'Unexpected release change'}
if(@($rows|Where-Object change -EQ 'replaced').Count -ne 5){throw 'Expected only 3 replaced runtime files and 2 modlists'}
$report=[ordered]@{schema=1;status='Publication approved by user; startup scope only';clientVersion='v0.8.2';patchVersion='v0.1.0';modName=$modName;modTag='AETHERIUS.-.MERIDIAN.-.COMPATIBILIDADE-v1.0.0';baselineTag='v0.8.1';baselineAssetId=602197793;baselineZipSha256=$baselineHash;candidate='20261001-090924';clientZipSha256=(Get-FileHash -LiteralPath $clientZip).Hash;clientBytes=(Get-Item -LiteralPath $clientZip).Length;modZipSha256=(Get-FileHash -LiteralPath $modZip).Hash;modBytes=(Get-Item -LiteralPath $modZip).Length;clientEntryCount=$rows.Count;modEntryCount=$modRows.Count;unchangedCount=@($rows|Where-Object change -EQ 'unchanged').Count;changes=$changes;overlayEntries=$candidate.entries;officialProfiles='Only one new highest-priority mod line per profile; all other baseline bytes preserved';legacyCef='Retained baseline assets; integrated runtime does not use legacy browser';newGameTests=$false;gameWrites=$false;mo2Writes=$false;release='prepared, pending remote confirmation'}
$report|ConvertTo-Json -Depth 7|Set-Content -LiteralPath (Join-Path $root 'reports/release-package-v0.8.2.json') -Encoding utf8NoBOM
$rows|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $out 'entry-hashes.json') -Encoding utf8NoBOM
Write-Output "VERIFIED: $clientZip"
Write-Output "VERIFIED: $modZip"
