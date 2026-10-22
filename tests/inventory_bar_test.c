#include "ui/inventory_bar.c"
#include "sim/local_player.h"
#include "view/viewport.h"
#include "fx/fx_inventory_bar_qty.h"
#include "fx/fx_shapes.h"
#include "sim/game_state.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "ui/ol_as_animated_ico.h"
#include "ui/ol_stack_ico.h"
#include "ui/item_slot.h"
#include "ui/ui_state.h"
#include "ui/ui_toggle.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

LocalPlayer g_local_player;
GameState g_game_state;
const Color MODAL_PANEL_BG = { 0 }, MODAL_PANEL_BORDER = { 0 }, MODAL_SELECTED = { 0 };
static bool stasis;
static int preview_count;
static int slot_count;
static Rectangle preview;
static char preview_item[MAX_ITEM_ID_LENGTH];
static int badge_quantity;
static Rectangle cooldown;
static float cooldown_charge;
static bool mouse_down;
static Vector2 mouse;

int GetScreenWidth(void) { return 800; }
int GetScreenHeight(void) { return 600; }
double GetTime(void) { return 10.0; }
bool viewport_is_mobile(void) { return false; }
bool local_player_in_stasis(void) { return stasis; }
float local_player_action_charge(double now) { return 0.5f; }
float local_player_consume_charge(double now) { return 0.25f; }
ObjectLayer* lookup_cached_layer(const char* id) { return NULL; }
bool local_player_is_coin(const char* id) { return 0 == strcmp("coin", id); }
int local_player_open_slots(CyberiaCapacityRule rule) { return rule.base; }
int local_player_unlock_level(CyberiaCapacityRule rule, int slot) { return 0; }
void item_slot_draw_empty(Rectangle r) {}
void item_slot_draw_locked(Rectangle r, int unlock_level) {}
void modal_notification_show(const char* title, const char* message, Color accent) {}
void text_draw_outlined(const char* text, int x, int y, int size, Color fg, Color outline, int rings) {}
const char* active_layer_item_id(const ObjectLayerState* layers, int count, const char* type) { return NULL; }
void ui_toggle_init(UIToggle* toggle, Rectangle anchor, bool expanded, UIToggleChevron chevron) {
    *toggle = (UIToggle){ .anchor = anchor, .expanded = expanded, .anim_t = expanded ? 1.0f : 0.0f };
}
void ui_toggle_set_drag(UIToggle* toggle) {}
void ui_toggle_set_anchor(UIToggle* toggle, Rectangle anchor) { toggle->anchor = anchor; }
void ui_toggle_draw(const UIToggle* toggle) {}
bool fx_inventory_bar_qty_slot_visible(const char* id) { return true; }
bool fx_inventory_bar_qty_slot_pulsing(const char* id, int slot) { return false; }
float fx_inventory_bar_qty_slot_scale(const char* id, int slot) { return 1.0f; }
int fx_inventory_bar_qty_display(const char* id, int slot, int actual) { return actual; }
void fx_inventory_bar_qty_draw(Rectangle rect, const char* id, int slot) {}
static bool panel_open;
bool container_panel_cell_at(int mx, int my, Box* box, int* slot) {
    if (!panel_open || my > 100) return false;
    *box = BOX_STORAGE;
    *slot = mx / 100;
    return true;
}
Rectangle container_panel_cell_rect(int slot) { return (Rectangle){ (float)slot * 100.0f, 0.0f, 100.0f, 100.0f }; }
void fx_inventory_bar_qty_draw_bottom(Rectangle anchor) {}
void DrawRectangle(int x, int y, int width, int height, Color color) {}
void DrawRectangleRounded(Rectangle rect, float roundness, int segments, Color color) {}
void DrawRectangleLinesEx(Rectangle rect, float thick, Color color) {}
void DrawRectangleRec(Rectangle rect, Color color) {}
static int trade_badges;
void ui_icon_draw(const char* icon_id, float cx, float cy, int size, bool bounce, float phase) {
    trade_badges += 0 == strcmp("trade", icon_id);
}
void DrawLine(int x1, int y1, int x2, int y2, Color color) {}
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
bool ui_toggle_handle_click(UIToggle* toggle, int mx, int my) {
    if (!ui_button_hit(toggle->anchor, mx, my)) return false;
    toggle->expanded = !toggle->expanded;
    return true;
}
void ui_toggle_update(UIToggle* toggle, float dt) {}
void fx_inventory_bar_qty_update(float dt) {}
bool CheckCollisionPointRec(Vector2 point, Rectangle rect) { return false; }
Color ColorBrightness(Color color, float factor) { return color; }
void item_slot_draw_ex(Rectangle rect, const ObjectLayerState* layer, ObjectLayersManager* manager,
                       Color highlight, float amount, bool full_color) { slot_count++; }
