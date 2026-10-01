const path = require('path');

// Como o SkyMP copia o arquivo de entrada para um diretorio temporario do Windows no boot,
// precisamos resolver o caminho original do gamemode de forma absoluta usando process.cwd().
const gamemodeDir = path.resolve(process.cwd(), './gamemode');

// ─────────────────────────────────────────────────────────────────────────────
// .env — PRECISA vir antes de qualquer outro require deste arquivo.
// ─────────────────────────────────────────────────────────────────────────────
require(path.join(gamemodeDir, 'node_modules', 'dotenv'))
  .config({ path: path.join(gamemodeDir, '.env'), quiet: true });

const db            = require(path.join(gamemodeDir, 'database'));
const whitelist     = require(path.join(gamemodeDir, 'whitelist'));
const commands      = require(path.join(gamemodeDir, 'commands'));
const moduleRegistry = require(path.join(gamemodeDir, 'core', 'module-registry'));
const uiEventRouter  = require(path.join(gamemodeDir, 'core', 'ui-event-router'));
const serverOptions  = require(path.join(gamemodeDir, 'core', 'server-options'));
const governance    = require(path.join(gamemodeDir, 'governance-service'));
const marketStalls  = require(path.join(gamemodeDir, 'market-stalls-service'));
const playerPanel   = require(path.join(gamemodeDir, 'player-panel-service'));
const deathService  = require(path.join(gamemodeDir, 'death-service'));
const voipService   = require(path.join(gamemodeDir, 'voip-service'));
const soulService   = require(path.join(gamemodeDir, 'soul-service'));

console.log("[phase0] SkyMP Heavy RP gamemode loaded");

// ─────────────────────────────────────────────────────────────────────────────
// Registro de módulos
// ─────────────────────────────────────────────────────────────────────────────

moduleRegistry.register({
  id: 'npc-cleaner',
  enabledBy: 'ENABLE_NPC_CLEANER',
  phase: 'core',
  dependencies: [],
  commands: [],
  initialize: async () => {
    require(path.join(gamemodeDir, 'npc-cleaner')).startWorldCleaner();
  }
});

moduleRegistry.register({
  id: 'death',
  enabledBy: 'ENABLE_DEATH_SERVICE',
  phase: 'lab',
  dependencies: [],
  commands: deathService.commandDefs(),
  initialize: async () => {
    deathService.initDeathService();
  }
});

moduleRegistry.register({
  id: 'governance',
  enabledBy: 'ENABLE_GOVERNANCE_SERVICE',
  phase: 'lab',
  dependencies: [],
  commands: governance.commandDefs(),
  initialize: async () => {
    await governance.initGovernanceService();
    uiEventRouter.register('governance', governance.handleUiEvent);
  },
  shutdown: async () => {
    uiEventRouter.unregister('governance');
    governance.shutdownGovernanceService();
  }
});

moduleRegistry.register({
  id: 'market-stalls',
  enabledBy: 'ENABLE_MARKET_STALLS_SERVICE',
  phase: 'lab',
  dependencies: ['governance'],
  commands: marketStalls.commandDefs(),
  initialize: async () => {
    await marketStalls.initMarketStallsService();
  },
  shutdown: async () => {
    marketStalls.shutdownMarketStallsService();
  }
});

moduleRegistry.register({
  id: 'player-panel',
  enabledBy: 'ENABLE_PLAYER_PANEL_SERVICE',
  phase: 'lab',
  dependencies: ['governance'],
  commands: playerPanel.commandDefs(),
  initialize: async () => {
    await playerPanel.initPlayerPanelService();
    uiEventRouter.register('panel', playerPanel.handleUiEvent);
  },
  shutdown: async () => {
    uiEventRouter.unregister('panel');
    playerPanel.shutdownPlayerPanelService();
  }
});

moduleRegistry.register({
  id: 'soul',
  enabledBy: 'ENABLE_SOUL_SERVICE',
  phase: 'lab',
  dependencies: [],
  commands: soulService.commandDefs(),
  initialize: async () => {
    await soulService.initSoulService();
  },
  shutdown: async () => {
    soulService.shutdownSoulService();
  }
});

