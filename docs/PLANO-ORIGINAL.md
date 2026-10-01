# Plano original — histórico, não relatório de conclusão

Consulte ESTADO-ATUAL.md para o que foi efetivamente executado e para a pausa solicitada.

# Plano de implementação e execução — Skyrim Platform + Meridian UI

Data de referência: 30/09/2026. Estado: planejamento; patch ainda não implementado, compilado ou validado no jogo.

## 1. Resultado esperado e decisão de arquitetura

Executar o client Aetherius com Skyrim Platform e Meridian UI habilitados, preservando scripting, multiplayer, login, chat, voz, HUD e os dois perfis oficiais do MO2. O renderer de navegador utilizado por essa distribuição será o Meridian; o Skyrim Platform continuará oferecendo suas APIs ao client sem carregar o CEF legado.

Fluxo proposto:

```text
Client Aetherius / sp.browser
          ↕
Adaptador nativo BrowserApiMeridian
          ↕
APIs públicas do Meridian UI
          ↕
CEF 152 e compositor do Meridian
```

Esta é uma modificação do runtime nativo do Skyrim Platform, acompanhada de integração e empacotamento no client. Não depende de um ESP. A proposta evita hospedar duas versões incompatíveis de CEF no mesmo processo. Sua viabilidade será demonstrada por compilação e testes, não presumida pelo desenho.

O launcher permanece como referência de instalação e distribuição. O protocolo do servidor e o comportamento dos mods existentes devem ser preservados. Não faz parte desta rodada atualizar CommonLib, adicionar suporte a VR, reescrever a interface visual ou substituir o runtime do Meridian.

## 2. Evidências e pendências

| Item | Estado observado | Consequência para a execução |
|---|---|---|
| Skyrim / SKSE | 1.6.1170 / 2.2.6 | Primeiro alvo de homologação será exatamente esse ambiente. |
| Client instalado | v0.8.1; arquivos nativos conferem com o release | A investigação parte do pacote realmente distribuído. |
| Meridian instalado | 1.5.0; CEF 152.0.6 | Fixar esse SDK/runtime no primeiro protótipo. |
| CEF do client | 108.4.13 | Não pode continuar sendo carregado pela distribuição integrada. |
| Import requerido pelo Meridian | `cef_get_exit_code`, ausente no CEF 108 | Incompatibilidade binária concreta que deve desaparecer nos testes. |
| Adaptador existente | `BrowserApiNirnLab` | Referência para preservar o contrato, não compatibilidade pronta com Meridian. |
| Linkagem nativa | `SkyrimPlatformImpl.dll` importa `libcef.dll`; CMake liga `ui` e `cef` | A seleção de backend em INI, isoladamente, não resolve o carregamento. |
| Inicialização legada | `TPOverlayService::Create` chama criação do client após `Initialize(...)` | Auditar e corrigir criação/render/reset após falha ou backend externo. |
| Checkout nativo disponível | `aetherius-server` em `AetheriusUI_Core/referencias/repositories` | Confirmar sua correspondência com os binários distribuídos antes de modificar. |
| Commit desse checkout | `c97fb3fe8b1e382d1e87aa09ca0ccf4c3b8d4c29` | Snapshot inspecionado, ainda não comprovado como origem exata do release. |
| Alterações locais nesse checkout | `server/ts/index.ts` e `aetheriusUiSystem.ts` | Trabalhar em checkout isolado; preservar esses arquivos. |
| Compilação local | MSVC 2022 14.44 disponível; CMake 4.3.0-rc3 no PATH | Confirmar compatibilidade e fixar ferramentas na reprodução do build. |
| vcpkg | Ausente nos caminhos esperados examinados | Bootstrap e aquisição das dependências precedem qualquer build. |
| Launcher | Seleciona primeiro release publicado, incluindo pré-releases; instala quando a tag difere | Candidato fica em draft até homologação; pré-release publicado não é ambiente privado de teste. |

