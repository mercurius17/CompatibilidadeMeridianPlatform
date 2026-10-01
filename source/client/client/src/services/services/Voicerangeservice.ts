import { Sp, CombinedController, ClientListener } from "./clientListener";

const VOICE_RANGE_KEY = 49;

export class VoiceRangeService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();

        this.controller.emitter.on("queryKeyCodeBindings", (e: any) => {
            const isDown: boolean = e.isDown([VOICE_RANGE_KEY]);

            if (isDown && !this.wasDown) {
                try {
                    this.sp.browser.executeJavaScript(
                        'window.cycleVoiceRange && window.cycleVoiceRange();'
                    );
                } catch (err) {
                    // sem canal de log confiável aqui
                }
            }
            this.wasDown = isDown;
        });
    }

    private wasDown = false;
}