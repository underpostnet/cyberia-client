#include "fx/fx_portal_aura.h"
#include "sim/game_state.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

GameState g_game_state;

static int sparks;
static bool blue;
static float highest_y;
static float leftmost_x;
static float rightmost_x;

float world_cell_size(void) { return 32.0f; }
const Color FX_SPARK_BLUE = { 80, 160, 255, 255 };
void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px);
    blue = blue && 80 == body.r && 160 == body.g && 255 == body.b;
    if (cy < highest_y) highest_y = cy;
    if (cx < leftmost_x) leftmost_x = cx;
    if (cx > rightmost_x) rightmost_x = cx;
    sparks++;
}

static void draw(void) {
    sparks = 0;
    blue = true;
    highest_y = 1e9f;
    leftmost_x = 1e9f;
    rightmost_x = -1e9f;
    fx_portal_aura_draw();
}

int main(void) {
    g_game_state.mask[0] = COMP_PLAYER | COMP_TRANSFORM;
    g_game_state.render[0].interp_pos = (Vector2){ 2.0f, 3.0f };
    g_game_state.transform[0].size = (Vector2){ 1.0f, 1.0f };
    Player* self = &g_game_state.player[0];
    fx_portal_aura_update(0.3f);

    /* Off a portal, no aura. */
    draw();
    assert(0 == sparks);

    /* On a portal, blue sparks gather around the feet (y = 4 cells) and climb. */
    self->on_portal = true;
    self->portal_hold = 0.0f;
    draw();
    int early = sparks;
    float early_top = highest_y;
    assert(0 < early && blue && early_top < 4.0f * 32.0f);
    assert(leftmost_x < 2.0f * 32.0f && rightmost_x > 3.0f * 32.0f);

    /* A fuller charge raises the column and adds sparks to it. */
    self->portal_hold = 1.0f;
    draw();
    assert(early < sparks && highest_y < early_top);

    /* Every player in view on a portal gets its own aura, where it stands. */
    self->on_portal = false;
    g_game_state.mask[1] = COMP_PLAYER | COMP_TRANSFORM;
    g_game_state.render[1].interp_pos = (Vector2){ 10.0f, 3.0f };
    g_game_state.transform[1].size = (Vector2){ 1.0f, 1.0f };
    g_game_state.player[1].on_portal = true;
    draw();
    assert(0 < sparks && leftmost_x >= 9.0f * 32.0f && rightmost_x <= 12.0f * 32.0f);

    puts("fx_portal_aura_test OK");
    return 0;
}
