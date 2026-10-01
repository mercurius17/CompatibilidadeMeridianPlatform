# Revisão da contribuição e novo candidato — 01/10/2026

Atualização posterior: o usuário encerrou os testes, considerou a abertura
bem-sucedida e autorizou releases. As restrições de não publicação abaixo
registram o estágio anterior, não a autorização atual. Ver
[PUBLICACAO-20261001.md](PUBLICACAO-20261001.md) para distribuição e limites.

## O que foi recebido e confirmado

Foi revisada a pasta `continuacao`, baseada no commit `9826b86`. Ela foi
preservada sem alterações. Não foram copiados sobre o projeto seu README,
licenças ou caminhos de instalação. O relato original está arquivado em
[RETOMADA-20261001.md](RETOMADA-20261001.md), com atribuição explícita.

A mudança recebida faz ServerDownService e AccessRevokedService processarem
saída no `tick`, sem depender de `update`/Papyrus, que pode não ocorrer no menu.
A reconexão também usa tick. O diagnóstico usa a API global do runtime, não
`require('skyrimPlatform')`.

Verificações independentes nesta revisão:

- 20 testes Node da contribuição passaram antes das novas alterações.
- O bundle foi recompilado com dependências congeladas e deploy desativado.
  Seu SHA-256 coincidiu exatamente com o recebido:
  `999D74232515982DA0A0C83F9D17F47EA619961D7E86A9E80475659D052D66D3`.
- Dois testes C++ isolados passaram com CMake/MSVC, sem precisar instalar Zig.

A evidência recebida em
[`continuacao-minimal-close-fixed.json`](../reports/continuacao-minimal-close-fixed.json)
registra carregamento das DLLs, ausência de erro DirectoryMonitor/JS, view/DOM
e encerramento CEF. A coleta foi posterior ao fechamento e **não conseguiu
inspecionar módulos**. Não comprova hashes carregados nem ausência de CEF legado
durante toda aquela sessão. O relato informa clique no botão e saída normal;
essa execução não foi observada novamente nesta revisão.

## Implementação adicional

1. Fechamento terminal: cancela reconexão pendente e ignora novos eventos de
   conexão/fechamento nesse serviço. Uma conexão aceita cancela pedido de
   reconexão ainda não processado. Confirmações repetidas de acesso revogado não
   solicitam saídas repetidas.
2. Foco real é sincronizado no tick. O teclado de gameplay permanece no update;
   APIs Papyrus de input não foram transferidas para tick.
3. PTT é liberado ao adquirir foco mesmo sem novo evento de quantidade de teclas.
   Perder foco sozinho não reabre o microfone. O próximo evento de teclado fora
   da UI conserva o comportamento original de leitura de V.
4. Atalhos da view Meridian focada não são processados novamente pelo teclado
   do jogo. A propriedade de uma tecla recebida pelo DOM permanece até uma
   consulta observar sua liberação, inclusive quando F6 já retirou o foco.
5. `win32.exitProcess()` encaminha uma tarefa única à thread do jogo pelo SKSE
   antes de escrever `RE::Main::quitGame`. Não chama TerminateProcess, não
   encerra o CEF global e não usa Papyrus. A interface SKSE ausente produz erro
   explícito. Mantidos CommonLib, pins, flags e exports da ABI original.

As regressões reproduziram os problemas de reconexão/saída repetida antes das
correções. Após integrar as alterações: **26/26 testes Node**, **2/2 CTest**,
webpack com checagem TypeScript e ambos os targets nativos Release passaram.
O build conserva o target TypeScript original; não foi necessário alterá-lo.

Scripts integrados usam caminhos parametrizados, validação de manifest e
instalação opt-in. A coleta registra timestamps dos logs e explicita a limitação
de snapshots posteriores à saída. O diagnóstico permanece opcional/local e não
foi instalado como parte deste overlay.

## Candidato `20261001-090924`

- DLLs novas: `binaries/candidate-20261001-090924`.
- Símbolos correspondentes: `symbols/candidate-20261001-090924-pdb.zip`.
- `overlay/current` contém o novo par e bundle; históricos permanecem intactos.
- Bundle: `15DB5ADD2D7FE3B3BB73FD9CDC7047F492CC9B4F5BFF3DDFF16BFF32B194B065`.
- Manifest/PE/hashes: [relatório](../reports/candidate-20261001-090924.json).
- Fontes nativas comparadas byte a byte com o checkout usado para o build:
  [hashes dos fontes](../reports/candidate-20261001-090924-source-hashes.json).
