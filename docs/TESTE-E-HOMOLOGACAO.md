# Preparação e homologação — retomada isolada

Os testes no jogo foram encerrados a pedido do usuário, que aprovou o sucesso
de abertura e solicitou a publicação. A matriz abaixo documenta limites reais,
não uma solicitação de novos testes nesta etapa. Para distribuição, consultar
[PUBLICACAO-20261001.md](PUBLICACAO-20261001.md).

O usuário autorizou a retomada. O candidato `20261001-090924` foi instalado somente
em perfis TESTE novos, sem trocar o perfil ativo durante a instalação. O usuário
selecionou o novo mínimo e abriu SKSE: carregamento e hashes dos módulos foram
conferidos no menu. Dois ciclos de abrir/fechar pela interface foram confirmados:
solicitação na thread do jogo, shutdown CEF bem-sucedido e processo encerrado.
Reabertura confirmada com logs novos e hashes dos módulos.
Perfis completos, foco/input e demais funções continuam pendentes; ver
[REVISAO-CONTINUACAO.md](REVISAO-CONTINUACAO.md).

## Restrição atual definida pelo usuário

O usuário escolheu o perfil oficial `AETHERIUS - GRAFICO - QUALIDADE`, ativou
nele o overlay TESTE e pediu **somente abertura**, para aproveitar a configuração
de shaders existente. O carregamento, hashes, CEF e logs dessa abertura foram
conferidos. Não solicitar novos testes no jogo nesta etapa nem alterar caches,
perfil, saves ou o modo de inicialização. O próximo trabalho é offline, no pacote
experimental local. A matriz abaixo permanece uma lista de homologação pendente,
não uma aprovação nem uma instrução para executar agora.

Os perfis oficiais e candidatos históricos continuam disponíveis. Nenhuma tela
de produção deve ser usada para provocar ban/revogação/queda durante a homologação.

## Retomada local

1. Fechar Skyrim. Não substituir DLL em uso.
2. Obter as dependências autorizadas: jogo/SKSE do alvo, Meridian 1.5.0, Address Library e client baseline. Manter o par de DLLs integrado como conjunto.
3. Preparar overlay atual com a DLL Node exata, usando `scripts/prepare-overlay.ps1`. A pasta final deve conter SKSE, Platform e MeridianUI diretamente, sem wrapper Data.
4. Criar um mod novo de teste no MO2. Duplicar um perfil antes de habilitar o mod; usar saves locais vazios. Não usar saves de produção.
5. Dar maior prioridade ao overlay apenas nos perfis TESTE. Overwrite também precisa ser inspecionado: ele pode vencer mods. Conferir INI efetivo meridian e PluginFolders de produção.
6. Primeiro habilitar somente Meridian, Address Library e overlay, mais o Data básico de client existente. Executar SKSE sem carregar save antigo.
7. Observar ausência do erro DirectoryMonitor e coletar logs SKSE/Meridian/Platform sanitizados e módulos efetivamente carregados. Conferir hashes do par atual, não do smoke anterior.
8. Só então avançar para os perfis completos e servidor de teste. Não contornar autenticação para simular aprovação.

A execução direta SKSE não recebe AETHERIUS_TOKEN do launcher. A tela que exige launcher é esperada; não comprova login aprovado. Não compartilhar tokens em logs/issues.

## Matriz restante

| Cenário | Evidência necessária |
|---|---|
| Atual abre sem DirectoryMonitor | Client JS inicia, view e bridge reais prontas |
| CEF legado ausente | Módulos/processos em execução; não somente imports estáticos |
| Interface/readiness | Uma view, mensagens ida/volta, front-loaded sem duplicação |
| Login permitido/negado | Servidor de teste, UI e motivo corretos |
| Entrada/spawn/reentrada | Sessão e HUD sem callbacks/views antigos |
| Chat/comandos | Esclarecer scaffold comentado da baseline; não marcar recurso inexistente como aprovado |
| Voz | Dois participantes, envio/recepção, PTT, espacialização/modos, áudio sob mod:// |
| Pop-ups | Acesso encerrado, queda/reconexão, fechamento/controlado sem alterar conta de produção |
| HUD/nomes/F1 | Fontes, posição, cor, atualização e performance |
| Menus/input | Inventário, mapa, console, F2/F6, Escape, Enter, Alt-Tab, cursor e WASD |
| Coexistência | Outros consumers Meridian e frameworks habilitados, incluindo PrismaUI |
| Falhas/teardown | Provider/API/página ausentes, callbacks antigos, encerramento/reabertura |
| Qualidade/Performance | Três aberturas completas e 30 minutos por perfil, com a matriz funcional |
| Instalação | Instalação limpa e atualização com arquivos CEF antigos remanescentes sem carregá-los |
| Recuperação | Runtime, recursos e perfis como conjunto compatível |
| Launcher | Pacote/lock/perfil/hashes realmente instalados pelo fluxo de atualização |

O mínimo anterior conservava MpClientPlugin no Data. Não é teste de instalação limpa. Não há homologação de performance ou estabilidade com base na única abertura.

## Scripts de retomada

`scripts/test-meridian.ps1` executa Node e CTest via MSVC, sem instalação ou jogo.
`scripts/stage-candidate.ps1` verifica correspondência dos fontes do build,
exports/imports e prepara um novo par/ZIP local, preservando históricos.
`scripts/install-test-profiles.ps1` sem `-Install` apenas apresenta o plano;
um candidato novo exige `-CandidateManifest`, com hashes dos oito arquivos.
Com `-Install`, jogo e MO2 devem estar fechados; só cria destinos novos e verifica
hashes dos perfis oficiais e ModOrganizer.ini. Não copia saves ou altera Data.
`scripts/collect-test-evidence.ps1 -Label <rotulo>` coleta trechos permitidos,
timestamps de logs e hashes de módulos se disponíveis. Correlacionar datas e
observações do usuário; logs antigos não são evidência de um novo teste.

## Produção e recuperação

O agente não alterou o Data nem os perfis oficiais. O usuário posteriormente
ativou o overlay TESTE no perfil oficial de Qualidade. Para deixar de usá-lo,
fechar o jogo/MO2 antes de trocar arquivos e desabilitar esse mod no perfil em
que foi ativado; sair de um perfil TESTE sozinho já não basta. Não apagar DLLs,
mods originais ou caches; preservar candidato e evidências para diagnóstico.

Voltar à combinação oficial antiga não garante abertura com Meridian: ela contém o conflito CEF original. Recuperação funcional precisa ser homologada como conjunto.

O launcher Aetherius consultado seleciona releases publicados, incluindo pré-releases, e espera o padrão `client-v<versão>.zip` com raiz `client/` e nomes oficiais de profiles. Um release público experimental no repositório de client não é um sandbox. Este repositório de compatibilidade não altera a seleção do launcher, e nenhum release oficial foi produzido.
