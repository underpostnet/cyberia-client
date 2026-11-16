# Graph Report - cyberia-client  (2026-11-16)

## Corpus Check
- 245 files · ~200,626 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 2734 nodes · 10175 edges · 103 communities (93 shown, 10 thin omitted)
- Extraction: 73% EXTRACTED · 27% INFERRED · 0% AMBIGUOUS · INFERRED: 2722 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `a34dfd89`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- message.c
- hud_minimap_overlay.c
- item_slot_grid.c
- modal_notification.c
- inventory_bar_draw
- instance_map_data.c
- ui_button_pixel_retro_draw
- string
- dev_ui.c
- chat_pane.c
- ui_toggle.c
- modal_instance_map.c
- draw_labels
- fx_loot.c
- text_line_height
- ui_button.c
- engine_client.c
- local_player.c
- inventory_modal.c
- fx_reward.c
- scan_entity
- game_render_world
- audio.c
- open_session
- map_marker.c
- audio_content.c
- preload.c
- modal_interact.c
- player_panel.c
- preload_frame
- inventory_bar.c
- game_render_ui
- input.c
- fx_dust.c
- Rectangle
- map_detail.c
- prediction_reconcile
- world_cell_size
- object_layers_management.c
- copy_str
- game_state_transform
- fx_shapes.c
- universe_map_data.c
- modal_instance_map_update
- hud_side_stack.c
- draw_entity_layers
- hash_table.c
- combat_cluster_on_tick
- game_state_slot
- fx_assemble.c
- container_ops.c
- get_or_fetch_atlas_data
- emit_step
- modal_dialogue_draw
- game_render_init
- social_panel.c
- interaction_bubble_update
- chat_panel_update
- market_panel.c
- quest_progress_store.c
- meta_cache.c
- merchant_panel.c
- session_gate.c
- fx_level_up.c
- trade_panel.c
- config.c
- asset_bridge.c
- ui_scroll_update
- ui_button_hit
- chat_log.c
- container_panel_update
- open_item_viewer
- ui_tabs.c
- drop_out
- inventory_bar_handle_click
- fct_update
- ol_as_ico_draw
- get_animation_state
- fx_tap_draw_cross
- dev_ui_draw
- cyberia_stats_values
- land
- stdbool
- draw_pixel_bevel
- world_cell_size
- draw_coin_slot
- game_state_appearance
- draw_edge_ends
- combat_cluster.c
- modal_draw_card
- fx_combat_reticle
- pointer_gesture_hold
- ui_consume_event
- render_init
- local_player_open_slots
- local_player_request_item_drop
- draw_quest_step_objectives
- on_dialogue_fetched
- lerp_entity
- own_centre
- local_player_party_member
- held_reason_text

## God Nodes (most connected - your core abstractions)
1. `copy_str()` - 111 edges
2. `viewport_is_mobile()` - 85 edges
3. `ui_button_hit()` - 66 edges
4. `instance_map_data_graph()` - 54 edges
5. `inventory_modal_draw()` - 51 edges
6. `serial_get_string()` - 49 edges
7. `modal_interact_draw()` - 40 edges
8. `client_event_push()` - 39 edges
9. `game_render_entities()` - 39 edges
10. `inventory_bar_draw()` - 37 edges

## Surprising Connections (you probably didn't know these)
- `json_unpack_snapshot()` --calls--> `fetch_deprioritize()`  [INFERRED]
  src/net/message.c → src/data/engine_client.c
- `fetch_now()` --calls--> `asset_bridge_now()`  [INFERRED]
  src/data/engine_client.c → src/js/asset_bridge.c
- `build_map()` --calls--> `instance_map_data_node_active_quests()`  [INFERRED]
  src/ui/map_detail.c → src/data/instance_map_data.c
- `build_map()` --calls--> `instance_map_data_node_active_actions()`  [INFERRED]
  src/ui/map_detail.c → src/data/instance_map_data.c
- `json_unpack_init_data()` --calls--> `quest_progress_store_reset()`  [INFERRED]
  src/net/message.c → src/data/quest_progress_store.c

