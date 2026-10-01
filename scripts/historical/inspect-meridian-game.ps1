param([Parameter(Mandatory)][string]$Candidate)
$ErrorActionPreference = 'Stop'
$deployment = Get-Content -LiteralPath (Join-Path $Candidate 'deployment.json') -Raw | ConvertFrom-Json
$manifest = Get-Content -LiteralPath (Join-Path $Candidate 'manifest.json') -Raw | ConvertFrom-Json
$processes = @(Get-Process SkyrimSE -ErrorAction SilentlyContinue)
if ($processes.Count -ne 1) { throw 'Exactly one running SkyrimSE process is required for a runtime snapshot' }
$process = $processes[0]
$profileLine = Get-Content -LiteralPath (Join-Path $deployment.instance 'ModOrganizer.ini') | Where-Object { $_ -match '^selected_profile=' }
$knownProfile = @($deployment.profiles | Where-Object { $profileLine -eq ('selected_profile=@ByteArray(' + $_.name + ')') })
$modules = @($process.Modules | Where-Object { $_.ModuleName -match 'SkyrimPlatform|Meridian|libcef|chrome_elf|libnode|MpClient' })
$rows = foreach ($module in $modules) {
  $path = $module.FileName
  [pscustomobject]@{name=$module.ModuleName; path=$path; fileVersion=(Get-Item -LiteralPath $path).VersionInfo.FileVersion; sha256=(Get-FileHash -LiteralPath $path).Hash}
}
$candidateDllsMatch = $true
foreach ($name in @('SkyrimPlatform.dll','SkyrimPlatformImpl.dll','libnode.dll')) {
  $expected = @($manifest.entries | Where-Object { [IO.Path]::GetFileName($_.path) -eq $name })
  $actual = @($rows | Where-Object name -EQ $name)
  if ($expected.Count -ne 1 -or $actual.Count -ne 1 -or $actual[0].sha256 -ne $expected[0].sha256) { $candidateDllsMatch = $false }
}
$cef = @($rows | Where-Object name -EQ 'libcef.dll')
$legacyProcess = @(Get-Process SkyrimPlatformCEF -ErrorAction SilentlyContinue)
$cefMajor152 = $cef.Count -eq 1 -and $cef[0].fileVersion -match '^152\.'
$provider = @($rows | Where-Object name -EQ 'MeridianUI.dll')
$report = [pscustomobject]@{
  createdUtc=[DateTime]::UtcNow.ToString('o'); processId=$process.Id; startedUtc=$process.StartTime.ToUniversalTime().ToString('o')
  selectedProfile=$profileLine; selectedTestProfile=$knownProfile.Count -eq 1; candidateDllsMatch=$candidateDllsMatch
  meridianProviderPresent=$provider.Count -eq 1; cefMajor152=$cefMajor152; legacySubprocessCount=$legacyProcess.Count; modules=$rows
  loadSnapshotPass=($knownProfile.Count -eq 1 -and $candidateDllsMatch -and $provider.Count -eq 1 -and $cefMajor152 -and $legacyProcess.Count -eq 0)
  coverage='One module/process snapshot only. Presence is not proof of successful SKSE initialization, bridge readiness, rendering, input, authentication, voice or stability.'
}
$path = Join-Path $Candidate ('runtime-snapshot-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.json')
$report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $path -Encoding utf8
$report | ConvertTo-Json -Depth 6

