#include "menu_registry.h"

#include "debug_menu_core.h"
#include "gc_engine.h"
#include "menus/diagnostics_gc.h"
#include "menus/devopt_features_gc.h"
#include "menus/character_spawn_gc.h"
#include "menus/hero_select_gc.h"
#include "menus/menu_catalogs.h"
#include "menus/mission_level_gc.h"
#include "menus/ngl_features_gc.h"
#include "menus/replay_gc.h"
#include "menus/script_progression_gc.h"

namespace usm::gc::registry {
namespace {

using menu::Command;
using menu::Entry;
using menu::EntryType;
using menu::Menu;

enum class SavedValueType : u8 {
    integer,
    boolean,
    floating,
};

bool is_initialized;
u8 physics_enabled = 1u;
u8 slow_motion_enabled;
u8 rumble_enabled = 1u;
u8 monkey_enabled;
u8 districts_unloaded;
s32 god_mode;
s32 save_slot;
s32 old_frame_lock;

uptr warp_terrain_cache = 0xFFFFFFFFu;
u32 warp_count_cache = 0xFFFFFFFFu;
u32 warp_signature_cache = 0xFFFFFFFFu;
uptr variant_terrain_cache = 0xFFFFFFFFu;
u32 variant_count_cache = 0xFFFFFFFFu;
u32 variant_signature_cache = 0xFFFFFFFFu;

char region_fallback_labels[255][24];

Menu* warp_menu;
Menu* variant_menu;

struct RuntimeMenus {
    Menu* missions;
    Menu* level_select;
    Menu* hero_select;
    Menu* ai;
    Menu* memory;
    Menu* script_memtrack;
    Menu* slabs;
    Menu* full_slabs;
    Menu* partial_slabs;
    Menu* allocated_objects;
    Menu* free_objects;
    Menu* entity_animations;
    Menu* entity_variants;
};

RuntimeMenus runtime_menus;
bool runtime_initialized;

// The retail GC compiler stores bool fields as full words. Native
// game_data_meat::init (0x7F0040F8) writes all 97 scalar fields at
// consecutive four-byte offsets 0x000..0x180. The packed PC layout diverges
// after OPT_SCORE_DISPLAY and must never be used to address this object.
constexpr u32 game_value_stride = sizeof(u32);

constexpr SavedValueType game_value_types[] = {
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::integer,
    SavedValueType::floating, SavedValueType::floating, SavedValueType::integer,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::integer,  SavedValueType::floating, SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::floating,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::boolean,  SavedValueType::boolean,  SavedValueType::boolean,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::floating,
    SavedValueType::floating, SavedValueType::floating, SavedValueType::floating,
    SavedValueType::floating, SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::floating, SavedValueType::floating, SavedValueType::floating,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,  SavedValueType::integer,  SavedValueType::integer,
    SavedValueType::integer,
};

static_assert(sizeof(game_value_types) / sizeof(game_value_types[0]) == 97u);

constexpr s8 ngl_native_indices[22] = {
    4, -1, 5, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

bool is_change(Command command) {
    return command == Command::activate || command == Command::decrement ||
           command == Command::increment;
}

bool is_adjustment(Command command) {
    return command == Command::decrement || command == Command::increment;
}

s32 direction(Command command) {
    return command == Command::decrement ? -1 : 1;
}

s32 bounded_adjustment(s32 current, s32 delta, s32 minimum, s32 maximum) {
    if (current < minimum) {
        current = minimum;
    }
    if (current > maximum) {
        current = maximum;
    }
    // Subtract from the small menu bounds before adding to potentially
    // externally supplied counters; never overflow an extreme native s32.
    if (delta > 0 && current > maximum - delta) {
        return maximum;
    }
    if (delta < 0 && current < minimum - delta) {
        return minimum;
    }
    return current + delta;
}

void make_numbered_label(char* output, u32 capacity, const char* prefix, u32 index) {
    menu::copy_text(output, capacity, prefix);
    char number[16];
    menu::format_s32(number, sizeof(number), static_cast<s32>(index));
    menu::append_text(output, capacity, number);
}

void camera_formatter(const Entry&, char* output, u32 capacity) {
    const s32 mode = engine::get_camera();
    menu::copy_text(output, capacity, mode == 1 ? "user" : mode == 2 ? "sceneanalyzer" : "chase");
}

void integer_target_formatter(const Entry& entry, char* output, u32 capacity) {
    if (entry.target == nullptr) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    menu::format_s32(output, capacity, *static_cast<volatile s32*>(entry.target));
}

void camera_handler(Entry& entry, Command command) {
    (void)entry;
    if (!is_change(command)) return;
    const s32 current = engine::get_camera();
    const s32 next = (current + (command == Command::decrement ? 2 : 1)) % 3;
    if (!engine::camera_available(next)) {
        menu::set_status("Camera objects unavailable in this level");
        return;
    }
    engine::set_camera(next);
}

void populate_camera(Menu& destination) {
    // The shared renderer adds ": "; retain the requested space before it.
    Entry* entry = menu::add(destination, "Camera ", EntryType::custom);
    if (entry != nullptr) {
        entry->minimum = 0;
        entry->maximum = 2;
        entry->handler = camera_handler;
        entry->formatter = camera_formatter;
    }
}

void controller2_formatter(const Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, engine::developer_options() == nullptr
                                          ? "N/A"
                                          : engine::controller2_camera_enabled() ? "True" : "False");
}

void controller2_handler(Entry&, Command command) {
    if (!is_change(command)) {
        return;
    }
    const bool enabled = !engine::controller2_camera_enabled();
    if (!engine::set_controller2_camera(enabled)) {
        menu::set_status(enabled ? "Controller 2 is disconnected or camera unavailable"
                                 : "User camera unavailable outside gameplay");
    }
}

void physics_handler(Entry& entry, Command command) {
    if (command != Command::frame && entry.target != nullptr) {
        auto* value = static_cast<volatile u8*>(entry.target);
        if (!engine::gameplay_rel_ready()) {
            *value = *value == 0u ? 1u : 0u;
            menu::set_status("Physics unavailable: game is not ready");
            return;
        }
        engine::set_physics(*value != 0u);
    }
}

void single_step_handler(Entry&, Command command) {
    if (command == Command::activate) {
        if (!engine::gameplay_rel_ready()) {
            menu::set_status("Single step unavailable outside gameplay");
            return;
        }
        engine::request_single_step();
        menu::close();
    }
}

void slow_motion_handler(Entry& entry, Command command) {
    if (command == Command::frame || entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile u8*>(entry.target);
    if (engine::developer_options() == nullptr) {
        *value = *value == 0u ? 1u : 0u;
        menu::set_status("Slow motion unavailable: Devopts not ready");
        return;
    }
    if (*value != 0u) {
        old_frame_lock = engine::get_developer_int(13u);
        engine::set_developer_int(13u, 120);
    } else {
        engine::set_developer_int(13u, old_frame_lock);
    }
    menu::close();
}

void monkey_handler(Entry& entry, Command command) {
    if (entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile u8*>(entry.target);
    if (command == Command::frame) {
        *value = engine::monkey_running() ? 1u : 0u;
        return;
    }
    if (!is_change(command)) {
        return;
    }
    if (!engine::gameplay_rel_ready()) {
        menu::set_status("Spider Monkey unavailable outside gameplay");
        return;
    }
    *value = engine::monkey_running() ? 0u : 1u;
    engine::set_monkey_running(*value != 0u);
    menu::close();
}

void monkey_formatter(const Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, engine::monkey_running() ? "True" : "False");
}

void rumble_handler(Entry& entry, Command command) {
    if (entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile u8*>(entry.target);
    void* settings = engine::settings();
    if (command == Command::frame) {
        if (settings != nullptr) {
            *value = *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(settings) +
                                                      0x358u + 13u * game_value_stride) != 0u
                         ? 1u
                         : 0u;
        }
        return;
    }
    if (!is_change(command)) {
        return;
    }
    if (settings == nullptr || !engine::gameplay_rel_ready()) {
        menu::set_status("Rumble setting unavailable outside gameplay");
        return;
    }
    const bool enabled =
        *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(settings) +
                                        0x358u + 13u * game_value_stride) == 0u;
    *value = enabled ? 1u : 0u;
    *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(settings) +
                                    0x358u + 13u * game_value_stride) = enabled ? 1u : 0u;
    engine::set_rumble(enabled);
}

void rumble_formatter(const Entry&, char* output, u32 capacity) {
    void* settings = engine::settings();
    if (settings == nullptr) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    const bool enabled =
        *reinterpret_cast<volatile u32*>(reinterpret_cast<u8*>(settings) +
                                        0x358u + 13u * game_value_stride) != 0u;
    menu::copy_text(output, capacity, enabled ? "True" : "False");
}

void god_mode_handler(Entry& entry, Command command) {
    if (entry.target == nullptr) {
        return;
    }
    auto* value = static_cast<volatile s32*>(entry.target);
    if (command == Command::frame) {
        *value = engine::get_god_mode();
        return;
    }
    if (!is_adjustment(command)) {
        return;
    }
    if (!engine::gameplay_rel_ready()) {
        menu::set_status("God Mode unavailable outside gameplay");
        return;
    }
    s32 next = *value + direction(command) * menu::change_multiplier();
    if (next < 0) {
        next = 0;
    }
    if (next > 5) {
        next = 5;
    }
    *value = next;
    engine::set_god_mode(next);
    menu::close();
}

void developer_flag_formatter(const Entry& entry, char* output, u32 capacity) {
    menu::copy_text(output, capacity, engine::developer_options() == nullptr
                                          ? "N/A"
                                          : engine::get_developer_flag(entry.id) ? "True" : "False");
}

void developer_flag_handler(Entry& entry, Command command) {
    if (is_change(command) && engine::developer_options() != nullptr) {
        const bool next = !engine::get_developer_flag(entry.id);
        if (entry.id == 91u) {
            if (!engine::set_controller2_camera(next)) {
                menu::set_status("Controller 2 is disconnected or camera unavailable");
            }
            return;
        }
        engine::set_developer_flag(entry.id, next);
    }
}

void save_handler(Entry&, Command command) {
    if (command == Command::activate) {
        if (engine::gameplay_rel_ready()) {
            engine::save_game(save_slot);
            menu::set_status("Save request submitted");
            menu::close();
        } else {
            menu::set_status("Save unavailable outside gameplay");
        }
    }
}

void load_handler(Entry&, Command command) {
    if (command == Command::activate) {
        if (engine::gameplay_rel_ready()) {
            if (mission_level::busy() || hero_select::busy()) {
                menu::set_status("Another world transition is in progress");
                return;
            }
            if (!character_spawn::prepare_transition()) {
                menu::set_status("Character cleanup queued; select Load again when complete");
                menu::close();
                return;
            }
            engine::load_game(save_slot);
            menu::close();
        } else {
            menu::set_status("Load unavailable outside gameplay");
        }
    }
}

void auto_load_handler(Entry&, Command command) {
    if (command == Command::activate) {
        if (engine::gameplay_rel_ready()) {
            if (mission_level::busy() || hero_select::busy()) {
                menu::set_status("Another world transition is in progress");
                return;
            }
            if (!character_spawn::prepare_transition()) {
                menu::set_status("Character cleanup queued; select Auto Load again when complete");
                menu::close();
                return;
            }
            engine::auto_load_game();
            menu::close();
        } else {
            menu::set_status("Auto-load unavailable outside gameplay");
        }
    }
}

void hires_handler(Entry&, Command command) {
    if (command == Command::activate) {
        engine::hires_screenshot();
        menu::close();
    }
}

void lores_handler(Entry&, Command command) {
    if (command == Command::activate) {
        engine::lores_screenshot();
        menu::set_status("Screenshot requested: usm_debug.tga");
    }
}

void unload_districts_handler(Entry&, Command command) {
    if (command != Command::activate) {
        return;
    }
    if (districts_unloaded == 0u) {
        if (engine::terrain() == nullptr) {
            menu::set_status("Districts unavailable: terrain is not ready");
            return;
        }
        engine::unload_all_districts();
        districts_unloaded = 1u;
        if (!engine::set_district_streaming_active(false)) {
            menu::set_status("Districts unloaded; streamer unavailable");
            return;
        }
        menu::set_status("Districts unloaded; streaming disabled");
        menu::close();
    } else {
        if (!engine::set_district_streaming_active(true)) {
            menu::set_status("District streamer unavailable");
            return;
        }
        districts_unloaded = 0u;
        menu::set_status("District streaming enabled");
        menu::close();
    }
}

u8* game_value_address(u32 id) {
    void* settings = engine::settings();
    if (settings == nullptr || !engine::gameplay_rel_ready() || id >= 97u) {
        return nullptr;
    }
    return reinterpret_cast<u8*>(settings) + 0x358u + id * game_value_stride;
}

void game_value_formatter(const Entry& entry, char* output, u32 capacity) {
    u8* value = game_value_address(entry.id);
    if (value == nullptr) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    switch (game_value_types[entry.id]) {
    case SavedValueType::boolean:
        menu::copy_text(output, capacity, *reinterpret_cast<volatile u32*>(value) != 0u ? "True" : "False");
        break;
    case SavedValueType::integer:
        menu::format_s32(output, capacity, *reinterpret_cast<volatile s32*>(value));
        break;
    case SavedValueType::floating:
        menu::format_float(output, capacity, *reinterpret_cast<volatile float*>(value));
        break;
    }
}

void game_value_handler(Entry& entry, Command command) {
    if (!is_change(command)) {
        return;
    }
    u8* value = game_value_address(entry.id);
    if (value == nullptr) {
        menu::set_status("Saved settings unavailable outside gameplay");
        return;
    }
    switch (game_value_types[entry.id]) {
    case SavedValueType::boolean: {
        auto* flag = reinterpret_cast<volatile u32*>(value);
        *flag = *flag == 0u ? 1u : 0u;
        if (entry.id == 13u) {
            engine::set_rumble(*flag != 0u);
        }
        break;
    }
    case SavedValueType::integer: {
        if (!is_adjustment(command)) {
            return;
        }
        auto* integer = reinterpret_cast<volatile s32*>(value);
        *integer = bounded_adjustment(*integer, direction(command) * menu::change_multiplier(),
                                       0, 1000);
        break;
    }
    case SavedValueType::floating: {
        if (!is_adjustment(command)) {
            return;
        }
        auto* floating = reinterpret_cast<volatile float*>(value);
        float current = *floating;
        // Recover nonfinite or out-of-range saved values before adjustment.
        // The ordered comparison also rejects NaN without a library call.
        if (!(current >= 0.0f)) {
            current = 0.0f;
        }
        if (current > 1000.0f) {
            current = 1000.0f;
        }
        float next = current + static_cast<float>(direction(command) * menu::change_multiplier()) *
                                   0.1f;
        if (next < 0.0f) {
            next = 0.0f;
        }
        if (next > 1000.0f) {
            next = 1000.0f;
        }
        *floating = next;
        break;
    }
    }
}

void debug_render_formatter(const Entry& entry, char* output, u32 capacity) {
    if (!engine::gameplay_rel_ready()) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    menu::format_s32(output, capacity, engine::debug_render_get(entry.id));
}

void debug_render_handler(Entry& entry, Command command) {
    if (!is_adjustment(command)) {
        return;
    }
    if (!engine::gameplay_rel_ready()) {
        menu::set_status("Debug Render REL is not loaded");
        return;
    }
    const s32 minimum = engine::debug_render_min(entry.id);
    const s32 maximum = engine::debug_render_max(entry.id);
    const s32 value = bounded_adjustment(engine::debug_render_get(entry.id),
                                         direction(command) * menu::change_multiplier(),
                                         minimum, maximum);
    engine::debug_render_set(entry.id, value);
}

void ngl_formatter(const Entry& entry, char* output, u32 capacity) {
    if (ngl_features::handles(entry.id)) {
        menu::format_s32(output, capacity, ngl_features::get(entry.id));
        return;
    }
    const s8 native = ngl_native_indices[entry.id];
    if (native < 0) {
        menu::copy_text(output, capacity, "N/A");
    } else {
        menu::format_s32(output, capacity,
                         static_cast<s32>(engine::ngl_debug_get(static_cast<u32>(native))));
    }
}

void ngl_handler(Entry& entry, Command command) {
    if (!is_change(command)) {
        return;
    }
    if (ngl_features::handles(entry.id)) {
        const s32 maximum = entry.id == 1u ? 2 : 1;
        s32 value = ngl_features::get(entry.id) + direction(command);
        if (command == Command::activate) {
            value = (ngl_features::get(entry.id) + 1) % (maximum + 1);
        }
        ngl_features::set(entry.id, value < 0 ? 0 : value > maximum ? maximum : value);
        return;
    }
    const s8 native = ngl_native_indices[entry.id];
    if (native < 0) {
        menu::set_status("This PC NGL flag has no GameCube field");
        return;
    }
    const u32 index = static_cast<u32>(native);
    const s32 maximum = entry.id == 1u ? 2 : 1;
    s32 value = static_cast<s32>(engine::ngl_debug_get(index)) +
                direction(command) * menu::change_multiplier();
    if (command == Command::activate) {
        value = engine::ngl_debug_get(index) == 0u ? 1 : 0;
    }
    if (value < 0) {
        value = 0;
    }
    if (value > maximum) {
        value = maximum;
    }
    engine::ngl_debug_set(index, static_cast<u32>(value));
}

void warp_handler(Entry& entry, Command command) {
    if (command == Command::activate) {
        if (!engine::interactive_world_ready()) {
            menu::set_status("Warp unavailable outside a running level");
            return;
        }
        engine::warp_to_region(entry.id);
        menu::close();
    }
}

void warp_poi_handler(Entry&, Command command) {
    if (command != Command::activate) {
        return;
    }
    if (engine::warp_to_point_of_interest()) {
        menu::close();
    } else {
        menu::set_status("No active point of interest");
    }
}

u32 region_signature(u32 count, bool include_variant_counts) {
    const u32 end = count < 255u ? count : 255u;
    u32 signature = 0x9E3779B9u ^ count;
    for (u32 index = 0u; index < end; ++index) {
        void* region = engine::terrain_region(index);
        signature = (signature << 5u) | (signature >> 27u);
        signature ^= reinterpret_cast<uptr>(region) + index * 0x45D9F3Bu;
        signature ^= reinterpret_cast<uptr>(engine::region_name(region)) * 0x119DE1F3u;
        signature ^= engine::region_is_warp_target(region) ? 0x7F4A7C15u : 0u;
        if (include_variant_counts) {
            signature ^= static_cast<u32>(engine::region_variant_count(region)) * 0x27D4EB2Du;
        }
    }
    return signature;
}

void refresh_warp(Menu& destination) {
    const uptr terrain = reinterpret_cast<uptr>(engine::terrain());
    const u32 count = engine::terrain_region_count();
    const u32 signature = region_signature(count, false);
    if (terrain == warp_terrain_cache && count == warp_count_cache &&
        signature == warp_signature_cache) {
        return;
    }
    warp_terrain_cache = terrain;
    warp_count_cache = count;
    warp_signature_cache = signature;
    menu::clear_entries(destination);

    Entry* poi = menu::add(destination, "Point of Interest", EntryType::action);
    if (poi != nullptr) {
        poi->handler = warp_poi_handler;
    }
    const u32 end = count < 255u ? count : 255u;
    for (u32 index = 0u; index < end; ++index) {
        void* region = engine::terrain_region(index);
        if (!engine::region_is_warp_target(region)) {
            continue;
        }
        const char* label = engine::region_name(region);
        if (label == nullptr) {
            make_numbered_label(region_fallback_labels[index], sizeof(region_fallback_labels[index]),
                                "Region ", index);
            label = region_fallback_labels[index];
        }
        Entry* entry = menu::add(destination, label, EntryType::action);
        if (entry != nullptr) {
            entry->id = index;
            entry->handler = warp_handler;
        }
    }
}

void variant_formatter(const Entry& entry, char* output, u32 capacity) {
    void* region = engine::terrain_region(entry.id);
    if (region == nullptr) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    menu::format_s32(output, capacity, engine::region_variant(region));
    menu::append_text(output, capacity, " [0,");
    char maximum[16];
    const s32 count = engine::region_variant_count(region);
    menu::format_s32(maximum, sizeof(maximum), count > 0 ? count - 1 : 0);
    menu::append_text(output, capacity, maximum);
    menu::append_text(output, capacity, "]");
}

void variant_handler(Entry& entry, Command command) {
    if (!is_adjustment(command)) {
        return;
    }
    void* region = engine::terrain_region(entry.id);
    if (region == nullptr) {
        return;
    }
    const s32 count = engine::region_variant_count(region);
    if (count <= 0) {
        menu::set_status("District has no variants");
        return;
    }
    s32 value = engine::region_variant(region) +
                direction(command) * menu::change_multiplier();
    if (value < 0) {
        value = count - 1;
    }
    if (value >= count) {
        value = 0;
    }
    engine::set_region_variant(entry.id, value);
}

void refresh_variants(Menu& destination) {
    const uptr terrain = reinterpret_cast<uptr>(engine::terrain());
    const u32 count = engine::terrain_region_count();
    const u32 signature = region_signature(count, true);
    if (terrain == variant_terrain_cache && count == variant_count_cache &&
        signature == variant_signature_cache) {
        return;
    }
    variant_terrain_cache = terrain;
    variant_count_cache = count;
    variant_signature_cache = signature;
    menu::clear_entries(destination);
    const u32 end = count < 255u ? count : 255u;
    for (u32 index = 0u; index < end; ++index) {
        void* region = engine::terrain_region(index);
        if (engine::region_variant_count(region) <= 1) {
            continue;
        }
        const char* label = engine::region_name(region);
        if (label == nullptr) {
            make_numbered_label(region_fallback_labels[index], sizeof(region_fallback_labels[index]),
                                "Region ", index);
            label = region_fallback_labels[index];
        }
        Entry* entry = menu::add(destination, label, EntryType::custom);
        if (entry != nullptr) {
            entry->id = index;
            entry->handler = variant_handler;
            entry->formatter = variant_formatter;
        }
    }
    if (destination.count == 0u) {
        menu::add(destination, "No terrain regions loaded", EntryType::read_only);
    }
}

void add_developer_alias(Menu& destination, const char* label, u32 option) {
    Entry* entry = menu::add(destination, label, EntryType::custom);
    if (entry != nullptr) {
        entry->id = option;
        entry->handler = developer_flag_handler;
        entry->formatter = developer_flag_formatter;
    }
}

Menu* require_menu(const char* title, u16 capacity, menu::Refresh refresh = nullptr) {
    Menu* value = menu::create(title, capacity, refresh);
    return value;
}

}  // namespace

