# Graph Report - cyberia-client  (2026-10-15)

## Corpus Check
- cluster-only mode — file stats not available

## Summary
- 2105 nodes · 7125 edges · 82 communities (75 shown, 7 thin omitted)
- Extraction: 75% EXTRACTED · 25% INFERRED · 0% AMBIGUOUS · INFERRED: 1781 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `96809723`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- message.c
- modal_instance_map.c
- container_panel.c
- modal_notification.c
- inventory_bar.c
- instance_map_data.c
- ui_button_draw
- string
- chat_log.c
- quest_journal.c
- fx_reward.c
- loot_fx.c
- ui_button_hit
- math
- engine_client.c
- local_player.c
- inventory_modal.c
- socket.c
- game_render_entities
- world_cell_size
- audio.c
- EntityID
- toolbar.c
- audio_content.c
- game_state.h
- modal_interact.c
- player_panel.c
- game_frame
- copy_str
- ui_dispatch_tap
- input.c
- game_client.c
- interaction_bubble_draw
- viewport_is_mobile
- prediction_reconcile
- render_on_tick
- object_layers_management.c
- text_line_height
- container_ops.c
- fx_shape_spark_shaded
- modal_anchor_layout_update
- quest_card_layout
- modal_instance_map_close
- draw_entity_layers
- hash_table.c
- inventory_modal_handle_click
- dev_ui_draw
- interaction_bubble.c
- fx_tap.c
- create_object_layer
- get_or_fetch_atlas_data
- emit_step
- local_player_open_slots
- game_render_init
- hud_side_stack.c
- modal_draw_card
- chat_panel.c
- audio_context.c
- quest_progress_store.c
- game_render_frame
- lookup_cached_layer
- modal_dialogue_open
- hud_joystick.c
- camera.c
- stack_move.c
- asset_bridge.c
- scan_entity
- utils
- prediction_on_snapshot
- meta_cache.c
- open_item_viewer
- cJSON
- main
- fx_level_up.c
- draw_skill_previews
- ol_as_ico_draw
- stat_panel.c
- fullscreen_bridge.c
- local_player_interaction
- cyberia_stats_values

## God Nodes (most connected - your core abstractions)
1. `viewport_is_mobile()` - 65 edges
2. `copy_str()` - 53 edges
3. `inventory_modal_draw()` - 50 edges
4. `ui_button_hit()` - 47 edges
5. `modal_interact_draw()` - 41 edges
6. `inventory_bar_draw()` - 40 edges
7. `game_render_entities()` - 37 edges
8. `render_on_tick()` - 34 edges
9. `modal_interact_update()` - 32 edges
10. `modal_notification_draw()` - 32 edges

## Surprising Connections (you probably didn't know these)
- `json_unpack_init_data()` --calls--> `quest_progress_store_reset()`  [INFERRED]
  src/net/message.c → src/data/quest_progress_store.c
- `json_unpack_snapshot()` --calls--> `fetch_deprioritize()`  [INFERRED]
  src/net/message.c → src/data/engine_client.c
- `unpack_self()` --calls--> `local_player_set_action_cooldown()`  [INFERRED]
  src/net/message.c → src/sim/local_player.c
- `unpack_self()` --calls--> `local_player_set_action_ready_at()`  [INFERRED]
  src/net/message.c → src/sim/local_player.c
- `unpack_self()` --calls--> `local_player_set_consume_ready_at()`  [INFERRED]
  src/net/message.c → src/sim/local_player.c

## Import Cycles
- None detected.

## Communities (82 total, 7 thin omitted)

### Community 0 - "message.c"
Cohesion: 0.06
Nodes (79): read_config(), settings(), fx_hit_burst_spawn(), client_event_flush(), pack_event(), network_send(), on_socket_open(), on_socket_receive() (+71 more)

### Community 1 - "modal_instance_map.c"
Cohesion: 0.05
Nodes (80): presentation_runtime_status_icon(), instance_map_cell_to_card(), instance_map_data_graph(), instance_map_data_state(), instance_map_presence_color(), ease_smoothstep(), centered_square(), close_bounds() (+72 more)

### Community 2 - "container_panel.c"
Cohesion: 0.06
Nodes (55): container_panel_accepts(), container_panel_animate_from(), container_panel_cell_at(), container_panel_cell_rect(), container_panel_drag_in(), container_panel_draw(), container_panel_draw_drag(), container_panel_open_storage() (+47 more)

