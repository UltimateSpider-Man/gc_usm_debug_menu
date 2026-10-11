#include "character_spawn_gc.h"

#include "../gc_engine.h"
#include "../resource_memory_gc.h"

namespace usm::gc::character_spawn {
namespace {

// Verified against main.original.dol, including native character_viewer
// push/pop at 0x7F0540D8/0x7F0542D0. Map labels have differing local drifts.
constexpr uptr stack_pointer = 0x8045F6F4u;
constexpr uptr resource_manager_pointer = 0x8045F214u;
constexpr uptr pack_table_pointer = 0x8045F220u;
constexpr uptr pack_table_count = 0x8045F224u;
constexpr uptr string_hash_set = 0x8015BF68u;
constexpr uptr get_pack_stats = 0x8015D730u;
constexpr uptr get_partition = 0x8015D968u;
constexpr uptr has_room = 0x8015DC80u;
constexpr uptr stack_waiting = 0x801DCB9Cu;
constexpr uptr stack_has_pack = 0x801DCA88u;
constexpr uptr stack_push = 0x801DC9F8u;
constexpr uptr stack_pop = 0x801DCA44u;
constexpr uptr directory_get = 0x8015FF5Cu;
constexpr uptr context_push = 0x8015CCE8u;
constexpr uptr context_pop = 0x8015CD78u;
constexpr uptr create_entity = 0x801CE6ECu;
constexpr uptr destroy_entity = 0x801CF224u;
constexpr uptr unique_entity_id = 0x800F9F88u;
constexpr uptr resolve_handle = 0x80218FD0u;
constexpr uptr absolute_pose = 0x801354C4u;
constexpr uptr set_visible = 0x80101318u;
constexpr uptr set_damage_attribute = 0x800E50E4u;
constexpr uptr heap_bytes_free = 0x8018D448u;
constexpr uptr sphere_intersection = 0x801D621Cu;
constexpr uptr accept_all_filter_pointer = 0x8046269Cu;
constexpr uptr sphere_filter_pointer = 0x804626C4u;
constexpr u32 minimum_heap_free = 1024u * 1024u;
constexpr u32 queue_capacity = 128u;
constexpr u32 live_capacity = character_count;
constexpr u32 maximum_slots = 256u;
constexpr u16 spawn_delay = 12u;

const char* const names[character_count] = {
    "alex_ohirn", "alex_ohirn_prison", "arachno_man", "beetle",
    "bolivar_trask_old", "boomerang", "carnage", "eddie_brock",
    "eddie_brock_sr", "electro_naked", "electro_nosuit", "electro_suit",
    "gang_ftb_base", "gang_ftb_boss", "gang_ftb_lt", "gang_mercs_base",
    "gang_mercs_boss", "gang_mercs_lt", "gang_skin_base", "gang_skin_boss",
    "gang_skin_lt", "gang_srk_base", "gang_srk_boss", "gang_srk_lt",
    "gmu_businessman", "gmu_child_male", "gmu_cop_fat", "gmu_cop_thin",
    "gmu_cop_thin_igc", "gmu_jogger_fem", "gmu_lab_fem", "gmu_man",
    "gmu_medic_fem", "gmu_sable_merc", "gmu_storeowner", "gmu_street_vendor",
    "gmu_student_female", "gmu_student_male", "gmu_utilityworker_male", "gmu_woman",
    "johnny_storm", "kong", "liz_allen", "mary_jane", "nick_fury",
    "ped_fem", "ped_male", "peter_hooded", "peter_shirtless", "rhino",
    "rhino_igc", "richard_parker", "sable", "sharon_carter", "shield_agent",
    "shield_agent_jetpack", "shield_agent_test", "shocker", "toomes",
    "usm_wrestling", "venom_eddie", "venom_spider_viewer", "boss_venom_spider",
    "venom_viewer", "wolverine", "gang_skin_boss_fem",
};
static_assert(sizeof(names) / sizeof(names[0]) == character_count);

struct ResourceKey { u32 hash; s32 type; };
struct PackLocation { u32 words[20]; };
struct ActorRecord { u32 handle; u32 name; s32 owned_pack; };
struct PackRecord { void* slot; u32 hash; u32 name; u32 references; s32 alias; bool used; };
struct AssetAlias { u32 name; const char* pack; const char* entity; };
// These PC mappings were also checked against type-4 resource locations in
// the extracted GC pack directories. The resolved pair is visible in the UI.
const AssetAlias aliases[] = {
    {1u, "ch_vwr_alex_ohirn", "alex_ohirn"},
    {3u, "ch_vwr_beetle_viewer", "beetle_viewer"},
    {9u, "ch_vwr_electro_nosuit", "electro_nosuit"},
    {24u, "city_gmu_businessman", "gmu_businessman"},
    {25u, "city_gmu_child_male", "gmu_child_male"},
    {27u, "city_thin_cop", "gmu_cop_thin"},
    {28u, "s02_igc4_pack", "gmu_cop_thin_igc_lite"},
    {29u, "city_gmu_jogger_fem", "gmu_jogger_fem"},
    {30u, "pk_v12_venvscar", "gmu_lab_fem_v09"},
    {31u, "city_gmu_man", "gmu_man_ct"},
    {32u, "city_gmu_medic_fem", "gmu_medic_fem"},
    {34u, "city_gmu_storeowner", "gmu_storeowner"},
    {35u, "city_gmu_street_vender", "gmu_street_vendor"},
    {36u, "city_gmu_student_female", "gmu_student_female"},
    {37u, "city_gmu_student_male", "gmu_student_male"},
    {38u, "city_gmu_utility_worker_male", "gmu_utilityworker_male"},
    {39u, "city_gmu_woman", "gmu_woman_ct"},
    {45u, "city_arena", "ped_fem"},
    {46u, "city_arena", "ped_male"},
    {47u, "ch_vwr_peter_hooded_viewer", "peter_hooded_viewer"},
    {48u, "ch_vwr_peter_parker_viewer", "peter_parker_viewer"},
    {50u, "s04_rhino_igc3", "rhino_igc"},
    {55u, "hot_pursuit_sable_pack", "shield_agent_jetpack"},
    {56u, "ch_vwr_shield_agent", "shield_agent"},
    {60u, "pk_s13_ending_igc", "venom_eddie_lite"},
};
constexpr u32 alias_count = sizeof(aliases) / sizeof(aliases[0]);
enum class Result : u8 { unknown, queued, spawned, missing_pack, missing_class,
                         failed, capacity, heap, invalid };

ActorRecord actors[live_capacity];
PackRecord packs[character_count];
Result results[character_count];
s32 resolved_alias[character_count];
u32 actor_count;
u32 requests[queue_capacity];
u32 queue_head;
u32 queue_size;
u16 delay;
bool pop_requested;
bool cleanup_requested;
bool worker_active;
void* owning_world;
void* owning_terrain;

template <typename Function> Function native(uptr address) {
    return reinterpret_cast<Function>(address);
}
bool readable(const void* pointer, u32 size) {
    const uptr address = reinterpret_cast<uptr>(pointer);
    return (address & 3u) == 0u && address >= 0x80000000u &&
           address < 0x81800000u && size <= 0x81800000u - address;
}
u32 word(const void* object, u32 offset) {
    return *reinterpret_cast<volatile const u32*>(static_cast<const u8*>(object) + offset);
}
void* pointer(const void* object, u32 offset) {
    return reinterpret_cast<void*>(word(object, offset));
}
void pack_name(u32 index, char (&output)[64], s32 alias = -1) {
    if (alias >= 0 && static_cast<u32>(alias) < alias_count) {
        menu::copy_text(output, sizeof(output), aliases[alias].pack); return;
    }
    menu::copy_text(output, sizeof(output), "ch_vwr_");
    menu::append_text(output, sizeof(output), names[index]);
}
engine::MString borrowed(const char* text) {
    engine::MString value{};
    value.size = static_cast<s32>(strlen(text));
    value.guts = text;
    return value;
}
ResourceKey key(const char* text, s32 type) {
    ResourceKey value{};
    native<void (*)(u32*, const char*)>(string_hash_set)(&value.hash, text);
    value.type = type;
    return value;
}
bool resources_ready() {
    void* manager = memory<void*>(resource_manager_pointer);
    const u32 count = memory<u32>(pack_table_count);
    return readable(manager, 8u) && readable(pointer(manager, 0u), 8u * 4u) &&
           count != 0u && count <= 65536u &&
           readable(memory<void*>(pack_table_pointer), count * sizeof(PackLocation));
}
bool pack_available(u32 index, ResourceKey& pack_key, PackLocation* location = nullptr, s32* chosen = nullptr) {
    if (index >= character_count || !resources_ready()) return false;
    char name[64]; pack_name(index, name);
    pack_key = key(name, 25);
    auto get = native<bool (*)(const ResourceKey*, PackLocation*, void*, void*)>(get_pack_stats);
    if (get(&pack_key, location, nullptr, nullptr)) { if (chosen != nullptr) *chosen = -1; return true; }
    for (u32 i = 0u; i < alias_count; ++i) {
        if (aliases[i].name != index) continue;
        pack_key = key(aliases[i].pack, 25);
        if (get(&pack_key, location, nullptr, nullptr)) {
            if (chosen != nullptr) *chosen = static_cast<s32>(i);
            return true;
        }
    }
    return false;
}
void status(u32 index, const char* reason) {
    char text[160];
    menu::copy_text(text, sizeof(text), index < character_count ? names[index] : "Character lineup");
    menu::append_text(text, sizeof(text), ": ");
    menu::append_text(text, sizeof(text), reason);
    menu::set_status(text, 300u);
}
void* stack() {
    void* value = memory<void*>(stack_pointer);
    return readable(value, 0x14u) ? value : nullptr;
}
bool stack_busy(void* manager) {
    return native<bool (*)(void*)>(stack_waiting)(manager);
}
bool returning_from_native_pause() {
    // unpause() advances the native "pause" process from PAUSED to its final
    // POP_PROCESS state. The process is popped on the next game update, after
    // this worker runs. It is not a level teardown and owns no resource work.
    void* game = engine::game();
    if (engine::game_state() != 14 || !readable(game, 0x1B4u) ||
        word(game, 0x1A0u) == 0u || word(game, 0x1B0u) != 0u) return false;
    const uptr begin = word(game, 0x84u);
    const uptr end = word(game, 0x88u);
    if (end < begin || end - begin < 0x18u || end - begin > 64u * 0x18u ||
        (end - begin) % 0x18u != 0u ||
        !readable(reinterpret_cast<void*>(begin), end - begin)) return false;
    void* process = reinterpret_cast<void*>(end - 0x18u);
    if (word(process, 8u) != 1u || word(process, 12u) != 2u) return false;
    void* states = pointer(process, 4u);
    if (!readable(states, 8u) || word(states, 0u) != 7u || word(states, 4u) != 14u) return false;
    const char* name = static_cast<const char*>(pointer(process, 0u));
    return readable(name, 8u) && name[0] == 'p' && name[1] == 'a' &&
           name[2] == 'u' && name[3] == 's' && name[4] == 'e' && name[5] == '\0';
}
bool slots(void* partition, void**& begin, u32& count) {
    if (!readable(partition, 0xB4u)) return false;
    const uptr first = word(partition, 8u);
    const uptr end = word(partition, 12u);
    if (end < first || (end - first) % 4u != 0u) return false;
    count = (end - first) / 4u;
    if (count > maximum_slots || (count != 0u && !readable(reinterpret_cast<void*>(first), count * 4u))) return false;
    begin = reinterpret_cast<void**>(first);
    return true;
}
bool slot_matches(void* slot, u32 hash) {
    // GC slot's resource_key precedes its state; the vtable is at +0x90.
    return readable(slot, 0x94u) && word(slot, 0u) == hash && word(slot, 4u) == 25u;
}
void* find_slot(void* partition, u32 hash, bool ready_only = true) {
    void** begin; u32 count;
    if (!slots(partition, begin, count)) return nullptr;
    for (u32 i = 0u; i < count; ++i) {
        if (slot_matches(begin[i], hash) && (!ready_only || word(begin[i], 8u) == 4u)) return begin[i];
    }
    return nullptr;
}
void* resident_slot(u32 hash) {
    for (s32 partition = 0; partition < 8; ++partition) {
        if (void* found = find_slot(native<void* (*)(s32)>(get_partition)(partition), hash)) return found;
    }
    return nullptr;
}
s32 owned_pack(void* slot, u32 hash) {
    for (u32 i = 0u; i < character_count; ++i) {
        if (packs[i].used && packs[i].slot == slot && packs[i].hash == hash) return static_cast<s32>(i);
    }
    return -1;
}
bool exact_class(void* slot, const ResourceKey& pack_key, const char* entity_name) {
    if (!slot_matches(slot, pack_key.hash) || word(slot, 8u) != 4u) return false;
    void* directory = pointer(slot, 0x18u);
    if (!readable(directory, 4u)) return false;
    const ResourceKey entity_key = key(entity_name, 4);
    s32 size = 0; void* source_slot = nullptr;
    void* data = native<void* (*)(void*, const ResourceKey*, s32*, void**)>(directory_get)(
        directory, &entity_key, &size, &source_slot);
    // Reject same-named classes resolved through a different parent pack.
    return size > 0 && source_slot == slot &&
           resource_memory::readable_range(data, static_cast<u32>(size));
}
bool release_one_pack(void* manager, void* partition, bool retain_requested = false) {
    void** begin; u32 count;
    if (!slots(partition, begin, count)) return false;
    for (u32 i = 0u; i < character_count; ++i) {
        PackRecord& record = packs[i];
        if (!record.used || record.references != 0u) continue;
        if (retain_requested && queue_size != 0u && requests[queue_head] == record.name) continue;
        if (!slot_matches(record.slot, record.hash)) { record.used = false; continue; }
        if (count == 0u || begin[count - 1u] != record.slot || word(record.slot, 8u) != 4u) continue;
        char text[64]; pack_name(record.name, text, record.alias);
        auto name = borrowed(text);
        if (!native<bool (*)(void*, const engine::MString*)>(stack_has_pack)(manager, &name)) continue;
        native<void (*)(void*, const engine::MString*, const engine::MString*)>(stack_pop)(manager, &name, &name);
        record.used = false;
        return true;
    }
    return false;
}
void discard_record(u32 position) {
    const ActorRecord record = actors[position];
    for (u32 i = position + 1u; i < actor_count; ++i) actors[i - 1u] = actors[i];
    --actor_count;
    if (record.owned_pack >= 0) {
        PackRecord& pack = packs[record.owned_pack];
        if (pack.references != 0u) --pack.references;
    }
    results[record.name] = Result::unknown;
    for (u32 i = 0u; i < actor_count; ++i) if (actors[i].name == record.name) results[record.name] = Result::spawned;
}
void prune_dead_actors() {
    for (u32 i = actor_count; i != 0u; --i) {
        const ActorRecord& record = actors[i - 1u];
        void* actor = native<void* (*)(const u32*)>(resolve_handle)(&record.handle);
        if (!readable(actor, 0xA0u) || word(actor, 0x1Cu) != record.handle) discard_record(i - 1u);
    }
}
bool remove_last_actor() {
    if (actor_count == 0u) return true;
    const ActorRecord record = actors[actor_count - 1u];
    void* actor = native<void* (*)(const u32*)>(resolve_handle)(&record.handle);
    void* hero = readable(owning_world, 0x228u) ? pointer(owning_world, 0x21Cu) : nullptr;
    if (actor != nullptr && actor == hero) {
        status(record.name, "is now the hero; ownership retained until switched away"); return false;
    }
    if (readable(actor, 0xA0u) && word(actor, 0x1Cu) == record.handle) {
        native<void (*)(void*, void*)>(destroy_entity)(static_cast<u8*>(owning_world) + 0x74u, actor);
    }
    discard_record(actor_count - 1u); return true;
}
bool enough_heap() {
    const s32 free = native<s32 (*)(s32)>(heap_bytes_free)(0);
    return free > 0 && static_cast<u32>(free) >= minimum_heap_free;
}
bool make_placement(float (&pose)[16]) {
    void* hero = pointer(owning_world, 0x21Cu);
    if (!readable(hero, 0xA0u)) return false;
    const float* source = native<const float* (*)(void*)>(absolute_pose)(hero);
    if (!readable(source, sizeof(pose))) return false;
    for (u32 i = 0u; i < 16u; ++i) {
        if (!(source[i] >= -1000000.0f && source[i] <= 1000000.0f)) return false;
        pose[i] = source[i];
    }
    const s32 column = actor_count == 0u ? 0 : static_cast<s32>((actor_count + 1u) / 2u) * (actor_count % 2u == 0u ? 1 : -1);
    const float x = static_cast<float>(column % 7) * 1.5f;
    const float z = 2.0f + static_cast<float>(actor_count / 7u) * 2.0f;
    for (u32 i = 0u; i < 3u; ++i) pose[12u + i] += source[i] * x + source[8u + i] * z;
    // Match the PC SpawnX collision fallback. The native GC routine writes
    // both optional hit-owner outputs without a null check, so provide them.
    void* ent_filter = memory<void*>(accept_all_filter_pointer);
    void* obb_filter = memory<void*>(sphere_filter_pointer);
    if (readable(ent_filter, 4u) && readable(obb_filter, 4u)) {
        Vec3 proposed{pose[12], pose[13], pose[14]}, impact{}, normal{};
        void* entity_hit = nullptr; void* obb_hit = nullptr;
        using Intersect = bool (*)(const Vec3*, float, const void*, const void*, Vec3*, Vec3*, void**, void**);
        if (native<Intersect>(sphere_intersection)(&proposed, 0.5f, ent_filter, obb_filter,
                                                  &impact, &normal, &entity_hit, &obb_hit)) {
            pose[12] = source[12]; pose[13] = source[13] + 0.1f; pose[14] = source[14];
        }
    }
    return true;
}
bool spawn(u32 index, void* manager, void* partition) {
    ResourceKey pack_key{}; PackLocation location{}; s32 alias = -1;
    if (!pack_available(index, pack_key, &location, &alias)) {
        results[index] = Result::missing_pack; status(index, "ch_vwr_ pack and verified GC fallback absent"); return true;
    }
    resolved_alias[index] = alias;
    const char* entity_name = alias >= 0 ? aliases[alias].entity : names[index];
    if (actor_count >= live_capacity) { results[index] = Result::capacity; status(index, "lineup full; Pop Character to resume"); return false; }
    if (!enough_heap()) { results[index] = Result::heap; status(index, "heap reserve reached; queued, Pop to resume"); return false; }
    void* slot = resident_slot(pack_key.hash);
    s32 pack_index = slot != nullptr ? owned_pack(slot, pack_key.hash) : -1;
    bool newly_loaded = false;
    if (slot == nullptr) {
        const s32 size = static_cast<s32>(location.words[3]);
        const s32 excluded = static_cast<s32>(location.words[5]);
        if (size <= 0 || excluded < 0 || excluded >= size) {
            results[index] = Result::invalid; status(index, "invalid pack memory size"); return true;
        }
        const s32 reserved = size - excluded; // Same calculation as native push.
        if (!native<bool (*)(void*, s32)>(has_room)(partition, reserved)) {
            results[index] = Result::capacity; status(index, "mission stack full; queued, Pop to resume"); return false;
        }
        for (u32 i = 0u; i < character_count; ++i) if (!packs[i].used) { pack_index = static_cast<s32>(i); break; }
        if (pack_index < 0) { status(index, "pack ownership table full; Pop to resume"); return false; }
        char text[64]; pack_name(index, text, alias); auto name = borrowed(text);
        // Do not claim a pack whose native owner is already loading it.
        if (native<bool (*)(void*, const engine::MString*)>(stack_has_pack)(manager, &name)) return false;
        native<void (*)(void*, const engine::MString*, const engine::MString*)>(stack_push)(manager, &name, &name);
        slot = find_slot(partition, pack_key.hash, false);
        if (slot == nullptr) { results[index] = Result::failed; status(index, "native pack load failed"); return true; }
        packs[pack_index] = PackRecord{slot, pack_key.hash, index, 0u, alias, true};
        newly_loaded = true;
        if (word(slot, 8u) != 4u) { status(index, "native pack load still pending; queued"); return false; }
    }
    if (!exact_class(slot, pack_key, entity_name)) {
        results[index] = Result::missing_class; status(index, "required class absent from resolved pack");
        if (newly_loaded) release_one_pack(manager, partition);
        return true;
    }
    if (!enough_heap()) {
        results[index] = Result::heap; status(index, "heap reserve reached after pack load; queued");
        if (newly_loaded) release_one_pack(manager, partition);
        return false;
    }
    alignas(16) float pose[16];
    if (!make_placement(pose)) {
        results[index] = Result::invalid; status(index, "hero placement unavailable");
        if (newly_loaded) release_one_pack(manager, partition);
        return true;
    }
    u32 class_hash = key(entity_name, 4).hash;
    u32 instance_hash = 0u;
    // CodeWarrior returns string_hash via a hidden result pointer.
    native<void (*)(u32*)>(unique_entity_id)(&instance_hash);
    auto tag = borrowed("");
    native<void* (*)(void*)>(context_push)(slot);
    using Create = void* (*)(void*, const u32*, const u32*, const float*, const engine::MString*, u32, void*);
    void* actor = native<Create>(create_entity)(static_cast<u8*>(owning_world) + 0x74u,
                                               &class_hash, &instance_hash, pose, &tag, 1u, nullptr);
    native<void* (*)()>(context_pop)();
    if (!readable(actor, 0xA0u) || word(actor, 0x1Cu) == 0u) {
        results[index] = Result::failed; status(index, "native actor creation failed");
        if (readable(actor, 0xA0u) && actor != pointer(owning_world, 0x21Cu))
            native<void (*)(void*, void*)>(destroy_entity)(static_cast<u8*>(owning_world) + 0x74u, actor);
        if (newly_loaded) release_one_pack(manager, partition);
        return true;
    }
    native<void (*)(void*, bool, bool)>(set_visible)(actor, true, false);
    void* damage = pointer(actor, 0x68u);
    if (readable(damage, 0x20Cu) && pointer(damage, 4u) == actor) {
        // Native HIT_POINTS setter bounds the current word at +0x200;
        // its authored maximum is +0x208 (PC's packed offset differs).
        const float maximum = *reinterpret_cast<const float*>(static_cast<u8*>(damage) + 0x208u);
        if (maximum > 0.0f && maximum <= 100000000.0f) {
            const ResourceKey health = key("HIT_POINTS", 67);
            native<bool (*)(void*, const ResourceKey*, float, bool)>(set_damage_attribute)(damage, &health, maximum, false);
        }
    }
    actors[actor_count++] = ActorRecord{word(actor, 0x1Cu), index, pack_index};
    if (pack_index >= 0) ++packs[pack_index].references;
    results[index] = Result::spawned;
    char detail[128]; char resolved[64]; pack_name(index, resolved, alias);
    menu::copy_text(detail, sizeof(detail), "spawned: "); menu::append_text(detail, sizeof(detail), resolved);
    menu::append_text(detail, sizeof(detail), " / "); menu::append_text(detail, sizeof(detail), entity_name);
    status(index, detail);
    return true;
}
void forget_world();
void enqueue(u32 index) {
    if (index >= character_count) { status(index, "invalid character selection"); return; }
    // The idle worker can have observed the world before its terrain settled.
    // Bind at acceptance, so a fresh selection is not mistaken for an old
    // world's pending request on the following update. Old objects belong to
    // native teardown and must never be destroyed through their stale handles.
    void* current_world = engine::world();
    void* current_terrain = engine::terrain();
    if (owning_world != current_world || owning_terrain != current_terrain) forget_world();
    owning_world = current_world; owning_terrain = current_terrain;
    if (queue_size >= queue_capacity) { status(index, "spawn queue full"); return; }
    requests[(queue_head + queue_size++) % queue_capacity] = index;
    results[index] = Result::queued;
}
void row_handler(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (!engine::interactive_world_ready()) { status(entry.id, "requires a loaded gameplay world"); return; }
    enqueue(entry.id); menu::close();
}
void control_handler(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (!engine::interactive_world_ready()) { status(character_count, "requires a loaded gameplay world"); return; }
    if (entry.id == 0u) pop_requested = true;
    else if (entry.id == 1u) { cancel_pending(); cleanup_requested = true; pop_requested = false; }
    else {
        if (queue_size > queue_capacity - character_count) { status(character_count, "not enough room to queue all 66"); return; }
        for (u32 i = 0u; i < character_count; ++i) enqueue(i);
        status(character_count, "all 66 requested; unavailable assets are reported");
    }
    menu::close();
}
void row_formatter(const menu::Entry& entry, char* output, u32 capacity) {
    const u32 index = entry.id;
    if (index >= character_count) return;
    const char* text = nullptr;
    switch (results[index]) {
    case Result::queued: text = "queued"; break;
    case Result::spawned: text = "spawned"; break;
    case Result::missing_pack: text = "pack absent"; break;
    case Result::missing_class: text = "class absent"; break;
    case Result::failed: text = "creation failed"; break;
    case Result::capacity: text = "capacity wait"; break;
    case Result::heap: text = "heap wait"; break;
    case Result::invalid: text = "invalid data"; break;
    default: {
        ResourceKey pack{};
        text = !resources_ready() ? "unavailable" : pack_available(index, pack) ? "pack available" : "pack absent";
        break;
    }
    }
    menu::copy_text(output, capacity, text);
    if (results[index] == Result::spawned) {
        const s32 alias = resolved_alias[index]; char resolved[64]; pack_name(index, resolved, alias);
        menu::append_text(output, capacity, ": "); menu::append_text(output, capacity, resolved);
        menu::append_text(output, capacity, " / "); menu::append_text(output, capacity, alias >= 0 ? aliases[alias].entity : names[index]);
    }
}
void forget_world() {
    actor_count = 0u;
    for (u32 i = 0u; i < character_count; ++i) { packs[i] = PackRecord{}; results[i] = Result::unknown; }
    cancel_pending(); pop_requested = false; cleanup_requested = false;
    owning_world = nullptr; owning_terrain = nullptr;
}

}  // namespace

const char* character_name(u32 index) { return index < character_count ? names[index] : nullptr; }
bool pending() { return queue_size != 0u || pop_requested || cleanup_requested; }
bool has_resources() {
    if (pending() || actor_count != 0u) return true;
    for (u32 i = 0u; i < character_count; ++i) if (packs[i].used) return true;
    return false;
}
bool prepare_transition() {
    cancel_pending(); pop_requested = false;
    if (has_resources()) { cleanup_requested = true; return false; }
    return true;
}
void cancel_pending() {
    queue_head = 0u; queue_size = 0u; delay = 0u;
    for (u32 i = 0u; i < character_count; ++i) if (results[i] == Result::queued || results[i] == Result::heap || results[i] == Result::capacity) results[i] = Result::unknown;
}
void populate(menu::Menu& destination) {
    const char* const controls[] = {"Pop Character", "Pop All Characters", "Spawn All Characters"};
    for (u32 i = 0u; i < 3u; ++i) {
        if (auto* entry = menu::add(destination, controls[i], menu::EntryType::action)) {
            entry->id = i; entry->handler = control_handler;
        }
    }
    for (u32 i = 0u; i < character_count; ++i) {
        if (auto* entry = menu::add(destination, names[i], menu::EntryType::action)) {
            entry->id = i; entry->handler = row_handler; entry->formatter = row_formatter;
        }
    }
}
void frame_advance() {
    const bool previous_lock = menu::interaction_is_locked();
    // Transition owners may hold navigation locked while they wait for our
    // cleanup. Creation never runs under another owner's lock.
    if (worker_active || (previous_lock && !cleanup_requested)) return;
    void* current_world = engine::world();
    void* current_terrain = engine::terrain();
    const bool same_world = owning_world != nullptr && owning_world == current_world &&
                            owning_terrain == current_terrain;
    if (!engine::interactive_world_ready() || !readable(current_world, 0x228u)) {
        // Menu close can expose the pause process's final state for one tick.
        // Keep ownership intact, but perform no actor/pack calls until gameplay
        // resumes. All other transitions still belong to native teardown.
        if (same_world && readable(current_world, 0x228u) && returning_from_native_pause()) return;
        if (owning_world != nullptr) forget_world();
        return;
    }
    if (owning_world != nullptr && !same_world) forget_world();
    if (!resources_ready()) return;
    void* manager = stack();
    void* partition = native<void* (*)(s32)>(get_partition)(3);
    if (manager == nullptr || !readable(partition, 0xB4u) || stack_busy(manager)) return;
    owning_world = engine::world(); owning_terrain = engine::terrain();
    worker_active = true;
    menu::set_interaction_locked(true);
    prune_dead_actors();
    if (cleanup_requested || pop_requested) {
        if (actor_count != 0u) remove_last_actor();
        pop_requested = false;
        release_one_pack(manager, partition);
        if (cleanup_requested && actor_count == 0u) {
            cleanup_requested = false;
            for (u32 i = 0u; i < character_count; ++i) if (packs[i].used) cleanup_requested = true;
        }
        menu::set_interaction_locked(previous_lock); worker_active = false; return;
    }
    if (release_one_pack(manager, partition, true)) { menu::set_interaction_locked(previous_lock); worker_active = false; return; }
    if (delay != 0u) { --delay; menu::set_interaction_locked(previous_lock); worker_active = false; return; }
    if (queue_size != 0u) {
        const u32 index = requests[queue_head];
        if (spawn(index, manager, partition)) { queue_head = (queue_head + 1u) % queue_capacity; --queue_size; }
        delay = spawn_delay;
    }
    menu::set_interaction_locked(previous_lock); worker_active = false;
}

}  // namespace usm::gc::character_spawn
