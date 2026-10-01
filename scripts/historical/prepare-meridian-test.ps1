param()
$ErrorActionPreference = 'Stop'
$workspace = '<WORKSPACE>'
$native = Join-Path $workspace 'aetherius-runtime-meridian'
$client = Join-Path $workspace 'aetherius-client-meridian'
$bin = Join-Path $native 'build\skyrim-platform\_platform_se\bin\Release'
$cache = Get-Content -LiteralPath (Join-Path $native 'build\CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^SKYRIM_PLATFORM_MERIDIAN:BOOL=ON\r?$') { throw 'Refusing to stage a legacy build' }
$peJson = & (Join-Path $workspace 'plans\inspect-meridian-native.ps1') `
  -Implementation (Join-Path $bin 'SkyrimPlatformImpl.dll') `
  -Entry (Join-Path $bin 'SkyrimPlatform.dll') -RequireNoLegacyCef
$pe = $peJson | ConvertFrom-Json
$runId = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$destination = Join-Path $workspace "outgoing\meridian-test-$runId"
if (Test-Path -LiteralPath $destination) { throw 'Test output already exists; never overwrite an earlier candidate' }
$mod = Join-Path $destination 'Aetherius - Meridian - TESTE'
New-Item -ItemType Directory -Path $mod | Out-Null
$inputs = [ordered]@{
  'SKSE/Plugins/SkyrimPlatform.dll' = (Join-Path $bin 'SkyrimPlatform.dll')
  'Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll' = (Join-Path $bin 'SkyrimPlatformImpl.dll')
  # LoadLibraryEx scopes libnode to the DLL's directory, including in MO2's VFS.
  'Platform/Distribution/RuntimeDependencies/libnode.dll' = (Join-Path $workspace 'outgoing\meridian-baseline\client\Data\Platform\Distribution\RuntimeDependencies\libnode.dll')
  'SKSE/Plugins/SkyrimPlatform.ini' = (Join-Path $client 'client\meridian\SkyrimPlatform.ini')
  'MeridianUI/aetherius-client/index.html' = (Join-Path $client 'client\index.html')
  'MeridianUI/aetherius-client/bootstrap.js' = (Join-Path $client 'client\meridian\bootstrap.js')
  'MeridianUI/aetherius-client/widgets.js' = (Join-Path $client 'client\meridian\widgets.js')
  'Platform/Plugins/skymp5-client.js' = (Join-Path $client 'build\dist\client\Data\Platform\Plugins\skymp5-client.js')
}
$bundle = Get-Item -LiteralPath $inputs['Platform/Plugins/skymp5-client.js']
$newestTs = Get-ChildItem -LiteralPath (Join-Path $client 'client\src') -Recurse -File -Filter '*.ts' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($newestTs.LastWriteTimeUtc -gt $bundle.LastWriteTimeUtc) { throw 'Client bundle is older than source; rebuild first' }
$newestCpp = Get-ChildItem -LiteralPath (Join-Path $native 'skyrim-platform\src\platform_se') -Recurse -File | Where-Object Extension -In '.cpp','.h','.hpp' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($newestCpp.LastWriteTimeUtc -gt (Get-Item -LiteralPath $inputs['Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll']).LastWriteTimeUtc) { throw 'Native build is older than source; rebuild first' }
$entries = foreach ($relative in $inputs.Keys) {
  $target = Join-Path $mod $relative.Replace('/', '\')
  New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
  Copy-Item -LiteralPath $inputs[$relative] -Destination $target
  [pscustomobject]@{ path=$relative; bytes=(Get-Item -LiteralPath $target).Length; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash }
}
$symbols = Join-Path $destination 'symbols'
New-Item -ItemType Directory -Path $symbols | Out-Null
Copy-Item -LiteralPath (Join-Path $bin 'SkyrimPlatform.pdb'), (Join-Path $bin 'SkyrimPlatformImpl.pdb') -Destination $symbols
$zipPath = Join-Path $destination 'aetherius-meridian-local-test.zip'
[System.IO.Compression.ZipFile]::CreateFromDirectory($mod, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
$baseline = Get-Content -LiteralPath (Join-Path $workspace 'outgoing\meridian-baseline\manifest.json') -Raw | ConvertFrom-Json
$manifest = [ordered]@{
  schema=1; createdUtc=[DateTime]::UtcNow.ToString('o'); status='EXPERIMENTAL - NOT HOMOLOGATED - DO NOT PUBLISH'
  intendedUse='MO2 test overlay only; not a launcher client package'
  game='Steam 1.6.1170'; skse='2.2.6'; meridian='1.5.0 / Meridian.View/1'
  nativeBase=$baseline.nativeCommit; clientBase=$baseline.clientCommit; vcpkgCommit=$baseline.vcpkgCommit
  vcpkgPinOrigin=$baseline.vcpkgPinOrigin; nativePe=$pe; entries=@($entries)
  zipPath=$zipPath; zipSha256=(Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash
  sourceState='working-tree candidate; base commits plus source file hashes accompany this local artifact'
  profiles='unchanged; duplicate a profile for testing, never add this overlay to a published modlist'
  deployment='none'; release='none'
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $destination 'manifest.json') -Encoding utf8
$sourceFiles = @(
  @(git -C $native diff --name-only)
  @(git -C $native ls-files --others --exclude-standard)
) | Sort-Object -Unique
$sourceReport = foreach ($relative in $sourceFiles) {
  if ($relative) { [pscustomobject]@{ repository='runtime'; path=$relative; sha256=(Get-FileHash -LiteralPath (Join-Path $native $relative)).Hash } }
}
$clientFiles = @(@(git -C $client diff --name-only); @(git -C $client ls-files --others --exclude-standard)) | Sort-Object -Unique
$sourceReport += foreach ($relative in $clientFiles) {
  if ($relative) { [pscustomobject]@{ repository='client'; path=$relative; sha256=(Get-FileHash -LiteralPath (Join-Path $client $relative)).Hash } }
}
$sourceReport | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $destination 'source-hashes.json') -Encoding utf8
Write-Output "STAGED (not installed or published): $destination"
Write-Output "MO2 test archive: $zipPath"

