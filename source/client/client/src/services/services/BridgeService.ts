import { MsgType } from "../../messages";
import { CustomPacketMessage } from "../messages/customPacketMessage";
import { BrowserMessageEvent } from "skyrimPlatform";

import { ClientListener, CombinedController, Sp } from "./clientListener";

export class BridgeService extends ClientListener {
  private voiceHelperPath: string;
  private voiceHelperProcess: any = null;

  constructor(private sp: Sp, private controller: CombinedController) {
    super();
    this.controller.on("browserMessage", (e) => this.onBrowserMessage(e));
    this.controller.emitter.on("customPacketMessage", (e: any) => this.onCustomPacketMessage(e));

    const proc = (globalThis as any).process;
    const skyrimPath = (typeof proc !== 'undefined' && typeof proc.cwd === 'function')
      ? proc.cwd()
      : null;

    this.voiceHelperPath = skyrimPath
      ? `${skyrimPath}\\Data\\Voice-helper\\voice-helper.exe`
      : '';
  }

  private onBrowserMessage(e: BrowserMessageEvent) {
    const args = e.arguments;
    if (args[0] !== "cef::ui:event") {
      return;
    }

    const type = args[1];
    const data = args[2];

    const message: CustomPacketMessage = {
      t: MsgType.CustomPacket,
      contentJsonDump: JSON.stringify({ type, data }),
    };

    this.controller.emitter.emit("sendMessage", {
      message: message,
      reliability: "reliable",
    });
  }

  /**
   * Segurar-pra-falar, chamado pelo VoipToggleService ao apertar (true) e
   * soltar (false) a tecla V.
   *
   * O helper (falar) é aberto pelo servidor no spawn e fica aberto a sessão
   * inteira, transmitindo sempre; a tecla só diz ao servidor se deve repassar.
   * Abrir/fechar o helper a cada fala era o modelo antigo (alternar), e além
   * de cortar 1-2s do começo de cada frase, apertar de novo antes do helper
   * subir abria um segundo processo e invertia o estado liga/desliga.
   *
   * Se o helper não estiver rodando (caiu, ou o pedido do spawn se perdeu),
   * apertar V pede outro — essa primeira fala perde o começo, as seguintes não.
   */
  public setPushToTalk(talking: boolean) {
    if (talking && !this.isHelperRunning()) {
      this.requestHelper();
    }
    try {
      this.sp.browser.executeJavaScript(
        `window.setPushToTalk && window.setPushToTalk(${talking ? 'true' : 'false'});`
      );
    } catch (e) {
      // sem canal de log confiável aqui
    }
  }

  /**
   * Checkbox "Supressão de ruído" do launcher, que chega como variável de
   * ambiente do usuário (mesmo caminho do AETHERIUS_TOKEN). Ligada por padrão:
   * só "0" desliga, então ausência da variável (launcher antigo) mantém ligada.
   */
  private isNoiseSuppressionEnabled(): boolean {
    const proc = (globalThis as any).process;
    const value = (typeof proc !== 'undefined' && proc.env)
      ? proc.env['AETHERIUS_NOISE_SUPPRESSION']
      : undefined;
    return String(value ?? '1').trim() !== '0';
  }

  private isHelperRunning(): boolean {
    const child = this.voiceHelperProcess;
    return !!child && child.exitCode == null && !child.killed;
  }

  // Um pedido (/voz) por vez: o helper leva alguns segundos pra chegar, e
  // repetir o pedido nesse meio emitiria outro ticket e outro processo.
  private helperRequestedAt = 0;
  private static readonly HELPER_REQUEST_TIMEOUT_MS = 10000;

