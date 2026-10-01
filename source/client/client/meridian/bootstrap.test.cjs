const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, 'bootstrap.js'), 'utf8');

function page(state = 'loading') {
  const events = new Map();
  const docEvents = new Map();
  const sent = [];
  const window = {
    document: {
      readyState: state,
      addEventListener: (name, callback) => docEvents.set(name, callback),
      querySelector: () => null,
      getElementById: () => null,
    },
    addEventListener: (name, callback) => events.set(name, callback),
    aetheriusNativeMessage: (value) => sent.push(JSON.parse(value)),
  };
  vm.runInNewContext(source, { window, TextEncoder });
  return { window, sent, events, docEvents };
}

test('queued messages snapshot JSON values and preserve argument order/Unicode', () => {
  const p = page();
  const object = { nome: 'João', nested: [null, true, 4.5] };
  p.window.skyrimPlatform.sendMessage('cef::ui:event', false, 1, object, ['á']);
  object.nome = 'changed';
  assert.equal(p.sent.length, 0);
  assert.equal(p.window.__aetheriusAttachMeridian(3), true);
  assert.deepEqual(p.sent, [
    { generation: 3, arguments: ['__aetherius::bridge-ready'] },
    { generation: 3, arguments: ['cef::ui:event', false, 1, { nome: 'João', nested: [null, true, 4.5] }, ['á']] },
  ]);
});
test('front-loaded follows DOM readiness, once, after bridge-ready', () => {
  const p = page();
  p.window.__aetheriusAttachMeridian(1);
  p.window.document.readyState = 'complete';
  p.docEvents.get('DOMContentLoaded')();
  p.window.__aetheriusAttachMeridian(1);
  p.docEvents.get('DOMContentLoaded')();
  assert.deepEqual(p.sent.map(m => m.arguments[0]), ['__aetherius::bridge-ready', 'front-loaded']);
});
test('DOMContentLoaded before attachment still sends front-loaded exactly once', () => {
  const p = page('complete');
  p.window.__aetheriusAttachMeridian(8);
  p.window.__aetheriusAttachMeridian(8);
  assert.equal(p.sent.filter(m => m.arguments[0] === 'front-loaded').length, 1);
});
test('invalid/replaced generation cannot attach or relabel old work', () => {
  const p = page();
  assert.equal(p.window.__aetheriusAttachMeridian(0), false);
  assert.equal(p.window.__aetheriusAttachMeridian(1.5), false);
  p.window.__aetheriusAttachMeridian(1);
  assert.equal(p.window.__aetheriusAttachMeridian(2), false);
  p.window.skyrimPlatform.sendMessage('x');
  assert.equal(p.sent.at(-1).generation, 1);
});
test('provider missing does not drop queued messages', () => {
  const p = page();
  delete p.window.aetheriusNativeMessage;
  p.window.skyrimPlatform.sendMessage('before-ready');
  assert.equal(p.window.__aetheriusAttachMeridian(1), false);
  p.window.aetheriusNativeMessage = json => p.sent.push(JSON.parse(json));
  p.window.__aetheriusAttachMeridian(1);
  assert.equal(p.sent.at(-1).arguments[0], 'before-ready');
});
test('queue count, UTF-8 byte and argument limits are enforced', () => {
  const p = page();
  for (let i = 0; i < 256; i++) p.window.skyrimPlatform.sendMessage(i);
  assert.throws(() => p.window.skyrimPlatform.sendMessage(257), /queue full/);
  p.window.__aetheriusAttachMeridian(1);
  assert.throws(() => p.window.skyrimPlatform.sendMessage('é'.repeat(1024 * 1024)), /too large/);
  assert.throws(() => p.window.skyrimPlatform.sendMessage(...Array(65).fill(null)), /Too many/);
});
test('unsupported/cyclic values are explicit errors, not changed wire types', () => {
  const p = page();
  for (const value of [undefined, Infinity, NaN, () => {}, Symbol('x')]) {
    assert.throws(() => p.window.skyrimPlatform.sendMessage(value), /Unsupported/);
  }
  const circular = {}; circular.x = circular;
  assert.throws(() => p.window.skyrimPlatform.sendMessage(circular), /circular/i);
});
test('pagehide cancels queued work and rejects subsequent sends', () => {
  const p = page();
  p.window.skyrimPlatform.sendMessage('old');
  p.events.get('pagehide')();
  assert.equal(p.window.__aetheriusAttachMeridian(1), false);
  assert.throws(() => p.window.skyrimPlatform.sendMessage('new'), /closed/);
  assert.equal(p.sent.length, 0);
});
test('focused shortcuts forward once; modal close and repeated keys are respected', () => {
  const p = page();
  p.window.__aetheriusAttachMeridian(1);
  const key = { key: 'Escape', preventDefault() { this.prevented = true; } };
  p.events.get('keydown')(key);
  assert.deepEqual(p.sent.at(-1).arguments, ['aetherius::browser-shortcut', 'Escape']);
  assert.equal(key.prevented, true);
  const count = p.sent.length;
  p.window.document.querySelector = () => ({});
  p.events.get('keydown')({ ...key, prevented: false });
  p.events.get('keydown')({ ...key, key: 'F1', repeat: true });
  assert.equal(p.sent.length, count);
});
