/* eslint-disable @typescript-eslint/no-explicit-any */
//
// Sinal de vida e desligamento limpo.
//
// A conexão do jogo (RakNet) só declara o servidor morto depois de 60s sem
// resposta (kTimeoutMs no MpClientPlugin): fechar ou derrubar o servidor
// deixava o jogador parado um minuto antes do pop-up de "conexão perdida".
// Baixar esse timeout exigiria recompilar a DLL do client e derrubaria gente
// em travadas longas. Em vez disso:
//
// - heartbeat: um pacote mínimo a cada 2s para cada jogador. O client
//   (ServerDownService) trata ~8s de silêncio como servidor fora.
// - Ctrl+C / SIGTERM: avisa todo mundo (serverShutdown) e desconecta antes de
//   sair, para o pop-up aparecer na hora num reinício planejado.

import { Log, System, SystemContext } from "./system";

const HEARTBEAT_INTERVAL_MS = 2000;
// Aviso primeiro, kick depois: kickar logo após o sendCustomPacket descartava
// o aviso ainda na fila (visto em teste: disconnect 42ms depois, sem pop-up).
const SHUTDOWN_NOTICE_MS = 800;
const SHUTDOWN_EXIT_MS = 1200;
const HEARTBEAT_PACKET = JSON.stringify({ customPacketType: "heartbeat" });
const SHUTDOWN_PACKET = JSON.stringify({ customPacketType: "serverShutdown" });

export class Heartbeat implements System {
  systemName = "Heartbeat";

  private users = new Set<number>();
  private ctx: SystemContext | null = null;
  private shuttingDown = false;

  constructor(private log: Log) { }

  async initAsync(ctx: SystemContext): Promise<void> {
    this.ctx = ctx;
    setInterval(() => this.beat(), HEARTBEAT_INTERVAL_MS).unref?.();
    // Assumir o SIGINT tira o encerramento padrão do Ctrl+C: quem sai agora
    // é o shutdown(), depois de avisar os jogadores.
    process.once("SIGINT", () => this.shutdown("SIGINT"));
    process.once("SIGTERM", () => this.shutdown("SIGTERM"));
  }

  connect(userId: number): void {
    this.users.add(userId);
  }

  disconnect(userId: number): void {
    this.users.delete(userId);
  }

  private beat(): void {
    if (!this.ctx || this.shuttingDown) return;
    const svr = this.ctx.svr as any;
    for (const userId of this.users) {
      try {
        if (svr.isConnected(userId)) svr.sendCustomPacket(userId, HEARTBEAT_PACKET);
      } catch { /* desconectou no meio */ }
    }
  }

  private shutdown(signal: string): void {
    if (this.shuttingDown) return;
    this.shuttingDown = true;
    this.log(`Heartbeat: ${signal} recebido; avisando ${this.users.size} jogador(es) antes de desligar`);
    const svr = this.ctx ? this.ctx.svr as any : null;
    const users = [...this.users];
    if (svr) {
      for (const userId of users) {
        try {
          svr.sendCustomPacket(userId, SHUTDOWN_PACKET);
        } catch { /* já desconectado */ }
      }
      setTimeout(() => {
        for (const userId of users) {
          try {
            if (svr.isConnected(userId)) svr.kick(userId);
          } catch { /* já desconectado */ }
        }
      }, SHUTDOWN_NOTICE_MS);
    }
    setTimeout(() => process.exit(0), SHUTDOWN_EXIT_MS);
  }
}
