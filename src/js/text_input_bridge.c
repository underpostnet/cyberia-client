#include "text_input_bridge.h"

#include <emscripten/emscripten.h>

EM_JS(void, text_input_bridge_init, (void), {
    const input = document.createElement('input');
    input.type = 'text';
    input.autocomplete = 'off';
    input.enterKeyHint = 'send';
    input.hidden = true;
    /* 16px keeps iOS from zooming the page on focus. */
    input.style.cssText = 'position:fixed;z-index:10;box-sizing:border-box;margin:0;padding:0 8px;' +
        'font:16px monospace;color:#c8c8d7;background:rgba(18,18,36,0.88);' +
        'border:1px solid rgba(50,50,88,0.6);border-radius:4px;outline:none';
    document.body.appendChild(input);
    Module.textInput = {element: input, submitted: false};
    /* Keys typed into the input never reach GLFW. Stopping the event at the
     * window also stops it at the input, so Enter and Escape are read here. */
    const guard = function(event) {
        if (document.activeElement !== input) return;
        event.stopImmediatePropagation();
        if ('keydown' !== event.type || event.isComposing) return;
        if ('Enter' === event.key) {
            event.preventDefault();
            Module.textInput.submitted = true;
        } else if ('Escape' === event.key) {
            input.blur();
        }
    };
    window.addEventListener('keydown', guard, true);
    window.addEventListener('keypress', guard, true);
})

EM_JS(void, text_input_bridge_show, (int x, int y, int width, int height, int max_len, const char* placeholder), {
    const input = Module.textInput.element;
    input.style.left = x + 'px';
    input.style.top = y + 'px';
    input.style.width = width + 'px';
    input.style.height = height + 'px';
    input.maxLength = max_len;
    input.placeholder = UTF8ToString(placeholder);
    input.hidden = false;
})

EM_JS(void, text_input_bridge_hide, (void), {
    const input = Module.textInput.element;
    input.blur();
    input.hidden = true;
    Module.textInput.submitted = false;
})

EM_JS(bool, text_input_bridge_submitted, (void), {
    const submitted = Module.textInput.submitted;
    Module.textInput.submitted = false;
    return submitted;
})

EM_JS(size_t, text_input_bridge_take, (char* out, size_t cap), {
    const input = Module.textInput.element;
    const length = stringToUTF8(input.value, out, cap);
    input.value = "";
    return length;
})
