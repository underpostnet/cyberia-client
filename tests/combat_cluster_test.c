#include "ui/combat_cluster.h"
#include "ui/floating_combat_text.h"
#include "ui/ui_state.h"

#include "content/stat_contract_generated.h"
#include "input/input.h"
#include "net/client_event.h"
#include "sim/auto_target.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "utils.h"
#include "world/object_layer.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Stubbed platform: an 844 x 390 phone in landscape, the camera on the centre,
 * 32 px cells and a 0.5 s action cooldown. The player stands at cell (10, 10)
 * and faces right; hostile bot 20 stands ahead, hostile bot 21 behind. */
#define SCREEN_W 844
#define SCREEN_H 390
#define DT       (1.0 / 60.0)
#define SELF     7
#define AHEAD    20
#define BEHIND   21

LocalPlayer g_local_player;
GameState   g_game_state;

static double  s_now;
static double  s_ready_at;
static bool    s_stasis;
static bool    s_keys[512];
static bool    s_shift;
static Vector2 s_mouse = { 40.0f, 195.0f };  /* far left of the player */
static int     s_handle;  /* the press the cluster holds */
static input_pointer_state_e s_pointer_state;
static Vector2 s_pointer;
static client_event_t s_events[16];
static int     s_event_count;
static uint32_t s_sequence;
static int     s_misfires;

double GetTime(void) { return s_now; }
int GetScreenWidth(void) { return SCREEN_W; }
int GetScreenHeight(void) { return SCREEN_H; }
bool IsKeyPressed(int key) { return s_keys[key]; }
bool IsKeyDown(int key) { return (KEY_LEFT_SHIFT == key || KEY_RIGHT_SHIFT == key) && s_shift; }
Vector2 GetMousePosition(void) { return s_mouse; }
Vector2 GetScreenToWorld2D(Vector2 position, Camera2D camera) { return position; }
Vector2 GetWorldToScreen2D(Vector2 position, Camera2D camera) { return position; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) { return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height; }
Camera2D camera_get(void) { return (Camera2D){ .offset = { SCREEN_W / 2.0f, SCREEN_H / 2.0f }, .zoom = 1.0f }; }
float world_cell_size(void) { return 32.0f; }
bool viewport_is_mobile(void) { return true; }
float toolbar_height(void) { return 40.0f; }
float inventory_bar_visible_height(void) { return 70.0f; }
float hud_side_stack_left(void) { return (float)SCREEN_W; }
bool session_gate_active(void) { return false; }
Direction local_player_view_direction(void) { return DIRECTION_RIGHT; }
int game_state_slot(EntityID id) { return SELF == id ? 0 : AHEAD == id ? 1 : BEHIND == id ? 2 : -1; }
const RenderState* game_state_own_render(void) { return &g_game_state.render[0]; }
const EntityTransform* game_state_own_transform(void) { return &g_game_state.transform[0]; }
const char* active_layer_item_id(const ObjectLayerState* layers, int count, const char* type) { return "skin"; }
bool local_player_in_stasis(void) { return s_stasis; }
float local_player_action_cooldown(void) { return 0.5f; }
void local_player_set_action_ready_at(double time_s) { s_ready_at = time_s; }
float local_player_action_charge(double now) {
    double left = s_ready_at - now;
    return 0.0 >= left ? 1.0f : (float)(1.0 - left / 0.5);
}
bool local_player_fct_push(const LocalFctEvent* ev) {
    s_misfires += FCT_TYPE_MISFIRE == ev->type;
    return true;
}
input_pointer_state_e input_pointer_at(int pointer, Vector2* position) {
    assert(pointer == s_handle);
    *position = s_pointer;
    return s_pointer_state;
}
void client_event_push(client_event_kind_t kind, client_event_payload_t p) {
    assert(CLIENT_EVENT_SKILL_ACTIVATE == kind);
    s_events[s_event_count++] = (client_event_t){ .kind = kind, .sequence = ++s_sequence, .payload = p };
}
const client_event_t* client_event_list(int* count) {
    *count = s_event_count;
    return s_events;
}

