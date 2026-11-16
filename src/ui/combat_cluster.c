#include "combat_cluster.h"

#include "floating_combat_text.h"
#include "hud_side_stack.h"
#include "inventory_bar.h"
#include "ol_as_animated_ico.h"
#include "ol_stack_ico.h"
#include "session_gate.h"
#include "skill_preview.h"
#include "text.h"
#include "toolbar.h"
#include "ui_icon.h"
#include "ui_state.h"

#include "content/presentation_runtime.h"
#include "content/stat_contract_generated.h"
#include "fx/fx_combat_aim.h"
#include "fx/fx_shapes.h"
#include "input/input.h"
#include "net/client_event.h"
#include "sim/auto_target.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "utils.h"
#include "view/camera.h"
#include "view/local_player_view.h"
#include "view/viewport.h"
#include "world/object_layer.h"
#include "world/object_layers_management.h"
#include "world/world_types.h"

#include <assert.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <string.h>

#define CC_PX            4.0f    /* side of one art pixel, as the joystick */
#define CC_PRIMARY_R     36.0f   /* radius of the first control */
#define CC_SECONDARY_R   26.0f   /* a 52 px control passes the 48 dp touch target */
#define CC_GAP           10.0f   /* between two controls */
#define CC_MARGIN        14.0f   /* from the edges of the free view */
#define CC_REACH         360.0f  /* most pixels between the character and the first control */
#define CC_HIT_SLOP      6.0f    /* a press this far past a rim still takes the control */
#define CC_AIM_DRAG_PX   14.0f   /* travel that makes a press a manual aim */
#define CC_AIM_HOLD_S    0.2     /* hold that shows the aim of a press */
#define CC_BUFFER_S      0.25    /* a cast this close to the end of the cooldown is queued */
#define CC_PENDING_S     1.5     /* a cast with no answer stops waiting after this */
#define CC_FLASH_S       0.45f
#define CC_READY_PING_S  0.35f
#define CC_SUBMIT_S      0.35f   /* the guide of an answered cast fades out in this time */
#define CC_GUIDE_START   0.6f    /* cells between the centre of the player and its guide */
#define CC_GUIDE_WIDTH   0.5f    /* cells */
#define CC_EDGE_PX       8.0f    /* a guide stops this far from the edge of the screen */
#define CC_OCTANT        (PI / 4.0f)

static const Color CC_SHADOW    = {   6,   8,  12, 110 };
static const Color CC_OUTLINE   = {  16,  18,  24, 230 };
static const Color CC_FILL      = {  30,  34,  46, 190 };
static const Color CC_SHADE     = {   8,  10,  16, 170 };  /* the part of the cooldown left */
static const Color CC_RIM_IDLE  = {  92,  98, 112, 200 };
static const Color CC_CAST      = { 140, 255, 190, 255 };
static const Color CC_MISFIRE   = { 196, 160, 255, 255 };
static const Color CC_REFUSED   = { 230, 120,  90, 255 };

static const int CC_KEYS[COMBAT_CLUSTER_MAX] = { KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR };

/* How one cast aims, as the event carries it. */
typedef struct {
    skill_aim_t mode;
    Vector2     direction;  /* SKILL_AIM_DIRECTION only */
} Aim;

typedef struct {
    char        skill_id[MAX_ITEM_ID_LENGTH];
    char        icon_id[MAX_ITEM_ID_LENGTH]; /* the entity the skill summons, else its item */
    SkillTarget targeting;
    float       reach;
    Vector2     center;
    float       radius;
} Control;

static struct {
    Control controls[COMBAT_CLUSTER_MAX];
    int     count;
    bool    shown;
    Vector2 cancel;
    float   cancel_r;
    bool    auto_target;
    /* The target an auto aim prefers: the one the server took, or the one the
     * player tapped. shown_target is the target a tap takes now. */
    EntityID target;
    float    target_s;
    EntityID shown_target;
    /* The press that holds a pointer. */
    bool    pressing;
    int     pointer;
    int     control;
    Vector2 origin;
    Vector2 at;
    double  held_s;
    bool    aiming;
    /* The cast in flight, and the cast queued for the end of the cooldown. */
    bool        pending;
    uint32_t    pending_seq;
    double      pending_s;
    char        pending_skill[MAX_ITEM_ID_LENGTH];
    skill_aim_t pending_aim;
    bool        queued;
    char        queued_skill[MAX_ITEM_ID_LENGTH];
    Aim         queued_aim;
    double      queued_s;
    /* The answer flash of one control, the ping of the end of the cooldown,
     * and the fading guide of the last cast. */
    char      flash_skill[MAX_ITEM_ID_LENGTH];
    Color     flash_color;
    float     flash_s;
    bool      was_ready;
    float     ready_s;
    Vector2   submitted;    /* zero for none */
    float     submitted_s;
} s = {
    .auto_target = true, .flash_s = CC_FLASH_S, .ready_s = CC_READY_PING_S,
    .submitted_s = CC_SUBMIT_S,
};

