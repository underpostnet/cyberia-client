# Graph Report - cyberia-client  (2026-10-15)

## Corpus Check
- cluster-only mode — file stats not available

## Summary
- 2101 nodes · 7105 edges · 68 communities (65 shown, 3 thin omitted)
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 1772 edges (avg confidence: 0.85)
- Token cost: 64,362 input · 1,021 output

## Graph Freshness
- Built from commit: `2da47dce`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- Instance Map Graph
- Inventory Bar Quantity
- Inventory & Dialogue Modals
- Entity Render & HUD
- Client Event Packing
- Action Craft Cache
- Notification & Grant FX
- Player Overhead Display
- Input & Party Events
- Chat Log
- World Entity Rendering
- UI & FX Modules
- Interact Modal Cards
- Fullscreen & HUD Bridges
- Dialogue Modal
- Loot Drop FX
- Interact Mode & Chat Bind
- Client Event Replication
- Heal Aura FX
- Audio & UI Headers
- Floating Combat Text
- Item Slot Grid
- Audio Bus Control
- Asset Bridge Fetch
- Subsystem Reset
- Container Panel Ops
- Interact Modal Layout
- Audio Content Fetch
- Atlas Sprite Sheet Data
- Entity Animation Render
- Container Stack Operations
- Emscripten Bridges & Main
- Game State Tick
- Dialogue Data Hash
- Quest Detail Drawing
- Reward FX
- Presentation Runtime Config
- Subsystem Init
- Container Panel Tabs
- Assemble FX
- Footsteps & Dust FX
- WebSocket Connection
- Audio Context Snapshot
- Dev UI Overlay
- Camera & Frame Sync
- Dust Puff FX
- Atlas Icon Draw
- Tap FX
- Stack Move Animation
- FX Shape Primitives
- Container Tab Visibility
- Level Up FX
- Layer Z Ordering
- HUD Joystick
- Death FX
- Pointer Gesture Input
- Startup & Init
- Inventory Capacity Rules
- Stat & Capacity Contract
- Asset Bridge JS
- Fetch Request & Font
- Subsystem Cleanup
- Camera Coordinate Transform
- Resource Destruction
- Object Layer Alloc
- Config URL Helpers

## God Nodes (most connected - your core abstractions)
1. `viewport_is_mobile()` - 64 edges
2. `copy_str()` - 53 edges
3. `inventory_modal_draw()` - 52 edges
4. `ui_button_hit()` - 45 edges
5. `modal_interact_draw()` - 41 edges
6. `inventory_bar_draw()` - 40 edges
7. `game_render_entities()` - 37 edges
8. `render_on_tick()` - 34 edges
9. `modal_interact_update()` - 32 edges
10. `modal_notification_draw()` - 32 edges

## Surprising Connections (you probably didn't know these)
- `draw_info_panel()` --calls--> `instance_map_data_node_active_actions()`  [INFERRED]
  src/ui/modal_instance_map.c → src/data/instance_map_data.c
- `draw_info_panel()` --calls--> `instance_map_data_node_active_quests()`  [INFERRED]
  src/ui/modal_instance_map.c → src/data/instance_map_data.c
- `game_render_frame()` --calls--> `fct_draw_overlay()`  [INFERRED]
  src/view/game_render.c → src/ui/floating_combat_text.c
- `game_render_world_objects()` --calls--> `nameplate_resolve_portal()`  [INFERRED]
  src/view/game_render.c → src/ui/nameplate.c
- `modal_interact_update()` --calls--> `modal_notification_is_on_cooldown()`  [INFERRED]
  src/ui/modal_interact.c → src/ui/modal_notification.c

## Import Cycles
- None detected.

## Communities (68 total, 3 thin omitted)

### Community 0 - "Instance Map Graph"
Cohesion: 0.05
Nodes (91): presentation_runtime_status_icon(), instance_map_cell_to_card(), instance_map_data_close(), instance_map_data_generation(), instance_map_data_graph(), instance_map_data_open(), instance_map_data_state(), instance_map_presence_color() (+83 more)

