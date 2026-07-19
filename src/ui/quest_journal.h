/**
 * quest_journal — right-side collapsible Quest Journal modal.
 *
 * A three-section tree (Active / Completed / Failed). The panel and each
 * section collapse independently via the shared ui_toggle component. Each
 * section paginates its own quest list (QUEST_JOURNAL_PAGE_SIZE per page,
 * ui-icon arrow navigation). Active cards list every quest step: completed in
 * muted green, the current step highlighted, future steps visually disabled.
 * Reads progress from quest_progress_store; step metadata from quest_cache.
 */

#ifndef QUEST_JOURNAL_H
#define QUEST_JOURNAL_H

#include <stdbool.h>

#define QUEST_JOURNAL_PAGE_SIZE 3

void quest_journal_init(void);
void quest_journal_update(float dt);
void quest_journal_draw(void);
bool quest_journal_handle_click(int mx, int my);

/* Wheel scroll while the pointer hovers the section list. Returns true when
 * consumed so the world zoom behind the panel is suppressed. */
bool quest_journal_handle_wheel(float wheel_delta);

/* Hidden by default; the toolbar's quest button toggles it. */
void quest_journal_toggle(void);
bool quest_journal_is_visible(void);

#endif /* QUEST_JOURNAL_H */
