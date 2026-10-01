param(
  [string]$Node='node',
  [string]$CMake='cmake',
  [string]$CTest='ctest'
)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$client=Join-Path $root 'source/client/client'
$out=Join-Path $root 'local-artifacts/tests'
New-Item -ItemType Directory -Path $out -Force | Out-Null
if (-not (Test-Path -LiteralPath (Join-Path $client 'node_modules/typescript'))) { throw 'Install frozen client dependencies first; no automatic install in this test script' }
$tests=@(Get-ChildItem -LiteralPath (Join-Path $client 'meridian') -Filter '*.test.cjs' | ForEach-Object FullName)+@(Join-Path $PSScriptRoot 'input-diagnostic.test.cjs')
& $Node --test @tests 2>&1 | Tee-Object -FilePath (Join-Path $out 'client.log')
if ($LASTEXITCODE -ne 0) { throw 'Client tests failed' }
$build=Join-Path $out 'native'
& $CMake -S (Join-Path $root 'source/runtime/skyrim-platform/tests/meridian') -B $build -G 'Visual Studio 17 2022' -A x64 2>&1 | Tee-Object -FilePath (Join-Path $out 'configure.log')
if ($LASTEXITCODE -ne 0) { throw 'Native unit-test configure failed' }
& $CMake --build $build --config Release 2>&1 | Tee-Object -FilePath (Join-Path $out 'build.log')
if ($LASTEXITCODE -ne 0) { throw 'Native unit-test build failed' }
& $CTest --test-dir $build -C Release --output-on-failure 2>&1 | Tee-Object -FilePath (Join-Path $out 'ctest.log')
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
Write-Output 'PASS: deterministic client/policy/view tests only. No game, CEF or authenticated voice validation.'
