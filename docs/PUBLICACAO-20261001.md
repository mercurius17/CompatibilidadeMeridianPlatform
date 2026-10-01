# Publicação da compatibilidade — 01/10/2026

O usuário encerrou os testes e aprovou a publicação após o sucesso de abertura
no perfil oficial de Qualidade. Essa aprovação não transforma funções não
exercitadas em funções homologadas: login, voz, multiplayer, input completo,
Performance e sessões prolongadas continuam sem validação real.

## Distribuições

Publicação concluída e verificada remotamente: todos os drafts abaixo foram
publicados; os assets estão completos e seus hashes coincidem com os locais.
A seleção da API usada pelo launcher aponta para v0.8.2 e seu único ZIP.
O v0.8.1 foi reconferido e permanece com o mesmo asset/digest. O relatório
`reports/publication-20261001.json` registra IDs, commits e horários reais.

- CompatibilidadeMeridianPlatform `v0.1.0`: fontes correspondentes, histórico,
  DLLs do candidato 090924, evidências, scripts, símbolos e overlay instalável.
- Aetherius Client `v0.8.2`, asset único `client-v0.8.2.zip`, com raiz `client/`.
- Aetherius Mods: título `AETHERIUS - MERIDIAN - COMPATIBILIDADE`, tag
  `AETHERIUS.-.MERIDIAN.-.COMPATIBILIDADE-v1.0.0`, asset único
  `AETHERIUS.-.MERIDIAN.-.COMPATIBILIDADE.zip`, raiz com o título exato do mod.

O formato de mods foi conferido em um ZIP já publicado; o launcher usa o título
do release como pasta, agrupa os assets pelo nome sanitizado e ignora drafts.
Para o client, usa o primeiro release publicado e o primeiro asset `.zip`,
inclusive quando o release é pre-release. Não anexar ZIP de símbolos ao release
do client ou de mods: poderia ser selecionado como pacote instalável.

Não é necessário modificar Meridian UI, sua ABI/CEF, launcher ou servidor.
Nenhum novo release desses componentes é criado. O server v0.8.0 é preservado.

## Baseline e perfis

O GitHub confirmou `v0.8.1` como distribuição mais recente do client, commit
`113f10ef479dbd12a182b8763cee319f80a3d00e`, asset 602197793. Seu SHA-256 é
`4BD9615AB3211805A51A34F901FB92B0DFD98AF82A1B98DDAAD5DCC0BB18B2C7` e coincide
com a baseline local usada para criar a prévia. Ela deixou de ser uma baseline
não conferida: foi comparada ao estado remoto antes desta publicação.

O client é derivado dessa distribuição, não de uma pasta pessoal do jogo/MO2.
Os 14 arquivos dos dois perfis oficiais são preservados, exceto pela adição de
uma linha habilitando o novo mod no topo de cada `modlist.txt`. Removida essa
linha, o conteúdo deve coincidir byte a byte com v0.8.1. Default, loadorder,
plugins, archives, settings, saves, shaders e presets não são alterados.
Nenhum perfil TESTE nem nome de mod TESTE é incluído na modlist oficial.

O overlay completo também está em `client/modOrganizer/mods/<nome do mod>/`.
Assim, a atualização do client é suficiente para colocá-lo no MO2; o release de
mods fornece a mesma distribuição para instalação/sincronização pelo launcher.
A maior prioridade do mod resolve o INI antigo de AETHERIUS - OVERWRITE que
ainda habilita PluginsDev. Impl e libnode ficam na mesma pasta física do mod,
como exige a carga privada de dependências no MO2.

As duas DLLs e o bundle são exatamente os já testados; não há recompilação
posterior nem mudança de código nativo nesta publicação. O Node não foi
alterado e é redistribuído com seu aviso oficial de licença v23.2.0. Recursos
CEF legados da baseline são mantidos para não remover dependências de terceiros
sem comprovação: o backend integrado não os carrega, conforme inspeções do
candidato. Isso não afirma que todo outro consumidor CEF foi homologado.

## Uso e recuperação

Feche o jogo antes de atualizar. No launcher, use VERIFICAR para obter a nova
tag de client e sincronizar os mods. Os dois perfis oficiais passam a habilitar
o mod de compatibilidade com maior prioridade. O antigo mod TESTE pode continuar
fisicamente instalado, mas não é habilitado pelas modlists distribuídas.

Instalação manual do mod: extrair o ZIP em `modOrganizer/mods`, conservando sua
pasta raiz, habilitar o mod e dar-lhe maior prioridade que AETHERIUS - OVERWRITE
e quaisquer outros provedores de SkyrimPlatform. Requer Meridian UI 1.5.0,
SKSE 2.2.6 e Skyrim Steam 1.6.1170, alvo efetivamente observado.

O client integrado requer Meridian; desabilitar apenas o mod não restaura o
browser antigo porque Data também foi corrigido. Para recuperação, reinstalar
o client v0.8.1 e as modlists anteriores, sem o novo mod. Meridian e o browser
legado não devem ser carregados juntos. Guardar backups antes de atualizar.
Não deletar tags, releases ou assets antigos.

## Evidências e limites

26 testes Node e 2 CTest previamente aprovados; dois ciclos mínimos de menu e
saída normal com shutdown CEF; abertura no perfil oficial de Qualidade, hashes
de DLLs/Node, Meridian 1.5.0 e um CEF 152.0.6. Esses resultados estão nos reports
do candidato e não se referem a uma instalação limpa do novo release completo.

O empacotamento valida hashes de cada entrada e uma lista explícita de mudanças
permitidas. O relatório `reports/release-package-v0.8.2.json` registra os pacotes
preparados; a confirmação remota fica em `reports/publication-20261001.json`.
Não há novos testes no jogo nesta etapa, a pedido do usuário.