### Community 3 - "modal_notification.c"
Cohesion: 0.09
Nodes (55): fx_grant_delivery_waiting(), ease_in_cubic(), ease_out_cubic(), add_entry_item(), begin_result_delivery(), draw_progress_bar(), hold_result_items(), mn_ok_pulse() (+47 more)

### Community 4 - "inventory_bar.c"
Cohesion: 0.06
Nodes (83): fx_grant_delivery_begin(), fx_grant_delivery_update(), grant_landed(), release_spent(), alloc_entry(), changed_cell(), draw_popup(), find() (+75 more)

### Community 5 - "instance_map_data.c"
Cohesion: 0.14
Nodes (19): apply_dynamic_capabilities(), clear_dynamic_capabilities(), find_presence_poi(), instance_map_data_find_node(), instance_map_data_generation(), instance_map_data_node_active_actions(), instance_map_data_node_active_quests(), instance_map_data_update() (+11 more)

### Community 6 - "ui_button_draw"
Cohesion: 0.14
Nodes (22): draw_capability_bar(), draw_capability_item(), draw_centered_label(), draw_centered_outlined_label(), draw_hp_bar(), draw_nameplate(), draw_pill(), draw_transport_bar() (+14 more)

### Community 7 - "string"
Cohesion: 0.08
Nodes (16): adopt_js_string(), config_init(), on_hints_fetched(), parse_color_object(), parse_response(), world_interpolation_ms(), game_state_layer_pool_reset(), game_state_reset() (+8 more)

### Community 9 - "chat_log.c"
Cohesion: 0.15
Nodes (20): append(), chat_log_get(), chat_log_mark_read(), chat_log_push(), chat_log_push_system(), chat_log_unread(), chat_log_unread_direct(), find() (+12 more)

### Community 10 - "quest_journal.c"
Cohesion: 0.07
Nodes (55): ease_approach(), ease_exp_factor(), column_content_height(), column_ensure_toggle(), column_hidden(), column_input_bounds(), column_scroll_view(), column_scrollbar_bounds() (+47 more)

### Community 12 - "fx_reward.c"
Cohesion: 0.06
Nodes (52): card_center(), current_margin(), fx_assemble_draw(), fx_assemble_init(), fx_assemble_show(), fx_assemble_update(), lcg_f01(), lcg_range() (+44 more)

### Community 13 - "loot_fx.c"
Cohesion: 0.10
Nodes (31): drop_alloc(), drop_find(), expend_target(), id_phase(), lcg_f01(), lcg_range(), loot_fx_delivery_token_at(), loot_fx_delivery_token_slot_count() (+23 more)

### Community 14 - "ui_button_hit"
Cohesion: 0.21
Nodes (18): card_title_wrap(), draw_bar_button(), draw_card_line(), draw_quest_grid_action_button(), draw_quest_grid_button(), draw_shop_card(), mi_font_btn(), mi_shop_price_font() (+10 more)

### Community 15 - "math"
Cohesion: 0.11
Nodes (10): draw_entry(), exit_scale(), fx_death_draw(), lane(), fx_heal_aura_reset(), fx_hit_burst_draw(), fx_hit_burst_reset(), fx_hit_burst_update() (+2 more)

### Community 16 - "engine_client.c"
Cohesion: 0.14
Nodes (20): presentation_runtime_start_fetch(), complete(), deliver(), discard(), enqueue(), fetch_deprioritize(), fetch_has_budget(), fetch_now() (+12 more)

### Community 17 - "local_player.c"
Cohesion: 0.09
Nodes (28): client_event_push(), local_player_fct_at(), local_player_fct_clear(), local_player_fct_count(), local_player_fct_push(), local_player_invite_from(), local_player_party_member(), local_player_request_craft() (+20 more)

### Community 18 - "inventory_modal.c"
Cohesion: 0.19
Nodes (24): close_rect(), dir_has_frames(), draw_small_btn(), im_button_h(), im_button_w(), im_close_size(), im_dir_button_gap(), im_dir_button_h() (+16 more)

### Community 19 - "socket.c"
Cohesion: 0.16
Nodes (9): connection_close(), socket_close(), socket_is_open(), socket_onclose_internal(), socket_onerror_internal(), socket_onmessage_internal(), socket_onopen_internal(), socket_open() (+1 more)

