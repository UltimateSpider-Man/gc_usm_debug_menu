// Freestanding i386 execution of the production worker. Fixed-address native
// entrypoint stubs preserve the GC pointer/layout boundary and return ABI.
#include "../../gamecube/menus/character_spawn_gc.cpp"
using namespace usm::gc;

extern "C" void* memcpy(void* d, const void* s, unsigned int n) {
    auto* out = static_cast<u8*>(d); auto* in = static_cast<const u8*>(s);
    for (u32 i = 0; i < n; ++i) out[i] = in[i];
    return d;
}
extern "C" void* memset(void* d, int v, unsigned int n) {
    auto* out = static_cast<u8*>(d); for (u32 i = 0; i < n; ++i) out[i] = static_cast<u8>(v); return d;
}
extern "C" unsigned int strlen(const char* s) { u32 n = 0; while (s[n]) ++n; return n; }

namespace fixture {
using namespace usm::gc::character_spawn;
constexpr uptr world_address = 0x81000000u;
constexpr uptr manager_address = 0x81000800u;
constexpr uptr partitions_address = 0x81001000u;
constexpr uptr mission_vector = 0x81009000u;
constexpr uptr table_address = 0x81010000u;
constexpr uptr game_address = 0x81011000u;
constexpr uptr process_address = 0x81012000u;
constexpr uptr process_states_address = 0x81013000u;
constexpr uptr process_name_address = 0x81014000u;
constexpr uptr slots_address = 0x81020000u;
constexpr uptr hero_address = 0x81100000u;
constexpr uptr pose_address = 0x81101000u;
constexpr uptr actors_address = 0x81200000u;
struct Asset { u32 hash; u32 entity; bool available; bool has_class; bool foreign_source; };
Asset assets[character_count + alias_count];
void* actor_values[256];
u32 actor_handles[256];
u32 created, destroyed, pushed, popped, visible_calls, health_calls, unique_calls;
u32 slot_count, context_depth, visited[character_count], failures, checks, abi_errors;
u32 collision_checks;
float created_pose[16];
u32 borrowed_pack_hash;
bool busy, room, create_fails, invalid_handle, interactive = true, locked, asynchronous;
bool collision;
bool paged_class, paged_short_extent;
s32 free_heap;
s32 current_state;
void* current_world;
void* current_terrain;
void* current_context;
char last_status[160];

int system_call(u32 number, u32 a, u32 b = 0, u32 c = 0) {
    int r; asm volatile("int $0x80" : "=a"(r) : "a"(number), "b"(a), "c"(b), "d"(c) : "memory"); return r;
}
void print(const char* s) { system_call(4, 1, reinterpret_cast<uptr>(s), strlen(s)); }
void check(bool good, const char* label) {
    ++checks; if (!good) { ++failures; print("FAIL: "); print(label); print("\n"); }
}
void put(uptr a, u32 v) { *reinterpret_cast<u32*>(a) = v; }
u32 get(uptr a) { return *reinterpret_cast<u32*>(a); }
bool same(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; } return *a == *b;
}
bool contains(const char* a, const char* b) {
    for (; *a; ++a) { const char* p = a; const char* q = b; while (*q && *p == *q) { ++p; ++q; } if (!*q) return true; } return false;
}
void hash(u32* out, const char* text) {
    u32 value = 2166136261u;
    while (*text) { u8 c = static_cast<u8>(*text++); if (c >= 'A' && c <= 'Z') c += 32; value = (value ^ c) * 16777619u; }
    *out = value;
}
u32 hashed(const char* text) { u32 result; hash(&result, text); return result; }
s32 asset_index(u32 value) {
    for (u32 i = 0; i < character_count + alias_count; ++i) if (assets[i].hash == value) return static_cast<s32>(i);
    return -1;
}
void* partition(s32 index) { return reinterpret_cast<void*>(partitions_address + static_cast<u32>(index) * 0x1000u); }
bool stats(const ResourceKey* value, PackLocation* out, void*, void*) {
    if (value->type != 25) ++abi_errors;
    const s32 i = asset_index(value->hash);
    if (i >= 0 && static_cast<u32>(i) < character_count) ++visited[i];
    if (i < 0 || !assets[i].available) return false;
    if (out) { for (u32 n = 0; n < 20; ++n) out->words[n] = 0; out->words[3] = 4096u; out->words[5] = 128u; }
    return true;
}
bool has_room_stub(void* p, s32 reserved) {
    if (p != partition(3) || reserved != 3968) ++abi_errors;
    return room;
}
bool waiting(void* p) { if (p != reinterpret_cast<void*>(manager_address)) ++abi_errors; return busy; }
bool has_pack(void*, const engine::MString* value) {
    const u32 h = hashed(value->guts);
    for (u32 i = 0; i < slot_count; ++i) if (get(get(mission_vector + i * 4u)) == h) return true;
    return false;
}
void append_slot(u32 h, u32 state = 4u) {
    const s32 a = asset_index(h);
    const uptr slot = slots_address + slot_count * 0x200u;
    put(slot, h); put(slot + 4u, 25u); put(slot + 8u, state); put(slot + 0x18u, slot + 0x100u);
    put(slot + 0x94u, static_cast<u32>(a)); put(slot + 0x100u, slot);
    put(slot + 0x178u, slot); put(slot + 0x180u, 0xF0000100u);
    put(slot + 0x188u, paged_short_extent ? 8u : 0x100u);
    put(mission_vector + slot_count++ * 4u, slot);
    put(reinterpret_cast<uptr>(partition(3)) + 12u, mission_vector + slot_count * 4u);
}
void push(void*, const engine::MString* name, const engine::MString* duplicate) {
    if (!locked || !same(name->guts, duplicate->guts) || name->size != static_cast<s32>(strlen(name->guts))) ++abi_errors;
    ++pushed; append_slot(hashed(name->guts), asynchronous ? 3u : 4u);
    character_spawn::frame_advance(); // A synchronous native render flush reenters safely.
}
void pop(void*, const engine::MString* name, const engine::MString* duplicate) {
    if (!locked || slot_count == 0 || !same(name->guts, duplicate->guts) || get(get(mission_vector + (slot_count - 1u) * 4u)) != hashed(name->guts)) { ++abi_errors; return; }
    ++popped; put(get(mission_vector + --slot_count * 4u) + 8u, 0u);
    put(reinterpret_cast<uptr>(partition(3)) + 12u, mission_vector + slot_count * 4u);
}
void* directory(void* dir, const ResourceKey* value, s32* size, void** source) {
    const uptr slot = get(reinterpret_cast<uptr>(dir)); const u32 a = get(slot + 0x94u);
    if (value->type != 4 || a >= character_count + alias_count || !assets[a].has_class || value->hash != assets[a].entity) return nullptr;
    *size = 16; *source = reinterpret_cast<void*>(assets[a].foreign_source ? slots_address + 0x100000u : slot);
    return reinterpret_cast<void*>(paged_class ? 0xF0000100u : slot + 0x140u);
}
void* push_context(void* slot) { if (!locked || context_depth != 0) ++abi_errors; ++context_depth; current_context = slot; return nullptr; }
void* pop_context() { if (!locked || context_depth != 1) ++abi_errors; --context_depth; current_context = nullptr; return nullptr; }
void unique(u32* out) { ++unique_calls; *out = 0x10000u + unique_calls; }
void* create(void* manager, const u32* class_id, const u32* id, const float* pose, const engine::MString* tag, u32 flags, void* regions) {
    if (!locked || manager != static_cast<u8*>(current_world) + 0x74u || context_depth != 1 || current_context == nullptr || flags != 1u || regions != nullptr || tag->size != 0 || pose[15] != 1.0f || *id != 0x10000u + unique_calls) ++abi_errors;
    const u32 a = get(reinterpret_cast<uptr>(current_context) + 0x94u);
    if (a >= character_count + alias_count || *class_id != assets[a].entity) ++abi_errors;
    if (create_fails) return nullptr;
    for (u32 i = 0u; i < 16u; ++i) created_pose[i] = pose[i];
    const uptr actor = actors_address + created * 0x400u;
    actor_values[created] = reinterpret_cast<void*>(actor); actor_handles[created] = invalid_handle ? 0u : *id;
    put(actor + 0x1Cu, actor_handles[created]); put(actor + 0x68u, actor + 0x100u);
    put(actor + 0x104u, actor); *reinterpret_cast<float*>(actor + 0x308u) = 125.0f;
    ++created; return reinterpret_cast<void*>(actor);
}
void* resolve(const u32* handle) {
    for (u32 i = 0; i < created; ++i) if (actor_handles[i] != 0u && actor_handles[i] == *handle) return actor_values[i];
    return nullptr;
}
void destroy(void* manager, void* actor) {
    if (!locked || actor == reinterpret_cast<void*>(get(reinterpret_cast<uptr>(current_world) + 0x21Cu)) || manager != static_cast<u8*>(current_world) + 0x74u) ++abi_errors;
    ++destroyed; for (u32 i = 0; i < created; ++i) if (actor_values[i] == actor) actor_handles[i] = 0u;
}
const float* pose(void* actor) { if (actor != reinterpret_cast<void*>(get(reinterpret_cast<uptr>(current_world) + 0x21Cu))) ++abi_errors; return reinterpret_cast<const float*>(pose_address); }
void show(void*, bool value, bool recurse) { if (!locked || !value || recurse) ++abi_errors; ++visible_calls; }
bool health(void* damage, const ResourceKey* value, float maximum, bool optional) {
    if (!locked || value->hash != hashed("HIT_POINTS") || value->type != 67 || maximum != 125.0f || optional) ++abi_errors;
    *reinterpret_cast<float*>(static_cast<u8*>(damage) + 0x200u) = maximum; ++health_calls; return true;
}
s32 heap(s32 index) { if (index != 0) ++abi_errors; return free_heap; }
bool intersect(const Vec3*, float radius, const void* ent, const void* obb, Vec3* impact, Vec3* normal, void** hit, void** tree) {
    if (!locked || radius != 0.5f || ent != reinterpret_cast<void*>(hero_address + 0x2000u) || obb != reinterpret_cast<void*>(hero_address + 0x2004u) || !impact || !normal || !hit || !tree) ++abi_errors;
    ++collision_checks; return collision;
}
template <typename F> void install(uptr address, F function) {
    auto* code = reinterpret_cast<u8*>(address); code[0] = 0xB8u;
    *reinterpret_cast<u32*>(code + 1) = reinterpret_cast<uptr>(function); code[5] = 0xFFu; code[6] = 0xE0u;
}
void reset_fixture(bool all_available = true) {
    forget_world(); memset(actors, 0, sizeof(actors)); memset(packs, 0, sizeof(packs));
    memset(visited, 0, sizeof(visited)); memset(actor_handles, 0, sizeof(actor_handles));
    created = destroyed = pushed = popped = visible_calls = health_calls = unique_calls = 0;
    slot_count = context_depth = 0; busy = false; room = true; create_fails = false; invalid_handle = false;
    collision_checks = 0; collision = false; paged_class = paged_short_extent = false;
    interactive = true; locked = false; asynchronous = false; free_heap = 8 * 1024 * 1024;
    current_state = 6;
    put(game_address + 0x84u, process_address); put(game_address + 0x88u, process_address + 0x18u);
    put(game_address + 0x1A0u, 1u); put(game_address + 0x1B0u, 0u);
    put(process_address, process_name_address); put(process_address + 4u, process_states_address);
    put(process_address + 8u, 1u); put(process_address + 12u, 2u);
    put(process_states_address, 7u); put(process_states_address + 4u, 14u);
    memcpy(reinterpret_cast<void*>(process_name_address), "pause", 6u);
    current_world = reinterpret_cast<void*>(world_address); current_terrain = reinterpret_cast<void*>(world_address + 0x400u); current_context = nullptr;
    put(world_address + 0x21Cu, hero_address); put(world_address + 0x224u, 1u);
    put(accept_all_filter_pointer, hero_address + 0x2000u); put(sphere_filter_pointer, hero_address + 0x2004u);
    for (u32 i = 0; i < 16; ++i) reinterpret_cast<float*>(pose_address)[i] = i % 5 == 0 ? 1.0f : 0.0f;
    put(resource_manager_pointer, manager_address + 0x100u); put(manager_address + 0x100u, manager_address + 0x200u);
    put(pack_table_pointer, table_address); put(pack_table_count, 1u); put(stack_pointer, manager_address);
    for (s32 i = 0; i < 8; ++i) {
        put(manager_address + 0x200u + static_cast<u32>(i) * 4u, reinterpret_cast<uptr>(partition(i)));
        put(reinterpret_cast<uptr>(partition(i)) + 8u, mission_vector);
        put(reinterpret_cast<uptr>(partition(i)) + 12u, mission_vector);
    }
    for (u32 i = 0; i < character_count; ++i) { char name[64]; pack_name(i, name); assets[i] = Asset{hashed(name), hashed(names[i]), all_available, true, false}; }
    for (u32 i = 0; i < alias_count; ++i) assets[character_count + i] = Asset{hashed(aliases[i].pack), hashed(aliases[i].entity), false, true, false};
}
void step(u32 n = 1u) { for (u32 i = 0; i < n; ++i) character_spawn::frame_advance(); }
void drain(u32 limit = 2200u) { while (character_spawn::pending() && limit-- != 0u) step(); }
void request(u32 index) { menu::Entry e{}; e.id = index; row_handler(e, menu::Command::activate); }
void control(u32 index) { menu::Entry e{}; e.id = index; control_handler(e, menu::Command::activate); }
}

