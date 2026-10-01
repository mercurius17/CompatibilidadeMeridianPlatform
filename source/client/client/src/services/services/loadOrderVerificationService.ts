import { Game, Utility, printConsole, createText, setTextSize } from "skyrimPlatform";
import { getScreenResolution } from "../../view/formView";
import { ClientListener, CombinedController, Sp } from "./clientListener";
import { Mod } from "../messages_http/serverManifest";
import { logTrace } from "../../logging";
import { SettingsService } from "./settingsService";

const STATE_KEY = 'loadOrderCheckState';

interface State {
  statusTextId?: number;
};

interface ClientPlugin extends Mod {
  loadOrderKind: "regular" | "light";
  slotIndex: number;
}

export class LoadOrderVerificationService extends ClientListener {
  constructor(private sp: Sp, private controller: CombinedController) {
    super();
    this.controller.once("update", () => this.onceUpdate());
  }

  private onceUpdate() {
    this.verifyLoadOrder();
  }

  private verifyLoadOrder() {
    const settingsService = this.controller.lookupListener(SettingsService);

    this.resetText();
    const clientRegularMods = this.getClientRegularMods();
    const clientLightMods = this.getClientLightMods();

    this.printModOrder('Client regular load order:', clientRegularMods);
    this.printModOrder('Client light load order:', clientLightMods);

    return settingsService.getServerMods()
      .then((serverMods) => {
        this.printModOrder('Server load order:', serverMods);

        // Helper para determinar se mod do servidor é light
        const isServerModLight = (m: Mod): boolean => {
          if (typeof m.isLight === "boolean") {
            return m.isLight;
          }
          // Fallback seguro: se extensão for .esl
          return m.filename.toLowerCase().endsWith(".esl");
        };

        // Filtra BSAs caso estejam na lista
        const serverPluginMods = serverMods.filter(
          m => !m.filename.toLowerCase().endsWith(".bsa")
        );

        const serverRegularMods = serverPluginMods.filter(m => !isServerModLight(m));
        const serverLightMods = serverPluginMods.filter(m => isServerModLight(m));

        if (clientRegularMods.length < serverRegularMods.length) {
          throw new Error(
            `Missing some regular server mods. Server has ${serverRegularMods.length}, we have ${clientRegularMods.length}`
          );
        }
        if (clientLightMods.length < serverLightMods.length) {
          throw new Error(
            `Missing some light server mods (ESL/ESPFE). Server has ${serverLightMods.length}, we have ${clientLightMods.length}`
          );
        }

        const hasExtraMods =
          clientRegularMods.length > serverRegularMods.length ||
          clientLightMods.length > serverLightMods.length;

        if (hasExtraMods) {
          const enforceStrict = Boolean(
            this.sp.settings['skymp5-client']?.['enforceStrictLoadOrder']
          );
          if (enforceStrict) {
            throw new Error(
              `LOAD ORDER ERROR: you have extra mods not present on server, and enforceStrictLoadOrder is enabled!`
            );
          } else {
            this.updateText(
              'LOAD ORDER WARNING: you have more mods than server!\nCheck console for details.',
              [255, 255, 0, 1], 5,
            );
          }
        }

        const fails: string[] = [];

        // 1. Validação estrita da sequência de regular mods (ordem relativa, nome, tamanho, CRC32)
        for (let i = 0; i < serverRegularMods.length; ++i) {
          const sMod = serverRegularMods[i];
          const cMod = clientRegularMods[i];
          if (
            cMod.filename.toLowerCase() !== sMod.filename.toLowerCase() ||
            cMod.size !== sMod.size ||
            cMod.crc32 !== sMod.crc32
          ) {
            fails.push(`Regular mod #${i} (${sMod.filename}) mismatch`);
            printConsole(`Regular mod #${i} does not match.`);
            printConsole(`Server has: ${JSON.stringify(sMod)}`);
            printConsole(`Client has: ${JSON.stringify(cMod)}`);
          }
        }

        // 2. Validação estrita da sequência de light mods (ordem relativa, nome, tamanho, CRC32)
        for (let i = 0; i < serverLightMods.length; ++i) {
          const sMod = serverLightMods[i];
          const cMod = clientLightMods[i];
          if (
            cMod.filename.toLowerCase() !== sMod.filename.toLowerCase() ||
            cMod.size !== sMod.size ||
            cMod.crc32 !== sMod.crc32
          ) {
            fails.push(`Light mod #${i} (${sMod.filename}) mismatch`);
            printConsole(`Light mod #${i} does not match.`);
            printConsole(`Server has: ${JSON.stringify(sMod)}`);
            printConsole(`Client has: ${JSON.stringify(cMod)}`);
          }
        }

        if (fails.length !== 0) {
          throw new Error('Load order check failed! Issues: ' + JSON.stringify(fails));
        }

        printConsole('Load order verified successfully (regular and light mods match server).');
      })
      .catch((err) => {
        printConsole(err);
        if (this.sp.settings['skymp5-client']['ignoreLoadOrderMismatch']) {
          this.updateText(
            'LOAD ORDER ERROR!\nHowever, ignoring it because of ignoreLoadOrderMismatch being set.' +
            '\nExpect EVERYTHING BREAK, unless you know what you are doing.\nCheck console for details.' +
            '\nThis message will disappear after 30 seconds.',
            [255, 0, 0, 1], 30,
          );
          return;
        }
        this.updateText(
          'LOAD ORDER ERROR!\nCheck console for details.',
          [255, 0, 0, 1],
        );
      });
  };