/* One entity of the view at a cell, alive, one cell wide. */
static void place(int slot, EntityID id, float x, float y, const char* behavior) {
    g_game_state.ids[slot]       = id;
    g_game_state.mask[slot]      = COMP_TRANSFORM | COMP_VITALS | (behavior ? COMP_AGENT : 0);
    g_game_state.render[slot]    = (RenderState){ .interp_pos = { x, y } };
    g_game_state.transform[slot] = (EntityTransform){ .pos = { x, y }, .size = { 1.0f, 1.0f } };
    if (behavior) copy_str(g_game_state.agent[slot].behavior, sizeof g_game_state.agent[slot].behavior, behavior);
}

static void push_skill(const char* trigger, const char* skill_id, SkillActivation activation, SkillTarget targeting) {
    UiSkillEntry skill = { .activation = activation, .targeting = targeting, .reach = 8.0f };
    copy_str(skill.trigger_item_id, sizeof skill.trigger_item_id, trigger);
    copy_str(skill.skill_id, sizeof skill.skill_id, skill_id);
    copy_str(skill.summoned_entity_item_id, sizeof skill.summoned_entity_item_id, "bullet");
    assert(0 <= ui_state_push_skill(&skill));
}

static void hold(const char* item_id, bool active) {
    ObjectLayerState* stack = &g_local_player.inventory[g_local_player.inventory_count++];
    *stack = (ObjectLayerState){ .active = active, .quantity = 1 };
    copy_str(stack->item_id, sizeof stack->item_id, item_id);
}

static void tick(void) {
    s_now += DT;
    combat_cluster_on_tick(DT);
}

static Vector2 control_at(int index) {
    Vector2 centers[COMBAT_CLUSTER_MAX];
    float   radii[COMBAT_CLUSTER_MAX];
    combat_cluster_layout((Rectangle){ 0, 40, SCREEN_W, SCREEN_H - 110 }, camera_get().offset, 2, centers, radii);
    return centers[index];
}

/* A press on `at` that moves to `to` and ends as `end` after `frames`. */
static void gesture(Vector2 at, Vector2 to, int frames, input_pointer_state_e end) {
    assert(combat_cluster_claim(++s_handle, at));
    s_pointer_state = INPUT_POINTER_HELD;
    s_pointer = to;
    for (int i = 0; frames > i; i++) tick();
    s_pointer_state = end;
    tick();
    s_pointer_state = INPUT_POINTER_GONE;
}

static void press(Vector2 at, Vector2 to, int frames) { gesture(at, to, frames, INPUT_POINTER_RELEASED); }

static void key(int code, bool shift) {
    s_keys[code] = true;
    s_shift = shift;
    tick();
    s_keys[code] = false;
    s_shift = false;
}

static const client_event_t* last_event(void) { return &s_events[s_event_count - 1]; }

/* Unit aims of the answers. */
static const Vector2 LEFT = { -1.0f, 0.0f }, RIGHT = { 1.0f, 0.0f }, DOWN = { 0.0f, 1.0f }, NO_AIM = { 0.0f, 0.0f };

/* The answer of the server to the last cast. */
static void ack(const char* reason, bool accepted, EntityID target, Vector2 aim, int ready_in_ms) {
    SkillAck answer = { .seq = last_event()->sequence, .accepted = accepted, .target_id = target,
                        .aim = aim, .ready_in_ms = ready_in_ms };
    copy_str(answer.skill_id, sizeof answer.skill_id, last_event()->payload.code);
    copy_str(answer.reason, sizeof answer.reason, reason);
    combat_cluster_on_ack(&answer);
}

static void expect_cast(int count, const char* skill_id, skill_aim_t aim, EntityID preferred) {
    assert(count == s_event_count);
    assert(0 == strcmp(skill_id, last_event()->payload.code));
    assert(aim == last_event()->payload.aim && preferred == last_event()->payload.entity_id);
}

