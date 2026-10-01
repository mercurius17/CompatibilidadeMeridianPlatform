const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const ts = require('typescript');
class ClientListener {}
class NetworkingService {}
class AccessRevokedService {}
function load(file, name) {
  const source = fs.readFileSync(path.join(__dirname, '../src/services/services', file), 'utf8');
  const js = ts.transpileModule(source, { compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2020 } }).outputText;
  const exports = {};
  vm.runInNewContext(js, { exports, require(name) {
    if (name === './clientListener') return { ClientListener };
    if (name === './networkingService') return { NetworkingService };
    if (name === './Accessrevokedservice') return { AccessRevokedService };
    throw new Error('Unexpected dependency: ' + name);
  } });
  return exports[name];
}
function context() {
  const handlers = new Map();
  const calls = [];
  const networking = {
    close: () => calls.push('network-close'),
    reconnect: () => calls.push('reconnect'),
    setAutoReconnect: value => calls.push(['auto-reconnect', value]),
  };
  const controller = {
    on: (name, cb) => handlers.set(name, cb),
    emitter: { on: (name, cb) => handlers.set(name, cb) },
    lookupListener: type => type === NetworkingService ? networking : { isShowing: () => false },
  };
  const sp = {
    browser: { executeJavaScript: () => {}, setVisible: () => {}, setFocused: () => {} },
    win32: { exitProcess: () => calls.push('quit') },
    Debug: { quitGame: () => { throw new Error('Papyrus is unavailable in a tick-only menu'); } },
  };
  return { handlers, calls, controller, sp };
}
test('disconnect popup closes on the next tick while gameplay updates are absent', () => {
  const Service = load('Serverdownservice.ts', 'ServerDownService');
  const c = context(); new Service(c.sp, c.controller);
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:close'] });
  assert.deepEqual(c.calls, [], 'browser callback only schedules the action');
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['quit']);
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['quit'], 'one close request is processed once');
});
test('disconnect popup reconnects once on tick without a gameplay update', () => {
  const Service = load('Serverdownservice.ts', 'ServerDownService');
  const c = context(); new Service(c.sp, c.controller);
  c.handlers.get('connectionAccepted')();
  c.handlers.get('connectionDisconnect')();
  c.calls.length = 0;
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:reconnect'] });
  assert.deepEqual(c.calls, []);
  c.handlers.get('tick')();
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['reconnect']);
});
test('access-revoked confirmation closes on tick in a menu', () => {
  const Service = load('Accessrevokedservice.ts', 'AccessRevokedService');
  const c = context(); new Service(c.sp, c.controller);
  c.handlers.get('browserMessage')({ arguments: ['cef::accessRevoked:confirm'] });
  assert.deepEqual(c.calls, []);
  assert.ok(c.handlers.has('tick'), 'the menu continues ticking');
  c.handlers.get('tick')();
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['quit']);
});
test('access-revoked UI failure can request normal shutdown without Papyrus', () => {
  const Service = load('Accessrevokedservice.ts', 'AccessRevokedService');
  const c = context(); new Service(c.sp, c.controller);
  c.sp.browser.executeJavaScript = () => { throw new Error('provider absent'); };
  c.handlers.get('customPacketMessage')({ message: { contentJsonDump: JSON.stringify({ customPacketType: 'accessRevoked', reason: 'test-only' }) } });
  assert.deepEqual(c.calls, []);
  assert.ok(c.handlers.has('tick'));
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['quit']);
});

test('close cancels a queued reconnect and makes shutdown terminal', () => {
  const Service = load('Serverdownservice.ts', 'ServerDownService');
  const c = context(); new Service(c.sp, c.controller);
  c.handlers.get('connectionAccepted')();
  c.handlers.get('connectionDisconnect')();
  c.calls.length = 0;
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:reconnect'] });
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:close'] });
  c.handlers.get('tick')();
  c.handlers.get('connectionAccepted')();
  c.handlers.get('connectionDisconnect')();
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:close'] });
  c.handlers.get('tick')();
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, ['quit']);
});

test('connection accepted cancels a reconnect queued before its tick', () => {
  const Service = load('Serverdownservice.ts', 'ServerDownService');
  const c = context(); new Service(c.sp, c.controller);
  c.handlers.get('connectionAccepted')();
  c.handlers.get('connectionDisconnect')();
  c.handlers.get('browserMessage')({ arguments: ['cef::serverDown:reconnect'] });
  c.handlers.get('connectionAccepted')();
  c.calls.length = 0;
  c.handlers.get('tick')();
  assert.deepEqual(c.calls, []);
});

test('repeated revoked confirmation cannot queue another shutdown', () => {
  const Service = load('Accessrevokedservice.ts', 'AccessRevokedService');
  const c = context(); new Service(c.sp, c.controller);
  for (let i = 0; i < 3; ++i) {
    c.handlers.get('browserMessage')({ arguments: ['cef::accessRevoked:confirm'] });
    c.handlers.get('tick')();
  }
  assert.deepEqual(c.calls, ['quit']);
});
