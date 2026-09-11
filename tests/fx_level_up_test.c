#include "fx/fx_level_up.h"
#include "ui/text.h"

#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static int labels;
static int sparks;
static int stars;
static float lowest_spark_y;
static float highest_spark_y;
static float leftmost_spark_x;
static float rightmost_spark_x;
static EntityState entity;

float world_cell_size(void) { return 32.0f; }
const EntityState* game_state_find_entity(const char* id) { return 0 == strcmp(id, entity.id) ? &entity : NULL; }
void text_draw_compat(const char* text, int x, int y, int size, Color color) {
    assert(0 == strcmp("LEVEL UP", text));
    if (255 == color.r) labels++;
}
int text_measure_compat(const char* text, int size) { return 80; }
void fx_shape_spark_shaded(float cx, float cy, float size_px, Color body) {
    assert(3.0f <= size_px && 255 == body.a);
    if (cy > lowest_spark_y) lowest_spark_y = cy;
    if (cy < highest_spark_y) highest_spark_y = cy;
    if (cx < leftmost_spark_x) leftmost_spark_x = cx;
    if (cx > rightmost_spark_x) rightmost_spark_x = cx;
    sparks++;
}
void ui_icon_draw_ex(const char* icon_id, float cx, float cy, float size, float rotation_deg, Color tint) {
    assert(0 == strcmp("star", icon_id));
    stars++;
}
const Color FX_SPARK_GOLD = { 255, 226, 20, 255 };

static void reset_probe(void) {
    sparks = 0;
    lowest_spark_y = -1e9f;
    highest_spark_y = 1e9f;
    leftmost_spark_x = 1e9f;
    rightmost_spark_x = -1e9f;
}

int main(void) {
    strcpy(entity.id, "e");
    entity.interp_pos = (Vector2){ 2.0f, 3.0f };
    entity.dims = (Vector2){ 1.0f, 1.0f };

    fx_level_up_reset();
    assert(false == fx_level_up_observe(0, 10, &entity));
    assert(false == fx_level_up_observe(10, 10, &entity));
    assert(false == fx_level_up_observe(10, 9, &entity));
    assert(false == fx_level_up_observe(2, 5, NULL));
    fx_level_up_draw();
    assert(0 == labels && 0 == sparks && 0 == stars);

    /* Sparks live between the feet (y = 4 cells) and above the head. */
    assert(true == fx_level_up_observe(2, 5, &entity));
    reset_probe();
    fx_level_up_draw();
    assert(1 == labels && 0 < sparks);
    assert(lowest_spark_y <= 4.0f * 32.0f && highest_spark_y >= 2.0f * 32.0f);

    /* The aura follows the entity: after it walks, every spark sits over the new footprint. */
    entity.interp_pos = (Vector2){ 10.0f, 3.0f };
    fx_level_up_update(1.0f);
    reset_probe();
    fx_level_up_draw();
    assert(0 < sparks && 1 == stars);
    assert(leftmost_spark_x >= 10.0f * 32.0f - 4.0f && rightmost_spark_x <= 11.0f * 32.0f + 4.0f);

    /* Still looping past two seconds, and gone after the whole duration. */
    assert(true == fx_level_up_active("e"));
    fx_level_up_update(1.2f);
    reset_probe();
    fx_level_up_draw();
    assert(0 < sparks);
    fx_level_up_update(2.0f);
    reset_probe();
    fx_level_up_draw();
    assert(0 == sparks && false == fx_level_up_active("e"));

    /* A second level-up on the same entity reuses its slot. */
    assert(true == fx_level_up_observe(5, 6, &entity));
    fx_level_up_reset();
    reset_probe();
    fx_level_up_draw();
    assert(0 == sparks);
    puts("fx_level_up_test OK");
    return 0;
}
