#include "audio_context.h"
#include "audio/audio.h"
#include "audio/audio_events.h"
#include "game_state.h"
#include "local_player.h"
#include "object_layer.h"
#include "world_types.h"
#include "util/utils.h"

#include <stdbool.h>
#include <string.h>

#define SEEN_SKILLS_CAP 128
#define TRACKED_LIVES_CAP 256

/* Life fractions a heal is worth sounding at. Regeneration ticks constantly and in small
 * amounts, so sounding every tick saturates the SFX bus as soon as a few wounded entities share
 * the view. Only a milestone sounds: coming back from zero, and each quarter of maximum life the
 * entity climbs past. */
static const float LIFE_MILESTONES[] = { 0.0f, 0.25f, 0.5f, 0.75f };

/* Seconds between one entity's footfalls while it is walking. Every walker keeps its own timer,
 * so a crowd sounds like a crowd; each is offset by a phase taken from its id, because feet that
 * all landed on the same frame would read as one loud step instead of several people. */
#define FOOTSTEP_CADENCE_SECONDS 0.46f

/* What the audio layer remembers about one entity between snapshots. */
typedef struct {
    char id[MAX_ID_LENGTH];
    float fraction;        /* life / max_life at the last snapshot */
    bool walking;          /* on feet and moving at the last snapshot */
    float step_remaining;  /* seconds until this entity's next footfall */
} TrackedEntity;

/* One snapshot's worth of tracked entities, and whether any of them crossed a life milestone. */
typedef struct {
    TrackedEntity entries[TRACKED_LIVES_CAP];
    int count;
    bool healed;
} EntityScan;

static char s_map[MAX_ID_LENGTH];
static char s_skills[SEEN_SKILLS_CAP][MAX_ID_LENGTH];
static int s_skill_count;
static TrackedEntity s_tracked[TRACKED_LIVES_CAP];
static int s_tracked_count;
static float s_life;
static int s_coins;
static float s_combat_remaining;
static bool s_portal_hold;
static float s_portal_progress;

static bool crossed_milestone(float before, float after) {
    for (size_t i = 0; sizeof(LIFE_MILESTONES) / sizeof(*LIFE_MILESTONES) > i; i++) {
        if (before <= LIFE_MILESTONES[i] && after > LIFE_MILESTONES[i]) return true;
    }
    return false;
}

/* A stable fraction of the cadence, derived from the entity id: two entities that start walking
 * on the same frame still land their feet apart. */
static float step_phase(const char* id) {
    unsigned hash = 2166136261u;
    for (const char* c = id; '\0' != *c; c++) hash = (hash ^ (unsigned char)*c) * 16777619u;
    return FOOTSTEP_CADENCE_SECONDS * (float)(hash % 1000u) / 1000.0f;
}

/* Carries one entity across the snapshot: its life fraction, whether it is walking, and the
 * footstep timer that survives with it. An entity seen for the first time is recorded without
 * sounding a heal — entering the area of interest already wounded is not a heal — and starts its
 * gait on its own phase so it never lands in step with everyone else.
 *
 * `on_feet` is false for what travels without walking: skill projectiles, coins and drops. */
static void scan_entity(EntityScan* scan, const EntityState* entity, bool on_feet, bool reset) {
    if (0.0f >= entity->max_life || TRACKED_LIVES_CAP <= scan->count) return;
    const float fraction = entity->life / entity->max_life;
    TrackedEntity* out = &scan->entries[scan->count];
    copy_str(out->id, MAX_ID_LENGTH, entity->id);
    out->fraction = fraction;
    out->walking = on_feet && MODE_WALKING == entity->mode;
    out->step_remaining = step_phase(entity->id);
    if (!reset) {
        for (int i = 0; s_tracked_count > i; i++) {
            if (0 != strcmp(s_tracked[i].id, entity->id)) continue;
            if (crossed_milestone(s_tracked[i].fraction, fraction)) scan->healed = true;
            // A walk already under way keeps its timer; one just beginning steps immediately.
            if (s_tracked[i].walking && out->walking) out->step_remaining = s_tracked[i].step_remaining;
            else if (out->walking) out->step_remaining = 0.0f;
            break;
        }
    }
    scan->count++;
}

/* One pass over everything in view. Heal sounds once for however many entities crossed a
 * milestone this snapshot — crossings are rare, and a crowd recovering together is one event to
 * the ear. Footsteps are the opposite: every walker keeps its own timer, so several sets of feet
 * are several sets of feet. */
static void entity_snapshot(bool map_changed) {
    EntityScan scan = { .count = 0, .healed = false };
    scan_entity(&scan, &g_game_state.player.base, true, map_changed);
    for (int i = 0; g_game_state.other_player_count > i; i++) {
        scan_entity(&scan, &g_game_state.other_players[i].base, true, map_changed);
    }
    for (int i = 0; g_game_state.bot_count > i; i++) {
        const BotState* bot = &g_game_state.bots[i];
        scan_entity(&scan, &bot->base, game_state_bot_has_feet(bot), map_changed);
    }
    memcpy(s_tracked, scan.entries, (size_t)scan.count * sizeof(TrackedEntity));
    s_tracked_count = scan.count;
    if (scan.healed) audio_event(AUDIO_EVENT_HEAL);
}

