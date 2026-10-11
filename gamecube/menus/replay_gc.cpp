#include "replay_gc.h"

#include "../gc_engine.h"
#include "hero_select_gc.h"
#include "mission_level_gc.h"

namespace usm::gc::replay {
namespace {

constexpr u32 port_count = 4u;
constexpr u16 button_z = 0x0010u;
constexpr u16 button_a = 0x0100u;

// Fixed arena storage: 3600 * 48 = 172800 bytes. No heap, files, RTDT
// manager, or native actor writes. Playback supplies inputs in the current
// world; it cannot rewind AI, physics, missions, or the player's position.
struct Sample { PadStatus pads[port_count]; };
static_assert(sizeof(Sample) == 48u);
Sample samples[maximum_samples];
enum class Mode : u32 { idle, arm_record, recording, arm_play, playing };
Mode mode;
u32 stored_samples;
u32 playback_cursor;
u32 controller_mask;
float duration_seconds;
bool clip_valid;
bool record_pending;
bool play_pending;

struct Owner {
    uptr game;
    uptr world;
    uptr terrain;
    uptr hero;
    u32 hero_handle;
};
Owner owner;

bool readable(uptr pointer, u32 bytes) {
    return (pointer & 3u) == 0u && pointer >= 0x80000000u &&
           pointer < 0x81800000u && bytes <= 0x81800000u - pointer;
}
u32 word(uptr pointer) { return *reinterpret_cast<volatile const u32*>(pointer); }

bool capture_owner(Owner& result) {
    const uptr game = reinterpret_cast<uptr>(engine::game());
    const uptr world = reinterpret_cast<uptr>(engine::world());
    if (!readable(game, 0x1ACu) || !readable(world, 0x228u) ||
        word(game + 0x68u) != world || word(game + 0x5Cu) == 0u ||
        word(game + 0x1A0u) == 0u || word(world + 0x224u) != 1u) return false;
    const uptr hero = word(world + 0x21Cu);
    const uptr terrain = word(world + 0x198u);
    if (!readable(hero, 0xC0u) || !readable(terrain, 4u) || word(hero + 0x1Cu) == 0u)
        return false;
    result = {game, world, terrain, hero, word(hero + 0x1Cu)};
    return true;
}

bool owner_current() {
    Owner latest{};
    return capture_owner(latest) && latest.game == owner.game &&
           latest.world == owner.world && latest.terrain == owner.terrain &&
           latest.hero == owner.hero && latest.hero_handle == owner.hero_handle;
}
bool transition_busy() {
    return menu::interaction_is_locked() || hero_select::busy() || mission_level::busy();
}
void stop(const char* message) {
    mode = Mode::idle;
    play_pending = false;
    menu::set_status(message);
}
void expire() {
    clip_valid = false;
    stop("Replay clip expired after world or hero changed");
}

bool command_ready() {
    if (transition_busy()) {
        menu::set_status("Replay waits for the current operation");
        return false;
    }
    Owner current{};
    if (!capture_owner(current)) {
        menu::set_status("Replay requires a loaded world and live hero");
        return false;
    }
    // State 14 is a native pause-return interval: allow arming there but
    // consume no samples until the native game actually returns to RUNNING.
    const s32 state = engine::game_state();
    if (state != 6 && state != 7 && state != 14) {
        menu::set_status("Replay waits for gameplay");
        return false;
    }
    if (word(current.game + 0x1A8u) != 0u) {
        menu::set_status("Enable Physics before recording or playing Replay");
        return false;
    }
    return true;
}
void clear_clip() {
    mode = Mode::idle;
    stored_samples = 0u;
    playback_cursor = 0u;
    controller_mask = 0u;
    duration_seconds = 0.0f;
    clip_valid = false;
    record_pending = false;
    play_pending = false;
    owner = {};
}

void action(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (entry.id == 2u) {
        stop("Replay stopped; recorded inputs retained");
        return;
    }
    if (entry.id == 3u) {
        clear_clip();
        menu::set_status("Replay recording cleared");
        return;
    }
    if (entry.id > 1u || !command_ready()) return;
    if (entry.id == 0u) {
        Owner current{};
        if (!capture_owner(current)) return;
        clear_clip();
        owner = current;
        clip_valid = true;
        mode = Mode::arm_record;
        menu::close();
        menu::set_status("Replay armed; release A. Z stops recording");
    } else {
        if (stored_samples == 0u || !clip_valid) {
            menu::set_status("Replay has no current recording; select Record first");
            return;
        }
        if (!owner_current()) { expire(); return; }
        playback_cursor = 0u;
        play_pending = false;
        mode = Mode::arm_play;
        menu::close();
        menu::set_status("Replay armed at current position; release A. Z stops");
    }
}

void format_state(const menu::Entry&, char* output, u32 capacity) {
    const char* text = "idle / empty";
    switch (mode) {
    case Mode::arm_record: text = "waiting to record; release A"; break;
    case Mode::recording: text = "recording; Z stops"; break;
    case Mode::arm_play: text = "waiting to play; release A"; break;
    case Mode::playing: text = "playing; Z stops"; break;
    case Mode::idle:
        if (stored_samples != 0u) text = clip_valid ? "idle / recording ready" : "idle / recording expired";
        break;
    }
    menu::copy_text(output, capacity, text);
}
void format_samples(const menu::Entry&, char* output, u32 capacity) {
    menu::format_s32(output, capacity, static_cast<s32>(stored_samples));
    menu::append_text(output, capacity, " / 3600, ");
    char duration[24];
    menu::format_float(duration, sizeof(duration), duration_seconds);
    menu::append_text(output, capacity, duration);
    menu::append_text(output, capacity, "s");
    if (mode == Mode::playing || mode == Mode::arm_play) {
        menu::append_text(output, capacity, "; played ");
        char cursor[16];
        menu::format_s32(cursor, sizeof(cursor), static_cast<s32>(playback_cursor));
        menu::append_text(output, capacity, cursor);
    }
}
void format_record(const menu::Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, "replace recording; Z stops");
}
void format_play(const menu::Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, "current position; world not rewound");
}

