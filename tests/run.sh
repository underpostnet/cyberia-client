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
    src/world/object_layer.c src/world/layer_z_order.c src/hash_table.c -lm
"$out/player_render_ready"

cc "${cc_flags[@]}" -o "$out/instance_route" tests/instance_route_test.c src/world/instance_route.c -lm
"$out/instance_route"

cc "${cc_flags[@]}" -o "$out/fx_level_up" tests/fx_level_up_test.c src/fx/fx_level_up.c -lm
"$out/fx_level_up"

cc "${cc_flags[@]}" -o "$out/fx_death" tests/fx_death_test.c src/fx/fx_death.c -lm
"$out/fx_death"

cc "${cc_flags[@]}" -o "$out/fx_heal_aura" tests/fx_heal_aura_test.c src/fx/fx_heal_aura.c -lm
"$out/fx_heal_aura"

cc "${cc_flags[@]}" -o "$out/fx_portal_aura" tests/fx_portal_aura_test.c src/fx/fx_portal_aura.c -lm
"$out/fx_portal_aura"

cc "${cc_flags[@]}" -o "$out/fx_hit_burst" tests/fx_hit_burst_test.c src/fx/fx_hit_burst.c -lm
"$out/fx_hit_burst"

cc "${cc_flags[@]}" -o "$out/fx_obstacle_burst" tests/fx_obstacle_burst_test.c src/fx/fx_obstacle_burst.c -lm
"$out/fx_obstacle_burst"

cc "${cc_flags[@]}" -o "$out/object_layer_metadata" tests/object_layer_metadata_test.c \
    src/world/object_layer.c src/hash_table.c libs/cJSON/cJSON.c -lm
"$out/object_layer_metadata"

cc "${cc_flags[@]}" -o "$out/overhead_occlusion" tests/overhead_occlusion_test.c src/view/overhead_occlusion.c -lm
"$out/overhead_occlusion"

cc "${cc_flags[@]}" -o "$out/pointer_gesture" tests/pointer_gesture_test.c src/input/pointer_gesture.c -lm
"$out/pointer_gesture"

cc "${cc_flags[@]}" -o "$out/input" tests/input_test.c src/input/input.c src/input/pointer_gesture.c -lm
"$out/input"

cc "${cc_flags[@]}" -o "$out/hud_joystick" tests/hud_joystick_test.c src/ui/hud_joystick.c -lm
"$out/hud_joystick"

cc "${cc_flags[@]}" -o "$out/skill_preview" tests/skill_preview_test.c src/ui/skill_preview.c src/ui/ui_state.c \
    src/sim/local_player.c src/utils.c libs/cJSON/cJSON.c -lm
"$out/skill_preview"

cc "${cc_flags[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -o "$out/world_depth" tests/world_depth_test.c -lm
"$out/world_depth"

cc "${cc_flags[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -o "$out/inventory_bar_preview" tests/inventory_bar_preview_test.c \
    src/ui/skill_preview.c src/ui/ui_state.c src/utils.c -lm
"$out/inventory_bar_preview"

cc "${cc_flags[@]}" -o "$out/entity_overhead_ui" tests/entity_overhead_ui_test.c src/ui/entity_overhead_ui.c -lm
"$out/entity_overhead_ui"

cc "${cc_flags[@]}" -ffunction-sections -fdata-sections -Wl,--gc-sections \
    -o "$out/loot_fx" tests/loot_fx_test.c src/fx/loot_fx.c src/utils.c -lm
"$out/loot_fx"

cc "${cc_flags[@]}" -o "$out/self_motion" tests/self_motion_test.c src/sim/self_motion.c -lm
"$out/self_motion"
