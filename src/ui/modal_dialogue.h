#ifndef MODAL_DIALOGUE_H
#define MODAL_DIALOGUE_H

#include <stdbool.h>

/* Dialogue modal for an NPC-type entity (bot or other player). Opening sends
 * "dialogue_start", which freezes the player server-side. Each tap advances
 * one line; the last line, a tap outside, or ESC closes and sends
 * "dialogue_end". Draws the text box with modal.c and the entity sprite with
 * ol_stack_ico. */

#define DIALOGUE_MAX_LINES 32
#define DIALOGUE_MAX_TEXT  256
#define DIALOGUE_MAX_SPEAKER 64

typedef struct {
    char speaker[DIALOGUE_MAX_SPEAKER];
    char text[DIALOGUE_MAX_TEXT];
    char mood[32];
    int  order;
} DialogueLine;

/* Fires after the modal closes and the "dialogue_end" message goes out. The
 * inventory modal uses it to take focus back after a preview. */
typedef void (*ModalDialogueOnClose)(void);

void modal_dialogue_init(void);

/* Selects what sprite the dialogue renders in its left column:
 *   ITEM   — the single item_id only (inventory lore button).
 *   ENTITY — the entity's full active ObjectLayer stack (NPC talk). The
 *            stack is shown only if the entity has an active skin; otherwise
 *            the column renders a "Not Available" state. */
typedef enum {
    MODAL_DIALOGUE_RENDER_ITEM,
    MODAL_DIALOGUE_RENDER_ENTITY,
} ModalDialogueRender;

/* `lines` is copied. */
void modal_dialogue_open(const char* entity_id, const char* item_id,
                         const char* dialog_code, ModalDialogueRender render,
                         const DialogueLine* lines, int line_count);

/* Sends "dialogue_end", then fires the on-close callback if one is set. */
void modal_dialogue_close(void);

/* Mark the next/current dialogue as a quest-talk so it renders the quest icon
 * and a yellow frame. Cleared automatically on close. */
void modal_dialogue_set_quest_style(bool on);

/* Set the one-shot close callback. NULL removes it. */
void modal_dialogue_set_on_close(ModalDialogueOnClose cb);

bool modal_dialogue_is_open(void);

/* True while an inventory-lore dialogue (opened from the inventory modal's
 * Dialog button) is up. */
bool modal_dialogue_is_item_lore(void);

/* True while the mobile fullscreen reader is up. The interact modal hides for
 * its duration. */
bool modal_dialogue_is_fullscreen(void);

/* Desktop: paired dialogue collapsed by its close button — the interact
 * modal reclaims the space and shows a Dialog button to restore it. */
bool modal_dialogue_is_collapsed(void);

/* Mobile: open the fullscreen reader. Desktop: restore a collapsed paired
 * dialogue to its bottom-half panel. */
void modal_dialogue_show_fullscreen(void);

/* Advance the typewriter effect and the animations. */
void modal_dialogue_update(float dt);

/* Draw in screen space. */
void modal_dialogue_draw(void);

bool modal_dialogue_handle_click(int mx, int my);

#endif /* MODAL_DIALOGUE_H */