moduleRegistry.register({
  id: 'voip',
  enabledBy: 'ENABLE_VOIP_SERVICE',
  phase: 'lab',
  dependencies: [],
  commands: voipService.commandDefs(),
  initialize: async () => {
    voipService.startVoipServer();
  }
});

// ─────────────────────────────────────────────────────────────────────────────
// Inicialização do servidor
// ─────────────────────────────────────────────────────────────────────────────

async function boot() {
  try {
    const options = serverOptions.load();
    console.log(`[phase0] server-options: ${options.usedFile ? options.path : 'nenhum arquivo, usando defaults'}`);
    for (const warning of options.warnings) {
      console.warn(`[phase0] server-options: ${warning}`);
    }

    db.init();
    console.log("[phase0] Database pool initialized");

    await moduleRegistry.bootAll();

  } catch (err) {
    console.error("[phase0] Fatal: Could not initialize database or services:", err.message);
  }
}

boot();

// ─────────────────────────────────────────────────────────────────────────────
// Runtime SkyMP
// ─────────────────────────────────────────────────────────────────────────────

if (typeof mp !== "undefined") {
  console.log("[phase0] mp API available");

  mp.makeProperty('browserVisible', {
    isVisibleByOwner: true,
    isVisibleByNeighbors: false,
    updateOwner: 'ctx.sp.browser.setVisible(ctx.value);',
    updateNeighbor: ''
  });
  mp.makeProperty('browserFocused', {
    isVisibleByOwner: true,
    isVisibleByNeighbors: false,
    updateOwner: 'ctx.sp.browser.setFocused(ctx.value);',
    updateNeighbor: ''
  });
  mp.makeProperty('browserModal', {
    isVisibleByOwner: true,
    isVisibleByNeighbors: false,
    updateOwner: `
      if (ctx.value && ctx.sp && ctx.sp.browser && ctx.sp.browser.executeJavaScript) {
        const payload = JSON.stringify(ctx.value);
        ctx.sp.browser.executeJavaScript('window.handleServerModal && window.handleServerModal(' + payload + ')');
      }
    `,
    updateNeighbor: ''
  });
  mp.makeProperty('panelData', {
    isVisibleByOwner: true,
    isVisibleByNeighbors: false,
    updateOwner: `
      if (ctx.value && ctx.sp && ctx.sp.browser && ctx.sp.browser.executeJavaScript) {
        const payload = JSON.stringify(ctx.value);
        ctx.sp.browser.executeJavaScript('window.handlePanelData && window.handlePanelData(' + payload + ')');
      }
    `,
    updateNeighbor: ''
  });
  mp.makeProperty('voipTicket', {
    isVisibleByOwner: true,
    isVisibleByNeighbors: false,
    updateOwner: `
      if (ctx.value && ctx.sp && ctx.sp.browser && ctx.sp.browser.executeJavaScript) {
        const payload = JSON.stringify(ctx.value);
        ctx.sp.browser.executeJavaScript('window.handleVoipTicket && window.handleVoipTicket(' + payload + ')');
      }
    `,
    updateNeighbor: ''
  });

  mp.on('customPacket', (userId, content) => {
    try {
      const actorId = mp.getUserActor(userId);
      if (!actorId) return;

      const uiEvent = JSON.parse(content);
      console.log(`[phase0] customPacket from actor ${actorId.toString(16)}:`, uiEvent);
      uiEventRouter.dispatch(actorId, uiEvent).catch(err =>
        console.error('[phase0] ui-event-router dispatch failed:', err.message)
      );
      if (uiEvent.type === 'cef::chat:send') {
        const text = uiEvent.data;
        commands.handleChatInput(actorId, text);
      }
      if (uiEvent.type === 'cef::voip:reconnect') {
        voipService.requestListenerReconnect(actorId);
      }
    } catch (err) {
      console.error("[phase0] Error in customPacket handler:", err.message);
    }
  });
} else {
  console.log("[phase0] mp API not available");
}

const activeUsers = new Set();
const userActorMap = new Map();

