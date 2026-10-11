#pragma once

#include "gc_types.h"

namespace usm::gc::engine {

struct MString {
    s32 field_0;
    s32 size;
    const char* guts;
    s32 field_c;
};

static_assert(sizeof(MString) == 16);

void* game();
void* world();
void* developer_options();
void* settings();
void* terrain();
bool game_ready();
bool gameplay_rel_ready();
bool interactive_world_ready();
s32 game_state();
bool game_paused();

void pause_game();
void unpause_game();
// Only the debug menu's owned-unpause path may dismiss the native pause UI.
// Returns true if the active native menu received Deactivate (which unpauses).
bool dismiss_debug_pause_menu();
void set_camera(s32 camera);
s32 get_camera();
bool camera_available(s32 camera);
bool set_controller2_camera(bool enabled);
bool controller2_camera_enabled();
void reboot_game();
void set_physics(bool enabled);
void request_single_step();
void set_god_mode(s32 mode);
s32 get_god_mode();
bool monkey_running();
void set_monkey_running(bool enabled);
void set_rumble(bool enabled);

const char* developer_flag_name(u32 index);
const char* developer_int_name(u32 index);
bool get_developer_flag(u32 index);
void set_developer_flag(u32 index, bool value);
s32 get_developer_int(u32 index);
void set_developer_int(u32 index, s32 value);

const char* debug_render_name(u32 index);
s32 debug_render_get(u32 index);
void debug_render_set(u32 index, s32 value);
s32 debug_render_min(u32 index);
s32 debug_render_max(u32 index);

u32 ngl_debug_get(u32 native_index);
void ngl_debug_set(u32 native_index, u32 value);

void save_game(s32 slot);
void load_game(s32 slot);
void auto_load_game();
void hires_screenshot();
void lores_screenshot();

u32 terrain_region_count();
void* terrain_region(u32 index);
const char* region_name(void* region);
bool region_is_warp_target(void* region);
s32 region_variant(void* region);
s32 region_variant_count(void* region);
bool warp_to_point_of_interest();
void warp_to_region(u32 index);
void set_region_variant(u32 index, s32 variant);
void unload_all_districts();
bool set_district_streaming_active(bool active);

void draw_panel(float left, float top, float right, float bottom, u32 color,
                float z = 0.5f);
void draw_text(const char* text, float x, float y, u32 color, float scale = 0.75f);
bool measure_text(const char* text, float scale, u32& width, u32& height);
void update_game_button(void* button, float time_inc);
void finish_scene();

bool readable_c_string(const char* text);

}  // namespace usm::gc::engine

extern "C" void* memcpy(void* destination, const void* source, unsigned int size);
extern "C" void* memset(void* destination, int value, unsigned int size);
extern "C" int memcmp(const void* lhs, const void* rhs, unsigned int size);
extern "C" unsigned int strlen(const char* value);
