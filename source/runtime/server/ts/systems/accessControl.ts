/* eslint-disable @typescript-eslint/no-explicit-any */
//
// Controle de acesso da alpha: whitelist (quem pode entrar) e blocklist
// (quem não pode, por conta ou por máquina).
//
// A fonte da verdade é o banco do SITE (Postgres), o mesmo que o login já usa
// para validar o token do launcher — site e jogo leem e escrevem nas mesmas
// tabelas, então não existem duas listas que se desencontram. O schema, o
// contrato com o site e o fluxo completo estão em
// docs/technical/ALPHA_ACCESS_CONTROL.md.
//
// A máquina é identificada pelo hash SHA-256 dos MACs dos adaptadores físicos,
// calculado pelo launcher e gravado pelo site em `sessions.device_hashes` no
// login. É uma barreira contra o jogador comum, não contra quem falsifica MAC.
//
// Desligado por padrão (GAME_ACCESS_CONTROL=true liga): o servidor não pode
// passar a recusar todo mundo antes de o site aplicar o schema.

import { Client, Pool } from "pg";
import { Log, System, SystemContext } from "./system";

const NOTIFY_CHANNEL = "game_access_changed";
const RECHECK_INTERVAL_MS = 60 * 1000;
const RECHECK_DEBOUNCE_MS = 1000;
const LISTEN_RETRY_MS = 10 * 1000;
// Quem perde o acesso com o jogo aberto vê um pop-up e o próprio jogo fecha
// quando a pessoa confirma (client v0.8.0+). O kick do servidor é só a rede de
// segurança para quem não clica ou está com um client antigo.
const KICK_DELAY_MS = 30 * 1000;
const MAX_DEVICE_HASHES = 16;
const DEVICE_HASH_RE = /^[0-9a-f]{64}$/;

export interface ActiveBlock {
  reason: string;
  expiresAt: Date | null;
}

export interface WhitelistEntry {
  deviceHashes: string[] | null;
}

export interface AccessDecision {
  allowed: boolean;
  reason?: string;
  /** Whitelist ainda sem máquina: vincular às máquinas desta sessão. */
  bindDevices?: boolean;
}

export const MESSAGES = {
  notWhitelisted: "Sua conta não está liberada para a alpha.",
  deviceMismatch: "Esta conta está vinculada a outro computador. Fale com a staff para liberar a troca.",
  noDevice: "Atualize o launcher para entrar na alpha.",
  unavailable: "Não foi possível verificar seu acesso agora. Tente novamente em instantes.",
};

/** Hashes válidos, sem repetição, com teto — o valor vem do cliente. */
export function normalizeDeviceHashes(value: unknown): string[] {
  if (!Array.isArray(value)) return [];
  const out: string[] = [];
  for (const item of value) {
    if (typeof item !== "string") continue;
    const hash = item.trim().toLowerCase();
    if (DEVICE_HASH_RE.test(hash) && !out.includes(hash)) out.push(hash);
    if (out.length >= MAX_DEVICE_HASHES) break;
  }
  return out;
}

export function describeBlock(block: ActiveBlock): string {
  const until = block.expiresAt
    ? `até ${block.expiresAt.toLocaleString("pt-BR", { timeZone: "America/Sao_Paulo" })}`
    : "permanente";
  return `Acesso bloqueado: ${block.reason} (${until}).`;
}

/**
 * A regra inteira, sem banco — o site implementa a mesma no login dele.
 * Ordem: bloqueio vence tudo; com a whitelist ligada, a conta precisa estar
 * nela e, se já vinculada, a sessão precisa vir de uma das máquinas dela.
 */
