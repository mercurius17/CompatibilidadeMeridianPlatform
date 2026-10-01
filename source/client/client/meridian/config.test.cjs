const { test } = require('node:test');
const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');

test('packaged Meridian configuration does not require a development folder', () => {
  const ini = readFileSync(path.join(__dirname, 'SkyrimPlatform.ini'), 'utf8');
  assert.match(ini, /^\[Main\]$/m);
  assert.match(ini, /^PluginFolders=Data\/Platform\/Plugins\r?$/m);
  assert.match(ini, /^BackendName=meridian\r?$/m);
  assert.doesNotMatch(ini, /^PluginFolders=.*PluginsDev/m);
});
