#include "menus/script_progression_gc.h"

#include "gc_engine.h"
#include "menus/progression_gc.h"

namespace usm::gc::script_progression {
namespace {

bool readable_object(uptr pointer, u32 size) {
    return (pointer & 3u) == 0u && pointer >= 0x80000000u &&
           pointer < 0x81800000u && size <= 0x81800000u - pointer;
}

u32 load_u32(uptr pointer) {
    return *reinterpret_cast<volatile const u32*>(pointer);
}

void kill_hero_handler(menu::Entry&, menu::Command command) {
    if (command != menu::Command::activate) return;
    const uptr world = reinterpret_cast<uptr>(engine::world());
    if (!engine::interactive_world_ready() || !readable_object(world, 0x228u) ||
        load_u32(world + 0x224u) == 0u) {
        menu::set_status("Kill Hero requires a live player");
        return;
    }
    const uptr hero = load_u32(world + 0x21Cu);
    if (!readable_object(hero, 0xC0u)) {
        menu::set_status("Hero pointer is invalid");
        return;
    }
    // actor::damage_ifc at 0x80135F18 loads +0x68. The native setter
    // 0x800E50E4 handles HIT_POINTS and clamps its bounded value itself.
    const uptr damage = load_u32(hero + 0x68u);
    if (!readable_object(damage, 0x234u) || load_u32(damage + 4u) != hero) {
        menu::set_status("Hero has no live damage interface");
        return;
    }
    struct ResourceKey { u32 hash; s32 type; } key{};
    using SetHash = void (*)(u32*, const char*);
    const auto set_hash = reinterpret_cast<SetHash>(0x8015BF68u);
    set_hash(&key.hash, "HIT_POINTS");
    key.type = 67;  // RESOURCE_KEY_TYPE_IFC_ATTRIBUTE, confirmed by setter.
    using SetAttribute = bool (*)(void*, const ResourceKey*, float, bool);
    if (!reinterpret_cast<SetAttribute>(0x800E50E4u)(
            reinterpret_cast<void*>(damage), &key, 0.0f, false)) {
        menu::set_status("Native HIT_POINTS setter rejected the request");
        return;
    }
    // Match the PC menu: raise DESTROYED through the event manager so the
    // normal mission failure/death flow also runs. Both arguments are refs
    // in the CodeWarrior ABI (verified at 0x800FFF78).
    u32 destroyed = 0u;
    set_hash(&destroyed, "DESTROYED");
    const u32 handle = load_u32(hero + 0x1Cu);
    using RaiseEvent = void (*)(const u32*, const u32*);
    reinterpret_cast<RaiseEvent>(0x800FFF78u)(&destroyed, &handle);
    menu::close();
}

}  // namespace

void populate_diagnostics(menu::Menu& script, menu::Menu& progression) {
    menu::clear_entries(script);
    menu::Entry* kill = menu::add(script, "Kill Hero", menu::EntryType::action);
    if (kill != nullptr) kill->handler = kill_hero_handler;
    usm::gc::progression::configure(progression);
}

}  // namespace usm::gc::script_progression
