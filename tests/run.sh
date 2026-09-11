#!/usr/bin/env bash
# Builds and runs the native unit tests. Each test includes the unit under
# test and stubs raylib and the browser bridge, so no emscripten is needed.
set -euo pipefail

cd "$(dirname "$0")/.."
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT

cc_flags=(-std=gnu11 -Wextra -Wno-unused-parameter -Wpointer-arith -Isrc -Ilibs/raylib/src -Ilibs/cJSON)

cc "${cc_flags[@]}" -o "$out/asset_stream" tests/asset_stream_test.c -lm
"$out/asset_stream"

cc "${cc_flags[@]}" -o "$out/player_render_ready" tests/player_render_ready_test.c \
    src/object_layer.c src/layer_z_order.c src/util/hash_table.c -lm
"$out/player_render_ready"

cc "${cc_flags[@]}" -o "$out/instance_route" tests/instance_route_test.c src/instance_route.c -lm
"$out/instance_route"

cc "${cc_flags[@]}" -o "$out/fx_level_up" tests/fx_level_up_test.c src/fx/fx_level_up.c -lm
"$out/fx_level_up"

cc "${cc_flags[@]}" -o "$out/fx_death" tests/fx_death_test.c src/fx/fx_death.c -lm
"$out/fx_death"