## Import Cycles
- None detected.

## Communities (103 total, 10 thin omitted)

### Community 0 - "message.c"
Cohesion: 0.05
Nodes (118): settings(), fx_hit_burst_spawn(), client_event_flush(), pack_event(), on_socket_receive(), chat_conversation_of(), claim_slot(), economy_reason_text() (+110 more)

### Community 1 - "hud_minimap_overlay.c"
Cohesion: 0.15
Nodes (29): ImapGraph, ImapNode, instance_map_cell_to_card(), instance_map_edge_destination(), instance_map_exit_direction(), instance_map_local_rect(), game_state_own_render(), centered_square() (+21 more)

### Community 2 - "item_slot_grid.c"
Cohesion: 0.15
Nodes (24): container_panel_cell_at(), container_panel_draw(), container_panel_draw_drag(), emit(), end_gesture(), grid_point_in_visible_bounds(), grid_rows(), item_slot_grid_animate_from_point() (+16 more)

### Community 3 - "modal_notification.c"
Cohesion: 0.06
Nodes (83): fx_grant_delivery_begin(), fx_grant_delivery_update(), fx_grant_delivery_waiting(), grant_landed(), release_spent(), alloc_entry(), changed_cell(), draw_popup() (+75 more)

### Community 4 - "inventory_bar_draw"
Cohesion: 0.18
Nodes (25): local_player_bag_stack(), local_player_unlock_level(), bar_slot_gap(), bar_slot_size(), coin_slot_rect(), coin_slot_x(), draw_offered(), find_coin_slot() (+17 more)

### Community 5 - "instance_map_data.c"
Cohesion: 0.08
Nodes (41): compose_maps(), fetch_static(), find_presence_poi(), ImapBounds, ImapEntrySpawn, ImapNodeLinks, ImapPlaced, ImapPopulation (+33 more)

### Community 6 - "ui_button_pixel_retro_draw"
Cohesion: 0.08
Nodes (30): presentation_runtime_status_icon(), emoji_icon_of(), emoji_px(), emoji_text_width(), emoji_text_wrap(), layout(), draw_capability_bar(), draw_capability_item() (+22 more)

### Community 7 - "string"
Cohesion: 0.12
Nodes (5): fct_event_on_self(), fct_init(), fct_spawn(), lcg_f01(), json_float()

### Community 8 - "dev_ui.c"
Cohesion: 0.14
Nodes (9): connection_get_stats(), dev_ui_on_tick(), dev_ui_update_network_stats(), camera_resize(), camera_set_zoom(), camera_sync_mobile_zoom(), camera_zoom_by(), game_render_set_screen_size() (+1 more)

### Community 9 - "chat_pane.c"
Cohesion: 0.20
Nodes (18): chat_font(), chat_layout(), chat_pane_bind(), chat_pane_draw(), chat_pane_hide(), chat_pane_press(), chat_pane_update(), chat_pane_wheel() (+10 more)

### Community 10 - "ui_toggle.c"
Cohesion: 0.18
Nodes (18): ease_approach(), ease_exp_factor(), modal_anchor_ease_height(), ui_switch_update(), arm_gesture(), chevron_icon_id(), follow_offset(), resolve_chevron() (+10 more)

### Community 11 - "modal_instance_map.c"
Cohesion: 0.08
Nodes (70): ImapDataState, instance_map_data_state(), universe_map_data_graph(), universe_map_data_stale(), universe_map_data_state(), map_detail_next_page(), canvas_color(), card_reach() (+62 more)

### Community 12 - "draw_labels"
Cohesion: 0.14
Nodes (23): instance_map_data_find_node(), instance_map_presence_color(), map_marker_size(), add_hit(), cell_px(), draw_entry_spawn(), draw_labels(), draw_node_overlay() (+15 more)

### Community 13 - "fx_loot.c"
Cohesion: 0.09
Nodes (35): audio_context_update(), craft_snapshot(), restore_music(), audio_event(), drop_alloc(), drop_find(), expend_target(), fx_loot_delivery_token_at() (+27 more)

