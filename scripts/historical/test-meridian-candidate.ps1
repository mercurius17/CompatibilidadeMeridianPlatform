param([Parameter(Mandatory)][string]$Candidate)
$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath (Join-Path $Candidate 'manifest.json') -Raw | ConvertFrom-Json
$mod = Join-Path $Candidate 'Aetherius - Meridian - TESTE'
if ($manifest.status -ne 'EXPERIMENTAL - NOT HOMOLOGATED - DO NOT PUBLISH') { throw 'Unexpected candidate status' }
if ((Get-FileHash -LiteralPath $manifest.zipPath).Hash -ne $manifest.zipSha256) { throw 'ZIP digest changed' }
$zip = [IO.Compression.ZipFile]::OpenRead($manifest.zipPath)
try {
  $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
  foreach ($entry in $zip.Entries) {
    $path = $entry.FullName.Replace('\','/')
    if ($path.EndsWith('/')) { continue }
    if ($path -match '(^/|:|(^|/)\.\.(/|$))' -or -not $seen.Add($path)) { throw "Unsafe/duplicate ZIP path: $path" }
    $expected = @($manifest.entries | Where-Object path -CEQ $path)
    if ($expected.Count -ne 1 -or $entry.Length -ne $expected[0].bytes) { throw "Unexpected ZIP entry: $path" }
    $stream = $entry.Open()
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $digest = [Convert]::ToHexString($sha.ComputeHash($stream)) } finally { $stream.Dispose(); $sha.Dispose() }
    if ($digest -ne $expected[0].sha256) { throw "ZIP content mismatch: $path" }
    if ((Get-FileHash -LiteralPath (Join-Path $mod $path)).Hash -ne $digest) { throw "Staged file mismatch: $path" }
  }
  if ($seen.Count -ne 8 -or $seen.Count -ne $manifest.entries.Count) { throw 'Expected exactly eight test-overlay files' }
} finally { $zip.Dispose() }
$files = @(Get-ChildItem -LiteralPath $mod -Recurse -File)
if ($files.Count -ne 8) { throw 'Extra files in staged mod' }
$ini = Get-Content -LiteralPath (Join-Path $mod 'SKSE/Plugins/SkyrimPlatform.ini') -Raw
if ($ini -notmatch '(?m)^PluginFolders=Data/Platform/Plugins\r?$' -or $ini -notmatch '(?m)^BackendName=meridian\r?$') { throw 'Launcher-safe Meridian/production plugin configuration required' }
$system = @('KERNEL32.dll','USER32.dll','WS2_32.dll','ADVAPI32.dll','ole32.dll','CRYPT32.dll','SHELL32.dll','VERSION.dll','IPHLPAPI.DLL','USERENV.dll','PSAPI.DLL','WINMM.dll','bcrypt.dll','dbghelp.dll','ntdll.dll','D3D11.dll','DXGI.dll','GDI32.dll','RPCRT4.dll')
$pe = foreach ($file in ($files | Where-Object Extension -EQ '.dll')) {
  $output = & 'C:\MinGW64\bin\objdump.exe' -p $file.FullName
  if ($LASTEXITCODE -ne 0) { throw "Cannot inspect PE: $($file.FullName)" }
  $imports = @($output | ForEach-Object { if ($_ -match '^\s*DLL Name:\s*(.+)$') { $Matches[1].Trim() } })
  foreach ($dependency in $imports) {
    if ($dependency -match '^(libcef|chrome_elf|MSVCP[0-9_]+D|VCRUNTIME[0-9_]+D|ucrtbased)\.dll$') { throw "Forbidden dependency: $dependency" }
    if ($dependency -eq 'libnode.dll') {
      if ($file.Name -ne 'SkyrimPlatformImpl.dll') { throw 'Unexpected private dependency edge' }
      continue
    }
    if ($dependency -notin $system -or -not (Test-Path -LiteralPath (Join-Path 'C:\Windows\System32' $dependency))) { throw "Unreviewed/missing system dependency: $dependency" }
  }
  [pscustomobject]@{file=$file.Name; imports=$imports; sha256=(Get-FileHash -LiteralPath $file.FullName).Hash}
}
[pscustomobject]@{
  result='PASS'; zipFiles=$seen.Count; privateDllClosure=@($pe)
  coverage='ZIP bytes, paths, staged files and static imports of all three private DLLs; Windows system DLLs are boundary leaves. Dynamic Meridian API and game behavior require runtime validation.'
} | ConvertTo-Json -Depth 6

