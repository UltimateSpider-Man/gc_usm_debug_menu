#include "mission_level_gc.h"

#include "../gc_engine.h"
#include "../resource_memory_gc.h"
#include "../scene_config_gc.h"
#include "hero_select_gc.h"
#include "character_spawn_gc.h"

namespace usm::gc::mission_level {
namespace {

// All addresses below are entry points or globals verified against the supplied
// NTSC-U main.dol.  Several similarly named gc_address.txt symbols point into
// these functions and therefore must not be used as callable entry points.
constexpr uptr mission_manager_pointer = 0x8045F6ECu;
constexpr uptr resource_manager_pointer = 0x8045F214u;
constexpr uptr mission_prepare_unload = 0x801DB040u;
constexpr uptr mission_force = 0x801DB3C4u;
constexpr uptr resource_get_partition = 0x8015D968u;
constexpr uptr string_hash_set = 0x8015BF68u;
constexpr uptr resource_directory_get = 0x8015FF5Cu;
constexpr uptr resource_get_pack_stats = 0x8015D730u;
constexpr uptr game_load_new_level = 0x8017EA14u;
constexpr uptr mission_stack_pointer = 0x8045F6F4u;
constexpr uptr mission_stack_waiting = 0x801DCB9Cu;
constexpr uptr game_enable_marky_camera = 0x8017EAC0u;

constexpr uptr mem1_begin = 0x80000000u;
constexpr uptr mem1_end = 0x81800000u;
constexpr u32 mission_condition_size = 0x34u;
constexpr u32 mission_instance_size = 0x68u;
constexpr u32 level_descriptor_size = 0x90u;
constexpr u32 maximum_conditions_per_table = 512u;
constexpr u32 maximum_instances_per_condition = 128u;
constexpr u32 maximum_level_descriptors = 512u;
constexpr u32 maximum_district_tables = 8u;
constexpr u32 maximum_mission_records = 1024u;
constexpr u32 mission_table_count = maximum_district_tables + 1u;
enum class Hero : u8 { current, venom, spiderman };
enum class LaunchPhase : u8 { idle, queued, unloading, switching };
constexpr u32 maximum_level_records = recommended_level_capacity;

struct ResourceKey {
    u32 hash;
    s32 type;
};

static_assert(sizeof(ResourceKey) == 8u);

struct MissionRecord {
    const char* name;
    const char* script_data;
    char name_copy[32];
    char script_data_copy[32];
    const void* source_condition;
    const void* source_instance;
    const void* source_table;
    u32 condition_index;
    u32 generation;
    s32 district;
    s32 instance;
    Hero target;
    u8 table_index;
    bool cinematic;
};

struct LevelRecord {
    const char* internal_name;
    const char* display_name;
    char internal_name_copy[32];
    const void* source_descriptor;
    u32 generation;
};

using PrepareUnloadFn = void (*)(void* manager);
using ForceMissionFn = void (*)(void* manager,
                                s32 district,
                                const char* mission,
                                s32 instance,
                                const char* script_data);
using GetPartitionFn = void* (*)(s32 partition);
using SetHashFn = void (*)(u32* hash, const char* text);
using GetResourceFn = void* (*)(void* directory,
                                const ResourceKey* key,
                                s32* size,
                                void** source_slot);
using GetPackStatsFn = bool (*)(const ResourceKey* key,
                                void* slot,
                                engine::MString* pack_name,
                                s32* source_index);
using LoadLevelFn = void (*)(void* game, engine::MString* level_name);

MissionRecord mission_records[maximum_mission_records];
char mission_labels[maximum_mission_records][96];
u32 mission_record_count;
uptr mission_cache_signature = 0xFFFFFFFFu;
void* mission_cache_manager;
u32 mission_cache_generation;
bool mission_catalogue_truncated;
menu::Menu* mission_root;
menu::Menu* mission_groups[4];
menu::Menu* mission_leaf;
u32 leaf_generation;
u32 leaf_group = 4u;
u32 leaf_table = mission_table_count;
char district_labels[mission_table_count][64];

struct MissionLaunch {
    LaunchPhase phase;
    void* manager;
    void* world;
    void* game;
    char name[32];
    char data[32];
    s32 district;
    s32 instance;
    Hero target;
    bool has_data;
    bool unload_only;
    bool owns_lock;
    bool owns_camera;
    bool cancelled;
    u32 ticks;
};
MissionLaunch launch;
struct LevelLaunch {
    bool queued;
    void* world;
    void* game;
    u32 ticks;
    char name[32];
};
LevelLaunch pending_level;

LevelRecord level_records[maximum_level_records];
u32 level_record_count;
const void* level_cache_data;
s32 level_cache_size = -1;
u32 level_cache_generation;
u32 level_cache_signature;
char level_labels[maximum_level_records][16];
menu::Menu* level_hero_menu;

bool readable_range(const void* pointer, u32 bytes) {
    const uptr address = reinterpret_cast<uptr>(pointer);
    if (address < mem1_begin || address >= mem1_end) {
        return false;
    }
    return bytes <= mem1_end - address;
}

bool readable_object(const void* pointer, u32 bytes) {
    return readable_range(pointer, bytes) &&
           (reinterpret_cast<uptr>(pointer) & 3u) == 0u;
}

void* load_pointer(const void* base, u32 offset) {
    if (!resource_memory::readable_object(base, offset + sizeof(void*))) {
        return nullptr;
    }
    const u8* bytes = static_cast<const u8*>(base);
    return *reinterpret_cast<void* const*>(bytes + offset);
}

u32 load_u32(const void* base, u32 offset) {
    const u8* bytes = static_cast<const u8*>(base);
    return *reinterpret_cast<const volatile u32*>(bytes + offset);
}

u16 load_u16(const void* base, u32 offset) {
    const u8* bytes = static_cast<const u8*>(base);
    return *reinterpret_cast<const volatile u16*>(bytes + offset);
}

bool bounded_string(const char* text, u32 capacity, bool allow_empty = false) {
    if (text == nullptr || capacity == 0u) {
        return false;
    }
    uptr end = 0u;
    if (!resource_memory::readable_extent(reinterpret_cast<uptr>(text), end)) return false;
    for (u32 index = 0u; index < capacity; ++index) {
        if (index >= end - reinterpret_cast<uptr>(text)) {
            return false;
        }
        const u8 value = static_cast<u8>(text[index]);
        if (value == 0u) {
            return allow_empty || index != 0u;
        }
        if (value < 0x20u || value > 0x7Eu) {
            return false;
        }
    }
    return false;
}

bool same_bounded_text(const char* current, const char* snapshot, u32 capacity) {
    if (!bounded_string(current, capacity, true)) {
        return false;
    }
    for (u32 index = 0u; index < capacity; ++index) {
        if (current[index] != snapshot[index]) {
            return false;
        }
        if (snapshot[index] == '\0') {
            return true;
        }
    }
    return false;
}

u32 text_signature(const char* text, u32 capacity) {
    if (!bounded_string(text, capacity, true)) {
        return 0xBAD57A1Eu;
    }
    u32 result = 2166136261u;
    for (u32 index = 0u; index < capacity && text[index] != '\0'; ++index) {
        result = (result ^ static_cast<u8>(text[index])) * 16777619u;
    }
    return result;
}

void* mission_manager() {
    void* manager = memory<void*>(mission_manager_pointer);
    return readable_object(manager, 0x100u) ? manager : nullptr;
}

bool resource_system_ready() {
    void* manager = memory<void*>(resource_manager_pointer);
    if (!readable_object(manager, sizeof(void*))) {
        return false;
    }
    // get_partition_pointer() immediately dereferences the manager's first
    // word as the partition-pointer array.
    return readable_object(load_pointer(manager, 0u), sizeof(void*));
}

ResourceKey make_resource_key(const char* path, s32 type) {
    ResourceKey key{};
    reinterpret_cast<SetHashFn>(string_hash_set)(&key.hash, path);
    key.type = type;
    return key;
}

bool pack_exists(const char* path) {
    if (!resource_system_ready() || path == nullptr) {
        return false;
    }
    ResourceKey key = make_resource_key(path, 25);
    return reinterpret_cast<GetPackStatsFn>(resource_get_pack_stats)(
        &key, nullptr, nullptr, nullptr);
}

bool mission_pack_exists(const char* mission) {
    // force_mission() copies both names into native fixedstring<32> fields.
    // Reject longer authored names before its native assertion/copy path.
    if (!bounded_string(mission, 32u)) {
        return false;
    }
    char path[36];
    menu::copy_text(path, sizeof(path), "pk_");
    menu::append_text(path, sizeof(path), mission);
    return pack_exists(path);
}

bool current_hero_type(s32& type) {
    type = 0;
    void* world = engine::world();
    if (!readable_object(world, 0x228u)) {
        return false;
    }
    void* hero = load_pointer(world, 0x21Cu);
    if (!readable_object(hero, 0x90u)) {
        return false;
    }
    void* controller = load_pointer(hero, 0x8Cu);
    if (!readable_object(controller, 0x424u)) {
        return false;
    }
    type = static_cast<s32>(load_u32(controller, 0x420u));
    return true;
}

char lower_ascii(char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + 'a' - 'A') : value;
}

bool equal_text(const char* first, const char* second) {
    if (first == nullptr || second == nullptr) return first == second;
    for (u32 i = 0u; i < 32u; ++i) {
        if (lower_ascii(first[i]) != lower_ascii(second[i])) return false;
        if (first[i] == '\0') return true;
    }
    return false;
}

bool family(const char* name, const char* prefix) {
    u32 i = 0u;
    for (; prefix[i] != '\0'; ++i) if (name[i] != prefix[i]) return false;
    return name[i] == '\0' || name[i] == '_';
}

Hero requested_hero(const char* script, u32 flags) {
    char stem[32];
    const char* start = script;
    for (const char* p = script; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') start = p + 1;
    }
    menu::copy_text(stem, sizeof(stem), start);
    char* last_dot = nullptr;
    for (char* p = stem; *p != '\0'; ++p) {
        *p = lower_ascii(*p);
        if (*p == '.') last_dot = p;
    }
    if (last_dot != nullptr) *last_dot = '\0';
    const char* name = stem;
    if (stem[0] == 'p' && stem[1] == 'k' && stem[2] == '_') name += 3;
    const char* const venom[] = {"v02_wolverine", "v07", "v09", "v10", "v12"};
    const char* const spidey[] = {"s10", "s11", "s08", "s06", "s05", "s04", "s13"};
    for (const char* prefix : venom) if (family(name, prefix)) return Hero::venom;
    for (const char* prefix : spidey) if (family(name, prefix)) return Hero::spiderman;
    const u32 mask = flags & 0x1C000u;
    return mask == 0x4000u ? Hero::venom :
           mask == 0x8000u ? Hero::spiderman : Hero::current;
}

const char* hero_name(Hero hero) {
    return hero == Hero::venom ? "venom" :
           hero == Hero::spiderman ? "ultimate_spiderman" : "";
}

bool cinematic_text(const char* text) {
    if (text == nullptr) return false;
    const char* const tokens[] = {"igc", "cutscene", "cut_scene", "cinematic"};
    auto alpha = [](char c) { c = lower_ascii(c); return c >= 'a' && c <= 'z'; };
    for (u32 at = 0u; text[at] != '\0'; ++at) {
        if (at != 0u && alpha(text[at - 1u])) continue;
        for (const char* token : tokens) {
            u32 n = 0u;
            while (token[n] != '\0' && text[at + n] != '\0' &&
                   lower_ascii(text[at + n]) == token[n]) ++n;
            if (token[n] == '\0' && !alpha(text[at + n])) return true;
        }
    }
    return false;
}

void append_index(char* output, u32 capacity, s32 index) {
    char number[16];
    menu::format_s32(number, sizeof(number), index);
    menu::append_text(output, capacity, number);
}

void build_mission_label(char* output,
                         u32 capacity,
                         const char* mission,
                         const char* script_data,
                         s32 instance) {
    menu::copy_text(output, capacity, mission);
    menu::append_text(output, capacity, " (");
    if (script_data != nullptr && bounded_string(script_data, 32u, true)) {
        menu::append_text(output, capacity, script_data);
    } else {
        append_index(output, capacity, instance);
    }
    menu::append_text(output, capacity, ")");
}

bool mission_record_is_current(void* manager, const MissionRecord& record) {
    if (manager != mission_cache_manager ||
        !resource_memory::readable_object(record.source_table, 0x48u)) {
        return false;
    }

    // A MEM1 pointer can remain readable after a district or mission table has
    // been replaced. Prove ownership in the live manager before dereferencing
    // the cached condition and instance.
    bool table_found = load_pointer(manager, 0x14u) == record.source_table;
    if (table_found && record.district != 0) {
        return false;
    }
    const u32 district_count = load_u32(manager, 0x38u);
    if (district_count > maximum_district_tables) {
        return false;
    }
    for (u32 index = 0u; !table_found && index < district_count; ++index) {
        if (load_pointer(manager, 0x18u + index * 4u) != record.source_table) {
            continue;
        }
        void* region = load_pointer(record.source_table, 0x44u);
        table_found = resource_memory::readable_object(region, 0xC4u) &&
                      static_cast<s32>(load_u32(region, 0xC0u)) == record.district;
    }
    if (!table_found) {
        return false;
    }

    const u32 condition_count = load_u16(record.source_table, 0x3Cu);
    void* conditions = load_pointer(record.source_table, 0x38u);
    if (condition_count > maximum_conditions_per_table ||
        record.condition_index >= condition_count ||
        !resource_memory::readable_object(conditions, condition_count * mission_condition_size) ||
        static_cast<const u8*>(conditions) +
                record.condition_index * mission_condition_size != record.source_condition) {
        return false;
    }
    const u32 instance_count = load_u16(record.source_condition, 0x0Cu);
    void* instances = load_pointer(record.source_condition, 0x08u);
    return record.instance >= 0 &&
           instance_count <= maximum_instances_per_condition &&
           static_cast<u32>(record.instance) < instance_count &&
           resource_memory::readable_object(instances, instance_count * mission_instance_size) &&
           static_cast<const u8*>(instances) +
                   static_cast<u32>(record.instance) * mission_instance_size ==
               record.source_instance;
}

bool launch_owner_live() {
    return launch.manager != nullptr && launch.manager == mission_manager() &&
           launch.world != nullptr && launch.world == engine::world() &&
           launch.game != nullptr && launch.game == engine::game();
}

void store_u32(void* object, u32 offset, u32 value) {
    *reinterpret_cast<volatile u32*>(static_cast<u8*>(object) + offset) = value;
}

bool hero_matches(Hero target) {
    void* world = engine::world();
    if (!readable_object(world, 0x3ECu) || load_u32(world, 0x224u) != 1u ||
        !readable_object(load_pointer(world, 0x21Cu), 0x90u)) return false;
    if (target == Hero::current) return true;
    const auto* name = reinterpret_cast<const engine::MString*>(
        static_cast<const u8*>(world) + 0x3DCu);
    return name->size > 0 && name->size < 32 && bounded_string(name->guts, 32u) &&
           equal_text(name->guts, hero_name(target));
}

bool transition_cameras_ready(void* game, void* world) {
    // GC game+5C is the loader-completed WORD (0/1), not PC's camera field.
    // Native enable_marky(8017EAC0) follows game+68 to the world, synchronizes
    // from game+74, and dereferences the Marky camera at world+6C.
    return readable_object(game, 0x7Cu) && readable_object(world, 0x228u) &&
           load_u32(game, 0x5Cu) != 0u && load_pointer(game, 0x68u) == world &&
           readable_object(load_pointer(game, 0x74u), sizeof(void*)) &&
           readable_object(load_pointer(world, 0x6Cu), 0x1E0u) &&
           readable_object(load_pointer(world, 0x220u), sizeof(void*));
}

bool packs_busy() {
    void* stack = memory<void*>(mission_stack_pointer);
    return !readable_object(stack, 0x0Cu) || load_u32(stack, 0u) != 0u ||
           reinterpret_cast<bool (*)(void*)>(mission_stack_waiting)(stack);
}

void finish_launch(const char* status) {
    if (launch_owner_live()) {
        if (launch.owns_lock && load_u32(launch.manager, 0xCCu) != 0u)
            store_u32(launch.manager, 0xCCu, 0u);
        if (launch.owns_camera && readable_object(launch.world, 0x74u) &&
            load_u32(launch.world, 0x70u) != 0u && engine::gameplay_rel_ready()) {
            reinterpret_cast<void (*)(void*, bool, bool, float, float)>(
                game_enable_marky_camera)(launch.game, false, true, 0.0f, 0.0f);
        }
    }
    launch = {};
    menu::set_interaction_locked(false);
    menu::set_status(status);
}

bool request_mission(const MissionRecord& record, void* manager) {
    void* world = engine::world();
    void* game = engine::game();
    if (readable_object(game, 0x1ACu) && load_u32(game, 0x1A8u) != 0u) {
        menu::set_status("Enable Physics before changing missions");
        return false;
    }
    if (busy() || hero_select::busy() ||
        !readable_object(world, 0x3ECu) || !readable_object(game, 0x1B4u) ||
        load_u32(world, 0x224u) != 1u || load_u32(game, 0x1A0u) == 0u ||
        load_u32(game, 0x5Cu) == 0u ||
        !readable_object(load_pointer(world, 0x21Cu), 0x90u) ||
        static_cast<s32>(load_u32(manager, 0xF0u)) != -1 ||
        load_pointer(manager, 0x3Cu) != nullptr || load_u32(manager, 0x7Cu) != 0u ||
        load_u32(manager, 0xCCu) != 0u || record.district < 0 || record.instance < 0) {
        menu::set_status("Mission launch is busy; wait for gameplay");
        return false;
    }
    if (record.target != Hero::current && !pack_exists(hero_name(record.target))) {
        menu::set_status("Required hero pack is unavailable");
        return false;
    }
    launch = {};
    launch.phase = LaunchPhase::queued;
    launch.manager = manager;
    launch.world = world;
    launch.game = game;
    menu::copy_text(launch.name, sizeof(launch.name), record.name_copy);
    menu::copy_text(launch.data, sizeof(launch.data), record.script_data_copy);
    launch.has_data = record.script_data != nullptr;
    launch.district = record.district;
    launch.instance = record.instance;
    launch.target = record.target;
    menu::close();
    character_spawn::prepare_transition();
    menu::set_interaction_locked(true);
    menu::set_status("Mission launch queued; cleaning up character lineup");
    return true;
}

void mission_action(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    void* manager = mission_manager();
    if (manager == nullptr || !engine::gameplay_rel_ready() || entry.id >= mission_record_count) {
        menu::set_status("Mission manager changed; reopen Missions");
        return;
    }
    const MissionRecord& record = mission_records[entry.id];
    if (record.generation != static_cast<u32>(entry.minimum) ||
        !mission_record_is_current(manager, record) ||
        !resource_memory::readable_object(record.source_condition, mission_condition_size) ||
        !resource_memory::readable_object(record.source_instance, mission_instance_size) ||
        load_pointer(record.source_condition, 0x18u) != record.name ||
        (((load_u32(record.source_instance, 0x54u) & 8u) != 0u)
             ? load_pointer(record.source_instance, 0x18u) : nullptr) != record.script_data ||
        !same_bounded_text(record.name, record.name_copy, sizeof(record.name_copy)) ||
        !mission_pack_exists(record.name_copy) ||
        record.target != requested_hero(record.name_copy, load_u32(record.source_condition, 0x28u)) ||
        (record.script_data != nullptr &&
         !same_bounded_text(record.script_data, record.script_data_copy, sizeof(record.script_data_copy)))) {
        menu::set_status("Mission data is no longer valid");
        return;
    }
    request_mission(record, manager);
}

void unload_mission(menu::Entry&, menu::Command command) {
    if (command != menu::Command::activate) return;
    void* manager = mission_manager();
    void* world = engine::world();
    void* game = engine::game();
    if (readable_object(game, 0x1ACu) && load_u32(game, 0x1A8u) != 0u) {
        menu::set_status("Enable Physics before unloading missions");
        return;
    }
    if (manager == nullptr || !engine::gameplay_rel_ready() || busy() || hero_select::busy() ||
        !readable_object(world, 0x228u) || !readable_object(game, 0x1B4u) ||
        load_u32(world, 0x224u) != 1u || load_u32(game, 0x1A0u) == 0u ||
        load_u32(game, 0x5Cu) == 0u ||
        load_u32(manager, 0xCCu) != 0u || static_cast<s32>(load_u32(manager, 0xF0u)) != -1) {
        menu::set_status("Mission manager is not ready for unload");
        return;
    }
    launch = {};
    launch.phase = LaunchPhase::queued;
    launch.manager = manager;
    launch.world = world;
    launch.game = game;
    launch.unload_only = true;
    menu::close();
    character_spawn::prepare_transition();
    menu::set_interaction_locked(true);
    menu::set_status("Mission unload queued; cleaning up character lineup");
}

void add_read_only(menu::Menu& destination, const char* label) {
    menu::add(destination, label, menu::EntryType::read_only);
}

bool add_mission_table(void* table, s32 district, u8 table_index) {
    if (!resource_memory::readable_object(table, 0x48u)) {
        return false;
    }
    void* conditions = load_pointer(table, 0x38u);
    const u32 condition_count = load_u16(table, 0x3Cu);
    if (condition_count > maximum_conditions_per_table ||
        (condition_count != 0u &&
         !resource_memory::readable_object(conditions, condition_count * mission_condition_size))) {
        return false;
    }

    for (u32 condition_index = 0u; condition_index < condition_count; ++condition_index) {
        const u8* condition = static_cast<const u8*>(conditions) +
                              condition_index * mission_condition_size;
        const char* mission = static_cast<const char*>(load_pointer(condition, 0x18u));
        void* instances = load_pointer(condition, 0x08u);
        const u32 instance_count = load_u16(condition, 0x0Cu);
        if (!bounded_string(mission, 32u) ||
            instance_count > maximum_instances_per_condition ||
            (instance_count != 0u &&
             !resource_memory::readable_object(instances, instance_count * mission_instance_size)) ||
            !mission_pack_exists(mission)) {
            continue;
        }

        for (u32 instance_index = 0u; instance_index < instance_count; ++instance_index) {
            const u8* instance = static_cast<const u8*>(instances) +
                                 instance_index * mission_instance_size;
            const u32 instance_flags = load_u32(instance, 0x54u);
            const char* script_data = nullptr;
            if ((instance_flags & 8u) != 0u) {
                script_data = static_cast<const char*>(load_pointer(instance, 0x18u));
                if (!bounded_string(script_data, 32u, true)) {
                    continue;
                }
            }

            bool duplicate = false;
            for (u32 n = 0u; n < mission_record_count; ++n) {
                const MissionRecord& existing = mission_records[n];
                if (existing.district == district && existing.instance == static_cast<s32>(instance_index) &&
                    equal_text(existing.name_copy, mission) &&
                    (existing.script_data != nullptr) == (script_data != nullptr) &&
                    (script_data == nullptr || equal_text(existing.script_data_copy, script_data))) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) continue;
            if (mission_record_count >= maximum_mission_records) {
                mission_catalogue_truncated = true;
                return true;
            }

            const u32 record_index = mission_record_count++;
            MissionRecord& record = mission_records[record_index];
            record.name = mission;
            record.script_data = script_data;
            menu::copy_text(record.name_copy, sizeof(record.name_copy), mission);
            menu::copy_text(record.script_data_copy, sizeof(record.script_data_copy), script_data);
            record.source_condition = condition;
            record.source_instance = instance;
            record.source_table = table;
            record.condition_index = condition_index;
            record.generation = mission_cache_generation;
            record.district = district;
            record.instance = static_cast<s32>(instance_index);
            build_mission_label(mission_labels[record_index],
                                sizeof(mission_labels[record_index]),
                                mission,
                                script_data,
                                record.instance);
            record.target = requested_hero(mission, load_u32(condition, 0x28u));
            record.table_index = table_index;
            record.cinematic = cinematic_text(mission) || cinematic_text(script_data);
        }
    }
    return true;
}

u32 mission_table_signature(void* table) {
    if (!resource_memory::readable_object(table, 0x48u)) {
        return reinterpret_cast<uptr>(table);
    }
    void* conditions = load_pointer(table, 0x38u);
    const u32 condition_count = load_u16(table, 0x3Cu);
    u32 signature = reinterpret_cast<uptr>(table) ^
                    reinterpret_cast<uptr>(conditions) ^ (condition_count << 16u);
    void* region = load_pointer(table, 0x44u);
    signature ^= reinterpret_cast<uptr>(region);
    if (resource_memory::readable_object(region, 0xC4u)) {
        signature ^= load_u32(region, 0xC0u);
    }
    if (condition_count > maximum_conditions_per_table ||
        (condition_count != 0u &&
         !resource_memory::readable_object(conditions, condition_count * mission_condition_size))) {
        return signature ^ 0xBAD00000u;
    }
    for (u32 index = 0u; index < condition_count; ++index) {
        const u8* condition = static_cast<const u8*>(conditions) +
                              index * mission_condition_size;
        signature = (signature << 5u) | (signature >> 27u);
        signature ^= reinterpret_cast<uptr>(load_pointer(condition, 0x18u));
        signature ^= text_signature(
            static_cast<const char*>(load_pointer(condition, 0x18u)), 32u);
        signature ^= reinterpret_cast<uptr>(load_pointer(condition, 0x08u));
        signature ^= static_cast<u32>(load_u16(condition, 0x0Cu)) << 8u;
        signature ^= load_u32(condition, 0x28u);
        void* instances = load_pointer(condition, 0x08u);
        const u32 instance_count = load_u16(condition, 0x0Cu);
        if (instance_count <= maximum_instances_per_condition &&
            resource_memory::readable_object(instances, instance_count * mission_instance_size)) {
            for (u32 instance_index = 0u; instance_index < instance_count; ++instance_index) {
                const u8* instance = static_cast<const u8*>(instances) +
                                     instance_index * mission_instance_size;
                signature = (signature << 5u) | (signature >> 27u);
                signature ^= load_u32(instance, 0x54u);
                signature ^= reinterpret_cast<uptr>(load_pointer(instance, 0x18u));
                if ((load_u32(instance, 0x54u) & 8u) != 0u) {
                    signature ^= text_signature(
                        static_cast<const char*>(load_pointer(instance, 0x18u)), 32u);
                }
            }
        }
    }
    return signature;
}

u32 mission_signature(void* manager) {
    if (manager == nullptr) {
        return 0u;
    }
    const u32 district_count = load_u32(manager, 0x38u);
    u32 signature = reinterpret_cast<uptr>(manager) ^ district_count;
    signature = (signature << 5u) | (signature >> 27u);
    void* global_table = load_pointer(manager, 0x14u);
    signature ^= mission_table_signature(global_table);
    if (district_count <= maximum_district_tables) {
        for (u32 index = 0u; index < district_count; ++index) {
            void* table = load_pointer(manager, 0x18u + index * 4u);
            signature = (signature << 5u) | (signature >> 27u);
            signature ^= mission_table_signature(table);
        }
    }
    s32 hero_type = 0;
    if (current_hero_type(hero_type)) {
        signature ^= static_cast<u32>(hero_type) * 0x9E3779B9u;
    }
    return signature;
}

bool get_level_descriptors(void*& data, s32& size) {
    data = nullptr;
    size = 0;
    if (!resource_system_ready()) {
        return false;
    }

    void* partition = reinterpret_cast<GetPartitionFn>(resource_get_partition)(0);
    if (!readable_object(partition, 0x10u)) {
        return false;
    }

    // The GC look_up_level_descriptor routine at 0x80143008 loads partition+8,
    // then dereferences that pointer to obtain the first (game) pack slot.
    // This differs from the STL-vector-shaped PC declaration.
    void* first_address = load_pointer(partition, 0x08u);
    const uptr first = reinterpret_cast<uptr>(first_address);
    const uptr last = reinterpret_cast<uptr>(load_pointer(partition, 0x0Cu));
    if (last <= first || (last - first) % sizeof(void*) != 0u ||
        (last - first) / sizeof(void*) > resource_memory::detail::maximum_slots ||
        !readable_object(first_address, last - first)) {
        return false;
    }
    void* slot = *static_cast<void**>(first_address);
    if (!readable_object(slot, 0x20u) || load_u32(slot, 0x08u) != 4u) {
        return false;
    }

    // The GC accessor at801961F4 reads slot+18. The directory's backreference
    // at+78 must identify this READY slot before the native lookup runs.
    void* directory = load_pointer(slot, 0x18u);
    if (!readable_object(directory, 0x8Cu) || load_pointer(directory, 0x78u) != slot) {
        return false;
    }

    ResourceKey key = make_resource_key("level", 9);
    void* source_slot = nullptr;
    data = reinterpret_cast<GetResourceFn>(resource_directory_get)(
        directory, &key, &size, &source_slot);
    if (size <= 0 || static_cast<u32>(size) % level_descriptor_size != 0u ||
        !resource_memory::readable_object(data, static_cast<u32>(size))) {
        data = nullptr;
        size = 0;
        return false;
    }
    return true;
}

bool find_level_descriptor(void* data, s32 size, const char* name, const void*& result) {
    result = nullptr;
    if (data == nullptr || size <= 0 || static_cast<u32>(size) % level_descriptor_size != 0u ||
        !resource_memory::readable_object(data, static_cast<u32>(size))) return false;
    const u32 count = static_cast<u32>(size) / level_descriptor_size;
    if (count > maximum_level_descriptors) return false;
    for (u32 i = 0u; i < count; ++i) {
        const u8* descriptor = static_cast<const u8*>(data) + i * level_descriptor_size;
        const char* internal_name = reinterpret_cast<const char*>(descriptor);
        if (bounded_string(internal_name, 32u) && equal_text(internal_name, name)) {
            result = descriptor;
            return true;
        }
    }
    return false;
}

void load_scene(const char* name) {
    if (!engine::gameplay_rel_ready() || busy() || hero_select::busy()) {
        menu::set_status("Wait for gameplay before loading a level");
        return;
    }
    void* data = nullptr;
    s32 size = 0;
    const void* descriptor = nullptr;
    if (!get_level_descriptors(data, size) || !find_level_descriptor(data, size, name, descriptor)) {
        menu::set_status("Selected scene has no authored level descriptor");
        return;
    }
    if (!pack_exists(name)) {
        menu::set_status("Selected scene pack is not present on this disc");
        return;
    }
    if (!character_spawn::prepare_transition()) {
        pending_level = {};
        pending_level.queued = true;
        pending_level.world = engine::world();
        pending_level.game = engine::game();
        menu::copy_text(pending_level.name, sizeof(pending_level.name), name);
        menu::close();
        menu::set_interaction_locked(true);
        menu::set_status("Level load queued; cleaning up character lineup");
        return;
    }
    if (equal_text(name, "city_arena")) {
        // The persistence controller keeps the menu available on failure and
        // resets only after the companion confirms the INI has been saved.
        scene_config::select_scene_and_reboot(name);
        return;
    }
    if (equal_text(name, "characterb_arena") && !scene_config::select_scene(name)) return;
    engine::MString level_name{};
    level_name.size = static_cast<s32>(strlen(name));
    level_name.guts = name;
    reinterpret_cast<LoadLevelFn>(game_load_new_level)(engine::game(), &level_name);
    menu::close();
}

void level_action(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (entry.id >= level_record_count) {
        menu::set_status("Level descriptor changed; reopen Level Select");
        return;
    }
    const LevelRecord& record = level_records[entry.id];
    void* data = nullptr;
    s32 size = 0;
    const void* current_descriptor = nullptr;
    if (record.generation != static_cast<u32>(entry.minimum) ||
        !get_level_descriptors(data, size) || data != level_cache_data || size != level_cache_size ||
        !find_level_descriptor(data, size, record.internal_name_copy, current_descriptor) ||
        current_descriptor != record.source_descriptor ||
        !same_bounded_text(record.internal_name, record.internal_name_copy,
                           sizeof(record.internal_name_copy))) {
        menu::set_status("Level descriptor is no longer valid");
        return;
    }
    load_scene(record.internal_name_copy);
}

void alias_level_action(menu::Entry& entry, menu::Command command) {
    if (command == menu::Command::activate && entry.id < 2u)
        load_scene(entry.id == 0u ? "city_arena" : "characterb_arena");
}

void persistence_formatter(const menu::Entry&, char* output, u32 capacity) {
    scene_config::format_persistence(output, capacity);
}

void reboot_action(menu::Entry&, menu::Command command) {
    if (command == menu::Command::activate) {
        engine::reboot_game();
    }
}

void add_level_aliases(menu::Menu& destination) {
    const char* const labels[2] = {"city", "characterb"};
    for (u32 i = 0u; i < 2u; ++i) {
        menu::Entry* entry = menu::add(destination, labels[i], menu::EntryType::action);
        if (entry != nullptr) { entry->id = i; entry->handler = alias_level_action; }
    }
    menu::Entry* persistence = menu::add(destination, "game.ini SCENE_NAME", menu::EntryType::read_only);
    if (persistence != nullptr) persistence->formatter = persistence_formatter;
}

void add_level_fixed_entries(menu::Menu& destination) {
    menu::Entry* reboot = menu::add(destination, "-- REBOOT --", menu::EntryType::action);
    if (reboot != nullptr) reboot->handler = reboot_action;
    if (level_hero_menu != nullptr) {
        menu::add_submenu(destination, *level_hero_menu);
    }
}

u32 group_index(Hero target) {
    return target == Hero::venom ? 0u : target == Hero::spiderman ? 1u : 2u;
}

void add_mission_link(menu::Menu& destination, u32 index) {
    menu::Entry* entry = menu::add(destination, mission_labels[index], menu::EntryType::action);
    if (entry != nullptr) {
        entry->id = index;
        entry->minimum = static_cast<s32>(mission_cache_generation);
        entry->handler = mission_action;
    }
}

void refresh_mission_folder(menu::Menu& destination) {
    // A single bounded leaf is reused for every district. Navigation updates
    // its parent, so the selected parent row identifies the requested table.
    if (mission_root == nullptr) return;
    refresh_missions(*mission_root);
    menu::Menu* parent = destination.parent;
    if (parent == nullptr || parent->selected >= parent->count) return;
    u32 group = 3u;
    for (u32 i = 0u; i < 3u; ++i) if (parent == mission_groups[i]) group = i;
    const u32 table = parent->entries[parent->selected].id;
    if (group == leaf_group && table == leaf_table && leaf_generation == mission_cache_generation &&
        destination.count != 0u) return;
    leaf_group = group;
    leaf_table = table;
    leaf_generation = mission_cache_generation;
    menu::clear_entries(destination);
    if (group >= 3u || table >= mission_table_count) {
        add_read_only(destination, "Mission folder is no longer available");
        return;
    }
    destination.title = district_labels[table];
    for (u32 i = 0u; i < mission_record_count; ++i)
        if (group_index(mission_records[i].target) == group && mission_records[i].table_index == table)
            add_mission_link(destination, i);
    if (mission_catalogue_truncated)
        add_read_only(destination, "Catalogue limit reached; further entries omitted");
    if (destination.count == 0u) add_read_only(destination, "No mission packs loaded in this district");
}

void rebuild_mission_groups() {
    for (u32 group = 0u; group < 4u; ++group) {
        if (mission_groups[group] == nullptr) continue;
        menu::clear_entries(*mission_groups[group]);
        if (group == 3u) {
            for (u32 i = 0u; i < mission_record_count; ++i)
                if (mission_records[i].cinematic) add_mission_link(*mission_groups[group], i);
            if (mission_catalogue_truncated)
                add_read_only(*mission_groups[group], "Catalogue limit reached; further entries omitted");
        } else {
            for (u32 table = 0u; table < mission_table_count; ++table) {
                bool present = false;
                for (u32 i = 0u; i < mission_record_count; ++i)
                    if (group_index(mission_records[i].target) == group &&
                        mission_records[i].table_index == table) { present = true; break; }
                if (!present || mission_leaf == nullptr) continue;
                menu::Entry* entry = menu::add_submenu(*mission_groups[group], *mission_leaf);
                if (entry != nullptr) { entry->label = district_labels[table]; entry->id = table; }
            }
        }
        if (mission_groups[group]->count == 0u)
            add_read_only(*mission_groups[group], "No authored mission packs loaded");
    }
}

void refresh_mission_group(menu::Menu&) {
    if (mission_root != nullptr) refresh_missions(*mission_root);
}

}  // namespace