Pendências iniciais: identificar o source/build que gerou os binários, confirmar dependências transitivas, inventariar tudo que usa o compositor legado e provar que o contrato web do client pode ser preservado pelas APIs públicas do Meridian.

## 3. Repositórios, locais de trabalho e entregáveis

| Componente | Local/repositório | Alteração planejada |
|---|---|---|
| Runtime Skyrim Platform | Código nativo presente em `AetheriusRP/aetherius-server`, sujeito à confirmação da origem | Backend Meridian, separação do browser legado e preset de compilação. |
| Client | `AetheriusRP/aetherius-client` | Bootstrap/recursos web, configuração, documentação e pacote de release. |
| Meridian | SDK/runtime 1.5.0 já instalado | Consumir APIs públicas, fixar versão e registrar proveniência/licenças. |
| Mods distribuídos | `AetheriusRP/aetherius-mods` | Conferir disponibilidade do Meridian e dependências; alterar assets somente se houver necessidade comprovada. |
| Launcher | `AetheriusRP/aetherius-launcher` | Consulta e testes de instalação com o comportamento atual. |

Criar checkout de implementação e diretórios de build/staging dentro do workspace autorizado, separados dos checkouts de referência. Usar branches próprios no runtime e no client. Registrar commits de ambos na documentação do pacote. Alterar fonte do runtime em revisão isolada; isso não implica criar um release do servidor.

Entregáveis finais: código revisável, build reproduzível, binários com símbolos guardados para diagnóstico, matriz de testes preenchida, manifest de arquivos/hashes, procedimento de recuperação e um ZIP de client publicado com o padrão já utilizado.

## 4. Fases de implementação e execução

### Fase 0 — congelar baseline e preparar recuperação

1. Registrar os commits, hashes de DLLs, versões de ferramentas, configuração efetiva do browser e inventário das dependências.
2. Guardar v0.8.0 e v0.8.1, suas listas de entradas e o estado dos dois perfis oficiais.
3. Fazer backup dos arquivos que a homologação substituirá, incluindo configurações e perfis. Preservar dados pessoais e credenciais; registrar somente a estrutura necessária, sem segredos.
4. Criar uma instância MO2 de homologação com perfis derivados de Qualidade e Performance. Os nomes oficiais continuam reservados ao pacote de produção.
5. Desabilitar hooks de deploy automático que apontem para o jogo/MO2 de produção. Toda compilação inicial entrega apenas em staging.

**Entregável:** baseline documentado, backups conferidos e ambiente de teste separado.

**Critério de avanço:** saber exatamente o que restaurar e demonstrar que testar um build não altera a instalação de produção por acidente.

### Fase 1 — reproduzir a compilação atual

1. Confirmar fonte/tag dos binários do release. Comparar interfaces exportadas, versões, opções e dependências; não atribuir a origem apenas pelo nome da pasta. Diferenças de hash de um rebuild podem decorrer de timestamps, símbolos ou ferramentas e precisam ser explicadas.
2. Preparar VS Developer Shell, CMake e vcpkg. Fixar commit do vcpkg, overlays, triplet, SDK Windows, Node/Frida e aquisição original de CommonLib.
3. Preservar a linhagem e as versões do projeto existente; evitar uma migração de CommonLib junto da correção de UI.
4. Reproduzir primeiro o target nativo atual, sem a integração nova. Confirmar que o artefato gerado é da configuração Release/RelWithDebInfo e não exige CRT de Debug.
5. Documentar comandos reais de configure/build. Só criar o preset integrado após a reprodução do build existente.

**Entregável:** build de controle, log de compilação e ficha de dependências.

**Critério de avanço:** configurar e compilar com sucesso e explicar a correspondência funcional/ABI com o runtime instalado. Um build ausente ou incompleto bloqueia a implementação dependente dele, mas permite continuar documentação e inventário.

### Fase 2 — separar o navegador legado do runtime