### Community 20 - "game_render_entities"
Cohesion: 0.10
Nodes (21): action_cache_fetch(), fx_death_active(), fx_death_observe(), fx_heal_aura_observe(), fx_level_up_active(), fx_level_up_observe(), fx_obstacle_burst_snapshot(), game_state_obstacle_depleted() (+13 more)

### Community 21 - "world_cell_size"
Cohesion: 0.22
Nodes (16): presentation_runtime_dev_ui(), world_cell_size(), fx_heal_aura_draw(), fx_obstacle_burst_draw(), fx_portal_aura_draw(), game_state_appearance(), game_state_data(), fct_draw() (+8 more)

### Community 22 - "audio.c"
Cohesion: 0.11
Nodes (36): apply_volume(), audio_get_volume(), audio_is_muted(), audio_params(), audio_play(), audio_request_absent(), audio_request_code(), audio_request_complete() (+28 more)

### Community 23 - "EntityID"
Cohesion: 0.09
Nodes (14): fx_footsteps_init(), fx_footsteps_update(), game_state_agent(), game_state_bot_has_feet(), game_state_is_player(), game_state_motion(), game_state_player(), game_state_portal() (+6 more)

### Community 24 - "toolbar.c"
Cohesion: 0.20
Nodes (18): chat_panel_is_visible(), modal_map_draw(), btn_rect(), close_interact_inventory_chain(), dismiss_world_covers(), draw_badge(), draw_btn(), interact_inventory_chain_is_open() (+10 more)

### Community 25 - "audio_content.c"
Cohesion: 0.19
Nodes (17): audio_preload(), apply_current(), audio_content_fetch(), audio_content_music_finished(), audio_content_restore_music(), audio_content_start(), audio_content_update(), audio_prefetch_map() (+9 more)

### Community 26 - "game_state.h"
Cohesion: 0.10
Nodes (3): function(), draw_centered(), draw_splash()

### Community 27 - "modal_interact.c"
Cohesion: 0.10
Nodes (52): container_panel_is_open(), modal_anchor_active(), modal_dialogue_is_collapsed(), modal_dialogue_set_quest_style(), modal_dialogue_show_fullscreen(), action_metadata(), active_dlg_key(), active_tab_content_height() (+44 more)

### Community 28 - "player_panel.c"
Cohesion: 0.22
Nodes (21): game_state_has(), local_player_leads_party(), avatar(), avatar_size(), bar(), button(), button_h(), draw_other_profile() (+13 more)

### Community 29 - "game_frame"
Cohesion: 0.12
Nodes (23): audio_start(), presentation_runtime_font_factor_size(), presentation_runtime_font_family(), presentation_runtime_is_ready(), fetch_frame_begin(), fetch_frame_end(), frame(), game_frame() (+15 more)

### Community 30 - "copy_str"
Cohesion: 0.09
Nodes (25): chat_direct(), chat_pane_hide(), container_panel_close(), open_slot_at(), inventory_modal_set_anchor_entity(), inventory_modal_set_on_close(), draw_craft_stack(), es_clear() (+17 more)

### Community 31 - "ui_dispatch_tap"
Cohesion: 0.21
Nodes (16): presentation_runtime_toggle_dev_ui(), fx_tap_default_params(), fx_tap_spawn(), modals_block_bubbles(), inventory_modal_is_open(), modal_dialogue_is_item_lore(), modal_dialogue_is_open(), modal_instance_map_is_open() (+8 more)

### Community 32 - "input.c"
Cohesion: 0.15
Nodes (17): axis_dir(), emit_steer_tap(), emit_tap(), input_pointer_gesture(), input_pop(), input_push(), input_push_client_events(), input_queue_filter() (+9 more)

### Community 33 - "game_client.c"
Cohesion: 0.11
Nodes (14): audio_context_reset(), fx_death_reset(), fx_level_up_reset(), client_event_reset(), client_reset_state(), connection_get_stats(), on_socket_close(), local_player_reset() (+6 more)

### Community 34 - "interaction_bubble_draw"
Cohesion: 0.11
Nodes (30): chat_pane_draw(), interaction_bubble_draw(), slot_rect(), draw_dialogue_sprite(), craft_card_height(), craft_recipe_ready(), craft_slot_size(), draw_card_base() (+22 more)