void attach_missions(menu::Menu& destination) {
    menu::clear_entries(destination);
    destination.refresh = refresh_missions;
    mission_root = &destination;
    const char* const titles[4] = {"Venom missions", "Spider-Man missions", "Other missions / Races",
                                   "IGC / Cutscenes (authored variants)"};
    for (u32 i = 0u; i < 4u; ++i)
        mission_groups[i] = menu::create(titles[i], i == 3u ? 513u : 10u, refresh_mission_group);
    mission_leaf = menu::create("Mission district", 513u, refresh_mission_folder);
    leaf_group = 4u;
    leaf_table = mission_table_count;
    mission_cache_signature = 0xFFFFFFFFu;
    refresh_missions(destination);
}

void attach_level_select(menu::Menu& destination, menu::Menu& hero_select) {
    menu::clear_entries(destination);
    destination.refresh = refresh_level_select;
    level_hero_menu = &hero_select;
    level_cache_data = nullptr;
    level_cache_size = -1;
    refresh_level_select(destination);
}

void refresh_missions(menu::Menu& destination) {
    void* manager = mission_manager();
    const bool ready = manager != nullptr && engine::gameplay_rel_ready();
    const u32 signature = mission_signature(manager) ^
                          (ready ? 0xA511E9B3u : 0u);
    if (signature == mission_cache_signature && destination.count != 0u) {
        return;
    }

    mission_cache_signature = signature;
    mission_cache_manager = manager;
    ++mission_cache_generation;
    mission_record_count = 0u;
    mission_catalogue_truncated = false;
    menu::clear_entries(destination);
    if (!ready) {
        add_read_only(destination, "Mission manager is not ready");
        return;
    }

    menu::Entry* unload = menu::add(destination,
                                    "UNLOAD CURRENT MISSION",
                                    menu::EntryType::action);
    if (unload != nullptr) {
        unload->handler = unload_mission;
    }

    menu::copy_text(district_labels[0], sizeof(district_labels[0]), "global");
    add_mission_table(load_pointer(manager, 0x14u), 0, 0u);
    const u32 district_count = load_u32(manager, 0x38u);
    if (district_count > maximum_district_tables) {
        add_read_only(destination, "Invalid district mission-table count");
        rebuild_mission_groups();
        return;
    }
    for (u32 index = 0u; index < district_count && !mission_catalogue_truncated; ++index) {
        void* table = load_pointer(manager, 0x18u + index * 4u);
        if (!resource_memory::readable_object(table, 0x48u)) continue;
        void* region = load_pointer(table, 0x44u);
        if (!resource_memory::readable_object(region, 0xC4u)) continue;
        const s32 district = static_cast<s32>(load_u32(region, 0xC0u));
        const char* label = engine::region_name(region);
        if (label != nullptr && bounded_string(label, 64u))
            menu::copy_text(district_labels[index + 1u], sizeof(district_labels[0]), label);
        else {
            menu::copy_text(district_labels[index + 1u], sizeof(district_labels[0]), "District ");
            append_index(district_labels[index + 1u], sizeof(district_labels[0]), district);
        }
        add_mission_table(table, district, static_cast<u8>(index + 1u));
    }
    rebuild_mission_groups();
    for (menu::Menu* group : mission_groups) if (group != nullptr) menu::add_submenu(destination, *group);
    if (mission_leaf == nullptr || mission_groups[0] == nullptr || mission_groups[1] == nullptr ||
        mission_groups[2] == nullptr || mission_groups[3] == nullptr)
        add_read_only(destination, "Insufficient menu storage for mission folders");
    if (mission_catalogue_truncated)
        add_read_only(destination, "Mission catalogue limit reached; further entries omitted");
    if (mission_record_count == 0u)
        add_read_only(destination, "No authored mission packs loaded");
}

