
// TODO: send event instead of direct dependency on FormView class
import { FormView } from "../../view/formView";
import { QueryKeyCodeBindings } from "../events/queryKeyCodeBindings";

import { ClientListener, CombinedController, Sp } from "./clientListener";
import { BrowserMessageEvent, DxScanCode, Menu, MenuCloseEvent, MenuOpenEvent } from "skyrimPlatform";

const unfocusEventString = `window.dispatchEvent(new CustomEvent('skymp5-client:browserUnfocused', {}))`;
const focusEventString = `window.dispatchEvent(new CustomEvent('skymp5-client:browserFocused', {}))`;

export class BrowserService extends ClientListener {
  constructor(private sp: Sp, private controller: CombinedController) {
    super();

    this.sp.browser.setVisible(false);

    this.controller.emitter.on("queryKeyCodeBindings", (e) => this.onQueryKeyCodeBindings(e));
    this.controller.once("update", () => this.onceUpdate());
    this.controller.on("browserMessage", (e) => this.onBrowserMessage(e));
    this.controller.on("menuOpen", (e) => this.onMenuOpen(e));
    this.controller.on("menuClose", (e) => this.onMenuClose(e));
    // Native browser state remains available while menus suppress update.
    this.controller.on("tick", () => this.syncFocus());
  }

  // TODO: keycodes should be configurable
  private onQueryKeyCodeBindings(e: QueryKeyCodeBindings) {
    // Focused Meridian shortcuts arrive via the DOM bridge. Do not toggle
    // them a second time via the game's physical keyboard query.
    const backend = (this.sp.browser as any).getBackend?.();
    this.domOwnedKeys.forEach(key => {
      if (!e.isDown([key])) this.domOwnedKeys.delete(key);
    });
    if (backend?.name === "meridian" && this.sp.browser.isFocused()) return;
    const isDown = (key: DxScanCode) => !this.domOwnedKeys.has(key) && e.isDown([key]);
    if (isDown(DxScanCode.F1)) {
      FormView.isDisplayingNicknames = !FormView.isDisplayingNicknames;
    }
    if (isDown(DxScanCode.F2)) {
      this.sp.browser.setVisible(!this.sp.browser.isVisible());
    }
    if (this.badMenusOpen.size === 0 && isDown(DxScanCode.F6)) {
      const newState = !this.sp.browser.isFocused();
      this.sp.browser.setFocused(newState);
    }
    if (this.badMenusOpen.size === 0 && isDown(DxScanCode.Enter)) {
      this.sp.browser.setFocused(true);
    }
    if (isDown(DxScanCode.Escape)) {
      if (this.sp.browser.isFocused()) {
        this.sp.browser.setFocused(false);
      }
    }
  }

  private onceUpdate() {
    this.sp.browser.setVisible(true);
  }

  private onBrowserMessage(e: BrowserMessageEvent) {
    const onFrontLoadedEventKey = "front-loaded";

    if (e.arguments[0] === onFrontLoadedEventKey) {
      this.controller.emitter.emit("browserWindowLoaded", {});
    }
    if (e.arguments[0] === "aetherius::browser-shortcut") {
      const key = ({ F1: DxScanCode.F1, F2: DxScanCode.F2, F6: DxScanCode.F6, Escape: DxScanCode.Escape } as Record<string, DxScanCode>)[e.arguments[1] as string];
      if (key !== undefined) this.domOwnedKeys.add(key);
      switch (e.arguments[1]) {
        case "F1": FormView.isDisplayingNicknames = !FormView.isDisplayingNicknames; break;
        case "F2": this.sp.browser.setVisible(false); break;
        case "F6": case "Escape": this.sp.browser.setFocused(false); break;
      }
    }
  }

  private syncFocus() {
    const focused = this.sp.browser.isFocused();
    if (focused !== this.lastActualFocus) {
      this.lastActualFocus = focused;
      this.sp.browser.executeJavaScript(focused ? focusEventString : unfocusEventString);
    }
    const backend = (this.sp.browser as any).getBackend?.();
    const blocked = backend?.name === "meridian" && backend.focusRequested && !focused;
    if (!!blocked !== this.lastFocusBlocked) {
      this.lastFocusBlocked = !!blocked;
      this.sp.browser.executeJavaScript(`window.__aetheriusFocusUnavailable?.(${!!blocked});`);
    }
  }
  private lastActualFocus = false;
  private lastFocusBlocked = false;
  // Keep ownership until a keyboard query observes release, including when
  // the DOM shortcut relinquishes focus before the physical query arrives.
  private domOwnedKeys = new Set<DxScanCode>();

  private onMenuOpen(e: MenuOpenEvent) {
    if (this.isBadMenu(e.name)) {
      this.sp.browser.setVisible(false);
      this.badMenusOpen.add(e.name);
    } else if (e.name === Menu.HUD) {
      this.sp.browser.setVisible(true);
    }
  }

  private onMenuClose(e: MenuCloseEvent) {
    if (this.badMenusOpen.delete(e.name)) {
      if (this.badMenusOpen.size === 0) {
        this.sp.browser.setVisible(true);
      }
    }

    if (e.name === Menu.HUD) {
      this.sp.browser.setVisible(false);
    }
  }

  private isBadMenu(menu: string) {
    return this.badMenus.includes(menu as Menu);
  }

  private badMenusOpen = new Set<string>();

  private readonly badMenus: Menu[] = [
    Menu.Barter,
    Menu.Book,
    Menu.Container,
    Menu.Crafting,
    Menu.Gift,
    Menu.Inventory,
    Menu.Journal,
    Menu.Lockpicking,
    Menu.Loading,
    Menu.Map,
    Menu.RaceSex,
    Menu.Stats,
    Menu.Tween,
    Menu.Console,
    Menu.Main,
  ];
}
