/* The reconcile of the own player: a server state that walked the same events
 * replays to the live prediction, so the view sees no correction. */

#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <cJSON.h>

#include "config.h"
#include "net/client_event.h"
#include "sim/game_state.h"
#include "sim/local_player.h"
#include "sim/replication.h"
#include "sim/self_motion.h"
#include "world/object_layers_management.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

double GetTime(void) { return 0.0; }

/* The sequences of the last batch sent. */
static int s_sent_first, s_sent_count;
bool network_send(cJSON* msg) {
    const cJSON* events = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(msg, "payload"), "events");
    s_sent_count = cJSON_GetArraySize(events);
    const cJSON* first = cJSON_GetArrayItem(events, 0);
    const cJSON* payload = cJSON_GetObjectItemCaseSensitive(first, "payload");
    s_sent_first = s_sent_count ? (int)cJSON_GetObjectItemCaseSensitive(payload, "seq")->valuedouble : 0;
    cJSON_Delete(msg);
    return true;
}
ObjectLayer* lookup_cached_layer(const char* item_id) { return NULL; }

#define SPEED 5.0f

static void expect_self(float x, float y, const char* what) {
    Vector2 pos = prediction_self()->pos;
    if (1e-4f < fabsf(pos.x - x) || 1e-4f < fabsf(pos.y - y)) {
        printf("%s: pos (%.5f, %.5f), want (%.5f, %.5f)\n", what, pos.x, pos.y, x, y);
        assert(false);
    }
}

static void expect_prev(float x, float y, const char* what) {
    Vector2 pos = prediction_prev()->pos;
    if (1e-4f < fabsf(pos.x - x) || 1e-4f < fabsf(pos.y - y)) {
        printf("%s: prev (%.5f, %.5f), want (%.5f, %.5f)\n", what, pos.x, pos.y, x, y);
        assert(false);
    }
}

static void step_frames(int n) {
    for (int i = 0; n > i; i++) {
        session_frame_advance();
        prediction_step(session_frame());
    }
    client_event_flush();
}

int main(void) {
    g_local_player.id = 1;
    int slot = game_state_spawn(1);
    g_game_state.mask[slot] |= COMP_MOTION | COMP_VITALS;
    local_player_set_move_speed(SPEED);

    self_motion_t idle = { .pos = { 2, 2 } };
    prediction_on_snapshot(&idle, 0, 0);
    prediction_reconcile();
    expect_self(2, 2, "first state");
    expect_prev(2, 2, "first state");

    /* A tap in frame 0 takes effect in step 1. */
    client_event_push(CLIENT_EVENT_PLAYER_ACTION, (client_event_payload_t){ .target_x = 8, .target_y = 2 });
    step_frames(10);
    const float live = 2.0f + 10.0f * SPEED / TICK_RATE_HZ;
    expect_self(live, 2, "prediction");

    /* No event consumed yet: the state stands for the step before the tap. */
    prediction_on_snapshot(&idle, 0, 0);
    prediction_reconcile();
    expect_self(live, 2, "state before the tap");
    expect_prev(live - SPEED / TICK_RATE_HZ, 2, "state before the tap");

    /* The server walks its own path, one node per cell, from the same step. */
    self_motion_t server = { .pos = { 2, 2 } };
    for (int x = 3; 8 >= x; x++) server.path[server.path_count++] = (Vector2){ (float)x, 2 };
    for (int i = 0; 4 > i; i++) self_motion_step(&server, SPEED, TICK_DURATION_S);
    prediction_on_snapshot(&server, 4, 1);
    prediction_reconcile();
    expect_self(live, 2, "consumed state");

    int count = 0;
    client_event_list(&count);
    assert(0 == count);

    /* input_resend: the server queued nothing after a batch it never got, so
     * the events from the gap go again, once, with the next flush. */
    for (int i = 0; 3 > i; i++)
        client_event_push(CLIENT_EVENT_PLAYER_ACTION, (client_event_payload_t){ .target_x = 3, .target_y = 2 });
    client_event_flush();
    assert(2 == s_sent_first && 3 == s_sent_count);
    s_sent_count = 0;
    client_event_flush();
    assert(0 == s_sent_count);
    client_event_resend(3);
    client_event_flush();
    assert(3 == s_sent_first && 2 == s_sent_count);
    client_event_resend(9);
    s_sent_count = 0;
    client_event_flush();
    assert(0 == s_sent_count);

    printf("replication_test OK\n");
    return 0;
}
