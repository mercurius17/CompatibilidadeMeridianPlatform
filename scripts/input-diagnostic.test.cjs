const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');

test('diagnostic runs in the embedded global API without a Node skyrimPlatform package', () => {
  const reports = [];
  const handlers = new Map();
  const scripts = [];
  const sp = {
    on(name, callback) { handlers.set(name, callback); },
    browser: {
      getBackend: () => ({ name: 'meridian', ready: true, available: true, generation: 1 }),
      isFocused: () => true,
      isVisible: () => true,
      executeJavaScript: code => scripts.push(code),
    },
  };
  const context = vm.createContext({
    skyrimPlatform: sp,
    require(name) {
      assert.equal(name, 'fs', 'native API must come from the injected global');
      return { appendFileSync(file, line) {
        assert.equal(file, 'diagnostic.jsonl');
        reports.push(JSON.parse(line));
      } };
    },
  });
  const source = fs.readFileSync(path.join(__dirname, 'input-diagnostic.template.js'), 'utf8')
    .replace('__REPORT_PATH__', JSON.stringify('diagnostic.jsonl'));
  vm.runInContext(source, context);
  handlers.get('update')();
  handlers.get('browserMessage')({ arguments: ['cef::serverDown:close'] });
  handlers.get('tick')();
  assert.equal(reports[0].kind, 'loaded');
  assert.equal(reports[1].detail, 'cef::serverDown:close');
  assert.equal(reports[2].detail.ticks, 1);
  assert.equal(reports[2].detail.updates, 1);
  assert.equal(scripts.length, 1);
  const count = reports.length;
  handlers.get('browserMessage')({ arguments: ['aetherius::diagnostic', 'typed-text', 'must-not-be-recorded'] });
  handlers.get('browserMessage')({ arguments: ['aetherius::diagnostic', 'click', 'unrecognized-input-id'] });
  assert.equal(reports.length, count, 'diagnostic accepts only fixed event/target labels');
  handlers.get('browserMessage')({ arguments: ['aetherius::diagnostic', 'click', 'server-down-close', 'must-not-be-recorded'] });
  assert.deepEqual(reports.at(-1).detail, ['click', 'server-down-close']);
  // Separate plugins/reloads share a global environment: do not leak const bindings.
  assert.doesNotThrow(() => vm.runInContext(source, context));
});