export function decideAccess(input: {
  block: ActiveBlock | null;
  whitelistEnabled: boolean;
  whitelistEntry: WhitelistEntry | null;
  sessionDeviceHashes: string[];
}): AccessDecision {
  const { block, whitelistEnabled, whitelistEntry, sessionDeviceHashes } = input;

  if (block) return { allowed: false, reason: describeBlock(block) };
  if (!whitelistEnabled) return { allowed: true };

  if (!whitelistEntry) return { allowed: false, reason: MESSAGES.notWhitelisted };
  // Sem hash da máquina não dá pra vincular nem conferir: launcher antigo, ou
  // alguém entrando sem passar pelo launcher.
  if (sessionDeviceHashes.length === 0) return { allowed: false, reason: MESSAGES.noDevice };

  const bound = whitelistEntry.deviceHashes;
  if (!bound || bound.length === 0) return { allowed: true, bindDevices: true };
  // Qualquer adaptador em comum basta: trocar do Wi-Fi pro cabo é a mesma máquina.
  const sameMachine = sessionDeviceHashes.some((hash) => bound.includes(hash));
  return sameMachine ? { allowed: true } : { allowed: false, reason: MESSAGES.deviceMismatch };
}

/** "30m", "12h", "3d", "2w" → ms; "perm"/"permanente" → null; inválido → undefined. */
export function parseDuration(text: string): number | null | undefined {
  const value = text.trim().toLowerCase();
  if (value === "perm" || value === "permanente") return null;
  const match = /^(\d+)(m|h|d|w)$/.exec(value);
  if (!match) return undefined;
  const amount = Number(match[1]);
  if (amount <= 0) return undefined;
  const unit = { m: 60e3, h: 3600e3, d: 86400e3, w: 7 * 86400e3 }[match[2] as "m" | "h" | "d" | "w"];
  return amount * unit;
}

interface SiteUser {
  id: string;
  username: string;
  profileId: number;
}

interface OnlineEntry {
  siteUserId: string;
  deviceHashes: string[];
}

export interface Actor {
  /** profile_id de quem executou, para registrar o usuário do site dele. */
  profileId?: number;
  /** Nome legível (personagem) caso o profile_id não resolva. */
  label: string;
}

export interface CommandResult {
  ok: boolean;
  message: string;
}

export class AccessControl implements System {
  systemName = "AccessControl";

  private pool: Pool | null = null;
  private listenClient: Client | null = null;
  private ctx: SystemContext | null = null;
  private online = new Map<number, OnlineEntry>(); // userId do servidor → sessão
  private recheckTimer: NodeJS.Timeout | null = null;
  private enabled: boolean;

  constructor(private log: Log, private pgConnectionString: string | null) {
    this.enabled = process.env.GAME_ACCESS_CONTROL === "true" && !!pgConnectionString;
  }

  isEnabled(): boolean {
    return this.enabled;
  }

  async initAsync(ctx: SystemContext): Promise<void> {
    this.ctx = ctx;
    if (!this.enabled) {
      this.log(
        this.pgConnectionString
          ? "AccessControl desligado (GAME_ACCESS_CONTROL != true)"
          : "AccessControl desligado (sem DATABASE_URL)"
      );
      return;
    }

    const pool = new Pool({ connectionString: this.pgConnectionString! });
    try {
      await pool.query("SELECT 1 FROM game_access_settings LIMIT 1");
    } catch (err: any) {
      // Schema do site ainda não aplicado (ou banco fora): fica desligado em
      // vez de recusar todo mundo por tabela inexistente.
      console.error(
        "[AccessControl] Tabelas de acesso não encontradas no banco do site; controle DESLIGADO. " +
        "Aplique docs/technical/alpha-access-control.sql e reinicie. Erro:", err?.message || err
      );
      this.enabled = false;
      pool.end().catch(() => undefined);
      return;
    }
    this.pool = pool;

    (ctx.svr as any).accessControl = this.gamemodeApi();
    this.listen();
    setInterval(() => this.scheduleRecheck(), RECHECK_INTERVAL_MS).unref?.();
    this.log("AccessControl ligado (whitelist/blocklist no banco do site)");
  }

  disconnect(userId: number): void {
    this.online.delete(userId);
  }

