# Graph Report - cyberia-client  (2026-10-29)

## Corpus Check
- 239 files · ~193,310 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2643 nodes · 9780 edges · 100 communities (92 shown, 8 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 2618 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `7fac7b03`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- message.c
- hud_minimap_overlay.c
- item_slot_grid.c
- modal_notification.c
- inventory_bar.c
- instance_map_data.c
- ui_button_pixel_retro_draw
- string
- chat_pane.c
- quest_journal.c
- modal_instance_map.c
- fx_reward.c
- fx_loot.c
- viewport_is_mobile
- fade_c
- engine_client.c
- copy_str
- inventory_modal.c
- socket.c
- game_render_entities
- world_cell_size
- audio.c
- open_session
- map_marker.c
- audio_content.c
- utils.h
- modal_interact.c
- player_panel.c
- preload_frame
- action_metadata
- game_render_ui
- input_queue_on_tick
- fx_level_up.c
- Rectangle
- map_detail.c
- prediction_reconcile
- render_on_tick
- object_layers_management.c
- text_wrap
- container_ops.c
- fx_shapes.c
- resolve_click
- quest_target.c
- hud_side_stack.c
- draw_entity_layers
- hash_table.c
- dialogue_data_get
- game_frame
- local_player.h
- ui_consume_event
- create_object_layer
- get_or_fetch_atlas_data
- emit_step
- container_panel.c
- game_render_init
- social_panel.c
- modal_draw_clipped_text
- chat_panel.c
- market_panel.c
- quest_progress_store.c
- camera_init
- merchant_panel.c
- session_gate.c
- replication.c
- trade_panel.c
- land
- asset_bridge.c
- scan_entity
- fx_assemble.c
- chat_log.c
- fetch_data_release
- open_item_viewer
- ui_tabs.c
- fetch_request_start
- fx_dust_spawn
- fct_spawn
- ol_as_ico_draw
- get_animation_state
- session_gate_draw
- local_player_interaction
- cyberia_stats_values
- game_client_on_tick
- config.c
- item_slot_draw_ex
- game_render_world_to_screen
- viewport.h
- render_init
- draw_loot_screen_fx
- game_render_fx_loot
- pointer_gesture_update
- fx_dust.c
- overhead_occlusion_opacity
- local_player_request_trade_offer
- identity_icon_draw
- dev_ui_on_tick
- local_player_request_item_drop
- free_atlas_sprite_sheet_data
- ledger_standard_from_string
- container_panel_cell_at

## God Nodes (most connected - your core abstractions)
1. `copy_str()` - 106 edges
2. `viewport_is_mobile()` - 84 edges
3. `ui_button_hit()` - 66 edges
4. `instance_map_data_graph()` - 54 edges
5. `inventory_modal_draw()` - 51 edges
6. `serial_get_string()` - 48 edges
7. `modal_interact_draw()` - 40 edges
8. `client_event_push()` - 38 edges
9. `game_render_entities()` - 38 edges
10. `inventory_bar_draw()` - 37 edges

## Surprising Connections (you probably didn't know these)
- `json_unpack_snapshot()` --calls--> `fetch_deprioritize()`  [INFERRED]
  src/net/message.c → src/data/engine_client.c
- `fetch_now()` --calls--> `asset_bridge_now()`  [INFERRED]
  src/data/engine_client.c → src/js/asset_bridge.c
- `nearer()` --calls--> `instance_map_data_graph()`  [INFERRED]
  src/data/quest_target.c → src/data/instance_map_data.c
- `build_map()` --calls--> `instance_map_data_node_active_quests()`  [INFERRED]
  src/ui/map_detail.c → src/data/instance_map_data.c
- `build_map()` --calls--> `instance_map_data_node_active_actions()`  [INFERRED]
  src/ui/map_detail.c → src/data/instance_map_data.c

## Import Cycles
- None detected.

## Communities (100 total, 8 thin omitted)

### Community 0 - "message.c"
Cohesion: 0.06
Nodes (115): audio_context_snapshot(), audio_context_update(), craft_snapshot(), entity_snapshot(), portal_snapshot(), restore_music(), audio_event(), read_config() (+107 more)

### Community 1 - "hud_minimap_overlay.c"
Cohesion: 0.16
Nodes (28): ImapGraph, ImapNode, instance_map_cell_to_card(), instance_map_edge_destination(), instance_map_exit_direction(), instance_map_local_rect(), centered_square(), close_bounds() (+20 more)

### Community 2 - "item_slot_grid.c"
Cohesion: 0.15
Nodes (23): container_panel_draw_drag(), emit(), end_gesture(), grid_point_in_visible_bounds(), grid_rows(), item_slot_grid_animate_from_point(), item_slot_grid_animate_move(), item_slot_grid_cell_at() (+15 more)

### Community 3 - "modal_notification.c"
Cohesion: 0.08
Nodes (64): fx_grant_delivery_begin(), fx_grant_delivery_update(), fx_grant_delivery_waiting(), grant_landed(), release_spent(), fx_inventory_bar_qty_hold_for_delivery(), fx_inventory_bar_qty_notify_arrival(), ease_in_cubic() (+56 more)

### Community 4 - "inventory_bar.c"
Cohesion: 0.06
Nodes (85): cyberia_capacity_slots(), cyberia_capacity_unlock_level(), alloc_entry(), changed_cell(), draw_popup(), find(), fx_inventory_bar_qty_display(), fx_inventory_bar_qty_draw() (+77 more)

### Community 5 - "instance_map_data.c"
Cohesion: 0.08
Nodes (41): compose_maps(), fetch_static(), find_presence_poi(), ImapBounds, ImapEntrySpawn, ImapNodeLinks, ImapPlaced, ImapPopulation (+33 more)

### Community 6 - "ui_button_pixel_retro_draw"
Cohesion: 0.05
Nodes (49): emoji_icon_of(), emoji_px(), emoji_text_width(), emoji_text_wrap(), layout(), draw_capability_bar(), draw_capability_item(), draw_centered_label() (+41 more)

### Community 7 - "string"
Cohesion: 0.13
Nodes (5): on_hints_fetched(), parse_color_object(), parse_response(), presentation_runtime_font_family(), text_font_settled()

### Community 9 - "chat_pane.c"
Cohesion: 0.25
Nodes (14): chat_font(), chat_layout(), chat_pane_bind(), chat_pane_draw(), chat_pane_hide(), chat_pane_press(), chat_pane_update(), chat_pane_wheel() (+6 more)

### Community 10 - "quest_journal.c"
Cohesion: 0.07
Nodes (63): container_panel_press(), ease_approach(), ease_exp_factor(), column_content_height(), column_ensure_toggle(), column_hidden(), column_input_bounds(), column_scroll_view() (+55 more)

### Community 11 - "modal_instance_map.c"
Cohesion: 0.10
Nodes (62): instance_map_data_find_node(), instance_map_data_graph(), map_marker_size(), add_hit(), canvas_color(), cell_px(), close_rect(), detail_card_rect() (+54 more)

### Community 12 - "fx_reward.c"
Cohesion: 0.18
Nodes (20): card_center(), current_margin(), draw_particle(), draw_trail(), ellipse_point(), fx_reward_draw(), fx_reward_init(), fx_reward_show() (+12 more)

### Community 13 - "fx_loot.c"
Cohesion: 0.17
Nodes (21): drop_alloc(), drop_find(), expend_target(), fx_loot_drop_render_pos(), fx_loot_note_spawn(), fx_loot_push(), fx_loot_reward_delivery(), fx_loot_slot_expend_at() (+13 more)

### Community 14 - "viewport_is_mobile"
Cohesion: 0.10
Nodes (39): dlg_hidden(), dlg_hidden_on_mobile(), dlg_pad(), dlg_side_pad(), dlg_sprite_size(), draw_quest_talk_btn(), modal_dialogue_draw(), modal_dialogue_update() (+31 more)

### Community 15 - "fade_c"
Cohesion: 0.11
Nodes (34): ImapEdge, universe_map_data_graph(), universe_map_data_stale(), card_reach(), draw_edge_ends(), draw_edge_line(), draw_node_name(), draw_picture() (+26 more)

### Community 16 - "engine_client.c"
Cohesion: 0.16
Nodes (19): complete(), deliver(), discard(), enqueue(), fetch_account(), fetch_deprioritize(), fetch_event(), fetch_forget() (+11 more)

### Community 17 - "copy_str"
Cohesion: 0.08
Nodes (48): client_event_push(), game_client_hold_resume(), cooldown_charge(), local_player_action_charge(), local_player_confirm_quote(), local_player_consume_charge(), local_player_incoming_requests(), local_player_market_bid() (+40 more)

### Community 18 - "inventory_modal.c"
Cohesion: 0.05
Nodes (77): game_state_is_active_item_type(), game_state_is_dead_item(), local_player_status_icon(), identity_appearance_ref(), interaction_bubble_dead_equip(), activate_enabled(), anchored_card_rect(), card_rect() (+69 more)

### Community 19 - "socket.c"
Cohesion: 0.18
Nodes (10): connection_close(), socket_close(), socket_is_active(), socket_is_open(), socket_onclose_internal(), socket_onerror_internal(), socket_onmessage_internal(), socket_onopen_internal() (+2 more)

### Community 20 - "game_render_entities"
Cohesion: 0.06
Nodes (35): presentation_runtime_entity_fallback_color(), presentation_runtime_palette(), fx_death_active(), fx_death_observe(), fx_heal_aura_observe(), fx_level_up_active(), fx_level_up_observe(), fx_obstacle_burst_snapshot() (+27 more)

### Community 21 - "world_cell_size"
Cohesion: 0.13
Nodes (25): presentation_runtime_dev_ui(), world_cell_size(), draw_entry(), exit_scale(), fx_death_draw(), lane(), fx_heal_aura_draw(), fx_hit_burst_draw() (+17 more)

### Community 22 - "audio.c"
Cohesion: 0.21
Nodes (20): apply_volume(), audio_get_volume(), audio_params(), audio_play(), audio_set_muted(), audio_set_volume(), audio_shutdown(), audio_stop_music() (+12 more)

### Community 23 - "open_session"
Cohesion: 0.15
Nodes (19): inventory_modal_switch_slot(), market_panel_reset(), merchant_panel_reset(), modal_dialogue_set_quest_style(), modal_dialogue_show_fullscreen(), es_clear(), es_pop(), modal_interact_discard_stack() (+11 more)

### Community 24 - "map_marker.c"
Cohesion: 0.16
Nodes (28): draw_bracket(), draw_count(), draw_plate(), faded(), fill_triangle(), label_arrow(), label_value_w(), map_marker_draw_arrow() (+20 more)

### Community 25 - "audio_content.c"
Cohesion: 0.14
Nodes (22): audio_preload(), audio_request_absent(), audio_request_code(), audio_request_priority(), apply_current(), audio_content_fetch(), audio_content_music_finished(), audio_content_restore_music() (+14 more)

### Community 27 - "modal_interact.c"
Cohesion: 0.12
Nodes (41): modal_anchor_active(), modal_dialogue_is_collapsed(), active_dlg_key(), active_tab_content_height(), anchor_content_card_height(), anchor_safe_area(), anchored_card_rect(), bar_buttons() (+33 more)

### Community 28 - "player_panel.c"
Cohesion: 0.19
Nodes (27): armed(), bar(), bind_friend(), button(), button_h(), draw_friend_list(), draw_friends(), draw_party() (+19 more)

### Community 29 - "preload_frame"
Cohesion: 0.19
Nodes (12): audio_start(), fetch_frame_begin(), fetch_frame_end(), frame(), draw_centered(), draw_gate(), draw_splash(), draw_wordmark() (+4 more)

### Community 30 - "action_metadata"
Cohesion: 0.12
Nodes (18): inventory_modal_set_anchor_entity(), inventory_modal_set_on_close(), action_metadata(), craft_tab_visible(), es_push(), handle_craft_click(), handle_quest_click(), handle_shop_click() (+10 more)

### Community 31 - "game_render_ui"
Cohesion: 0.16
Nodes (25): container_panel_close(), modals_block_bubbles(), inventory_modal_is_open(), dlg_fullscreen(), emit_dlg_complete(), modal_dialogue_close(), modal_dialogue_finish(), modal_dialogue_handle_click() (+17 more)

### Community 32 - "input_queue_on_tick"
Cohesion: 0.15
Nodes (16): fx_tap_draw(), axis_dir(), emit_steer_tap(), emit_tap(), input_pointer_gesture(), input_pop(), input_push(), input_push_client_events() (+8 more)

### Community 33 - "fx_level_up.c"
Cohesion: 0.11
Nodes (20): audio_context_reset(), fx_death_reset(), draw_entry(), ease_out_cubic(), exit_scale(), fx_heal_aura_reset(), lane(), fx_hit_burst_reset() (+12 more)

### Community 34 - "Rectangle"
Cohesion: 0.12
Nodes (32): local_player_inventory_has_room(), local_player_inventory_used(), local_player_is_coin(), local_player_item_quantity(), local_player_open_slots(), container_panel_draw(), craft_card_height(), craft_fee_h() (+24 more)

### Community 35 - "map_detail.c"
Cohesion: 0.10
Nodes (43): presentation_runtime_status_icon(), ImapPresencePoi, ImapProvider, ImapQuestState, ImapService, instance_map_node_label(), instance_map_presence_color(), instance_map_quest_state() (+35 more)

### Community 36 - "prediction_reconcile"
Cohesion: 0.13
Nodes (15): walk_speed_ref(), client_event_list(), client_event_resend(), client_event_trim(), local_player_move_speed(), local_player_set_stasis(), apply_facts(), apply_motion() (+7 more)

### Community 37 - "render_on_tick"
Cohesion: 0.11
Nodes (25): presentation_runtime_overhead_occlusion_fade_ms(), fx_death_update(), fx_dust_update(), fx_heal_aura_update(), fx_hit_burst_update(), fx_level_up_update(), fx_obstacle_burst_update(), fx_portal_aura_update() (+17 more)

### Community 38 - "object_layers_management.c"
Cohesion: 0.21
Nodes (13): is_object_layer_cid(), json_get_bool_safe(), json_get_int_safe(), json_get_string_safe(), on_atlas_meta_fetched(), parse_direction_frame_data(), parse_item(), parse_ledger() (+5 more)

### Community 39 - "text_wrap"
Cohesion: 0.14
Nodes (16): local_player_invite_from(), local_player_invite_to(), local_player_leads_party(), local_player_party_member(), local_player_xp_ratio(), draw_other_profile(), draw_own_profile(), font_title() (+8 more)

### Community 40 - "container_ops.c"
Cohesion: 0.22
Nodes (16): apply(), bankable_while_worn(), check(), container_ops_apply(), container_ops_bind_vault(), container_ops_move(), item_type_of(), list_of() (+8 more)

### Community 41 - "fx_shapes.c"
Cohesion: 0.38
Nodes (4): fx_shape_bar(), fx_shape_cooldown_bar(), fx_shape_plus(), scale_channel()

### Community 42 - "resolve_click"
Cohesion: 0.08
Nodes (22): quest_target_untrack(), linked(), request(), status_from_string(), universe_map_data_in_place(), universe_map_data_open(), universe_map_data_retry(), universe_map_data_travel() (+14 more)

### Community 43 - "quest_target.c"
Cohesion: 0.08
Nodes (36): ImapDataState, instance_map_data_generation(), instance_map_data_state(), instance_map_data_update(), map_route_entry(), map_route_first(), map_route_hops(), map_route_revision() (+28 more)

### Community 44 - "hud_side_stack.c"
Cohesion: 0.07
Nodes (54): instance_map_data_close(), instance_map_data_open(), universe_map_data_close(), fx_dust_reset(), fx_footsteps_reset(), fx_tap_reset(), input_gestures_set_blocked(), chat_panel_is_visible() (+46 more)

### Community 45 - "draw_entity_layers"
Cohesion: 0.19
Nodes (7): get_frames_with_fallback(), draw_dev_ui_box(), draw_entity_layers(), get_frame_count_and_direction(), layers_render_ready(), pick_available_direction(), atlas_get_direction_frames()

### Community 46 - "hash_table.c"
Cohesion: 0.33
Nodes (9): find_insert_slot(), find_occupied(), hash_string(), hash_table_contains(), hash_table_put(), hash_table_remove(), hash_table_remove_if(), raw_insert() (+1 more)

### Community 47 - "dialogue_data_get"
Cohesion: 0.33
Nodes (4): dialogue_data_available(), dialogue_data_get(), on_dialogue_fetched(), parse_response()

### Community 48 - "game_frame"
Cohesion: 0.09
Nodes (20): presentation_runtime_is_ready(), world_interpolation_ms(), game_frame(), pending(), interpolation_compute_view(), prediction_on_snapshot(), prediction_prev(), prediction_self() (+12 more)

### Community 50 - "ui_consume_event"
Cohesion: 0.17
Nodes (13): presentation_runtime_toggle_dev_ui(), fx_tap_alloc_entry(), fx_tap_default_params(), fx_tap_draw_cross(), fx_tap_ease_out_quart(), fx_tap_init(), fx_tap_scale_px(), fx_tap_snapf() (+5 more)

### Community 51 - "create_object_layer"
Cohesion: 0.50
Nodes (3): create_object_layer(), free_object_layer(), free_layer_value()

### Community 52 - "get_or_fetch_atlas_data"
Cohesion: 0.32
Nodes (9): dialogue_data_request(), dialogue_data_request_code(), hash_table_get(), atlas_layout_url(), atlas_texture(), get_atlas_texture(), get_or_fetch_atlas_data(), obj_layers_mgr_schedule_atlas_fetch() (+1 more)

### Community 53 - "emit_step"
Cohesion: 0.12
Nodes (12): crossed_milestone(), scan_entity(), step_phase(), cell_size(), emit_step(), fx_footsteps_update(), step_is_visible(), track_entity() (+4 more)

### Community 54 - "container_panel.c"
Cohesion: 0.13
Nodes (23): container_panel_animate_from(), container_panel_cell_rect(), container_panel_is_open(), container_panel_open_storage(), container_panel_open_vault(), container_panel_update(), container_panel_wheel(), drop_out() (+15 more)

### Community 55 - "game_render_init"
Cohesion: 0.20
Nodes (9): dialogue_data_init(), hash_table_init(), interaction_bubble_init(), inventory_modal_init(), modal_dialogue_init(), modal_notification_init(), create_entity_render(), game_render_init() (+1 more)

### Community 56 - "social_panel.c"
Cohesion: 0.17
Nodes (19): chat_panel_draw(), control_side(), hud_panel_close_rect(), hud_panel_content_rect(), hud_panel_draw_close(), hud_panel_draw_frame(), hud_panel_tabs(), player_panel_hide_input() (+11 more)

### Community 57 - "modal_draw_clipped_text"
Cohesion: 0.18
Nodes (11): connection_is_open(), network_send(), modal_map_draw(), shadow_text(), modal_draw_card(), modal_draw_clipped_text(), modal_draw_float_shadow(), modal_draw_title() (+3 more)

### Community 58 - "chat_panel.c"
Cohesion: 0.14
Nodes (23): chat_direct(), chat_log_unread_direct(), back_rect(), chat_panel_covers_point(), chat_panel_handle_click(), chat_panel_handle_wheel(), chat_panel_open_direct(), chat_panel_toggle() (+15 more)

### Community 59 - "market_panel.c"
Cohesion: 0.10
Nodes (42): local_player_coins(), act(), action_button(), bid(), bought(), button_h(), cancelled(), card_frame() (+34 more)

### Community 60 - "quest_progress_store.c"
Cohesion: 0.08
Nodes (26): action_cache_fetch(), action_cache_get(), ingest_craft_items(), ingest_doc(), ingest_item_ids(), on_action_fetched(), find_or_create(), head_at() (+18 more)

### Community 61 - "camera_init"
Cohesion: 0.25
Nodes (6): presentation_runtime_camera_zoom(), camera_init(), camera_resize(), camera_sync_mobile_zoom(), game_render_set_screen_size(), on_window_resize()

### Community 62 - "merchant_panel.c"
Cohesion: 0.15
Nodes (20): local_player_trading(), local_player_unsaved(), bag_stack_of(), button_h(), coin_amount(), confirmed(), draw_card(), font_body() (+12 more)

### Community 63 - "session_gate.c"
Cohesion: 0.17
Nodes (20): config_instance_code(), config_open_launch(), client_on_init_received(), game_client_enter_world(), save_status_joined(), action_count(), enter_world(), finish_leave() (+12 more)

### Community 64 - "replication.c"
Cohesion: 0.13
Nodes (7): pinch_zoom_on_tick(), hud_joystick_draw(), pixel_ring(), pixel_sector(), row_cells(), camera_set_zoom(), camera_zoom_by()

### Community 65 - "trade_panel.c"
Cohesion: 0.22
Nodes (16): button(), button_h(), decline_request(), draw_offer(), draw_trade(), font_body(), gap(), max_cell() (+8 more)

### Community 66 - "land"
Cohesion: 0.42
Nodes (7): container_ops_check(), container_ops_stack(), confirmed(), land(), place_rect(), slide_in(), stack_move_drop()

### Community 67 - "asset_bridge.c"
Cohesion: 0.29
Nodes (3): asset_bridge_now(), error(), function()

### Community 68 - "scan_entity"
Cohesion: 0.29
Nodes (7): presentation_runtime_status_border(), copy_active_layers(), find_slot(), interaction_bubble_get_alive_layers(), scan_entity(), status_border_color(), upsert_slot()

### Community 69 - "fx_assemble.c"
Cohesion: 0.28
Nodes (13): card_center(), current_margin(), fx_assemble_draw(), fx_assemble_init(), fx_assemble_show(), fx_assemble_update(), lcg_f01(), lcg_range() (+5 more)

### Community 70 - "chat_log.c"
Cohesion: 0.31
Nodes (12): append(), chat_log_at(), chat_log_count(), chat_log_get(), chat_log_mark_read(), chat_log_open_direct(), chat_log_push(), chat_log_unread() (+4 more)

### Community 71 - "fetch_data_release"
Cohesion: 0.21
Nodes (10): audio_request_complete(), audio_unload(), find_asset(), oldest_asset(), release_asset(), reserve_asset(), fetch_data_release(), on_fetched() (+2 more)

### Community 72 - "open_item_viewer"
Cohesion: 0.67
Nodes (3): config_data_server_url(), open_item_viewer(), url_encode()

### Community 73 - "ui_tabs.c"
Cohesion: 0.38
Nodes (9): every_tab_fits(), grid(), ui_tabs_draw(), ui_tabs_font(), ui_tabs_height(), ui_tabs_hit(), ui_tabs_rect(), ui_tabs_row() (+1 more)

### Community 74 - "fetch_request_start"
Cohesion: 0.20
Nodes (11): audio_init(), presentation_runtime_font_factor_size(), presentation_runtime_start_fetch(), fetch_init(), fetch_request_start(), fetch_request_start_at(), fetch_request_start_limited(), main() (+3 more)

### Community 75 - "fx_dust_spawn"
Cohesion: 0.20
Nodes (8): alloc_puff(), fx_dust_default_params(), fx_dust_init(), fx_dust_spawn(), lcg_f01(), lcg_range(), step_params(), pool_take()

### Community 76 - "fct_spawn"
Cohesion: 0.15
Nodes (9): local_player_fct_at(), local_player_fct_clear(), local_player_fct_count(), local_player_fct_push(), fct_init(), fct_spawn(), fct_update(), lcg_f01() (+1 more)

### Community 77 - "ol_as_ico_draw"
Cohesion: 0.20
Nodes (5): draw_dialogue_sprite(), modal_interact_get_cached_layers(), fallback_circle(), ol_as_ico_draw(), ol_stack_ico_draw()

### Community 78 - "get_animation_state"
Cohesion: 0.24
Nodes (8): dialogue_data_cleanup(), hash_table_destroy(), anim_pool_alloc(), destroy_entity_render(), entity_render_gc(), get_animation_state(), game_render_cleanup(), destroy_object_layers_manager()

### Community 79 - "session_gate_draw"
Cohesion: 0.29
Nodes (5): audio_is_muted(), fullscreen_bridge_is_active(), fullscreen_bridge_toggle(), session_gate_draw(), session_gate_settings()

### Community 82 - "game_client_on_tick"
Cohesion: 0.22
Nodes (10): config_ws_url(), connection_open(), game_client_on_session_end(), game_client_on_tick(), on_socket_close(), start_resume(), message_set_init_handler(), session_gate_account() (+2 more)

### Community 83 - "config.c"
Cohesion: 0.33
Nodes (7): adopt_js_string(), config_enter_world(), config_init(), config_take_launch_ticket(), on_socket_open(), instance_route_sanitize_code(), instance_route_ws_url()

### Community 84 - "item_slot_draw_ex"
Cohesion: 0.33
Nodes (6): badge_font(), item_slot_draw(), item_slot_draw_empty(), item_slot_draw_ex(), item_slot_draw_locked(), item_slot_draw_quantity()

### Community 85 - "game_render_world_to_screen"
Cohesion: 0.28
Nodes (6): own_centre(), world_drop_cell(), world_drop_draw_target(), game_render_screen_to_world(), game_render_world_to_screen(), local_player_view_position()

### Community 86 - "viewport.h"
Cohesion: 0.29
Nodes (3): save_status_observe(), save_status_reset(), shows_condition()

### Community 87 - "render_init"
Cohesion: 0.25
Nodes (8): fx_footsteps_init(), fx_grant_delivery_init(), fx_loot_reset(), dev_ui_init(), hud_minimap_overlay_init(), modal_instance_map_init(), modal_map_init(), render_init()

### Community 88 - "draw_loot_screen_fx"
Cohesion: 0.29
Nodes (5): fx_loot_delivery_token_at(), fx_loot_delivery_token_slot_count(), fx_loot_screen_particle_at(), fx_loot_screen_particle_slot_count(), draw_loot_screen_fx()

### Community 89 - "game_render_fx_loot"
Cohesion: 0.29
Nodes (5): fx_loot_particle_at(), fx_loot_particle_slot_count(), fx_loot_render_at(), fx_loot_slot_count(), game_render_fx_loot()

### Community 90 - "pointer_gesture_update"
Cohesion: 0.53
Nodes (4): pointer_gesture_update(), release(), sector_axis(), start()

### Community 91 - "fx_dust.c"
Cohesion: 0.47
Nodes (3): alpha_curve(), fx_dust_draw(), size_curve()

### Community 92 - "overhead_occlusion_opacity"
Cohesion: 0.50
Nodes (3): presentation_runtime_overhead_occlusion_hidden_opacity(), find_roof(), overhead_occlusion_opacity()

### Community 93 - "local_player_request_trade_offer"
Cohesion: 0.40
Nodes (4): local_player_request_trade_offer(), offer_coins(), offer_slot(), offer_units()

### Community 94 - "identity_icon_draw"
Cohesion: 0.50
Nodes (3): find_player(), identity_icon_draw(), identity_preview_draw()

### Community 95 - "dev_ui_on_tick"
Cohesion: 0.50
Nodes (3): connection_get_stats(), dev_ui_on_tick(), dev_ui_update_network_stats()

### Community 97 - "free_atlas_sprite_sheet_data"
Cohesion: 0.50
Nodes (3): create_atlas_sprite_sheet_data(), free_atlas_sprite_sheet_data(), free_atlas_value()

## Knowledge Gaps
- **8 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `copy_str()` connect `copy_str` to `message.c`, `modal_notification.c`, `inventory_bar.c`, `instance_map_data.c`, `string`, `chat_pane.c`, `modal_instance_map.c`, `viewport_is_mobile`, `fade_c`, `inventory_modal.c`, `audio.c`, `open_session`, `audio_content.c`, `modal_interact.c`, `player_panel.c`, `action_metadata`, `game_render_ui`, `Rectangle`, `map_detail.c`, `render_on_tick`, `text_wrap`, `container_ops.c`, `resolve_click`, `quest_target.c`, `game_frame`, `container_panel.c`, `market_panel.c`, `quest_progress_store.c`, `merchant_panel.c`, `session_gate.c`, `trade_panel.c`, `chat_log.c`, `fetch_data_release`, `fetch_request_start`, `config.c`, `identity_icon_draw`?**
  _High betweenness centrality (0.046) - this node is a cross-community bridge._
- **Are the 105 inferred relationships involving `copy_str()` (e.g. with `audio_play()` and `audio_update()`) actually correct?**
  _`copy_str()` has 105 INFERRED edges - model-reasoned connections that need verification._
- **Should `message.c` be split into smaller, more focused modules?**
  _Cohesion score 0.05592705167173252 - nodes in this community are weakly interconnected._
- **Why does `ingest_craft_items()` connect `quest_progress_store.c` to `copy_str`, `instance_map_data.c`?**
  _High betweenness centrality (0.014) - this node is a cross-community bridge._
- **Are the 83 inferred relationships involving `viewport_is_mobile()` (e.g. with `chat_font()` and `chat_row_h()`) actually correct?**
  _`viewport_is_mobile()` has 83 INFERRED edges - model-reasoned connections that need verification._
- **Should `modal_notification.c` be split into smaller, more focused modules?**
  _Cohesion score 0.08293224731580896 - nodes in this community are weakly interconnected._
- **Why does `game_state_render()` connect `game_render_entities` to `hud_minimap_overlay.c`, `render_on_tick`, `string`, `fx_loot.c`, `game_frame`, `inventory_modal.c`, `emit_step`?**
  _High betweenness centrality (0.012) - this node is a cross-community bridge._