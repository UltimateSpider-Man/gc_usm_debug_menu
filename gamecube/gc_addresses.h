#pragma once

#include "gc_types.h"

namespace usm::gc::address {

// This table is tied to the supplied NTSC-U main.dol.  Symbols in
// gc_address.txt often point into functions; the entry points below were
// checked against the actual instructions in that DOL.
constexpr uptr payload_base = 0x80636040u;
constexpr uptr hook_site = 0x8016E55Cu;
constexpr u32 hook_original_instruction = 0x4812636Du;
constexpr uptr update_hook_site = 0x8017BEE0u;
constexpr u32 update_hook_original_instruction = 0x4BFC7909u;
constexpr uptr pad_read_hook_site = 0x801ACCC8u;
constexpr u32 pad_read_hook_original_instruction = 0x48165239u;
constexpr uptr arena_call_1 = 0x802F39B8u;
constexpr uptr arena_call_2 = 0x802F39F4u;
constexpr u32 arena_call_1_original = 0x480012A1u;
constexpr u32 arena_call_2_original = 0x48001265u;

constexpr uptr game_pointer = 0x8045F694u;
constexpr uptr world_pointer = 0x8045F180u;
constexpr uptr developer_options_pointer = 0x804627F8u;
// r13 is initialized to 0x80466A80 at 0x80003110. These slots are
// respectively r13-0x4270 and r13-0x43B0 in the native camera/input code.
constexpr uptr input_manager_pointer = 0x80462810u;
constexpr uptr mouselook_controller_pointer = 0x804626D0u;
constexpr uptr ngl_system_font_pointer = 0x80460C30u;
constexpr uptr ngl_debug_flags = 0x803D8D90u;
constexpr uptr ngl_debug_flags_copy = 0x803D8DA3u;
constexpr uptr developer_flag_names = 0x80370518u;
constexpr uptr developer_int_names = 0x80370A08u;
constexpr uptr debug_render_names = 0x803B7CFCu;
constexpr uptr spider_monkey_running = 0x8045EF74u;
constexpr uptr god_mode_cheat = 0x8045EFACu;
constexpr uptr ultra_god_mode_cheat = 0x8045EFB0u;
constexpr uptr mega_god_mode_cheat = 0x8045EFB4u;

constexpr uptr pad_read = 0x80311F00u;
constexpr uptr pad_control_motor = 0x80312200u;
// game_settings::export_game_options calls these with input_manager+4.
// They respectively clear/set the native rumble-disabled bit at manager+0x28.
constexpr uptr rumble_enable = 0x801D736Cu;
constexpr uptr rumble_disable = 0x801D7380u;
constexpr uptr os_set_arena_lo = 0x802F4C58u;
// OSResetSystem's prologue and three arguments are at 0x802F84B4 in
// main.original.dol; gc_address.txt's 0x802F8A34 is inside another function.
constexpr uptr os_reset_system = 0x802F84B4u;

constexpr uptr ngl_list_end_scene = 0x802948C8u;
constexpr uptr ngl_add_string = 0x8029AB00u;
constexpr uptr ngl_get_string_dimensions = 0x8029AD4Cu;
constexpr uptr ngl_add_quad = 0x8029BD84u;
constexpr uptr ngl_init_quad = 0x8029BEE8u;
constexpr uptr ngl_set_quad_color = 0x8029BFB0u;
constexpr uptr ngl_set_quad_rect = 0x8029BFC4u;
constexpr uptr ngl_set_quad_z = 0x8029BFE8u;
constexpr uptr ngl_begin_hires_screenshot = 0x802948F0u;
constexpr uptr ngl_screenshot = 0x8029C774u;

constexpr uptr game_get_state = 0x80142620u;
constexpr uptr game_pause = 0x8017E67Cu;
constexpr uptr game_unpause = 0x8017E8C0u;
constexpr uptr game_set_camera = 0x8017E408u;
constexpr uptr scene_analyzer_enabled = 0x801803D8u;
constexpr uptr enable_scene_analyzer = 0x801803E0u;
constexpr uptr actor_suspend = 0x8010949Cu;
constexpr uptr actor_unsuspend = 0x801094C4u;
constexpr uptr game_frame_advance_level = 0x8017DC58u;
constexpr uptr game_load_new_level = 0x8017EA14u;
constexpr uptr game_load_new_level_at_position = 0x8017EA6Cu;
constexpr uptr game_enable_marky_camera = 0x8017EAC0u;
constexpr uptr game_enable_physics = 0x8017F0C8u;
constexpr uptr game_button_update = 0x801437E8u;
constexpr uptr set_god_mode = 0x800E1B70u;

constexpr uptr spider_monkey_start = 0x800D84D8u;
constexpr uptr spider_monkey_frame_advance = 0x800D8570u;
constexpr uptr spider_monkey_on_level_load = 0x800D8734u;
constexpr uptr spider_monkey_on_level_unload = 0x800D8A50u;
constexpr uptr spider_monkey_is_running = 0x800D8AD0u;

constexpr uptr world_malor_point = 0x80152B40u;
constexpr uptr terrain_unload_all_districts = 0x80178A20u;
constexpr uptr terrain_set_district_variant = 0x80179438u;
constexpr uptr terrain_unlock_district = 0x8017AE28u;
constexpr uptr region_get_variant = 0x80185F60u;
constexpr uptr resource_manager_pointer = 0x8045F214u;
constexpr uptr resource_manager_get_partition = 0x8015D968u;
constexpr uptr resource_pack_streamer_set_active = 0x8015DFB0u;
constexpr uptr fe_igo_pointer = 0x803B848Cu;
constexpr uptr pause_menu_system_pointer = 0x803B8490u;
constexpr uptr comicpanel_system_pointer = 0x804607E4u;

// Main-game REL addresses. Calls are gated on a live game/settings object.
constexpr uptr game_settings_save = 0x7F004AF0u;
constexpr uptr game_settings_reset_container = 0x7F004DACu;
constexpr uptr game_settings_load_game = 0x7F0050C8u;
constexpr uptr game_settings_load_most_recent = 0x7F005394u;
// Actual REL entry in main.original.dol. gc_address.txt's 7F025CF4 is shifted.
constexpr uptr pause_menu_deactivate = 0x7F025700u;
constexpr uptr entity_tracker_get_arrow_target = 0x7F023E40u;
constexpr uptr debug_render_init_items = 0x7F010C70u;
constexpr uptr debug_render_get_int = 0x7F010CA0u;
constexpr uptr debug_render_set_int = 0x7F010CB4u;
constexpr uptr debug_render_get_bool = 0x7F010D24u;
constexpr uptr debug_render_get_min = 0x7F010D4Cu;
constexpr uptr debug_render_get_max = 0x7F010D60u;

constexpr u32 developer_flag_count = 151u;
constexpr u32 developer_int_count = 78u;
constexpr u32 ngl_debug_flag_count = 19u;

}  // namespace usm::gc::address
