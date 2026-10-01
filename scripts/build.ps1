param(
  [ValidateSet('Native','Client','All')][string]$Component='All',
  [switch]$Prepare,
  [string]$CMake='cmake',
  [string]$Node='node',
  [string]$Yarn='yarn',
  [string]$VisualStudio='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$work=Join-Path $root 'build-work'
if ($Prepare) {
  if (Test-Path -LiteralPath $work) { throw 'build-work exists; choose a fresh checkout rather than overwrite prior work' }
  New-Item -ItemType Directory -Path $work | Out-Null
  Copy-Item -LiteralPath (Join-Path $root 'source/runtime'), (Join-Path $root 'source/client') -Destination $work -Recurse
  # Standalone source snapshots have no .git history; this marks their origin.
  foreach ($name in @('runtime','client')) {
    & git -C (Join-Path $work $name) init -b compatibility-snapshot
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize local build snapshot' }
  }
}
if (-not (Test-Path -LiteralPath (Join-Path $work 'runtime/CMakeLists.txt'))) { throw 'Run with -Prepare once in a fresh checkout' }
$env:VCPKG_DISABLE_METRICS='1'
$env:VCPKG_MAX_CONCURRENCY='8'
$env:VCPKG_VISUAL_STUDIO_PATH=$VisualStudio
$env:DEPLOY_PLUGIN='false'
$env:ZIP_PLUGIN='false'
if ($Component -in @('Native','All')) {
  $runtime=Join-Path $work 'runtime'
  $vcpkg=Join-Path $runtime 'vcpkg'
  $pin='cb2981c4e03d421fa03b9bb5044cd1986180e7e4'
  if (-not (Test-Path -LiteralPath $vcpkg)) {
    & git clone https://github.com/microsoft/vcpkg.git $vcpkg
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg clone failed' }
    & git -C $vcpkg checkout --detach $pin
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg pin checkout failed' }
  }
  $actual=& git -C $vcpkg rev-parse HEAD
  if ($actual -ne $pin) { throw 'Unexpected vcpkg revision; refusing dependency drift' }
  if (-not (Test-Path -LiteralPath (Join-Path $vcpkg 'vcpkg.exe'))) {
    & (Join-Path $vcpkg 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed' }
  }
  & $CMake -S $runtime -B (Join-Path $runtime 'build') -G 'Visual Studio 17 2022' -A x64 `
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' -DSKYRIM_PLATFORM_MERIDIAN=ON `
    -DBUILD_UNIT_TESTS=OFF -DBUILD_CLIENT=OFF -DBUILD_FRONT=OFF -DBUILD_GAMEMODE=OFF `
    -DBUILD_SCRIPTS=OFF -DINSTALL_CLIENT_DIST=OFF -DGENERATE_CLIENT_SETTINGS=OFF `
    -DPREPARE_NEXUS_ARCHIVES=OFF -DNO_CLEAN_AFTER_BUILD=ON -DSKYMP_WARNINGS_AS_ERRORS=OFF
  if ($LASTEXITCODE -ne 0) { throw 'Native configure failed' }
  & $CMake --build (Join-Path $runtime 'build') --config Release --target skyrim_platform skyrim_platform_entry --parallel 8
  if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
}
if ($Component -in @('Client','All')) {
  Push-Location (Join-Path $work 'client/client')
  try {
    & $Yarn install --frozen-lockfile --ignore-scripts
    if ($LASTEXITCODE -ne 0) { throw 'Client dependencies failed' }
    & $Node 'node_modules/webpack-cli/bin/cli.js'
    if ($LASTEXITCODE -ne 0) { throw 'Client build failed' }
  } finally { Pop-Location }
}
Write-Output 'Build output remains in build-work. No game, MO2 or launcher deployment.'