// Polling de Conexões de Rede (2 em 2 segundos)
setInterval(() => {
  if (typeof mp === "undefined") return;

  for (let userId = 1; userId <= 10; userId++) {
    const connected = mp.isConnected(userId);
    if (connected && !activeUsers.has(userId)) {
      console.log(`[phase0] Connection detected! User ID: ${userId}`);

      try {
        const actorId = mp.getUserActor(userId);
        console.log(`[phase0] User ${userId} actor:`, actorId ? actorId.toString(16) : 'none');

        if (actorId) {
          // Só marca como "processado" agora que o ator realmente existe —
          // se marcássemos antes, e o ator ainda não tivesse carregado, este
          // usuário nunca mais seria reavaliado nos próximos ticks.
          activeUsers.add(userId);

          let foundProfileId = -1;
          for (let pId = 1; pId <= 50; pId++) {
            const actors = mp.getActorsByProfileId(pId);
            if (actors && actors.includes(actorId)) {
              foundProfileId = pId;
              break;
            }
          }
          console.log(`[phase0] User ${userId} mapped to profileId: ${foundProfileId}`);
          userActorMap.set(userId, actorId);

          if (foundProfileId !== -1) {
            if (process.env.ALLOW_LOCAL_AUTOWHITELIST === 'true') {
              // Aprovação automática local — sem tocar no banco de dados.
              // Ainda assim precisamos registrar o personagem como "ativo",
              // porque é esse registro que /voz e outros comandos consultam
              // (normalmente feito pelo whitelist.js no caminho de sucesso,
              // que estamos pulando aqui).
              console.log(`[phase0] ALLOW_LOCAL_AUTOWHITELIST=true: auto-aprovando user ${userId} (profileId ${foundProfileId}) sem checagem no banco.`);
              commands.registerActiveCharacter(
                actorId,
                { id: foundProfileId, first_name: 'Local', last_name: `Tester${foundProfileId}` },
                userId,
                foundProfileId
              );

              // Limpa qualquer toast antigo preso na property (ex: de uma
              // tentativa anterior de /voz que falhou antes do personagem
              // estar registrado) — senão o motor re-sincroniza o valor
              // velho e o toast reaparece sozinho, em loop.
              if (typeof mp !== 'undefined') {
                try {
                  mp.set(actorId, 'browserModal', null);
                  voipService.connectOnSpawn(actorId);
                } catch (err) {
                  console.error(`[phase0] Failed to clear stale browserModal for actor ${actorId.toString(16)}:`, err.message);
                }
              }
            } else {
              whitelist.checkWhitelist(userId, foundProfileId, actorId)
                .then((allowed) => {
                  if (allowed) {
                    console.log(`[phase0] User ${userId} successfully approved by database check.`);
                  } else {
                    console.log(`[phase0] User ${userId} was rejected and kicked by database check.`);
                    activeUsers.delete(userId);
                    userActorMap.delete(userId);
                    commands.removeActiveCharacter(actorId);
                    playerPanel.cleanup(actorId);
                  }
                })
                .catch((err) => {
                  console.error(`[phase0] Error in async checkWhitelist for user ${userId}:`, err.message);
                  if (typeof mp !== 'undefined') mp.kick(userId);
                  activeUsers.delete(userId);
                  userActorMap.delete(userId);
                  commands.removeActiveCharacter(actorId);
                  playerPanel.cleanup(actorId);
                });
            }
          } else {
            console.log(`[phase0] User ${userId} actor ${actorId.toString(16)} has no associated profileId in server registry.`);
          }
        }
      } catch (err) {
        console.error(`[phase0] Error processing connection for user ${userId}:`, err.message);
      }
    } else if (!connected && activeUsers.has(userId)) {
      activeUsers.delete(userId);
      console.log(`[phase0] Disconnection detected! User ID: ${userId}`);

      const actorId = userActorMap.get(userId);
      userActorMap.delete(userId);
      if (actorId) {
        try {
          commands.removeActiveCharacter(actorId);
          playerPanel.cleanup(actorId);
        } catch (err) {
          console.error(`[phase0] Error cleaning up disconnected user ${userId}:`, err.message);
        }
      }
    }
  }
}, 2000);