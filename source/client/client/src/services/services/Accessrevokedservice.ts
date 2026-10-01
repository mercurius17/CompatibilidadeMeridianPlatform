import { BrowserMessageEvent } from "skyrimPlatform";
import { Sp, CombinedController, ClientListener } from "./clientListener";

// Acesso encerrado com o jogo aberto (ban, saiu da whitelist, outra máquina):
// o servidor manda `accessRevoked` com o motivo, a UI mostra um pop-up e, quando
// a pessoa clica em Confirmar, o jogo fecha. O servidor derruba a conexão ~30s
// depois de qualquer jeito, para quem não clicar.
const CONFIRM_MESSAGE = "cef::accessRevoked:confirm";

export class AccessRevokedService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();
        this.controller.emitter.on("customPacketMessage", (e: any) => this.onCustomPacketMessage(e));
        this.controller.on("browserMessage", (e) => this.onBrowserMessage(e));
        this.controller.on("update", () => this.onUpdate());
    }

    private onCustomPacketMessage(event: any): void {
        let content: Record<string, unknown> = {};
        try {
            content = JSON.parse(event.message.contentJsonDump);
        } catch (e) {
            return;
        }
        if (content["customPacketType"] !== "accessRevoked") return;

        const reason = typeof content["reason"] === "string" && content["reason"]
            ? content["reason"] as string
            : "Seu acesso ao servidor foi encerrado.";
        this.showing = true;
        try {
            this.sp.browser.executeJavaScript(
                `window.showAccessRevoked && window.showAccessRevoked(${JSON.stringify(reason)});`
            );
            // Cursor livre e UI por cima do jogo, para dar para clicar no botão.
            this.sp.browser.setVisible(true);
            this.sp.browser.setFocused(true);
        } catch (e) {
            // Sem UI não há o que confirmar: fecha direto.
            this.quitRequested = true;
        }
    }

    private onBrowserMessage(e: BrowserMessageEvent): void {
        if (e.arguments[0] === CONFIRM_MESSAGE) this.quitRequested = true;
    }

    // Fecha no "update", não dentro do evento do navegador: funções do Papyrus
    // (Debug.QuitGame) não podem ser chamadas desse contexto.
    private onUpdate(): void {
        if (!this.quitRequested) return;
        this.quitRequested = false;
        this.sp.Debug.quitGame();
    }

    /** O ServerDownService não mostra "conexão perdida" por cima deste aviso. */
    isShowing(): boolean {
        return this.showing;
    }

    private quitRequested = false;
    private showing = false;
}