bool initialized() {
    return is_initialized;
}

void render_overlays() {
    ngl_features::render();
    devopt_features::render();
}

void initialize_for_render() {
    if (is_initialized) {
        return;
    }
    runtime_initialized = false;
    runtime_menus = RuntimeMenus{};
    menu::reset();

    Menu* root = require_menu("Debug Menu", 14u);
    warp_menu = require_menu("Warp", 256u, refresh_warp);
    Menu* game = require_menu("Game", 20u);
    Menu* save_load = require_menu("Save/Load", 4u);
    Menu* screenshot = require_menu("Screenshot", 2u);
    Menu* devopts = require_menu("Devopts", 229u);
    Menu* gamefile = require_menu("Saved Game Settings", 97u);
    Menu* missions =
        require_menu("Missions", mission_level::recommended_mission_capacity);
    variant_menu = require_menu("District variants", 256u, refresh_variants);
    Menu* replay_menu = require_menu("Replay", replay::recommended_capacity);
    Menu* script = require_menu("Script", 96u);
    Menu* progression = require_menu("Progression", 512u);
    Menu* level_select =
        require_menu("Level Select", mission_level::recommended_level_capacity);
    Menu* hero_select = require_menu("Hero Select", 10u);
    Menu* debug_render = require_menu("Debug Render", 52u);
    Menu* ngl = require_menu("NGL Debug", 22u);
    Menu* ai = require_menu("AI", 32u);
    Menu* memory = require_menu("Memory", 5u);
    Menu* script_memtrack = require_menu("Script Memtrack", 1u);
    Menu* slabs = require_menu("Slabs", 6u);
    Menu* full_slabs = require_menu("Full Slabs", 44u);
    Menu* partial_slabs = require_menu("Partial Slabs", 44u);
    Menu* allocated_objects = require_menu("Allocated Objects", 44u);
    Menu* free_objects = require_menu("Free Objects", 44u);
    Menu* entity_animations = require_menu("Entity Animations", 32u);
    Menu* entity_variants = require_menu("Entity Variants", 32u);

    if (root == nullptr || warp_menu == nullptr || game == nullptr || save_load == nullptr ||
        screenshot == nullptr || devopts == nullptr || gamefile == nullptr || missions == nullptr ||
        variant_menu == nullptr || replay_menu == nullptr || script == nullptr || progression == nullptr ||
        level_select == nullptr || hero_select == nullptr || debug_render == nullptr || ngl == nullptr ||
        ai == nullptr || memory == nullptr || script_memtrack == nullptr || slabs == nullptr ||
        full_slabs == nullptr || partial_slabs == nullptr || allocated_objects == nullptr ||
        free_objects == nullptr || entity_animations == nullptr || entity_variants == nullptr) {
        return;
    }

    runtime_menus.missions = missions;
    runtime_menus.level_select = level_select;
    runtime_menus.hero_select = hero_select;
    runtime_menus.ai = ai;
    runtime_menus.memory = memory;
    runtime_menus.script_memtrack = script_memtrack;
    runtime_menus.slabs = slabs;
    runtime_menus.full_slabs = full_slabs;
    runtime_menus.partial_slabs = partial_slabs;
    runtime_menus.allocated_objects = allocated_objects;
    runtime_menus.free_objects = free_objects;
    runtime_menus.entity_animations = entity_animations;
    runtime_menus.entity_variants = entity_variants;

    Entry* entry = nullptr;
    menu::add_submenu(*root, *warp_menu);
    menu::add_submenu(*root, *game);
    menu::add_submenu(*root, *missions);
    menu::add_submenu(*root, *debug_render);
    menu::add_submenu(*root, *variant_menu);
    menu::add_submenu(*root, *replay_menu);
    menu::add_submenu(*root, *ai);
    menu::add_submenu(*root, *memory);
    menu::add_submenu(*root, *entity_animations);
    menu::add_submenu(*root, *entity_variants);
    menu::add_submenu(*root, *level_select);
    menu::add_submenu(*root, *script);
    menu::add_submenu(*root, *progression);
    populate_camera(*root);

    entry = menu::add(*game, "Physics Enabled", EntryType::boolean8);
    entry->target = &physics_enabled;
    entry->handler = physics_handler;
    entry = menu::add(*game, "Single Step", EntryType::action);
    entry->handler = single_step_handler;
    entry = menu::add(*game, "Slow Motion Enabled", EntryType::boolean8);
    entry->target = &slow_motion_enabled;
    entry->handler = slow_motion_handler;
    monkey_enabled = engine::monkey_running() ? 1u : 0u;
    entry = menu::add(*game, "Monkey Enabled", EntryType::custom);
    entry->target = &monkey_enabled;
    entry->handler = monkey_handler;
    entry->formatter = monkey_formatter;
    entry = menu::add(*game, "Rumble Enabled", EntryType::custom);
    entry->target = &rumble_enabled;
    entry->handler = rumble_handler;
    entry->formatter = rumble_formatter;
    god_mode = engine::get_god_mode();
    entry = menu::add(*game, "God Mode", EntryType::custom);
    entry->target = &god_mode;
    entry->minimum = 0;
    entry->maximum = 5;
    entry->handler = god_mode_handler;
    entry->formatter = integer_target_formatter;
    add_developer_alias(*game, "Show Districts", 88u);
    add_developer_alias(*game, "Show Hero Position", 21u);
    add_developer_alias(*game, "Show FPS", 5u);
    entry = menu::add(*game, "User Camera on Controller 2", EntryType::custom);
    entry->handler = controller2_handler;
    entry->formatter = controller2_formatter;
    entry = menu::add(*game, "Invert Camera Look", EntryType::custom);
    entry->id = 42u;  // Saved Game Settings: CAM_INVERTED_Y.
    entry->handler = game_value_handler;
    entry->formatter = game_value_formatter;
    entry = menu::add(*game, "Toggle Unload All Districts", EntryType::action);
    entry->handler = unload_districts_handler;
    menu::add_submenu(*game, *save_load);
    menu::add_submenu(*game, *screenshot);
    menu::add_submenu(*game, *devopts);
    menu::add_submenu(*game, *gamefile);
    devopt_features::configure_game_extras(*game);
    replay::configure_replay(*replay_menu);

    entry = menu::add(*save_load, "Save Slot", EntryType::integer32);
    entry->target = &save_slot;
    entry->minimum = 0;
    entry->maximum = 2;
    entry = menu::add(*save_load, "Save Game", EntryType::action);
    entry->handler = save_handler;
    entry = menu::add(*save_load, "Load Game", EntryType::action);
    entry->handler = load_handler;
    entry = menu::add(*save_load, "Attempt Auto Load", EntryType::action);
    entry->handler = auto_load_handler;

    entry = menu::add(*screenshot, "Hires Screenshot", EntryType::action);
    entry->handler = hires_handler;
    entry = menu::add(*screenshot, "Lores Screenshot", EntryType::action);
    entry->handler = lores_handler;

    devopt_features::configure_devopts(*devopts);

    for (u32 index = 0u; index < catalog::gamefile_label_count; ++index) {
        entry = menu::add(*gamefile, catalog::gamefile_labels[index], EntryType::custom);
        entry->id = index;
        entry->handler = game_value_handler;
        entry->formatter = game_value_formatter;
    }

    script_progression::populate_diagnostics(*script, *progression);
    character_spawn::populate(*script);

    for (u32 index = 0u; index < catalog::debug_render_label_count; ++index) {
        entry = menu::add(*debug_render, catalog::debug_render_labels[index], EntryType::custom);
        entry->id = index;
        entry->handler = debug_render_handler;
        entry->formatter = debug_render_formatter;
    }
    menu::add_submenu(*debug_render, *ngl);
    for (u32 index = 0u; index < catalog::ngl_flag_label_count; ++index) {
        entry = menu::add(*ngl, catalog::ngl_flag_labels[index], EntryType::custom);
        entry->id = index;
        entry->handler = ngl_handler;
        entry->formatter = ngl_formatter;
    }

    menu::set_root(*root);
    is_initialized = true;
}

void initialize() {
    initialize_for_render();
    if (!is_initialized || runtime_initialized) {
        return;
    }

    mission_level::attach_missions(*runtime_menus.missions);
    mission_level::attach_level_select(*runtime_menus.level_select,
                                       *runtime_menus.hero_select);
    hero_select::configure(*runtime_menus.hero_select);
    diagnostics::configure_ai(*runtime_menus.ai);
    diagnostics::configure_memory(*runtime_menus.memory,
                                  *runtime_menus.script_memtrack,
                                  *runtime_menus.slabs,
                                  *runtime_menus.full_slabs,
                                  *runtime_menus.partial_slabs,
                                  *runtime_menus.allocated_objects,
                                  *runtime_menus.free_objects);
    diagnostics::configure_entity_animations(*runtime_menus.entity_animations);
    diagnostics::configure_entity_variants(*runtime_menus.entity_variants);
    refresh_warp(*warp_menu);
    refresh_variants(*variant_menu);
    runtime_initialized = true;
}

}  // namespace usm::gc::registry
