import { Sp, CombinedController, ClientListener } from "./clientListener";
import { BridgeService } from "./BridgeService";

// Scan code DirectInput da tecla V.
const VOIP_SHORTCUT_KEY = 47;

// Segurar-pra-falar: fala enquanto V estiver pressionada. Só as bordas
// (apertou/soltou) viram mensagem — o polling roda a cada frame.
export class VoipToggleService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();

        this.controller.emitter.on("queryKeyCodeBindings", (e: any) => {
            // A focused Meridian view owns text input. Typing V must not open
            // the microphone; acquiring focus releases an ongoing PTT press.
            const isDown: boolean = !this.sp.browser.isFocused() && e.isDown([VOIP_SHORTCUT_KEY]);

            if (isDown !== this.wasDown) {
                this.controller.lookupListener(BridgeService).setPushToTalk(isDown);
            }
            this.wasDown = isDown;
        });
    }

    private wasDown = false;
}
