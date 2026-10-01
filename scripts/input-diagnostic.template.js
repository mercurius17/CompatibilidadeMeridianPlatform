/* Local test plugin only. Records no credentials, text input, URLs or player data. */
(() => {
// The embedded runtime exposes this API as a global, not a Node package.
const sp = globalThis.skyrimPlatform;
const fs = require('fs');
const reportPath = __REPORT_PATH__;
const started = Date.now();
let lastSample = 0;
let installed = false;
let ticks = 0;
let updates = 0;
let records = 0;
const allowedEvents = ['pointerdown','pointerup','mousedown','mouseup','click','keydown'];
const allowedTargets = ['server-down-close', 'server-down-reconnect', 'access-revoked-confirm', 'other'];
function record(kind, detail) {
  if (Date.now() - started > 600000 || records >= 4000) return;
  records++;
  fs.appendFileSync(reportPath, JSON.stringify({ elapsedMs: Date.now() - started, kind, detail }) + '\n');
}
record('loaded', { version: 1 });
sp.on('update', () => { updates++; });
sp.on('browserMessage', event => {
  if (event.arguments[0] === 'aetherius::diagnostic') {
    const [type, target] = event.arguments.slice(1, 3);
    if ((allowedEvents.includes(type) && allowedTargets.includes(target)) ||
        (type === 'installed' && target === 'capture-listeners')) {
      record('page', [type, target]);
    }
  } else if (event.arguments[0] === 'front-loaded' || event.arguments[0] === 'cef::serverDown:close') {
    record('bridge-message', event.arguments[0]);
  }
});
sp.on('tick', () => {
  ticks++;
  if (Date.now() - started > 600000 || Date.now() - lastSample < 1000) return;
  lastSample = Date.now();
  const backend = sp.browser.getBackend();
  record('backend', {
    name: backend.name, ready: backend.ready, available: backend.available,
    generation: backend.generation, focusRequested: backend.focusRequested,
    focusResult: backend.focusResult, focused: sp.browser.isFocused(), visible: sp.browser.isVisible(), ticks, updates
  });
  if (backend.ready && !installed) {
    installed = true;
    sp.browser.executeJavaScript(`(function () {
      if (window.__aetheriusInputDiagnostic) return;
      window.__aetheriusInputDiagnostic = true;
      const allowedIds = ['server-down-close', 'server-down-reconnect', 'access-revoked-confirm'];
      function send(type, target) {
        window.skyrimPlatform.sendMessage('aetherius::diagnostic', type, target);
      }
      for (const type of ['pointerdown','pointerup','mousedown','mouseup','click','keydown']) {
        window.addEventListener(type, event => {
          const id = allowedIds.includes(event.target?.id) ? event.target.id : 'other';
          send(type, id);
        }, true);
      }
      send('installed', 'capture-listeners');
    })();`);
  }
});
})();