void combat_cluster_layout(Rectangle view, Vector2 anchor, int count,
                           Vector2 centers[COMBAT_CLUSTER_MAX], float radii[COMBAT_CLUSTER_MAX]) {
    assert(0 <= count && COMBAT_CLUSTER_MAX >= count);
    if (0 == count) return;
    float   r0     = CC_PRIMARY_R;
    float   r1     = CC_SECONDARY_R;
    Vector2 corner = { view.x + view.width - CC_MARGIN - r0, view.y + view.height - CC_MARGIN - r0 };
    Vector2 to     = Vector2Subtract(corner, anchor);
    float   far    = Vector2Length(to);
    centers[0] = CC_REACH < far ? Vector2Add(anchor, Vector2Scale(to, CC_REACH / far)) : corner;
    radii[0]   = r0;
    /* Left, upper left, then up: the thumb reaches each from the first control. */
    float ring = r0 + r1 + 2.0f * CC_GAP;
    for (int i = 1; count > i; i++) {
        float angle = PI + (float)(i - 1) * CC_OCTANT;
        centers[i]  = (Vector2){ centers[0].x + cosf(angle) * ring, centers[0].y + sinf(angle) * ring };
        radii[i]    = r1;
    }
}

static int find_control(const Control* controls, int count, const char* skill_id) {
    for (int i = 0; count > i; i++) {
        if (0 == strcmp(controls[i].skill_id, skill_id)) return i;
    }
    return -1;
}

/* The manual skills of the active items, in loadout order, one control per skill id. */
static int manual_skills(Control out[COMBAT_CLUSTER_MAX]) {
    int n = 0;
    for (int i = 0; g_local_player.inventory_count > i; i++) {
        const ObjectLayerState* stack = &g_local_player.inventory[i];
        if (!stack->active) continue;
        for (int k = 0; ui_state_skill_count() > k && COMBAT_CLUSTER_MAX > n; k++) {
            const UiSkillEntry* skill = ui_state_skill_at(k);
            if (SKILL_ACTIVATION_MANUAL != skill->activation || 0 != strcmp(skill->trigger_item_id, stack->item_id)) continue;
            if (0 <= find_control(out, n, skill->skill_id)) continue;
            Control* control = &out[n++];
            const char* icon = skill_preview_summon_id(skill);
            copy_str(control->skill_id, sizeof control->skill_id, skill->skill_id);
            copy_str(control->icon_id, sizeof control->icon_id, NULL != icon ? icon : stack->item_id);
            control->targeting = skill->targeting;
            control->reach     = skill->reach;
        }
    }
    return n;
}

/* The player stands in the world and can act. */
static bool playing(void) {
    if (0 == g_local_player.id || 0 > game_state_slot(g_local_player.id) || session_gate_active()) return false;
    return !local_player_in_stasis() && 0 == game_state_vitals(g_local_player.id)->respawn_in;
}

/* The screen the HUD leaves to the world: under the toolbar, over the bar, left of the side column. */
static Rectangle free_view(void) {
    float top    = toolbar_height();
    float bottom = (float)GetScreenHeight() - inventory_bar_visible_height();
    return (Rectangle){ 0.0f, top, hud_side_stack_left(), bottom - top };
}

/* The centre of the player in world cells. */
static Vector2 player_cell(void) {
    Vector2 at   = game_state_own_render()->interp_pos;
    Vector2 size = game_state_own_transform()->size;
    return (Vector2){ at.x + size.x * 0.5f, at.y + size.y * 0.5f };
}

static Vector2 cell_to_screen(Vector2 cell) {
    return GetWorldToScreen2D(Vector2Scale(cell, world_cell_size()), camera_get());
}

static Vector2 facing_vector(Direction facing) {
    return (Vector2){ CYBERIA_DIRECTION_VECTORS[facing][0], CYBERIA_DIRECTION_VECTORS[facing][1] };
}

static double cooldown_left(double now) {
    return (double)(1.0f - local_player_action_charge(now)) * (double)local_player_action_cooldown();
}

