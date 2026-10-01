import { ClientListener, CombinedController, Sp } from "./clientListener";

const TIMESCALE_FORM_ID = 0x3a;
const GAME_HOUR_FORM_ID = 0x38;
const GAME_DAY_FORM_ID = 0x37;
const GAME_MONTH_FORM_ID = 0x36;
const GAME_YEAR_FORM_ID = 0x35;

const TIMESCALE = 12;
const DEFAULT_SPAWN_HOUR = 8;

// Referência fixa e igual pra todo mundo — não importa quando cada jogador
// conectou, essa conta sempre dá o mesmo resultado pra todo cliente no mesmo
// instante real, porque parte só do relógio real (Date.now()), nunca de
// "há quanto tempo esse cliente está aberto".
const EPOCH_MS = Date.UTC(2020, 0, 1, 0, 0, 0);

export class TimeService extends ClientListener {
    constructor(private sp: Sp, private controller: CombinedController) {
        super();
        controller.once("update", () => this.setInitialTimescale());
        controller.on("update", () => this.onUpdate());
    }

    /**
     * Usado por remoteServer.ts ao dar spawn no personagem. Propositalmente
     * NÃO chama nada do motor do jogo aqui (Game.getFormEx etc.) — esse
     * método roda no meio do processo de spawn, antes do jogo estar pronto
     * pra esse tipo de chamada, e fazer isso já quebrou o carregamento do
     * personagem uma vez. A sincronia de verdade acontece depois, no loop
     * periódico (ver onUpdate/every2seconds), que roda em segurança.
     */
    public getTime() {
        return { newGameHourValue: DEFAULT_SPAWN_HOUR };
    }

    /**
     * Calcula o dia/hora "acelerado 12x", só a partir do relógio real —
     * mesma conta em qualquer cliente, no mesmo instante, não importa quando
     * cada um conectou. É isso que mantém todo mundo vendo o mesmo dia/noite.
     */
    private getAcceleratedTime() {
        const elapsedMs = Date.now() - EPOCH_MS;
        const elapsedGameHours = (elapsedMs / (1000 * 60 * 60)) * TIMESCALE;

        const hourOfDay = elapsedGameHours % 24;
        const totalGameDays = Math.floor(elapsedGameHours / 24);

        // Meses "de 30 dias" só pra ter uma progressão de calendário simples;
        // não precisa bater com o calendário real de Tamriel, só precisa ser
        // consistente entre clientes.
        const dayOfMonth = (totalGameDays % 30) + 1;
        const totalMonths = Math.floor(totalGameDays / 30);
        const month = totalMonths % 12;
        const year = 199 + Math.floor(totalMonths / 12);

        return { hourOfDay, dayOfMonth, month, year };
    }

    private every2seconds() {
        const gameHour = this.sp.GlobalVariable.from(this.sp.Game.getFormEx(GAME_HOUR_FORM_ID));
        const gameDay = this.sp.GlobalVariable.from(this.sp.Game.getFormEx(GAME_DAY_FORM_ID));
        const gameMonth = this.sp.GlobalVariable.from(this.sp.Game.getFormEx(GAME_MONTH_FORM_ID));
        const gameYear = this.sp.GlobalVariable.from(this.sp.Game.getFormEx(GAME_YEAR_FORM_ID));

        if (!gameHour || !gameDay || !gameMonth || !gameYear) {
            return;
        }

        const { hourOfDay, dayOfMonth, month, year } = this.getAcceleratedTime();

        const diff = Math.abs(gameHour.getValue() - hourOfDay);

        // Só corrige quando o desvio já é grande (>= 1 minuto de jogo) — o
        // timescale=12 nativo já avança a hora sozinho o resto do tempo;
        // isso aqui só evita o cliente derivar/desviar dos outros com o tempo.
        if (diff >= 1 / 60) {
            gameHour.setValue(hourOfDay);
            gameDay.setValue(dayOfMonth);
            gameMonth.setValue(month);
            gameYear.setValue(year);
        }
    }

    private onUpdate() {
        if (Date.now() - this.lastTimeUpd <= 2000) {
            return;
        }
        this.lastTimeUpd = Date.now();
        this.every2seconds();
    }

    private setInitialTimescale() {
        const timeScale = this.sp.GlobalVariable.from(this.sp.Game.getFormEx(TIMESCALE_FORM_ID));
        if (!timeScale) {
            return;
        }
        // Fixo, sem o hack de 0.6/1.2 pra "alcançar" — como já calculamos
        // acelerado 12x na fonte, não tem o que alcançar, só corrigir desvio
        // pequeno de vez em quando (feito em every2seconds).
        timeScale.setValue(TIMESCALE);
    }

    private lastTimeUpd = 0;
}