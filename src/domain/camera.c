#include "camera.h"

#include "presentation_runtime.h"
#include "viewport.h"
#include "game_state.h"
#include "network/replication.h"
#include "util/log.h"

#include <math.h>
#include <raymath.h>
#include <stdbool.h>

/* Frame-rate independent exponential-smoothing constant for camera follow.
 * The original constant 0.15 was tuned at 60 fps → lambda ≈ 10. */
#define CAMERA_FOLLOW_LAMBDA 10.0f

/* On a mobile viewport (domain/viewport.h), pull the initial zoom back
 * several levels from the server-hinted default so more of the world is
 * visible on a small screen. One "level" matches the manual zoom-out step
 * used by the zoom button/scroll wheel (see ui_dispatch.c). */
#define CAMERA_MOBILE_ZOOM_STEP   0.9f
#define CAMERA_MOBILE_ZOOM_LEVELS 4

static Camera2D g_camera = {
    .offset   = {0.0f, 0.0f},
    .target   = {0.0f, 0.0f},
    .rotation = 0.0f,
    .zoom     = 1.0f,
};

/* Tracks whether the one-time mobile zoom pullback has already been applied,
 * independent of g_camera.zoom's own value — camera_init() runs more than
 * once (startup, then again once init_data arrives) and the pullback must
 * land exactly once, not compound on every re-init. */
static bool s_mobile_zoom_applied = false;

void camera_init(int screen_width, int screen_height) {
    Vector2 self = prediction_self_position();
    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 1.0f;
    float cx = (self.x + g_game_state.player.base.dims.x / 2.0f) * cell;
    float cy = (self.y + g_game_state.player.base.dims.y / 2.0f) * cell;

    g_camera.offset   = (Vector2){ screen_width / 2.0f, screen_height / 2.0f };
    g_camera.target   = (Vector2){ cx, cy };
    g_camera.rotation = 0.0f;
    if (g_camera.zoom <= 0.0f) {
        g_camera.zoom = presentation_runtime_camera_zoom();
    }
    if (!s_mobile_zoom_applied) {
        s_mobile_zoom_applied = true;
        if (viewport_is_mobile()) {
            g_camera.zoom *= powf(CAMERA_MOBILE_ZOOM_STEP, (float)CAMERA_MOBILE_ZOOM_LEVELS);
        }
    }
    LOG_INFO("camera_init target=(%.1f, %.1f) zoom=%.2f", cx, cy, g_camera.zoom);
}

void camera_resize(int screen_width, int screen_height) {
    g_camera.offset.x = screen_width / 2.0f;
    g_camera.offset.y = screen_height / 2.0f;
}

void camera_set_zoom(float zoom) {
    g_camera.zoom = Clamp(zoom, 0.1f, 5.0f);
}

float camera_zoom(void) {
    return g_camera.zoom;
}

void camera_zoom_by(float factor) {
    camera_set_zoom(g_camera.zoom * factor);
}

void camera_on_tick(float frame_dt) {
    float cell = g_game_state.cell_size > 0.0f ? g_game_state.cell_size : 12.0f;
    Vector2 self = g_game_state.player.base.interp_pos;
    float dx = self.x + g_game_state.player.base.dims.x / 2.0f;
    float dy = self.y + g_game_state.player.base.dims.y / 2.0f;
    Vector2 desired = { dx * cell, dy * cell };

    float blend = 1.0f - expf(-CAMERA_FOLLOW_LAMBDA * frame_dt);
    g_camera.target.x += (desired.x - g_camera.target.x) * blend;
    g_camera.target.y += (desired.y - g_camera.target.y) * blend;
}

Camera2D camera_get(void) {
    return g_camera;
}