### Community 35 - "viewport_is_mobile"
Cohesion: 0.10
Nodes (31): chat_font(), dlg_fullscreen(), dlg_hidden(), dlg_hidden_on_mobile(), dlg_pad(), dlg_side_pad(), dlg_sprite_size(), draw_quest_talk_btn() (+23 more)

### Community 36 - "prediction_reconcile"
Cohesion: 0.12
Nodes (17): walk_speed_ref(), client_event_list(), client_event_trim(), client_on_init_received(), local_player_in_stasis(), local_player_move_speed(), local_player_set_stasis(), apply_facts() (+9 more)

### Community 37 - "render_on_tick"
Cohesion: 0.12
Nodes (20): presentation_runtime_overhead_occlusion_fade_ms(), presentation_runtime_overhead_occlusion_hidden_opacity(), fx_death_update(), fx_heal_aura_update(), fx_level_up_update(), fx_obstacle_burst_update(), fx_portal_aura_update(), fx_tap_update() (+12 more)

### Community 38 - "object_layers_management.c"
Cohesion: 0.16
Nodes (16): create_atlas_sprite_sheet_data(), free_atlas_sprite_sheet_data(), ledger_standard_from_string(), free_atlas_value(), json_get_bool_safe(), json_get_int_safe(), json_get_string_safe(), on_atlas_meta_fetched() (+8 more)

### Community 39 - "text_line_height"
Cohesion: 0.19
Nodes (12): draw_quest_step_objectives(), draw_quest_steps(), quest_objective_verb(), effective_factor(), text_active_font(), text_draw_compat(), text_draw_outlined(), text_line_height() (+4 more)

### Community 40 - "container_ops.c"
Cohesion: 0.26
Nodes (15): apply(), bankable_while_worn(), check(), container_ops_apply(), container_ops_bind_vault(), container_ops_move(), item_type_of(), list_of() (+7 more)

### Community 41 - "fx_shape_spark_shaded"
Cohesion: 0.17
Nodes (11): draw_entry(), ease_out_cubic(), exit_scale(), lane(), draw_entity(), lane(), fx_shape_bar(), fx_shape_cooldown_bar() (+3 more)

### Community 42 - "modal_anchor_layout_update"
Cohesion: 0.26
Nodes (12): anchored_card_rect(), update_anchor_layout(), entity_point(), fit_into(), modal_anchor_capture(), modal_anchor_card_size(), modal_anchor_layout_reset(), modal_anchor_layout_update() (+4 more)

### Community 44 - "modal_instance_map_close"
Cohesion: 0.16
Nodes (18): instance_map_data_close(), instance_map_data_open(), fx_dust_reset(), fx_footsteps_reset(), fx_tap_reset(), input_gestures_set_blocked(), dev_ui_cleanup(), hud_minimap_overlay_cleanup() (+10 more)

### Community 45 - "draw_entity_layers"
Cohesion: 0.18
Nodes (8): anim_pool_alloc(), draw_dev_ui_box(), draw_entity_layers(), get_animation_state(), get_frame_count_and_direction(), layers_render_ready(), pick_available_direction(), atlas_get_direction_frames()

### Community 46 - "hash_table.c"
Cohesion: 0.28
Nodes (10): find_insert_slot(), find_occupied(), hash_string(), hash_table_contains(), hash_table_put(), hash_table_remove(), hash_table_remove_if(), raw_insert() (+2 more)

### Community 47 - "inventory_modal_handle_click"
Cohesion: 0.17
Nodes (20): dialogue_data_available(), dialogue_data_get(), game_state_is_active_item_type(), game_state_is_dead_item(), local_player_status_icon(), activate_enabled(), card_rect(), current_ols() (+12 more)

### Community 48 - "dev_ui_draw"
Cohesion: 0.17
Nodes (8): game_state_progress(), own_level(), dev_ui_draw(), dev_ui_get_active_item_count(), dev_ui_get_effective_stats_sum(), direction_to_string(), mode_to_string(), local_player_view_direction()

### Community 49 - "interaction_bubble.c"
Cohesion: 0.13
Nodes (4): dev_ui_on_tick(), dev_ui_update_network_stats(), item_type_of(), storable()

### Community 50 - "fx_tap.c"
Cohesion: 0.29
Nodes (6): fx_tap_alloc_entry(), fx_tap_draw(), fx_tap_draw_cross(), fx_tap_ease_out_quart(), fx_tap_scale_px(), fx_tap_snapf()