1. Mapear dependências de `BrowserApiTilted`, `TPOverlayService`, renderer D3D11, hooks de input, `ui`, wrappers CEF e subprocesso `SkyrimPlatformCEF`.
2. Inventariar funções não relacionadas ao browser que passam por essas classes: textos sobre jogadores, nomes, HUD e renderização auxiliar. Preservá-las em componentes independentes ou planejar migração explícita antes de remover uma classe inteira.
3. Criar configuração de compilação para a distribuição Meridian. Excluir código e linkagem de CEF legado, inclusive dependências transitivas, inicializadores e chamadas remanescentes.
4. Separar os targets de browser legado e Meridian. Se o legado continuar disponível para outra distribuição, mantê-lo em preset separado; o artefato integrado não deve conter uma dependência que carregue CEF 108.
5. Corrigir o ciclo criar/renderizar/resetar/destruir: falha de inicialização não pode ser seguida por criação do navegador nem por uso de objetos inválidos.
6. Tornar a escolha de backend explícita. Auditar `auto`, `off`, branches e fall-through para que nenhum caminho da distribuição integrada volte silenciosamente ao renderer antigo.
7. Encaminhar mensagens SKSE somente ao backend selecionado, evitando registrar o consumidor NirnLab incondicionalmente.

**Entregável:** runtime que mantém as APIs não gráficas e não depende do CEF 108.

**Critério de avanço:** analisar imports/dependências recursivamente e confirmar em execução que o processo não carrega o CEF legado nem inicia o subprocesso legado. O teste desta fase pode ter UI ainda indisponível; não é um client pronto para distribuição.

### Fase 3 — implementar o adaptador Meridian

1. Criar `BrowserApiMeridian` com uma interface interna independente de CEF. Reutilizar o contrato do adaptador NirnLab como referência, sem trocar apenas namespaces ou interpretar ponteiros de uma API como se fossem da outra.
2. Preferir `Meridian.View/1` para propriedade de views, foco e ciclo de vida. Validar no protótipo se cobre todo o contrato; recorrer ao `IUIPlatformAPI` público somente para lacunas demonstradas e com suas regras de lifetime documentadas.
3. Adquirir a interface no estágio SKSE permitido pelo SDK, inicialmente `kInputLoaded`. Criar a view no estágio documentado, inicialmente `kDataLoaded`; provar a disponibilidade da tela de login durante a abertura do jogo.
4. Verificar versão da API e versão suportada do runtime antes de usar a interface. Registrar estados de aquisição, criação, DOM pronto, foco e shutdown.
5. Usar estados explícitos: indisponível → interface adquirida → view criada → DOM pronto → encerrando/encerrado. Falhas saem para um estado de erro, sem repetição infinita ou fallback CEF legado.
6. Separar estado solicitado e estado efetivo. `isFocused()` deve refletir foco concedido, não apenas uma solicitação; visibilidade deve respeitar o contrato existente do client.
7. Guardar operações pré-DOM em fila limitada, com ordem, cancelamento no shutdown e identificação da geração da view. Recriação não pode executar callbacks antigos.
8. Copiar payloads recebidos pelo callback CEF antes de sair da função. Encaminhar acesso a Skyrim/Napi para a fila apropriada do runtime; não executar código de jogo na thread CEF.
9. Liberar somente recursos pertencentes ao client. O adaptador não deve desligar o Meridian global e interromper outros mods.

**Entregável:** adaptador, logs e protótipo de view com comunicação bidirecional.

**Critério de avanço:** criar uma página simples, trocar mensagens nas duas direções, adquirir/liberar foco e fechar sem erro de DLL ou dependência de CEF 108.

### Fase 4 — preservar o contrato web e integrar o client