void refresh_level_select(menu::Menu& destination) {
    void* data = nullptr;
    s32 size = 0;
    if (!get_level_descriptors(data, size)) {
        if (level_cache_size != -1 || destination.count == 0u) {
            level_cache_data = nullptr;
            level_cache_size = -1;
            level_record_count = 0u;
            menu::clear_entries(destination);
            add_level_aliases(destination);
            add_read_only(destination, "Level descriptor table is not ready");
            add_level_fixed_entries(destination);
        }
        return;
    }
    u32 descriptor_count = static_cast<u32>(size) / level_descriptor_size;
    if (descriptor_count > maximum_level_descriptors) {
        descriptor_count = maximum_level_descriptors;
    }
    const u8* descriptors = static_cast<const u8*>(data);
    u32 signature = static_cast<u32>(size);
    for (u32 index = 0u; index < descriptor_count; ++index) {
        const u8* descriptor = descriptors + index * level_descriptor_size;
        signature = (signature << 5u) | (signature >> 27u);
        signature ^= text_signature(reinterpret_cast<const char*>(descriptor), 32u);
        signature ^= text_signature(reinterpret_cast<const char*>(descriptor + 0x60u), 16u);
    }
    if (data == level_cache_data && size == level_cache_size && signature == level_cache_signature &&
        destination.count != 0u) {
        return;
    }

    level_cache_data = data;
    level_cache_size = size;
    level_cache_signature = signature;
    ++level_cache_generation;
    level_record_count = 0u;
    menu::clear_entries(destination);

    add_level_aliases(destination);
    // Keep room for the manual reboot marker, Hero Select and a truncation
    // notice after any additional authored levels.
    const u32 fixed_entries = level_hero_menu != nullptr ? 2u : 1u;
    bool catalogue_truncated = static_cast<u32>(size) / level_descriptor_size >
                               maximum_level_descriptors;
    for (u32 index = 0u; index < descriptor_count; ++index) {
        const u8* descriptor = descriptors + index * level_descriptor_size;
        const char* internal_name = reinterpret_cast<const char*>(descriptor);
        const char* display_name = reinterpret_cast<const char*>(descriptor + 0x60u);
        if (!bounded_string(internal_name, 32u) ||
            equal_text(internal_name, "city_arena") || equal_text(internal_name, "characterb_arena") ||
            !bounded_string(display_name, 16u) ||
            !pack_exists(internal_name)) {
            continue;
        }
        if (level_record_count >= maximum_level_records ||
            destination.count + fixed_entries + 1u >= destination.capacity) {
            catalogue_truncated = true;
            break;
        }
        const u32 record_index = level_record_count++;
        level_records[record_index].internal_name = internal_name;
        level_records[record_index].display_name = display_name;
        menu::copy_text(level_records[record_index].internal_name_copy,
                        sizeof(level_records[record_index].internal_name_copy), internal_name);
        level_records[record_index].source_descriptor = descriptor;
        level_records[record_index].generation = level_cache_generation;
        menu::copy_text(level_labels[record_index], sizeof(level_labels[record_index]), display_name);
        menu::Entry* entry = menu::add(destination,
                                       level_labels[record_index],
                                       menu::EntryType::action);
        if (entry != nullptr) {
            entry->id = record_index;
            entry->minimum = static_cast<s32>(level_cache_generation);
            entry->handler = level_action;
        }
    }
    if (level_record_count == 0u && destination.count + fixed_entries < destination.capacity) {
        // City/Characterb already have explicit rows above. The retail disc
        // commonly contains only City's pack, so zero extra rows is valid.
        add_read_only(destination, "No additional level packs found");
    }
    if (catalogue_truncated && destination.count + fixed_entries < destination.capacity) {
        add_read_only(destination, "Level catalogue limit reached; further entries omitted");
    }
    add_level_fixed_entries(destination);
}