  /**
   * Chamado pelo Login depois de validar o token. Registra a sessão para a
   * reverificação e devolve a decisão. Qualquer erro nega: é controle de
   * acesso, falhar aberto deixaria entrar quem está bloqueado.
   */
  async checkLogin(userId: number, token: string): Promise<AccessDecision> {
    if (!this.pool) return { allowed: true };
    try {
      const rows = await this.pool.query(
        `SELECT s.user_id::text AS user_id, COALESCE(s.device_hashes, '{}') AS device_hashes
         FROM sessions s WHERE s.token = $1 AND s.expires_at > now()`,
        [token]
      );
      if (rows.rowCount === 0) return { allowed: false, reason: MESSAGES.unavailable };

      const siteUserId: string = rows.rows[0].user_id;
      const deviceHashes = normalizeDeviceHashes(rows.rows[0].device_hashes);
      const decision = await this.decideFor(siteUserId, deviceHashes);

      if (decision.allowed) {
        if (decision.bindDevices) await this.bindDevices(siteUserId, deviceHashes);
        this.online.set(userId, { siteUserId, deviceHashes });
        this.log(
          `AccessControl: userId ${userId} liberado (usuário do site ${siteUserId}, ` +
          `${deviceHashes.length} máquina(s)${decision.bindDevices ? ", vinculada agora" : ""})`
        );
      } else {
        this.log(`AccessControl: userId ${userId} recusado (usuário do site ${siteUserId}): ${decision.reason}`);
      }
      return decision;
    } catch (err) {
      console.error("[AccessControl] Falha ao verificar acesso no login:", err);
      return { allowed: false, reason: MESSAGES.unavailable };
    }
  }

  private async decideFor(siteUserId: string, deviceHashes: string[]): Promise<AccessDecision> {
    const pool = this.pool!;
    // Ban vale para a própria conta OU para qualquer conta banida que tenha
    // usado uma destas máquinas (ban_device_hashes). Ban vencido não conta,
    // mesmo com is_banned ainda true.
    const [blockRes, settingsRes, userRes] = await Promise.all([
      pool.query(
        `SELECT COALESCE(ban_reason, 'sem motivo informado') AS reason, ban_expires_at AS expires_at FROM users
         WHERE is_banned AND (ban_expires_at IS NULL OR ban_expires_at > now())
           AND (id::text = $1 OR (cardinality($2::text[]) > 0 AND ban_device_hashes && $2::text[]))
         ORDER BY (id::text = $1) DESC, ban_expires_at DESC NULLS FIRST LIMIT 1`,
        [siteUserId, deviceHashes]
      ),
      pool.query(`SELECT whitelist_enabled FROM game_access_settings LIMIT 1`),
      pool.query(`SELECT is_whitelisted, whitelist_device_hashes FROM users WHERE id::text = $1`, [siteUserId]),
    ]);

    const user = userRes.rows[0];
    return decideAccess({
      block: blockRes.rowCount
        ? { reason: blockRes.rows[0].reason, expiresAt: blockRes.rows[0].expires_at }
        : null,
      whitelistEnabled: settingsRes.rowCount ? settingsRes.rows[0].whitelist_enabled === true : false,
      whitelistEntry: user && user.is_whitelisted ? { deviceHashes: user.whitelist_device_hashes } : null,
      sessionDeviceHashes: deviceHashes,
    });
  }

  private async bindDevices(siteUserId: string, deviceHashes: string[]): Promise<void> {
    // Só se ainda não vinculado: se o site vinculou antes (no login do
    // launcher), não sobrescreve.
    await this.pool!.query(
      `UPDATE users SET whitelist_device_hashes = $2
       WHERE id::text = $1 AND is_whitelisted
         AND (whitelist_device_hashes IS NULL OR cardinality(whitelist_device_hashes) = 0)`,
      [siteUserId, deviceHashes]
    );
  }

  // ── expulsão de quem perdeu o acesso com o jogo aberto ──────────────────

