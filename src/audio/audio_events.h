#ifndef CYBERIA_AUDIO_EVENTS_H
#define CYBERIA_AUDIO_EVENTS_H

/* Canonical audio LogicIds — the runtime events this client emits through audio_event().
 *
 * Cross-process contract: engine-cyberia binds these ids to assets per map
 * (src/client/components/cyberia/SharedDefaultsCyberia.js AUDIO_LOGIC_IDS, bound by
 * DEFAULT_AUDIO_BINDINGS). An id emitted here with no counterpart there is silence, so the
 * two lists must be changed together. Skill LogicIds (SKILL_LOGIC_IDS) are emitted by the
 * same call and bind the same way. */

#define AUDIO_EVENT_IDLE            "idle"
#define AUDIO_EVENT_COMBAT          "combat"
#define AUDIO_EVENT_BOSS            "boss"
#define AUDIO_EVENT_VICTORY         "victory"
#define AUDIO_EVENT_PORTAL_COOLDOWN "portal-cooldown"
#define AUDIO_EVENT_PORTAL          "portal"
#define AUDIO_EVENT_HIT             "hit"
#define AUDIO_EVENT_HEAL            "heal"
#define AUDIO_EVENT_DROP            "drop"
#define AUDIO_EVENT_ITEM_PICKUP     "item-pickup"
#define AUDIO_EVENT_CRAFT           "craft"
#define AUDIO_EVENT_UI_CLICK        "ui-click"
#define AUDIO_EVENT_FOOTSTEPS       "footsteps"

/* Bus ids, as a binding's `settings.bus` spells them: the same vocabulary that names the
 * `src/audio-module/<bus-id>/` directory an asset is authored in. */
#define AUDIO_BUS_ID_MUSIC "music"
#define AUDIO_BUS_ID_SFX   "sfx"

/* Skill LogicIds the client raises directly. */
#define AUDIO_EVENT_PROJECTILE      "projectile"
#define AUDIO_EVENT_COIN            "coin_drop_or_transaction"

#endif
