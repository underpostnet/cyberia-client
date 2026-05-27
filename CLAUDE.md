# Code Style Rules

## 1. Struct init = curly braces
```c
Foo f = { .a = 1, .b = 2 };   // yes
f.a = 1; f.b = 2;             // no
```

## 2. No doxygen
No `/** @param */`, `@return`, `@brief`. Even if surrounding code has them.

## 3. Yoda comparisons
Constant left.
```c
0 == strcmp(s, "x")    // yes
NULL != ptr            // yes
strcmp(s, "x") == 0    // no
```

## 4. Root-relative includes
- Project cross-directory: path from `src/`. `#include "js/services.h"` — never `"../js/services.h"`.
- Same-directory siblings: bare name is fine. From `src/ui/inventory_modal.c`, `#include "inventory_bar.h"` is OK — no need to prefix with `ui/`. Build adds `-Isrc` only; gcc's `"..."` auto-search of the includer's directory handles siblings.
- Third-party: `<>` brackets — `<raylib.h>`, `<raymath.h>`, `<cJSON.h>`.

## 5. No `(void)param;` for unused args
Drop the silencer cast. Leave param unused.

## 6. Inline single-use helpers
One call site → keep inline. Extract only if ≥2 sites or inline obscures control flow at function scale (not loop scale).

## 7. Brief comments only — no archaeology
- Comments state the live invariant in ≤1 line. Reader needs to understand current behavior, nothing more.
- Banned content: bug postmortems, "we tried X but Y broke", "previously", "used to", "added because of", historical justification, restoration sagas.
- Test: "If I delete this comment, will a reader misunderstand the code?" No → delete. Yes → minimum text, present tense.
- Decision rationale + bug fix narration → goes in the commit message that introduced the change, not on the line forever.

## 8. Never auto-edit Makefiles
- `Makefile`, `Web.mk`, `config.mk`, any `*.mk` → off-limits to autonomous edits.
- Build flags (`-sASYNCIFY`, `-D_DEBUG`, `-O3`, linker order, etc.) are load-bearing in non-obvious ways. Wrong flag = silent miscompile, heap corruption, broken release.
- If a change seems to require a Makefile edit: STOP, explain what flag/line you want to change and why, ask the user to confirm they understand the implication. Wait for explicit approval.
- Adding a new source file does NOT count as auto-edit if the Makefile globs `src/**/*.c`. Verify it globs before assuming.

## 9. One theme per commit
- One commit = one logical theme. No bundling unrelated changes.
- Before `git commit`: scan staged diff. ≥2 themes → unstage, commit each theme separately.
- Each commit stages the minimum file set needed for that theme. No drive-by edits, no "while I'm here" cleanups.
- Themes that touch a shared file (main.c, message_parser, central state): land the feature commits first, then one final "wire X through main loop" glue commit. Don't merge themes just to avoid the glue commit.
- Commit subject names ONE concern. If you need "and" or "+" to describe it, it's two commits.