  private listen(): void {
    const client = new Client({ connectionString: this.pgConnectionString! });
    const retry = () => {
      if (this.listenClient !== client) return;
      this.listenClient = null;
      client.end().catch(() => undefined);
      setTimeout(() => this.listen(), LISTEN_RETRY_MS);
    };
    this.listenClient = client;
    client.on("notification", (msg) => {
      this.log(`AccessControl: aviso do banco (${msg.payload || "?"}); reverificando ${this.online.size} jogador(es) online`);
      this.scheduleRecheck();
    });
    client.on("error", (err) => {
      console.error("[AccessControl] Conexão LISTEN caiu; reconectando:", err.message);
      retry();
    });
    client.connect()
      .then(() => client.query(`LISTEN ${NOTIFY_CHANNEL}`))
      .then(() => this.log(`AccessControl: escutando mudanças de acesso em tempo real (LISTEN ${NOTIFY_CHANNEL})`))
      .catch((err) => {
        console.error("[AccessControl] Falha no LISTEN; usando só a reverificação periódica:", err.message);
        retry();
      });
  }

  /** Agrupa rajadas de avisos (ex.: vários bloqueios seguidos) numa verificação só. */
  private scheduleRecheck(): void {
    if (this.recheckTimer) return;
    this.recheckTimer = setTimeout(() => {
      this.recheckTimer = null;
      this.recheckOnline().catch((err) => console.error("[AccessControl] Falha na reverificação:", err));
    }, RECHECK_DEBOUNCE_MS);
  }

  private async recheckOnline(): Promise<void> {
    if (!this.pool || !this.ctx) return;
    for (const [userId, entry] of [...this.online.entries()]) {
      let decision: AccessDecision;
      try {
        decision = await this.decideFor(entry.siteUserId, entry.deviceHashes);
      } catch (err) {
        // Banco fora do ar não expulsa quem já está jogando; só o login nega.
        console.error("[AccessControl] Reverificação indisponível:", err);
        return;
      }
      if (!decision.allowed) this.kick(userId, decision.reason || MESSAGES.notWhitelisted);
    }
  }

  private kick(userId: number, reason: string): void {
    const svr = this.ctx!.svr as any;
    this.online.delete(userId);
    this.log(`AccessControl: expulsando userId ${userId}: ${reason}`);
    try {
      // accessRevoked (não loginFailedBanned): o jogador já está dentro do
      // jogo, e a tela de falha de login só existe durante a autenticação.
      svr.sendCustomPacket(userId, JSON.stringify({ customPacketType: "accessRevoked", reason }));
    } catch { /* já desconectou */ }
    // Dá tempo de ler o aviso e confirmar; depois derruba de qualquer jeito.
    setTimeout(() => {
      try {
        if (svr.isConnected(userId)) svr.kick(userId);
      } catch { /* já desconectou */ }
    }, KICK_DELAY_MS);
  }

  // ── API para os comandos do gamemode (mp.accessControl) ─────────────────

  private gamemodeApi() {
    return {
      whitelistAdd: (target: string, by: Actor, note?: string) => this.whitelistAdd(target, by, note),
      whitelistRemove: (target: string, by: Actor) => this.whitelistRemove(target, by),
      whitelistResetDevice: (target: string, by: Actor) => this.whitelistResetDevice(target, by),
      whitelistList: () => this.whitelistList(),
      setWhitelistEnabled: (enabled: boolean, by: Actor) => this.setWhitelistEnabled(enabled, by),
      block: (target: string, by: Actor, reason: string, durationMs: number | null) =>
        this.block(target, by, reason, durationMs),
      unblock: (target: string, by: Actor) => this.unblock(target, by),
      blockList: () => this.blockList(),
      parseDuration,
    };
  }

