/**
 * access-admin.js
 * Comandos de staff para a whitelist e a blocklist da alpha.
 *
 * As listas moram no banco do SITE (Postgres), não no MySQL do gamemode: site
 * e jogo gerenciam a mesma lista. Quem fala com esse banco é o sistema
 * AccessControl do servidor (server/ts/systems/accessControl.ts), exposto aqui
 * como `mp.accessControl`. Este módulo só confere permissão, chama, audita e
 * responde. Ver docs/technical/ALPHA_ACCESS_CONTROL.md.
 *
 * O alvo é sempre o id OU o nome de usuário do site — as duas são chaves
 * únicas, e funcionam com a pessoa offline (o actorId não).
 */
const commands = require('./commands');
const admin = require('./admin-service');
const moderationLog = require('./core/moderation-log');

const USAGE_WHITELIST =
  'Uso: /whitelist add <id|usuario> [nota] | remove <id|usuario> | trocarpc <id|usuario> | lista | ligar | desligar';
const USAGE_BAN = 'Uso: /ban <id|usuario> <30m|12h|3d|2w|perm> <motivo>';

function api() {
  return typeof mp !== 'undefined' ? mp.accessControl : undefined;
}

/** Quem executou: profile_id (vira o usuário do site no registro) + personagem. */
function actorOf(actorId) {
  const data = commands.getActiveCharacterData(actorId);
  const name = data ? `${data.firstName || ''} ${data.lastName || ''}`.trim() : '';
  return {
    by: { profileId: data?.profileId, label: `jogo:${name || '0x' + actorId.toString(16)}` },
    accountId: data?.accountId ?? null,
    name: name || `0x${actorId.toString(16)}`
  };
}

/**
 * Roda uma operação do AccessControl e responde ao staff. Erros viram
 * mensagem, nunca exceção solta no handler do chat.
 */
async function run(actorId, permission, auditAction, auditDetails, operation, onSuccess) {
  if (!admin.hasPermission(actorId, permission)) {
    commands.sendNotification(actorId, '[Staff] Permissão negada.');
    return;
  }
  const access = api();
  if (!access) {
    commands.sendNotification(actorId, '[Staff] Controle de acesso desligado neste servidor (GAME_ACCESS_CONTROL).');
    return;
  }
  const who = actorOf(actorId);
  try {
    const result = await operation(access, who.by);
    commands.sendNotification(actorId, `[Staff] ${result.message}`);
    if (result.ok) {
      await admin.auditLog(who.accountId, null, auditAction, `role=${admin.getRole(actorId)} ${auditDetails}`);
      if (onSuccess) onSuccess(who);
    }
  } catch (err) {
    console.error(`[access-admin] ${auditAction} falhou:`, err);
    commands.sendNotification(actorId, '[Staff] Erro ao executar o comando. Veja o log do servidor.');
  }
}

function handleWhitelist(actorId, args) {
  const [sub, target, ...rest] = (args || '').trim().split(/\s+/);
  const note = rest.join(' ');

  switch ((sub || '').toLowerCase()) {
    case 'add':
    case 'adicionar':
      if (!target) return commands.sendNotification(actorId, USAGE_WHITELIST);
      return run(actorId, 'manage_whitelist', 'staff:whitelist_add', `target=${target}`,
        (a, by) => a.whitelistAdd(target, by, note),
        (who) => moderationLog.notify({ kind: 'whitelist_approve', target, moderator: who.name, reason: note || undefined }));

    case 'remove':
    case 'remover':
      if (!target) return commands.sendNotification(actorId, USAGE_WHITELIST);
      return run(actorId, 'manage_whitelist', 'staff:whitelist_remove', `target=${target}`,
        (a, by) => a.whitelistRemove(target, by),
        (who) => moderationLog.notify({ kind: 'whitelist_reject', target, moderator: who.name }));

    case 'trocarpc':
    case 'resetpc':
      if (!target) return commands.sendNotification(actorId, USAGE_WHITELIST);
      return run(actorId, 'manage_whitelist', 'staff:whitelist_reset_device', `target=${target}`,
        (a, by) => a.whitelistResetDevice(target, by),
        (who) => moderationLog.notify({ kind: 'whitelist_reset', target, moderator: who.name }));

    case 'lista':
    case 'list':
      return run(actorId, 'manage_whitelist', 'staff:whitelist_list', '', (a) => a.whitelistList());

    // Ligar/desligar muda quem pode entrar no servidor inteiro: exige 'ban'
    // (admin+), não só 'manage_whitelist' (moderador).
    case 'ligar':
    case 'on':
      return run(actorId, 'ban', 'staff:whitelist_enable', '', (a, by) => a.setWhitelistEnabled(true, by));
    case 'desligar':
    case 'off':
      return run(actorId, 'ban', 'staff:whitelist_disable', '', (a, by) => a.setWhitelistEnabled(false, by));

    default:
      return commands.sendNotification(actorId, USAGE_WHITELIST);
  }
}

function handleBan(actorId, args) {
  const [target, durationText, ...rest] = (args || '').trim().split(/\s+/);
  const reason = rest.join(' ').trim();
  const access = api();
  if (!target || !durationText || !reason) return commands.sendNotification(actorId, USAGE_BAN);

  const durationMs = access ? access.parseDuration(durationText) : undefined;
  if (access && durationMs === undefined) {
    return commands.sendNotification(actorId, `Duração inválida '${durationText}'. ${USAGE_BAN}`);
  }
  return run(actorId, 'ban', 'staff:ban', `target=${target} duration=${durationText} reason=${reason}`,
    (a, by) => a.block(target, by, reason, durationMs),
    (who) => moderationLog.notify({ kind: 'ban', target, moderator: who.name, reason: `${reason} (${durationText})` }));
}

function handleUnban(actorId, args) {
  const target = (args || '').trim();
  if (!target) return commands.sendNotification(actorId, 'Uso: /unban <id|usuario>');
  return run(actorId, 'ban', 'staff:unban', `target=${target}`, (a, by) => a.unblock(target, by));
}

function handleBanList(actorId) {
  return run(actorId, 'ban', 'staff:banlist', '', (a) => a.blockList());
}

module.exports = { handleWhitelist, handleBan, handleUnban, handleBanList };
