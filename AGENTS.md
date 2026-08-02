# Engineering conventions

Rules for working in this repo. Apply them to your own edits and to code you touch.

## Use ASD-STE100 Simplified Technical English
Write comments in simplified technical English.

Key rules:
- Use the approved words of ASD-STE100 when possible.
- Use one word for one idea. Do not use two words for the same thing.
- Write short sentences. Use 20 words or less for instructions.
- Use active voice. Write "Turn the switch", not "The switch must be turned"
- Write short paragraphs. Keep one topic in each paragraph.
- No filler.

The goal is easy reading.

Report back to the user following the same rules.

## Brief comments — no archaeology

Comments state the live invariant in ≤1 line. The reader needs current behavior, nothing more.

- Banned content: bug postmortems, "we tried X but Y broke", "previously", "used to", "added because of", historical justification, restoration sagas.
- Test: "If I delete this comment, will a reader misunderstand the code?" No → delete. Yes → minimum text, present tense.
- Decision rationale and bug-fix narration go in the commit message that introduced the change — there it's dated and attributable; on the line it's stale weight forever.

## Keep code in the C11 standard
We compile with `-std=gnu11`, but write standard C11. Use a GNU extension only when C11 has no alternative.

## Yoda comparisons
Constant left. Applies to all comparisons — strings, pointers, integers, enums.
```c
0 == strcmp(s, "x")    // yes
NULL != ptr            // yes
0 == queue.count       // yes
strcmp(s, "x") == 0    // no
queue.count == 0       // no
```

Exceptions:
Comparing against NULL does not matter
```c
NULL != ptr            // yes
ptr != NULL            // yes
```

## Root-relative includes
- Project cross-directory: path from `src/`. `#include "js/services.h"` — never `"../js/services.h"`.
- Same-directory siblings: bare name is fine. From `src/ui/inventory_modal.c`, `#include "inventory_bar.h"` is OK — no need to prefix with `ui/`. Build adds `-Isrc` only; gcc's `"..."` auto-search of the includer's directory handles siblings.
- Third-party: `<>` brackets — `<raylib.h>`, `<raymath.h>`, `<cJSON.h>`.

## No `(void)param;` for unused args
Drop the silencer cast. Leave param unused.

## Inline single-use helpers
One call site → keep inline. Extract only if ≥2 sites or inline obscures control flow at function scale (not loop scale).

## Never auto-edit Makefiles
- `Makefile`, `Web.mk`, `config.mk`, any `*.mk` → off-limits to autonomous edits.
- Build flags (`-sASYNCIFY`, `-D_DEBUG`, `-O3`, linker order, etc.) are load-bearing in non-obvious ways. Wrong flag = silent miscompile, heap corruption, broken release.
- If a change seems to require a Makefile edit: STOP, explain what flag/line you want to change and why, ask the user to confirm they understand the implication. Wait for explicit approval.
- Adding a new source file does NOT count as auto-edit if the Makefile globs `src/**/*.c`. Verify it globs before assuming.

## Asserts over defensive checks
- Prefer `assert(x);` over `if (!x) { LOG_ERROR(...); return; }` for invariant violations.
- Applies to callback ctx/data params, malloc OOM, and any value that's a bug if null/invalid, not a recoverable runtime condition.
- Keep defensive checks only at true external boundaries where the value can legitimately be null (e.g. emscripten WS close `reason` field).

```c
char* msg = malloc(n);
assert(msg);                                       // yes
if (!msg) { LOG_ERROR("OOM"); return; }            // no
```

## Explicit includes — no transitive reliance
- Every symbol used in a .c/.h file must come from a header that file directly `#include`s.
- Don't lean on transitive inclusion. If `input.c` uses `Vector2`, it must `#include <raylib.h>` itself, even if `input.h` already does.

## One theme per commit
One commit = one logical theme. No bundling unrelated changes.

- Before `git commit`: scan staged diff. ≥2 themes → unstage, commit each theme separately.
- Each commit stages the minimum file set needed for that theme. No drive-by edits, no "while I'm here" cleanups.
- Themes that touch a shared file (main.c, message_parser, central state): land the feature commits first, then one final "wire X through main loop" glue commit. Don't merge themes just to avoid the glue commit.
- Commit subject names ONE concern. If you need "and" or "+" to describe it, it's two commits.