### Community 1 - "Inventory Bar Quantity"
Cohesion: 0.05
Nodes (92): alloc_entry(), changed_cell(), draw_popup(), find(), fx_inventory_bar_qty_display(), fx_inventory_bar_qty_draw(), fx_inventory_bar_qty_draw_bottom(), fx_inventory_bar_qty_init() (+84 more)

### Community 2 - "Inventory & Dialogue Modals"
Cohesion: 0.05
Nodes (77): dialogue_data_available(), dialogue_data_get(), client_on_init_received(), game_state_is_active_item_type(), game_state_is_dead_item(), local_player_status_icon(), interaction_bubble_dead_equip(), activate_enabled() (+69 more)

### Community 3 - "Entity Render & HUD"
Cohesion: 0.05
Nodes (68): presentation_runtime_toggle_dev_ui(), input_pointer_position(), container_panel_press(), column_content_height(), column_ensure_toggle(), column_hidden(), column_input_bounds(), column_scroll_view() (+60 more)

### Community 4 - "Client Event Packing"
Cohesion: 0.08
Nodes (70): read_config(), settings(), fx_hit_burst_spawn(), client_event_flush(), pack_event(), chat_conversation_of(), claim_slot(), entity_alive() (+62 more)

### Community 5 - "Action Craft Cache"
Cohesion: 0.05
Nodes (46): action_cache_fetch(), action_cache_get(), ingest_craft_items(), ingest_doc(), on_action_fetched(), apply_dynamic_capabilities(), clear_dynamic_capabilities(), find_presence_poi() (+38 more)

### Community 6 - "Notification & Grant FX"
Cohesion: 0.08
Nodes (62): fx_grant_delivery_begin(), fx_grant_delivery_update(), fx_grant_delivery_waiting(), grant_landed(), release_spent(), fx_inventory_bar_qty_hold_for_delivery(), fx_inventory_bar_qty_notify_arrival(), ease_in_cubic() (+54 more)

### Community 7 - "Player Overhead Display"
Cohesion: 0.07
Nodes (48): presentation_runtime_font_family(), game_state_has(), game_state_progress(), local_player_leads_party(), own_level(), draw_capability_bar(), draw_capability_item(), draw_centered_label() (+40 more)

### Community 8 - "Input & Party Events"
Cohesion: 0.06
Nodes (43): axis_dir(), emit_steer_tap(), emit_tap(), input_pointer_gesture(), input_pop(), input_push(), input_push_client_events(), input_queue_filter() (+35 more)

### Community 9 - "Chat Log"
Cohesion: 0.08
Nodes (44): append(), chat_channel(), chat_log_get(), chat_log_mark_read(), chat_log_push(), chat_log_push_system(), chat_log_unread(), chat_log_unread_direct() (+36 more)

### Community 10 - "World Entity Rendering"
Cohesion: 0.07
Nodes (41): presentation_runtime_dev_ui(), presentation_runtime_palette(), world_cell_size(), fx_heal_aura_observe(), fx_hit_burst_draw(), fx_obstacle_burst_draw(), fx_obstacle_burst_snapshot(), fx_shape_spark_shaded() (+33 more)

### Community 11 - "UI & FX Modules"
Cohesion: 0.07
Nodes (3): compare_depth(), compare_entities_by_depth(), compare_foregrounds_by_depth()

### Community 12 - "Interact Modal Cards"
Cohesion: 0.11
Nodes (36): draw_quest_talk_btn(), card_title_wrap(), craft_card_height(), craft_recipe_ready(), craft_slot_size(), draw_bar_button(), draw_card_base(), draw_card_line() (+28 more)

### Community 13 - "Fullscreen & HUD Bridges"
Cohesion: 0.10
Nodes (34): audio_is_muted(), fullscreen_bridge_is_active(), fullscreen_bridge_toggle(), chat_panel_is_visible(), hud_minimap_overlay_is_visible(), chat_height(), chat_reserve(), column_x() (+26 more)

