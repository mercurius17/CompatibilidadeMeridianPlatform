const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
function setup() {
  function element(tag) {
    return { tag, children: [], attributes: {}, listeners: {},
      appendChild(child) { this.children.push(child); },
      setAttribute(key, value) { this.attributes[key] = value; },
      addEventListener(key, callback) { this.listeners[key] = callback; },
      replaceChildren(fragment) { this.children = fragment.children; } };
  }
  const container = element('div');
  const window = { skyrimPlatform: {}, document: {
    getElementById: () => container,
    createDocumentFragment: () => element('fragment'),
    createElement: element,
  } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, 'widgets.js'), 'utf8'), { window });
  return { window, container };
}
test('form/input/password/button contract preserves callbacks without HTML evaluation', () => {
  const p = setup(); const inputs = []; let clicks = 0;
  p.window.skyrimPlatform.widgets.set([{ type: 'form', caption: '<img onerror=evil>', elements: [
    { type: 'text', text: '<script>evil</script>' },
    { type: 'icon', text: 'Usuário' },
    { type: 'inputText', placeholder: 'Nome', change: value => inputs.push(value) },
    { type: 'inputPass', placeholder: 'Senha', change: value => inputs.push(value) },
    { type: 'button', text: 'Entrar', click: () => clicks++ },
  ] }]);
  const children = p.container.children[0].children;
  assert.equal(children[0].textContent, '<img onerror=evil>');
  assert.equal(children[1].textContent, '<script>evil</script>');
  children[3].value = 'João'; children[3].listeners.input();
  children[4].value = 'test-only'; children[4].listeners.input();
  assert.equal(children[4].type, 'password');
  assert.deepEqual(inputs, ['João', 'test-only']);
  children[5].listeners.click(); assert.equal(clicks, 1);
});
test('invalid widget updates cannot clear a previously valid form; empty array clears it', () => {
  const p = setup();
  p.window.skyrimPlatform.widgets.set([{ type: 'form', elements: [] }]);
  const old = p.container.children;
  assert.throws(() => p.window.skyrimPlatform.widgets.set([{ type: 'unknown' }]), /Unsupported/);
  assert.equal(p.container.children, old);
  assert.throws(() => p.window.skyrimPlatform.widgets.set([{ type: 'form', elements: [{ type: 'unknown' }] }]), /Unsupported/);
  assert.equal(p.container.children, old);
  p.window.skyrimPlatform.widgets.set([]);
  assert.equal(p.container.children.length, 0);
});