### Community 14 - "text_line_height"
Cohesion: 0.25
Nodes (15): card_title_wrap(), draw_card_line(), draw_quest_grid_action_button(), draw_quest_grid_button(), draw_shop_card(), mi_shop_price_font(), mi_shop_price_sz(), quest_grid_action_font() (+7 more)

### Community 15 - "ui_button.c"
Cohesion: 0.13
Nodes (22): skill_preview_pager_draw(), skill_preview_summon_id(), skill_preview_summons(), draw_outlined_wrap(), has_str(), or_default(), resolve_font(), resolve_gap() (+14 more)

### Community 16 - "engine_client.c"
Cohesion: 0.13
Nodes (24): complete(), deliver(), discard(), enqueue(), fetch_account(), fetch_deprioritize(), fetch_event(), fetch_forget() (+16 more)

### Community 17 - "local_player.c"
Cohesion: 0.08
Nodes (47): client_event_push(), cooldown_charge(), local_player_action_charge(), local_player_confirm_quote(), local_player_consume_charge(), local_player_incoming_requests(), local_player_invite_from(), local_player_invite_to() (+39 more)

### Community 18 - "inventory_modal.c"
Cohesion: 0.06
Nodes (77): dialogue_data_available(), dialogue_data_get(), game_state_is_active_item_type(), game_state_is_dead_item(), local_player_status_icon(), local_player_trading(), activate_enabled(), anchored_card_rect() (+69 more)

### Community 19 - "fx_reward.c"
Cohesion: 0.18
Nodes (20): card_center(), current_margin(), draw_particle(), draw_trail(), ellipse_point(), fx_reward_draw(), fx_reward_init(), fx_reward_show() (+12 more)

### Community 20 - "scan_entity"
Cohesion: 0.16
Nodes (11): game_state_is_player(), game_state_player(), local_player_friend_of(), copy_active_layers(), find_slot(), interaction_bubble_get_alive_layers(), scan_entity(), upsert_slot() (+3 more)

### Community 21 - "game_render_world"
Cohesion: 0.12
Nodes (15): on_hints_fetched(), parse_color_object(), parse_response(), presentation_runtime_camera_zoom(), presentation_runtime_entity_fallback_color(), presentation_runtime_overhead_occlusion_fade_ms(), presentation_runtime_overhead_occlusion_hidden_opacity(), presentation_runtime_status_border() (+7 more)

### Community 22 - "audio.c"
Cohesion: 0.15
Nodes (28): apply_volume(), audio_get_volume(), audio_params(), audio_play(), audio_request_complete(), audio_set_muted(), audio_set_volume(), audio_shutdown() (+20 more)

### Community 23 - "open_session"
Cohesion: 0.13
Nodes (24): container_panel_close(), inventory_modal_init(), market_panel_reset(), merchant_panel_reset(), collapse_quest_detail(), es_clear(), es_pop(), es_push() (+16 more)

### Community 24 - "map_marker.c"
Cohesion: 0.21
Nodes (25): draw_bracket(), draw_count(), draw_plate(), faded(), fill_triangle(), label_arrow(), label_value_w(), map_marker_draw_arrow() (+17 more)

### Community 25 - "audio_content.c"
Cohesion: 0.13
Nodes (23): audio_preload(), audio_request_absent(), audio_request_code(), audio_request_priority(), apply_current(), audio_content_fetch(), audio_content_music_finished(), audio_content_restore_music() (+15 more)

### Community 27 - "modal_interact.c"
Cohesion: 0.10
Nodes (40): modal_dialogue_set_quest_style(), modal_dialogue_show_fullscreen(), active_tab_content_height(), anchor_content_card_height(), anchor_safe_area(), anchored_card_rect(), bar_buttons(), bar_rect() (+32 more)

### Community 28 - "player_panel.c"
Cohesion: 0.16
Nodes (38): local_player_leads_party(), local_player_xp_ratio(), armed(), bar(), bind_friend(), button(), button_h(), draw_friend_list() (+30 more)

