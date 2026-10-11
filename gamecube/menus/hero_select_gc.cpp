#include "hero_select_gc.h"

#include "../gc_engine.h"
#include "character_spawn_gc.h"
#include "mission_level_gc.h"

namespace usm::gc::hero_select {
namespace {

// These are the actual function entries verified in the supplied NTSC-U DOL.
// gc_address.txt labels several of the corresponding symbols after their
// prologues, so those label addresses are not safe call targets.
constexpr uptr world_add_player = 0x8015323Cu;
constexpr uptr world_remove_player = 0x80153A7Cu;
constexpr uptr game_enable_marky_camera = 0x8017EAC0u;
constexpr uptr string_hash_set = 0x8015BF68u;
constexpr uptr resource_get_pack_stats = 0x8015D730u;

// Gameplay-REL entries.  Calls are made only in game states 6/7, while the
// REL mapping is live.  The gc_address.txt labels (0x7F02CD78/0x7F02D1A0)
// come from a different linked revision: in the supplied DOL they land in
// unrelated function bodies.  These entries were recovered by matching each
// complete routine and its ABI; the local -0x644 shift is not generalized.
constexpr uptr health_widget_set_type = 0x7F02C734u;
constexpr uptr health_widget_set_shown = 0x7F02CB5Cu;

constexpr uptr resource_manager_pointer = 0x8045F214u;
constexpr uptr igo_frontend_pointer = 0x803B848Cu;
constexpr uptr mem1_begin = 0x80000000u;
constexpr uptr mem1_end = 0x81800000u;

constexpr u32 world_hero_offset = 0x21Cu;
constexpr u32 world_chase_camera_offset = 0x220u;
constexpr u32 world_scene_camera_offset = 0x6Cu;
constexpr u32 world_player_count_offset = 0x224u;
constexpr u32 world_hero_name_offset = 0x3DCu;
constexpr u32 actor_handle_offset = 0x1Cu;
constexpr u32 actor_player_controller_offset = 0x8Cu;
constexpr u32 controller_hero_type_offset = 0x420u;
constexpr u32 igo_hero_health_offset = 0x0Cu;
constexpr s32 pack_resource_type = 25;
constexpr u8 interstage_delay_frames = 2u;
constexpr u8 recovery_retry_delay_frames = 30u;
constexpr u16 hud_retry_limit_frames = 180u;

const char* const hero_names[hero_count] = {
    "ultimate_spiderman",
    "arachno_man_costume",
    "usm_wrestling_costume",
    "usm_blacksuit_costume",
    "peter_parker",
    "peter_parker_costume",
    "peter_hooded",
    "peter_hooded_costume",
    "venom",
    "venom_spider",
};

enum class Phase : u8 {
    idle,
    queued,
    remove_player,
    wait_before_add,
    wait_before_hud,
    wait_for_camera_cleanup,
};

struct ResourceKey {
    u32 hash;
    s32 type;
};

static_assert(sizeof(ResourceKey) == 8u);

using RemovePlayerFn = s32 (*)(void* world, s32 player_index);
using AddPlayerFn = s32 (*)(void* world, engine::MString* hero_name);
using EnableMarkyCameraFn = void (*)(void* game,
                                     bool enabled,
                                     bool preserve_camera,
                                     float transition_time,
                                     float camera_time);
using SetHashFn = void (*)(u32* hash, const char* text);
using GetPackStatsFn = bool (*)(const ResourceKey* key,
                                void* slot,
                                engine::MString* pack_name,
                                s32* source_index);
using HealthSetTypeFn = void (*)(void* widget, s32 type, const u32* hero_handle);
using HealthSetShownFn = void (*)(void* widget, bool shown);

Phase phase;
u32 selected_hero;
u8 delay_frames;
u16 hud_retry_frames;
bool marky_camera_owned;
bool recovering_previous_hero;
u32 recovery_hero;
void* operation_game;
void* operation_world;
u16 cleanup_wait_ticks;

template <typename Function>
inline Function function_at(uptr location) {
    return reinterpret_cast<Function>(location);
}

bool readable_range(const void* pointer, u32 size) {
    const uptr address = reinterpret_cast<uptr>(pointer);
    return address >= mem1_begin && address < mem1_end && size <= mem1_end - address;
}

bool readable_object(const void* pointer, u32 size) {
    return readable_range(pointer, size) &&
           (reinterpret_cast<uptr>(pointer) & 3u) == 0u;
}

void* load_pointer(const void* object, u32 offset) {
    if (!readable_object(object, offset + sizeof(void*))) {
        return nullptr;
    }
    const auto* bytes = static_cast<const u8*>(object);
    return *reinterpret_cast<void* const*>(bytes + offset);
}

s32 load_s32(const void* object, u32 offset) {
    const auto* bytes = static_cast<const u8*>(object);
    return *reinterpret_cast<const volatile s32*>(bytes + offset);
}

u32 load_u32(const void* object, u32 offset) {
    const auto* bytes = static_cast<const u8*>(object);
    return *reinterpret_cast<const volatile u32*>(bytes + offset);
}

bool interactive_world_ready() {
    const s32 state = engine::game_state();
    return state == 6 || state == 7;
}

bool same_operation_context() {
    return interactive_world_ready() && operation_game != nullptr &&
           operation_world != nullptr && engine::game() == operation_game &&
           engine::world() == operation_world &&
           readable_object(operation_world, world_player_count_offset + sizeof(s32));
}

bool resource_system_ready() {
    void* manager = memory<void*>(resource_manager_pointer);
    return readable_object(manager, sizeof(void*)) &&
           readable_object(load_pointer(manager, 0u), sizeof(void*));
}

bool hero_pack_available(const char* name) {
    if (name == nullptr || !resource_system_ready()) {
        return false;
    }
    ResourceKey key{};
    function_at<SetHashFn>(string_hash_set)(&key.hash, name);
    key.type = pack_resource_type;
    return function_at<GetPackStatsFn>(resource_get_pack_stats)(
        &key, nullptr, nullptr, nullptr);
}

void set_marky_camera(bool enabled) {
    void* game = engine::game();
    if (game == nullptr || !interactive_world_ready()) {
        return;
    }
    function_at<EnableMarkyCameraFn>(game_enable_marky_camera)(
        game, enabled, true, -1000.0f, 0.0f);
    marky_camera_owned = enabled;
}

void finish_operation() {
    menu::set_interaction_locked(false);
    phase = Phase::idle;
    selected_hero = 0u;
    delay_frames = 0u;
    hud_retry_frames = 0u;
    marky_camera_owned = false;
    recovering_previous_hero = false;
    recovery_hero = 0u;
    operation_game = nullptr;
    operation_world = nullptr;
    cleanup_wait_ticks = 0u;
}

void abort_operation(const char* reason) {
    if (marky_camera_owned && operation_game != nullptr &&
        operation_game == engine::game() && interactive_world_ready()) {
        function_at<EnableMarkyCameraFn>(game_enable_marky_camera)(
            operation_game, false, true, -1000.0f, 0.0f);
        finish_operation();
    } else if (marky_camera_owned && operation_game != nullptr &&
               operation_game == engine::game()) {
        // A level transition can temporarily make camera teardown unsafe.
        // Retain ownership and retry when this same game object is interactive.
        phase = Phase::wait_for_camera_cleanup;
    } else {
        finish_operation();
    }
    menu::set_status(reason);
}

bool find_recovery_hero(void* world, u32& index) {
    // add_player() persists its input mString inline at world+0x3DC.  The
    // entity ID at hero+0x10 is instead the instance ID (normally "HERO"),
    // so it cannot identify the resource pack used to construct the actor.
    if (!readable_object(world, world_hero_name_offset + sizeof(engine::MString))) {
        return false;
    }
    const auto* stored = reinterpret_cast<const engine::MString*>(
        static_cast<const u8*>(world) + world_hero_name_offset);
    const s32 stored_size = stored->size;
    const char* stored_name = stored->guts;
    if (stored_size <= 0 || stored_size > 127 ||
        !readable_range(stored_name, static_cast<u32>(stored_size) + 1u) ||
        !engine::readable_c_string(stored_name) || stored_name[stored_size] != '\0') {
        return false;
    }

    for (u32 candidate = 0u; candidate < hero_count; ++candidate) {
        const u32 candidate_size = strlen(hero_names[candidate]);
        if (candidate_size == static_cast<u32>(stored_size) &&
            memcmp(stored_name, hero_names[candidate], candidate_size) == 0 &&
            hero_pack_available(hero_names[candidate])) {
            index = candidate;
            return true;
        }
    }
    return false;
}

bool operation_objects_unchanged() {
    return operation_game != nullptr && operation_world != nullptr &&
           engine::game() == operation_game && engine::world() == operation_world &&
           readable_object(operation_world, world_player_count_offset + sizeof(s32));
}

bool wait_through_transient_state(const char* cancelled_status) {
    if (operation_objects_unchanged()) {
        return true;
    }
    abort_operation(cancelled_status);
    return false;
}

bool retry_hud_or_finish(const char* waiting_status) {
    if (hud_retry_frames != 0u) {
        --hud_retry_frames;
        if ((hud_retry_frames % 30u) == 0u) {
            menu::set_status(waiting_status);
        }
        return true;
    }
    const bool restored_previous = recovering_previous_hero;
    finish_operation();
    menu::set_status(restored_previous
                         ? "Previous hero restored; health HUD repair timed out"
                         : "Hero changed; health HUD repair timed out");
    return false;
}

bool map_hud_type(s32 hero_type, s32& hud_type) {
    switch (hero_type) {
    case 1:
        hud_type = 0;
        return true;
    case 2:
        hud_type = 4;
        return true;
    case 3:
        hud_type = 5;
        return true;
    default:
        hud_type = 0;
        return false;
    }
}

bool repair_health_hud(const void* hero, s32 hud_type) {
    if (!engine::gameplay_rel_ready() ||
        !readable_object(hero, actor_handle_offset + sizeof(u32))) {
        return false;
    }
    const u32 hero_handle = load_u32(hero, actor_handle_offset);
    if (hero_handle == 0u) {
        return false;
    }

    void* igo = memory<void*>(igo_frontend_pointer);
    if (!readable_object(igo, igo_hero_health_offset + sizeof(void*))) {
        return false;
    }
    void* widget = load_pointer(igo, igo_hero_health_offset);
    if (!readable_object(widget, 0x5Cu)) {
        return false;
    }
    const s32 hud_type_count = load_s32(widget, 0x34u);
    if (hud_type < 0 || hud_type >= hud_type_count || hud_type_count > 16 ||
        !readable_object(load_pointer(widget, static_cast<u32>(hud_type) * 4u),
                         sizeof(void*))) {
        return false;
    }

    function_at<HealthSetTypeFn>(health_widget_set_type)(widget, hud_type, &hero_handle);
    if (load_s32(widget, 0x38u) != hud_type ||
        load_u32(widget, 0x30u) != hero_handle) {
        return false;
    }
    function_at<HealthSetShownFn>(health_widget_set_shown)(widget, true);
    return true;
}

void select_hero(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) {
        return;
    }
    if (entry.id >= hero_count) {
        menu::set_status("Invalid Hero Select entry");
        return;
    }
    if (phase != Phase::idle || mission_level::busy()) {
        menu::set_status("A hero or mission change is already in progress");
        return;
    }