static void test_layout(void) {
    Vector2 centers[COMBAT_CLUSTER_MAX];
    float   radii[COMBAT_CLUSTER_MAX];
    Rectangle phone = { 0, 40, SCREEN_W, SCREEN_H - 110 };
    combat_cluster_layout(phone, camera_get().offset, COMBAT_CLUSTER_MAX, centers, radii);
    for (int i = 0; COMBAT_CLUSTER_MAX > i; i++) {
        /* Each control stays in the free view and passes the 48 dp touch target. */
        assert(24.0f <= radii[i]);
        assert(phone.x <= centers[i].x - radii[i] && centers[i].x + radii[i] <= phone.x + phone.width);
        assert(phone.y <= centers[i].y - radii[i] && centers[i].y + radii[i] <= phone.y + phone.height);
        /* Two controls keep at least 8 px between their rims. */
        for (int k = 0; i > k; k++) assert(radii[i] + radii[k] + 8.0f <= Vector2Distance(centers[i], centers[k]));
        if (0 < i) assert(centers[i].x <= centers[0].x && centers[i].y <= centers[0].y);
    }
    assert(phone.x + phone.width - 60.0f < centers[0].x + radii[0]);

    /* A wide screen keeps the first control near the character. */
    combat_cluster_layout((Rectangle){ 0, 40, 1920, 940 }, (Vector2){ 960, 540 }, 1, centers, radii);
    assert(400.0f > Vector2Distance(centers[0], (Vector2){ 960, 540 }));
    assert(960.0f < centers[0].x && 540.0f < centers[0].y);
}

static void test_prediction(void) {
    Vector2 from = { 10.5f, 10.5f };
    /* Ahead of the facing wins over nearer behind; the preferred target wins while it lives in reach. */
    assert(AHEAD == auto_target_predict(from, DIRECTION_RIGHT, 8.0f, 0));
    assert(BEHIND == auto_target_predict(from, DIRECTION_LEFT, 8.0f, 0));
    assert(BEHIND == auto_target_predict(from, DIRECTION_RIGHT, 8.0f, BEHIND));
    assert(0 == auto_target_predict(from, DIRECTION_RIGHT, 0.5f, 0));
    assert(AHEAD == auto_target_at((Vector2){ 14.5f, 10.5f }) && 0 == auto_target_at((Vector2){ 12.5f, 10.5f }));
}

