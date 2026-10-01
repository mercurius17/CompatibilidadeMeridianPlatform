param([switch]$Meridian)
$ErrorActionPreference = 'Stop'
$runtime = '<WORKSPACE>\aetherius-runtime-meridian'
$env:GIT_CONFIG_COUNT = '2'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = $runtime.Replace('\', '/')
$env:GIT_CONFIG_KEY_1 = 'safe.directory'
$env:GIT_CONFIG_VALUE_1 = ($runtime + '\vcpkg').Replace('\', '/')
$env:VCPKG_DISABLE_METRICS = '1'
$env:VCPKG_MAX_CONCURRENCY = '8'
$env:VCPKG_VISUAL_STUDIO_PATH = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
$env:PATH = '<USER>\AppData\Roaming\npm;C:\Program Files\nodejs;' + $env:PATH
$logDirectory = '<WORKSPACE>\outgoing\meridian-build-logs'
New-Item -ItemType Directory -Force -Path $logDirectory | Out-Null
$runId = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')
$buildLabel = if ($Meridian) { 'meridian' } else { 'control' }
$backendFlag = if ($Meridian) { '-DSKYRIM_PLATFORM_MERIDIAN=ON' } else { '-DSKYRIM_PLATFORM_MERIDIAN=OFF' }
& 'C:\Program Files\CMake\bin\cmake.exe' -S $runtime -B ($runtime + '\build') `
    -G 'Visual Studio 17 2022' -A x64 `
    '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' `
    $backendFlag `
    -DBUILD_UNIT_TESTS=OFF -DBUILD_CLIENT=OFF -DBUILD_FRONT=OFF -DBUILD_GAMEMODE=OFF `
    -DBUILD_SCRIPTS=OFF -DINSTALL_CLIENT_DIST=OFF -DGENERATE_CLIENT_SETTINGS=OFF `
    -DPREPARE_NEXUS_ARCHIVES=OFF -DNO_CLEAN_AFTER_BUILD=ON -DSKYMP_WARNINGS_AS_ERRORS=OFF `
    2>&1 | Tee-Object -FilePath (Join-Path $logDirectory "$buildLabel-configure-$runId.log")
if ($LASTEXITCODE -ne 0) { throw "$buildLabel configure failed ($LASTEXITCODE); see configure log" }
& 'C:\Program Files\CMake\bin\cmake.exe' --build ($runtime + '\build') --config Release `
    --target skyrim_platform skyrim_platform_entry --parallel 8 `
    2>&1 | Tee-Object -FilePath (Join-Path $logDirectory "$buildLabel-build-$runId.log")
if ($LASTEXITCODE -ne 0) { throw "$buildLabel build failed ($LASTEXITCODE); see build log" }