static bool directional(const Control* control) { return SKILL_TARGET_DIRECTION == control->targeting; }

/* The first control that aims at a direction: its reach bounds the target. */
static const Control* primary_directional(void) {
    for (int i = 0; s.count > i; i++) {
        if (directional(&s.controls[i])) return &s.controls[i];
    }
    return NULL;
}

/* The aim of a tap, a key or a hold with no drag. */
static Aim tap_aim(void) { return (Aim){ .mode = s.auto_target ? SKILL_AIM_AUTO : SKILL_AIM_FACING }; }

/* The aim of the press: its drag for a directional skill past the slop, else a tap. */
static Aim press_aim(const Control* control) {
    Vector2 drag = Vector2Subtract(s.at, s.origin);
    if (directional(control) && CC_AIM_DRAG_PX < Vector2Length(drag)) return (Aim){ SKILL_AIM_DIRECTION, drag };
    return tap_aim();
}

static void flash(const char* skill_id, Color color) {
    copy_str(s.flash_skill, sizeof s.flash_skill, skill_id);
    s.flash_color = color;
    s.flash_s     = 0.0f;
}

static void send(const char* skill_id, Aim aim) {
    client_event_payload_t p = {
        .aim       = aim.mode,
        .target_x  = aim.direction.x,
        .target_y  = aim.direction.y,
        .entity_id = SKILL_AIM_AUTO == aim.mode ? s.target : 0,
    };
    copy_str(p.code, sizeof p.code, skill_id);
    client_event_push(CLIENT_EVENT_SKILL_ACTIVATE, p);
    int count = 0;
    const client_event_t* events = client_event_list(&count);
    s.pending     = true;
    s.pending_seq = events[count - 1].sequence;
    s.pending_s   = 0.0;
    s.pending_aim = aim.mode;
    copy_str(s.pending_skill, sizeof s.pending_skill, skill_id);
}

/* Send a cast now, or queue it behind the cast in flight or for the last part
 * of the cooldown. A cast further from the end of the cooldown is refused here. */
static void request_cast(const char* skill_id, Aim aim) {
    double left = cooldown_left(GetTime());
    if (!s.pending && 0.0 >= left) {
        send(skill_id, aim);
    } else if (s.pending || CC_BUFFER_S >= left) {
        s.queued     = true;
        s.queued_s   = 0.0;
        s.queued_aim = aim;
        copy_str(s.queued_skill, sizeof s.queued_skill, skill_id);
    } else {
        flash(skill_id, CC_REFUSED);
    }
}

static bool over_cancel(Vector2 at) { return s.cancel_r + CC_HIT_SLOP >= Vector2Distance(at, s.cancel); }

bool combat_cluster_claim(int pointer, Vector2 position) {
    if (!s.shown) return false;
    for (int i = 0; s.count > i; i++) {
        if (s.controls[i].radius + CC_HIT_SLOP < Vector2Distance(position, s.controls[i].center)) continue;
        /* A second press on a control is held too, so it never walks the player. */
        if (s.pressing) return true;
        s.pressing = true;
        s.pointer  = pointer;
        s.control  = i;
        s.origin   = position;
        s.at       = position;
        s.held_s   = 0.0;
        s.aiming   = false;
        return true;
    }
    return false;
}

/* Lay the controls out again; a press keeps the skill it took. */
static void refresh(void) {
    Control controls[COMBAT_CLUSTER_MAX];
    int count = playing() ? manual_skills(controls) : 0;
    Vector2 centers[COMBAT_CLUSTER_MAX];
    float   radii[COMBAT_CLUSTER_MAX];
    combat_cluster_layout(free_view(), camera_get().offset, count, centers, radii);
    float top = (float)GetScreenHeight();
    for (int i = 0; count > i; i++) {
        controls[i].center = centers[i];
        controls[i].radius = radii[i];
        top = fminf(top, centers[i].y - radii[i]);
    }
    if (s.pressing) {
        s.control  = find_control(controls, count, s.controls[s.control].skill_id);
        s.pressing = 0 <= s.control;
    }
    memcpy(s.controls, controls, sizeof controls);
    s.count = count;
    s.shown = 0 < count;
    if (!s.shown) return;
    s.cancel_r = CC_SECONDARY_R;
    s.cancel   = (Vector2){ centers[0].x, fmaxf(top - 2.0f * CC_GAP - s.cancel_r, free_view().y + s.cancel_r) };
}