int main(void) {
    test_layout();

    g_local_player.id = SELF;
    place(0, SELF, 10.0f, 10.0f, NULL);
    place(1, AHEAD, 14.0f, 10.0f, "hostile");
    place(2, BEHIND, 8.0f, 10.0f, "hostile");
    g_game_state.bots[0] = AHEAD;
    g_game_state.bots[1] = BEHIND;
    g_game_state.bot_count = 2;
    test_prediction();

    push_skill("pistol", "shot", SKILL_ACTIVATION_MANUAL, SKILL_TARGET_DIRECTION);
    push_skill("emitter", "echo", SKILL_ACTIVATION_MANUAL, SKILL_TARGET_SELF);
    push_skill("rifle", "shot", SKILL_ACTIVATION_MANUAL, SKILL_TARGET_DIRECTION);
    push_skill("gel", "dose", SKILL_ACTIVATION_AUTOMATIC, SKILL_TARGET_SELF);
    push_skill("coin", "scatter", SKILL_ACTIVATION_EVENT, SKILL_TARGET_NONE);
    push_skill("knife", "throw", SKILL_ACTIVATION_MANUAL, SKILL_TARGET_DIRECTION);
    hold("pistol", true);
    hold("emitter", true);
    hold("rifle", true);
    hold("gel", true);
    hold("coin", true);
    hold("knife", false);
    tick();

    /* One control per manual skill of the active items, in loadout order. */
    Vector2 shot = control_at(0), echo = control_at(1);
    assert(combat_cluster_covers_point((int)shot.x, (int)shot.y));
    assert(combat_cluster_covers_point((int)echo.x, (int)echo.y));
    assert(!combat_cluster_claim(0, (Vector2){ 100.0f, 100.0f }));

    /* Auto target is on: a tap asks the server for the best target, with no preferred target yet. */
    assert(combat_cluster_auto_target());
    press(shot, shot, 3);
    expect_cast(1, "shot", SKILL_AIM_AUTO, 0);
    assert(0.0f == last_event()->payload.target_x && 0.0f == last_event()->payload.target_y);

    /* A press while the cast flies waits for its answer; the answer names the target the next tap prefers. */
    press(echo, echo, 2);
    assert(1 == s_event_count);
    ack("", true, BEHIND, LEFT, 500);
    tick();
    assert(1 == s_event_count);

    /* A cast waits a quarter second at most; one near the end of the cooldown waits for it and goes. */
    for (int i = 0; 16 > i; i++) tick();
    assert(1 == s_event_count);
    press(echo, echo, 1);
    assert(1 == s_event_count);
    for (int i = 0; 15 > i && 1 == s_event_count; i++) tick();
    expect_cast(2, "echo", SKILL_AIM_AUTO, BEHIND);

    /* A misfire answers with the floating text over the player; the target stays. */
    ack("misfire", true, BEHIND, NO_AIM, 0);
    assert(1 == s_misfires);

    /* A drag aims along the drag, auto target or not; a release over the cancel zone casts nothing. */
    press(shot, Vector2Add(shot, (Vector2){ -60.0f, 0.0f }), 4);
    expect_cast(3, "shot", SKILL_AIM_DIRECTION, 0);
    assert(-60.0f == last_event()->payload.target_x && 0.0f == last_event()->payload.target_y);
    ack("", true, 0, LEFT, 0);
    press(shot, (Vector2){ shot.x, shot.y - 82.0f }, 20);
    assert(3 == s_event_count);

    /* A lost finger casts nothing, wherever it was. */
    gesture(shot, Vector2Add(shot, (Vector2){ -60.0f, 0.0f }), 4, INPUT_POINTER_CANCELLED);
    gesture(shot, shot, 1, INPUT_POINTER_CANCELLED);
    assert(3 == s_event_count);

    /* A self skill ignores the drag. */
    press(echo, Vector2Add(echo, (Vector2){ 0.0f, -40.0f }), 4);
    expect_cast(4, "echo", SKILL_AIM_AUTO, BEHIND);
    ack("", true, BEHIND, NO_AIM, 0);

    /* Keys cast with the aim of a tap: the mouse far to the left never aims them. */
    key(KEY_ONE, false);
    expect_cast(5, "shot", SKILL_AIM_AUTO, BEHIND);
    ack("", true, BEHIND, LEFT, 0);

    /* Auto target off: a tap and a key fly along the facing, a drag along the drag. */
    combat_cluster_set_auto_target(false);
    press(shot, shot, 2);
    expect_cast(6, "shot", SKILL_AIM_FACING, 0);
    ack("", true, 0, RIGHT, 0);
    key(KEY_ONE, false);
    expect_cast(7, "shot", SKILL_AIM_FACING, 0);
    ack("", true, 0, RIGHT, 0);
    press(shot, Vector2Add(shot, (Vector2){ 0.0f, 50.0f }), 4);
    expect_cast(8, "shot", SKILL_AIM_DIRECTION, 0);
    ack("", true, 0, DOWN, 0);

    /* Shift with a key is the explicit aim at the mouse. */
    key(KEY_ONE, true);
    expect_cast(9, "shot", SKILL_AIM_DIRECTION, 0);
    assert(0.0f > last_event()->payload.target_x);
    ack("", true, 0, LEFT, 0);
    combat_cluster_set_auto_target(true);

    /* A tap on a hostile in the world makes it the preferred target; a tap on the ground keeps the target. */
    combat_cluster_on_world_tap((Vector2){ 14.5f, 10.5f });
    combat_cluster_on_world_tap((Vector2){ 12.5f, 10.5f });
    key(KEY_ONE, false);
    expect_cast(10, "shot", SKILL_AIM_AUTO, AHEAD);
    ack("", true, AHEAD, RIGHT, 0);

    /* A target that dies stops being the preferred target. */
    g_game_state.vitals[1].respawn_in = 5;
    tick();
    key(KEY_ONE, false);
    expect_cast(11, "shot", SKILL_AIM_AUTO, 0);
    ack("", true, 0, RIGHT, 0);
    g_game_state.vitals[1].respawn_in = 0;

    /* In stasis the cluster hides and takes no press. */
    s_stasis = true;
    tick();
    assert(!combat_cluster_claim(0, shot) && !combat_cluster_covers_point((int)shot.x, (int)shot.y));
    s_stasis = false;
    tick();

    /* A new session drops the target and the cast in flight: its late answer changes nothing. */
    combat_cluster_on_world_tap((Vector2){ 14.5f, 10.5f });
    press(shot, shot, 1);
    expect_cast(12, "shot", SKILL_AIM_AUTO, AHEAD);
    combat_cluster_reset();
    ack("misfire", true, AHEAD, RIGHT, 0);
    assert(1 == s_misfires);
    key(KEY_ONE, false);
    expect_cast(13, "shot", SKILL_AIM_AUTO, 0);

    puts("combat_cluster_test OK");
    return 0;
}
