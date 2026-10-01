# Preparação e homologação — testes pausados

Este guia registra como retomar quando o usuário autorizar. **Não foram iniciados novos testes no jogo durante a preparação desta publicação.**

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

## Produção e recuperação

Os perfis oficiais e Data não foram alterados durante os testes. Para deixar de usar o candidato local, fechar o jogo e sair dos perfis TESTE. Não apagar DLLs/mods originais; o candidato e suas evidências devem ser preservados para diagnóstico.

Voltar à combinação oficial antiga não garante abertura com Meridian: ela contém o conflito CEF original. Recuperação funcional precisa ser homologada como conjunto.

O launcher Aetherius consultado seleciona releases publicados, incluindo pré-releases, e espera o padrão `client-v<versão>.zip` com raiz `client/` e nomes oficiais de profiles. Um release público experimental no repositório de client não é um sandbox. Este repositório de compatibilidade não altera a seleção do launcher, e nenhum release oficial foi produzido.