### Community 29 - "preload_frame"
Cohesion: 0.13
Nodes (18): audio_init(), audio_start(), presentation_runtime_font_factor_size(), presentation_runtime_start_fetch(), fetch_init(), frame(), main(), draw_centered() (+10 more)

### Community 30 - "inventory_bar.c"
Cohesion: 0.21
Nodes (20): bar_slots_settled(), bar_toggle_anchor(), bar_toggle_pad(), bar_toggle_size(), bar_top(), draw_lift(), ensure_bar_toggle(), inventory_bar_cell_at() (+12 more)

### Community 31 - "game_render_ui"
Cohesion: 0.09
Nodes (30): fx_death_update(), fx_heal_aura_update(), fx_hit_burst_update(), fx_level_up_update(), fx_obstacle_burst_update(), fx_portal_aura_update(), fx_tap_update(), combat_cluster_covers_point() (+22 more)

### Community 32 - "input.c"
Cohesion: 0.12
Nodes (30): axis_dir(), emit_steer_tap(), emit_tap(), end(), find_handle(), find_held(), first_held(), input_joystick() (+22 more)

### Community 33 - "fx_dust.c"
Cohesion: 0.17
Nodes (12): alloc_puff(), alpha_curve(), fx_dust_default_params(), fx_dust_draw(), fx_dust_init(), fx_dust_spawn(), fx_dust_update(), lcg_f01() (+4 more)

### Community 34 - "Rectangle"
Cohesion: 0.13
Nodes (29): draw_count(), draw_list(), craft_card_height(), craft_fee_h(), craft_slot_size(), draw_card_base(), draw_craft_card(), draw_craft_stack() (+21 more)

### Community 35 - "map_detail.c"
Cohesion: 0.07
Nodes (61): ImapPresencePoi, ImapProvider, ImapQuestState, instance_map_data_graph(), instance_map_node_label(), instance_map_quest_state(), map_route_first(), quest_active_step_index() (+53 more)

### Community 36 - "prediction_reconcile"
Cohesion: 0.09
Nodes (22): walk_speed_ref(), game_frame(), client_event_list(), client_event_resend(), client_event_trim(), local_player_in_stasis(), local_player_move_speed(), local_player_set_stasis() (+14 more)

### Community 37 - "world_cell_size"
Cohesion: 0.15
Nodes (23): presentation_runtime_dev_ui(), presentation_runtime_palette(), world_cell_size(), fx_death_draw(), fx_heal_aura_draw(), fx_hit_burst_draw(), lane(), fx_level_up_draw() (+15 more)

### Community 38 - "object_layers_management.c"
Cohesion: 0.16
Nodes (15): create_object_layer(), free_object_layer(), ledger_standard_from_string(), free_layer_value(), json_get_bool_safe(), json_get_int_safe(), json_get_string_safe(), parse_direction_frame_data() (+7 more)

### Community 39 - "copy_str"
Cohesion: 0.13
Nodes (13): ingest_craft_items(), ingest_doc(), ingest_item_ids(), game_client_hold_resume(), local_player_interaction(), local_player_interaction_flags(), find_player(), identity_icon_draw() (+5 more)

### Community 40 - "game_state_transform"
Cohesion: 0.11
Nodes (22): fx_death_active(), fx_death_observe(), fx_heal_aura_observe(), fx_level_up_active(), fx_level_up_observe(), alive(), auto_target_at(), auto_target_center() (+14 more)

### Community 41 - "fx_shapes.c"
Cohesion: 0.09
Nodes (21): draw_entry(), exit_scale(), lane(), draw_entry(), ease_out_cubic(), exit_scale(), lane(), draw_entity() (+13 more)

### Community 42 - "universe_map_data.c"
Cohesion: 0.16
Nodes (12): find_node(), linked(), on_fetched(), parse_doc(), request(), status_from_string(), universe_map_data_in_place(), universe_map_data_open() (+4 more)

### Community 43 - "modal_instance_map_update"
Cohesion: 0.11
Nodes (24): instance_map_data_generation(), instance_map_data_update(), map_route_entry(), map_route_hops(), map_route_revision(), map_route_tree(), map_route_update(), quest_progress_store_revision() (+16 more)