### Community 14 - "Dialogue Modal"
Cohesion: 0.10
Nodes (36): modals_block_bubbles(), inventory_modal_is_open(), dlg_fullscreen(), dlg_hidden(), dlg_hidden_on_mobile(), dlg_pad(), dlg_side_pad(), dlg_sprite_size() (+28 more)

### Community 15 - "Loot Drop FX"
Cohesion: 0.10
Nodes (31): drop_alloc(), drop_find(), expend_target(), id_phase(), lcg_f01(), lcg_range(), loot_fx_delivery_token_at(), loot_fx_delivery_token_slot_count() (+23 more)

### Community 16 - "Interact Mode & Chat Bind"
Cohesion: 0.08
Nodes (35): chat_direct(), chat_pane_bind(), chat_pane_hide(), container_panel_close(), inventory_modal_open(), inventory_modal_open_external(), inventory_modal_set_anchor_entity(), inventory_modal_set_on_close() (+27 more)

### Community 17 - "Client Event Replication"
Cohesion: 0.09
Nodes (22): world_interpolation_ms(), walk_speed_ref(), client_event_list(), client_event_trim(), local_player_in_stasis(), local_player_move_speed(), local_player_set_stasis(), apply_facts() (+14 more)

### Community 18 - "Heal Aura FX"
Cohesion: 0.09
Nodes (17): draw_entry(), ease_out_cubic(), exit_scale(), fx_heal_aura_draw(), lane(), fx_hit_burst_reset(), fx_hit_burst_update(), lane() (+9 more)

### Community 20 - "Floating Combat Text"
Cohesion: 0.11
Nodes (12): adopt_js_string(), config_init(), fct_draw_overlay(), fct_event_on_self(), fct_init(), fct_spawn(), lcg_f01(), copy_ucfirst() (+4 more)

### Community 21 - "Item Slot Grid"
Cohesion: 0.14
Nodes (24): container_panel_draw_drag(), emit(), end_gesture(), grid_point_in_visible_bounds(), grid_rows(), item_slot_grid_animate_from_point(), item_slot_grid_animate_move(), item_slot_grid_begin_external_drag() (+16 more)

### Community 22 - "Audio Bus Control"
Cohesion: 0.17
Nodes (27): apply_volume(), audio_get_volume(), audio_params(), audio_play(), audio_request_complete(), audio_set_muted(), audio_set_volume(), audio_shutdown() (+19 more)

### Community 23 - "Asset Bridge Fetch"
Cohesion: 0.15
Nodes (21): complete(), deliver(), discard(), enqueue(), fetch_account(), fetch_data_release(), fetch_deprioritize(), fetch_event() (+13 more)

### Community 24 - "Subsystem Reset"
Cohesion: 0.08
Nodes (20): audio_context_reset(), config_ws_url(), fx_death_reset(), fx_heal_aura_reset(), fx_level_up_reset(), client_event_reset(), client_reset_state(), connection_close() (+12 more)

### Community 25 - "Container Panel Ops"
Cohesion: 0.14
Nodes (19): container_panel_animate_from(), container_panel_cell_at(), container_panel_cell_rect(), container_panel_open_storage(), container_panel_open_vault(), container_panel_update(), container_panel_wheel(), drop_out() (+11 more)

### Community 26 - "Interact Modal Layout"
Cohesion: 0.15
Nodes (29): modal_anchor_active(), active_tab_content_height(), anchor_content_card_height(), anchor_safe_area(), anchored_card_rect(), bar_buttons(), bar_rect(), card_rect() (+21 more)

### Community 27 - "Audio Content Fetch"
Cohesion: 0.15
Nodes (21): audio_preload(), audio_request_absent(), audio_request_code(), audio_request_priority(), apply_current(), audio_content_fetch(), audio_content_music_finished(), audio_content_restore_music() (+13 more)

