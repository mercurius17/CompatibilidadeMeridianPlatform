/* The form contract consumed by the existing AuthService. GPL-3.0. */
(function (root) {
  'use strict';
  const platform = root.skyrimPlatform;
  platform.widgets = {
    set(widgets) {
      const container = root.document.getElementById('aetherius-widgets');
      if (!container) throw new Error('Aetherius widget container missing');
      if (!Array.isArray(widgets)) throw new TypeError('widgets.set expects an array');
      const fragment = root.document.createDocumentFragment();
      for (const widget of widgets) {
        if (widget.type !== 'form') throw new TypeError('Unsupported widget type: ' + widget.type);
        const panel = root.document.createElement('section');
        panel.className = 'aetherius-form';
        panel.setAttribute('role', 'dialog');
        const title = root.document.createElement('h1');
        title.textContent = widget.caption || '';
        panel.appendChild(title);
        for (const element of widget.elements || []) {
          let node;
          switch (element.type) {
            case 'text': case 'icon':
              node = root.document.createElement('p');
              node.textContent = element.text || '';
              break;
            case 'inputText': case 'inputPass':
              node = root.document.createElement('input');
              node.type = element.type === 'inputPass' ? 'password' : 'text';
              node.placeholder = element.placeholder || '';
              node.setAttribute('aria-label', node.placeholder || element.type);
              node.autocomplete = element.type === 'inputPass' ? 'current-password' : 'username';
              node.addEventListener('input', () => element.change?.(node.value));
              break;
            case 'button':
              node = root.document.createElement('button');
              node.type = 'button';
              node.textContent = element.text || '';
              node.title = element.hint || '';
              node.addEventListener('click', () => element.click?.());
              break;
            default: throw new TypeError('Unsupported form element: ' + element.type);
          }
          panel.appendChild(node);
        }
        fragment.appendChild(panel);
      }
      // Build before replacing: a malformed update must not clear a valid form.
      container.replaceChildren(fragment);
    }
  };
})(window);
