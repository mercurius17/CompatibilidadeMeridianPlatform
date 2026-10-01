param(
  [Parameter(Mandatory)][string]$OverlayDirectory,
  [string]$CandidateManifest,
  [string]$MO2='C:\modOrganizer',
  [string]$Suffix=([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')),
  [switch]$Install
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$moRoot=[IO.Path]::GetFullPath($MO2).TrimEnd('\')
$overlay=[IO.Path]::GetFullPath($OverlayDirectory)
if ($Suffix -notmatch '^[a-zA-Z0-9-]+$') { throw 'Invalid test suffix' }
if (Get-Process SkyrimSE,ModOrganizer -ErrorAction SilentlyContinue) { throw 'Close Skyrim and MO2 before preparing profiles' }
if (-not (Test-Path -LiteralPath (Join-Path $moRoot 'ModOrganizer.ini'))) { throw 'Not a portable MO2 instance' }
$expected=@{
  'SKSE/Plugins/SkyrimPlatform.dll'='2A3E39D949DBBEEE35ECE867EB3E875E07FED917945FB3FFA88C00BAB644D59C'
  'Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll'='75678B5A2FAC8B6653DEBDBE0D3C0A580FE58759EAF076E3FDEF6CE07E37050C'
  'Platform/Distribution/RuntimeDependencies/libnode.dll'='F22C87FA55E3C0E8B5626FD94C801BA7F1BF581E6FA555CE702C44E553D2D210'
}
# Without a manifest only the historical published pair is accepted. A new
# candidate must explicitly identify all eight files, not only the DLLs.
$relativeFiles=@($expected.Keys)+@('SKSE/Plugins/SkyrimPlatform.ini','MeridianUI/aetherius-client/index.html','MeridianUI/aetherius-client/bootstrap.js','MeridianUI/aetherius-client/widgets.js','Platform/Plugins/skymp5-client.js')
if ($CandidateManifest) {
  $candidate=Get-Content -LiteralPath $CandidateManifest -Raw | ConvertFrom-Json
  if ($candidate.schema -ne 1 -or $candidate.status -ne 'EXPERIMENTAL - NOT HOMOLOGATED - DO NOT PUBLISH') { throw 'Unexpected manifest schema/status' }
  if (@($candidate.entries).Count -ne 8) { throw 'Manifest must identify exactly eight files' }
  $candidateExpected=@{}
  foreach ($entry in $candidate.entries) {
    if ($entry.path -cnotin $relativeFiles -or $candidateExpected.ContainsKey($entry.path) -or $entry.sha256 -notmatch '^[A-Fa-f0-9]{64}$') { throw 'Invalid/duplicate manifest entry' }
    $candidateExpected[$entry.path]=$entry.sha256
  }
  if ($candidateExpected['Platform/Distribution/RuntimeDependencies/libnode.dll'] -ne $expected['Platform/Distribution/RuntimeDependencies/libnode.dll']) { throw 'Dependency drift: libnode' }
  $expected=$candidateExpected
}
if (@(Get-ChildItem -LiteralPath $overlay -Recurse -File).Count -ne $relativeFiles.Count) { throw 'Overlay must contain exactly eight files' }
$entries=@(foreach ($relative in $relativeFiles) {
  $file=Join-Path $overlay $relative
  if (-not (Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing overlay file: $relative" }
  $hash=(Get-FileHash -LiteralPath $file).Hash
  if ($expected.ContainsKey($relative) -and $hash -ne $expected[$relative]) { throw "Wrong candidate/dependency: $relative" }
  [pscustomobject]@{path=$relative;sha256=$hash;bytes=(Get-Item -LiteralPath $file).Length}
})
$ini=Get-Content -LiteralPath (Join-Path $overlay 'SKSE/Plugins/SkyrimPlatform.ini') -Raw
if ($ini -notmatch '(?m)^PluginFolders=Data/Platform/Plugins\r?$' -or $ini -notmatch '(?m)^BackendName=meridian\r?$') { throw 'Production folder and Meridian backend required' }
$modName="Aetherius - Meridian - TESTE - $Suffix"
$modTarget=Join-Path $moRoot "mods/$modName"
$profiles=@(
  [pscustomobject]@{source='AETHERIUS - GRAFICO - QUALIDADE';name="TESTE MERIDIAN - MINIMO - $Suffix";minimal=$true},
  [pscustomobject]@{source='AETHERIUS - GRAFICO - QUALIDADE';name="TESTE MERIDIAN - QUALIDADE - $Suffix";minimal=$false},
  [pscustomobject]@{source='AETHERIUS - PERFORMANCE';name="TESTE MERIDIAN - PERFORMANCE - $Suffix";minimal=$false}
)
$profileFiles=@('archives.txt','initweaks.ini','loadorder.txt','lockedorder.txt','modlist.txt','plugins.txt','settings.ini')
$dependencies=@('Meridian UI - GPU Rendered User Interface for Skyrim - DLSS 5 Support','Address Library for SKSE Plugins')
foreach ($target in @($modTarget)+@($profiles | ForEach-Object { Join-Path $moRoot "profiles/$($_.name)" })) {
  $absolute=[IO.Path]::GetFullPath($target)
  if (-not $absolute.StartsWith($moRoot+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Target escapes MO2' }
  if (Test-Path -LiteralPath $absolute) { throw "Refusing existing target: $absolute" }
}
foreach ($source in @($profiles.source | Sort-Object -Unique)) {
  foreach ($file in $profileFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $moRoot "profiles/$source/$file") -PathType Leaf)) { throw "Missing source: $source/$file" }
  }
  $list=@(Get-Content -LiteralPath (Join-Path $moRoot "profiles/$source/modlist.txt"))
  if ($list.Count -lt 2 -or -not $list[0].StartsWith('#')) { throw 'Unexpected MO2 modlist format' }
  $settings=Get-Content -LiteralPath (Join-Path $moRoot "profiles/$source/settings.ini") -Raw
  if ($settings -notmatch '(?m)^LocalSaves=(true|false)\r?$') { throw 'Unexpected LocalSaves setting' }
  foreach ($dependency in $dependencies) {
    if (('+'+$dependency) -notin $list) { throw "Dependency not enabled in source: $dependency" }
  }
}
if (Get-ChildItem -LiteralPath (Join-Path $moRoot 'overwrite') -Recurse -File | Where-Object { $_.Name -in @('SkyrimPlatform.dll','SkyrimPlatformImpl.dll','libnode.dll','SkyrimPlatform.ini','skymp5-client.js') }) { throw 'MO2 overwrite could override candidate; inspect before installing' }
$protectedPaths=@((Join-Path $moRoot 'ModOrganizer.ini'))+@(foreach ($source in @($profiles.source | Sort-Object -Unique)) { foreach ($file in $profileFiles) { Join-Path $moRoot "profiles/$source/$file" } })
$before=@($protectedPaths | ForEach-Object { [pscustomobject]@{path=$_;sha256=(Get-FileHash -LiteralPath $_).Hash} })
$plan=[ordered]@{status='PREVIEW';instance=$moRoot;mod=$modName;target=$modTarget;profiles=$profiles;entries=$entries;candidateManifest=$CandidateManifest;sourceProfilesUnchanged=$true;activeProfileChanged=$false;savesCopied=$false;gameStarted=$false}
if (-not $Install) { $plan | ConvertTo-Json -Depth 6; return }
New-Item -ItemType Directory -Path $modTarget | Out-Null
foreach ($folder in @('SKSE','Platform','MeridianUI')) { Copy-Item -LiteralPath (Join-Path $overlay $folder) -Destination $modTarget -Recurse }
foreach ($profile in $profiles) {
  $target=Join-Path $moRoot "profiles/$($profile.name)"
  New-Item -ItemType Directory -Path $target | Out-Null
  foreach ($file in $profileFiles) { Copy-Item -LiteralPath (Join-Path $moRoot "profiles/$($profile.source)/$file") -Destination (Join-Path $target $file) }
  $list=@(Get-Content -LiteralPath (Join-Path $target 'modlist.txt'))
  if ($profile.minimal) {
    $list=@($list | ForEach-Object { if ($_ -match '^[+-](.+)$') { if ($Matches[1] -in $dependencies) { '+'+$Matches[1] } else { '-'+$Matches[1] } } else { $_ } })
    $order=@(Get-Content -LiteralPath (Join-Path $target 'loadorder.txt') | Where-Object { $_.StartsWith('#') -or $_ -in @('Skyrim.esm','Update.esm','Dawnguard.esm','HearthFires.esm','Dragonborn.esm','_ResourcePack.esl') -or $_ -match '^cc.+\.(esm|esl)$' })
    $order | Set-Content -LiteralPath (Join-Path $target 'loadorder.txt') -Encoding utf8NoBOM
    foreach ($file in @('plugins.txt','lockedorder.txt')) { '# This file was automatically generated by Mod Organizer.' | Set-Content -LiteralPath (Join-Path $target $file) -Encoding utf8NoBOM }
  }
  @($list[0];('+'+$modName);$list[1..($list.Count-1)]) | Set-Content -LiteralPath (Join-Path $target 'modlist.txt') -Encoding utf8NoBOM
  $settings=Get-Content -LiteralPath (Join-Path $target 'settings.ini') -Raw
  $settings -replace '(?m)^LocalSaves=(true|false)\r?$','LocalSaves=true' | Set-Content -LiteralPath (Join-Path $target 'settings.ini') -Encoding utf8NoBOM
}
foreach ($entry in $entries) { if ((Get-FileHash -LiteralPath (Join-Path $modTarget $entry.path)).Hash -ne $entry.sha256) { throw "Installed hash mismatch: $($entry.path)" } }
foreach ($row in $before) { if ((Get-FileHash -LiteralPath $row.path).Hash -ne $row.sha256) { throw "Protected file modified: $($row.path)" } }
$plan.status='INSTALLED - RUNTIME VALIDATION PENDING'
$plan.protectedFiles=$before
$audit=Join-Path $root 'local-artifacts'
New-Item -ItemType Directory -Path $audit -Force | Out-Null
$plan | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $audit "deployment-$Suffix.json") -Encoding utf8NoBOM
$plan | ConvertTo-Json -Depth 6