### Community 28 - "Atlas Sprite Sheet Data"
Cohesion: 0.16
Nodes (16): create_atlas_sprite_sheet_data(), free_atlas_sprite_sheet_data(), ledger_standard_from_string(), free_atlas_value(), json_get_bool_safe(), json_get_int_safe(), json_get_string_safe(), on_atlas_meta_fetched() (+8 more)

### Community 29 - "Entity Animation Render"
Cohesion: 0.15
Nodes (11): anim_pool_alloc(), anim_pool_free(), draw_dev_ui_box(), draw_entity_layers(), entity_render_gc(), free_anim_state(), get_animation_state(), get_frame_count_and_direction() (+3 more)

### Community 31 - "Container Stack Operations"
Cohesion: 0.25
Nodes (16): apply(), bankable_while_worn(), check(), container_ops_apply(), container_ops_bind_vault(), container_ops_move(), container_ops_stack(), item_type_of() (+8 more)

### Community 32 - "Emscripten Bridges & Main"
Cohesion: 0.15
Nodes (3): function(), draw_centered(), draw_splash()

### Community 33 - "Game State Tick"
Cohesion: 0.16
Nodes (17): fx_death_update(), fx_dust_update(), fx_heal_aura_update(), fx_level_up_update(), fx_tap_update(), game_state_despawn(), game_state_layer_alloc(), game_state_layer_pool_reset() (+9 more)

### Community 34 - "Dialogue Data Hash"
Cohesion: 0.19
Nodes (14): dialogue_data_request(), dialogue_data_request_code(), on_dialogue_fetched(), parse_response(), find_insert_slot(), find_occupied(), hash_string(), hash_table_contains() (+6 more)

### Community 35 - "Quest Detail Drawing"
Cohesion: 0.16
Nodes (17): chat_font(), chat_pane_draw(), hint_font(), draw_quest_detail(), draw_quest_step_objectives(), draw_quest_steps(), mi_close_sz(), mi_font_desc() (+9 more)

### Community 36 - "Reward FX"
Cohesion: 0.21
Nodes (15): card_center(), current_margin(), ellipse_point(), fx_reward_init(), fx_reward_show(), fx_reward_trigger(), fx_reward_update(), lcg_f01() (+7 more)

### Community 37 - "Presentation Runtime Config"
Cohesion: 0.15
Nodes (13): on_hints_fetched(), parse_color_object(), parse_response(), presentation_runtime_entity_fallback_color(), presentation_runtime_is_ready(), presentation_runtime_overhead_occlusion_fade_ms(), presentation_runtime_overhead_occlusion_hidden_opacity(), presentation_runtime_start_fetch() (+5 more)

### Community 38 - "Subsystem Init"
Cohesion: 0.11
Nodes (17): dialogue_data_init(), fx_grant_delivery_init(), loot_fx_reset(), hash_table_init(), dev_ui_init(), hud_minimap_overlay_init(), interaction_bubble_init(), inventory_modal_init() (+9 more)

### Community 39 - "Container Panel Tabs"
Cohesion: 0.26
Nodes (18): container_panel_draw(), draw_container_tab(), draw_craft_tab(), draw_integration_tab(), draw_quest_tab(), draw_shop_tab(), draw_stack_tab(), draw_stats_tab() (+10 more)

### Community 40 - "Assemble FX"
Cohesion: 0.25
Nodes (13): card_center(), current_margin(), fx_assemble_draw(), fx_assemble_init(), fx_assemble_show(), fx_assemble_update(), lcg_f01(), lcg_range() (+5 more)

### Community 41 - "Footsteps & Dust FX"
Cohesion: 0.18
Nodes (8): cell_size(), emit_step(), fx_footsteps_init(), fx_footsteps_update(), step_is_visible(), step_params(), track_entity(), tracker_for()

### Community 42 - "WebSocket Connection"
Cohesion: 0.18
Nodes (8): socket_close(), socket_is_open(), socket_onclose_internal(), socket_onerror_internal(), socket_onmessage_internal(), socket_onopen_internal(), socket_open(), socket_send()