void ol_as_ico_draw(ObjectLayersManager* manager, const char* id, int x, int y, int size,
                    const char* direction, int frame_ms, Color tint) {}
void ol_stack_ico_draw(ObjectLayersManager* manager, const ObjectLayerState* layers, int count,
                       int x, int y, int size, const char* direction, int frame_ms, Color tint) {
    assert(1 == count && 1 == layers[0].quantity);
    copy_str(preview_item, sizeof preview_item, layers[0].item_id);
    preview = (Rectangle){ (float)x, (float)y, (float)size, (float)size };
    preview_count++;
}
void item_slot_draw_quantity(Rectangle r, int quantity) { badge_quantity = quantity; }
static int dropped_from = -2, dropped_to = -2;
static Box dropped_box;
bool stack_move_drop(Box from, int from_slot, Box to, int to_slot, Rectangle origin) {
    assert(BOX_BAG == from);
    dropped_from = from_slot;
    dropped_to = to_slot;
    dropped_box = to;
    return true;
}
MoveKind container_ops_check(Box from, int from_slot, Box to, int to_slot, int qty) { return MOVE_PLACE; }
static bool covered, offer_open;
static int offered = -2, ground_slot = -2;
static Vector2 ground_cell;
bool ui_dispatch_covers_point(int x, int y) { return covered; }
bool trade_panel_offer_at(int mx, int my) { return offer_open && my < 100; }
Rectangle trade_panel_offer_rect(void) { return (Rectangle){ 0 }; }
void trade_panel_offer_bag_stack(int bag_slot) { offered = bag_slot; }
Vector2 world_drop_cell(Vector2 screen) { return (Vector2){ screen.x / 100.0f, screen.y / 100.0f }; }
bool world_drop_allowed(const char* item_id, const char** reason) { return true; }
void world_drop_draw_target(Vector2 cell, bool allowed) {}
void world_drop_ask(Box box, int slot, const char* item_id, int quantity, Vector2 cell) {
    assert(BOX_BAG == box);
    ground_slot = slot;
    ground_cell = cell;
}
const ObjectLayerState* container_ops_stack(Box box, int slot) {
    int at = BAG_WALLET_SLOT == slot ? find_coin_slot() : local_player_bag_stack(slot);
    return 0 <= at ? &g_local_player.inventory[at] : NULL;
}
void fx_shape_cooldown_bar(Rectangle r, float elapsed) {
    cooldown = r;
    cooldown_charge = elapsed;
}

/* Press and release at the centre of `r`, and settle the gesture. */
static void press(Rectangle r) {
    mouse = (Vector2){ r.x + r.width * 0.5f, r.y + r.height * 0.5f };
    assert(inventory_bar_handle_click((int)mouse.x, (int)mouse.y, NULL));
    mouse_down = true;
    inventory_bar_update(0.05f);
    mouse_down = false;
    inventory_bar_update(0.05f);
}

/* Press at the centre of `from`, drag to the centre of `to`, and release. */
static void drag(Rectangle from, Rectangle to) {
    mouse = (Vector2){ from.x + from.width * 0.5f, from.y + from.height * 0.5f };
    assert(inventory_bar_handle_click((int)mouse.x, (int)mouse.y, NULL));
    mouse_down = true;
    inventory_bar_update(0.05f);
    mouse = (Vector2){ to.x + to.width * 0.5f, to.y + to.height * 0.5f };
    inventory_bar_update(0.05f);
    mouse_down = false;
    inventory_bar_update(0.05f);
}

static void carry(int at, const char* item_id, int slot) {
    memset(&g_local_player.inventory[at], 0, sizeof g_local_player.inventory[at]);
    strcpy(g_local_player.inventory[at].item_id, item_id);
    g_local_player.inventory[at].quantity = 1;
    g_local_player.inventory[at].slot = slot;
}

