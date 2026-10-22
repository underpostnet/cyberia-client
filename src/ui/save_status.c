#include "save_status.h"

#include "text.h"
#include "ui_icon.h"
#include "view/viewport.h"

#include <raylib.h>
#include <stdio.h>
#include <string.h>

/* A save this old shows; a normal one lands in tens of milliseconds. */
#define SLOW_SAVE_MS 3000
/* How long a note shows, and how long the line fades in or out. */
#define NOTE_S 3.0f
#define FADE_S 0.25f

typedef enum { NOTE_NONE, NOTE_SAVED, NOTE_RESUMED, NOTE_KEPT } Note;

static const Color C_SLOW     = { 40, 48, 70, 230 };
static const Color C_DELAYED  = { 128, 86, 20, 235 };
static const Color C_RECOVERY = { 120, 34, 30, 240 };
static const Color C_NOTE     = { 26, 96, 64, 230 };

static struct {
    Durability durability;
    int        unsaved_ms;
    bool       warned; /* a condition line showed since the last save */
    Note       note;
    float      note_age;
    float      alpha;
    char       text[96];
} s;

static bool shows_condition(void) {
    return DURABILITY_DEGRADED == s.durability || DURABILITY_RECOVERY == s.durability ||
           (DURABILITY_SAVING == s.durability && SLOW_SAVE_MS <= s.unsaved_ms);
}

void save_status_observe(Durability durability, int unsaved_ms) {
    s.durability = durability;
    s.unsaved_ms = unsaved_ms;
    if (shows_condition()) {
        s.warned = true;
    } else if (DURABILITY_SAVED == durability && s.warned) {
        s.warned = false;
        s.note = NOTE_SAVED;
        s.note_age = 0.0f;
    }
}

void save_status_joined(bool resumed, bool adopted) {
    if (!resumed && !adopted) return;
    s.note = adopted ? NOTE_KEPT : NOTE_RESUMED;
    s.note_age = 0.0f;
}

void save_status_reset(void) { memset(&s, 0, sizeof s); }

/* The text, colour and icon of the line now; false when nothing shows. */
static bool line(Color* bg, const char** icon) {
    int minutes = s.unsaved_ms / 60000;
    switch (s.durability) {
        case DURABILITY_RECOVERY:
            if (0 < minutes) snprintf(s.text, sizeof s.text, "Last %d min not saved yet - trades paused", minutes);
            else snprintf(s.text, sizeof s.text, "Latest changes not saved yet - trades paused");
            *bg = C_RECOVERY, *icon = "cloud";
            return true;
        case DURABILITY_DEGRADED:
            snprintf(s.text, sizeof s.text, "Saving delayed - retrying");
            *bg = C_DELAYED, *icon = "reload";
            return true;
        case DURABILITY_SAVING:
            if (SLOW_SAVE_MS > s.unsaved_ms) break;
            snprintf(s.text, sizeof s.text, "Saving...");
            *bg = C_SLOW, *icon = "cloud";
            return true;
        case DURABILITY_SAVED: break;
    }
    if (NOTE_NONE == s.note || NOTE_S <= s.note_age) return false;
    snprintf(s.text, sizeof s.text, "%s",
             NOTE_SAVED == s.note  ? "Progress saved"
             : NOTE_KEPT == s.note ? "Latest progress kept - saving"
                                   : "Reconnected");
    *bg = C_NOTE, *icon = "check";
    return true;
}

void save_status_draw(int screen_width, float top) {
    float dt = GetFrameTime();
    if (NOTE_NONE != s.note) s.note_age += dt;
    Color bg = { 0 };
    const char* icon = NULL;
    bool on = line(&bg, &icon);
    s.alpha += (on ? dt : -dt) / FADE_S;
    s.alpha = 0.0f > s.alpha ? 0.0f : 1.0f < s.alpha ? 1.0f : s.alpha;
    if (0.0f >= s.alpha || '\0' == s.text[0]) return;

    int font = viewport_is_mobile() ? 13 : 14;
    float w = (float)text_measure_compat(s.text, font) + 40.0f;
    if ((float)screen_width - 16.0f < w) w = (float)screen_width - 16.0f;
    Rectangle r = { ((float)screen_width - w) * 0.5f, top, w, 26.0f };
    unsigned char a = (unsigned char)(s.alpha * 255.0f);
    bg.a = (unsigned char)((float)bg.a * s.alpha);
    DrawRectangleRounded(r, 0.5f, 8, bg);
    DrawRectangleRoundedLinesEx(r, 0.5f, 8, 1.0f, (Color){ 0, 0, 0, (unsigned char)(160.0f * s.alpha) });
    ui_icon_draw_ex(icon, r.x + 15.0f, r.y + r.height * 0.5f, 16.0f, 0.0f, (Color){ 255, 255, 255, a });
    text_draw_outlined(s.text, (int)(r.x + 28.0f), (int)(r.y + (r.height - (float)font) * 0.5f), font,
                       (Color){ 245, 245, 245, a }, (Color){ 0, 0, 0, a }, 1);
}
