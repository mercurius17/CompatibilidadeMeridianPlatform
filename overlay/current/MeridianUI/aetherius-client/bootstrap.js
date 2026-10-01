/* Aetherius transport compatibility for Meridian.View/1. GPL-3.0. */
(function (root) {
  'use strict';
  const MAX_MESSAGES = 256;
  const MAX_BYTES = 1024 * 1024;
  const MAX_ARGUMENTS = 64;
  let generation = null;
  let queued = [];
  let queuedBytes = 0;
  let attached = false;
  let frontLoaded = false;
  let disposed = false;
  const platform = root.skyrimPlatform || (root.skyrimPlatform = {});

  function byteLength(value) {
    return new TextEncoder().encode(value).byteLength;
  }
  function send(args) {
    if (disposed) throw new Error('Meridian bridge is closed');
    if (args.length > MAX_ARGUMENTS) throw new RangeError('Too many bridge arguments');
    // Snapshot now, preserving JSON types and ordering; reject unsupported
    // values instead of silently altering the authentication/packet contract.
    const json = JSON.stringify(args, function (key, value) {
      if (value === undefined || typeof value === 'function' || typeof value === 'symbol' ||
          (typeof value === 'number' && !Number.isFinite(value))) {
        throw new TypeError('Unsupported bridge value');
      }
      return value;
    });
    const bytes = byteLength(json);
    if (bytes > MAX_BYTES - 128) throw new RangeError('Bridge message too large');
    if (attached) {
      root.aetheriusNativeMessage(JSON.stringify({ generation, arguments: JSON.parse(json) }));
      return;
    }
    if (queued.length >= MAX_MESSAGES || bytes > MAX_BYTES - queuedBytes) {
      throw new RangeError('Meridian pre-ready message queue full');
    }
    queued.push(json);
    queuedBytes += bytes;
  }
  platform.sendMessage = function (...args) { send(args); };
  root.__aetheriusFocusUnavailable = function (blocked) {
    const hint = root.document.getElementById('aetherius-focus-hint');
    if (hint) hint.hidden = !blocked;
  };

  function maybeFrontLoaded() {
    if (attached && !frontLoaded && root.document.readyState !== 'loading') {
      frontLoaded = true;
      send(['front-loaded']);
    }
  }
  root.__aetheriusAttachMeridian = function (nextGeneration) {
    if (disposed || !Number.isSafeInteger(nextGeneration) || nextGeneration < 1 ||
        typeof root.aetheriusNativeMessage !== 'function') return false;
    if (attached) return generation === nextGeneration; // Never duplicate readiness.
    generation = nextGeneration;
    attached = true;
    send(['__aetherius::bridge-ready']);
    const pending = queued;
    queued = [];
    queuedBytes = 0;
    for (const json of pending) {
      root.aetheriusNativeMessage(JSON.stringify({ generation, arguments: JSON.parse(json) }));
    }
    maybeFrontLoaded();
    return true;
  };
  root.document.addEventListener('DOMContentLoaded', maybeFrontLoaded, { once: true });
  root.addEventListener('pagehide', () => {
    disposed = true;
    attached = false;
    queued = [];
    queuedBytes = 0;
  }, { once: true });

  // Meridian consumes focused key batches before Skyrim's sinks. Forward only
  // client shortcuts here; do not install a second native keyboard interceptor.
  root.addEventListener('keydown', (event) => {
    if (!attached || event.repeat || event.defaultPrevented) return;
    if (!['Escape', 'F1', 'F2', 'F6'].includes(event.key)) return;
    const critical = root.document.querySelector('#access-revoked.visible, #server-down.visible, #aetherius-widgets:not(:empty)');
    if (critical && (event.key === 'Escape' || event.key === 'F2' || event.key === 'F6')) return;
    event.preventDefault();
    send(['aetherius::browser-shortcut', event.key]);
  });
})(window);
