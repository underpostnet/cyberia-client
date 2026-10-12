#include "ui/inventory_bar.c"
#include "domain/local_player.h"
#include "domain/viewport.h"
#include "fx/fx_inventory_bar_qty.h"
#include "fx/fx_shapes.h"
#include "game_state.h"
#include "object_layer.h"
#include "object_layers_management.h"
#include "ol_as_animated_ico.h"
#include "ol_stack_ico.h"
#include "ui/item_slot.h"
#include "ui/ui_state.h"
#include "ui/ui_toggle.h"

#include <assert.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

LocalPlayer g_local_player;
GameState g_game_state;
static bool stasis;
static int preview_count;
static int slot_count;
static Rectangle preview;
static Rectangle cooldown;
static float charge = 1.0f;
static bool mouse_down;
static Vector2 mouse;

int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }
double GetTime(void) { return 10.0; }
bool viewport_is_mobile(void) { return false; }
bool local_player_in_stasis(void) { return stasis; }
float local_player_action_charge(double now) { return 0.5f; }
ObjectLayer* lookup_cached_layer(const char* id) { return NULL; }
const char* active_layer_item_id(const ObjectLayerState* layers, int count, const char* type) { return NULL; }
void ui_toggle_init(UIToggle* toggle, Rectangle anchor, bool expanded, UIToggleChevron chevron) {
    *toggle = (UIToggle){ .anchor = anchor, .expanded = expanded, .anim_t = expanded ? 1.0f : 0.0f };
}
void ui_toggle_set_drag(UIToggle* toggle) {}
void ui_toggle_set_anchor(UIToggle* toggle, Rectangle anchor) { toggle->anchor = anchor; }
void ui_toggle_draw(const UIToggle* toggle) {}
bool fx_inventory_bar_qty_slot_visible(const char* id) { return true; }
bool fx_inventory_bar_qty_slot_pulsing(const char* id) { return false; }
float fx_inventory_bar_qty_slot_scale(const char* id) { return 1.0f; }
int fx_inventory_bar_qty_display(const char* id, int actual) { return actual; }
void fx_inventory_bar_qty_draw(Rectangle slot, const char* id) {}
void fx_inventory_bar_qty_draw_bottom(Rectangle anchor) {}
void DrawRectangle(int x, int y, int width, int height, Color color) {}
void DrawRectangleRounded(Rectangle rect, float roundness, int segments, Color color) {}
void DrawRectangleLinesEx(Rectangle rect, float thick, Color color) {}
void DrawLine(int x1, int y1, int x2, int y2, Color color) {}
void DrawCircle(int x, int y, float radius, Color color) {}
void DrawText(const char* text, int x, int y, int size, Color color) {}
int MeasureText(const char* text, int size) { return 10; }
Vector2 GetMousePosition(void) { return mouse; }
int GetMouseX(void) { return (int)mouse.x; }
int GetMouseY(void) { return (int)mouse.y; }
int GetTouchPointCount(void) { return 0; }
Vector2 GetTouchPosition(int index) { return mouse; }
bool IsMouseButtonDown(int button) { return mouse_down; }
bool ui_button_hit(Rectangle b, int mx, int my) {
    return mx >= b.x && mx < b.x + b.width && my >= b.y && my < b.y + b.height;
}
bool ui_toggle_handle_click(UIToggle* toggle, int mx, int my) { return false; }
void ui_toggle_update(UIToggle* toggle, float dt) {}
void fx_inventory_bar_qty_update(float dt) {}
bool item_use_is_consumable(const char* id) { return 0 == strcmp("gel", id); }
float item_use_charge(double now) { return charge; }
item_use_state_e item_use_state(const char* id, double now) { return ITEM_USE_READY; }
bool CheckCollisionPointRec(Vector2 point, Rectangle rect) { return false; }
Color ColorBrightness(Color color, float factor) { return color; }
void item_slot_draw_ex(Rectangle rect, const ObjectLayerState* layer, ObjectLayersManager* manager,
                       Color highlight, float amount, bool full_color) { slot_count++; }