static Rectangle cell(int si) { return slot_rect(si, bar_top(600)); }

static Rectangle center_of(Rectangle r) { return (Rectangle){ r.x + r.width * 0.5f, r.y + r.height * 0.5f, 0, 0 }; }

/* Each cell is a bag slot: a stack shows in the cell of its slot, and the empty
 * cells between stacks stay empty. */
static void test_cells_are_bag_slots(void) {
    g_local_player.inventory_count = 3;
    carry(0, "skin", 0);
    carry(1, "pistol", 3);
    carry(2, "coin", BAG_WALLET_SLOT);
    Rectangle empty = center_of(cell(1)), stack = center_of(cell(3)), coin = center_of(coin_slot_rect(800, bar_top(600)));
    assert(-1 == inventory_bar_get_tapped_slot((int)empty.x, (int)empty.y));
    assert(1 == inventory_bar_get_tapped_slot((int)stack.x, (int)stack.y));

    int slot = 0;
    assert(inventory_bar_cell_at((int)empty.x, (int)empty.y, &slot) && 1 == slot);
    assert(inventory_bar_cell_at((int)coin.x, (int)coin.y, &slot) && BAG_WALLET_SLOT == slot);
    assert(!inventory_bar_cell_at(300, 100, &slot));
    Rectangle rect = inventory_bar_cell_rect(3);
    assert(rect.x == cell(3).x);

    /* A new item lands in the lowest free slot. */
    Rectangle landing;
    assert(inventory_bar_predicted_item_slot_rect("gem", &landing) && landing.x == cell(1).x);
}

/* A press on a stack taps it, and a drag moves it to the cell under the
 * release, of the bar or of the open container. Only the track and the wheel
 * over the bar scroll the strip. */