- ZIP local Data-relativo com exatamente oito arquivos, libnode baseline,
  sem CEF legado. Foram verificados conteúdo/hash de cada entrada, caminhos,
  exports, imports das três DLLs privadas e dependências de sistema.
  Isso não demonstra comportamento em jogo nem compatibilidade de APIs dinâmicas.

Instalado somente em um mod novo e três perfis novos do MO2 `C:\modOrganizer`:

- `TESTE MERIDIAN - MINIMO - 20261001-090924`;
- `TESTE MERIDIAN - QUALIDADE - 20261001-090924`;
- `TESTE MERIDIAN - PERFORMANCE - 20261001-090924`.

Os 14 arquivos dos perfis oficiais de origem e ModOrganizer.ini conservaram os
hashes anteriores. O perfil ativo não foi trocado; nenhum save foi copiado e
nenhum arquivo do Data do jogo foi modificado. O jogo dessa instância aponta
para `C:\Games`, não para o caminho E: do relato recebido.

## Teste de carregamento observado — 09:17 UTC (06:17 local)

O usuário abriu o novo perfil mínimo e informou estar no menu principal.
A coleta desta sessão confirmou o perfil ativo `20261001-090924`, as duas DLLs
SKSE como `loaded correctly`, aquisição Meridian.View/1, view criada e DOM pronto.
Não foram observados erro DirectoryMonitor ou exceção JavaScript de inicialização.
A linha `JavaScript error:` com conteúdo vazio é emitida incondicionalmente pelo
construtor JsEngine nesta fonte; não é tratada como exceção.

Com o jogo em execução, a inspeção de módulos estava disponível: os hashes de
SkyrimPlatform.dll, SkyrimPlatformImpl.dll e libnode.dll coincidiram com os do
candidato. Foi observado um único libcef, versão 152.0.6, pertencente ao Meridian,
e nenhum processo SkyrimPlatformCEF nesse instante. Também estava carregado
MpClientPlugin do Data existente; isto continua não sendo instalação limpa.
O Meridian registrou fallback SyncCopy, sem medição de performance.

houseCARL confirmou que DLLs, INI, bundle e HTML do mod TESTE vencem os arquivos
do Data no perfil ativo. Os oito arquivos instalados e os 14 arquivos dos perfis
oficiais conservaram seus hashes. ModOrganizer.ini mudou pela seleção do perfil
de teste feita pelo usuário, como esperado; o agente não o modificou.

Evidência: [snapshot em execução](../reports/candidate-090924-menu-20261001-091713.json),
capturado em 09:17:13 UTC, processo iniciado em 09:16:11 UTC e logs atualizados
depois desse início. É carregamento confirmado em um instante; não prova
funcionalidade completa da UI, áudio, foco, teardown ou estabilidade da sessão.

## Fechamento confirmado — 09:19 UTC (06:19 local)

O usuário confirmou que clicou em Fechar na interface e o jogo encerrou.
Os logs desta mesma abertura registram:

- 06:19:55.604: `Normal engine shutdown requested on the game thread`;
- 06:19:55.604: Meridian rejeita novos trabalhos e solicita shutdown do CEF;
- 06:19:55.695: `CEFService::CEFShutdown successfully`;
- 06:19:56.380: destruição de JsEngine.

A coleta às 09:20:37 UTC encontrou zero processos SkyrimSE. Os timestamps e
o perfil correspondem à abertura cujos módulos foram conferidos em execução.
O snapshot após fechar não contém módulos, como esperado; a comprovação dos
hashes continua sendo o snapshot anterior.

Evidência: [coleta após saída](../reports/candidate-090924-shutdown-20261001-092037.json).
Este ciclo confirma abertura até o menu, resposta ao botão e saída normal pelo
novo caminho na thread do jogo, com shutdown CEF bem-sucedido. Não valida outros
botões, login, voz, gameplay ou estabilidade prolongada.

## Reabertura confirmada — 09:23 UTC (06:23 local)

O usuário voltou ao menu no mesmo perfil mínimo. O novo processo iniciou em
09:22:41 UTC, depois da coleta de encerramento anterior. Os três logs foram
atualizados após esse início: não são os logs da abertura anterior.

A coleta às 09:23:50 UTC confirmou novamente carregamento das duas DLLs SKSE,
view criada/DOM pronto, ausência de erro DirectoryMonitor/JS e os hashes do
par nativo e libnode. Um único CEF 152 foi observado e nenhum subprocesso
SkyrimPlatformCEF estava presente naquele instante. houseCARL confirmou o
perfil mínimo `20261001-090924` ativo.

Evidência: [segunda abertura em execução](../reports/candidate-090924-reopen-20261001-092349.json).
Naquela coleta: duas aberturas observadas e um fechamento confirmado.
O fechamento da segunda sessão foi conferido posteriormente, conforme abaixo.

