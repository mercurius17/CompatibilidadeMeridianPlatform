param([Parameter(Mandatory)][string]$Libnode)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$expected='F22C87FA55E3C0E8B5626FD94C801BA7F1BF581E6FA555CE702C44E553D2D210'
if ((Get-FileHash -LiteralPath $Libnode -Algorithm SHA256).Hash -ne $expected) { throw 'libnode is not the exact baseline dependency' }
$packageRoot=Join-Path $root 'packages'
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
$output=Join-Path $packageRoot ('MeridianPlatform-EXPERIMENTAL-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
if (Test-Path -LiteralPath $output) { throw 'Output already exists' }
New-Item -ItemType Directory -Path $output | Out-Null
foreach ($folder in @('SKSE','Platform','MeridianUI')) {
  Copy-Item -LiteralPath (Join-Path $root "overlay/current/$folder") -Destination $output -Recurse
}
Copy-Item -LiteralPath $Libnode -Destination (Join-Path $output 'Platform/Distribution/RuntimeDependencies/libnode.dll')
$zip=$output + '.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($output,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
Write-Output "LOCAL TEST OVERLAY ONLY: $zip"
Write-Output 'Not installed, not homologated, not a launcher client package. Do not upload as an official client release.'
