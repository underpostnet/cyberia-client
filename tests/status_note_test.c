#include "content/stat_contract_generated.h"
#include <assert.h>
#include <raylib.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    assert(0 == GetCodepointCount(""));
    assert(1 == GetCodepointCount("🌲"));
    assert(2 == GetCodepointCount("e\xCC\x81"));
    assert(7 == GetCodepointCount("👩‍👩‍👧‍👦"));
    assert(2 == GetCodepointCount("🇨🇱"));
    char note[4 * (CYBERIA_STATUS_NOTE_MAX_RUNES + 1) + 1] = {0};
    for (int i = 0; CYBERIA_STATUS_NOTE_MAX_RUNES > i; i++) strcat(note, "🌲");
    assert(CYBERIA_STATUS_NOTE_MAX_RUNES == GetCodepointCount(note));
    strcat(note, "🌲");
    assert(CYBERIA_STATUS_NOTE_MAX_RUNES < GetCodepointCount(note));
    puts("status_note_test OK");
    return 0;
}
