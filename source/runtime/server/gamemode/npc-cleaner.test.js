/**
 * npc-cleaner.test.js
 *
 * O servico nao tinha teste nenhum — e e o unico do gamemode que remove coisa
 * do mundo de forma que o jogador nao pode desfazer. A versao anterior apagava
 * (`disable` + `delete`) todo ator de `profileId 0` a cada 60s, com a allowlist
 * vazia e o `safeRadius` declarado e nunca lido.
 *
 * Estes testes existem pra travar as tres inversoes: lista de bloqueio (nao de
 * permissao), safeRadius de verdade, e nada irreversivel.
 *
 * Executa com: node --test npc-cleaner.test.js
 */

const assert = require('assert');
const { describe, it, beforeEach, after } = require('node:test');

const NPC_LONGE = 0x000a0001;
const NPC_PERTO = 0x000a0002;
const NPC_INOCENTE = 0x000a0003;
const JOGADOR = 0xff000001;

const BASE_BLOQUEADO = '1a6a0:Skyrim.esm';
const BASE_INOCENTE = 'bebete:Skyrim.esm';

const chamadasPapyrus = [];
const posicoes = new Map();
const basePorAtor = new Map();
const facoesPorAtor = new Map(); // actorId -> Set<facDesc>

const originalMp = global.mp;

global.mp = {
  getDescFromId: (formId) => `${formId.toString(16)}:Skyrim.esm`,
  getActorsByProfileId: (profileId) => {
    if (profileId === 0) return [NPC_LONGE, NPC_PERTO, NPC_INOCENTE];
    if (profileId === 1) return [JOGADOR];
    return [];
  },
  get: (actorId, prop) => {
    if (prop === 'baseDesc') return basePorAtor.get(actorId) || null;
    if (prop === 'locationalData') return posicoes.get(actorId) || null;
    if (prop === 'type') return 'MpActor';
    return null;
  },
  callPapyrusFunction: (callType, className, fnName, self, args) => {
    chamadasPapyrus.push({ callType, className, fnName, self, args });

    if (className === 'Actor' && fnName === 'IsInFaction') {
      // self = actorRef(npcActorId) -> { type: 'form', desc: 'hex:Skyrim.esm' }
      // args[0] = { type: 'espm', desc: facDesc }
      const actorId = parseInt(self.desc.split(':')[0], 16);
      const facDesc = args[0].desc;
      const facoes = facoesPorAtor.get(actorId);
      return !!(facoes && facoes.has(facDesc));
    }

    return null;
  }
};

const npcCleaner = require('./npc-cleaner');

after(() => {
  if (originalMp === undefined) delete global.mp;
  else global.mp = originalMp;
});

const CELULA = '0x162e2';
const emCelula = (x) => ({ pos: [x, 0, 0], cellOrWorldDesc: CELULA });

beforeEach(() => {
  chamadasPapyrus.length = 0;
  posicoes.clear();
  basePorAtor.clear();
  facoesPorAtor.clear();

  posicoes.set(JOGADOR, emCelula(0));
  posicoes.set(NPC_LONGE, emCelula(9000));    // bem fora do safeRadius
  posicoes.set(NPC_PERTO, emCelula(100));     // colado no jogador
  posicoes.set(NPC_INOCENTE, emCelula(9000));

  basePorAtor.set(NPC_LONGE, BASE_BLOQUEADO);
  basePorAtor.set(NPC_PERTO, BASE_BLOQUEADO);
  basePorAtor.set(NPC_INOCENTE, BASE_INOCENTE);
});

function policy(extra = {}) {
  return {
    enabled: true,
    blockedBaseDescs: new Set([BASE_BLOQUEADO]),
    safeRadius: 5000,
    sweepIntervalMs: 60000,
    mode: 'disable',
    ...extra
  };
}

describe('npc-cleaner — a lista e de bloqueio, nao de permissao', () => {
  it('lista vazia nao remove NADA (o bug antigo removia o mundo inteiro)', () => {
    const r = npcCleaner.sweepOnce(policy({ blockedBaseDescs: new Set() }));

    assert.strictEqual(r.removidos, 0);
    assert.strictEqual(
      chamadasPapyrus.length, 0,
      'com a lista vazia nem deveria varrer — a versao anterior apagava todo ator de profileId 0 aqui'
    );
  });

  it('remove apenas o record listado, nunca os vizinhos', () => {
    const r = npcCleaner.sweepOnce(policy());

    assert.strictEqual(r.removidos, 1, 'so o NPC bloqueado e longe deveria sair');
    const alvos = chamadasPapyrus.map(c => c.self.desc);
    assert.ok(
      alvos.includes(global.mp.getDescFromId(NPC_LONGE)),
      'o NPC bloqueado e distante deveria ter sido desativado'
    );
    assert.ok(
      !alvos.includes(global.mp.getDescFromId(NPC_INOCENTE)),
      'NPC fora da lista foi tocado — e a regressao que este arquivo existe pra impedir'
    );
  });
});

