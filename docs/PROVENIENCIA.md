# Proveniência e versões

## Fontes-base

| Componente | Origem | Commit-base |
|---|---|---|
| Runtime Skyrim Platform/Aetherius | AetheriusRP/aetherius-server (privado na conferência) | `1b8f9e4793cd188fb50e3b63cc474096fbf01359` |
| Client Aetherius | AetheriusRP/aetherius-client (privado na conferência) | `113f10ef479dbd12a182b8763cee319f80a3d00e` |
| vcpkg | microsoft/vcpkg | `cb2981c4e03d421fa03b9bb5044cd1986180e7e4` |
| CommonLib original via overlay | CharmedBaryon/CommonLibSSE | `b93280e832f263dbef44e44cbe2936622a02f91a`, quatro patches preservados |
| Meridian SDK público | Meridian UI 1.5.0 instalado | Quatro arquivos copiados sem alteração; hashes na PROVENANCE do SDK |

O repositório Aetherius omitiu o submódulo vcpkg. O pin acima foi recuperado do gitlink upstream consultado durante a execução, **não foi comprovado como o usado para compilar o client oficial**. O source/build exato do binário distribuído também não foi demonstrado. O controle é uma reprodução funcional de compilação, não um rebuild bit-a-bit do release.

Snapshots isolados em branches compat/meridian-ui estavam sem commits da implementação. O repositório de compatibilidade é um novo snapshot; não afirma ser a origem histórica dos releases anteriores. `reports/current-source-changes.json` registra a implementação da publicação inicial. A revisão posterior preserva a contribuição baseada em `9826b86` e registra seus novos hashes em `reports/candidate-20261001-090924-source-hashes.json`.

## Ambiente da compilação realizada

Windows x64; Visual Studio 2022 Build Tools; MSVC 19.44.35228 / VC 14.44.35207; Windows SDK 10.0.26100.0; CMake 4.3.0-rc3; triplet x64-windows-sp; CommonLib SE+AE flatrim, VR desativado; Frida 16.0.19; Node prebuilt do overlay original. Mantidos pins, patches e opções do projeto, sem migração de biblioteca.

SKYMP_WARNINGS_AS_ERRORS continua ON por padrão, mas a reprodução local usou OFF para warnings existentes da toolchain mais nova. Os testes C++ novos usam /W4 /WX. Native release version permanece 2.9.0.0; use hash para distinguir este fork do DLL oficial com a mesma versão.

## Pares de DLLs

| Par | SkyrimPlatform.dll SHA-256 | SkyrimPlatformImpl.dll SHA-256 |
|---|---|---|
| Candidato 090924, saída na thread do jogo | `DD4D483F7C13897C2B96218EB8A2C747C61A0618CC38A3275EF03BBF0892151E` | `8AE377F695BE956318FFE5757718DF42B3847CC75F2597036B3F3911A8C39DFF` |
| Atual, PluginsDev corrigido | `2A3E39D949DBBEEE35ECE867EB3E875E07FED917945FB3FFA88C00BAB644D59C` | `75678B5A2FAC8B6653DEBDBE0D3C0A580FE58759EAF076E3FDEF6CE07E37050C` |
| Smoke test até o menu | `DCD945F7C14A18DBFCF1C326CA4F186D5EF003EFCB0F806BEEC0E251D8BC7D58` | `5E5AD3F962ECD1017776181422285D9C0B1403D775E2FC37CF8D7D7B962C6509` |
| Controle legado | `A896A207D678A8E006DC9BEFBCDDDDA14A57060C1B34AEFB5B2D32519D2BEE2C` | `CD65E843D69224B3C7AEE21EE79C2E0ADE8DBD65F03D8A785AB9F473A4604B4A` |

O candidato 090924 corresponde ao snapshot `source/runtime` atual. O par histórico 034454 antecede a alteração Win32Api.cpp para saída na thread do jogo. O par smoke também antecede a correção posterior de configuração e seus defaults Settings.cpp/Settings.h; a configuração original está no manifest smoke e a reconstrução está em BUILD.md. O controle foi produzido na base antes de alterações de lógica, com a opção /WX; não é o client oficial nem uma recuperação aprovada com Meridian.

PDBs separados correspondem a cada par e contêm os caminhos de compilação embutidos pelo compilador. Arquivos de símbolos e DLLs não foram pós-processados: alterar bytes destruiria a correspondência dos hashes/símbolos. Relatórios textuais têm caminhos locais neutralizados.

## Dependência Node externa

Atualização de distribuição: os releases autorizados em 01/10/2026 incluem esta
dependência inalterada dentro do ZIP do mod e do client. Ela não é versionada
solta no Git. A licença oficial está em `licenses/NODE-LICENSE-v23.2.0.txt`.

`libnode.dll` não modificado, versão observada 23.2.0, esperado ao lado da implementação:

```text
F22C87FA55E3C0E8B5626FD94C801BA7F1BF581E6FA555CE702C44E553D2D210
```

A cópia usada na compilação e no smoke coincide com a baseline oficial. A aquisição original está em `source/runtime/overlay_ports/node-embedder-api-prebuilt/portfile.cmake`, incluindo URL e SHA-512 do arquivo original. Não substituir por outra distribuição Node com o mesmo número de versão: ela pode não exportar a API de embedding usada pelo fork.

Na preparação do overlay MO2, extrair essa DLL do client baseline autorizado ou da aquisição original da toolchain, validar o SHA-256 e copiar ao lado de SkyrimPlatformImpl. Não copiar libcef/chrome_elf do client legado. Meridian e CEF 152 continuam pertencendo ao mod Meridian instalado separadamente.