### Community 44 - "hud_side_stack.c"
Cohesion: 0.06
Nodes (59): instance_map_data_close(), instance_map_data_open(), universe_map_data_close(), fx_dust_reset(), fx_footsteps_reset(), fx_tap_reset(), input_gestures_set_blocked(), chat_panel_is_visible() (+51 more)

### Community 45 - "draw_entity_layers"
Cohesion: 0.10
Nodes (13): identity_appearance_ref(), interaction_bubble_dead_equip(), get_frames_with_fallback(), draw_dev_ui_box(), draw_entity_layers(), get_frame_count_and_direction(), layers_render_ready(), pick_available_direction() (+5 more)

### Community 46 - "hash_table.c"
Cohesion: 0.26
Nodes (13): dialogue_data_request(), dialogue_data_request_code(), find_insert_slot(), find_occupied(), hash_string(), hash_table_contains(), hash_table_get(), hash_table_put() (+5 more)

### Community 47 - "combat_cluster_on_tick"
Cohesion: 0.15
Nodes (18): local_player_set_action_ready_at(), combat_cluster_on_ack(), combat_cluster_on_tick(), cooldown_left(), directional(), find_control(), flash(), follow_press() (+10 more)

### Community 48 - "game_state_slot"
Cohesion: 0.11
Nodes (20): audio_context_snapshot(), crossed_milestone(), entity_snapshot(), portal_snapshot(), scan_entity(), step_phase(), fx_footsteps_update(), fx_obstacle_burst_snapshot() (+12 more)

### Community 50 - "fx_assemble.c"
Cohesion: 0.28
Nodes (13): card_center(), current_margin(), fx_assemble_draw(), fx_assemble_init(), fx_assemble_show(), fx_assemble_update(), lcg_f01(), lcg_range() (+5 more)

### Community 51 - "container_ops.c"
Cohesion: 0.26
Nodes (15): apply(), bankable_while_worn(), check(), container_ops_apply(), container_ops_bind_vault(), container_ops_move(), item_type_of(), list_of() (+7 more)

### Community 52 - "get_or_fetch_atlas_data"
Cohesion: 0.21
Nodes (11): create_atlas_sprite_sheet_data(), free_atlas_sprite_sheet_data(), atlas_layout_url(), atlas_texture(), free_atlas_value(), get_atlas_texture(), get_or_fetch_atlas_data(), is_object_layer_cid() (+3 more)

### Community 53 - "emit_step"
Cohesion: 0.31
Nodes (5): cell_size(), emit_step(), step_is_visible(), track_entity(), tracker_for()

### Community 54 - "modal_dialogue_draw"
Cohesion: 0.15
Nodes (18): inventory_bar_visible_height(), dlg_fullscreen(), dlg_hidden(), dlg_hidden_on_mobile(), dlg_pad(), dlg_side_pad(), dlg_sprite_size(), draw_quest_talk_btn() (+10 more)

### Community 55 - "game_render_init"
Cohesion: 0.22
Nodes (9): dialogue_data_init(), hash_table_init(), interaction_bubble_init(), modal_dialogue_init(), modal_notification_init(), quest_journal_init(), create_entity_render(), game_render_init() (+1 more)

### Community 56 - "social_panel.c"
Cohesion: 0.25
Nodes (10): chat_panel_open_direct(), panel_rect(), show_news(), social_panel_close(), social_panel_covers_point(), social_panel_handle_click(), social_panel_handle_wheel(), social_panel_toggle() (+2 more)

### Community 57 - "interaction_bubble_update"
Cohesion: 0.19
Nodes (19): column_content_height(), column_ensure_toggle(), column_hidden(), column_input_bounds(), column_scroll_view(), column_scrollbar_bounds(), column_slide_width(), column_width() (+11 more)

### Community 58 - "chat_panel_update"
Cohesion: 0.22
Nodes (17): back_rect(), chat_panel_covers_point(), chat_panel_draw(), chat_panel_handle_click(), chat_panel_handle_wheel(), draw_conversation_header(), in_party(), pane_rect() (+9 more)