### Community 51 - "create_object_layer"
Cohesion: 0.50
Nodes (3): create_object_layer(), free_object_layer(), free_layer_value()

### Community 52 - "get_or_fetch_atlas_data"
Cohesion: 0.21
Nodes (10): dialogue_data_request(), dialogue_data_request_code(), on_dialogue_fetched(), parse_response(), hash_table_get(), atlas_metadata_url(), atlas_texture(), get_atlas_texture() (+2 more)

### Community 53 - "emit_step"
Cohesion: 0.24
Nodes (6): cell_size(), emit_step(), step_is_visible(), step_params(), track_entity(), tracker_for()

### Community 54 - "local_player_open_slots"
Cohesion: 0.29
Nodes (8): cyberia_capacity_slots(), cyberia_capacity_unlock_level(), local_player_inventory_has_room(), local_player_inventory_used(), local_player_is_coin(), local_player_item_quantity(), local_player_open_slots(), craft_outputs_fit()

### Community 55 - "game_render_init"
Cohesion: 0.14
Nodes (15): dialogue_data_cleanup(), dialogue_data_init(), hash_table_destroy(), hash_table_init(), interaction_bubble_init(), inventory_modal_init(), modal_dialogue_init(), modal_notification_init() (+7 more)

### Community 56 - "hud_side_stack.c"
Cohesion: 0.37
Nodes (11): hud_minimap_overlay_is_visible(), chat_height(), chat_reserve(), column_x(), hud_side_stack_available(), hud_side_stack_chat_slot(), hud_side_stack_journal_slot(), hud_side_stack_map_slot() (+3 more)

### Community 57 - "modal_draw_card"
Cohesion: 0.29
Nodes (7): shadow_text(), modal_draw_card(), modal_draw_clipped_text(), modal_draw_float_shadow(), modal_draw_title(), modal_pop_alpha(), toolbar_toggle_right()

### Community 58 - "chat_panel.c"
Cohesion: 0.23
Nodes (16): chat_channel(), chat_panel_covers_point(), chat_panel_desired_height(), chat_panel_draw(), chat_panel_handle_click(), chat_panel_handle_wheel(), chat_panel_toggle(), chat_panel_unread() (+8 more)

### Community 59 - "audio_context.c"
Cohesion: 0.21
Nodes (12): audio_context_snapshot(), audio_context_update(), craft_snapshot(), crossed_milestone(), entity_snapshot(), portal_snapshot(), restore_music(), scan_entity() (+4 more)

### Community 60 - "quest_progress_store.c"
Cohesion: 0.23
Nodes (12): find_by_code(), quest_progress_store_apply(), quest_progress_store_count(), quest_progress_store_find(), quest_progress_store_get(), quest_progress_store_is_completed(), quest_progress_store_parse_status(), quest_progress_store_reset() (+4 more)

### Community 61 - "game_render_frame"
Cohesion: 0.13
Nodes (12): presentation_runtime_camera_zoom(), prediction_self(), fct_draw_overlay(), camera_get(), camera_init(), camera_resize(), camera_sync_mobile_zoom(), game_render_frame() (+4 more)

### Community 62 - "lookup_cached_layer"
Cohesion: 0.20
Nodes (5): interaction_bubble_dead_equip(), layer_z_priority(), layer_z_sort(), active_layer_item_id(), lookup_cached_layer()

### Community 63 - "modal_dialogue_open"
Cohesion: 0.13
Nodes (13): inventory_modal_handle_content_click(), inventory_modal_open(), inventory_modal_open_external(), inventory_modal_switch_slot(), on_dialogue_close_reopen(), rebuild_dir_str(), reset_view_state(), shown_inv_idx() (+5 more)

### Community 64 - "hud_joystick.c"
Cohesion: 0.33
Nodes (4): hud_joystick_draw(), pixel_ring(), pixel_sector(), row_cells()

### Community 66 - "stack_move.c"
Cohesion: 0.27
Nodes (7): container_ops_check(), container_ops_stack(), confirmed(), land(), place_rect(), slide_in(), stack_move_drop()

### Community 67 - "asset_bridge.c"
Cohesion: 0.29
Nodes (3): asset_bridge_now(), error(), function()