static void test_stacks_drag_and_tap(void) {
    g_local_player.inventory_count = 3;
    carry(0, "skin", 0);
    carry(1, "pistol", 3);
    carry(2, "coin", BAG_WALLET_SLOT);

    int tapped = -1;
    press(cell(3));
    assert(inventory_bar_take_tap(&tapped) && 1 == tapped && -2 == dropped_from);

    drag(cell(3), cell(5));
    assert(3 == dropped_from && 5 == dropped_to && BOX_BAG == dropped_box);
    assert(!inventory_bar_take_tap(&tapped));
    dropped_from = -2;

    /* Over the world, a stack drops on the ground at the cell under the pointer; the coins never do. */
    drag(cell(0), (Rectangle){ 300.0f, 100.0f, 10.0f, 10.0f });
    assert(-2 == dropped_from && 0 == ground_slot && 3.05f == ground_cell.x && 1.05f == ground_cell.y);
    ground_slot = -2;
    drag(coin_slot_rect(800, bar_top(600)), (Rectangle){ 300.0f, 100.0f, 10.0f, 10.0f });
    assert(-2 == ground_slot);
    covered = true;
    drag(cell(0), (Rectangle){ 300.0f, 100.0f, 10.0f, 10.0f });
    assert(-2 == ground_slot && -2 == dropped_from);
    covered = false;

    /* Over the own offer of an open trade, a stack joins the offer. */
    offer_open = true;
    drag(cell(3), (Rectangle){ 300.0f, 40.0f, 10.0f, 10.0f });
    assert(3 == offered && -2 == ground_slot && -2 == dropped_from);
    offer_open = false;

    /* A drag from an empty cell neither moves a stack, nor scrolls, nor taps. */
    drag(cell(2), cell(0));
    assert(-2 == dropped_from && 0.0f == s_scroll_px && !inventory_bar_take_tap(&tapped));

    /* A drag of the thumb pans the strip by the share of its travel. */
    Rectangle track = track_rect(bar_top(600)), thumb = thumb_rect(track);
    float max = max_scroll_px(800, strip_cells());
    assert(track.y + track.height <= cell(0).y && 0.0f < max);
    drag((Rectangle){ thumb.x + 100.0f, thumb.y, 0.0f, thumb.height },
         (Rectangle){ thumb.x + 200.0f, thumb.y, 0.0f, thumb.height });
    assert(0.01f > fabsf(100.0f / (track.width - thumb.width) * max - s_scroll_px));
    assert(-2 == dropped_from && !inventory_bar_take_tap(&tapped));

    /* A press on the track brings the thumb under the pointer. */
    press((Rectangle){ track.x + track.width - 2.0f, track.y, 0.0f, track.height });
    assert(max == s_scroll_px && !inventory_bar_take_tap(&tapped));
    s_scroll_px = 0.0f;

    /* With a container open, a stack and the coins drop on its cells. */
    panel_open = true;
    drag(cell(0), (Rectangle){ 250.0f, 40.0f, 10.0f, 10.0f });
    assert(0 == dropped_from && BOX_STORAGE == dropped_box && 2 == dropped_to);
    drag(coin_slot_rect(800, bar_top(600)), (Rectangle){ 150.0f, 40.0f, 10.0f, 10.0f });
    assert(BAG_WALLET_SLOT == dropped_from && 1 == dropped_to);
    panel_open = false;

    mouse = (Vector2){ 300.0f, 580.0f };
    assert(inventory_bar_handle_wheel(-1.0f) && (float)slot_pitch() == s_scroll_px);
    mouse = (Vector2){ 300.0f, 100.0f };
    assert(!inventory_bar_handle_wheel(-1.0f));
    s_scroll_px = 0.0f;
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
    g_local_player.inventory[1].slot = 1;
    g_local_player.inventory[1].active = true;

    draw_at(1.0f);
    assert(1 == preview_count && 2 == slot_count && 0 == strcmp("bullet", preview_item));
    assert(0.5f == cooldown_charge && 0 == badge_quantity);
    Rectangle shown = preview;
    assert(cooldown.y + cooldown.height < 600.0f - (float)bar_height());

    draw_at(0.5f);
    assert(1 == preview_count && 2 == slot_count);
    assert(shown.x == preview.x && shown.y + (float)bar_height() * 0.5f == preview.y);

    draw_at(0.0f);
    assert(1 == preview_count && 0 == slot_count);
    assert(shown.x == preview.x && shown.y + (float)bar_height() == preview.y);
    assert(cooldown.y + cooldown.height < 600.0f);

    stasis = true;
    draw_at(0.0f);
    assert(0 == preview_count);
    stasis = false;
    g_local_player.inventory[1].active = false;
    draw_at(0.0f);
    assert(0 == preview_count);

    /* A held skill previews its inactive stack above the slot: the item, the units left and the consume cooldown. */
    UiSkillEntry dose = { .held = true };
    strcpy(dose.trigger_item_id, "gel");
    strcpy(dose.summoned_entity_item_id, "gel");
    assert(0 <= ui_state_push_skill(&dose));
    g_local_player.inventory_count = 3;
    strcpy(g_local_player.inventory[2].item_id, "gel");
    g_local_player.inventory[2].quantity = 2;
    g_local_player.inventory[2].slot = 2;
    draw_at(1.0f);
    Rectangle gel = slot_rect(2, bar_top(600));
    assert(1 == preview_count && 0 == strcmp("gel", preview_item) && 2 == badge_quantity);
    assert(0.25f == cooldown_charge);
    assert(preview.y + preview.height < gel.y && preview.x > gel.x && preview.x + preview.width < gel.x + gel.width);

    /* The preview takes no press: a press on the slot opens it. */
    int tapped = -1;
    assert(!inventory_bar_point_covered((int)(preview.x + 1.0f), (int)(preview.y + 1.0f)));
    press(gel);
    assert(inventory_bar_take_tap(&tapped) && 2 == tapped);

    /* In stasis the preview hides. */
    stasis = true;
    draw_at(1.0f);
    assert(0 == preview_count);
    stasis = false;

    s_bar_toggle.anim_t = 1.0f;
    s_bar_toggle.expanded = true;
    test_cells_are_bag_slots();
    test_stacks_drag_and_tap();

    /* While a trade is open, the offered stack and the offered coins show the trade icon. */
    g_local_player.trade.state = TRADE_OPEN;
    g_local_player.trade.mine.count = 1;
    g_local_player.trade.mine.items[0].slot = 3;
    trade_badges = 0;
    draw_at(1.0f);
    assert(1 == trade_badges);
    g_local_player.trade.mine.coins = 5;
    trade_badges = 0;
    draw_at(1.0f);
    assert(2 == trade_badges);
    g_local_player.trade.state = TRADE_NONE;
    trade_badges = 0;
    draw_at(1.0f);
    assert(0 == trade_badges);

    puts("inventory_bar_test OK");
    return 0;
}
