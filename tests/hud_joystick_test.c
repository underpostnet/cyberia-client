#include "ui/hud_joystick.h"
#include "input/pointer_gesture.h"

#include <assert.h>
#include <raylib.h>
#define RAYMATH_IMPLEMENTATION
#include <raymath.h>
#include <stddef.h>
#include <stdio.h>

#define BASE_X 100
#define BASE_Y 200
#define R ((int)POINTER_JOYSTICK_RADIUS)

static int s_rects;
static int s_left;   /* art pixels in the left half of the base, past half the radius */
static int s_right;
static int s_min_x, s_max_x, s_min_y, s_max_y;

void DrawRectangle(int x, int y, int w, int h, Color color) {
    assert(0 < w && 0 < h && 0 < color.a);
    if (x + w <= BASE_X - R / 2) s_left++;
    if (BASE_X + R / 2 <= x) s_right++;
    if (0 == s_rects++) {
        s_min_x = x; s_max_x = x + w; s_min_y = y; s_max_y = y + h;
    }
    if (x < s_min_x) s_min_x = x;
    if (x + w > s_max_x) s_max_x = x + w;
    if (y < s_min_y) s_min_y = y;
    if (y + h > s_max_y) s_max_y = y + h;
}

static void draw(pointer_gesture_t g) {
    s_rects = s_left = s_right = 0;
    hud_joystick_draw(&g);
}

int main(void) {
    pointer_gesture_t g = {
        .origin = { BASE_X, BASE_Y },
        .thumb  = { BASE_X, BASE_Y },
    };
    const enum pointer_state hidden[] = { POINTER_IDLE, POINTER_UI_PENDING, POINTER_PENDING, POINTER_SUPPRESSED };
    for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); i++) {
        g.state = hidden[i];
        draw(g);
        assert(0 == s_rects);
    }

    /* The base fills its radius around the origin; a centered thumb stays inside. */
    g.state = POINTER_JOYSTICK;
    draw(g);
    assert(0 < s_rects);
    assert(BASE_X - R == s_min_x && BASE_X + R == s_max_x);
    assert(BASE_Y - R == s_min_y && BASE_Y + R == s_max_y);

    /* A heading lights art pixels on its own side only. */
    g.thumb = (Vector2){ BASE_X + 30.0f, BASE_Y };
    draw(g);
    int left = s_left, right = s_right;
    g.dir_x = 1;
    draw(g);
    assert(left == s_left && right < s_right);

    puts("hud_joystick_test OK");
    return 0;
}
