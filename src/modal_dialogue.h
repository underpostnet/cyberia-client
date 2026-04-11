/**
 * @file modal_dialogue.h
 * @brief Dialogue/lore modal built on top of modal.c
 *
 * Displays a dialogue overlay when the player interacts with an NPC-type
 * entity (BOT or other PLAYER) that has associated dialogue data.
 *
 * Flow:
 *   1. Player taps on an entity that has dialogue data attached to one of
 *      its active item layers.
 *   2. modal_dialogue_open() is called with the target entity's ID and the
 *      dialogue item_id → sends "dialogue_start" to server (grants immunity).
 *   3. Each tap advances to the next line.  When all lines are exhausted
 *      the modal closes and sends "dialogue_end" (removes immunity).
 *   4. Tapping outside or pressing ESC also closes early.
 *
 * The modal uses modal.c's Modal struct for rendering the text box and
 * ol_as_animated_ico for displaying the associated item sprite.
 *
 *   ┌───────────────────────────────────────────┐
 *   │  [item sprite]   Speaker Name             │
 *   │                                           │
 *   │  "Dialogue line text goes here ..."       │
 *   │                                           │
 *   │              [ Tap to continue ]          │
 *   └───────────────────────────────────────────┘
 */

#ifndef MODAL_DIALOGUE_H
#define MODAL_DIALOGUE_H

#include <stdbool.h>
#include "object_layers_management.h"

/* Maximum dialogue lines that can be loaded for a single interaction. */
#define DIALOGUE_MAX_LINES 32
#define DIALOGUE_MAX_TEXT  256
#define DIALOGUE_MAX_SPEAKER 64

/**
 * @brief Single dialogue line received from the API.
 */
typedef struct {
    char speaker[DIALOGUE_MAX_SPEAKER];
    char text[DIALOGUE_MAX_TEXT];
    char mood[32];
    int  order;
} DialogueLine;

/**
 * @brief Optional callback invoked when the dialogue modal closes.
 *
 * Used by the inventory modal to regain focus after a dialogue preview.
 * The callback fires after the server "dialogue_end" message is sent.
 */
typedef void (*ModalDialogueOnClose)(void);

/* ── Public API ──────────────────────────────────────────────────────── */

/**
 * @brief Initialise the dialogue modal subsystem.
 * @param ol_manager Pointer to the shared ObjectLayersManager.
 */
void modal_dialogue_init(ObjectLayersManager* ol_manager);

/**
 * @brief Open a dialogue interaction.
 *
 * @param entity_id   ID of the entity the player is speaking with.
 * @param item_id     Item ID whose dialogue data to display.
 * @param lines       Array of dialogue lines (copied internally).
 * @param line_count  Number of entries in @p lines.
 */
void modal_dialogue_open(const char* entity_id, const char* item_id,
                         const DialogueLine* lines, int line_count);

/**
 * @brief Close the dialogue (sends "dialogue_end" to server).
 *
 * If an on_close callback was set, it fires after the close.
 */
void modal_dialogue_close(void);

/**
 * @brief Set a one-shot callback that fires when the modal closes.
 *
 * The callback is cleared after it fires (one-shot).  Pass NULL to remove.
 */
void modal_dialogue_set_on_close(ModalDialogueOnClose cb);

/**
 * @brief True while the dialogue overlay is visible.
 */
bool modal_dialogue_is_open(void);

/**
 * @brief Advance animations / typewriter effect.
 * @param dt Delta-time in seconds.
 */
void modal_dialogue_update(float dt);

/**
 * @brief Render the dialogue overlay in screen space.
 */
void modal_dialogue_draw(void);

/**
 * @brief Process a pointer event.
 *
 * @param mx      Screen X.
 * @param my      Screen Y.
 * @param clicked True on the frame the tap/click was released.
 * @return        True if the event was consumed.
 */
bool modal_dialogue_handle_click(int mx, int my, bool clicked);

#endif /* MODAL_DIALOGUE_H */