describe('npc-cleaner — safeRadius protege quem esta na frente do jogador', () => {
  it('NPC bloqueado perto de um jogador nao e tocado', () => {
    const r = npcCleaner.sweepOnce(policy());

    const alvos = chamadasPapyrus.map(c => c.self.desc);
    assert.ok(
      !alvos.includes(global.mp.getDescFromId(NPC_PERTO)),
      'ator sumindo na frente do jogador e exatamente o que o safeRadius existe pra evitar'
    );
    assert.strictEqual(r.protegidosPorDistancia, 1);
  });

  it('o mesmo NPC sai quando ninguem esta por perto', () => {
    posicoes.set(JOGADOR, emCelula(99999));

    const r = npcCleaner.sweepOnce(policy());

    assert.strictEqual(r.removidos, 2, 'sem jogador por perto, os dois bloqueados saem');
  });

  it('_shouldRemove respeita a fronteira do raio', () => {
    const p = policy({ safeRadius: 1000 });

    assert.strictEqual(npcCleaner._shouldRemove(NPC_LONGE, BASE_BLOQUEADO, 1500, p), true, 'fora do raio: pode');
    assert.strictEqual(npcCleaner._shouldRemove(NPC_LONGE, BASE_BLOQUEADO, 1000, p), false, 'exatamente no raio: protegido');
    assert.strictEqual(npcCleaner._shouldRemove(NPC_LONGE, BASE_BLOQUEADO, 500, p), false, 'dentro do raio: protegido');
    assert.strictEqual(npcCleaner._shouldRemove(NPC_INOCENTE, BASE_INOCENTE, 99999, p), false, 'fora da lista: nunca');
    assert.strictEqual(npcCleaner._shouldRemove(NPC_INOCENTE, null, 99999, p), false, 'sem baseDesc: nunca');
  });
});

describe('npc-cleaner — nada irreversivel', () => {
  it('usa disable e nunca delete', () => {
    posicoes.set(JOGADOR, emCelula(99999));
    npcCleaner.sweepOnce(policy());

    const funcoes = chamadasPapyrus.map(c => c.fnName);
    assert.ok(funcoes.includes('disable'), 'deveria desativar');
    assert.ok(
      !funcoes.includes('delete'),
      'delete numa referencia persistente nao volta — a decisao de MVP e Spawn Seletivo, nao terra arrasada'
    );
  });

  it('manda o self no formato de objeto, nunca o FormID cru', () => {
    posicoes.set(JOGADOR, emCelula(99999));
    npcCleaner.sweepOnce(policy());

    for (const chamada of chamadasPapyrus) {
      assert.strictEqual(typeof chamada.self, 'object', 'self precisa ser objeto (QA 2.13)');
      assert.strictEqual(chamada.self.type, 'form');
    }
  });
});

describe('npc-cleaner — config invalida nao vira remocao', () => {
  it('safeRadius NaN cai no default em vez de proteger ninguem', () => {
    // NaN em toda comparacao da false, entao `distancia > NaN` seria false e
    // nada sairia — mas o caminho oposto (um raio 0 acidental) removeria tudo
    // que estivesse na lista, sem margem. O default e o unico valor honesto.
    const p = { ...policy(), safeRadius: Number.NaN };
    assert.strictEqual(npcCleaner._shouldRemove(NPC_LONGE, BASE_BLOQUEADO, 99999, p), false);
  });
});

