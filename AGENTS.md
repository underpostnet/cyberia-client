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

## Makefile edits — source list only
- Applies to `Makefile`, `Web.mk`, `config.mk` and any `*.mk`.
- You can edit a Makefile if and only if the change edits the `src_files` variable or changes how source files are discovered. Nothing else in the same edit.
- All other edits need manual review: compile flags, link flags, linker order, targets, toolchain. STOP, name the flag or line, tell why, and wait for explicit approval.
- Build flags (`-sASYNCIFY`, `-D_DEBUG`, `-O3`, linker order, etc.) are load-bearing in non-obvious ways. Wrong flag = silent miscompile, heap corruption, broken release.
- There is no recursive glob. `src_files` in `config.mk` lists one wildcard per directory under `src/`.
- A new `.c` in a listed directory builds with no edit. A new directory needs a `src_files` line — add it. A file in an unlisted directory compiles nowhere and links to nothing.

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

The rule counts only the JS we write. JS inside raylib, emscripten or another library we use does not count.

- A library call that runs JS internally is a correct replacement for our own JS. Do not reject it because the JS moves into the library.
- Judge the library call on behavior only: does it do the task correctly?

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
- A `Web.mk` flag edit needs approval. See "Makefile edits — source list only".
- Code for a behavior of a current browser is not a compatibility layer. Keep it. Example: iOS zooms on focus of an input under 16px.

## Code questions — query the graphify graph first

`graphify-out/` holds a knowledge graph of the code. Use it first for a question about structure, relations or "where is X". Then read the source.

- `graphify-out/GRAPH_REPORT.md` — summary: main nodes, communities, cross links.
- `graphify query "<question>"` — search the graph for a question.
- `graphify path "A" "B"` — shortest path between two nodes.
- `graphify explain "X"` — a node and its neighbors.

The graph can be older than the code. Confirm each result in the source before you edit. If `graphify-out/` is absent, read the source directly.

# System Map

Three processes. Each one has one job.

**engine-cyberia — content.** It holds what exists and how it works: object
layers, atlases, maps, instances, quests, actions, dialogue and stat rules. It
holds no simulation state. When content changes, it tells the server to
hot-reload (gRPC control, REST fallback).

**cyberia-server — authority.** It owns the state of the world and of every
entity, and it has the final word. It loads content from the engine at boot
and on hot-reload. One fixed tick runs the phases in order: input, lifecycle,
skills, AI, movement, loot, portals, then replication.

**cyberia-client — view and prediction.** It holds a full copy of the state
and writes only the entity it owns: the own player. It draws, plays audio and
runs the UI. The UI is local: it reads local state and never waits for the
socket.

Client and server mirror their folders on subject: `net/`, `sim/`, `world/`
and `content/` exist on both sides. A folder with no counterpart is not
mirrored.

## Communication

**Content — engine to both.** The client reads content over HTTPS REST
(`cyberia-client/src/data/engine_client.c`). The server reads it over gRPC,
with REST as fallback (`cyberia-server/engine_client/`). The game server never
serves content, and the engine never sees simulation state.

**Game link — one WebSocket, `/ws`, JSON envelope.**
`cyberia-client/src/net/socket.c` ↔ `cyberia-server/game/net/`.

- **Client pushes events.** The client sends what the player did, never what
  it has. Every module pushes through `client_event_push`, which stamps frame,
  sequence and time. Raw input stays in the client.
  - Deferred events send when a UI action completes: `item_ops` (the
    add/remove list of a vault session) and `talk_done`. Slot order is not
    state and never syncs.
  - Immediate events send when a fact changes. Today only `player_stasis`
    (modal open/close), because it drives immunity this tick.
- **Server pushes state.** `PhaseInput` applies each player's events in
  sequence order. It replays item ops against its own items and clamps them.
  Each snapshot is a whole state with its `frame` and
  `inputConsumedThrough`: the highest sequence the server consumed. Consumed
  is not honored. The snapshot is the answer.

**Replay and reconciliation.** The client applies its own events at once.
At the start of each tick, `prediction_reconcile` replaces the own state with
the newest snapshot, then `client_event_trim` drops events at or below
`inputConsumedThrough`. The client re-applies the events that remain, in order,
with the same motion function the server uses. There is no other correction.
Other entities take the server state; the view interpolates them.

A reconnect is a new session. Sequence, frame and cursor restart at 0. The
client drops its pending events, takes `init_data` as truth and resends one
`player_stasis`.
