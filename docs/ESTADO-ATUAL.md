# Estágio atual — 01/10/2026

## Encerramento dos testes e publicação autorizada

O usuário considerou bem-sucedido o teste de abertura no perfil oficial de
Qualidade, encerrou os testes e solicitou a publicação. Essa aprovação substitui
a antiga proibição de publicar o candidato, mas não amplia o escopo das
evidências. A distribuição usa exatamente o par 090924 e o bundle conferidos;
não há novo teste ou recompilação. Ver [PUBLICACAO-20261001.md](PUBLICACAO-20261001.md)
e os relatórios de empacotamento/publicação. As seções seguintes preservam o
histórico e as funções ainda não homologadas.

## Atualização após revisão da contribuição

Implementação retomada a pedido do usuário. O candidato mais recente é
`20261001-090924`: compilado, 26/26 Node e 2/2 CTest aprovados, oito arquivos
do ZIP verificados e instalado em três perfis TESTE novos. **Carregamento até o
menu, hashes dos módulos e dois fechamentos pelo botão da interface confirmados
em duas sessões; também observado carregamento no perfil oficial de Qualidade,
por escolha do usuário. UI completa e homologação dos perfis ainda pendentes.** O shutdown foi
registrado na thread do jogo e o CEF encerrou com sucesso;
há proteções adicionais para fechamento/reconexão, foco e PTT. Consulte
[REVISAO-CONTINUACAO.md](REVISAO-CONTINUACAO.md) para hashes, escopo e evidências.

A contribuição relata teste positivo de menu/fechamento com o par histórico
034454, sem inspeção dos módulos. Não é a execução de smoke com hashes abaixo.
O restante desta página descreve o histórico inicial, com as correções de estado
assinaladas. Releases antigos e código do launcher são preservados. O usuário ativou o
mod TESTE no perfil oficial de Qualidade; o agente não alterou essa configuração.

## Resumo de execução

A implementação foi feita em checkouts isolados do runtime e do client. Os checkouts originais com alterações locais foram preservados. Nenhum commit/push, release ou alteração de launcher/client de produção foi realizado naquela etapa.

Primeiro foi reproduzido o build de controle legado. Depois foram compilados os dois targets nativos integrados em Release e o bundle webpack do client, com deploy automático desativado.

Foram executados testes determinísticos: inicialmente 14 testes Node e 2 testes CTest, todos aprovados. Depois do erro real de PluginsDev foi acrescentado um teste da configuração distribuída: a execução seguinte teve **15 testes Node aprovados**. O log Node preservado em `reports/client-tests.log` registra a execução anterior de 14; o resultado de 15 foi observado no terminal local, sem arquivo bruto adicional. Não confundir esses testes com homologação CEF/gameplay.

## Teste real já realizado

Instância MO2 escolhida pelo usuário: a instalação do launcher. Foram criados um mod de teste e três perfis novos, separados dos oficiais. O modlist de teste põe o overlay na maior prioridade de mods. LocalSaves=true, sem cópia de saves oficiais.

O perfil mínimo habilitava Meridian, Address Library e o overlay, com onze masters/CC. O Data original permaneceu e ainda continha MpClientPlugin; portanto, não era uma instalação limpa.

Na abertura até o menu:

- O usuário relatou ausência de aviso de DLL e confirmou o jogo aberto no menu.
- SKSE registrou `MeridianUIPlugin.dll` e `SkyrimPlatform.dll` como `loaded correctly`.
- O snapshot do processo confirmou as DLLs do candidato testado, `libnode.dll` esperado e Meridian 1.5.0.
- Somente um módulo libcef foi observado: CEF **152.0.6** do Meridian. Não foi observado processo `SkyrimPlatformCEF`.
- Log do runtime: `Meridian.View/1 acquired at InputLoaded`, dois SpriteFonts carregados, view 1 criada com geração 1 e DOM pronto.
- O Meridian informou fallback de transporte gráfico para SyncCopy no adaptador disponível. Isso não foi tratado como falha de carregamento, mas seu efeito visual/performance não foi medido.

Evidência sanitizada: [snapshot](../reports/smoke-runtime-snapshot-20261001-034334.json). É **um snapshot pontual**, não vigilância durante toda a sessão. Presença de módulo não substitui os logs de inicialização nem prova funcionalidade da interface.

## Falha inicial de configuração

O runtime registrou:

```text
Error: DirectoryMonitor(Data/Platform/PluginsDev) failed with code 2
```

Essa pasta opcional de desenvolvimento não existia na instalação utilizada. O INI do primeiro overlay não declarava PluginFolders e herdou o default com PluginsDev. O erro compromete o fluxo JavaScript do client; uma view criada/DOM pronto não demonstra que o client tenha inicializado corretamente.