| Contrato atual | Comportamento exigido na integração |
|---|---|
| `sp.browser.setVisible/isVisible` | Mostrar/ocultar mantendo a semântica dos serviços existentes. |
| `sp.browser.setFocused/isFocused` | Solicitar/liberar foco e comunicar recusas reais, como `Busy` e `NotReady`. |
| `sp.browser.loadUrl` | Carregar conteúdo autorizado; validar tradução das URLs legadas e rejeitar destinos incompatíveis. |
| `sp.browser.executeJavaScript` | Executar após readiness, com fila ordenada e descarte seguro durante shutdown. |
| `window.skyrimPlatform.sendMessage(...)` | Manter namespace, ordem dos argumentos e tipos no payload. |
| `browserMessage.arguments` | Preservar o evento que alimenta auth, browser e bridge services. |
| `front-loaded` | Emitir quando o frontend estiver pronto, sem duplicação acidental por reinjeção. |
| `cef::ui:event` | Preservar o contrato lógico usado pelo transporte de custom packets; o nome não exige CEF legado. |

1. Inventariar HTML/JS/CSS, fontes, imagens, sons, rotas e chamadas a `sp.browser`. Incluir login, chat, pop-ups de acesso/reconexão, widgets e voz.
2. Hospedar a UI em `Data/MeridianUI/aetherius-client/` e usar a origem proposta `mod://aetherius-client/index.html`, com owner correspondente.
3. Adaptar referências de `file://` e caminhos relativos. Validar fetch, armazenamento, URLs de áudio e demais recursos realmente utilizados. Não liberar conteúdo remoto globalmente para contornar o modelo de origem do Meridian.
4. Criar o bootstrap que preserva `window.skyrimPlatform.sendMessage` através do listener do adaptador. Validar serialização completa de strings, booleanos, números, objetos, arrays e caracteres não ASCII.
5. Preservar o protocolo de autenticação e multiplayer. Ajustes TypeScript ficam limitados ao transporte/readiness da UI e tratamento de foco/erro necessários.
6. Coordenar login, chat e pop-ups prioritários com outras views. `Busy` não pode roubar foco; um fluxo crítico deve aguardar ou apresentar indicação clara, sem travar silenciosamente.
7. Usar foco não pausado nos overlays do multiplayer. Preservar WASD, tecla V da voz, atalhos do client e retorno de movimento ao fechar a UI.

**Entregável:** frontend do client operando pelo Meridian, com contrato documentado e configuração `meridian` explícita na distribuição.

**Critério de avanço:** login/handshake, chat e comandos ida/volta funcionando em servidor de teste, com origem e bindings corretos.

### Fase 5 — testes automatizados e inspeção dos artefatos

Criar testes de comportamento que cubram as fronteiras novas, sem exigir iniciar o jogo em cada execução:

- Tradução e validação de URLs; rejeição de origem incorreta/traversal.
- Serialização e round-trip das mensagens e preservação de tipos/ordem.
- Estados de readiness; execução ordenada, fila limitada e cancelamento de callbacks de uma geração anterior.
- Falha de API, foco recusado, fechamento repetido e shutdown durante operação pendente.
- Contrato entre o bootstrap JS, `browserMessage` e os serviços de auth/bridge.
- Inspeção de imports/dependências para impedir reintrodução de CEF legado ou CRT de Debug.
- Manifest de pacote, paths, duplicatas de ZIP, arquivos esperados e hashes.

Mocks do Meridian verificam o adaptador, não provam renderização real. Compilar com sucesso comprova o build, não a compatibilidade no jogo.

**Entregável:** testes executáveis, resultados e relatório de inspeção do build.

**Critério de avanço:** testes determinísticos aprovados e dependências/exports explicados; manter símbolos disponíveis para a homologação.

### Fase 6 — homologação dentro do jogo

Executar primeiro com o mínimo de mods necessário para isolar o adaptador. Depois repetir com os dois perfis completos, sem transformar o perfil mínimo em pacote de produção.