  private getState(): State {
    if (typeof this.sp.storage[STATE_KEY] !== 'object') {
      return {};
    }
    return this.sp.storage[STATE_KEY] as State;
  };

  private setState(replacement: State) {
    const oldState = this.sp.storage[STATE_KEY] = this.getState();
    for (const [k, v] of Object.entries(replacement)) {
      (oldState as Record<string, any>)[k] = v;
    }
  };

  private resetText() {
    let { statusTextId } = this.getState();
    if (statusTextId) {
      this.sp.destroyText(statusTextId);
      statusTextId = undefined;
      this.setState({ statusTextId });
    }
  };

  private updateText(text: string, color: [number, number, number, number], clearDelay?: number) {
    const { width, height } = getScreenResolution();
    this.resetText();
    const statusTextId = createText(width / 2, height / 2, text, color);
    setTextSize(statusTextId, 0.5);
    this.setState({ statusTextId });
    if (clearDelay) {
      Utility.wait(clearDelay).then(() => this.resetText());
    }
  }

  private enumerateClientMods(
    getCount: () => number,
    getAt: (idx: number) => string,
    kind: "regular" | "light"
  ): ClientPlugin[] {
    const result: ClientPlugin[] = [];
    const count = typeof getCount === "function" ? getCount() : 0;
    for (let i = 0; i < count; ++i) {
      const filename = getAt(i);
      if (!filename || filename.trim() === "") {
        continue;
      }
      const { crc32, size } = this.getFileInfoSafe(filename);
      result.push({
        filename,
        crc32,
        size,
        loadOrderKind: kind,
        isLight: kind === "light",
        slotIndex: i,
      });
    }
    return result;
  }

  private getClientRegularMods(): ClientPlugin[] {
    return this.enumerateClientMods(Game.getModCount, Game.getModName, "regular");
  }

  private getClientLightMods(): ClientPlugin[] {
    return typeof Game.getLightModCount === "function"
      ? this.enumerateClientMods(Game.getLightModCount, Game.getLightModName, "light")
      : [];
  }

  private getClientMods(): ClientPlugin[] {
    return [...this.getClientRegularMods(), ...this.getClientLightMods()];
  };

  private printModOrder(header: string, order: Mod[]) {
    printConsole(header);
    for (const [i, mod] of Object.entries(order)) {
      printConsole(`#${i} ${JSON.stringify(mod)}`);
    }
  };

  private getFileInfoSafe(filename: string) {
    try {
      return this.sp.getFileInfo(filename);
    } catch (e) {
      const message = (e as Record<string, unknown>).message;

      if (typeof message === "string" && message.includes('is not a valid argument')) {
        logTrace(this, `Failed to get file info for`, filename);
        return { crc32: 0, size: 0 };
      } else {
        throw e;
      }
    }
  }
}