  private requestHelper() {
    const now = Date.now();
    if (now - this.helperRequestedAt < BridgeService.HELPER_REQUEST_TIMEOUT_MS) {
      return;
    }
    this.helperRequestedAt = now;

    const message: CustomPacketMessage = {
      t: MsgType.CustomPacket,
      contentJsonDump: JSON.stringify({ type: 'cef::chat:send', data: '/voz' }),
    };

    this.controller.emitter.emit("sendMessage", {
      message: message,
      reliability: "reliable",
    });
  }

  /**
   * O helper é spawnado `detached`, então sobrevive ao jogo: fechar o Skyrim
   * (ou ele crashar) deixava o voice-helper.exe da sessão anterior rodando,
   * com o microfone aberto. Fecha qualquer um que tenha sobrado antes de
   * abrir o desta sessão.
   */
  private orphansKilled = false;

  private killOrphanHelpers(cp: any) {
    if (this.orphansKilled) return;
    this.orphansKilled = true;
    try {
      cp.execSync('taskkill /F /IM voice-helper.exe', { stdio: 'ignore' });
    } catch (e) {
      // código != 0 quando não há nenhum rodando — o caso normal
    }
  }

  private onCustomPacketMessage(event: any): void {
    const msg = event.message;

    let msgContent: Record<string, unknown> = {};
    try {
      msgContent = JSON.parse(msg.contentJsonDump);
    } catch (e) {
      return;
    }

    if (msgContent["customPacketType"] !== "voipAutoConnect") {
      return;
    }

    if (!this.voiceHelperPath) {
      return;
    }

    const actorIdHex = msgContent["actorIdHex"] as string;
    const ticket = msgContent["ticket"] as string;
    const host = msgContent["host"] as string;
    const port = msgContent["port"] as number;

    const req = (globalThis as any).require;
    if (typeof req !== 'function') return;

    let cp: any;
    let fs: any;
    try {
      cp = req('child_process');
      fs = req('fs');
    } catch (e) {
      return;
    }

    this.killOrphanHelpers(cp);

    // Um ticket novo substitui o helper atual (o servidor também troca o
    // slot de sender) — sem isto, dois helpers transmitiriam ao mesmo tempo.
    if (this.isHelperRunning()) {
      try {
        this.voiceHelperProcess.kill();
      } catch (e) {
        // já pode ter morrido sozinho
      }
    }
    this.voiceHelperProcess = null;

    try {
      const outFd = fs.openSync('voice-helper-stdout.log', 'a');
      const errFd = fs.openSync('voice-helper-stderr.log', 'a');

      const args = [
        '--actor-id', actorIdHex,
        '--ticket', ticket,
        '--host', host,
        '--port', String(port),
        // O helper manda Opus por padrão; o voip-service e o index.html deste
        // servidor só tocam PCM s16le. Tirar isto exige decodificar Opus na UI.
        '--pcm',
      ];
      if (!this.isNoiseSuppressionEnabled()) {
        args.push('--no-denoise');
      }
      // O helper é detached e sobreviveria ao jogo; com o PID do jogo (o Node
      // do SkyrimPlatform roda dentro do próprio SkyrimSE.exe) ele encerra
      // sozinho quando o jogo fecha ou crasha.
      const gamePid = (globalThis as any).process?.pid;
      if (typeof gamePid === 'number' && gamePid > 0) {
        args.push('--parent-pid', String(gamePid));
      }

      const child = cp.spawn(
        this.voiceHelperPath,
        args,
        { detached: true, stdio: ['ignore', outFd, errFd] }
      );

      child.unref();
      this.voiceHelperProcess = child;
      this.helperRequestedAt = 0;

      // Helper caiu sozinho: esquece a referência, e o próximo V pede outro.
      // Antes, o estado continuava "ligado" e o V seguinte só desligava algo
      // que já estava morto — parecia que a tecla não funcionava.
      if (typeof child.on === 'function') {
        child.on('exit', () => {
          if (this.voiceHelperProcess === child) this.voiceHelperProcess = null;
        });
      }
    } catch (e) {
      // sem canal de log confiável aqui
    }
  }
}