    selected_hero = entry.id;
    // Do not alter the world from the command callback.  frame_advance()
    // consumes this one-tick barrier on the following game-update hook.
    operation_game = engine::game();
    operation_world = engine::world();
    cleanup_wait_ticks = 0u;
    if (character_spawn::has_resources()) {
        character_spawn::prepare_transition();
        menu::close();
    }
    menu::set_interaction_locked(true);
    phase = Phase::queued;
    menu::set_status("Hero change queued; cleaning up character lineup");
}

void refresh_heroes(menu::Menu& destination) {
    if (phase != Phase::idle) {
        return;
    }
    u32 available_mask = 0u;
    for (u32 index = 0u; index < hero_count; ++index) {
        if (hero_pack_available(hero_names[index])) {
            available_mask |= 1u << index;
        }
    }

    u32 current_mask = 0u;
    for (u16 row = 0u; row < destination.count; ++row) {
        const menu::Entry& entry = destination.entries[row];
        if (entry.handler == select_hero && entry.id < hero_count) {
            current_mask |= 1u << entry.id;
        }
    }
    if (current_mask == available_mask && destination.count != 0u) {
        return;
    }

    menu::clear_entries(destination);
    for (u32 index = 0u; index < hero_count; ++index) {
        if ((available_mask & (1u << index)) == 0u) {
            continue;
        }
        menu::Entry* entry =
            menu::add(destination, hero_names[index], menu::EntryType::action);
        if (entry != nullptr) {
            entry->id = index;
            entry->handler = select_hero;
        }
    }
    if (destination.count == 0u) {
        menu::add(destination, "No Hero Select packs available",
                  menu::EntryType::read_only);
    }
}

}  // namespace

