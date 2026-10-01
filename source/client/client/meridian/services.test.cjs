const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const ts = require('typescript');
const FormView = { isDisplayingNicknames: false };
class ClientListener {}
class BridgeService {}
const Menu = { HUD: 'HUDMenu', Main: 'MainMenu', Inventory: 'InventoryMenu' };
const DxScanCode = { F1: 1, F2: 2, F6: 6, Enter: 28, Escape: 1 };
function load(file, name) {
  const source = fs.readFileSync(path.join(__dirname, '../src/services/services', file), 'utf8');
  const js = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 } }).outputText;
  const exports = {};
  vm.runInNewContext(js, { exports, require(name) {
    if (name === '../../view/formView') return { FormView };
    if (name === './clientListener') return { ClientListener };
    if (name === './BridgeService') return { BridgeService };
    if (name === 'skyrimPlatform') return { Menu, DxScanCode };
    throw new Error('Unexpected dependency: ' + name);
  } });
  return exports[name];
}
function context() {
  const handlers = new Map();
  const emissions = [];
  const scripts = [];
  const requests = [];
  let focus = false;
  let focusRequested = false;
  let visible = false;
  const controller = {
    on: (name, cb) => handlers.set(name, cb),
    once: (name, cb) => handlers.set('once:' + name, cb),
    emitter: { on: (name, cb) => handlers.set(name, cb), emit: (name, value) => emissions.push({ name, value }) },
    lookupListener: () => ({ setPushToTalk: state => requests.push(['ptt', state]) }),
  };
  const sp = { browser: {
    setVisible: state => { visible = state; requests.push(['visible', state]); },
    isVisible: () => visible,
    setFocused: state => { focusRequested = state; requests.push(['focus', state]); },
    isFocused: () => focus,
    executeJavaScript: code => scripts.push(code),
    getBackend: () => ({ name: 'meridian', focusRequested }),
  } };
  return { sp, controller, handlers, emissions, scripts, requests, grantFocus(state) { focus = state; } };
}
test('BrowserService reports actual focus, not an asynchronous request', () => {
  const BrowserService = load('browserService.ts', 'BrowserService');
  const c = context(); new BrowserService(c.sp, c.controller);
  c.handlers.get('queryKeyCodeBindings')({ isDown: keys => keys.includes(DxScanCode.F6) });
  c.handlers.get('update')();
  assert.equal(c.scripts.some(s => s.includes("CustomEvent('skymp5-client:browserFocused'")), false);
  assert.ok(c.scripts.some(s => s.includes('__aetheriusFocusUnavailable?.(true)')));
  c.grantFocus(true); c.handlers.get('update')();
  assert.equal(c.scripts.filter(s => s.includes("CustomEvent('skymp5-client:browserFocused'")).length, 1);
  c.handlers.get('update')();
  assert.equal(c.scripts.filter(s => s.includes("CustomEvent('skymp5-client:browserFocused'")).length, 1);
  c.grantFocus(false); c.handlers.get('update')();
  assert.ok(c.scripts.some(s => s.includes('browserUnfocused')));
});
test('browserMessage contract forwards readiness and focused close shortcuts', () => {
  const BrowserService = load('browserService.ts', 'BrowserService');
  const c = context(); new BrowserService(c.sp, c.controller);
  c.handlers.get('browserMessage')({ arguments: ['front-loaded'] });
  assert.equal(c.emissions.at(-1).name, 'browserWindowLoaded');
  c.handlers.get('browserMessage')({ arguments: ['aetherius::browser-shortcut', 'Escape'] });
  assert.deepEqual(c.requests.at(-1), ['focus', false]);
  c.handlers.get('browserMessage')({ arguments: ['aetherius::browser-shortcut', 'F2'] });
  assert.deepEqual(c.requests.at(-1), ['visible', false]);
});
test('PTT typing guard releases V on browser focus and resumes outside it', () => {
  const VoipToggleService = load('Voiptoggleservice.ts', 'VoipToggleService');
  const c = context(); new VoipToggleService(c.sp, c.controller);
  const poll = () => c.handlers.get('queryKeyCodeBindings')({ isDown: () => true });
  poll(); assert.deepEqual(c.requests.at(-1), ['ptt', true]);
  c.grantFocus(true); poll(); assert.deepEqual(c.requests.at(-1), ['ptt', false]);
  const count = c.requests.length; poll(); assert.equal(c.requests.length, count);
  c.grantFocus(false); poll(); assert.deepEqual(c.requests.at(-1), ['ptt', true]);
});
