# CompatibilidadeMeridianPlatform

Protótipo experimental para executar **Skyrim Platform e Meridian UI no mesmo processo**, usando o Meridian como navegador do client Aetherius e removendo a dependência do CEF legado do Skyrim Platform integrado.

**Estado em 01/10/2026: implementação compilada; smoke test de carregamento positivo; homologação funcional incompleta e testes pausados a pedido do usuário. NÃO é uma versão pronta para produção.**

O Skyrim abriu até o menu sem aviso de DLL na versão `smoke-tested-20261001-033633`. Os logs SKSE registraram Meridian e SkyrimPlatform carregados corretamente. O snapshot confirmou CEF 152, as DLLs modificadas pelos hashes esperados e ausência de `SkyrimPlatformCEF`. Porém, houve erro JavaScript por uma pasta `PluginsDev` inexistente. A correção foi compilada na versão `current-20261001-034454`, mas **não foi instalada nem testada no jogo**.

## Conteúdo

| Pasta | Conteúdo |
|---|---|
| `source/runtime` | Snapshot completo dos arquivos versionados do runtime, mais alterações e testes novos |
| `source/client` | Snapshot completo do client, mais bootstrap, widgets, configuração e testes novos |
| `binaries/current-20261001-034454` | Duas DLLs mais recentes, com a correção PluginsDev; não testadas no jogo |
| `binaries/smoke-tested-20261001-033633` | Duas DLLs efetivamente observadas no smoke test; contêm o problema de configuração documentado |
| `binaries/control-legacy` | Build de controle anterior à integração; ainda depende do CEF legado; apenas referência |
| `overlay/current` | DLLs, INI, HTML, bootstrap, widgets e bundle do client nos caminhos relativos a Data |
| `symbols` | PDBs de cada par de DLLs em ZIPs separados para diagnóstico |
| `scripts` | Reprodução do build e preparação de overlay, sem instalação automática no jogo |
| `scripts/historical` | Scripts da execução local com caminhos neutralizados; arquivo histórico, não executar diretamente |
| `reports` | Hashes, inspeções PE, resultados anteriores e snapshot de módulos sanitizado |
| `docs` | Arquitetura, estágio atual, build, teste, proveniência e plano histórico |
| `licenses` | Avisos de dependências preservados do ambiente de build |

Os fontes são snapshots de trabalho, não forks com o histórico Git original. As bases e alterações estão em [PROVENIENCIA.md](docs/PROVENIENCIA.md). Nenhuma mudança de servidor ou launcher foi implementada por este protótipo.

## Ler antes de usar

1. [Estado atual e falhas conhecidas](docs/ESTADO-ATUAL.md).
2. [Arquitetura e contratos preservados](docs/ARQUITETURA.md).
3. [Como reproduzir o build](docs/BUILD.md).
4. [Preparação segura de teste e matriz pendente](docs/TESTE-E-HOMOLOGACAO.md).
5. [Proveniência e versões de DLLs](docs/PROVENIENCIA.md).
6. [Licenças e componentes externos](THIRD_PARTY_NOTICES.md).

Alvo observado: Windows x64, Skyrim SE Steam **1.6.1170**, SKSE **2.2.6**, Meridian UI **1.5.0**, API pública **Meridian.View/1** e CEF **152.0.6**. Não há homologação de outros runtimes, VR ou outra versão do Meridian.

## Limites importantes

- Este repositório não contém Skyrim, SKSE, Meridian runtime/CEF, MO2, mods, credenciais, tokens ou saves pessoais.
- `libnode.dll` não foi alterado e não é redistribuído aqui: o overlay necessita da cópia exata indicada na documentação, junto de `SkyrimPlatformImpl.dll` quando usado com MO2.
- O ZIP local original com Node e os backups completos de client não foram publicados. Há hashes e instruções para obter a dependência original.
- A distribuição oficial `client-v<versão>.zip` e os releases anteriores do Aetherius **não foram alterados**.
- O overlay não é um pacote para o launcher; não renomeá-lo como release oficial de client.
- Não substituir CEF 108 por CEF 152 em uma DLL compilada para a ABI antiga. Não misturar os pares de DLLs das três pastas.
- Compilação e testes mockados não demonstram login, multiplayer, voz, foco, HUD, coexistência ou estabilidade em jogo.
- Os testes no jogo permanecem pausados. A publicação deste snapshot não constitui retomada da homologação.

Licença do projeto-base: GPL v3, preservada em [LICENSE](LICENSE). O SDK público Meridian conserva seu aviso MIT, e demais componentes conservam seus próprios avisos.
