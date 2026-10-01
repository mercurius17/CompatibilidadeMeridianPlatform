param([Parameter(Mandatory)][string]$Previous, [Parameter(Mandatory)][string]$Candidate)
$ErrorActionPreference = 'Stop'
if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) { throw 'Close SkyrimSE before replacing test DLLs' }
& (Join-Path $PSScriptRoot 'test-meridian-candidate.ps1') -Candidate $Candidate | Out-Null
$deployment = Get-Content -LiteralPath (Join-Path $Previous 'deployment.json') -Raw | ConvertFrom-Json
$old = Get-Content -LiteralPath (Join-Path $Previous 'manifest.json') -Raw | ConvertFrom-Json
$new = Get-Content -LiteralPath (Join-Path $Candidate 'manifest.json') -Raw | ConvertFrom-Json
$target = [IO.Path]::GetFullPath($deployment.modTarget)
if (-not $target.StartsWith('<MO2>\mods\Aetherius - Meridian - TESTE - ', [StringComparison]::OrdinalIgnoreCase)) { throw 'Refusing any non-test mod target' }
if ((Test-Path -LiteralPath (Join-Path $Candidate 'deployment.json'))) { throw 'New candidate already has a deployment record' }
foreach ($entry in $old.entries) {
  if ((Get-FileHash -LiteralPath (Join-Path $target $entry.path)).Hash -ne $entry.sha256) { throw "Test mod changed independently: $($entry.path)" }
}
foreach ($entry in $new.entries) {
  if ($entry.path -notin $old.entries.path) { throw 'Unexpected new overlay destination' }
}
# Active-profile selection may change while the user tests; do not overwrite it.
# All official profiles, original Data and original INI must still match.
foreach ($row in $deployment.protectedFiles | Where-Object { $_.path -ne '<MO2>\ModOrganizer.ini' }) {
  if ((Test-Path -LiteralPath $row.path) -ne $row.existed) { throw "Original path changed: $($row.path)" }
  if ($row.existed -and (Get-FileHash -LiteralPath $row.path).Hash -ne $row.sha256) { throw "Original file changed: $($row.path)" }
}
foreach ($entry in $new.entries) {
  # Previous ZIP/staging is retained as recovery; only this exact test overlay changes.
  Copy-Item -LiteralPath (Join-Path $Candidate ('Aetherius - Meridian - TESTE/' + $entry.path)) -Destination (Join-Path $target $entry.path)
  if ((Get-FileHash -LiteralPath (Join-Path $target $entry.path)).Hash -ne $entry.sha256) { throw "Updated test file mismatch: $($entry.path)" }
}
$deployment.createdUtc = [DateTime]::UtcNow.ToString('o')
$deployment | Add-Member -NotePropertyName previousCandidate -NotePropertyValue ([IO.Path]::GetFullPath($Previous))
$deployment | Add-Member -NotePropertyName updateScope -NotePropertyValue 'Only existing test overlay files. Test profile names unchanged. Production Data/profiles untouched; previous candidate retained.'
$deployment | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $Candidate 'deployment.json') -Encoding utf8
Write-Output "UPDATED ONLY TEST MOD: $target"
Write-Output "Candidate: $Candidate"
Write-Output 'Use the same TESTE profiles. No game was started; previous candidate preserved.'