### Community 59 - "market_panel.c"
Cohesion: 0.10
Nodes (40): local_player_coins(), local_player_unsaved(), action_button(), bid(), bought(), button_h(), cancelled(), card_frame() (+32 more)

### Community 60 - "quest_progress_store.c"
Cohesion: 0.19
Nodes (14): find_by_code(), quest_objective_type(), quest_objective_type_name(), quest_progress_store_apply(), quest_progress_store_count(), quest_progress_store_get(), quest_progress_store_parse_status(), quest_progress_store_reset() (+6 more)

### Community 61 - "meta_cache.c"
Cohesion: 0.16
Nodes (10): action_cache_fetch(), action_cache_get(), on_action_fetched(), find_or_create(), head_at(), meta_cache_fetch(), meta_cache_find(), meta_cache_on_fetched() (+2 more)

### Community 62 - "merchant_panel.c"
Cohesion: 0.22
Nodes (12): bag_stack_of(), button_h(), draw_card(), font_body(), merchant_panel_count(), merchant_panel_draw(), merchant_panel_press(), merchant_panel_wheel() (+4 more)

### Community 63 - "session_gate.c"
Cohesion: 0.05
Nodes (50): audio_is_muted(), config_instance_code(), config_open_launch(), config_ws_url(), fullscreen_bridge_is_active(), fullscreen_bridge_toggle(), client_on_init_received(), connection_close() (+42 more)

### Community 64 - "fx_level_up.c"
Cohesion: 0.11
Nodes (17): fx_death_reset(), fx_heal_aura_reset(), draw_entry(), ease_out_cubic(), exit_scale(), fx_level_up_reset(), lane(), fx_obstacle_burst_reset() (+9 more)

### Community 65 - "trade_panel.c"
Cohesion: 0.23
Nodes (15): button(), button_h(), decline_request(), draw_offer(), draw_trade(), font_body(), gap(), max_cell() (+7 more)

### Community 66 - "config.c"
Cohesion: 0.33
Nodes (7): adopt_js_string(), config_enter_world(), config_init(), config_take_launch_ticket(), on_socket_open(), instance_route_sanitize_code(), instance_route_ws_url()

### Community 67 - "asset_bridge.c"
Cohesion: 0.29
Nodes (3): asset_bridge_now(), error(), function()

### Community 68 - "ui_scroll_update"
Cohesion: 0.36
Nodes (13): container_panel_press(), scroll_capture_bounds(), scroll_input_contains(), scroll_max_offset(), scroll_move(), scroll_show_bar(), ui_scroll_cancel_press(), ui_scroll_nudge() (+5 more)

### Community 69 - "ui_button_hit"
Cohesion: 0.08
Nodes (36): container_panel_is_open(), merchant_panel_update(), modal_dialogue_is_collapsed(), action_metadata(), action_pending(), active_dlg_key(), craft_tab_visible(), default_tab() (+28 more)

### Community 70 - "chat_log.c"
Cohesion: 0.16
Nodes (18): append(), chat_direct(), chat_log_at(), chat_log_count(), chat_log_get(), chat_log_mark_read(), chat_log_open_direct(), chat_log_push() (+10 more)

### Community 71 - "container_panel_update"
Cohesion: 0.17
Nodes (10): local_player_vault(), container_panel_open_storage(), container_panel_open_vault(), container_panel_update(), held(), open_cells(), seed(), sync() (+2 more)

### Community 72 - "open_item_viewer"
Cohesion: 0.67
Nodes (3): config_data_server_url(), open_item_viewer(), url_encode()

### Community 73 - "ui_tabs.c"
Cohesion: 0.36
Nodes (8): every_tab_fits(), grid(), ui_tabs_draw(), ui_tabs_font(), ui_tabs_hit(), ui_tabs_rect(), ui_tabs_row(), ui_tabs_wrap()

### Community 74 - "drop_out"
Cohesion: 0.22
Nodes (11): container_panel_animate_from(), container_panel_cell_rect(), container_panel_wheel(), drop_out(), handle(), hint_font(), hint_line(), release_rect() (+3 more)