/* The music bed to return to once a one-off cue ends: the portal charge outlives
 * a combat exchange, so a hold still in progress keeps the bus. */
static void restore_music(void) {
    audio_event(s_portal_hold ? AUDIO_EVENT_PORTAL_COOLDOWN : AUDIO_EVENT_IDLE);
}

/* Portal audio is driven by the authoritative hold flag, never by the map code: an
 * intra-map portal moves the player without changing maps, so a map-code watch is
 * silent for exactly the case the player notices most.
 *
 * The charge is a held music bed; completing it fires the portal SFX while the
 * departing map's bindings are still current, because audio_set_map would otherwise
 * ask the map just entered for a binding it has not loaded yet. */
static void portal_snapshot(bool map_changed) {
    if (local_player_on_portal()) {
        if (!s_portal_hold) {
            s_portal_hold = true;
            audio_event(AUDIO_EVENT_PORTAL_COOLDOWN);
        }
        s_portal_progress = local_player_portal_hold_progress();
        return;
    }
    if (!s_portal_hold) return;
    s_portal_hold = false;
    /* The hold ends either way; only a teleport sounds. TELEPORTING is the direct
     * signal, but it survives a single snapshot, so a changed map or a charge that
     * reached full still counts the jump if that one snapshot was coalesced away. */
    const bool teleported = map_changed || MODE_TELEPORTING == g_game_state.player.base.mode ||
                            0.98f <= s_portal_progress;
    s_portal_progress = 0.0f;
    if (teleported) audio_event(AUDIO_EVENT_PORTAL);
    /* On a transfer the caller's audio_set_map installs the destination bed instead. */
    if (!map_changed) audio_event(AUDIO_EVENT_IDLE);
}

void audio_context_reset(void) {
    s_map[0] = '\0';
    s_skill_count = 0;
    s_tracked_count = 0;
    s_combat_remaining = 0;
    s_portal_hold = false;
    s_portal_progress = 0.0f;
    audio_set_map("");
}

void audio_context_snapshot(void) {
    const PlayerState* player = &g_game_state.player;
    if ('\0' == g_local_player.map_code[0]) return;
    bool changed = 0 != strcmp(s_map, g_local_player.map_code);
    portal_snapshot(changed);
    entity_snapshot(changed);
    if (changed) {
        copy_str(s_map, sizeof(s_map), g_local_player.map_code);
        audio_set_map(s_map);
        s_skill_count = 0;
        s_combat_remaining = 0;
    } else {
        // Impact itself is sounded by the server's damage events (see message.c), which cover
        // every entity. Own damage only decides whether combat music takes over.
        if (s_life > player->base.life && 0.0f < player->base.max_life) {
            if (0.0f >= s_combat_remaining) audio_event(AUDIO_EVENT_COMBAT);
            s_combat_remaining = 8;
        }
        if (s_coins != local_player_coins()) audio_event(AUDIO_EVENT_COIN);
    }
    s_life = player->base.life;
    s_coins = local_player_coins();

    char current[SEEN_SKILLS_CAP][MAX_ID_LENGTH];
    int count = 0;
    for (int i = 0; g_game_state.bot_count > i && SEEN_SKILLS_CAP > count; i++) {
        const BotState* bot = &g_game_state.bots[i];
        if (0 != strcmp(bot->behavior, "skill") || 0 != strcmp(bot->caster_id, player->base.id)) continue;
        bool seen = changed;
        for (int j = 0; s_skill_count > j; j++) {
            if (0 == strcmp(s_skills[j], bot->base.id)) seen = true;
        }
        if (!seen) audio_event(AUDIO_EVENT_PROJECTILE);
        copy_str(current[count++], MAX_ID_LENGTH, bot->base.id);
    }
    memcpy(s_skills, current, (size_t)count * MAX_ID_LENGTH);
    s_skill_count = count;
}

void audio_context_update(float dt) {
    // Each walker's own gait. Feet landing within the mixer's retrigger window collapse into one
    // sound, which is why the phase offsets matter: they are what turns a crowd into a crowd
    // rather than into a single louder step.
    for (int i = 0; s_tracked_count > i; i++) {
        if (!s_tracked[i].walking) continue;
        s_tracked[i].step_remaining -= dt;
        if (0.0f < s_tracked[i].step_remaining) continue;
        s_tracked[i].step_remaining = FOOTSTEP_CADENCE_SECONDS;
        audio_event(AUDIO_EVENT_FOOTSTEPS);
    }
    if (0.0f >= s_combat_remaining) return;
    s_combat_remaining -= dt;
    if (0.0f >= s_combat_remaining) restore_music();
}