## Segundo fechamento confirmado — 09:25 UTC (06:25 local)

O usuário confirmou que o jogo encerrou novamente. Os logs da segunda abertura
registraram `Normal engine shutdown requested on the game thread` às
06:25:10.252 e `CEFService::CEFShutdown successfully` às 06:25:10.357.
A coleta às 09:26:25 UTC encontrou zero processos SkyrimSE. Os logs de shutdown
foram atualizados depois do snapshot da segunda sessão em execução, no mesmo
perfil e com as âncoras de inicialização dessa reabertura.

Evidência: [segundo encerramento](../reports/candidate-090924-shutdown2-20261001-092625.json).
Resultado limitado: **dois ciclos de abrir até o menu e fechar pela interface**
confirmados no perfil mínimo. Os hashes foram conferidos enquanto o jogo estava
aberto em ambas as sessões. Não é aprovação de gameplay, foco/atalhos, voz,
reconexão autenticada, perfis completos ou estabilidade prolongada.

## Qualidade oficial — teste restrito de abertura, 09:39 UTC

O usuário mudou expressamente a abordagem: ativou o mod
`Aetherius - Meridian - TESTE - 20261001-090924` no perfil oficial
`AETHERIUS - GRAFICO - QUALIDADE`, para aproveitar a configuração/cache já usados,
e limitou este teste a abrir o jogo. Essa alteração de perfil foi feita pelo
usuário, não pelo agente. Nenhum cache de shader foi limpo ou modificado pelo
agente, e não foram solicitados novos testes de gameplay ou carregamento de saves.

houseCARL confirmou esse perfil ativo, 552 mods habilitados e 423 plugins ativos.
O mod TESTE vence o Data para as DLLs, Node e bundle, e vence o INI de
`AETHERIUS - OVERWRITE`. O processo observado iniciou em 09:36:51 UTC; os três
logs pertencem a essa abertura, não aos testes mínimos anteriores.

Na coleta às 09:39:33 UTC, as duas DLLs SKSE estavam carregadas corretamente,
view criada e DOM pronto, sem erro DirectoryMonitor/JS de inicialização. Os
hashes do par nativo e libnode coincidiram com o candidato; um único libcef 152
e nenhum processo SkyrimPlatformCEF foram observados nesse instante.

Evidência: [abertura no perfil oficial](../reports/candidate-090924-quality-official-startup-20261001-093932.json).
Resultado: carregamento conjunto observado com a modlist de Qualidade. Não
afirma gameplay, interface completa, performance, shaders, voz ou homologação
desse perfil. Não foi solicitado um teste de encerramento adicional.

## Próximo passo local — prévia do pacote

Respeitando a restrição de testes, foi preparada uma prévia **somente local** do
client com `scripts/prepare-client-preview.ps1`, a partir do ZIP congelado v0.8.1
identificado por SHA-256 e do overlay de oito arquivos. Mantida a raiz `client/`,
com Data e modOrganizer nos caminhos da baseline. Cada entrada resultante teve
tamanho e SHA-256 verificados após reabrir o ZIP; mudanças fora da allowlist do
overlay são recusadas. Nada foi instalado no jogo/MO2.

Artefato em `packages/client-preview-20261001-094535/client-meridian-EXPERIMENTAL.zip`
(ignorado pelo Git), com [manifest resumido](../reports/client-preview-20261001-094535.json).
O nome deliberadamente não é `client-v<versão>.zip`: não usar com o launcher nem
publicar. Os profiles permanecem byte-idênticos à **baseline histórica**, não
foram obtidos do MO2 ativo e não incluem a ativação local do mod TESTE. Eles
precisam ser comparados à distribuição oficial atual antes de preparar um
pacote instalável, para não reinstalar perfis antigos.

Os recursos CEF legados da baseline foram preservados nesta prévia; sua remoção
aguarda revisão de consumidores. O binário Meridian observado não os carregou,
mas isso não comprova ausência de outros consumidores no pacote. Ainda faltam
revisão da baseline atual, perfis, limpeza de dependências, licenças da distribuição
e teste de instalação. Nenhuma tag de produção foi escolhida e nenhum upload,
draft ou release foi realizado. Esta prévia não está pronta para instalação.

Login/reconexão autenticada, multiplayer, PTT/voz com dois participantes, HUD,
coexistência e estabilidade dos perfis completos continuam pendentes. Não se
deve provocar ban/queda nem alterar autenticação de produção para testar. O
launcher e os releases oficiais permanecem intactos. Nenhum commit/push ou
release foi realizado nesta revisão.