### Community 75 - "inventory_bar_handle_click"
Cohesion: 0.32
Nodes (12): bar_height(), bar_margin(), bar_track_h(), clamp_scroll(), drag_thumb(), inventory_bar_full_height(), inventory_bar_handle_click(), max_scroll_px() (+4 more)

### Community 76 - "fct_update"
Cohesion: 0.33
Nodes (5): local_player_fct_at(), local_player_fct_clear(), local_player_fct_count(), local_player_fct_push(), fct_update()

### Community 77 - "ol_as_ico_draw"
Cohesion: 0.11
Nodes (12): badge_font(), item_slot_draw(), item_slot_draw_empty(), item_slot_draw_ex(), item_slot_draw_locked(), item_slot_draw_quantity(), coin_amount(), draw_dialogue_sprite() (+4 more)

### Community 78 - "get_animation_state"
Cohesion: 0.24
Nodes (8): dialogue_data_cleanup(), hash_table_destroy(), anim_pool_alloc(), destroy_entity_render(), entity_render_gc(), get_animation_state(), game_render_cleanup(), destroy_object_layers_manager()

### Community 79 - "fx_tap_draw_cross"
Cohesion: 0.24
Nodes (8): fx_tap_alloc_entry(), fx_tap_default_params(), fx_tap_draw_cross(), fx_tap_ease_out_quart(), fx_tap_init(), fx_tap_scale_px(), fx_tap_snapf(), fx_tap_spawn()

### Community 80 - "dev_ui_draw"
Cohesion: 0.20
Nodes (7): game_state_progress(), own_level(), dev_ui_draw(), dev_ui_get_active_item_count(), dev_ui_get_effective_stats_sum(), direction_to_string(), mode_to_string()

### Community 82 - "land"
Cohesion: 0.42
Nodes (7): container_ops_check(), container_ops_stack(), confirmed(), land(), place_rect(), slide_in(), stack_move_drop()

### Community 84 - "draw_pixel_bevel"
Cohesion: 0.20
Nodes (6): draw_skill_previews(), draw_track(), skill_preview_layout(), draw_pixel_bevel(), ui_rect_contain(), ui_rect_scale()

### Community 85 - "world_cell_size"
Cohesion: 0.17
Nodes (15): fx_tap_draw(), cell_to_screen(), combat_cluster_claim(), combat_cluster_draw(), combat_cluster_layout(), draw_aim(), draw_guide(), draw_target_reticle() (+7 more)

### Community 86 - "draw_coin_slot"
Cohesion: 0.22
Nodes (7): fx_inventory_bar_qty_init(), game_state_get_entity_default(), bar_qty_font(), bar_slot_padding(), coin_item_key(), draw_coin_slot(), inventory_bar_init()

### Community 87 - "game_state_appearance"
Cohesion: 0.29
Nodes (7): presentation_runtime_font_family(), presentation_runtime_is_ready(), pending(), game_state_appearance(), text_font_settled(), immediate_scene_ready(), player_render_ready()

### Community 88 - "draw_edge_ends"
Cohesion: 0.57
Nodes (7): ImapEdge, draw_edge_ends(), draw_edge_line(), edge_color(), edge_endpoint(), edge_random(), edge_shown()

### Community 90 - "modal_draw_card"
Cohesion: 0.43
Nodes (5): modal_draw_card(), modal_draw_float_shadow(), modal_draw_title(), modal_pop_alpha(), toolbar_toggle_right()

### Community 91 - "fx_combat_reticle"
Cohesion: 0.44
Nodes (5): bracket(), fx_combat_guide(), fx_combat_reticle(), fx_player_anchor(), scaled()

### Community 92 - "pointer_gesture_hold"
Cohesion: 0.40
Nodes (4): hold_world(), pointer_gesture_hold(), pointer_gesture_start(), sector_axis()