bool busy() { return launch.phase != LaunchPhase::idle || pending_level.queued; }

void frame_advance() {
    if (pending_level.queued) {
        if (pending_level.world != engine::world() || pending_level.game != engine::game()) {
            pending_level = {};
            menu::set_interaction_locked(false);
            menu::set_status("Level load cancelled after world replacement");
            return;
        }
        if (engine::game_state() != 6 || engine::game_paused()) return;
        if (++pending_level.ticks >= 7200u) {
            pending_level = {};
            menu::set_interaction_locked(false);
            menu::set_status("Level load cancelled; character packs are still owned");
            return;
        }
        if (!character_spawn::prepare_transition()) return;
        char name[32];
        menu::copy_text(name, sizeof(name), pending_level.name);
        pending_level = {};
        menu::set_interaction_locked(false);
        load_scene(name);
        return;
    }
    if (launch.phase == LaunchPhase::idle) return;
    if (!launch_owner_live()) { finish_launch("Mission launch cancelled after world replacement"); return; }
    void* manager = launch.manager;
    const s32 switch_stage = static_cast<s32>(load_u32(manager, 0xF0u));
    if (launch.cancelled && (launch.phase != LaunchPhase::switching || switch_stage == -1)) {
        finish_launch("Mission launch timed out"); return;
    }
    if (engine::game_state() != 6 || engine::game_paused() ||
        !readable_object(launch.game, 0x1B4u) || load_u32(launch.game, 0x1A0u) == 0u) return;
    if (++launch.ticks >= 7200u) { launch.cancelled = true; return; }
    const bool busy = packs_busy();
    if (launch.phase == LaunchPhase::queued) {
        if (!character_spawn::prepare_transition()) return;
        menu::set_interaction_locked(true);
        if (busy || load_pointer(manager, 0x3Cu) != nullptr || switch_stage != -1) return;
        if (load_u32(manager, 0x7Cu) != 0u || load_u32(manager, 0xCCu) != 0u) {
            finish_launch("Mission manager claimed by another request"); return;
        }
        store_u32(manager, 0xCCu, 1u);
        launch.owns_lock = true;
        reinterpret_cast<PrepareUnloadFn>(mission_prepare_unload)(manager);
        launch.phase = LaunchPhase::unloading;
        return;
    }
    if (launch.phase == LaunchPhase::unloading) {
        if (load_pointer(manager, 0x40u) != nullptr || load_pointer(manager, 0x3Cu) != nullptr ||
            load_u32(manager, 0x4Cu) != 0u || busy || switch_stage != -1) return;
        if (launch.unload_only) { finish_launch("Current mission unloaded"); return; }
        if (!hero_matches(Hero::current)) { finish_launch("Mission launch requires one live hero"); return; }
        if (!hero_matches(launch.target) && launch.target != Hero::current) {
            if (!pack_exists(hero_name(launch.target)) ||
                !transition_cameras_ready(launch.game, launch.world)) {
                finish_launch("Hero pack or cameras are unavailable"); return;
            }
            if (load_u32(launch.world, 0x70u) == 0u) {
                reinterpret_cast<void (*)(void*, bool, bool, float, float)>(game_enable_marky_camera)(
                    launch.game, true, true, 0.0f, 0.0f);
                launch.owns_camera = true;
            }
            menu::copy_text(reinterpret_cast<char*>(static_cast<u8*>(manager) + 0xD0u), 32u,
                            hero_name(launch.target));
            store_u32(manager, 0xF0u, 0u);
            launch.phase = LaunchPhase::switching;
            return;
        }
    } else if (launch.phase == LaunchPhase::switching) {
        if (switch_stage != -1 || busy) return;
        if (!hero_matches(launch.target)) { finish_launch("Native switch did not produce the requested hero"); return; }
    }
    if (!hero_matches(launch.target) || busy) return;
    reinterpret_cast<ForceMissionFn>(mission_force)(manager, launch.district, launch.name,
                                                  launch.instance, launch.has_data ? launch.data : nullptr);
    finish_launch("Mission dispatched with matching hero");
}

}  // namespace usm::gc::mission_level