void ol_as_ico_draw(ObjectLayersManager* manager, const char* id, int x, int y, int size,
                    const char* direction, int frame_ms, Color tint) {}
void ol_stack_ico_draw(ObjectLayersManager* manager, const ObjectLayerState* layers, int count,
                       int x, int y, int size, const char* direction, int frame_ms, Color tint) {
    assert(1 == count && 1 == layers[0].quantity);
    if (0 == strcmp("gel", layers[0].item_id)) return;
    assert(0 == strcmp("bullet", layers[0].item_id));
    preview = (Rectangle){ (float)x, (float)y, (float)size, (float)size };
    preview_count++;
}
void fx_shape_cooldown_bar(Rectangle r, float elapsed) { cooldown = r; }

/* Press and release at the centre of `r`, and settle the gesture. */
static void press(Rectangle r) {
    mouse = (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    assert(inventory_bar_handle_click((int)mouse.x, (int)mouse.y, NULL));
    mouse_down = true;
    inventory_bar_update(0.05f);
    mouse_down = false;
    inventory_bar_update(0.05f);
}

static void draw_at(float animation) {
    s_bar_toggle.anim_t = animation;
    s_bar_toggle.expanded = 1.0f == animation;
    preview_count = slot_count = 0;
    inventory_bar_draw();
}

int main(void) {
    UiSkillEntry skill = { 0 };
    strcpy(skill.trigger_item_id, "pistol");
    strcpy(skill.summoned_entity_item_id, "bullet");
    assert(0 <= ui_state_push_skill(&skill));
    g_local_player.inventory_count = 2;
    strcpy(g_local_player.inventory[0].item_id, "skin");
    strcpy(g_local_player.inventory[1].item_id, "pistol");
    g_local_player.inventory[1].active = true;

    draw_at(1.0f);
    assert(1 == preview_count && 2 == slot_count);
    Rectangle shown = preview;
    assert(cooldown.y + cooldown.height < 600.0f - (float)bar_height());

    draw_at(0.5f);
    assert(1 == preview_count && 2 == slot_count);
    assert(shown.x == preview.x && shown.y + 36.0f == preview.y);

    draw_at(0.0f);
    assert(1 == preview_count && 0 == slot_count);
    assert(shown.x == preview.x && shown.y + 72.0f == preview.y);
    assert(cooldown.y + cooldown.height < 600.0f);

    stasis = true;
    draw_at(0.0f);
    assert(0 == preview_count);
    stasis = false;
    g_local_player.inventory[1].active = false;
    draw_at(0.0f);
    assert(0 == preview_count);

    /* A consumable slot carries a quick-use button above it, as an active slot carries its skills. */
    g_local_player.inventory_count = 3;
    strcpy(g_local_player.inventory[2].item_id, "gel");
    g_local_player.inventory[2].quantity = 2;
    charge = 0.5f;
    draw_at(1.0f);
    Rectangle gel = slot_rect(2, bar_top(600));
    assert(1 == s_use_button_count);
    Rectangle button = s_use_buttons[0].rect;
    assert(button.y + button.height < gel.y && button.x > gel.x && button.x + button.width < gel.x + gel.width);
    assert(cooldown.y + cooldown.height <= button.y + button.height && cooldown.y > button.y);
    assert(inventory_bar_point_covered((int)(button.x + 1.0f), (int)(button.y + 1.0f)));

    /* A press on the button spends one unit and opens nothing; a press on the slot opens it. */
    char used[MAX_ITEM_ID_LENGTH];
    int tapped = -1;
    press(button);
    assert(inventory_bar_take_use(used, sizeof used) && 0 == strcmp("gel", used));
    assert(!inventory_bar_take_tap(&tapped));
    press(gel);
    assert(!inventory_bar_take_use(used, sizeof used));
    assert(inventory_bar_take_tap(&tapped) && 2 == tapped);

    /* In stasis the buttons hide and take no press. */
    stasis = true;
    draw_at(1.0f);
    assert(0 == s_use_button_count && !inventory_bar_point_covered((int)(button.x + 1.0f), (int)(button.y + 1.0f)));
    stasis = false;

    puts("inventory_bar_preview_test OK");
    return 0;
}