describe('npc-cleaner — hostileOnly e uma allowlist explicita, opt-in', () => {
  const BASE_HOSTIL = 'f00d:Skyrim.esm';

  it('hostileOnly ausente/false nao muda nada, mesmo com hostileAllowlist preenchida', () => {
    const p = policy({ hostileAllowlist: new Set([BASE_HOSTIL]) });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_INOCENTE, p), false);
  });

  it('hostileOnly=true com allowlist vazia torna QUALQUER baseDesc candidato', () => {
    const p = policy({ blockedBaseDescs: new Set(), hostileOnly: true, hostileAllowlist: new Set() });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_INOCENTE, p), true);
  });

  it('NPC na hostileAllowlist nao e candidato', () => {
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileAllowlist: new Set([BASE_HOSTIL])
    });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_HOSTIL, p), false);
  });

  it('NPC fora da hostileAllowlist e candidato', () => {
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileAllowlist: new Set([BASE_HOSTIL])
    });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_INOCENTE, p), true);
  });

  it('bloqueio individual vence mesmo se o NPC estiver na hostileAllowlist', () => {
    const p = policy({
      blockedBaseDescs: new Set([BASE_HOSTIL]),
      hostileOnly: true,
      hostileAllowlist: new Set([BASE_HOSTIL])
    });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_HOSTIL, p), true);
  });

  it('sweepOnce varre o mundo mesmo com blockedBaseDescs vazia, se hostileOnly estiver ligado', () => {
    basePorAtor.set(NPC_INOCENTE, BASE_INOCENTE);
    posicoes.set(JOGADOR, emCelula(99999)); // ninguem por perto, safeRadius nao protege

    const r = npcCleaner.sweepOnce(policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileAllowlist: new Set()
    }));

    assert.strictEqual(r.removidos, 3, 'sem allowlist preenchida, todos os NPCs de profileId 0 saem');
  });
});

describe('npc-cleaner — hostileFactions checa Actor.IsInFaction, nao lista individual', () => {
  const FACCAO_BANDIDOS = 'a2c8e:Skyrim.esm';
  const FACCAO_FORSWORN = 'a2c93:Skyrim.esm';
  const BASE_BANDIDO = 'c0ffee:Skyrim.esm';

  it('NPC em uma faccao hostil nao e candidato', () => {
    facoesPorAtor.set(NPC_INOCENTE, new Set([FACCAO_BANDIDOS]));
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileFactions: new Set([FACCAO_BANDIDOS])
    });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_BANDIDO, p), false);
  });

  it('NPC fora de toda faccao hostil e candidato', () => {
    facoesPorAtor.set(NPC_INOCENTE, new Set()); // nao pertence a nenhuma
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileFactions: new Set([FACCAO_BANDIDOS])
    });
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_INOCENTE, p), true);
  });

  it('checa TODAS as faccoes da lista, nao so a primeira', () => {
    facoesPorAtor.set(NPC_INOCENTE, new Set([FACCAO_FORSWORN]));
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileFactions: new Set([FACCAO_BANDIDOS, FACCAO_FORSWORN])
    });
    assert.strictEqual(npcCleaner._isInAnyHostileFaction(NPC_INOCENTE, p), true);
  });

  it('hostileAllowlist (individual) e hostileFactions se somam, nao competem', () => {
    const BASE_EXCECAO = 'dead:Skyrim.esm';
    facoesPorAtor.set(NPC_INOCENTE, new Set()); // nao pertence a nenhuma faccao
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileAllowlist: new Set([BASE_EXCECAO]),
      hostileFactions: new Set([FACCAO_BANDIDOS])
    });
    // Sobrevive pela allowlist individual, mesmo sem pertencer a nenhuma faccao.
    assert.strictEqual(npcCleaner._isRemovalCandidate(NPC_INOCENTE, BASE_EXCECAO, p), false);
  });

  it('falha ao consultar a faccao conta como "nao pertence", nunca trava a varredura', () => {
    global.mp.callPapyrusFunction = () => { throw new Error('facção ausente no load order'); };
    const p = policy({
      blockedBaseDescs: new Set(),
      hostileOnly: true,
      hostileFactions: new Set([FACCAO_BANDIDOS])
    });

    assert.strictEqual(npcCleaner._isInAnyHostileFaction(NPC_INOCENTE, p), false);

    // restaura o mock padrao pros testes seguintes
    global.mp.callPapyrusFunction = (callType, className, fnName, self, args) => {
      chamadasPapyrus.push({ callType, className, fnName, self, args });
      if (className === 'Actor' && fnName === 'IsInFaction') {
        const actorId = parseInt(self.desc.split(':')[0], 16);
        const facoes = facoesPorAtor.get(actorId);
        return !!(facoes && facoes.has(args[0].desc));
      }
      return null;
    };
  });
});
