#ifndef CYBERIA_UI_COMBAT_CLUSTER_H
#define CYBERIA_UI_COMBAT_CLUSTER_H

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

#include "world/object_layer.h"
#include "world/world_types.h"

/* Combat cluster: one round control per manual skill of the active items,
 * between the character and the right thumb. A control keeps its place while
 * the loadout stays the same.
 *
 * Every activation names its aim; the mouse never aims by itself.
 *
 *   - A tap, a key 1 to 4, or a hold with no drag: with AUTO TARGET on, the
 *     server aims at the best target; off, the skill flies along the facing.
 *   - A drag of a control: the skill flies along the drag. The release casts;
 *     a release over the cancel zone, Escape or the right button casts nothing.
 *   - Shift with a key: the skill flies toward the mouse.
 *   - A tap on a hostile in the world makes it the target an auto aim prefers.
 *
 * Each cast is one skill_activate event. The client never decides an outcome:
 * skill_ack answers each cast with the target and the facing the server took,
 * and the action cooldown of the server times the controls. A cast in the last
 * moment of the cooldown is queued, shows as queued, and goes when it ends. */

#define COMBAT_CLUSTER_MAX 4

/* The answer of the server to one cast. */
typedef struct {
    uint32_t  seq;
    char      skill_id[MAX_ITEM_ID_LENGTH];
    char      reason[32];   /* "" for a cast */
    bool      accepted;     /* the attempt spent the cooldown: a cast or a misfire */
    EntityID  target_id;    /* the target an auto aim took, 0 for none */
    Vector2   aim;          /* the unit vector the skill flew along, at any angle; zero for none */
    int       ready_in_ms;
} SkillAck;

/* Takes a press on a control: input_claim_fn. The press stays with its control
 * while another pointer steers the joystick. */
bool combat_cluster_claim(int pointer, Vector2 position);

/* Follow the held pointer, the keys, the target and the queued cast. Call once
 * per frame after input_queue_on_tick. */
void combat_cluster_on_tick(double dt);

void combat_cluster_on_ack(const SkillAck* ack);

/* A tap on the world at `at` (cells): a hostile there becomes the target. */
void combat_cluster_on_world_tap(Vector2 at);

/* Drop the press, the target, the queued cast and the cast in flight: a new session. */
void combat_cluster_reset(void);

bool combat_cluster_covers_point(int x, int y);

/* The target reticle, the aim guide, the cancel zone and the controls, in screen space. */
void combat_cluster_draw(void);

/* AUTO TARGET: on, a tap aims at the best target; off, along the facing. */
void combat_cluster_set_auto_target(bool on);
bool combat_cluster_auto_target(void);

/* The centers and radii of `count` controls. The first sits in the bottom
 * right corner of `view`, but never far from `anchor`, the screen point of the
 * character; the others ring it from the left to the top. */
void combat_cluster_layout(Rectangle view, Vector2 anchor, int count,
                           Vector2 centers[COMBAT_CLUSTER_MAX], float radii[COMBAT_CLUSTER_MAX]);

#endif /* CYBERIA_UI_COMBAT_CLUSTER_H */
