# CompatibilidadeMeridianPlatform

Patch para executar **Skyrim Platform e Meridian UI no mesmo processo**, usando o Meridian como navegador do client Aetherius e removendo a dependência do CEF legado do Skyrim Platform integrado.

**Estado em 01/10/2026: testes encerrados e publicação aprovada pelo usuário após sucesso de abertura no perfil oficial de Qualidade. 26 testes Node e 2 CTest aprovados. O release documenta esse escopo; não afirma homologação de login, voz, multiplayer ou estabilidade prolongada.**

O smoke inicial confirmou carregamento conjunto e CEF 152, mas encontrou erro de `PluginsDev`. A contribuição recebida relata correção desse erro e fechamento normal no par `current-20261001-034454`, sem inspeção dos módulos. A revisão independente confirmou testes e bundle recebido, acrescentou proteções de fechamento, foco/PTT e saída na thread do jogo. O novo `candidate-20261001-090924` completou dois ciclos no menu: hashes das DLLs/Node conferidos em execução, um CEF 152 e nenhum subprocesso legado nos snapshots, sem erro DirectoryMonitor/JS. Em ambos, o botão Fechar acionou a saída na thread do jogo e o CEF encerrou com sucesso. **UI completa, perfis completos e homologação continuam pendentes.** Consulte [a revisão completa](docs/REVISAO-CONTINUACAO.md).

## Conteúdo

Dois ciclos de abrir até o menu e fechar pela interface foram confirmados no
perfil mínimo, com hashes dos módulos e shutdown CEF bem-sucedido em ambas as
sessões. Por escolha do usuário, também foi conferida somente a abertura no
perfil oficial de Qualidade com o overlay ativado por ele. Nenhum novo teste no
jogo será solicitado nesta etapa. A baseline v0.8.1 foi reconferida no GitHub;
o pacote v0.8.2 preserva sua estrutura `client/`, acrescenta o mod de
compatibilidade e uma única linha nos dois perfis oficiais. A antiga prévia
experimental não é o pacote do release. Ver [publicação](docs/PUBLICACAO-20261001.md)
e [revisão](docs/REVISAO-CONTINUACAO.md).

| Pasta | Conteúdo |
|---|---|
| `source/runtime` | Snapshot completo dos arquivos versionados do runtime, mais alterações e testes novos |
| `source/client` | Snapshot completo do client, mais bootstrap, widgets, configuração e testes novos |
| `binaries/candidate-20261001-090924` | Novo par, hashes e dois ciclos mínimos confirmados; demais funções pendentes |
| `binaries/current-20261001-034454` | Par histórico com PluginsDev corrigido; teste de menu relatado na contribuição |
| `binaries/smoke-tested-20261001-033633` | Duas DLLs efetivamente observadas no smoke test; contêm o problema de configuração documentado |
| `binaries/control-legacy` | Build de controle anterior à integração; ainda depende do CEF legado; apenas referência |
| `overlay/current` | DLLs, INI, HTML, bootstrap, widgets e bundle do client nos caminhos relativos a Data |
| `symbols` | PDBs de cada par de DLLs em ZIPs separados para diagnóstico |
| `scripts` | Build/testes, staging, instalação opt-in em perfis TESTE e coleta de evidências |
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
6. [Revisão da contribuição e candidato mais recente](docs/REVISAO-CONTINUACAO.md).
7. [Licenças e componentes externos](THIRD_PARTY_NOTICES.md).

Alvo observado: Windows x64, Skyrim SE Steam **1.6.1170**, SKSE **2.2.6**, Meridian UI **1.5.0**, API pública **Meridian.View/1** e CEF **152.0.6**. Não há homologação de outros runtimes, VR ou outra versão do Meridian.

## Limites importantes

- Este repositório não contém Skyrim, SKSE, Meridian runtime/CEF, MO2, mods, credenciais, tokens ou saves pessoais.
- `libnode.dll` não foi alterado. Não é versionado solto no Git; os novos ZIPs de release incluem a cópia exata da baseline e sua licença, junto de `SkyrimPlatformImpl.dll` no mod MO2.
- O ZIP original de teste e os backups pessoais não são publicados; os novos pacotes são derivados da baseline oficial e do candidato com hashes verificados.
- O novo client usa `client-v0.8.2.zip`; os releases e assets anteriores são preservados.
- `overlay/current` isolado não é pacote do launcher. Use o client completo ou o ZIP do mod descritos na documentação de publicação.
- Não substituir CEF 108 por CEF 152 em uma DLL compilada para a ABI antiga. Não misturar pares de DLLs entre pastas.
- Compilação e testes mockados não demonstram login, multiplayer, voz, foco, HUD, coexistência ou estabilidade em jogo.
- Abertura no perfil oficial de Qualidade autorizada e aprovada pelo usuário; demais testes encerrados por solicitação dele. Consulte os relatórios e os limites antes de distribuir para outros runtimes.

Licença do projeto-base: GPL v3, preservada em [LICENSE](LICENSE). O SDK público Meridian conserva seu aviso MIT, e demais componentes conservam seus próprios avisos.
