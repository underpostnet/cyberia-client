#include "ui/entity_overhead_ui.h"
#include "world_types.h"

#include <assert.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Rounded rectangles drawn, and the widest one that is not a full bar: the HP fill. */
static int   s_rounded;
static float s_fill_w;
/* The rows drawn, from the bottom up: whether the transport label came last. */
static bool  s_transport_top;

void DrawRectangleRounded(Rectangle rec, float roundness, int segments, Color color) {
    s_rounded++;
    if (rec.width < (float)EOHUD_HP_BAR_W && rec.width > s_fill_w) s_fill_w = rec.width;
}
void DrawRectangleRoundedLinesEx(Rectangle rec, float roundness, int segments, float thick, Color color) {}
Color ColorLerp(Color a, Color b, float t) { return a; }
const char* presentation_runtime_status_icon(uint8_t status_id) { return ""; }
void text_draw_compat(const char* text, int x, int y, int size, Color color) {
    s_transport_top = 0 == strcmp("Transporting...", text);
}
int text_measure_compat(const char* text, int size) { return 10; }
void text_draw_outlined(const char* text, int x, int y, int size, Color fg, Color outline, int rings) {}
void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase) {}

static const EntityTransform s_transform = { .pos = { 4.0f, 4.0f }, .size = { 2.0f, 2.0f } };

static void draw(const EntityData* o) {
    s_rounded = 0;
    s_fill_w = 0.0f;
    entity_overhead_ui_draw_obstacle(o, &s_transform, 32.0f);
}

int main(void) {
    EntityData o = { 0 };

    /* An ordinary obstacle carries no life and draws no bar. */
    draw(&o);
    assert(0 == s_rounded);

    /* A standing destructible obstacle shows the shared HP bar, filled by its life. */
    o.max_life = 12.0f;
    o.life = 6.0f;
    draw(&o);
    assert(0 < s_rounded && (float)EOHUD_HP_BAR_W * 0.5f == s_fill_w);

    /* The next snapshot moves the fill. */
    o.life = 3.0f;
    draw(&o);
    assert((float)EOHUD_HP_BAR_W * 0.25f == s_fill_w);

    /* Depleted, it draws no bar. */
    o.life = 0.0f;
    draw(&o);
    assert(0 == s_rounded);

    /* A resource keeps the same bar through the shared path. */
    EntityOverheadParams resource = { .name = "ore", .life = 5.0f, .max_life = 10.0f, .show_hp = true };
    s_fill_w = 0.0f;
    entity_overhead_ui_draw(&resource, 4.0f, 4.0f, 2.0f, 32.0f);
    assert((float)EOHUD_HP_BAR_W * 0.5f == s_fill_w);

    /* A player on a portal shows the transport bar at the top, filled by its charge. */
    EntityOverheadParams traveler = { .name = "traveler", .life = 10.0f, .max_life = 10.0f, .show_hp = true,
                                      .show_name = true, .on_portal = true, .portal_hold = 0.25f };
    s_fill_w = 0.0f;
    entity_overhead_ui_draw(&traveler, 4.0f, 4.0f, 1.0f, 32.0f);
    assert(s_transport_top && (float)EOHUD_HP_BAR_W * 0.25f == s_fill_w);

    /* Off a portal, no transport bar. */
    traveler.on_portal = false;
    entity_overhead_ui_draw(&traveler, 4.0f, 4.0f, 1.0f, 32.0f);
    assert(!s_transport_top);

    puts("entity_overhead_ui_test OK");
    return 0;
}