### Community 43 - "Audio Context Snapshot"
Cohesion: 0.22
Nodes (12): audio_context_snapshot(), audio_context_update(), craft_snapshot(), crossed_milestone(), entity_snapshot(), portal_snapshot(), restore_music(), scan_entity() (+4 more)

### Community 44 - "Dev UI Overlay"
Cohesion: 0.17
Nodes (8): connection_get_stats(), dev_ui_draw(), dev_ui_get_active_item_count(), dev_ui_get_effective_stats_sum(), dev_ui_on_tick(), dev_ui_update_network_stats(), direction_to_string(), mode_to_string()

### Community 45 - "Camera & Frame Sync"
Cohesion: 0.12
Nodes (12): presentation_runtime_camera_zoom(), frame(), game_frame(), prediction_self(), session_frame_advance(), camera_init(), camera_resize(), camera_sync_mobile_zoom() (+4 more)

### Community 46 - "Dust Puff FX"
Cohesion: 0.22
Nodes (10): alloc_puff(), alpha_curve(), fx_dust_default_params(), fx_dust_draw(), fx_dust_init(), fx_dust_spawn(), lcg_f01(), lcg_range() (+2 more)

### Community 47 - "Atlas Icon Draw"
Cohesion: 0.21
Nodes (8): fallback_circle(), get_frames_with_fallback(), ol_as_ico_draw(), atlas_metadata_url(), atlas_texture(), get_atlas_texture(), get_or_fetch_atlas_data(), obj_layers_mgr_schedule_atlas_fetch()

### Community 49 - "Tap FX"
Cohesion: 0.26
Nodes (9): fx_tap_alloc_entry(), fx_tap_default_params(), fx_tap_draw(), fx_tap_draw_cross(), fx_tap_ease_out_quart(), fx_tap_init(), fx_tap_scale_px(), fx_tap_snapf() (+1 more)

### Community 50 - "Stack Move Animation"
Cohesion: 0.31
Nodes (6): container_ops_check(), confirmed(), land(), place_rect(), slide_in(), stack_move_drop()

### Community 51 - "FX Shape Primitives"
Cohesion: 0.25
Nodes (7): draw_particle(), draw_trail(), fx_reward_draw(), fx_shape_bar(), fx_shape_cooldown_bar(), fx_shape_plus(), fx_shape_spark()

### Community 52 - "Container Tab Visibility"
Cohesion: 0.24
Nodes (10): container_panel_is_open(), action_metadata(), craft_tab_visible(), leading_tab(), open_container_tab(), quest_tab_visible(), shop_tab_visible(), storage_capacity() (+2 more)

### Community 53 - "Level Up FX"
Cohesion: 0.31
Nodes (7): draw_entry(), ease_out_cubic(), exit_scale(), fx_level_up_active(), fx_level_up_draw(), fx_level_up_observe(), lane()

### Community 54 - "Layer Z Ordering"
Cohesion: 0.22
Nodes (3): ol_stack_ico_draw(), layer_z_priority(), layer_z_sort()

### Community 55 - "HUD Joystick"
Cohesion: 0.33
Nodes (4): hud_joystick_draw(), pixel_ring(), pixel_sector(), row_cells()

### Community 56 - "Death FX"
Cohesion: 0.33
Nodes (6): draw_entry(), exit_scale(), fx_death_active(), fx_death_draw(), fx_death_observe(), lane()

### Community 57 - "Pointer Gesture Input"
Cohesion: 0.39
Nodes (4): pointer_gesture_update(), release(), sector_axis(), start()

### Community 58 - "Startup & Init"
Cohesion: 0.25
Nodes (9): audio_init(), audio_start(), fetch_init(), main(), enter_game(), preload_frame(), tapped(), telemetry_event() (+1 more)

### Community 59 - "Inventory Capacity Rules"
Cohesion: 0.33
Nodes (8): local_player_inventory_has_room(), local_player_inventory_used(), local_player_is_coin(), local_player_item_quantity(), local_player_open_slots(), container_panel_accepts(), container_panel_drag_in(), craft_outputs_fit()