void configure(menu::Menu& destination) {
    menu::clear_entries(destination);
    destination.refresh = refresh_heroes;
    finish_operation();
    refresh_heroes(destination);
}

void frame_advance() {
    switch (phase) {
    case Phase::idle:
        return;

    case Phase::queued:
        if (!operation_objects_unchanged() || !interactive_world_ready()) {
            abort_operation("Hero change cancelled after world replacement");
            return;
        }
        if (++cleanup_wait_ticks >= 7200u) {
            abort_operation("Hero change cancelled; character packs are still owned");
            return;
        }
        if (!character_spawn::prepare_transition()) return;
        menu::set_interaction_locked(true);
        phase = Phase::remove_player;
        return;

    case Phase::remove_player: {
        if (selected_hero >= hero_count || !interactive_world_ready()) {
            abort_operation("Hero Select requires a running or paused level");
            return;
        }
        operation_game = engine::game();
        operation_world = engine::world();
        if (operation_game == nullptr ||
            !readable_object(operation_world,
                             world_player_count_offset + sizeof(s32))) {
            abort_operation("Hero Select: world is not ready");
            return;
        }
        if (load_s32(operation_world, world_player_count_offset) != 1) {
            abort_operation("Hero Select requires exactly one current player");
            return;
        }
        // remove_player() decrements the player count before consuming these
        // objects, including a virtual call through the scene camera. A valid
        // saved hero name alone does not establish that construction settled.
        if (!readable_object(operation_game, 0x7Cu) ||
            load_u32(operation_game, 0x5Cu) == 0u ||
            load_pointer(operation_game, 0x68u) != operation_world ||
            !readable_object(load_pointer(operation_game, 0x74u), sizeof(void*)) ||
            !readable_object(load_pointer(operation_world, world_hero_offset),
                             actor_player_controller_offset + sizeof(void*)) ||
            !readable_object(load_pointer(operation_world, world_chase_camera_offset),
                             sizeof(void*)) ||
            !readable_object(load_pointer(operation_world, world_scene_camera_offset),
                             sizeof(void*))) {
            abort_operation("Hero Select: player or camera is not ready");
            return;
        }
        if (!hero_pack_available(hero_names[selected_hero])) {
            abort_operation("Selected hero pack is not available");
            return;
        }
        if (!find_recovery_hero(operation_world, recovery_hero)) {
            abort_operation("Current hero pack cannot be recovered safely");
            return;
        }
        if (recovery_hero == selected_hero) {
            finish_operation();
            menu::set_status("Selected hero is already active");
            return;
        }

        function_at<RemovePlayerFn>(world_remove_player)(operation_world, 0);
        if (load_s32(operation_world, world_player_count_offset) != 0) {
            abort_operation("Hero Select could not remove the current player");
            return;
        }
        set_marky_camera(true);
        if (!marky_camera_owned) {
            abort_operation("Hero Select could not enable the transition camera");
            return;
        }
        delay_frames = interstage_delay_frames;
        phase = Phase::wait_before_add;
        menu::set_status("Removing current hero...");
        return;
    }

    case Phase::wait_before_add: {
        if (!same_operation_context()) {
            wait_through_transient_state(
                "Hero Select cancelled: gameplay context changed");
            return;
        }
        if (delay_frames != 0u) {
            --delay_frames;
            return;
        }
        if (load_s32(operation_world, world_player_count_offset) != 0) {
            abort_operation("Hero Select: player removal did not settle");
            return;
        }

        const char* name = hero_names[selected_hero];
        if (recovering_previous_hero && !hero_pack_available(name)) {
            delay_frames = recovery_retry_delay_frames;
            menu::set_status("Waiting to restore the previous hero pack...");
            return;
        }
        engine::MString hero_name{};
        hero_name.size = static_cast<s32>(strlen(name));
        hero_name.guts = name;
        function_at<AddPlayerFn>(world_add_player)(operation_world, &hero_name);

        const s32 settled_count =
            load_s32(operation_world, world_player_count_offset);
        if (settled_count != 1) {
            if (settled_count == 0) {
                if (!recovering_previous_hero) {
                    selected_hero = recovery_hero;
                    recovering_previous_hero = true;
                    delay_frames = interstage_delay_frames;
                    menu::set_status("Hero add failed; restoring previous hero...");
                } else {
                    delay_frames = recovery_retry_delay_frames;
                    menu::set_status("Previous hero restore pending...");
                }
                return;
            }
            set_marky_camera(false);
            abort_operation("Hero Select stopped: unexpected player count");
            return;
        }
        set_marky_camera(false);
        const bool restored_previous = recovering_previous_hero;
        delay_frames = interstage_delay_frames;
        hud_retry_frames = hud_retry_limit_frames;
        phase = Phase::wait_before_hud;
        menu::set_status(restored_previous ? "Previous hero restored after add failure"
                                           : "Adding selected hero...");
        return;
    }

    case Phase::wait_before_hud: {
        if (!same_operation_context()) {
            wait_through_transient_state("Hero Select cancelled before HUD repair");
            return;
        }
        if (delay_frames != 0u) {
            --delay_frames;
            return;
        }
        if (load_s32(operation_world, world_player_count_offset) != 1) {
            abort_operation("Hero Select: new player is not ready");
            return;
        }

        void* hero = load_pointer(operation_world, world_hero_offset);
        if (!readable_object(hero, actor_player_controller_offset + sizeof(void*))) {
            retry_hud_or_finish("Waiting for the new hero object...");
            return;
        }
        void* controller = load_pointer(hero, actor_player_controller_offset);
        if (!readable_object(controller, controller_hero_type_offset + sizeof(s32))) {
            retry_hud_or_finish("Waiting for the new hero controller...");
            return;
        }

        s32 hud_type = 0;
        if (!map_hud_type(load_s32(controller, controller_hero_type_offset), hud_type)) {
            retry_hud_or_finish("Waiting for a recognized HUD hero type...");
            return;
        }
        if (!repair_health_hud(hero, hud_type)) {
            retry_hud_or_finish("Waiting for the health HUD...");
            return;
        }

        const bool restored_previous = recovering_previous_hero;
        finish_operation();
        menu::set_status(restored_previous
                             ? "Selected hero failed; previous hero restored"
                             : "Hero changed and health HUD repaired");
        menu::close();
        return;
    }

    case Phase::wait_for_camera_cleanup:
        if (operation_game == nullptr || operation_game != engine::game()) {
            finish_operation();
            return;
        }
        if (interactive_world_ready()) {
            function_at<EnableMarkyCameraFn>(game_enable_marky_camera)(
                operation_game, false, true, -1000.0f, 0.0f);
            finish_operation();
        }
        return;
    }
}

bool busy() { return phase != Phase::idle; }

}  // namespace usm::gc::hero_select