/* Keep the target while it lives in reach, and find the target a tap takes now. */
static void track_target(void) {
    const Control* primary = s.shown ? primary_directional() : NULL;
    if (NULL == primary) {
        s.target = s.shown_target = 0;
        return;
    }
    Vector2   from   = player_cell();
    Direction facing = local_player_view_direction();
    if (0 != s.target && s.target != auto_target_predict(from, facing, primary->reach, s.target)) s.target = 0;
    s.shown_target = s.auto_target ? auto_target_predict(from, facing, primary->reach, s.target) : 0;
}

/* Follow the held press. Its release casts, unless it lifts over the cancel
 * zone; a cancelled press casts nothing. */
static void follow_press(double dt) {
    if (!s.pressing) return;
    Vector2 at = s.at;
    input_pointer_state_e state = input_pointer_at(s.pointer, &at);
    s.at = at;
    if (INPUT_POINTER_HELD == state) {
        s.held_s += dt;
        s.aiming = s.aiming || CC_AIM_DRAG_PX < Vector2Distance(at, s.origin) || CC_AIM_HOLD_S <= s.held_s;
        return;
    }
    s.pressing = false;
    if (INPUT_POINTER_RELEASED != state || (s.aiming && over_cancel(s.at))) return;
    const Control* control = &s.controls[s.control];
    request_cast(control->skill_id, press_aim(control));
}

void combat_cluster_on_tick(double dt) {
    refresh();
    if (!s.shown) s.queued = false;
    if (s.pending && CC_PENDING_S < (s.pending_s += dt)) s.pending = false;
    s.flash_s     += (float)dt;
    s.ready_s     += (float)dt;
    s.target_s    += (float)dt;
    s.submitted_s += (float)dt;
    double now = GetTime();
    bool ready = 0.0 >= cooldown_left(now);
    if (ready && !s.was_ready) s.ready_s = 0.0f;
    s.was_ready = ready;

    track_target();
    follow_press(dt);
    if (s.pressing && IsKeyPressed(KEY_ESCAPE)) s.pressing = false;
    /* A key casts with the aim of a tap; only Shift aims a directional skill at the mouse. */
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    for (int i = 0; s.count > i; i++) {
        if (!IsKeyPressed(CC_KEYS[i])) continue;
        Aim aim = tap_aim();
        if (shift && directional(&s.controls[i])) {
            Vector2 mouse = Vector2Scale(GetScreenToWorld2D(GetMousePosition(), camera_get()), 1.0f / world_cell_size());
            Vector2 to    = Vector2Subtract(mouse, player_cell());
            if (0.0f != to.x || 0.0f != to.y) aim = (Aim){ SKILL_AIM_DIRECTION, to };
        }
        request_cast(s.controls[i].skill_id, aim);
    }

    if (!s.queued || s.pending) return;
    if (ready) {
        s.queued = false;
        send(s.queued_skill, s.queued_aim);
    } else if (CC_BUFFER_S < (s.queued_s += dt)) {
        s.queued = false;
        flash(s.queued_skill, CC_REFUSED);
    }
}

void combat_cluster_on_ack(const SkillAck* ack) {
    assert(ack);
    local_player_set_action_ready_at(GetTime() + (double)ack->ready_in_ms / 1000.0);
    if (!s.pending || ack->seq != s.pending_seq) return;
    s.pending = false;
    if (ack->accepted && (0.0f != ack->aim.x || 0.0f != ack->aim.y)) {
        s.submitted   = ack->aim;
        s.submitted_s = 0.0f;
    }
    if (ack->accepted && SKILL_AIM_AUTO == s.pending_aim) {
        s.target   = ack->target_id;
        s.target_s = 0.0f;
    }
    if ('\0' == ack->reason[0]) {
        flash(ack->skill_id, CC_CAST);
    } else if (0 == strcmp(ack->reason, "misfire")) {
        flash(ack->skill_id, CC_MISFIRE);
        if (playing()) {
            Vector2 at = player_cell();
            LocalFctEvent misfire = { .world_x = at.x, .world_y = game_state_own_render()->interp_pos.y,
                                      .type = FCT_TYPE_MISFIRE };
            local_player_fct_push(&misfire);
        }
    } else {
        flash(ack->skill_id, CC_REFUSED);
    }
}

void combat_cluster_on_world_tap(Vector2 at) {
    EntityID id = auto_target_at(at);
    if (0 == id) return;
    s.target   = id;
    s.target_s = 0.0f;
}