namespace usm::gc::engine {
void* world() { return fixture::current_world; }
void* terrain() { return fixture::current_terrain; }
void* game() { return reinterpret_cast<void*>(fixture::game_address); }
s32 game_state() { return fixture::current_state; }
bool interactive_world_ready() { return fixture::interactive && (fixture::current_state == 6 || fixture::current_state == 7); }
}
namespace usm::gc::menu {
Entry* add(Menu& target, const char* label, EntryType type) {
    if (target.count >= target.capacity) return nullptr;
    Entry* e = target.entries + target.count++; *e = Entry{}; e->label = label; e->type = type; return e;
}
void copy_text(char* out, u32 capacity, const char* s) {
    if (!capacity) return;
    u32 i = 0; while (i + 1u < capacity && s && s[i]) { out[i] = s[i]; ++i; } out[i] = '\0';
}
void append_text(char* out, u32 capacity, const char* s) { const u32 size = strlen(out); if (size < capacity) copy_text(out + size, capacity - size, s); }
void set_status(const char* s, u16) { copy_text(fixture::last_status, sizeof(fixture::last_status), s); }
void close() {}
void set_interaction_locked(bool value) { fixture::locked = value; }
bool interaction_is_locked() { return fixture::locked; }
}

extern "C" int run_tests() {
    using namespace fixture;
    u32 mapping[6] = {0x80000000u, 0x1800000u, 7u, 0x32u, ~0u, 0u};
    if (static_cast<uptr>(system_call(90, reinterpret_cast<uptr>(mapping))) != mapping[0]) { print("Cannot map MEM1 fixture\n"); return 2; }
    install(string_hash_set, hash); install(get_pack_stats, stats); install(get_partition, partition);
    install(has_room, has_room_stub); install(stack_waiting, waiting); install(stack_has_pack, has_pack);
    install(stack_push, push); install(stack_pop, pop); install(directory_get, directory);
    install(context_push, push_context); install(context_pop, pop_context); install(unique_entity_id, unique);
    install(create_entity, create); install(destroy_entity, destroy); install(resolve_handle, resolve);
    install(absolute_pose, pose); install(set_visible, show); install(set_damage_attribute, health); install(heap_bytes_free, heap);
    install(sphere_intersection, intersect);

    reset_fixture(); menu::Entry entries[80]{}; menu::Menu menu{}; menu.entries = entries; menu.capacity = 80;
    populate(menu);
    check(menu.count == 69u && same(entries[0].label, "Pop Character") && same(entries[2].label, "Spawn All Characters"), "three controls plus all 66 requested rows");
    check(same(character_name(0), "alex_ohirn") && same(character_name(65), "gang_skin_boss_fem") && character_name(66) == nullptr, "catalogue order and bounds");
    control(2); step(); check(created == 1u && queue_size == 65u, "Spawn All queues every row and creates one actor per worker step");
    step(12); check(created == 1u, "native creation is paced across frames");
    drain(); check(created == 66u && actor_count == 66u && pushed == 66u && queue_size == 0u, "all 66 available assets complete without silent omissions");
    bool every = true; for (u32 i = 0; i < character_count; ++i) every = every && visited[i] != 0u && results[i] == Result::spawned;
    check(every && visible_calls == 66u && health_calls == 66u && unique_calls == 66u && collision_checks == 66u, "every exact pair checks placement and creates a visible unique actor with authored health");
    check(!locked && context_depth == 0u, "native reentrant flush preserves transaction and resource context balance");
    control(1); drain(); check(actor_count == 0u && destroyed == 66u && popped == 66u && slot_count == 0u, "Pop All removes only tracked actors and releases their native packs in LIFO order");

    reset_fixture(); request(0); drain(); request(0); drain();
    check(created == 2u && pushed == 1u && packs[0].references == 2u, "duplicate actors reuse an owned pack and retain both references");
    control(0); step(); check(actor_count == 1u && destroyed == 1u && popped == 0u && packs[0].references == 1u, "Pop Character removes newest actor and keeps a shared pack live");
    control(0); step(); check(actor_count == 0u && popped == 1u, "last reference releases owned pack");

    reset_fixture(); request(0); drain(); append_slot(0xAABBCCDDu);
    control(1); drain(); check(destroyed == 1u && popped == 0u && packs[0].used, "foreign native stack entry blocks debug pack unload");
    --slot_count; put(reinterpret_cast<uptr>(partition(3)) + 12u, mission_vector + slot_count * 4u); step();
    check(popped == 1u && !packs[0].used, "deferred owned pack cleanup resumes when it becomes native stack top");

    reset_fixture(); append_slot(assets[0].hash); borrowed_pack_hash = assets[0].hash;
    request(0); drain(); control(1); drain(); check(created == 1u && destroyed == 1u && pushed == 0u && popped == 0u, "resident foreign-owned pack is borrowed and never unloaded");

    reset_fixture(); busy = true; request(0); step(20);
    check(created == 0u && queue_size == 1u, "native streamer busy preserves queued creation"); busy = false;
    locked = true; step(); check(created == 0u && locked, "preexisting mission/hero transaction lock is retained"); locked = false;
    free_heap = 128; step(); check(created == 0u && pushed == 0u && queue_size == 1u && results[0] == Result::heap, "heap reserve pauses work before native allocation");
    free_heap = 8 * 1024 * 1024; room = false; step(13); check(created == 0u && queue_size == 1u, "native mission memory capacity preserves request");
    room = true; drain(); check(created == 1u && !pending(), "queued creation resumes once guarded capacity recovers");

    reset_fixture(false); free_heap = 0; control(2); drain();
    check(queue_size == 0u && created == 0u && pushed == 0u, "absent assets are reported and do not deadlock Spawn All under low memory");
    check(results[65] == Result::missing_pack && contains(last_status, "fallback absent"), "missing final requested character is explicit");

    reset_fixture(); assets[0].has_class = false; request(0); drain();
    check(created == 0u && pushed == 1u && popped == 1u && results[0] == Result::missing_class, "missing class rolls back its newly loaded pack");
    reset_fixture(); assets[0].foreign_source = true; request(0); drain();
    check(created == 0u && popped == 1u, "same-named class from a parent pack cannot steal the request");
    reset_fixture(); paged_class = true; request(0); drain();
    check(created == 1u && actor_count == 1u && results[0] == Result::spawned,
          "class mash owned by a live READY paged pack reaches native entity creation");
    reset_fixture(); paged_class = paged_short_extent = true; request(0); drain();
    check(created == 0u && popped == 1u && results[0] == Result::missing_class,
          "paged class spanning beyond its owning pack is rejected and rolled back");
    reset_fixture(); paged_class = true; append_slot(assets[0].hash);
    put(slots_address + 0x178u, 0u); borrowed_pack_hash = assets[0].hash; request(0); drain();
    check(created == 0u && popped == 0u && results[0] == Result::missing_class,
          "paged class without a directory ownership back-reference is rejected");
    reset_fixture(); create_fails = true; request(0); drain();
    check(created == 0u && pushed == 1u && popped == 1u && context_depth == 0u, "native creation failure balances context and rolls back owned pack");
    reset_fixture(); invalid_handle = true; request(0); drain();
    check(created == 1u && destroyed == 1u && popped == 1u && actor_count == 0u, "invalid native handle destroys actor before releasing its resources");

    reset_fixture(); request(0); drain(); actor_handles[0] = 0u; step();
    check(actor_count == 0u && destroyed == 0u && popped == 1u, "externally destroyed generation handle is pruned without a stale destroy call");
    reset_fixture(); request(0); drain(); put(world_address + 0x21Cu, reinterpret_cast<uptr>(actor_values[0]));
    control(1); step(); check(actor_count == 1u && destroyed == 0u && popped == 0u && packs[0].references == 1u, "current hero is protected together with its owned resource reference");
    put(world_address + 0x21Cu, hero_address); drain(); check(destroyed == 1u && popped == 1u, "protected actor cleanup resumes after hero ownership moves away");

    reset_fixture(); step(); current_terrain = reinterpret_cast<void*>(world_address + 0x500u);
    request(0); drain();
    check(created == 1u && actor_count == 1u && queue_size == 0u,
          "first selection after an idle terrain change binds the current world before queuing");
    reset_fixture(); request(0); drain();
    current_world = reinterpret_cast<void*>(world_address + 0x800u);
    put(reinterpret_cast<uptr>(current_world) + 0x21Cu, hero_address);
    current_terrain = reinterpret_cast<void*>(world_address + 0xC00u);
    request(1); drain();
    check(created == 2u && actor_count == 1u && actors[0].name == 1u && destroyed == 0u && popped == 0u,
          "first selection in a replacement world drops old ownership without cancelling the new request");
    reset_fixture(); request(0); current_terrain = reinterpret_cast<void*>(world_address + 0x500u); step();
    check(queue_size == 0u && created == 0u, "terrain transition after selection still discards a stale queued request before creation");
    reset_fixture(); current_state = 7; request(0); current_state = 14; step();
    check(queue_size == 1u && created == 0u && pushed == 0u && results[0] == Result::queued,
          "closing the debug menu preserves its first spawn through native pause POP_PROCESS");
    current_state = 6; drain();
    check(actor_count == 1u && created == 1u && queue_size == 0u,
          "first accepted spawn executes once after the native pause process returns to gameplay");
    reset_fixture(); request(0); drain(); current_state = 14; step();
    check(actor_count == 1u && destroyed == 0u && popped == 0u && packs[0].references == 1u,
          "native pause return preserves existing actor and pack ownership without touching them");
    current_state = 6; control(0); drain();
    check(actor_count == 0u && destroyed == 1u && popped == 1u,
          "Pop Character remains able to remove a lineup after returning from pause");
    reset_fixture(); request(0); current_state = 14;
    put(process_states_address, 5u); step();
    check(queue_size == 0u && actor_count == 0u && created == 0u,
          "a non-pause POP_PROCESS still discards stale queued work");
    reset_fixture(); current_state = 7; control(2); current_state = 14; step();
    check(queue_size == 66u && created == 0u && pushed == 0u,
          "Spawn All retains every accepted character across the native pause return");
    current_state = 6; drain();
    check(created == 66u && actor_count == 66u && !pending(),
          "Spawn All resumes its paced native creation after the pause process is popped");
    reset_fixture(); request(0); drain(); current_state = 7; control(1); current_state = 14; step();
    check(actor_count == 1u && cleanup_requested && destroyed == 0u && popped == 0u,
          "Pop All waits for native pause return without discarding live ownership");
    current_state = 6; drain();
    check(actor_count == 0u && !pending() && destroyed == 1u && popped == 1u,
          "Pop All destroys and releases the retained lineup after gameplay resumes");
    reset_fixture(); request(0); current_state = 14; put(process_address + 8u, 0u); step();
    check(queue_size == 0u && created == 0u,
          "a malformed pause process index is not treated as an unpause return");
    reset_fixture(); request(0); current_state = 14;
    memcpy(reinterpret_cast<void*>(process_name_address), "other", 6u); step();
    check(queue_size == 0u && created == 0u,
          "another native process with the same state list cannot retain stale queued work");
    reset_fixture(); request(0); current_state = 14;
    put(game_address + 0x88u, 0x81800018u); step();
    check(queue_size == 0u && created == 0u,
          "an invalid process vector is rejected before its top process is read");
    reset_fixture(); request(0); current_state = 14; put(game_address + 0x1A0u, 0u); step();
    check(queue_size == 0u && created == 0u,
          "an unloaded world cannot retain a request even with a matching pause process");
    reset_fixture(); request(0); current_state = 14;
    current_terrain = reinterpret_cast<void*>(world_address + 0x500u); step();
    check(queue_size == 0u && created == 0u,
          "a terrain replacement during pause return cancels the request");
    reset_fixture(); request(0); drain(); interactive = false; step();
    check(actor_count == 0u && destroyed == 0u && popped == 0u, "level teardown forgets old ownership without dereferencing unloaded native objects");

    reset_fixture(); asynchronous = true; request(0); step();
    check(queue_size == 1u && created == 0u && packs[0].used && pushed == 1u, "partially loaded native pack is retained as owned");
    put(slots_address + 8u, 4u); asynchronous = false; drain();
    check(created == 1u && pushed == 1u && popped == 0u, "completion retries the same native pack without reload or premature pop");

    reset_fixture(false); const s32 beetle = 1; assets[character_count + beetle].available = true;
    request(3); drain(); char rendered[160]; menu::Entry e{}; e.id = 3; row_formatter(e, rendered, sizeof(rendered));
    check(created == 1u && contains(rendered, "ch_vwr_beetle_viewer") && contains(rendered, "/ beetle_viewer"), "verified GC viewer alias reports resolved pack and class explicitly");
    control(1); drain(); check(popped == 1u, "aliased pack is released by its actual native name");
    reset_fixture(); request(0); drain(); request(1);
    check(has_resources() && !prepare_transition() && queue_size == 0u && cleanup_requested,
          "state transition cancels queued spawning and starts cleanup before taking an engine lock");
    drain(); check(prepare_transition() && !has_resources() && destroyed == 1u && popped == 1u,
                   "state transition proceeds only after owned actors and packs are fully drained");
    reset_fixture(); request(0); drain(); append_slot(0xAABBCCDDu); prepare_transition(); drain();
    check(!prepare_transition() && has_resources() && popped == 0u,
          "foreign native stack ownership continues blocking a state transition after actors are removed");
    reset_fixture(); collision = true; request(0); drain();
    check(created == 1u && created_pose[12] == 0.0f && created_pose[13] > 0.099f && created_pose[13] < 0.101f && created_pose[14] == 0.0f,
          "obstructed placement uses the verified native sphere-query fallback above the hero");
    reset_fixture(); request(0); drain(); request(1); drain(); request(2); prepare_transition(); locked = true; step();
    check(locked && created == 2u && destroyed == 1u && cleanup_requested,
          "prelocked transition permits owned cleanup and preserves the external navigation lock");
    drain(); check(locked && destroyed == 2u && popped == 2u && prepare_transition(),
                   "prelocked cleanup drains every actor and pack without allowing queued creation");
    request(2); step(20); check(locked && created == 2u && queue_size == 1u,
                               "foreign transaction lock blocks creation after cleanup completes");
    check(abi_errors == 0u, "all native calls satisfy verified layouts, arguments, flags and transaction boundaries");
    char digits[16]; u32 length = 0u, number = checks;
    do { digits[length++] = static_cast<char>('0' + number % 10u); number /= 10u; } while (number);
    print(failures == 0u ? "PASS: " : "FAILED: ");
    while (length) { char digit[2] = {digits[--length], '\0'}; print(digit); }
    print(" Character spawning native-boundary lifecycle checks\n");
    return failures == 0u ? 0 : 1;
}
asm(".global _start\n_start:\n call run_tests\n mov %eax, %ebx\n mov $1, %eax\n int $0x80\n");