| Cenário | Evidência exigida |
|---|---|
| Abrir pelo SKSE/MO2 | Ambos os plugins carregados corretamente, sem erro 127; módulos do processo mostram o CEF esperado e nenhum CEF 108. |
| Abrir pelo launcher | Mesma combinação de runtime e perfis, caminhos/atalhos corretos e versão instalada identificada. |
| Login permitido/negado | UI visível, campos aceitam texto, servidor recebe mensagens e motivo correto é apresentado. |
| Entrada/spawn/reentrada | Sessão e HUD surgem na ordem correta, sem telas duplicadas ou tarefas antigas. |
| Chat e comandos | Texto, acentos, Enter/Escape e envio ao servidor preservados. |
| Voz | Push-to-talk, transmissão/recepção e controles visuais continuam funcionais; sem vazamento da tecla V por foco incorreto. |
| Pop-ups de acesso/conexão | Motivo, reconectar e fechar executam o fluxo esperado. |
| HUD e nomes | Widgets, textos/nomes sobre jogadores e atalho F1 preservados. |
| Menus e input | Inventário, mapa, console, F2/F6, Alt-Tab e campos de texto devolvem foco/cursor/movimento corretamente. |
| Outros consumidores Meridian | Views de mods realmente presentes e disponíveis não perdem foco, listeners ou runtime quando o client abre/fecha. |
| PrismaUI/outros frameworks | Verificar os frameworks habilitados nos perfis atuais; nomes e presença não comprovam coexistência. |
| Falhas controladas | Meridian ausente, API incompatível, página ausente ou criação recusada geram erro claro e encerram a inicialização de UI com segurança. |
| Encerramento e repetição | Fechar e abrir novamente sem processos órfãos de navegador/voz, crashes ou estado de foco preso. |
| Performance e Qualidade | Passar a matriz em ambos, preservando suas listas e diferenças de ativação. |

Registrar hora, cenário, profile, hash do build, resultado, log e evidência observada. Fazer inicialmente pelo menos três aberturas completas consecutivas por perfil e uma sessão de 30 minutos por perfil com login/chat/voz/menus. Esses mínimos são uma regra proposta de homologação, não garantia estatística; falhas demandam reteste do comportamento afetado e dos cenários relacionados.

Comparar FPS/frame times, memória e estabilidade com a baseline disponível e com o protótipo mínimo. Explicar regressões antes de distribuição.

**Entregável:** matriz preenchida, logs, inventário de módulos e defeitos resolvidos ou explicitamente impeditivos.

**Critério de avanço:** nenhum defeito crítico de abertura, login, rede, input, shutdown ou consumidor existente; a integração continua não aprovada enquanto esses cenários não tiverem evidência no jogo.

### Fase 7 — preparar o pacote e testar instalação

1. Registrar commits do runtime/client, versões do SDK/dependências, comandos de build e hashes em proveniência reproduzível. Revisar licenças e avisos da distribuição dos binários/headers modificados.
2. Recompilar ou atualizar o bundle TypeScript somente quando a implementação o exigir, com sua fonte correspondente.
3. Montar um novo pacote a partir da baseline do client mais as mudanças autorizadas no manifest. Preservar arquivos de gameplay/rede/voz que não precisem mudar.
4. Retirar do novo pacote as dependências CEF legadas identificadas como exclusivas do browser antigo. Confirmar antes que não existem outros consumidores dessas dependências.
5. Manter o padrão externo e os caminhos do launcher:

```text
client-v<versao>.zip
└── client/
    ├── Data/
    │   ├── SKSE/Plugins/SkyrimPlatform.dll
    │   ├── Platform/Distribution/RuntimeDependencies/SkyrimPlatformImpl.dll
    │   ├── Platform/Plugins/...
    │   └── MeridianUI/aetherius-client/...
    └── modOrganizer/
        ├── ModOrganizer.exe
        └── profiles/
            ├── AETHERIUS - GRAFICO - QUALIDADE/
            ├── AETHERIUS - PERFORMANCE/
            └── Default/
```

