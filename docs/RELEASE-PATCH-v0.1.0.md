# Meridian UI / Skyrim Platform — compatibilidade v0.1.0

Publicação aprovada pelo usuário após sucesso de abertura no perfil oficial
de Qualidade. Testes no jogo encerrados por solicitação dele.

Inclui revisão da contribuição recebida, fontes completas correspondentes,
DLLs do candidato 20261001-090924, bundle, scripts, licenças, símbolos e
evidências sanitizadas. Dois ciclos mínimos de abrir/fechar confirmados;
26 testes Node e 2 CTest previamente aprovados. Não houve recompilação depois
do candidato testado.

O backend integrado usa Meridian.View/1, sem inicializar o browser CEF antigo
do Skyrim Platform. Correções de configuração PluginsDev, foco/PTT,
fechamento/reconexão e saída pela thread do jogo estão incluídas.

## Pacotes

- `AETHERIUS.-.MERIDIAN.-.COMPATIBILIDADE.zip`: mod MO2 completo, com a pasta
  raiz `AETHERIUS - MERIDIAN - COMPATIBILIDADE`, libnode original junto da
  implementação e avisos de licença. Habilitar na maior prioridade.
- `candidate-20261001-090924-pdb.zip`: símbolos, não instalar como mod.
- `release-package-v0.8.2.json`: inventário e hashes do mod/client.
- `SHA256SUMS.txt`: hashes dos pacotes e símbolos.

Para atualização automática, usar o client v0.8.2 no repositório
`AetheriusRP/aetherius-client`. Não apresentar este repositório como fonte de
mods ao launcher; sua origem de mods continua `AetheriusRP/aetherius-mods`.

## Alvo e limites

Windows x64, Skyrim Steam 1.6.1170, SKSE 2.2.6, Meridian UI 1.5.0 / CEF 152.0.6.
Abertura aprovada no perfil oficial de Qualidade, DLLs/Node conferidos em
execução e somente um libcef observado. Login, multiplayer, voz, input completo,
Performance, instalação limpa e estabilidade prolongada não foram homologados.
Não substituir CEF 108 por CEF 152 em binários antigos nem misturar pares de DLLs.

Distribuições anteriores são preservadas. Consulte `docs/PUBLICACAO-20261001.md`
para instalação, recuperação e detalhes do padrão do launcher.
