param(
  [Parameter(Mandatory)][string]$NativeSource,
  [Parameter(Mandatory)][string]$NativeBin,
  [Parameter(Mandatory)][string]$Libnode,
  [string]$Objdump='objdump',
  [string]$Label=([DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if ($Label -notmatch '^\d{8}-\d{6}$') { throw 'Timestamp label required' }
$native=[IO.Path]::GetFullPath($NativeSource)
$cache=Get-Content -LiteralPath (Join-Path $native 'build/CMakeCache.txt') -Raw
if ($cache -notmatch '(?m)^SKYRIM_PLATFORM_MERIDIAN:BOOL=ON\r?$') { throw 'Not a Meridian build' }
if ((Get-FileHash -LiteralPath $Libnode).Hash -ne 'F22C87FA55E3C0E8B5626FD94C801BA7F1BF581E6FA555CE702C44E553D2D210') { throw 'Wrong libnode dependency' }
$sources=@(foreach ($file in Get-ChildItem -LiteralPath (Join-Path $root 'source/runtime') -Recurse -File) {
  $relative=[IO.Path]::GetRelativePath((Join-Path $root 'source/runtime'),$file.FullName)
  $builtSource=Join-Path $native $relative
  $hash=(Get-FileHash -LiteralPath $file.FullName).Hash
  if (-not (Test-Path -LiteralPath $builtSource -PathType Leaf) -or (Get-FileHash -LiteralPath $builtSource).Hash -ne $hash) { throw "Native source differs from canonical snapshot: $relative" }
  [pscustomobject]@{path=('source/runtime/'+$relative.Replace('\','/'));sha256=$hash}
})
$pe=@(foreach ($name in @('SkyrimPlatform.dll','SkyrimPlatformImpl.dll')) {
  $file=Join-Path $NativeBin $name
  $dump=& $Objdump -p $file
  if ($LASTEXITCODE -ne 0) { throw 'PE inspection failed' }
  $imports=@($dump | ForEach-Object { if ($_ -match '^\s*DLL Name:\s*(.+)$') { $Matches[1].Trim() } })
  $exports=@($dump | ForEach-Object { if ($_ -match '^\s*\[\s*\d+\]\s+(SKSEPlugin_\w+|SkyrimPlatform_\w+)\s*$') { $Matches[1] } })
  $required=if ($name -eq 'SkyrimPlatform.dll') { @('SKSEPlugin_Load','SKSEPlugin_Query','SKSEPlugin_Version','SkyrimPlatform_IpcSend','SkyrimPlatform_IpcSubscribe','SkyrimPlatform_IpcUnsubscribe') } else { @('SKSEPlugin_Load_Impl','SkyrimPlatform_IpcSend_Impl','SkyrimPlatform_IpcSubscribe_Impl','SkyrimPlatform_IpcUnsubscribe_Impl') }
  foreach ($symbol in $required) { if ($symbol -notin $exports) { throw "Missing export: $symbol" } }
  if (@($imports | Where-Object { $_ -match '^(libcef|chrome_elf|MSVCP[0-9_]+D|VCRUNTIME[0-9_]+D|ucrtbased)\.dll$' }).Count) { throw 'Legacy CEF or Debug CRT import' }
  [pscustomobject]@{file=$name;sha256=(Get-FileHash -LiteralPath $file).Hash;imports=$imports;exports=$exports;fileVersion=(Get-Item -LiteralPath $file).VersionInfo.FileVersion}
})
$client=Join-Path $root 'source/client/client'
$bundle=Join-Path $root 'source/client/build/dist/client/Data/Platform/Plugins/skymp5-client.js'
$newestTs=Get-ChildItem -LiteralPath (Join-Path $client 'src') -Recurse -File -Filter '*.ts' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ((Get-Item -LiteralPath $bundle).LastWriteTimeUtc -lt $newestTs.LastWriteTimeUtc) { throw 'Rebuild client before staging' }
$newestNative=Get-ChildItem -LiteralPath (Join-Path $root 'source/runtime/skyrim-platform/src/platform_se') -Recurse -File | Where-Object Extension -In '.cpp','.h','.hpp' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ((Get-Item -LiteralPath (Join-Path $NativeBin 'SkyrimPlatformImpl.dll')).LastWriteTimeUtc -lt $newestNative.LastWriteTimeUtc) { throw 'Rebuild native before staging' }
$version=Join-Path $root "binaries/candidate-$Label"
$symbolZip=Join-Path $root "symbols/candidate-$Label-pdb.zip"
$candidate=Join-Path $root "packages/candidate-$Label"
foreach ($path in @($version,$symbolZip,$candidate)) { if (Test-Path -LiteralPath $path) { throw "Refusing existing artifact: $path" } }
New-Item -ItemType Directory -Path $version,$candidate | Out-Null
foreach ($name in @('SkyrimPlatform.dll','SkyrimPlatformImpl.dll')) { Copy-Item -LiteralPath (Join-Path $NativeBin $name) -Destination $version }
$zip=[IO.Compression.ZipFile]::Open($symbolZip,[IO.Compression.ZipArchiveMode]::Create)
try { foreach ($name in @('SkyrimPlatform.pdb','SkyrimPlatformImpl.pdb')) { [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,(Join-Path $NativeBin $name),$name,[IO.Compression.CompressionLevel]::Optimal) | Out-Null } } finally { $zip.Dispose() }
$overlay=Join-Path $root 'overlay/current'
Copy-Item -LiteralPath (Join-Path $version 'SkyrimPlatform.dll') -Destination (Join-Path $overlay 'SKSE/Plugins/SkyrimPlatform.dll')
Copy-Item -LiteralPath (Join-Path $version 'SkyrimPlatformImpl.dll') -Destination (Join-Path $overlay 'Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll')
Copy-Item -LiteralPath $bundle -Destination (Join-Path $overlay 'Platform/Plugins/skymp5-client.js')
$mod=Join-Path $candidate 'Aetherius - Meridian - TESTE'
New-Item -ItemType Directory -Path $mod | Out-Null
foreach ($folder in @('SKSE','Platform','MeridianUI')) { Copy-Item -LiteralPath (Join-Path $overlay $folder) -Destination $mod -Recurse }
Copy-Item -LiteralPath $Libnode -Destination (Join-Path $mod 'Platform/Distribution/RuntimeDependencies/libnode.dll')
$entries=@(foreach ($file in Get-ChildItem -LiteralPath $mod -Recurse -File) { [pscustomobject]@{path=[IO.Path]::GetRelativePath($mod,$file.FullName).Replace('\','/');bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName).Hash} })
if ($entries.Count -ne 8) { throw 'Exactly eight files expected' }
$zipPath=Join-Path $candidate 'aetherius-meridian-local-test.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($mod,$zipPath,[IO.Compression.CompressionLevel]::Optimal,$false)
$manifest=[ordered]@{schema=1;label=$Label;createdUtc=[DateTime]::UtcNow.ToString('o');status='EXPERIMENTAL - NOT HOMOLOGATED - DO NOT PUBLISH';intendedUse='MO2 test overlay only; not a launcher client package';entries=$entries;nativePe=$pe;zipPath=$zipPath;zipSha256=(Get-FileHash -LiteralPath $zipPath).Hash;deployment='none';release='none'}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $candidate 'manifest.json') -Encoding utf8NoBOM
$sources+=@(foreach ($file in Get-ChildItem -LiteralPath (Join-Path $client 'src'),(Join-Path $client 'meridian') -Recurse -File) { [pscustomobject]@{path=[IO.Path]::GetRelativePath($root,$file.FullName).Replace('\','/');sha256=(Get-FileHash -LiteralPath $file.FullName).Hash} })
$sources | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $root "reports/candidate-$Label-source-hashes.json") -Encoding utf8NoBOM
[ordered]@{label=$Label;status=$manifest.status;entries=$entries;nativePe=$pe;symbolsSha256=(Get-FileHash -LiteralPath $symbolZip).Hash;coverage='Verified snapshot equality to native build tree, source/binary freshness, exports and direct imports; not game validation. Historical binaries remain intact.'} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root "reports/candidate-$Label.json") -Encoding utf8NoBOM
Write-Output "STAGED ONLY: $candidate"
