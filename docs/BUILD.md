# Reprodução e fontes correspondentes

## Distribuição autorizada após encerramento dos testes

`scripts/prepare-release.ps1` empacota o candidato já compilado, sem recompilar
ou abrir o jogo. Exige `-PublicationApproved`, a baseline oficial v0.8.1
conferida no GitHub e a pasta do candidato 090924. Verifica todas as entradas,
limita substituições a três arquivos de runtime/client e duas modlists,
inclui o mod completo e avisos de licença nos caminhos do launcher.
Gera `client-v0.8.2.zip`, o ZIP do mod e o relatório com hashes.
A prévia experimental anterior é um artefato histórico, não o release.

## Pré-requisitos

Windows x64, VS2022 Build Tools com C++/MSVC e SDK Windows, Git, CMake, Node e Yarn Classic. A execução original usou versões/pins registrados em PROVENIENCIA.md. Manter os overlays e triplet do projeto: não migrar CommonLib junto da reprodução.

O repositório contém snapshots completos autorizados pelo proprietário, inclusive metadata original de build. Os componentes server aparecem porque a base é um monorepo; este protótipo não alterou nem compilou um release de servidor.

## Script portátil

```powershell
& '.\scripts\build.ps1' -Prepare -Component All
```

O script prepara cópias em build-work, obtém vcpkg no pin informado, configura os targets nativos integrados e compila o bundle sem deploy. Requer acesso de rede para dependências. Não usa SKYRIM_DIR e não instala no jogo/MO2. Uma nova execução sem -Prepare reutiliza build-work; o script recusa sobrescrever a preparação com -Prepare.

**Este wrapper portátil foi escrito para a publicação e validado sintaticamente; não foi executado como build end-to-end.** Os builds realmente executados estão nos logs e no script histórico configure-meridian-control, com seus caminhos de máquina neutralizados. Não afirmar que uma reprodução independente já aconteceu.

A raiz de fonte original exige `<runtime>/build` como build directory. O manifest vcpkg ainda baixa CEF legado para a toolchain completa, embora os targets Meridian não o liguem. Isso não é um fallback de UI.

Targets e flags efetivamente usados:

```powershell
cmake -S <runtime> -B <runtime>/build -G 'Visual Studio 17 2022' -A x64 `
  '-DCMAKE_POLICY_VERSION_MINIMUM=3.5' -DSKYRIM_PLATFORM_MERIDIAN=ON `
  -DBUILD_UNIT_TESTS=OFF -DBUILD_CLIENT=OFF -DBUILD_FRONT=OFF -DBUILD_GAMEMODE=OFF `
  -DBUILD_SCRIPTS=OFF -DINSTALL_CLIENT_DIST=OFF -DGENERATE_CLIENT_SETTINGS=OFF `
  -DPREPARE_NEXUS_ARCHIVES=OFF -DNO_CLEAN_AFTER_BUILD=ON -DSKYMP_WARNINGS_AS_ERRORS=OFF
cmake --build <runtime>/build --config Release `
  --target skyrim_platform skyrim_platform_entry --parallel 8
```

DLLs/PDBs: `build-work/runtime/build/skyrim-platform/_platform_se/bin/Release`.
Bundle: `build-work/client/build/dist/client/Data/Platform/Plugins/skymp5-client.js`.

Para o client foram usados `yarn install --frozen-lockfile --ignore-scripts` e `node node_modules/webpack-cli/bin/cli.js`, com DEPLOY_PLUGIN=false e ZIP_PLUGIN=false. Não usar o modo de deploy antigo apontando ao jogo.

## Testes existentes

A retomada executou `scripts/test-meridian.ps1` com Node/CMake/CTest explícitos:
26 testes Node e dois C++ passaram com VS2022/MSVC. O script testa a fonte
canônica em `source`, exige dependências client já instaladas e não instala no
jogo. Logs locais ficam em `local-artifacts/tests`.

Client, após instalar dependências na cópia de build:

```powershell
node --test meridian/*.test.cjs
```

Executar no diretório `build-work/client/client`.

C++ standalone:

```powershell
cmake -S source/runtime/skyrim-platform/tests/meridian -B local-artifacts/native-tests `
  -G 'Visual Studio 17 2022' -A x64
cmake --build local-artifacts/native-tests --config Release
ctest --test-dir local-artifacts/native-tests -C Release --output-on-failure
```

Os dois testes não carregam Skyrim/CEF. Provider/DOM mockados comprovam fronteiras determinísticas, não a compatibilidade real.

## Fontes da versão smoke e do controle

O snapshot atual inclui as alterações da revisão descrita em
REVISAO-CONTINUACAO.md. O par 034454 antecede a alteração Win32Api.cpp de
encaminhamento à thread do jogo; revertê-la em uma cópia separada ao reconstruir
esse par histórico. Para reconstruir a fonte da versão smoke, adicionalmente:

- Settings.h: repor a versão do commit-base (esse arquivo não era alterado no smoke).
- Settings.cpp: remover somente o novo conditional Meridian do default PluginFolders, mantendo a seleção BrowserBackend meridian já existente.
- INI client: repor o conteúdo do candidato smoke, somente `[Browser] BackendName=meridian` e o comentário de restrição de origem.
- Conferir todos os arquivos alterados contra `reports/smoke-source-hashes.json`; o INI está identificado no manifest smoke.

O controle corresponde ao commit-base do runtime, com a opção SKYMP_WARNINGS_AS_ERRORS adicionada em cmake/apply_default_settings.cmake e OFF no build. Não possui o adaptador Meridian. Hash diferente em uma reprodução pode decorrer de timestamps, toolchain e debug paths; uma reconstrução funcional não é promessa de bytes idênticos.

Não é necessário obter o private Git history para ler as fontes deste snapshot autorizado. Caso use as bases originais para reconstruir versões históricas, será necessário acesso aos repositórios privados. O repositório novo não preserva o histórico original.

## Overlay

O diretório `overlay/current` é Data-relativo, mas deliberadamente não contém libnode/CEF/Meridian runtime. Após obter a cópia autorizada de Node exata:

```powershell
& '.\scripts\prepare-overlay.ps1' -Libnode 'C:\caminho\autorizado\libnode.dll'
```

O script confere o hash Node, cria um ZIP local em packages e não instala/inicia nada. É um wrapper de empacotamento não executado no jogo durante esta publicação. Consulte TESTE-E-HOMOLOGACAO.md antes de usar.

PDBs de cada par estão separados em symbols; não usar símbolos de um par com outra DLL.

## Prévia offline do pacote completo

`scripts/prepare-client-preview.ps1 -BaselineZip <zip-v0.8.1-congelado> -CandidateDirectory <candidate-090924>`
foi executado e verificou um ZIP local com raiz `client/`, preservando os bytes
de todas as entradas fora da allowlist de oito arquivos do overlay. Cada entrada
resultante foi reaberta e conferida por tamanho/hash. O script não instala,
não obtém profiles do MO2 ativo e não publica. O nome experimental impede
confundi-lo com o asset `client-v<versão>.zip` esperado pelo launcher.

A baseline é histórica. Seus profiles e recursos CEF antigos foram preservados
deliberadamente; ainda precisam de revisão antes de um pacote instalável.
Manifest: `reports/client-preview-20261001-094535.json`. Não usar esta prévia
para atualizar um client do servidor ou para validar uma instalação de produção.
