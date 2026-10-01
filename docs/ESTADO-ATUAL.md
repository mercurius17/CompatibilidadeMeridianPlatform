# Estágio atual — 01/10/2026

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

## Falha encontrada e correção não homologada

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

As DLLs dessa correção estão em `binaries/current-20261001-034454`. **Ainda não foram instaladas no mod de teste nem abertas no jogo.** O snapshot de smoke refere-se exclusivamente à versão anterior.

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

## O que não está aprovado

| Item | Estado |
|---|---|
| Carregamento conjunto na versão smoke | Observado no perfil mínimo, até o menu |
| Ausência CEF legado na versão smoke | Observada no snapshot de módulos/processos |
| Versão atual com correção PluginsDev | Compilada; teste real pendente |
| UI completa e mensagens bidirecionais reais | Pendente |
| Login permitido/negado e launcher | Pendente |
| Spawn, multiplayer, HUD/nomes e menus | Pendente |
| Voz envio/recepção, PTT e spatialização | Pendente |
| Foco/input, outros consumidores Meridian e PrismaUI | Pendente |
| Shutdown/reabertura e sessão prolongada | Pendente |
| Perfis completos Qualidade/Performance | Pendente |
| Instalação limpa, atualização e recuperação | Pendente |
| Pacote/release oficial | Não preparado nem publicado |

O HTML baseline tem o scaffold de chat textual comentado. Este trabalho não habilitou nem inventou um protocolo de chat; essa limitação precisa ser esclarecida antes de aprovar o requisito original de chat. Voz mantém o WebSocket original; origem mod://, permissões e efeitos sobre áudio ainda dependem de teste real.

## Ponto de retomada

O usuário pediu para interromper os testes e continuar posteriormente. Não foi aplicado o novo candidato após essa pausa. Quando houver autorização para retomada: instalar somente no mod de teste, conferir hashes, repetir primeiro a abertura mínima e observar ausência do erro DirectoryMonitor; só depois avançar para os dois perfis completos e servidor de teste.
