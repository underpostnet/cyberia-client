#ifndef CYBERIA_UI_DISPATCH_H
#define CYBERIA_UI_DISPATCH_H

#include <stdbool.h>
#include "input/input.h"
#include "domain/presentation_runtime.h"
#include "domain/camera.h"


/* UI tap dispatcher.
 *
 * Single entry point that lets the input module ask the UI layer: "did the
 * UI absorb the tap?" instead of reaching into individual modules to
 * check open modals, inventory bars, etc. The implementation walks the UI
 * stack in z-order and returns the first hit.
 *
 *   ui_dispatch_tap          true  → UI absorbed the tap, world should ignore
 *   ui_dispatch_covers_point true  → screen pixel is currently covered by UI
 *                                    (used by the world hit-test guard)
 */

bool ui_dispatch_tap(int screen_x, int screen_y);
bool ui_dispatch_covers_point(int screen_x, int screen_y);

static void ui_on_tick(input_queue_t* input_queue, double dt) {
    input_queue_t bkp_queue = { 0 };

    input_event_t evt = { 0 };
    while (input_pop(input_queue, &evt)) {
        bool consumed = false;
        if(!consumed && INPUT_TAP == evt.type) {
            int mx = (int)evt.screen_position.x;
            int my = (int)evt.screen_position.y;
            if (!consumed && ui_dispatch_tap(mx, my)) { consumed = true; }
            if (!consumed && ui_dispatch_covers_point(mx, my)) { consumed = true; }
        }
        if(!consumed && INPUT_KEY_DEBUG == evt.type) {
            presentation_runtime_toggle_dev_ui();
            consumed = true;
        }

        if(!consumed && INPUT_ZOOM == evt.type) {
            if(evt.zoom_in) { camera_zoom_by(1.1); } else { camera_zoom_by(0.9); }
            consumed = true;
        }

        // unconsumed event back to the queue
        if(!consumed) {
            input_push(&bkp_queue, evt );
            continue;
        }
    }

    // return unconsummed events to the original queue
    input_event_t bkp_evt = { 0 };
    while (input_pop(&bkp_queue, &bkp_evt)) { input_push(input_queue, bkp_evt ); }
}

#endif /* CYBERIA_UI_DISPATCH_H */