### Community 60 - "Stat & Capacity Contract"
Cohesion: 0.32
Nodes (4): cyberia_capacity_slots(), cyberia_capacity_unlock_level(), cyberia_stats_from_values(), cyberia_stats_values()

### Community 61 - "Asset Bridge JS"
Cohesion: 0.29
Nodes (3): asset_bridge_now(), error(), function()

### Community 62 - "Fetch Request & Font"
Cohesion: 0.43
Nodes (5): presentation_runtime_font_factor_size(), fetch_request_start(), fetch_request_start_at(), fetch_request_start_limited(), text_font_sync()

### Community 63 - "Subsystem Cleanup"
Cohesion: 0.33
Nodes (6): fx_dust_reset(), fx_footsteps_reset(), fx_tap_reset(), dev_ui_cleanup(), modal_map_cleanup(), render_cleanup()

### Community 64 - "Camera Coordinate Transform"
Cohesion: 0.50
Nodes (3): camera_get(), game_render_screen_to_world(), game_render_world_to_screen()

### Community 65 - "Resource Destruction"
Cohesion: 0.60
Nodes (5): dialogue_data_cleanup(), hash_table_destroy(), destroy_entity_render(), game_render_cleanup(), destroy_object_layers_manager()

### Community 66 - "Object Layer Alloc"
Cohesion: 0.50
Nodes (3): create_object_layer(), free_object_layer(), free_layer_value()

### Community 67 - "Config URL Helpers"
Cohesion: 0.67
Nodes (3): config_data_server_url(), open_item_viewer(), url_encode()

## Knowledge Gaps
- **3 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `copy_str()` connect `Interact Mode & Chat Bind` to `Instance Map Graph`, `Inventory Bar Quantity`, `Inventory & Dialogue Modals`, `Entity Render & HUD`, `Client Event Packing`, `Action Craft Cache`, `Notification & Grant FX`, `Player Overhead Display`, `Input & Party Events`, `Chat Log`, `World Entity Rendering`, `Interact Modal Cards`, `Dialogue Modal`, `Floating Combat Text`, `Audio Bus Control`, `Container Panel Ops`, `Audio Content Fetch`, `Container Stack Operations`, `Quest Detail Drawing`, `Audio Context Snapshot`, `Fetch Request & Font`?**
  _High betweenness centrality (0.039) - this node is a cross-community bridge._
- **Are the 63 inferred relationships involving `viewport_is_mobile()` (e.g. with `chat_font()` and `chat_row_h()`) actually correct?**
  _`viewport_is_mobile()` has 63 INFERRED edges - model-reasoned connections that need verification._
- **Should `Instance Map Graph` be split into smaller, more focused modules?**
  _Cohesion score 0.0501906941266209 - nodes in this community are weakly interconnected._
- **Why does `inventory_modal_draw()` connect `Inventory & Dialogue Modals` to `Inventory Bar Quantity`, `Quest Detail Drawing`, `Player Overhead Display`, `Container Panel Tabs`, `Chat Log`, `Interact Modal Cards`, `Dialogue Modal`, `Atlas Icon Draw`, `Interact Mode & Chat Bind`, `Interact Modal Layout`, `Stat & Capacity Contract`?**
  _High betweenness centrality (0.014) - this node is a cross-community bridge._
- **Are the 52 inferred relationships involving `copy_str()` (e.g. with `audio_play()` and `audio_update()`) actually correct?**
  _`copy_str()` has 52 INFERRED edges - model-reasoned connections that need verification._
- **Should `Inventory Bar Quantity` be split into smaller, more focused modules?**
  _Cohesion score 0.05214917825537294 - nodes in this community are weakly interconnected._
- **Should `Inventory & Dialogue Modals` be split into smaller, more focused modules?**
  _Cohesion score 0.05171717171717172 - nodes in this community are weakly interconnected._