O conteúdo do Meridian runtime permanece distribuído pelo mod correspondente; validar que o fluxo client → instalação de mods o disponibiliza antes de jogar. Evitar uma segunda cópia de CEF 152 no client que possa divergir do mod.

6. Preservar nomes e formatos de arquivos dos perfis oficiais e o `Default`. Conferir o snapshot de modlist/loadorder aprovado antes de empacotar; nomes dos perfis de teste e artefatos de desenvolvimento não entram no ZIP.
7. Comparar entrada por entrada contra a baseline: somente o manifest aprovado pode divergir. Verificar paths, duplicatas, integridade de todos os conteúdos e SHA-256 do ZIP.
8. Testar instalação limpa e atualização sobre v0.8.1 usando o procedimento do launcher. A instalação atual copia arquivos e não remove automaticamente todos os arquivos antigos: o candidato deve tolerar o CEF antigo remanescente em disco sem carregá-lo. A mesma exigência vale para caches, configurações e mods já existentes.
9. Confirmar que dados pessoais/configurações que o launcher preserva continuam preservados e que o conteúdo modificado chega efetivamente ao jogo/MO2 no disco selecionado.

**Entregável:** candidato completo, manifest de diferenças, checksum e resultado de instalação/atualização.

**Critério de avanço:** pacote instalado pelos caminhos reais, UI e perfis corretos, nenhuma dependência acidental do estado de uma máquina de desenvolvimento.

### Fase 8 — publicar e conferir a distribuição

1. Escolher a próxima versão livre do client; `v0.8.2` é a proposta atual, sujeita a confirmar que continua disponível na execução.
2. Enviar mudanças de fonte revisadas aos repositórios correspondentes e fixar a proveniência do runtime no commit do client associado ao release.
3. Criar release em draft com título e asset no padrão existente: `AetheriusRP Client v<versao>` / `client-v<versao>.zip`. Manter exatamente um asset ZIP de instalação, pois o launcher seleciona o primeiro `.zip`.
4. Conferir o upload pelo digest SHA-256 e tamanho retornados pelo GitHub.
5. Publicar somente após homologação e revisão do pacote. O rótulo pré-release pode seguir o padrão atual, mas não protege usuários do download: o launcher também seleciona pré-releases publicadas.
6. Consultar a API com a mesma lógica do launcher, confirmando que a primeira versão não-draft é o novo release e que aponta para o asset correto.
7. Fazer a atualização real por **VERIFICAR**, confirmar o lock com a nova tag, hashes dos arquivos instalados e lançamento dos dois perfis. Registrar esse teste de distribuição separadamente do teste manual no MO2.

**Entregável:** release publicado e instalação real verificada; releases anteriores preservados.

**Critério de conclusão:** usuários recebem o binário e os recursos corretos pelo launcher e a matriz permanece aprovada após a instalação final.

## 5. Recuperação e tratamento de falhas

- Falha antes de publicar: candidato permanece draft e a correção volta à fase correspondente. Um build incompleto não vira release instalável.
- Falha local de homologação: encerrar jogo/MO2, restaurar o conjunto de arquivos e perfis guardado na fase 0 e verificar seus hashes. Não misturar um runtime antigo com perfis que ativem componentes incompatíveis.
- Falha depois de publicar: preparar um pacote de recuperação baseado em uma combinação que tenha sido validada. Não assumir que v0.8.1 é uma recuperação funcional — essa combinação contém o conflito conhecido com Meridian ativo.
- Como o launcher decide pela lista de releases e pelo lock, simplesmente escolher um release antigo manualmente não garante recuperação automática. Publicar a combinação de recuperação como nova tag, após validar a seleção efetiva da API e o pacote. Preservar releases existentes e registrar o motivo.
- Não substituir o CEF 108 por 152 mantendo código compilado contra ABI antiga; não carregar duas versões por mero renomeamento; não esconder a falha escolhendo continuar o jogo sem avaliar as interfaces perdidas.
- Caso uma API pública do Meridian não atenda a um recurso indispensável, registrar a lacuna e um protótipo reproduzível. Só então decidir entre outro endpoint público ou extensão do runtime. Uma solução com processos CEF separados e IPC será alternativa de arquitetura, não requisito assumido desta primeira rodada.

