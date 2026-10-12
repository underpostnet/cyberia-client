#include "fx/fx_heal_aura.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static int sparks;
static int pluses;
static bool green;
static float leftmost_x;
static float rightmost_x;
static float lowest_y;
static float highest_y;
static EntityState entity;

float world_cell_size(void) { return 32.0f; }
const EntityState* game_state_find_entity(const char* id) { return 0 == strcmp(id, entity.id) ? &entity : NULL; }
const Color FX_SPARK_GREEN = { 70, 220, 100, 255 };

static void probe(float cx, float cy, Color body) {
    green = green && 70 == body.r && 220 == body.g && 100 == body.b;
    if (cx < leftmost_x) leftmost_x = cx;
    if (cx > rightmost_x) rightmost_x = cx;
    if (cy > lowest_y) lowest_y = cy;
    if (cy < highest_y) highest_y = cy;
}
void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px);
    probe(cx, cy, body);
    sparks++;
}
void fx_shape_plus(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px);
    probe(cx, cy, body);
    pluses++;
}

static void draw(void) {
    sparks = 0;
    pluses = 0;
    green = true;
    leftmost_x = 1e9f;
    rightmost_x = -1e9f;
    lowest_y = -1e9f;
    highest_y = 1e9f;
    fx_heal_aura_draw();
}

int main(void) {
    strcpy(entity.id, "e");
    entity.interp_pos = (Vector2){ 2.0f, 3.0f };
    entity.dims = (Vector2){ 1.0f, 1.0f };
    fx_heal_aura_reset();

    /* Only the loot behavior that heals starts an aura, and only on an entity in view. */
    assert(false == fx_heal_aura_observe("", &entity));
    assert(false == fx_heal_aura_observe("consume-on-pickup", &entity));
    assert(false == fx_heal_aura_observe(NULL, &entity));
    assert(false == fx_heal_aura_observe("consume-on-pickup-heal", NULL));
    draw();
    assert(0 == sparks && 0 == pluses);

    /* Green sparks circle the feet (y = 4 cells) and rise through the body. */
    assert(true == fx_heal_aura_observe("consume-on-pickup-heal", &entity));
    fx_heal_aura_update(0.4f);
    draw();
    assert(0 < sparks && green);
    assert(lowest_y >= 4.0f * 32.0f && highest_y < 4.0f * 32.0f);
    assert(leftmost_x < 2.0f * 32.0f && rightmost_x > 3.0f * 32.0f);

    /* Plus marks float above the body while the aura runs. */
    assert(0 < pluses);

    /* The aura follows the entity: after it walks, every shape sits around the new footprint. */
    entity.interp_pos = (Vector2){ 10.0f, 3.0f };
    fx_heal_aura_update(0.4f);
    draw();
    assert(0 < sparks);
    assert(leftmost_x >= 9.0f * 32.0f && rightmost_x <= 12.0f * 32.0f);

    /* Gone after the whole duration. */
    fx_heal_aura_update(1.1f);
    draw();
    assert(0 == sparks && 0 == pluses);

    /* A reset clears a running aura. */
    assert(true == fx_heal_aura_observe("consume-on-pickup-heal", &entity));
    fx_heal_aura_reset();
    draw();
    assert(0 == sparks && 0 == pluses);

    puts("fx_heal_aura_test OK");
    return 0;
}
