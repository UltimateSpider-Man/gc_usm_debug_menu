#include "devopt_features_gc.h"

#include "../gc_addresses.h"
#include "../gc_engine.h"

namespace usm::gc::devopt_features {
namespace {

using menu::Command;
using menu::Entry;
using menu::EntryType;
using menu::Menu;

constexpr u32 option_count = address::developer_flag_count + address::developer_int_count;
char fallback_labels[option_count][24];
void* slow_options_owner;
s32 previous_frame_lock;
bool previous_frame_lock_valid;
u32 previous_tick;
u32 elapsed_ticks;
u32 rendered_frames;
bool tick_valid;
float measured_fps;

bool equal_text(const char* lhs, const char* rhs) {
    if (lhs == nullptr || rhs == nullptr) return false;
    for (u32 i = 0u;; ++i) {
        if (lhs[i] != rhs[i]) return false;
        if (lhs[i] == '\0') return true;
    }
}

bool mem1_object(const void* object, u32 size) {
    return is_main_ram_pointer(object) &&
           size <= 0x81800000u - reinterpret_cast<uptr>(object);
}

bool options_ready() {
    // Last GC integer is +0x474. A plausible pointer to the last MEM1 word
    // does not authorize reading the whole native options object.
    return mem1_object(engine::developer_options(), 0x478u);
}

bool world_ready() {
    void* game = engine::game();
    void* world = engine::world();
    return mem1_object(game, 0x1B4u) && mem1_object(world, 0x220u) &&
           engine::interactive_world_ready() &&
           *reinterpret_cast<void**>(static_cast<u8*>(game) + 0x68u) == world &&
           *reinterpret_cast<volatile u32*>(static_cast<u8*>(game) + 0x1A0u) != 0u &&
           *reinterpret_cast<volatile u32*>(static_cast<u8*>(game) + 0x5Cu) != 0u;
}

bool changed(Command command) {
    return command == Command::activate || command == Command::increment ||
           command == Command::decrement;
}

s32 adjusted(s32 value, Command command, s32 minimum, s32 maximum) {
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    s32 amount = menu::change_multiplier();
    if (amount < 1) amount = 1;
    if (amount > 1000) amount = 1000;
    if (command == Command::decrement)
        return value < minimum + amount ? minimum : value - amount;
    return value > maximum - amount ? maximum : value + amount;
}

void boolean_text(bool value, char* output, u32 capacity) {
    menu::copy_text(output, capacity, value ? "True" : "False");
}

bool flag_named(u32 index, const char* name) {
    return equal_text(engine::developer_flag_name(index), name);
}

bool int_named(u32 index, const char* name) {
    return equal_text(engine::developer_int_name(index), name);
}

void flag_formatter(const Entry& entry, char* output, u32 capacity) {
    if (!options_ready() || entry.id >= address::developer_flag_count) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    if (flag_named(entry.id, "USERCAM_ON_CONTROLLER2")) {
        boolean_text(engine::controller2_camera_enabled(), output, capacity);
    } else {
        boolean_text(engine::get_developer_flag(entry.id), output, capacity);
    }
}

void flag_handler(Entry& entry, Command command) {
    if (!changed(command)) return;
    if (!options_ready() || entry.id >= address::developer_flag_count) {
        menu::set_status("Developer options unavailable");
        return;
    }
    if (flag_named(entry.id, "USERCAM_ON_CONTROLLER2")) {
        if (!engine::set_controller2_camera(!engine::controller2_camera_enabled()))
            menu::set_status("Controller 2 is disconnected or camera unavailable");
        return;
    }
    engine::set_developer_flag(entry.id, !engine::get_developer_flag(entry.id));
}

void int_formatter(const Entry& entry, char* output, u32 capacity) {
    if (!options_ready() || entry.id >= address::developer_int_count) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    const s32 value = world_ready() && int_named(entry.id, "GOD_MODE")
                          ? engine::get_god_mode()
                          : world_ready() && int_named(entry.id, "CAMERA_STATE")
                                ? engine::get_camera()
                                : engine::get_developer_int(entry.id);
    menu::format_s32(output, capacity, value);
}

void int_handler(Entry& entry, Command command) {
    if (command != Command::decrement && command != Command::increment) return;
    if (!options_ready() || entry.id >= address::developer_int_count) {
        menu::set_status("Developer options unavailable");
        return;
    }
    if (int_named(entry.id, "CAMERA_STATE")) {
        const s32 next = adjusted(engine::get_camera(), command, 0, 2);
        if (!engine::camera_available(next)) {
            menu::set_status("Camera objects unavailable in this level");
            return;
        }
        engine::set_camera(next);
        return;
    }
    if (int_named(entry.id, "GOD_MODE")) {
        const s32 current = world_ready() ? engine::get_god_mode()
                                         : engine::get_developer_int(entry.id);
        const s32 next = adjusted(current, command, 0, 5);
        engine::set_developer_int(entry.id, next);
        // Retail only applies this integer during app initialization at
        // 801CFD54. Apply the verified live cheat setter as the PC Game row does.
        if (world_ready()) engine::set_god_mode(next);
        return;
    }
    engine::set_developer_int(entry.id,
                             adjusted(engine::get_developer_int(entry.id), command, -1000, 1000));
}

void physics_formatter(const Entry&, char* output, u32 capacity) {
    if (!world_ready()) {
        menu::copy_text(output, capacity, "N/A");
        return;
    }
    boolean_text(*reinterpret_cast<volatile u32*>(static_cast<u8*>(engine::game()) +
                                                 0x1A8u) == 0u, output, capacity);
}

void physics_handler(Entry&, Command command) {
    if (!changed(command)) return;
    if (!world_ready()) {
        menu::set_status("Physics unavailable outside gameplay");
        return;
    }
    const bool enabled = *reinterpret_cast<volatile u32*>(
                             static_cast<u8*>(engine::game()) + 0x1A8u) == 0u;
    engine::set_physics(!enabled);
}

void slow_motion_formatter(const Entry&, char* output, u32 capacity) {
    if (!options_ready()) menu::copy_text(output, capacity, "N/A");
    else boolean_text(engine::get_developer_int(13u) == 120, output, capacity);
}

void slow_motion_handler(Entry&, Command command) {
    if (!changed(command)) return;
    if (!options_ready()) {
        menu::set_status("Slow motion unavailable: Devopts not ready");
        return;
    }
    void* owner = engine::developer_options();
    if (slow_options_owner != owner) {
        slow_options_owner = owner;
        previous_frame_lock_valid = false;
    }
    const s32 current = engine::get_developer_int(13u);
    if (current == 120) {
        engine::set_developer_int(13u, previous_frame_lock_valid ? previous_frame_lock : 0);
        previous_frame_lock_valid = false;
    } else {
        previous_frame_lock = current;
        previous_frame_lock_valid = true;
        engine::set_developer_int(13u, 120);
    }
    menu::close();
}

void god_mode_formatter(const Entry&, char* output, u32 capacity) {
    if (!world_ready() || !options_ready()) menu::copy_text(output, capacity, "N/A");
    else menu::format_s32(output, capacity, engine::get_god_mode());
}

void god_mode_handler(Entry&, Command command) {
    if (command != Command::decrement && command != Command::increment) return;
    if (!world_ready() || !options_ready()) {
        menu::set_status("God Mode unavailable outside gameplay");
        return;
    }
    const s32 next = adjusted(engine::get_god_mode(), command, 0, 5);
    engine::set_developer_int(29u, next);
    engine::set_god_mode(next);
    menu::close();
}

void assign(Entry& entry, menu::Handler handler, menu::Formatter formatter) {
    entry.type = EntryType::custom;
    entry.target = nullptr;
    entry.handler = handler;
    entry.formatter = formatter;
}

void numbered_label(char* output, const char* prefix, u32 index) {
    menu::copy_text(output, 24u, prefix);
    char number[16];
    menu::format_s32(number, sizeof(number), static_cast<s32>(index));
    menu::append_text(output, 24u, number);
}

void append_float(char* output, u32 capacity, const char* label, float value) {
    menu::append_text(output, capacity, label);
    char text[24];
    if (value >= -1000000.0f && value <= 1000000.0f)
        menu::format_float(text, sizeof(text), value);
    else menu::copy_text(text, sizeof(text), "N/A");
    menu::append_text(output, capacity, text);
}

void update_fps() {
    // SDK OSGetTick has a one-word return (802FA778: mftb r3; blr). Bus
    // clock at 800000F8 is the SDK's timebase source; TB ticks at bus/4.
    const u32 now = reinterpret_cast<u32 (*)()>(0x802FA778u)();
    const u32 bus_clock = volatile_memory<u32>(0x800000F8u);
    if (bus_clock < 16000000u || bus_clock > 400000000u) {
        tick_valid = false;
        measured_fps = 0.0f;
        return;
    }
    const u32 frequency = bus_clock / 4u;
    if (!tick_valid) {
        previous_tick = now;
        elapsed_ticks = 0u;
        rendered_frames = 0u;
        tick_valid = true;
        return;
    }
    const u32 delta = now - previous_tick; // Defined unsigned wrap at TB low-word rollover.
    previous_tick = now;
    if (delta > frequency * 2u || delta > 0xFFFFFFFFu - elapsed_ticks) {
        elapsed_ticks = 0u;
        rendered_frames = 0u;
        measured_fps = 0.0f;
        return;
    }
    elapsed_ticks += delta;
    ++rendered_frames;
    if (elapsed_ticks >= frequency / 4u) {
        measured_fps = static_cast<float>(rendered_frames) * static_cast<float>(frequency) /
                       static_cast<float>(elapsed_ticks);
        elapsed_ticks = 0u;
        rendered_frames = 0u;
    }
}

void* hero() {
    void* value = *reinterpret_cast<void**>(static_cast<u8*>(engine::world()) + 0x21Cu);
    return mem1_object(value, 0x50u) ? value : nullptr;
}

u32 region_count() {
    void* terrain = engine::terrain();
    if (!mem1_object(terrain, 0x10u)) return 0u;
    const s32 count = *reinterpret_cast<volatile s32*>(static_cast<u8*>(terrain) + 0xCu);
    if (count <= 0 || count >= 1024) return 0u;
    void* regions = *reinterpret_cast<void**>(terrain);
    return mem1_object(regions, static_cast<u32>(count) * 4u) ? static_cast<u32>(count) : 0u;
}

}  // namespace

void configure_devopts(Menu& destination) {
    menu::clear_entries(destination);
    for (u32 index = 0u; index != address::developer_flag_count; ++index) {
        const char* label = engine::developer_flag_name(index);
        if (label == nullptr) {
            numbered_label(fallback_labels[index], "Flag ", index);
            label = fallback_labels[index];
        }
        Entry* entry = menu::add(destination, label, EntryType::custom);
        if (entry == nullptr) break;
        entry->id = index;
        assign(*entry, flag_handler, flag_formatter);
    }
    for (u32 index = 0u; index != address::developer_int_count; ++index) {
        const char* label = engine::developer_int_name(index);
        if (label == nullptr) {
            numbered_label(fallback_labels[address::developer_flag_count + index], "Int ", index);
            label = fallback_labels[address::developer_flag_count + index];
        }
        Entry* entry = menu::add(destination, label, EntryType::custom);
        if (entry == nullptr) break;
        entry->id = index;
        entry->minimum = -1000;
        entry->maximum = 1000;
        assign(*entry, int_handler, int_formatter);
    }
}

void configure_game_extras(Menu& destination) {
    for (u32 index = 0u; index != destination.count; ++index) {
        Entry& entry = destination.entries[index];
        if (equal_text(entry.label, "Physics Enabled")) assign(entry, physics_handler, physics_formatter);
        else if (equal_text(entry.label, "Slow Motion Enabled"))
            assign(entry, slow_motion_handler, slow_motion_formatter);
        else if (equal_text(entry.label, "God Mode")) assign(entry, god_mode_handler, god_mode_formatter);
        else {
            const char* name = equal_text(entry.label, "Show Districts") ? "SHOW_DEBUG_TEXT"
                               : equal_text(entry.label, "Show Hero Position") ? "SHOW_DEBUG_INFO"
                               : equal_text(entry.label, "Show FPS") ? "SHOW_FPS" : nullptr;
            if (name == nullptr) continue;
            for (u32 option = 0u; option != address::developer_flag_count; ++option) {
                if (!flag_named(option, name)) continue;
                entry.id = option;
                assign(entry, flag_handler, flag_formatter);
                break;
            }
        }
    }
}

void render() {
    if (!options_ready() || !world_ready()) {
        tick_valid = false;
        return;
    }
    const bool fps = engine::get_developer_flag(5u);
    const bool position = engine::get_developer_flag(21u);
    const bool districts = engine::get_developer_flag(88u) || engine::get_developer_flag(99u);
    const bool streamer = engine::get_developer_flag(6u);
    if (!fps) tick_valid = false;
    if (!fps && !position && !districts && !streamer) return;
    char text[192];
    float y = 12.0f;
    if (fps) {
        update_fps();
        menu::copy_text(text, sizeof(text), "FPS: ");
        char value[24];
        if (measured_fps > 0.0f && measured_fps < 1000000.0f)
            menu::format_float(value, sizeof(value), measured_fps);
        else menu::copy_text(value, sizeof(value), "...");
        menu::append_text(text, sizeof(text), value);
        engine::draw_text(text, 24.0f, y, 0xFFFFFFFFu, 0.65f);
        y += 18.0f;
    }
    void* actor = hero();
    if (position) {
        menu::copy_text(text, sizeof(text), "Hero");
        // Actual entity_base::get_abs_po at801354C4 returns native pose+14,
        // resolving the dirty matrix first. Translation is pose+30.
        void* pose = actor == nullptr ? nullptr
                        : reinterpret_cast<void* (*)(void*)>(0x801354C4u)(actor);
        if (mem1_object(pose, 0x3Cu)) {
            auto* coordinates = reinterpret_cast<volatile float*>(static_cast<u8*>(pose) + 0x30u);
            append_float(text, sizeof(text), " X: ", coordinates[0]);
            append_float(text, sizeof(text), " Y: ", coordinates[1]);
            append_float(text, sizeof(text), " Z: ", coordinates[2]);
        } else menu::append_text(text, sizeof(text), ": N/A");
        engine::draw_text(text, 24.0f, y, 0xFFFFFFFFu, 0.65f);
        y += 18.0f;
    }
    if (streamer) {
        menu::copy_text(text, sizeof(text), "Regions: ");
        char count[16];
        menu::format_s32(count, sizeof(count), static_cast<s32>(region_count()));
        menu::append_text(text, sizeof(text), count);
        engine::draw_text(text, 24.0f, y, 0xFFFFFFFFu, 0.65f);
    }
    if (districts) {
        // get_primary_region's actual entry is80100D94. Only accept its
        // result if it remains in the current terrain's region catalogue.
        void* region = actor == nullptr ? nullptr
                          : reinterpret_cast<void* (*)(void*)>(0x80100D94u)(actor);
        bool owned = false;
        for (u32 index = 0u, count = region_count(); index != count; ++index)
            if (engine::terrain_region(index) == region) { owned = true; break; }
        engine::draw_text("DISTRICTS", 269.0f, 345.0f, 0xFFFFFFFFu, 0.75f);
        menu::copy_text(text, sizeof(text), "--");
        if (owned && mem1_object(region, 0xC4u)) {
            // region_lookup_cache::init8017790C copies region+C0 into its
            // district-id table. +CC is a different scalar.
            menu::format_s32(text, sizeof(text), *reinterpret_cast<volatile s32*>(
                                                   static_cast<u8*>(region) + 0xC0u));
        }
        engine::draw_text(text, 237.0f, 369.0f, 0x20E040FFu, 0.75f);
    }
}

}  // namespace usm::gc::devopt_features
