/* The save status line: what shows for each durability the server reports,
 * the note once the progress saved again, and the greeting of a rejoined
 * session. */

#include "ui/save_status.h"
#include "ui/text.h"
#include "ui/ui_icon.h"
#include "view/viewport.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static float s_dt = 0.1f;
static char s_drawn[128];

float GetFrameTime(void) { return s_dt; }
bool viewport_is_mobile(void) { return false; }
int text_measure_compat(const char* text, int size) { return (int)strlen(text) * size / 2; }
void text_draw_outlined(const char* text, int x, int y, int size, Color fg, Color outline, int rings) {
    snprintf(s_drawn, sizeof s_drawn, "%s", text);
}
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) {}
void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float thick, Color color) {}

/* What the line draws after `seconds` of frames. */
static const char* shown(float seconds) {
    s_drawn[0] = '\0';
    for (float t = 0.0f; seconds > t; t += s_dt) {
        s_drawn[0] = '\0';
        save_status_draw(1280, 50.0f);
    }
    return s_drawn;
}

int main(void) {
    save_status_observe(DURABILITY_SAVED, 0);
    assert(0 == strcmp("", shown(1.0f)));
    save_status_observe(DURABILITY_SAVING, 120);
    assert(0 == strcmp("", shown(1.0f)));
    save_status_observe(DURABILITY_SAVED, 0);
    assert(0 == strcmp("", shown(1.0f)));

    save_status_observe(DURABILITY_SAVING, 3500);
    assert(0 == strcmp("Saving...", shown(0.5f)));
    save_status_observe(DURABILITY_DEGRADED, 16000);
    assert(0 == strcmp("Saving delayed - retrying", shown(0.5f)));
    save_status_observe(DURABILITY_RECOVERY, 20000);
    assert(0 == strcmp("Latest changes not saved yet - trades paused", shown(0.5f)));
    save_status_observe(DURABILITY_RECOVERY, 185000);
    assert(0 == strcmp("Last 3 min not saved yet - trades paused", shown(0.5f)));

    /* The note shows once the server reports every change saved, then fades. */
    save_status_observe(DURABILITY_SAVED, 0);
    assert(0 == strcmp("Progress saved", shown(0.5f)));
    assert(0 == strcmp("", shown(4.0f)));
    save_status_observe(DURABILITY_SAVED, 0);
    assert(0 == strcmp("", shown(1.0f)));

    save_status_joined(true, false);
    assert(0 == strcmp("Reconnected", shown(0.5f)));
    save_status_joined(false, true);
    assert(0 == strcmp("Latest progress kept - saving", shown(0.5f)));
    /* A condition the player can act on outranks a note. */
    save_status_observe(DURABILITY_RECOVERY, 1000);
    assert(0 == strcmp("Latest changes not saved yet - trades paused", shown(0.5f)));
    save_status_reset();
    assert(0 == strcmp("", shown(1.0f)));
    save_status_joined(false, false);
    assert(0 == strcmp("", shown(1.0f)));

    puts("save_status_test OK");
    return 0;
}