## 6. Marcos e ordem de trabalho

| Marco | Dependência | Demonstração necessária |
|---|---|---|
| M0 — baseline | Nenhuma | Backups, versões e ambiente de teste definidos. |
| M1 — compilação reproduzida | M0 | Fonte e ferramentas identificadas; controle nativo compilado. |
| M2 — dependência legada removida | M1 | Runtime sem carregar CEF 108, com funções não gráficas preservadas. |
| M3 — integração mínima | M2 | Uma view Meridian e comunicação bidirecional funcionando. |
| M4 — client integrado | M3 | Login, contrato `sp.browser` e frontend funcionando pelo Meridian. |
| M5 — homologado | M4 | Matriz completa aprovada nos dois perfis. |
| M6 — pacote aprovado | M5 | ZIP, instalação limpa e atualização validados. |
| M7 — distribuído | M6 | Release selecionado pelo launcher e instalação final comprovada. |

Documentação, inventário de recursos e desenho dos testes podem avançar em paralelo à preparação da toolchain. Alterações de build e adaptador devem ser integradas em passos pequenos: provar primeiro o protótipo, depois o client completo. Empacotamento e publicação dependem dos resultados das fases anteriores.

Não fixar um prazo de entrega antes de M1/M2: a reprodução da build e a separação do compositor legado são as maiores incertezas. Ao concluir esses marcos, estimar o restante com base nos targets compilados e nas lacunas de integração observadas.

## 7. Checklist final de conclusão

- [ ] Skyrim Platform e Meridian carregam corretamente no runtime alvo.
- [ ] CEF legado ausente da lista de módulos e nenhum subprocesso legado ativo.
- [ ] Contrato `sp.browser`, mensagens, login, chat, voz, HUD e nomes preservados.
- [ ] Foco, input e coexistência com consumidores reais dos perfis aprovados.
- [ ] Falhas de inicialização e encerramento tratadas sem chamadas em objetos inválidos.
- [ ] Fontes, ferramentas, dependências, licenças, logs e hashes registrados.
- [ ] Qualidade e Performance completos homologados.
- [ ] Instalação limpa e atualização sobre o pacote anterior aprovadas.
- [ ] ZIP e nomes dos perfis/assets respeitam o padrão do launcher.
- [ ] Recovery validado como conjunto de runtime, recursos e perfis compatíveis.
- [ ] Release anterior preservado e nova instalação conferida após publicação.

## 8. Referências da decisão

- Fonte nativa inspecionada: `skyrim-platform/src/platform_se/CMakeLists.txt`, `skyrim_platform/BrowserApi.cpp`, `BrowserApiNirnLab.cpp/.h`, `BrowserApiTilted.cpp`, `main.cpp` e `TPOverlayService.cpp`, no checkout de referência de `aetherius-server`.
- Contratos do client: `client/src/services/services/browserService.ts`, `bridgeService.ts`, `authService.ts` e `Accessrevokedservice.ts`.
- [Meridian UI — APIs públicas](https://github.com/heathbrownkeyworks/MeridianUI#public-apis).
- [Meridian.View/1 — guia de consumo](https://github.com/heathbrownkeyworks/MeridianUI/blob/main/docs/MeridianView-AuthorGuide.md).
- [Skyrim Platform — backends de browser](https://github.com/skyrim-multiplayer/skymp/blob/main/docs/skyrim_platform/ini_settings.md).
- [Windows — ordem de carregamento de DLLs](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
- A análise de APIs/imports fundamenta a proposta; não substitui a prova de coexistência, ABI e comportamento dentro do jogo.