  /** Alvo por id ou nome de usuário do site — as duas são chaves únicas. */
  private async resolveUser(target: string): Promise<SiteUser | null> {
    const value = target.trim();
    if (!value) return null;
    const res = await this.pool!.query(
      `SELECT id::text AS id, username, profile_id FROM users
       WHERE id::text = $1 OR lower(username) = lower($1) LIMIT 2`,
      [value]
    );
    if (res.rowCount !== 1) return null;
    const row = res.rows[0];
    return { id: row.id, username: row.username, profileId: Number(row.profile_id) };
  }

  /**
   * UUID do usuário do site de quem executou, pelo profile_id do staff — é o
   * que o site grava em whitelisted_by/banned_by. null se não resolver.
   */
  private async actorSiteId(by: Actor): Promise<string | null> {
    if (by.profileId === undefined) return null;
    try {
      const res = await this.pool!.query(`SELECT id::text AS id FROM users WHERE profile_id = $1`, [by.profileId]);
      return res.rowCount ? res.rows[0].id : null;
    } catch {
      return null;
    }
  }

  private async withUser(target: string, fn: (user: SiteUser) => Promise<CommandResult>): Promise<CommandResult> {
    if (!this.pool) return { ok: false, message: "Controle de acesso desligado neste servidor." };
    try {
      const user = await this.resolveUser(target);
      if (!user) return { ok: false, message: `Usuário '${target}' não encontrado (use o id ou o nome de usuário do site).` };
      return await fn(user);
    } catch (err: any) {
      console.error("[AccessControl] Falha no comando:", err);
      return { ok: false, message: `Erro ao acessar o banco do site: ${err?.message || err}` };
    }
  }

  // As listas são as colunas que o site já usa em `users` (is_whitelisted,
  // is_banned...). A nota do /whitelist add fica só no audit_logs do jogo: o
  // site não tem coluna para ela.

  private async whitelistAdd(target: string, by: Actor, _note?: string): Promise<CommandResult> {
    return this.withUser(target, async (user) => {
      const res = await this.pool!.query(
        `UPDATE users SET is_whitelisted = true, whitelisted_at = now(), whitelisted_by = $2
         WHERE id::text = $1 AND NOT is_whitelisted`,
        [user.id, await this.actorSiteId(by)]
      );
      return res.rowCount
        ? { ok: true, message: `${user.username} liberado para a alpha.` }
        : { ok: false, message: `${user.username} já está na whitelist.` };
    });
  }

  private async whitelistRemove(target: string, _by: Actor): Promise<CommandResult> {
    return this.withUser(target, async (user) => {
      const res = await this.pool!.query(
        `UPDATE users SET is_whitelisted = false, whitelist_device_hashes = NULL
         WHERE id::text = $1 AND is_whitelisted`,
        [user.id]
      );
      this.scheduleRecheck();
      return res.rowCount
        ? { ok: true, message: `${user.username} removido da whitelist.` }
        : { ok: false, message: `${user.username} não estava na whitelist.` };
    });
  }

  private async whitelistResetDevice(target: string, _by: Actor): Promise<CommandResult> {
    return this.withUser(target, async (user) => {
      const res = await this.pool!.query(
        `UPDATE users SET whitelist_device_hashes = NULL WHERE id::text = $1 AND is_whitelisted`,
        [user.id]
      );
      return res.rowCount
        ? { ok: true, message: `Máquina de ${user.username} liberada: será vinculada no próximo login.` }
        : { ok: false, message: `${user.username} não está na whitelist.` };
    });
  }

  private async whitelistList(): Promise<CommandResult> {
    if (!this.pool) return { ok: false, message: "Controle de acesso desligado neste servidor." };
    const [settings, rows] = await Promise.all([
      this.pool.query(`SELECT whitelist_enabled FROM game_access_settings LIMIT 1`),
      this.pool.query(
        `SELECT username, (whitelist_device_hashes IS NOT NULL) AS bound FROM users
         WHERE is_whitelisted ORDER BY whitelisted_at DESC NULLS LAST LIMIT 50`
      ),
    ]);
    const on = settings.rowCount ? settings.rows[0].whitelist_enabled : false;
    const list = rows.rows.map((r: any) => `${r.username}${r.bound ? "" : " (sem máquina)"}`).join(", ");
    return { ok: true, message: `Whitelist ${on ? "LIGADA" : "desligada"} — ${rows.rowCount} conta(s): ${list || "nenhuma"}` };
  }

