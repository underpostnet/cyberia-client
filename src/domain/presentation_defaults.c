#include "presentation_defaults.h"

#include <string.h>

/* Canonical palette — must stay in 1-to-1 parity with
 * engine/src/api/cyberia-instance-conf/cyberia-presentation-hints.defaults.js
 * (PALETTE export).
 *
 * The simulation does not consult any of these values.
 */
const PresentationColorEntry kPresentationPalette[] = {
    /* World ------------------------------------------------------- */
    { "BACKGROUND",          (Color){ 30,  30,  30,  255 } },
    { "FLOOR_BACKGROUND",    (Color){ 45,  45,  45,  255 } },
    { "FLOOR",               (Color){ 60,  60,  60,  255 } },
    { "OBSTACLE",            (Color){ 80,  80,  80,  255 } },
    { "PORTAL",              (Color){ 0,   200, 200, 255 } },
    { "PORTAL_INTER_PORTAL", (Color){ 0,   200, 200, 255 } },
    { "PORTAL_INTER_RANDOM", (Color){ 80,  130, 255, 255 } },
    { "PORTAL_INTRA_RANDOM", (Color){ 220, 200, 50,  255 } },
    { "PORTAL_INTRA_PORTAL", (Color){ 200, 80,  200, 255 } },
    { "FOREGROUND",          (Color){ 255, 255, 255, 189 } },
    /* Entity solid-colour fallbacks ------------------------------- */
    { "PLAYER",              (Color){ 0,   255, 0,   255 } },
    { "OTHER_PLAYER",        (Color){ 128, 128, 255, 255 } },
    { "BOT",                 (Color){ 255, 128, 0,   255 } },
    { "GHOST",               (Color){ 200, 200, 255, 100 } },
    { "COIN",                (Color){ 255, 215, 0,   255 } },
    { "SKILL",               (Color){ 255, 255, 50,  255 } },
    { "RESOURCE",            (Color){ 100, 180, 80,  255 } },
    /* UI-only ----------------------------------------------------- */
    { "WEAPON",              (Color){ 180, 50,  50,  255 } },
    { "SELF_BORDER",         (Color){ 220, 190, 60,  240 } },
};
const int kPresentationPaletteCount =
    (int)(sizeof(kPresentationPalette) / sizeof(kPresentationPalette[0]));

const PresentationEntityColorKey kPresentationEntityColorKeys[] = {
    { "player",       "PLAYER" },
    { "other_player", "OTHER_PLAYER" },
    { "bot",          "BOT" },
    { "skill",        "SKILL" },
    { "coin",         "COIN" },
    { "floor",        "FLOOR" },
    { "obstacle",     "OBSTACLE" },
    { "portal",       "PORTAL" },
    { "foreground",   "FOREGROUND" },
    { "resource",     "RESOURCE" },
};
const int kPresentationEntityColorKeyCount =
    (int)(sizeof(kPresentationEntityColorKeys) / sizeof(kPresentationEntityColorKeys[0]));

const PresentationStatusIcon kPresentationStatusIcons[] = {
    { 0, NULL,               false, (Color){ 70,  70,  120, 200 } }, /* none            */
    { 1, "arrow-down-gray",  false, (Color){ 130, 140, 160, 200 } }, /* passive bot     */
    { 2, "arrow-down-red",   true,  (Color){ 210, 50,  50,  240 } }, /* hostile bot     */
    { 3, "chat",             true,  (Color){ 80,  160, 220, 240 } }, /* frozen player   */
    { 4, "arrow-down",       false, (Color){ 60,  190, 90,  240 } }, /* alive player    */
    { 5, "skull",            false, (Color){ 160, 130, 200, 200 } }, /* dead            */
    { 6, "arrow-down-gray",  false, (Color){ 100, 180, 80,  220 } }, /* resource        */
    { 7, "clock",            false, (Color){ 160, 130, 200, 200 } }, /* resource-extr.  */
    { 8, "chat",             true,  (Color){ 220, 190, 60,  240 } }, /* action-provider */
};
const int kPresentationStatusIconCount =
    (int)(sizeof(kPresentationStatusIcons) / sizeof(kPresentationStatusIcons[0]));

Color presentation_palette_lookup(const char* key) {
    if (key && key[0] != '\0') {
        for (int i = 0; i < kPresentationPaletteCount; i++) {
            if (strcmp(kPresentationPalette[i].key, key) == 0) {
                return kPresentationPalette[i].color;
            }
        }
    }
    return (Color){ 100, 100, 100, 200 }; /* neutral grey fallback */
}

Color presentation_entity_fallback_color(const char* entity_type) {
    if (!entity_type) return presentation_palette_lookup(NULL);
    for (int i = 0; i < kPresentationEntityColorKeyCount; i++) {
        if (strcmp(kPresentationEntityColorKeys[i].entity_type, entity_type) == 0) {
            return presentation_palette_lookup(kPresentationEntityColorKeys[i].color_key);
        }
    }
    return presentation_palette_lookup(NULL);
}

const char* presentation_status_icon_id(uint8_t status_id) {
    for (int i = 0; i < kPresentationStatusIconCount; i++) {
        if (kPresentationStatusIcons[i].id == status_id) {
            return kPresentationStatusIcons[i].icon_id;
        }
    }
    return NULL;
}

Color presentation_status_icon_border(uint8_t status_id) {
    for (int i = 0; i < kPresentationStatusIconCount; i++) {
        if (kPresentationStatusIcons[i].id == status_id) {
            return kPresentationStatusIcons[i].border_color;
        }
    }
    return (Color){ 70, 70, 120, 200 };
}
