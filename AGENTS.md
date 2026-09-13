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

## Root-relative includes
- Project cross-directory: path from `src/`. `#include "js/interact_bridge.h"` — never `"../js/interact_bridge.h"`.
- Same-directory siblings: bare name is fine. From `src/ui/inventory_modal.c`, `#include "inventory_bar.h"` is OK — no need to prefix with `ui/`. A `"..."` search starts in the includer's own directory, so a sibling needs no `-I`.
- Third-party: `<>` brackets — `<raylib.h>`, `<raymath.h>`, `<cJSON.h>`.

## No `(void)param;` for unused args
Drop the silencer cast. Leave param unused. Warnings for this are allowed.

## Inline single-use helpers
One call site → keep inline. Extract only if ≥2 sites or inline obscures control flow at function scale (not loop scale).

## Never auto-edit Makefiles
- `Makefile`, `Web.mk`, `config.mk`, any `*.mk` → off-limits to autonomous edits.
- Build flags (`-sASYNCIFY`, `-D_DEBUG`, `-O3`, linker order, etc.) are load-bearing in non-obvious ways. Wrong flag = silent miscompile, heap corruption, broken release.
- If a change seems to require a Makefile edit: STOP, explain what flag/line you want to change and why, ask the user to confirm they understand the implication. Wait for explicit approval.
- There is no recursive glob. `config.mk` lists one wildcard per directory: `src`, `js`, `network`, `ui`, `input`, `domain`, `fx`, `util`.
- A new `.c` in one of those directories builds with no edit. A new directory does not — it needs a `config.mk` line, so stop and ask. A file in an unlisted directory compiles nowhere and links to nothing.

## src/shell.html — keep it close to raylib's shell

`libs/raylib/src/shell.html` is the reference. Keep `src/shell.html` as close to it as possible.

Before you change `src/shell.html`, do these steps in sequence:

1. Search the codebase. Find out if the change can go in C.
2. Use standard C when possible.
3. Else use a well-known library: raylib, emscripten `html5.h`. Optionally, search the internet and suggest open source libraries.
4. Change `src/shell.html` only if no library does it and standard C cannot do it.

Every difference from the reference must be necessary. If you find a difference that C or a library can replace, move it out of the shell.

## JS — last resort

This rule applies to all JS: `EM_JS`, `EM_ASM`, `--js-library` files, and inline scripts.

1. Search the full codebase for a C path or an existing bridge that does the task.
2. Use standard C, raylib, or emscripten `html5.h` / `emscripten_fetch` when they do the task. Optionally, search the internet and suggest open source libraries.
3. Write new JS only if no alternative exists. Keep it small, and put it in an existing bridge in `src/js/` when possible.

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

## manifests/ — engine-cyberia owns it

`manifests/` is not ours. `engine-cyberia` writes it and an external tool syncs
it here. This repo is a passive consumer.

- A change that seems to need a `manifests/` edit: **stop**. Name the file and
  the line, and let the user take it to the owner. The sync will overwrite it.
- A change that touches `manifests/` and nothing else: allowed. Keep it in its
  own commit, and warn the user the next sync can overwrite it.
- Never bundle a `manifests/` edit with source changes in one commit.
- Do not read it as the source of truth for deploy config.

## Browser support

Browser compatibility is not a goal. Keep the code minimal and target recent browsers.

- If a change needs a compatibility layer (a polyfill, a vendor prefix, a feature check or a fallback), do not write it. Drop support for the older browser instead.
- `Web.mk` holds the supported minimums: `-sMIN_CHROME_VERSION`, `-sMIN_FIREFOX_VERSION`, `-sMIN_SAFARI_VERSION`. Check them before you use a browser API. If the API needs a newer version, raise the minimum in the same commit.
- Raise a minimum when the higher value makes emscripten emit less code. To find the version gates, grep `MIN_*_VERSION` in the emsdk `src/` and `emcc.py`.
- A `Web.mk` edit needs approval. See "Never auto-edit Makefiles".
- Code for a behavior of a current browser is not a compatibility layer. Keep it. Example: iOS zooms on focus of an input under 16px.

# System Map

Three processes:

| process | role | talks to |
|---|---|---|
| **cyberia-client** | game client, game canvas | game server (WebSocket), engine (REST) |
| **cyberia-server** | authoritative simulation | client (WebSocket), engine (gRPC + REST) |
| **engine-cyberia** | external content authority (assets, config data, asset data) | serves both |

Two links carry client traffic:

1. **Game link** — one WebSocket, `/ws`, JSON envelope.
   `cyberia-client/src/network/socket.c` ↔ `cyberia-server/cmd/cyberia-server/main.go`.
2. **Content link** — HTTPS REST to the engine origin.
   `cyberia-client/src/network/engine_client.c` (`emscripten_fetch`)

The game server never serves game content, and the engine never sees simulation
state.
