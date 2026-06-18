/**
 * quest_journal — right-side collapsible Quest Journal modal.
 *
 * A three-section tree (Active / Completed / Failed). The panel and each
 * section collapse independently via the shared ui_toggle component. Each
 * section paginates its own quest list (10 per page) and can expand one
 * inline quest detail at a time. Reads entirely from quest_progress_store — no REST.
 */

#ifndef QUEST_JOURNAL_H
#define QUEST_JOURNAL_H

#include <stdbool.h>

#define QUEST_JOURNAL_PAGE_SIZE 10

void quest_journal_init(void);
void quest_journal_update(float dt);
void quest_journal_draw(void);
bool quest_journal_handle_click(int mx, int my);

#endif /* QUEST_JOURNAL_H */