u32 connected_mask(const PadStatus* pads) {
    u32 mask = 0u;
    for (u32 port = 0u; port < port_count; ++port)
        if (pads[port].error == 0) mask |= 1u << port;
    return mask;
}
bool physical_z(const PadStatus* pads) {
    for (u32 port = 0u; port < port_count; ++port)
        if (pads[port].error == 0 && (pads[port].button & button_z) != 0u) return true;
    return false;
}
bool accept_held(const PadStatus* pads) {
    for (u32 port = 0u; port < port_count; ++port)
        if (pads[port].error == 0 && (pads[port].button & button_a) != 0u) return true;
    return false;
}
void neutralize(PadStatus* pads) {
    for (u32 port = 0u; port < port_count; ++port) {
        const s8 error = pads[port].error;
        pads[port] = {};
        pads[port].error = error;
    }
}
void deliver(PadStatus* pads, const Sample& sample) {
    for (u32 port = 0u; port < port_count; ++port) pads[port] = sample.pads[port];
}

}  // namespace

void configure_replay(menu::Menu& destination) {
    menu::clear_entries(destination);
    destination.refresh = nullptr;
    const char* const names[] = {"Record", "Play", "Stop", "Clear"};
    for (u32 index = 0u; index < 4u; ++index) {
        menu::Entry* entry = menu::add(destination, names[index], menu::EntryType::action);
        if (entry != nullptr) { entry->id = index; entry->handler = action; }
    }
    if (destination.count > 0u) destination.entries[0].formatter = format_record;
    if (destination.count > 1u) destination.entries[1].formatter = format_play;
    menu::Entry* entry = menu::add(destination, "State", menu::EntryType::read_only);
    if (entry != nullptr) entry->formatter = format_state;
    entry = menu::add(destination, "Samples / Duration", menu::EntryType::read_only);
    if (entry != nullptr) entry->formatter = format_samples;
}

void process_game_pads(PadStatus* pads, bool menu_has_focus) {
    if (pads == nullptr || mode == Mode::idle) return;
    // observe_pads() already saw physical Z. Never inject on this poll, so
    // recorded input cannot trap the menu or replace its navigation controls.
    if (physical_z(pads)) {
        stop("Replay stopped by Z; recorded inputs retained");
        neutralize(pads);
        return;
    }
    if (transition_busy()) { stop("Replay stopped for a game operation"); return; }
    if (!clip_valid || !owner_current()) { expire(); return; }
    if (menu_has_focus) return;
    const s32 state = engine::game_state();
    if (state == 7 || state == 14) return;
    if (state != 6) { stop("Replay stopped while gameplay is unavailable"); return; }
    if (word(owner.game + 0x1A8u) != 0u) { stop("Replay stopped because Physics is disabled"); return; }

    const u32 connected = connected_mask(pads);
    if (mode == Mode::arm_record || mode == Mode::arm_play) {
        if (connected == 0u || accept_held(pads)) { neutralize(pads); return; }
        if (mode == Mode::arm_record) {
            controller_mask = connected;
            mode = Mode::recording;
        } else mode = Mode::playing;
    }
    for (u32 port = 0u; port < port_count; ++port) {
        if ((controller_mask & (1u << port)) == 0u) continue;
        if (pads[port].error == -1) { stop("Replay stopped: recorded controller disconnected"); return; }
        if (pads[port].error != 0) {
            // Preserve SDK NOT_READY/TRANSFER and hold the stream position.
            if (mode == Mode::playing) neutralize(pads);
            return;
        }
    }
    if (mode == Mode::recording) {
        if (record_pending) {
            // gc_joypad_device::poll may call PADRead once per device. Every
            // device sees this update's same captured four-port sample.
            deliver(pads, samples[stored_samples - 1u]);
            return;
        }
        if (stored_samples >= maximum_samples) { stop("Replay buffer full; recording retained"); return; }
        Sample& sample = samples[stored_samples++];
        for (u32 port = 0u; port < port_count; ++port) {
            sample.pads[port] = pads[port];
            sample.pads[port].button = static_cast<u16>(sample.pads[port].button & ~button_z);
        }
        record_pending = true;
        deliver(pads, sample);
    } else if (mode == Mode::playing) {
        if (playback_cursor >= stored_samples) { stop("Replay playback complete"); return; }
        deliver(pads, samples[playback_cursor]);
        play_pending = true;
    }
}

void frame_advance(float time_inc) {
    if (record_pending) {
        // Count one stock update, not one PADRead per controller. Reject
        // non-finite, negative and unreasonable durations before summation.
        if (time_inc >= 0.0f && time_inc <= 0.25f) duration_seconds += time_inc;
        record_pending = false;
        if (mode == Mode::recording && stored_samples == maximum_samples)
            stop("Replay buffer full; recording retained");
    }
    if (play_pending) {
        play_pending = false;
        if (mode == Mode::playing) {
            ++playback_cursor;
            if (playback_cursor >= stored_samples)
                stop("Replay playback complete; live controls restored");
        }
    }
    if (clip_valid && !owner_current()) expire();
}

}  // namespace usm::gc::replay
