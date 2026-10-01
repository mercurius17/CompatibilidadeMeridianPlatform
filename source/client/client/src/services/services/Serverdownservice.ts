import { BrowserMessageEvent } from "skyrimPlatform";
import { Sp, CombinedController, ClientListener } from "./clientListener";
import { NetworkingService } from "./networkingService";
import { AccessRevokedService } from "./Accessrevokedservice";

// Servidor caiu com o jogador dentro: em vez do loop silencioso de reconexão,
// mostra um pop-up com "Reconectar" e "Fechar". Reconectar faz UMA tentativa;
// se o servidor voltar, o login refaz sozinho (authService, como antes) e o
// pop-up some; se não, a pessoa decide de novo.
const RECONNECT_MESSAGE = "cef::serverDown:reconnect";
const CLOSE_MESSAGE = "cef::serverDown:close";

// A conexão só declara o servidor morto após 60s (timeout do MpClientPlugin).
// O servidor manda `heartbeat` a cada 2s: 8s de silêncio já é servidor fora.
// Só vigia depois do primeiro heartbeat — servidor antigo, sem heartbeat,
// continua dependendo do timeout da conexão.
const SILENCE_LIMIT_MS = 8000;
// Se o próprio jogo travou (tela de carregamento, alt-tab) por mais que isto,
// a contagem recomeça: o silêncio foi nosso, não do servidor.
const FROZEN_GAP_MS = 2000;

type PopupState = "lost" | "connecting" | "failed";

export class ServerDownService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();
        this.controller.emitter.on("connectionAccepted", () => this.onConnectionAccepted());
        this.controller.emitter.on("connectionDisconnect", () => this.onConnectionDisconnect());
        this.controller.emitter.on("connectionFailed", () => this.onConnectionUnavailable());
        this.controller.emitter.on("connectionDenied", () => this.onConnectionUnavailable());
        this.controller.emitter.on("customPacketMessage", (e: any) => this.onCustomPacketMessage(e));
        this.controller.on("browserMessage", (e) => this.onBrowserMessage(e));
        // "tick" roda também com menu aberto, diferente de "update".
        this.controller.on("tick", () => {
            this.processPendingActions();
            this.checkSilence();
        });
    }

    private onCustomPacketMessage(event: any): void {
        if (this.closing || this.quitRequested) return;
        let type: unknown;
        try {
            type = JSON.parse(event.message.contentJsonDump)["customPacketType"];
        } catch (e) {
            return;
        }
        if (type === "heartbeat") {
            this.heardHeartbeat = true;
            this.lastHeardAt = Date.now();
        } else if (type === "serverShutdown") {
            this.goDown();
        }
    }

    private checkSilence(): void {
        if (this.closing || this.quitRequested) return;
        const now = Date.now();
        if (this.lastCheckAt && now - this.lastCheckAt > FROZEN_GAP_MS) {
            this.lastHeardAt = now;
        }
        this.lastCheckAt = now;

        if (!this.heardHeartbeat || !this.everConnected || this.popupShown) return;
        if (now - this.lastHeardAt > SILENCE_LIMIT_MS) this.goDown();
    }

    private onConnectionAccepted(): void {
        if (this.closing || this.quitRequested) return;
        this.reconnectRequested = false;
        this.everConnected = true;
        this.lastHeardAt = Date.now();
        if (!this.popupShown) return;
        this.popupShown = false;
        this.reconnecting = false;
        this.networking().setAutoReconnect(true);
        this.runInBrowser("window.hideServerDown && window.hideServerDown();");
        try {
            this.sp.browser.setFocused(false);
        } catch (e) { /* sem UI */ }
    }

    private onConnectionDisconnect(): void {
        this.goDown();
    }

    /**
     * Servidor fora: queda detectada pela conexão, silêncio do heartbeat ou
     * aviso de desligamento. Fecha o cliente de rede (a próxima tentativa
     * começa do zero) e mostra o pop-up.
     */
    private goDown(): void {
        // Antes do primeiro login quem cuida é a tela de login; ban tem o
        // pop-up próprio; fechando o jogo não há o que mostrar.
        if (!this.everConnected || this.closing || this.quitRequested || this.popupShown) return;
        if (this.controller.lookupListener(AccessRevokedService).isShowing()) return;

        const networking = this.networking();
        networking.setAutoReconnect(false);
        try {
            networking.close();
        } catch (e) { /* já fechado */ }
        this.popupShown = true;
        this.reconnecting = false;
        this.show("lost");
    }

    private onConnectionUnavailable(): void {
        if (this.closing || this.quitRequested) return;
        if (!this.popupShown || !this.reconnecting) return;
        this.reconnecting = false;
        this.show("failed");
    }

    private onBrowserMessage(e: BrowserMessageEvent): void {
        if (this.closing || this.quitRequested) return;
        const kind = e.arguments[0];
        if (kind === RECONNECT_MESSAGE && this.popupShown && !this.reconnecting) {
            this.reconnectRequested = true;
        } else if (kind === CLOSE_MESSAGE) {
            this.reconnectRequested = false;
            this.quitRequested = true;
        }
    }

    // Processa fora do callback do navegador, mesmo no menu principal. A API
    // win32.exitProcess solicita a saída normal via Main::quitGame; não depende
    // do update/Papyrus e não termina o processo à força.
    private processPendingActions(): void {
        if (this.closing) return;
        if (this.quitRequested) {
            this.closing = true;
            this.reconnectRequested = false;
            this.quitRequested = false;
            this.sp.win32.exitProcess();
            return;
        }
        if (this.reconnectRequested) {
            this.reconnectRequested = false;
            this.reconnecting = true;
            this.show("connecting");
            this.networking().reconnect();
        }
    }

    private show(state: PopupState): void {
        this.runInBrowser(`window.showServerDown && window.showServerDown(${JSON.stringify(state)});`);
        try {
            this.sp.browser.setVisible(true);
            this.sp.browser.setFocused(true);
        } catch (e) { /* sem UI */ }
    }

    private runInBrowser(code: string): void {
        try {
            this.sp.browser.executeJavaScript(code);
        } catch (e) {
            // sem canal de log confiável aqui
        }
    }

    private networking(): NetworkingService {
        return this.controller.lookupListener(NetworkingService);
    }

    private heardHeartbeat = false;
    private lastHeardAt = 0;
    private lastCheckAt = 0;
    private everConnected = false;
    private popupShown = false;
    private reconnecting = false;
    private reconnectRequested = false;
    private quitRequested = false;
    private closing = false;
}