Correção já feita em fonte e compilada:

- INI distribuído passa a declarar `[Main] PluginFolders=Data/Platform/Plugins`.
- Defaults gerados/lidos no build Meridian usam somente a pasta de produção; o default legado separado é preservado.
- GetPathsSemicolonSeparated passa a usar os próprios parâmetros section/key/defaultValue, em vez de constantes internas.
- Acrescentado teste que impede reintroduzir PluginsDev na configuração distribuída.

As DLLs dessa correção estão em `binaries/current-20261001-034454`. A contribuição posterior relata abertura e fechamento desse par no menu, sem confirmar os módulos carregados. O snapshot inicial de smoke com hashes refere-se exclusivamente à versão anterior. O candidato 090924 teve seus módulos conferidos em execução e saída normal pelo botão confirmada nos logs; funcionalidade completa e repetição continuam pendentes.

## O que está implementado

- Build integrado sem linkagem ao browser CEF legado ou seu subprocesso.
- Carregador com busca privada de dependências, sem adicionar a pasta CEF antiga ao PATH global.
- BrowserApiMeridian, View/1, propriedade de view e origem mod:// restrita.
- Encaminhamento seguro de mensagens, handshake de readiness, filas limitadas e geração/cancelamento.
- Foco efetivo não-preemptivo, visibilidade, atalhos e sinalização de recusa de foco.
- Renderer de textos/HUD D3D11/DirectXTK independente e snapshot entre threads.
- Bootstrap web, widgets, preservação dos contratos de eventos e alteração limitada dos serviços client.
- Guarda de push-to-talk durante digitação e remoção de debug logs contendo campos de credenciais.
- Proveniência do SDK, testes automatizados, scripts de inspeção e hashes.

## Validação por escopo

| Item | Estado |
|---|---|
| Carregamento conjunto na versão smoke | Observado no perfil mínimo, até o menu |
| Ausência CEF legado na versão smoke | Observada no snapshot de módulos/processos |
| Par 034454, PluginsDev corrigido | Teste de menu/fechamento relatado na contribuição; hashes de módulos não observados |
| Novo candidato 090924 | Dois ciclos mínimos de abrir/fechar e hashes confirmados; demais funções pendentes |
| Qualidade oficial com overlay TESTE | Abertura, logs e hashes confirmados; apenas carregamento, não homologação |
| UI completa e mensagens bidirecionais reais | Pendente |
| Login permitido/negado e launcher | Pendente |
| Spawn, multiplayer, HUD/nomes e menus | Pendente |
| Voz envio/recepção, PTT e spatialização | Pendente |
| Foco/input, outros consumidores Meridian e PrismaUI | Pendente |
| Shutdown/reabertura e sessão prolongada | Dois ciclos mínimos confirmados; estabilidade e perfis completos pendentes |
| Perfis completos Qualidade/Performance | Pendente |
| Instalação limpa, atualização e recuperação | Pendente |
| Pacote/release oficial | Patch v0.1.0, client v0.8.2 e mod v1.0.0 publicados; hashes, tags e seleção do launcher confirmados no relatório de publicação |
| Prévia local de client | ZIP com estrutura client/ e entradas verificadas; baseline histórica, não instalar/publicar |

O HTML baseline tem o scaffold de chat textual comentado. Este trabalho não habilitou nem inventou um protocolo de chat; essa limitação precisa ser esclarecida antes de aprovar o requisito original de chat. Voz mantém o WebSocket original; origem mod://, permissões e efeitos sobre áudio ainda dependem de teste real.

## Prévia histórica e encerramento

Após os dois ciclos mínimos, o usuário pediu somente teste de abertura usando
o perfil oficial de Qualidade, com o mod TESTE ativado por ele. Esse carregamento
foi conferido e não foram solicitados testes adicionais. Avançou-se para uma
prévia local do pacote: `client-preview-20261001-094535`. Ela conserva profiles
históricos e recursos CEF antigos, portanto ainda não é instalável/aprovada para
o launcher. Próximos passos offline: revisar baseline/perfis atuais, consumidores
das dependências e licenças. Login, voz e multiplayer continuam não homologados;
os testes foram encerrados por solicitação do usuário, que aprovou a abertura
e autorizou a distribuição. A baseline v0.8.1 foi reconferida remotamente e o
pacote final conserva seus arquivos, acrescentando somente o overlay/avisos e
uma linha de prioridade nas duas modlists. Os três releases estão publicados;
o launcher seleciona v0.8.2 e o release anterior permanece intacto. Ver
[registro remoto](../reports/publication-20261001.json). Não foram feitos novos
testes no jogo nem alterações locais na instalação do jogo/MO2 nesta publicação.
