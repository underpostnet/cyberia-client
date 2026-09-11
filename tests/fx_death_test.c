#include "fx/fx_death.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static int sparks;
static int skulls;
static float leftmost_spark_x;
static float rightmost_spark_x;
static EntityState entity;

float world_cell_size(void) { return 32.0f; }
const EntityState* game_state_find_entity(const char* id) { return 0 == strcmp(id, entity.id) ? &entity : NULL; }
void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px && 220 == body.r && 255 == body.a);
    if (cx < leftmost_spark_x) leftmost_spark_x = cx;
    if (cx > rightmost_spark_x) rightmost_spark_x = cx;
    sparks++;
}
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) {
    assert(0 == strcmp("skull", icon_id));
    skulls++;
}
const Color FX_SPARK_RED = { 220, 40, 40, 255 };

static void reset_probe(void) {
    sparks = 0;
    skulls = 0;
    leftmost_spark_x = 1e9f;
    rightmost_spark_x = -1e9f;
}

int main(void) {
    strcpy(entity.id, "e");
    entity.interp_pos = (Vector2){ 2.0f, 3.0f };
    entity.dims = (Vector2){ 1.0f, 1.0f };

    fx_death_reset();
    assert(false == fx_death_observe(false, true, &entity));  /* first seen dead */
    assert(false == fx_death_observe(true, false, &entity));  /* still alive */
    assert(false == fx_death_observe(true, true, NULL));
    fx_death_draw();
    assert(0 == sparks && 0 == skulls);

    assert(true == fx_death_observe(true, true, &entity));
    reset_probe();
    fx_death_draw();
    assert(0 < sparks);

    /* The cue follows the body: after it is moved, the sparks sit near the new spot. */
    entity.interp_pos = (Vector2){ 10.0f, 3.0f };
    fx_death_update(0.3f);
    reset_probe();
    fx_death_draw();
    assert(0 < sparks && 1 == skulls);
    assert(leftmost_spark_x > 8.0f * 32.0f && rightmost_spark_x < 13.0f * 32.0f);

    /* One shot: gone after its duration. */
    assert(true == fx_death_active("e"));
    fx_death_update(2.5f);
    reset_probe();
    fx_death_draw();
    assert(0 == sparks && 0 == skulls && false == fx_death_active("e"));

    assert(true == fx_death_observe(true, true, &entity));
    fx_death_reset();
    reset_probe();
    fx_death_draw();
    assert(0 == sparks);
    puts("fx_death_test OK");
    return 0;
}