### Community 93 - "ui_consume_event"
Cohesion: 0.29
Nodes (6): presentation_runtime_toggle_dev_ui(), combat_cluster_on_world_tap(), interaction_bubble_on_world_tap(), tap_hits_provider(), quest_journal_handle_wheel(), ui_consume_event()

### Community 94 - "render_init"
Cohesion: 0.25
Nodes (8): fx_footsteps_init(), fx_grant_delivery_init(), fx_loot_reset(), dev_ui_init(), hud_minimap_overlay_init(), modal_instance_map_init(), modal_map_init(), render_init()

### Community 95 - "local_player_open_slots"
Cohesion: 0.27
Nodes (9): cyberia_capacity_slots(), cyberia_capacity_unlock_level(), local_player_inventory_has_room(), local_player_inventory_used(), local_player_is_coin(), local_player_item_quantity(), local_player_open_slots(), craft_outputs_fit() (+1 more)

### Community 99 - "lerp_entity"
Cohesion: 0.40
Nodes (4): world_interpolation_ms(), compute_alpha_for(), interpolation_compute_view(), lerp_entity()

### Community 100 - "own_centre"
Cohesion: 0.50
Nodes (3): own_centre(), world_drop_cell(), local_player_view_position()

## Knowledge Gaps
- **10 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `copy_str()` connect `copy_str` to `message.c`, `modal_notification.c`, `inventory_bar_draw`, `instance_map_data.c`, `string`, `chat_pane.c`, `modal_instance_map.c`, `draw_labels`, `fx_loot.c`, `text_line_height`, `local_player.c`, `inventory_modal.c`, `audio.c`, `open_session`, `audio_content.c`, `modal_interact.c`, `player_panel.c`, `preload_frame`, `game_render_ui`, `Rectangle`, `map_detail.c`, `fx_shapes.c`, `universe_map_data.c`, `modal_instance_map_update`, `combat_cluster_on_tick`, `game_state_slot`, `container_ops.c`, `market_panel.c`, `quest_progress_store.c`, `meta_cache.c`, `merchant_panel.c`, `session_gate.c`, `trade_panel.c`, `config.c`, `ui_button_hit`, `chat_log.c`, `container_panel_update`, `drop_out`, `draw_pixel_bevel`?**
  _High betweenness centrality (0.067) - this node is a cross-community bridge._
- **Are the 110 inferred relationships involving `copy_str()` (e.g. with `audio_play()` and `audio_update()`) actually correct?**
  _`copy_str()` has 110 INFERRED edges - model-reasoned connections that need verification._
- **Should `message.c` be split into smaller, more focused modules?**
  _Cohesion score 0.054368167044223384 - nodes in this community are weakly interconnected._
- **Why does `ui_button_hit()` connect `ui_button_hit` to `hud_minimap_overlay.c`, `item_slot_grid.c`, `modal_notification.c`, `inventory_bar_draw`, `chat_pane.c`, `modal_instance_map.c`, `text_line_height`, `ui_button.c`, `local_player.c`, `inventory_modal.c`, `open_session`, `modal_interact.c`, `player_panel.c`, `inventory_bar.c`, `game_render_ui`, `Rectangle`, `map_detail.c`, `hud_side_stack.c`, `modal_dialogue_draw`, `social_panel.c`, `interaction_bubble_update`, `chat_panel_update`, `market_panel.c`, `quest_progress_store.c`, `merchant_panel.c`, `session_gate.c`, `trade_panel.c`, `ui_tabs.c`, `inventory_bar_handle_click`?**
  _High betweenness centrality (0.023) - this node is a cross-community bridge._
- **Are the 84 inferred relationships involving `viewport_is_mobile()` (e.g. with `chat_font()` and `chat_row_h()`) actually correct?**
  _`viewport_is_mobile()` has 84 INFERRED edges - model-reasoned connections that need verification._
- **Should `modal_notification.c` be split into smaller, more focused modules?**
  _Cohesion score 0.05785819482432148 - nodes in this community are weakly interconnected._
- **Why does `ingest_craft_items()` connect `copy_str` to `instance_map_data.c`, `string`?**
  _High betweenness centrality (0.014) - this node is a cross-community bridge._