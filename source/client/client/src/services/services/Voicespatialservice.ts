import { Sp, CombinedController, ClientListener } from "./clientListener";

// Som espacial do chat de voz: a página (index.html) posiciona cada voz em
// volta do jogador, e para isso precisa saber onde ele está e para onde olha.
// O servidor manda a posição de quem fala a cada 1s; a do próprio jogador vem
// daqui, a cada 100ms, para virar o personagem mudar o lado da voz na hora.
//
// Usa o ângulo do PERSONAGEM (getAngleZ): a SkyrimPlatform não expõe o da
// câmera. Em primeira pessoa e andando em terceira eles coincidem; girando só
// a câmera parado em terceira pessoa, o som acompanha o personagem.
const INTERVAL_MS = 100;
const MIN_MOVE = 5;      // unidades do Skyrim
const MIN_TURN_DEG = 1;

export class VoiceSpatialService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();
        this.controller.on("update", () => this.onUpdate());
    }

    private onUpdate() {
        const now = Date.now();
        if (now - this.lastSentAt < INTERVAL_MS) return;
        this.lastSentAt = now;

        const player = this.sp.Game.getPlayer();
        if (!player) return;

        const x = player.getPositionX();
        const y = player.getPositionY();
        const z = player.getPositionZ();
        const angle = player.getAngleZ();

        // Só manda quando mudou: executeJavaScript a 10Hz parado seria ruído.
        const last = this.last;
        if (last) {
            const moved = Math.hypot(x - last[0], y - last[1], z - last[2]) >= MIN_MOVE;
            const turn = Math.abs(((angle - last[3] + 540) % 360) - 180);
            if (!moved && turn < MIN_TURN_DEG) return;
        }
        this.last = [x, y, z, angle];

        try {
            this.sp.browser.executeJavaScript(
                `window.setListenerPose && window.setListenerPose(${x.toFixed(1)}, ${y.toFixed(1)}, ${z.toFixed(1)}, ${angle.toFixed(1)});`
            );
        } catch (e) {
            // sem canal de log confiável aqui
        }
    }

    private lastSentAt = 0;
    private last: [number, number, number, number] | null = null;
}
