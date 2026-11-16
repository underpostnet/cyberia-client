#include "pointer_bridge.h"

#include <emscripten/emscripten.h>
#include <stddef.h>

_Static_assert(20 == sizeof(pointer_event_t), "the drain writes five 32-bit words per event");
_Static_assert(12 == offsetof(pointer_event_t, x), "x is the fourth word");

/* The kinds are the pointer_event_kind_e values: 0 down, 1 move, 2 up, 3
 * cancel. `live` holds the pointers that are down in the browser; `held` holds
 * the pointers C has a press for. Past 256 waiting events the queue resets: it
 * cancels each pointer C holds and forgets the rest, so no press stays held. */
EM_JS(int, pointer_bridge_drain, (pointer_event_t* out, int max), {
    var bridge = Module.cyberiaPointers;
    if (!bridge) {
        var canvas = Module.canvas;
        bridge = Module.cyberiaPointers = { queue: [], moves: new Map(), live: new Set(), held: new Set() };
        var cancel = function(id) {
            bridge.live.delete(id);
            bridge.moves.delete(id);
            bridge.queue.push({ kind: 3, id: id, touch: 0, x: 0, y: 0 });
        };
        var cancel_all = function() { Array.from(bridge.live).forEach(cancel); };
        var push = function(kind, e) {
            if (256 <= bridge.queue.length) {
                bridge.queue = [];
                bridge.moves.clear();
                bridge.live.clear();
                bridge.held.forEach(function(id) { bridge.queue.push({ kind: 3, id: id, touch: 0, x: 0, y: 0 }); });
                return false;
            }
            var rect  = canvas.getBoundingClientRect();
            var event = { kind: kind, id: e.pointerId, touch: 'mouse' === e.pointerType ? 0 : 1,
                          x: e.clientX - rect.left, y: e.clientY - rect.top };
            if (1 === kind && bridge.moves.has(e.pointerId)) {
                bridge.queue[bridge.moves.get(e.pointerId)] = event;
                return true;
            }
            if (1 === kind) bridge.moves.set(e.pointerId, bridge.queue.length);
            else bridge.moves.delete(e.pointerId);
            bridge.queue.push(event);
            return true;
        };
        var options = { capture: true, passive: true };
        canvas.addEventListener('pointerdown', function(e) {
            if ('mouse' === e.pointerType && 0 !== e.button) return;
            if (bridge.live.has(e.pointerId)) cancel(e.pointerId);
            if (!push(0, e)) return;
            bridge.live.add(e.pointerId);
            canvas.setPointerCapture(e.pointerId);
        }, options);
        canvas.addEventListener('pointermove', function(e) {
            if (!bridge.live.has(e.pointerId)) return;
            if (2 === e.button && 0 !== (e.buttons & 2)) cancel(e.pointerId);
            else push(1, e);
        }, options);
        canvas.addEventListener('pointerup', function(e) {
            if (!bridge.live.has(e.pointerId) || ('mouse' === e.pointerType && 0 !== e.button)) return;
            bridge.live.delete(e.pointerId);
            push(2, e);
        }, options);
        var lost = function(e) { if (bridge.live.has(e.pointerId)) cancel(e.pointerId); };
        canvas.addEventListener('pointercancel', lost, options);
        canvas.addEventListener('lostpointercapture', lost, options);
        window.addEventListener('blur', cancel_all);
        document.addEventListener('visibilitychange', function() { if (document.hidden) cancel_all(); });
    }
    var count = Math.min(max, bridge.queue.length);
    for (var i = 0; i < count; i++) {
        var event = bridge.queue[i];
        var word  = (out >> 2) + 5 * i;
        HEAP32[word]      = event.kind;
        HEAP32[word + 1]  = event.id;
        HEAP32[word + 2]  = event.touch;
        HEAPF32[word + 3] = event.x;
        HEAPF32[word + 4] = event.y;
        if (0 === event.kind) bridge.held.add(event.id);
        else if (1 !== event.kind) bridge.held.delete(event.id);
    }
    bridge.queue.splice(0, count);
    bridge.moves.clear();
    return count;
})