void combat_cluster_reset(void) {
    s.pressing       = false;
    s.pending        = false;
    s.queued         = false;
    s.target         = 0;
    s.shown_target   = 0;
    s.flash_skill[0] = '\0';
    s.submitted_s    = CC_SUBMIT_S;
}

bool combat_cluster_covers_point(int x, int y) {
    if (!s.shown) return false;
    for (int i = 0; s.count > i; i++) {
        if (s.controls[i].radius >= Vector2Distance((Vector2){ (float)x, (float)y }, s.controls[i].center)) return true;
    }
    return false;
}

void combat_cluster_set_auto_target(bool on) { s.auto_target = on; }
bool combat_cluster_auto_target(void) { return s.auto_target; }

/* The pixels a guide from `from` runs along unit `dir` before the edge of the screen. */
static float edge_distance(Vector2 from, Vector2 dir) {
    float tx = 0.0f < dir.x ? ((float)GetScreenWidth() - from.x) / dir.x : 0.0f > dir.x ? -from.x / dir.x : INFINITY;
    float ty = 0.0f < dir.y ? ((float)GetScreenHeight() - from.y) / dir.y : 0.0f > dir.y ? -from.y / dir.y : INFINITY;
    return fminf(tx, ty) - CC_EDGE_PX;
}

static void draw_target_reticle(EntityID id, fx_reticle_style_e style, float age, float t) {
    Vector2 size = game_state_transform(id)->size;
    float   half = fmaxf(size.x, size.y) * world_cell_size() * camera_get().zoom * 0.5f + CC_PX;
    fx_combat_reticle(cell_to_screen(auto_target_center(id)), half, style, age, t);
}

/* The guide of a directional aim along unit `dir`: a manual reticle at its end,
 * or a locked one on its target. */
static void draw_guide(Vector2 from, const Control* control, Vector2 dir, EntityID target, Color tone, bool cancel, float t) {
    float cell   = world_cell_size() * camera_get().zoom;
    float length = fminf(control->reach * cell, edge_distance(from, dir));
    fx_combat_guide(from, dir, CC_GUIDE_START * cell, length, CC_GUIDE_WIDTH * cell, tone, t);
    if (cancel) return;
    if (0 != target) draw_target_reticle(target, FX_RETICLE_LOCKED, s.target_s, t);
    else fx_combat_reticle(Vector2Add(from, Vector2Scale(dir, length)), CC_GUIDE_WIDTH * cell, FX_RETICLE_MANUAL, 0.0f, t);
}

/* The aim of the held press, from the character, and the cancel zone. The
 * guide takes the exact angle the cast sends. */
static void draw_aim(Vector2 from, float t) {
    const Control* control = &s.controls[s.control];
    bool cancel = over_cancel(s.at);
    if (directional(control)) {
        Aim       aim    = press_aim(control);
        EntityID  target = SKILL_AIM_AUTO == aim.mode ? s.shown_target : 0;
        Vector2 dir = facing_vector(local_player_view_direction());
        if (SKILL_AIM_DIRECTION == aim.mode) {
            dir = Vector2Normalize(aim.direction);
        } else if (0 != target) {
            dir = Vector2Normalize(Vector2Subtract(auto_target_center(target), player_cell()));
        }
        Color tone = cancel ? FX_AIM_CANCEL : 0 != target ? FX_AIM_LOCK : FX_AIM_ACCENT;
        draw_guide(from, control, dir, target, tone, cancel, t);
    } else {
        float r = world_cell_size() * camera_get().zoom * 0.9f + 2.0f * sinf(t * 8.0f);
        fx_shape_pixel_ring(from, r, r - CC_PX, CC_PX, cancel ? FX_AIM_CANCEL : FX_AIM_ACCENT);
    }
    fx_shape_pixel_ring(s.cancel, s.cancel_r, 0.0f, CC_PX, CC_FILL);
    fx_shape_pixel_ring(s.cancel, s.cancel_r, s.cancel_r - CC_PX, CC_PX, cancel ? FX_AIM_CANCEL : CC_OUTLINE);
    ui_icon_draw("close-yellow", s.cancel.x, s.cancel.y, (int)s.cancel_r, false, 0.0f);
}

