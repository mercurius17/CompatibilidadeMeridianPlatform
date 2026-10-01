param(
  [Parameter(Mandatory)][string]$Label,
  [string]$MO2='C:\modOrganizer',
  [string]$LogDirectory=(Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games/Skyrim Special Edition/SKSE')
)
$ErrorActionPreference='Stop'
if ($Label -notmatch '^[a-zA-Z0-9-]+$') { throw 'Invalid evidence label' }
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$output=Join-Path $root 'local-artifacts'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$skse=Get-Content -LiteralPath (Join-Path $LogDirectory 'skse64.log') -Raw
$platform=Get-Content -LiteralPath (Join-Path $LogDirectory 'skyrim-platform.log') -Raw
$meridian=Get-Content -LiteralPath (Join-Path $LogDirectory 'MeridianUI.log') -Raw
$gameProcesses=@(Get-Process SkyrimSE -ErrorAction SilentlyContinue)
$moduleRows=@()
$moduleAvailable=$false
if ($gameProcesses.Count -eq 1) {
  try {
    $modules=$gameProcesses[0].Modules
    $moduleAvailable=$null -ne $modules -and $modules.Count -gt 0
    if ($moduleAvailable) {
      $moduleRows=@($modules | Where-Object ModuleName -Match 'SkyrimPlatform|Meridian|libcef|chrome_elf|libnode|MpClient' | ForEach-Object { [pscustomobject]@{name=$_.ModuleName;sha256=(Get-FileHash -LiteralPath $_.FileName).Hash;version=$_.FileVersionInfo.FileVersion} })
    }
  } catch { $moduleAvailable=$false }
}
$anchors=[ordered]@{
  skse=@($skse -split '\r?\n' | Where-Object { $_ -match '^SKSE64 runtime:|^plugin (MeridianUIPlugin|SkyrimPlatform)\.dll .*loaded correctly' })
  platform=@($platform -split '\r?\n' | Where-Object { $_ -match 'browser backend: Meridian|Meridian.View/1 acquired|Meridian HUD:|Meridian: (created view|DOM ready|malformed/stale|.*limit reached)' })
  meridian=@($meridian -split '\r?\n' | Where-Object { $_ -match 'CEFService::CEFInitialize successfully|Meridian.View/1 requested|ViewAPIController: created Meridian.View/aetherius-client|CEFService: browser [0-9]+ registered|FocusArbiter:|SyncCopy fallback enabled|CEFSyncCopyRenderLayer: texture created|Shutdown|destroyed' })
}
$anchors.platform+=@($platform -split '\r?\n' | Where-Object { $_ -match 'Normal engine shutdown requested on the game thread|Normal shutdown failed:' })
$logMetadata=@(foreach ($name in @('skse64.log','skyrim-platform.log','MeridianUI.log')) {
  $file=Get-Item -LiteralPath (Join-Path $LogDirectory $name)
  [pscustomobject]@{name=$name;lastWriteUtc=$file.LastWriteTimeUtc.ToString('o');bytes=$file.Length}
})
$report=[ordered]@{
  capturedUtc=[DateTime]::UtcNow.ToString('o');label=$Label
  profile=@(Get-Content -LiteralPath (Join-Path $MO2 'ModOrganizer.ini') | Where-Object { $_ -match '^selected_profile=' })
  currentGameProcessCount=$gameProcesses.Count
  processStartedUtc=if ($gameProcesses.Count -eq 1) { try { $gameProcesses[0].StartTime.ToUniversalTime().ToString('o') } catch { $null } } else { $null }
  logMetadata=$logMetadata
  sksePluginLoaded=($skse -match '(?m)^plugin SkyrimPlatform\.dll .*loaded correctly')
  meridianPluginLoaded=($skse -match '(?m)^plugin MeridianUIPlugin\.dll .*loaded correctly')
  directoryMonitorError=($platform -match 'DirectoryMonitor\(.*failed')
  javascriptInitializationError=($platform -match '(?m)JavaScript error: [^\r\n\s]|Failed to (create environment|execute script|initialize NodeInstance)')
  viewCreated=($platform -match 'Meridian: created view')
  domReady=($platform -match 'Meridian: DOM ready')
  moduleInspectionAvailable=$moduleAvailable
  modules=$moduleRows
  legacySubprocessCount=@(Get-Process SkyrimPlatformCEF -ErrorAction SilentlyContinue).Count
  logAnchors=$anchors
  coverage='Snapshot plus existing log excerpts, possibly from a previous session: correlate logMetadata and processStartedUtc manually. Module hashes require moduleInspectionAvailable=true. No-process/legacy count after exit cannot establish modules or subprocesses for the prior session. DOM readiness is not proof of handshake, authenticated gameplay, voice, or stability. UI checks require separate observations.'
}
$path=Join-Path $output "evidence-$Label.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding utf8NoBOM
Write-Output "Saved initialization evidence: $path"
$report.GetEnumerator() | Where-Object Key -NotIn @('logAnchors','modules') | ForEach-Object { Write-Output "$($_.Key): $($_.Value)" }