### Community 68 - "scan_entity"
Cohesion: 0.24
Nodes (9): presentation_runtime_entity_fallback_color(), presentation_runtime_palette(), presentation_runtime_status_border(), copy_active_layers(), find_slot(), interaction_bubble_get_alive_layers(), scan_entity(), status_border_color() (+1 more)

### Community 69 - "utils"
Cohesion: 0.22
Nodes (3): action_cache_get(), on_action_fetched(), on_quest_fetched()

### Community 70 - "prediction_on_snapshot"
Cohesion: 0.40
Nodes (3): prediction_on_snapshot(), prediction_prev(), session_next_input_sequence()

### Community 71 - "meta_cache.c"
Cohesion: 0.38
Nodes (6): find_or_create(), head_at(), meta_cache_fetch(), meta_cache_find(), meta_cache_on_fetched(), quest_cache_get()

### Community 72 - "open_item_viewer"
Cohesion: 0.67
Nodes (3): config_data_server_url(), open_item_viewer(), url_encode()

### Community 73 - "cJSON"
Cohesion: 0.36
Nodes (5): ingest_craft_items(), ingest_doc(), ingest_quest_doc(), json_int(), json_str()

### Community 74 - "main"
Cohesion: 0.22
Nodes (8): audio_init(), config_ws_url(), fetch_init(), main(), connection_open(), message_set_init_handler(), preload_start(), text_font_init()

### Community 75 - "fx_level_up.c"
Cohesion: 0.39
Nodes (5): draw_entry(), ease_out_cubic(), exit_scale(), fx_level_up_draw(), lane()

### Community 76 - "draw_skill_previews"
Cohesion: 0.33
Nodes (5): cooldown_charge(), local_player_action_charge(), local_player_consume_charge(), draw_skill_previews(), skill_preview_layout()

### Community 77 - "ol_as_ico_draw"
Cohesion: 0.29
Nodes (3): fallback_circle(), get_frames_with_fallback(), ol_as_ico_draw()

### Community 78 - "stat_panel.c"
Cohesion: 0.48
Nodes (4): stat_panel_draw(), stat_panel_grid_draw(), stat_panel_sum_draw(), value_color()

## Knowledge Gaps
- **7 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `copy_str()` connect `copy_str` to `message.c`, `modal_instance_map.c`, `container_panel.c`, `modal_notification.c`, `inventory_bar.c`, `instance_map_data.c`, `string`, `chat_log.c`, `ui_button_hit`, `local_player.c`, `inventory_modal.c`, `audio.c`, `audio_content.c`, `player_panel.c`, `game_frame`, `interaction_bubble_draw`, `viewport_is_mobile`, `text_line_height`, `container_ops.c`, `inventory_modal_handle_click`, `audio_context.c`, `quest_progress_store.c`, `modal_dialogue_open`, `meta_cache.c`, `cJSON`, `draw_skill_previews`?**
  _High betweenness centrality (0.029) - this node is a cross-community bridge._
- **Are the 64 inferred relationships involving `viewport_is_mobile()` (e.g. with `chat_font()` and `chat_row_h()`) actually correct?**
  _`viewport_is_mobile()` has 64 INFERRED edges - model-reasoned connections that need verification._
- **Should `message.c` be split into smaller, more focused modules?**
  _Cohesion score 0.06265143648321218 - nodes in this community are weakly interconnected._
- **Why does `on_window_resize()` connect `game_render_frame` to `game_render.c`?**
  _High betweenness centrality (0.021) - this node is a cross-community bridge._
- **Are the 52 inferred relationships involving `copy_str()` (e.g. with `audio_play()` and `audio_update()`) actually correct?**
  _`copy_str()` has 52 INFERRED edges - model-reasoned connections that need verification._
- **Should `modal_instance_map.c` be split into smaller, more focused modules?**
  _Cohesion score 0.05486327448944271 - nodes in this community are weakly interconnected._
- **Why does `ui_button_hit()` connect `ui_button_hit` to `modal_instance_map.c`, `interaction_bubble_draw`, `container_panel.c`, `inventory_bar.c`, `viewport_is_mobile`, `modal_notification.c`, `ui_button_draw`, `chat_log.c`, `quest_journal.c`, `inventory_modal_handle_click`, `inventory_modal.c`, `toolbar.c`, `chat_panel.c`, `modal_interact.c`, `player_panel.c`, `copy_str`, `modal_dialogue_open`?**
  _High betweenness centrality (0.019) - this node is a cross-community bridge._