static Color rim_color(const Control* control, bool ready, bool pressed, float t) {
    if (pressed && s.aiming) return FX_AIM_ACCENT;
    if (s.pending && 0 == strcmp(s.pending_skill, control->skill_id))
        return Fade(FX_AIM_ACCENT, 0.5f + 0.5f * sinf(t * 18.0f));
    if (CC_FLASH_S > s.flash_s && 0 == strcmp(s.flash_skill, control->skill_id))
        return ColorLerp(s.flash_color, ready ? FX_AIM_LOCK : CC_RIM_IDLE, s.flash_s / CC_FLASH_S);
    if (ready) return Fade(FX_AIM_LOCK, 0.8f + 0.2f * sinf(t * 3.0f));
    return CC_RIM_IDLE;
}

static void draw_control(int index, float charge, float t) {
    const Control* control = &s.controls[index];
    bool    pressed = s.pressing && s.control == index;
    bool    ready   = 1.0f <= charge;
    float   r       = roundf(control->radius * (pressed ? 0.92f : 1.0f));
    Vector2 at      = { roundf(control->center.x), roundf(control->center.y) };

    fx_shape_pixel_ring((Vector2){ at.x, at.y + CC_PX }, r, 0.0f, CC_PX, CC_SHADOW);
    fx_shape_pixel_ring(at, r, r - CC_PX, CC_PX, CC_OUTLINE);
    fx_shape_pixel_ring(at, r - CC_PX, 0.0f, CC_PX, CC_FILL);

    float side = r * 1.25f;
    ObjectLayerState icon = { .active = true, .quantity = 1 };
    copy_str(icon.item_id, sizeof icon.item_id, control->icon_id);
    ol_stack_ico_draw(obj_layers_mgr_get(), &icon, 1, (int)(at.x - side * 0.5f), (int)(at.y - side * 0.5f), (int)side,
                      OL_ICO_DEFAULT_DIR, 0, ready ? WHITE : GRAY);

    if (!ready) {
        fx_shape_pixel_arc(at, r - CC_PX, 0.0f, CC_PX, -0.5f * PI + charge * 2.0f * PI, (1.0f - charge) * 2.0f * PI, CC_SHADE);
        float seconds = (1.0f - charge) * local_player_action_cooldown();
        if (1.0f <= seconds) {
            char text[8];
            snprintf(text, sizeof text, "%d", (int)ceilf(seconds));
            int size = (int)(r * 0.7f);
            text_draw_outlined(text, (int)at.x - text_measure_compat(text, size) / 2, (int)at.y - size / 2, size, WHITE,
                               BLACK, 1);
        }
    }
    fx_shape_pixel_ring(at, r - CC_PX, r - 2.0f * CC_PX, CC_PX, rim_color(control, ready, pressed, t));

    /* A queued cast: three gold segments turn around the control until it goes. */
    if (s.queued && 0 == strcmp(s.queued_skill, control->skill_id)) {
        for (int k = 0; 3 > k; k++) {
            float from = t * 6.0f + (float)k * 2.0f * PI / 3.0f;
            fx_shape_pixel_arc(at, r + 2.0f * CC_PX, r + CC_PX, CC_PX, from, CC_OCTANT, FX_AIM_ACCENT);
        }
    }
    if (ready && CC_READY_PING_S > s.ready_s) {
        float grow = s.ready_s / CC_READY_PING_S;
        float ping = r + 4.0f * CC_PX * grow;
        fx_shape_pixel_ring(at, ping, ping - CC_PX, CC_PX, Fade(FX_AIM_LOCK, 1.0f - grow));
    }
    if (!viewport_is_mobile()) {
        char key[2] = { (char)('1' + index), '\0' };
        text_draw_outlined(key, (int)(at.x + r * 0.45f), (int)(at.y + r * 0.3f), 12, WHITE, BLACK, 1);
    }
}

void combat_cluster_draw(void) {
    if (!s.shown) return;
    float   t    = (float)GetTime();
    Vector2 from = cell_to_screen(player_cell());
    const Control* primary = primary_directional();
    if (NULL != primary && (0.0f != s.submitted.x || 0.0f != s.submitted.y) && CC_SUBMIT_S > s.submitted_s) {
        Color fade = Fade(FX_AIM_ACCENT, 1.0f - s.submitted_s / CC_SUBMIT_S);
        draw_guide(from, primary, s.submitted, 0, fade, true, t);
    }
    if (s.pressing && s.aiming) {
        draw_aim(from, t);
    } else if (0 != s.shown_target) {
        bool locked = s.shown_target == s.target;
        draw_target_reticle(s.shown_target, locked ? FX_RETICLE_LOCKED : FX_RETICLE_CANDIDATE, s.target_s, t);
    }
    float charge = local_player_action_charge(GetTime());
    for (int i = 0; s.count > i; i++) draw_control(i, charge, t);
}
