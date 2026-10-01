# Aetherius Client v0.8.2 — compatibilidade Meridian UI / Skyrim Platform

Compatível com Aetherius Server v0.8.0. Mantém o padrão pre-release do client.

## Correção

- Integra o backend Meridian.View/1 e as duas DLLs do candidato 090924, sem
  inicialização do browser CEF legado do Skyrim Platform.
- Inclui bootstrap, widgets, bundle e configuração de produção sem PluginsDev.
- Incorpora o mod `AETHERIUS - MERIDIAN - COMPATIBILIDADE`, com libnode original
  na mesma pasta física da implementação e os avisos de licença.
- Habilita somente esse novo mod na maior prioridade dos dois perfis oficiais.
  Demais entradas das modlists, loadorder, plugins, archives, configurações e
  perfil Default são preservados byte a byte da distribuição v0.8.1.
- Não limpa caches de shader, não copia saves nem inclui perfis TESTE.

Asset único: `client-v0.8.2.zip`, raiz `client/`, estrutura MO2/Data preservada.
SHA-256: `51F63DA130714B737E25A4C46C8B547994A3F17C24546D5947CE9FA29F682550`.
Baseline v0.8.1 conferida no GitHub. Cada entrada foi validada por tamanho/hash;
2084 arquivos permanecem idênticos, cinco são substituídos (três do runtime/
client e duas modlists), e 69 são acrescentados (overlay e avisos).

## Atualização

Feche o jogo, abra o launcher e clique em VERIFICAR. O launcher seleciona a nova
tag e o ZIP, instala Data/MO2 e usa os mesmos nomes dos perfis oficiais. O mod
novo vence o INI antigo de AETHERIUS - OVERWRITE. O mod TESTE não é habilitado
pelas modlists publicadas. Nenhuma atualização de launcher/servidor é necessária.

## Validação e limites

O usuário aprovou o sucesso de abertura no perfil oficial de Qualidade e pediu
encerramento dos testes/publicação. Dois ciclos mínimos de menu/fechamento,
hashes das DLLs/Node, um CEF 152.0.6, 26 testes Node e 2 CTest foram confirmados.
Login, multiplayer, voz, input completo, Performance, instalação limpa e
estabilidade prolongada não foram homologados. Alvo observado: Skyrim Steam
1.6.1170, SKSE 2.2.6 e Meridian UI 1.5.0.

v0.8.1 e todos os releases anteriores permanecem disponíveis. Recuperação exige
reinstalar o client anterior e suas modlists sem o novo mod; desabilitar só o
mod não reverte Data. Não carregar Meridian junto do browser CEF legado.

Fontes nativas correspondentes, DLLs, licenças e documentação:
https://github.com/mercurius17/CompatibilidadeMeridianPlatform