  private async setWhitelistEnabled(enabled: boolean, _by: Actor): Promise<CommandResult> {
    if (!this.pool) return { ok: false, message: "Controle de acesso desligado neste servidor." };
    await this.pool.query(`UPDATE game_access_settings SET whitelist_enabled = $1, updated_at = now()`, [enabled]);
    this.scheduleRecheck();
    return { ok: true, message: `Whitelist ${enabled ? "LIGADA: só contas liberadas entram" : "desligada: qualquer conta não banida entra"}.` };
  }

  private async block(target: string, by: Actor, reason: string, durationMs: number | null): Promise<CommandResult> {
    return this.withUser(target, async (user) => {
      // Ban da máquina: todos os hashes que a conta já usou (sessões e
      // vínculo da whitelist). Outra conta na mesma máquina também é barrada.
      const res = await this.pool!.query(
        `UPDATE users u SET
           is_banned = true, ban_reason = $2, banned_at = now(), banned_by = $3,
           ban_expires_at = CASE WHEN $4::bigint IS NULL THEN NULL
                                 ELSE now() + ($4::bigint * interval '1 millisecond') END,
           ban_device_hashes = ARRAY(SELECT DISTINCT h FROM (
             SELECT unnest(s.device_hashes) AS h FROM sessions s WHERE s.user_id = u.id AND s.device_hashes IS NOT NULL
             UNION
             SELECT unnest(u.whitelist_device_hashes)
             UNION
             SELECT unnest(u.ban_device_hashes)
           ) t WHERE h IS NOT NULL)
         WHERE u.id::text = $1
         RETURNING cardinality(u.ban_device_hashes) AS devices, u.ban_expires_at AS expires_at`,
        [user.id, reason, await this.actorSiteId(by), durationMs]
      );
      // Sem sessão válida o launcher não entrega token novo nem para o jogo.
      await this.pool!.query(`DELETE FROM sessions WHERE user_id::text = $1`, [user.id]);
      this.scheduleRecheck();
      const row = res.rows[0] || {};
      const until = describeBlock({ reason, expiresAt: row.expires_at || null }).replace(/^Acesso bloqueado: /, "");
      return {
        ok: true,
        message: `${user.username} banido — ${until} ${row.devices || 0} adaptador(es) de rede bloqueado(s).`,
      };
    });
  }

  private async unblock(target: string, _by: Actor): Promise<CommandResult> {
    return this.withUser(target, async (user) => {
      // ban_reason/banned_at/banned_by ficam como registro do último ban; o
      // que desliga é is_banned, e as máquinas deixam de ser bloqueadas.
      const res = await this.pool!.query(
        `UPDATE users SET is_banned = false, ban_expires_at = NULL, ban_device_hashes = NULL
         WHERE id::text = $1 AND is_banned`,
        [user.id]
      );
      return res.rowCount
        ? { ok: true, message: `${user.username} desbanido.` }
        : { ok: false, message: `${user.username} não está banido.` };
    });
  }

  private async blockList(): Promise<CommandResult> {
    if (!this.pool) return { ok: false, message: "Controle de acesso desligado neste servidor." };
    const rows = await this.pool.query(
      `SELECT username, ban_reason, ban_expires_at FROM users
       WHERE is_banned AND (ban_expires_at IS NULL OR ban_expires_at > now())
       ORDER BY banned_at DESC NULLS LAST LIMIT 30`
    );
    const list = rows.rows
      .map((r: any) => `${r.username}: ${r.ban_reason || "sem motivo"}${r.ban_expires_at ? "" : " (perm)"}`)
      .join(" | ");
    return { ok: true, message: `${rows.rowCount} ban(s) ativo(s): ${list || "nenhum"}` };
  }
}
