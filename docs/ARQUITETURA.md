# Arquitetura e contratos

## Problema observado

A distribuição anterior do client usava CEF 108.4.13, embora o comentário originalmente consultado citasse outra versão do Skyrim Platform. O Meridian instalado era 1.5.0 com CEF 152.0.6. Seu import `cef_get_exit_code` não existia no CEF 108 observado. A coexistência da ABI antiga no mesmo processo provocava erro de carregamento 127.

Mudar somente BackendName em INI não eliminaria o import direto libcef da implementação antiga. Trocar/renomear DLLs CEF tampouco recompila consumidores para a nova ABI.

## Solução implementada

```text
Client TypeScript / sp.browser
            ↕
BrowserApiMeridian (SkyrimPlatformImpl)
            ↕ Meridian.View/1
Meridian UI → seu CEF 152

HUD/TextApi → snapshot JS → DirectXTK/D3D11
```

O backend integrado conserva scripting/Node, rede e APIs de Skyrim Platform, mas hospeda a interface através do provider público Meridian. O HUD não passa pelo compositor CEF antigo.

`SKYRIM_PLATFORM_MERIDIAN=ON` exclui BrowserApiTilted/NirnLab, TPOverlayService, renderer/input legados, Windows/DInput hooks e targets/linkagem ui/cef/subprocesso. OFF conserva a rota legada separada. O manifest vcpkg original ainda adquire CEF como dependência da toolchain; isso **não** é evidência de linkagem/carregamento no target Meridian.

## Ciclo de vida e threads

- Query de Meridian.View/1 em SKSE InputLoaded; criação após DataLoaded por tarefa do runtime.
- Owner `aetherius-client`, view `SkyrimPlatform_Default` e URL autorizada `mod://aetherius-client/index.html`.
- DOM pronto inicia attachment do bootstrap; handshake interno precede o evento front-loaded e liberação dos scripts pendentes.
- Payload do callback CEF é copiado antes do retorno. A tarefa SKSE valida envelope, limites e geração; o worker JavaScript existente produz browserMessage.
- Operações pré-ready têm limites de quantidade/bytes e FIFO. Recriação invalida a geração anterior; callbacks obsoletos são rejeitados.
- Shutdown invalida o estado e destrói somente a view própria. Não há desligamento do Meridian global nem chamada de provider no DllMain.
- Renderer de HUD lê snapshot imutável publicado pelo worker JS, usa SpriteFonts originais e restaura a pipeline D3D11 após desenhar.

Essas regras foram implementadas e parcialmente testadas com mocks. Timing, teardown, hook Present e renderização ainda precisam de prova real prolongada.

## Contratos client

| Superfície | Implementação |
|---|---|
| setVisible/isVisible | Solicitação serializada; estado efetivo consultável |
| setFocused/isFocused | Foco Meridian Unpaused, não-preemptivo; consulta real, não mera intenção |
| loadUrl | Entrada mod:// fixa ou tradução do file:///Data/Platform/UI/index.html antigo, com query/hash validado |
| executeJavaScript | Fila limitada até handshake; invalidação por geração |
| window.skyrimPlatform.sendMessage | Bootstrap serializa argumentos, tipos e Unicode pelo listener público |
| browserMessage.arguments | Evento do runtime alimenta serviços existentes |
| front-loaded | Após DOM/bridge, sem duplicação no bootstrap |
| cef::ui:event | Nome lógico de protocolo preservado; não implica usar CEF antigo |

Destinos arbitrários/remotos e hot reload http://localhost estão desativados no backend integrado. Isso reduz a superfície, mas é uma mudança explícita para desenvolvimento.

BrowserService sincroniza foco real, sinaliza recusa e recebe atalhos durante foco. Pop-ups e formulários críticos não são fechados pelos atalhos interceptados. VoipToggle não ativa PTT V ao digitar e solta PTT ao receber foco.

## Limites de escopo

Não houve patch ESP/Papyrus, atualização de CommonLib, porte VR, substituição do provider Meridian ou redesign do servidor/launcher. SKSE::Init segue a API de um argumento do pin original, não uma sobrecarga de biblioteca mais nova. SDK público copiado sem alterações, com proveniência e licença MIT.

Referências primárias: [Meridian View guide](https://github.com/heathbrownkeyworks/MeridianUI/blob/main/docs/MeridianView-AuthorGuide.md), [Meridian APIs](https://github.com/heathbrownkeyworks/MeridianUI#public-apis), [Windows DLL search](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order).
