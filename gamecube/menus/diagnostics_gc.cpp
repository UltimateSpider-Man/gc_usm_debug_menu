#include "diagnostics_gc.h"

#include "../gc_engine.h"
#include "../ai_export_gc.h"
#include "../resource_memory_gc.h"

namespace usm::gc::diagnostics {
namespace {

// All addresses below were checked against the supplied NTSC-U main.dol.
// gc_address.txt points twelve bytes into several of these functions, so the
// entry points intentionally differ from the text-file labels.
// Native high-list insertion8021B650 and removal8021CB10 use r13-0x72D0.
// The adjacent8045F7B4 is the low-priority list, which may be empty in City.
constexpr uptr ai_high_list_slot = 0x8045F7B0u;
constexpr uptr found_entities_slot = 0x8045F018u;     // r13 - 31336
constexpr uptr find_entities_entry = 0x80103244u;
constexpr uptr hash_to_string_entry = 0x8015BFB8u;
constexpr uptr entity_is_actor_entry = 0x80135124u;
constexpr uptr actor_is_actor_entry = 0x80135F88u;

constexpr uptr slab_partial_lists_slot = 0x804627C0u; // r13 - 17088
constexpr uptr slab_full_lists_slot = 0x804627C4u;    // r13 - 17084
constexpr uptr slab_free_list_slot = 0x804627C8u;     // r13 - 17080
constexpr uptr slab_initialized = 0x8045F644u;        // r13 - 29756
constexpr uptr slab_headers_slot = 0x8045F64Cu;       // r13 - 29748

constexpr uptr main_ram_begin = 0x80000000u;
constexpr uptr main_ram_end = 0x81800000u;
constexpr uptr gameplay_rel_begin = 0x7F004000u;
constexpr uptr gameplay_rel_end = 0x7F1C4000u;

constexpr u32 slab_bucket_count = 44u;
constexpr u32 slab_header_count = 128u;
constexpr u32 slab_header_size = 36u;
constexpr u32 slab_list_size = 12u;
constexpr u32 maximum_ai_nodes = 1024u;
constexpr u32 maximum_entity_nodes = 4096u;
constexpr u32 maximum_ai_rows = 30u;      // Summary, page control, then rows.
constexpr u32 maximum_entity_rows = 31u;  // Page control, then rows.

constexpr u32 slab_begin_sentry = 0xE1E1E1E1u;
constexpr u32 slab_end_sentry = 0x1E1E1E1Eu;

enum class ScanState : u8 {
    game_not_ready,
    ready,
    invalid,
};

enum SlabMetric : u32 {
    slab_total = 0u,
    slab_free = 1u,
    slab_full = 2u,
    slab_partial = 3u,
    object_allocated = 4u,
    object_free = 5u,
};

struct AiRecord {
    uptr core;
    u32 actor_hash;
    u32 actor_handle;
};

struct AiSnapshot {
    ScanState state;
    u32 total;
    u32 visible_total;
    u32 record_count;
    u32 first;
    bool owner_found;
    AiRecord records[maximum_ai_rows];
};

struct EntityRecord {
    uptr actor;
    u32 actor_hash;
    u32 animation_count;
    u32 actor_handle;
};

struct EntitySnapshot {
    ScanState state;
    u32 actor_count;
    u32 record_count;
    u32 first;
    EntityRecord records[maximum_entity_rows];
};

struct SlabSnapshot {
    bool initialized;
    bool valid;
    s32 total_slabs;
    s32 free_slabs;
    s32 full_slabs[slab_bucket_count];
    s32 partial_slabs[slab_bucket_count];
    s32 allocated_objects[slab_bucket_count];
    s32 free_objects[slab_bucket_count];
};

AiSnapshot ai_snapshot;
EntitySnapshot entity_snapshot;
SlabSnapshot slab_snapshot;

char ai_labels[maximum_ai_rows][56];
char entity_labels[maximum_entity_rows][56];
char slab_labels[slab_bucket_count][16];

// These menus are reused rather than allocating a new pool for every actor.
// Their refresh callbacks resolve the owner again from the parent selection.
menu::Menu* ai_detail_menu;
menu::Menu* ai_parameter_menu;
menu::Menu* animation_detail_menu;
menu::Menu* variant_detail_menu;
char ai_detail_labels[64][56];
char ai_parameter_labels[128][56];
char animation_labels[128][56];
char variant_labels[128][56];
uptr selected_ai_core;
uptr selected_ai_actor;
uptr selected_ai_world;
u32 selected_ai_handle;
uptr selected_parameter_block;
uptr selected_animation_actor;
uptr selected_animation_world;
u32 selected_animation_handle;
uptr selected_variant_actor;
uptr selected_variant_world;
u32 selected_variant_handle;
u32 parameter_page;
u32 animation_page;
u32 variant_page;
u32 ai_root_page;
u32 animation_root_page;
u32 variant_root_page;
u32 inode_page;

void add_page(menu::Menu& destination, u32& page, u32 count, u32 page_size);

u32 entity_refresh_delay;
uptr entity_last_world;

bool range_in(uptr address, u32 size, uptr begin, uptr end) {
    return address >= begin && address < end && size <= end - address;
}

bool main_range(uptr address, u32 size) {
    return range_in(address, size, main_ram_begin, main_ram_end);
}

bool aligned_main_range(uptr address, u32 size) {
    return (address & 3u) == 0u && main_range(address, size);
}

// Authored arrays, clips and display values may be in an active pack's ARAM
// paging extent. Mutable actors, AI parameter blocks and owners stay in MEM1.
bool resource_object(uptr address, u32 size) {
    return resource_memory::readable_object(reinterpret_cast<const void*>(address), size);
}

bool runtime_data_range(uptr address, u32 size) {
    if (main_range(address, size)) {
        return true;
    }
    return engine::gameplay_rel_ready() &&
           range_in(address, size, gameplay_rel_begin, gameplay_rel_end);
}

u32 load_u32(uptr address) {
    return *reinterpret_cast<volatile const u32*>(address);
}

s32 load_s32(uptr address) {
    return *reinterpret_cast<volatile const s32*>(address);
}

u16 load_u16(uptr address) {
    return *reinterpret_cast<volatile const u16*>(address);
}

template <typename Function>
Function function_at(uptr address) {
    return reinterpret_cast<Function>(address);
}

void append_hex_u32(char* output, u32 capacity, u32 value) {
    static const char digits[] = "0123456789ABCDEF";
    char text[11];
    text[0] = '0';
    text[1] = 'x';
    for (u32 index = 0u; index != 8u; ++index) {
        const u32 shift = (7u - index) * 4u;
        text[index + 2u] = digits[(value >> shift) & 0xFu];
    }
    text[10] = '\0';
    menu::append_text(output, capacity, text);
}

bool copy_runtime_string(char* output, u32 capacity, const char* source) {
    if (output == nullptr || capacity == 0u || source == nullptr) {
        return false;
    }
    const uptr source_address = reinterpret_cast<uptr>(source);
    uptr source_end = 0u;
    if (!resource_memory::readable_extent(source_address, source_end)) {
        output[0] = '\0';
        return false;
    }

    u32 written = 0u;
    while (written + 1u < capacity && written < source_end - source_address) {
        const char value = *reinterpret_cast<volatile const char*>(source_address + written);
        if (value == '\0') {
            output[written] = '\0';
            return written != 0u;
        }
        if (value < 0x20 || value >= 0x7F) {
            output[0] = '\0';
            return false;
        }
        output[written++] = value;
    }
    output[written] = '\0';
    return written != 0u;
}

void make_hash_label(char* output, u32 capacity, u32 hash) {
    output[0] = '\0';
    using ToString = const char* (*)(const u32*);
    const char* name = function_at<ToString>(hash_to_string_entry)(&hash);
    if (copy_runtime_string(output, capacity, name)) {
        return;
    }
    menu::copy_text(output, capacity, "hash ");
    append_hex_u32(output, capacity, hash);
}

void format_unavailable(const menu::Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, "unavailable");
}

void format_read_only(const menu::Entry&, char* output, u32 capacity) {
    menu::copy_text(output, capacity, "read only");
}

void stripped_memtrack_handler(menu::Entry&, menu::Command command) {
    if (command == menu::Command::activate) {
        menu::set_status("Retail GC: memory leak tracker stripped");
    }
}

void stripped_script_memtrack_handler(menu::Entry&, menu::Command command) {
    if (command == menu::Command::activate) {
        menu::set_status("Retail GC: script memtrack stripped");
    }
}

bool ai_snapshots_equal(const AiSnapshot& left, const AiSnapshot& right) {
    if (left.state != right.state || left.total != right.total ||
        left.visible_total != right.visible_total ||
        left.record_count != right.record_count || left.first != right.first) {
        return false;
    }
    for (u32 index = 0u; index != left.record_count; ++index) {
        if (left.records[index].core != right.records[index].core ||
            left.records[index].actor_hash != right.records[index].actor_hash ||
            left.records[index].actor_handle != right.records[index].actor_handle) {
            return false;
        }
    }
    return true;
}

AiSnapshot scan_ai(u32 first = 0u, uptr find_core = 0u, u32 find_handle = 0u) {
    AiSnapshot result{};
    result.first = first;
    if (!engine::game_ready()) {
        result.state = ScanState::game_not_ready;
        return result;
    }

    result.state = ScanState::ready;
    const uptr list = load_u32(ai_high_list_slot);
    if (list == 0u) {
        return result;
    }
    if (!aligned_main_range(list, 8u)) {
        result.state = ScanState::invalid;
        return result;
    }

    const uptr sentinel = load_u32(list + 4u);
    if (!aligned_main_range(sentinel, 12u)) {
        result.state = ScanState::invalid;
        return result;
    }

    uptr previous = sentinel;
    uptr node = load_u32(sentinel);
    for (u32 visited = 0u; node != sentinel; ++visited) {
        if (visited >= maximum_ai_nodes || !aligned_main_range(node, 12u) ||
            load_u32(node + 4u) != previous) {
            result.state = ScanState::invalid;
            return result;
        }

        const uptr next = load_u32(node);
        const uptr core = load_u32(node + 8u);
        if ((next != sentinel && !aligned_main_range(next, 12u)) ||
            !aligned_main_range(core, 0x5Cu)) {
            result.state = ScanState::invalid;
            return result;
        }

        const uptr actor = load_u32(core + 0x58u);
        if (!aligned_main_range(actor, 0x20u)) {
            result.state = ScanState::invalid;
            return result;
        }

        ++result.total;
        const u32 actor_handle = load_u32(actor + 0x1Cu);
        if (core == find_core && actor_handle == find_handle) result.owner_found = true;
        const u32 actor_flags = load_u32(actor + 4u);
        if ((actor_flags & 0x800u) == 0u) {
            ++result.visible_total;
            if (result.visible_total > first && result.record_count != maximum_ai_rows) {
                AiRecord& record = result.records[result.record_count++];
                record.core = core;
                record.actor_hash = load_u32(actor + 0x10u);
                record.actor_handle = actor_handle;
            }
        }

        previous = node;
        node = next;
    }

    if (load_u32(sentinel + 4u) != previous) {
        result.state = ScanState::invalid;
    }
    return result;
}

void format_ai_summary(const menu::Entry&, char* output, u32 capacity) {
    if (ai_snapshot.state == ScanState::game_not_ready) {
        menu::copy_text(output, capacity, "game not ready");
    } else if (ai_snapshot.state == ScanState::invalid) {
        menu::copy_text(output, capacity, "validation failed");
    } else {
        menu::format_s32(output, capacity, static_cast<s32>(ai_snapshot.total));
        menu::append_text(output, capacity, " total");
    }
}

void rebuild_ai_menu(menu::Menu& destination) {
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);

    menu::Entry* summary = menu::add(destination, "High-priority AI cores",
                                     menu::EntryType::read_only);
    if (summary != nullptr) {
        summary->formatter = format_ai_summary;
    }

    if (ai_snapshot.state == ScanState::ready) {
        add_page(destination, ai_root_page, ai_snapshot.visible_total, maximum_ai_rows);
        for (u32 index = 0u; index != ai_snapshot.record_count; ++index) {
            make_hash_label(ai_labels[index], sizeof(ai_labels[index]),
                            ai_snapshot.records[index].actor_hash);
            menu::Entry* entry = menu::add(destination, ai_labels[index],
                                           ai_detail_menu != nullptr
                                               ? menu::EntryType::submenu
                                               : menu::EntryType::read_only);
            if (entry != nullptr) {
                entry->target = reinterpret_cast<void*>(ai_snapshot.records[index].core);
                entry->id = ai_snapshot.records[index].actor_hash;
                entry->minimum = static_cast<s32>(ai_snapshot.records[index].actor_handle);
                entry->submenu = ai_detail_menu;
                if (ai_detail_menu == nullptr) entry->formatter = format_read_only;
            }
        }
    }

    if (destination.count != 0u) {
        destination.selected = old_selection < destination.count
                                   ? old_selection
                                   : static_cast<u16>(destination.count - 1u);
    }
}

void refresh_ai(menu::Menu& destination) {
    AiSnapshot next = scan_ai(ai_root_page * maximum_ai_rows);
    const u32 last = next.visible_total == 0u ? 0u : (next.visible_total - 1u) / maximum_ai_rows;
    if (ai_root_page > last) {
        ai_root_page = last;
        next = scan_ai(ai_root_page * maximum_ai_rows);
    }
    if (!ai_snapshots_equal(next, ai_snapshot)) {
        ai_snapshot = next;
        rebuild_ai_menu(destination);
    }
}

enum class ActorTest : u8 {
    no,
    yes,
    unknown,
};

ActorTest is_actor(uptr entity) {
    if (!aligned_main_range(entity, 0x14u)) {
        return ActorTest::unknown;
    }
    const uptr vtable = load_u32(entity);
    if ((vtable & 3u) != 0u || !runtime_data_range(vtable + 0xD0u, 8u)) {
        return ActorTest::unknown;
    }

    // CodeWarrior vtable entries are {this adjustment, function}.  There are
    // exactly two is_an_actor implementations in the supplied symbol map, so
    // comparing the validated function address avoids invoking arbitrary code
    // through a damaged entity.
    const uptr predicate = load_u32(vtable + 0xD4u);
    if (predicate == actor_is_actor_entry) {
        return ActorTest::yes;
    }
    if (predicate == entity_is_actor_entry) {
        return ActorTest::no;
    }
    return ActorTest::unknown;
}

bool actor_animation_directory(uptr actor, uptr& locations, u32& location_count) {
    locations = 0u;
    location_count = 0u;
    if (!aligned_main_range(actor, 0xC0u)) {
        return false;
    }
    const uptr slot = load_u32(actor + 0xBCu);
    if (slot == 0u) {
        return true;
    }
    if (!aligned_main_range(slot, 0x1Cu) || load_u32(slot + 8u) != 4u) {
        return false;
    }

    // The GC resource_pack_slot stores its resource_directory at +0x18.
    const uptr directory = load_u32(slot + 0x18u);
    if (directory == 0u) {
        return true;
    }
    if (!aligned_main_range(directory, 0x7Cu) || load_u32(directory + 0x78u) != slot) {
        return false;
    }

    // TLRESOURCE_TYPE_ANIM (9) uses the vector at +0x50 and u16 count +0x54.
    locations = load_u32(directory + 0x50u);
    location_count = load_u16(directory + 0x54u);
    if (location_count > 4096u) {
        return false;
    }
    if (location_count != 0u && !resource_object(locations, location_count * 12u)) {
        return false;
    }
    return true;
}

// The READY directory's type-9 locations each resolve directly to one
// nalAnimClass, whose tlFixedString hash is at +8. nalAnimFile containers
// are consumed by the loader before this directory is published. The clip's
// +4 next link also points at other directory entries; following it here
// would enumerate the same clips repeatedly.
bool actor_animations(uptr actor, u32 first, uptr* clips, u32 capacity, u32& count) {
    count = 0u;
    uptr locations = 0u;
    u32 location_count = 0u;
    if (!actor_animation_directory(actor, locations, location_count)) return false;
    for (u32 index = 0u; index != location_count; ++index) {
        const uptr location = locations + index * 12u;
        const uptr clip = load_u32(location + 8u);
        if (clip == 0u) continue;
        if (!resource_object(clip, 0x34u)) return false;
        const uptr vtable = load_u32(clip);
        if ((vtable & 3u) != 0u || !runtime_data_range(vtable, 4u) ||
            load_u32(clip + 8u) != load_u32(location)) return false;
        if (count >= first && count - first < capacity && clips != nullptr)
            clips[count - first] = clip;
        ++count;
    }
    return true;
}

bool actor_animation_count(uptr actor, u32& count) {
    return actor_animations(actor, 0u, nullptr, 0u, count);
}

bool entity_snapshots_equal(const EntitySnapshot& left, const EntitySnapshot& right) {
    if (left.state != right.state || left.actor_count != right.actor_count ||
        left.record_count != right.record_count || left.first != right.first) {
        return false;
    }
    for (u32 index = 0u; index != left.record_count; ++index) {
        const EntityRecord& a = left.records[index];
        const EntityRecord& b = right.records[index];
        if (a.actor != b.actor || a.actor_hash != b.actor_hash ||
            a.animation_count != b.animation_count || a.actor_handle != b.actor_handle) {
            return false;
        }
    }
    return true;
}

EntitySnapshot scan_entity_animations(u32 first = 0u) {
    EntitySnapshot result{};
    result.first = first;
    if (!engine::gameplay_rel_ready() || engine::world() == nullptr) {
        result.state = ScanState::game_not_ready;
        return result;
    }

    using FindEntities = s32 (*)(s32);
    function_at<FindEntities>(find_entities_entry)(1);

    result.state = ScanState::ready;
    const uptr list = load_u32(found_entities_slot);
    if (!aligned_main_range(list, 8u)) {
        result.state = ScanState::invalid;
        return result;
    }
    const uptr sentinel = load_u32(list + 4u);
    if (!aligned_main_range(sentinel, 12u)) {
        result.state = ScanState::invalid;
        return result;
    }

    uptr previous = sentinel;
    uptr node = load_u32(sentinel);
    for (u32 visited = 0u; node != sentinel; ++visited) {
        if (visited >= maximum_entity_nodes || !aligned_main_range(node, 12u) ||
            load_u32(node + 4u) != previous) {
            result.state = ScanState::invalid;
            return result;
        }
        const uptr next = load_u32(node);
        const uptr entity = load_u32(node + 8u);
        if ((next != sentinel && !aligned_main_range(next, 12u)) ||
            !resource_object(entity, 0x14u)) {
            result.state = ScanState::invalid;
            return result;
        }

        // The native search also returns authored/static entities in live
        // paged packs. Validate and skip those; only MEM1 actors are mutable.
        if (is_actor(entity) == ActorTest::yes) {
            u32 animation_count = 0u;
            if (actor_animation_count(entity, animation_count) && animation_count != 0u) {
                ++result.actor_count;
                if (result.actor_count > first && result.record_count != maximum_entity_rows) {
                    EntityRecord& record = result.records[result.record_count++];
                    record.actor = entity;
                    record.actor_hash = load_u32(entity + 0x10u);
                    record.animation_count = animation_count;
                    record.actor_handle = load_u32(entity + 0x1Cu);
                }
            }
        }

        previous = node;
        node = next;
    }
    if (load_u32(sentinel + 4u) != previous) {
        result.state = ScanState::invalid;
    }
    return result;
}

void format_animation_count(const menu::Entry& entry, char* output, u32 capacity) {
    menu::format_s32(output, capacity, static_cast<s32>(entry.id));
    menu::append_text(output, capacity, entry.id == 1u ? " anim" : " anims");
}

void format_entity_state(const menu::Entry&, char* output, u32 capacity) {
    if (entity_snapshot.state == ScanState::game_not_ready) {
        menu::copy_text(output, capacity, "game not ready");
    } else if (entity_snapshot.state == ScanState::invalid) {
        menu::copy_text(output, capacity, "validation failed");
    } else {
        menu::copy_text(output, capacity, "0 anims");
    }
}

void rebuild_entity_menu(menu::Menu& destination) {
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);

    if (entity_snapshot.state == ScanState::ready && entity_snapshot.record_count != 0u) {
        add_page(destination, animation_root_page, entity_snapshot.actor_count, maximum_entity_rows);
        for (u32 index = 0u; index != entity_snapshot.record_count; ++index) {
            const EntityRecord& record = entity_snapshot.records[index];
            make_hash_label(entity_labels[index], sizeof(entity_labels[index]), record.actor_hash);
            menu::Entry* entry = menu::add(destination, entity_labels[index],
                                           animation_detail_menu != nullptr
                                               ? menu::EntryType::submenu
                                               : menu::EntryType::read_only);
            if (entry != nullptr) {
                entry->target = reinterpret_cast<void*>(record.actor);
                entry->id = record.animation_count;
                entry->minimum = static_cast<s32>(record.actor_handle);
                entry->formatter = format_animation_count;
                entry->submenu = animation_detail_menu;
            }
        }
    } else {
        const char* label = entity_snapshot.state == ScanState::invalid
                                ? "Entity list validation failed"
                                : (entity_snapshot.state == ScanState::game_not_ready
                                       ? "Entity scan"
                                       : "No actors with loaded animations");
        menu::Entry* entry = menu::add(destination, label, menu::EntryType::read_only);
        if (entry != nullptr) {
            entry->formatter = format_entity_state;
        }
    }

    if (destination.count != 0u) {
        destination.selected = old_selection < destination.count
                                   ? old_selection
                                   : static_cast<u16>(destination.count - 1u);
    }
}

void refresh_entity_animations(menu::Menu& destination) {
    const uptr world = reinterpret_cast<uptr>(engine::world());
    if (world != entity_last_world) {
        entity_last_world = world;
        entity_refresh_delay = 0u;
        animation_root_page = 0u;
    }
    if (entity_snapshot.first != animation_root_page * maximum_entity_rows) entity_refresh_delay = 0u;
    if (entity_refresh_delay != 0u) {
        --entity_refresh_delay;
        return;
    }
    entity_refresh_delay = 30u;

    EntitySnapshot next = scan_entity_animations(animation_root_page * maximum_entity_rows);
    const u32 last = next.actor_count == 0u ? 0u : (next.actor_count - 1u) / maximum_entity_rows;
    if (animation_root_page > last) {
        animation_root_page = last;
        next = scan_entity_animations(animation_root_page * maximum_entity_rows);
    }
    if (!entity_snapshots_equal(next, entity_snapshot)) {
        entity_snapshot = next;
        rebuild_entity_menu(destination);
    }
}

bool slab_pointer(uptr slab, uptr headers) {
    return aligned_main_range(headers, slab_header_count * slab_header_size) &&
           slab >= headers && slab < headers + slab_header_count * slab_header_size &&
           (slab - headers) % slab_header_size == 0u;
}

bool scan_slab_list(uptr list,
                    s32 bucket,
                    uptr headers,
                    u8* seen,
                    s32& slab_count,
                    s32& allocated_count,
                    s32& free_count) {
    slab_count = 0;
    allocated_count = 0;
    free_count = 0;
    if (!aligned_main_range(list, slab_list_size)) {
        return false;
    }

    const uptr first = load_u32(list);
    const uptr last = load_u32(list + 4u);
    const s32 declared_size = load_s32(list + 8u);
    if (declared_size < 0 || declared_size > static_cast<s32>(slab_header_count)) {
        return false;
    }
    if (declared_size == 0) {
        return first == 0u && last == 0u;
    }
    if (!slab_pointer(first, headers) || !slab_pointer(last, headers)) {
        return false;
    }

    uptr previous = 0u;
    uptr slab = first;
    for (s32 index = 0; index != declared_size; ++index) {
        if (!slab_pointer(slab, headers) || load_u32(slab) != slab_begin_sentry ||
            load_u32(slab + 0x20u) != slab_end_sentry ||
            load_u32(slab + 0x18u) != previous || load_u32(slab + 0x1Cu) != list) {
            return false;
        }

        const u32 header_index = (slab - headers) / slab_header_size;
        if (seen[header_index] != 0u) {
            return false;
        }
        seen[header_index] = 1u;

        if (bucket >= 0) {
            const u32 object_size = load_u16(slab + 0x0Au);
            const u32 total_objects = load_u16(slab + 0x0Cu);
            const u32 allocated_objects = load_u16(slab + 0x0Eu);
            if (object_size != static_cast<u32>(bucket + 1) * 4u ||
                total_objects == 0u || total_objects > 480u ||
                allocated_objects > total_objects) {
                return false;
            }
            allocated_count += static_cast<s32>(allocated_objects);
            free_count += static_cast<s32>(total_objects - allocated_objects);
        }

        ++slab_count;
        previous = slab;
        slab = load_u32(slab + 0x14u);
        if (index + 1 != declared_size && slab == 0u) {
            return false;
        }
    }
    return slab == 0u && previous == last && slab_count == declared_size;
}

SlabSnapshot scan_slabs() {
    SlabSnapshot result{};
    result.initialized = load_u32(slab_initialized) != 0u;
    if (!result.initialized) {
        result.valid = true;
        return result;
    }

    const uptr partial_lists = load_u32(slab_partial_lists_slot);
    const uptr full_lists = load_u32(slab_full_lists_slot);
    const uptr free_list = load_u32(slab_free_list_slot);
    const uptr headers = load_u32(slab_headers_slot);
    if (!aligned_main_range(partial_lists, slab_bucket_count * slab_list_size) ||
        !aligned_main_range(full_lists, slab_bucket_count * slab_list_size) ||
        !aligned_main_range(free_list, slab_list_size) ||
        !aligned_main_range(headers, slab_header_count * slab_header_size)) {
        return result;
    }

    u8 seen[slab_header_count]{};
    s32 ignored_allocated = 0;
    s32 ignored_free = 0;
    if (!scan_slab_list(free_list, -1, headers, seen, result.free_slabs,
                        ignored_allocated, ignored_free)) {
        return result;
    }
    result.total_slabs = result.free_slabs;

    for (u32 bucket = 0u; bucket != slab_bucket_count; ++bucket) {
        s32 partial_allocated = 0;
        s32 partial_free = 0;
        if (!scan_slab_list(partial_lists + bucket * slab_list_size,
                            static_cast<s32>(bucket), headers, seen,
                            result.partial_slabs[bucket], partial_allocated,
                            partial_free)) {
            return result;
        }

        s32 full_allocated = 0;
        s32 full_free = 0;
        if (!scan_slab_list(full_lists + bucket * slab_list_size,
                            static_cast<s32>(bucket), headers, seen,
                            result.full_slabs[bucket], full_allocated, full_free)) {
            return result;
        }

        result.allocated_objects[bucket] = partial_allocated + full_allocated;
        result.free_objects[bucket] = partial_free + full_free;
        result.total_slabs += result.partial_slabs[bucket] + result.full_slabs[bucket];
        if (result.total_slabs > static_cast<s32>(slab_header_count)) {
            return result;
        }
    }

    result.valid = true;
    return result;
}

void refresh_slabs(menu::Menu&) {
    slab_snapshot = scan_slabs();
}

void format_slab_value(const menu::Entry& entry, char* output, u32 capacity) {
    if (!slab_snapshot.initialized) {
        menu::copy_text(output, capacity, "not initialized");
        return;
    }
    if (!slab_snapshot.valid) {
        menu::copy_text(output, capacity, "validation failed");
        return;
    }

    const u32 metric = entry.id >> 8u;
    const u32 bucket = entry.id & 0xFFu;
    s32 value = 0;
    switch (metric) {
    case slab_total:
        value = slab_snapshot.total_slabs;
        break;
    case slab_free:
        value = slab_snapshot.free_slabs;
        break;
    case slab_full:
        value = bucket < slab_bucket_count ? slab_snapshot.full_slabs[bucket] : 0;
        break;
    case slab_partial:
        value = bucket < slab_bucket_count ? slab_snapshot.partial_slabs[bucket] : 0;
        break;
    case object_allocated:
        value = bucket < slab_bucket_count ? slab_snapshot.allocated_objects[bucket] : 0;
        break;
    case object_free:
        value = bucket < slab_bucket_count ? slab_snapshot.free_objects[bucket] : 0;
        break;
    default:
        menu::copy_text(output, capacity, "invalid metric");
        return;
    }
    menu::format_s32(output, capacity, value);
}

void add_slab_value(menu::Menu& destination, const char* label, SlabMetric metric, u32 bucket) {
    menu::Entry* entry = menu::add(destination, label, menu::EntryType::read_only);
    if (entry != nullptr) {
        entry->id = (static_cast<u32>(metric) << 8u) | (bucket & 0xFFu);
        entry->formatter = format_slab_value;
    }
}

void restore_selection(menu::Menu& destination, u16 selection) {
    if (destination.count != 0u) {
        destination.selected = selection < destination.count
                                   ? selection
                                   : static_cast<u16>(destination.count - 1u);
    }
}

const menu::Entry* owner_entry(const menu::Menu& destination) {
    const menu::Menu* parent = destination.parent;
    return parent != nullptr && parent->selected < parent->count
               ? &parent->entries[parent->selected] : nullptr;
}

void empty_detail(menu::Menu& destination, const char* label) {
    menu::clear_entries(destination);
    menu::add(destination, label, menu::EntryType::read_only);
}

bool ai_owner_live() {
    if (!engine::interactive_world_ready() ||
        reinterpret_cast<uptr>(engine::world()) != selected_ai_world) return false;
    const AiSnapshot live = scan_ai(0u, selected_ai_core, selected_ai_handle);
    return live.state == ScanState::ready && live.owner_found &&
           aligned_main_range(selected_ai_core, 0x64u) &&
           load_u32(selected_ai_core + 0x58u) == selected_ai_actor;
}

bool parameter_array(uptr block, uptr& values, u32& count) {
    values = 0u;
    count = 0u;
    if (!aligned_main_range(block, 12u)) return false;
    const uptr array = load_u32(block + 4u);
    if (array == 0u) return true;
    if (!aligned_main_range(array, 0x18u)) return false;
    count = load_u32(array + 4u);
    values = load_u32(array + 8u);
    return count <= 4096u &&
           (count == 0u || aligned_main_range(values, count * 4u));
}

bool parameter_block_live(uptr block) {
    if (!ai_owner_live()) return false;
    if (block == selected_ai_core + 0x44u) return true;
    const uptr nodes = load_u32(selected_ai_core + 0x54u);
    if (!aligned_main_range(nodes, 12u)) return false;
    const u32 node_count = load_u32(nodes + 4u);
    const uptr node_values = load_u32(nodes + 8u);
    if (node_count > 4096u ||
        (node_count != 0u && !aligned_main_range(node_values, node_count * 4u))) return false;
    for (u32 index = 0u; index != node_count; ++index) {
        const uptr node = load_u32(node_values + index * 4u);
        if (node + 0x10u == block && aligned_main_range(node, 0x1Cu) &&
            load_u32(node + 8u) == selected_ai_core) return true;
    }
    return false;
}

bool parameter_live(uptr parameter) {
    if (!parameter_block_live(selected_parameter_block)) return false;
    uptr values = 0u;
    u32 count = 0u;
    if (!parameter_array(selected_parameter_block, values, count)) return false;
    for (u32 index = 0u; index != count; ++index)
        if (load_u32(values + index * 4u) == parameter)
            return aligned_main_range(parameter, 12u);
    return false;
}

void append_float(char* output, u32 capacity, uptr value) {
    char number[32];
    menu::format_float(number, sizeof(number), *reinterpret_cast<const float*>(value));
    menu::append_text(output, capacity, number);
}

void parameter_formatter(const menu::Entry& entry, char* output, u32 capacity) {
    const uptr parameter = reinterpret_cast<uptr>(entry.target);
    if (!parameter_live(parameter)) {
        menu::copy_text(output, capacity, "owner expired");
        return;
    }
    output[0] = '\0';
    const u32 type = load_u32(parameter + 4u);
    const uptr value = load_u32(parameter);
    switch (type) {
    case 0u: append_float(output, capacity, parameter); break;
    case 1u: menu::format_s32(output, capacity, load_s32(parameter)); break;
    case 2u: make_hash_label(output, capacity, value); break;
    case 3u:
        if (!copy_runtime_string(output, capacity, reinterpret_cast<const char*>(value)))
            menu::copy_text(output, capacity, value == 0u ? "<NULL>" : "invalid string");
        break;
    case 4u:
    case 5u: {
        const u32 components = type == 4u ? 3u : 2u;
        if (!resource_object(value, components * 4u)) {
            menu::copy_text(output, capacity, value == 0u ? "<NULL>" : "invalid value");
            break;
        }
        for (u32 component = 0u; component != components; ++component) {
            if (component != 0u) menu::append_text(output, capacity, ", ");
            append_float(output, capacity, value + component * 4u);
        }
        break;
    }
    case 6u:
    case 7u: append_hex_u32(output, capacity, value); break;
    default: menu::copy_text(output, capacity, "unsupported type"); break;
    }
}

void parameter_handler(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::increment && command != menu::Command::decrement) return;
    const uptr parameter = reinterpret_cast<uptr>(entry.target);
    if (!parameter_live(parameter)) {
        menu::set_status("AI parameter owner expired; reopen AI");
        return;
    }
    const s32 delta = command == menu::Command::increment
                          ? menu::change_multiplier() : -menu::change_multiplier();
    const u32 type = load_u32(parameter + 4u);
    if (type == 0u) {
        // Ignore NaN/infinity rather than converting a damaged value to zero.
        if ((load_u32(parameter) & 0x7F800000u) == 0x7F800000u) return;
        const float old_value = *reinterpret_cast<const float*>(parameter);
        const float next = old_value + static_cast<float>(delta) * 0.3f;
        if (next == next && next >= -3.4028234e38f && next <= 3.4028234e38f)
            *reinterpret_cast<volatile float*>(parameter) = next;
    } else if (type == 1u) {
        const s32 old_value = load_s32(parameter);
        if ((delta > 0 && old_value <= 2147483647 - delta) ||
            (delta < 0 && old_value >= (-2147483647 - 1) - delta))
            *reinterpret_cast<volatile s32*>(parameter) = old_value + delta;
    } else return;
    // Native add_param (7F07E95C) writes only this frame stamp when changing
    // an existing scalar. +8 is an ownership flag, not a dirty bit.
    const uptr world = reinterpret_cast<uptr>(engine::world());
    if (aligned_main_range(world, 0x150u))
        *reinterpret_cast<volatile u32*>(selected_parameter_block) = load_u32(world + 0x14Cu);
}

// The retail native file API ends at an unavailable file-server stub. The
// companion persists this complete, immutable snapshot instead. Raw words
// accompany displayed numbers so floating-point formatting loses no data.
struct ExportWriter {
    char* output;
    u32 capacity;
    u32 length;
    bool valid;

    void byte(char value) {
        if (!valid || length + 1u >= capacity) { valid = false; return; }
        output[length++] = value;
        output[length] = '\0';
    }
    void text(const char* value) {
        while (valid && *value != '\0') byte(*value++);
    }
    void hex(u32 value) {
        static const char digits[] = "0123456789ABCDEF";
        text("0x");
        for (u32 index = 0u; index != 8u; ++index)
            byte(digits[(value >> ((7u - index) * 4u)) & 15u]);
    }
    void integer(s32 value) {
        char number[16];
        menu::format_s32(number, sizeof(number), value);
        text(number);
    }
    void floating(uptr value) {
        char number[32];
        menu::format_float(number, sizeof(number), *reinterpret_cast<const float*>(value));
        text(number);
    }
    void string(uptr address) {
        if (address == 0u) { text("<NULL>"); return; }
        uptr end = 0u;
        if (!resource_memory::readable_extent(address, end)) { valid = false; return; }
        static const char digits[] = "0123456789ABCDEF";
        byte('"');
        for (uptr current = address; valid && current < end; ++current) {
            const u8 value = *reinterpret_cast<volatile const u8*>(current);
            if (value == 0u) { byte('"'); return; }
            switch (value) {
            case '\\': text("\\\\"); break;
            case '"': text("\\\""); break;
            case '\n': text("\\n"); break;
            case '\r': text("\\r"); break;
            case '\t': text("\\t"); break;
            default:
                if (value >= 0x20u && value < 0x7Fu) byte(static_cast<char>(value));
                else { text("\\x"); byte(digits[value >> 4u]); byte(digits[value & 15u]); }
                break;
            }
        }
        // No terminator inside the live owner, or no room for the full value.
        valid = false;
    }
};

bool serialize_selected_ai_block(char* output, u32 capacity, u32& output_length) {
    output_length = 0u;
    if (output == nullptr || capacity == 0u) return false;
    output[0] = '\0';
    const uptr block = selected_parameter_block;
    uptr values = 0u;
    u32 count = 0u;
    if (!parameter_block_live(block) || !parameter_array(block, values, count)) return false;
    ExportWriter writer{output, capacity, 0u, true};
    writer.text("AI_PARAMETER_BLOCK v1\nactor_hash=");
    writer.hex(load_u32(selected_ai_actor + 0x10u));
    writer.text(" actor_handle="); writer.hex(selected_ai_handle);
    writer.text(" block="); writer.hex(static_cast<u32>(block));
    writer.text(" count="); writer.integer(static_cast<s32>(count)); writer.byte('\n');
    static const char* const types[] = {
        "float", "int", "string_hash", "fixedstring", "vector3d",
        "float_variance", "entity", "pointer"
    };
    for (u32 index = 0u; writer.valid && index != count; ++index) {
        const uptr parameter = load_u32(values + index * 4u);
        if (!aligned_main_range(parameter, 12u)) { writer.valid = false; break; }
        const u32 type = load_u32(parameter + 4u);
        const uptr value = load_u32(parameter);
        if (type >= 8u) { writer.valid = false; break; }
        writer.integer(static_cast<s32>(index)); writer.text(" name_hash=");
        writer.hex(load_u32(parameter + 8u)); writer.text(" type="); writer.text(types[type]);
        writer.text(" raw="); writer.hex(static_cast<u32>(value)); writer.text(" value=");
        switch (type) {
        case 0u: writer.floating(parameter); break;
        case 1u: writer.integer(load_s32(parameter)); break;
        case 3u: writer.string(value); break;
        case 4u:
        case 5u: {
            const u32 components = type == 4u ? 3u : 2u;
            if (!resource_object(value, components * 4u)) { writer.valid = false; break; }
            for (u32 component = 0u; component != components; ++component) {
                if (component != 0u) writer.text(", ");
                writer.floating(value + component * 4u);
                writer.byte('['); writer.hex(load_u32(value + component * 4u)); writer.byte(']');
            }
            break;
        }
        default: writer.hex(static_cast<u32>(value)); break;
        }
        writer.byte('\n');
    }
    uptr final_values = 0u;
    u32 final_count = 0u;
    if (!writer.valid || !parameter_block_live(block) ||
        !parameter_array(block, final_values, final_count) ||
        values != final_values || count != final_count) {
        output[0] = '\0';
        return false;
    }
    output_length = writer.length;
    return output_length != 0u;
}

void export_ai_block(menu::Entry&, menu::Command command) {
    if (command == menu::Command::activate)
        ai_export::request_export(serialize_selected_ai_block);
}

void page_formatter(const menu::Entry& entry, char* output, u32 capacity) {
    const auto* page = static_cast<const u32*>(entry.target);
    menu::format_s32(output, capacity, static_cast<s32>(*page + 1u));
    menu::append_text(output, capacity, "/");
    char total[16];
    menu::format_s32(total, sizeof(total), static_cast<s32>(entry.maximum + 1));
    menu::append_text(output, capacity, total);
}

void page_handler(menu::Entry& entry, menu::Command command) {
    auto* page = static_cast<u32*>(entry.target);
    if (command == menu::Command::decrement && *page != 0u) --*page;
    if ((command == menu::Command::increment || command == menu::Command::activate) &&
        *page < static_cast<u32>(entry.maximum)) ++*page;
}

void add_page(menu::Menu& destination, u32& page, u32 count, u32 page_size) {
    const u32 last = count == 0u ? 0u : (count - 1u) / page_size;
    if (page > last) page = last;
    menu::Entry* entry = menu::add(destination, "Page (left/right)", menu::EntryType::custom);
    if (entry != nullptr) {
        entry->target = &page;
        entry->maximum = static_cast<s32>(last);
        entry->handler = page_handler;
        entry->formatter = page_formatter;
    }
}

void refresh_ai_parameters(menu::Menu& destination) {
    const menu::Entry* owner = owner_entry(destination);
    const uptr block = owner != nullptr ? reinterpret_cast<uptr>(owner->target) : 0u;
    if (block != selected_parameter_block) { selected_parameter_block = block; parameter_page = 0u; }
    uptr values = 0u;
    u32 count = 0u;
    if (!parameter_block_live(block) || !parameter_array(block, values, count)) {
        empty_detail(destination, "AI parameter owner expired");
        return;
    }
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);
    add_page(destination, parameter_page, count, 126u);
    menu::Entry* export_entry = menu::add(destination, "--Export this block--", menu::EntryType::action);
    if (export_entry != nullptr) export_entry->handler = export_ai_block;
    for (u32 row = 0u, index = parameter_page * 126u;
         row != 126u && index < count; ++row, ++index) {
        const uptr parameter = load_u32(values + index * 4u);
        if (!aligned_main_range(parameter, 12u)) continue;
        make_hash_label(ai_parameter_labels[row], sizeof(ai_parameter_labels[row]), load_u32(parameter + 8u));
        menu::Entry* entry = menu::add(destination, ai_parameter_labels[row], menu::EntryType::custom);
        if (entry != nullptr) {
            entry->target = reinterpret_cast<void*>(parameter);
            entry->formatter = parameter_formatter;
            entry->handler = parameter_handler;
        }
    }
    restore_selection(destination, old_selection);
}

void refresh_ai_details(menu::Menu& destination) {
    const menu::Entry* owner = owner_entry(destination);
    const uptr core = owner != nullptr ? reinterpret_cast<uptr>(owner->target) : 0u;
    const u32 handle = owner != nullptr ? static_cast<u32>(owner->minimum) : 0u;
    if (core != selected_ai_core || handle != selected_ai_handle ||
        reinterpret_cast<uptr>(engine::world()) != selected_ai_world) {
        selected_ai_core = core;
        selected_ai_world = reinterpret_cast<uptr>(engine::world());
        selected_ai_handle = handle;
        selected_ai_actor = aligned_main_range(core, 0x64u) ? load_u32(core + 0x58u) : 0u;
        selected_parameter_block = 0u;
        inode_page = 0u;
    }
    if (!ai_owner_live()) { empty_detail(destination, "AI core owner expired"); return; }
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);
    menu::Entry* entry = menu::add(destination, "-Core params", menu::EntryType::submenu);
    if (entry != nullptr) { entry->target = reinterpret_cast<void*>(core + 0x44u); entry->submenu = ai_parameter_menu; }
    const uptr nodes = load_u32(core + 0x54u);
    if (nodes != 0u && aligned_main_range(nodes, 12u)) {
        const u32 count = load_u32(nodes + 4u);
        const uptr values = load_u32(nodes + 8u);
        if (count <= 4096u && (count == 0u || aligned_main_range(values, count * 4u))) {
            add_page(destination, inode_page, count, 62u);
            for (u32 row = 0u, index = inode_page * 62u;
                 row < 62u && index < count; ++row, ++index) {
                const uptr node = load_u32(values + index * 4u);
                if (!aligned_main_range(node, 0x1Cu) || load_u32(node + 8u) != core) continue;
                make_hash_label(ai_detail_labels[row], sizeof(ai_detail_labels[row]), load_u32(node + 4u));
                menu::append_text(ai_detail_labels[row], sizeof(ai_detail_labels[row]), " inode params");
                entry = menu::add(destination, ai_detail_labels[row], menu::EntryType::submenu);
                if (entry != nullptr) { entry->target = reinterpret_cast<void*>(node + 0x10u); entry->submenu = ai_parameter_menu; }
            }
        }
    }
    restore_selection(destination, old_selection);
}

bool actor_live(uptr actor, uptr world, s32 filter) {
    if (!engine::interactive_world_ready() || reinterpret_cast<uptr>(engine::world()) != world) return false;
    function_at<s32 (*)(s32)>(find_entities_entry)(filter);
    const uptr list = load_u32(found_entities_slot);
    if (!aligned_main_range(list, 8u)) return false;
    const uptr sentinel = load_u32(list + 4u);
    if (!aligned_main_range(sentinel, 12u)) return false;
    uptr node = load_u32(sentinel);
    for (u32 visited = 0u; node != sentinel; ++visited) {
        if (visited >= maximum_entity_nodes || !aligned_main_range(node, 12u)) return false;
        if (load_u32(node + 8u) == actor) return true;
        node = load_u32(node);
    }
    return false;
}

void play_animation(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (menu::interaction_is_locked()) return;
    const uptr actor = reinterpret_cast<uptr>(entry.target);
    if (!actor_live(actor, selected_animation_world, 1) || is_actor(actor) != ActorTest::yes ||
        load_u32(actor + 0x1Cu) != static_cast<u32>(entry.minimum)) {
        menu::set_status("Animation actor expired; reopen Entity Animations"); return;
    }
    // Re-resolve the hash from the actor's current loaded bank before calling
    // the native controller; its missing-animation path dereferences null.
    uptr clip[1]{};
    u32 count = 0u;
    if (!actor_animations(actor, entry.reserved, clip, 1u, count) || clip[0] == 0u ||
        load_u32(clip[0] + 8u) != entry.id) {
        menu::set_status("Animation bank changed; reopen actor"); return;
    }
    const uptr context = load_u32(actor + 0xBCu);
    if (!aligned_main_range(context, 0x1Cu)) return;
    menu::set_interaction_locked(true);
    function_at<void* (*)(void*)>(0x8015CCE8u)(reinterpret_cast<void*>(context));
    if (load_u32(actor + 0x74u) == 0u)
        function_at<void (*)(void*, u32, void*)>(0x8010523Cu)(reinterpret_cast<void*>(actor), 0u, nullptr);
    const uptr controller = load_u32(actor + 0x74u);
    const char* status = "Actor has no animation controller";
    bool started = false;
    do {
        if (!aligned_main_range(controller, 0x14u) || load_u32(controller) != actor) break;
        const u32 hash = entry.id;
        // This is the same resolver used by native play. A bank entry alone
        // does not guarantee ALS lookup or skeleton compatibility; native
        // play reports a fatal error for either missing or incompatible data.
        const uptr resolved = reinterpret_cast<uptr>(
            function_at<void* (*)(const u32*, void*, void*)>(0x800A6BB4u)(
                &hash, reinterpret_cast<void*>(load_u32(controller + 8u)),
                reinterpret_cast<void*>(actor)));
        status = "Animation is unavailable in the actor's resource context";
        if (!resource_object(resolved, 0x34u)) break;
        const uptr skeleton = load_u32(resolved + 0x30u);
        const uptr actor_skeleton = load_u32(controller + 4u);
        status = "Animation skeleton is unavailable";
        if (!resource_object(skeleton, 0x48u) || !resource_object(actor_skeleton, 0x48u)) break;
        u32 animation_type[8];
        for (u32 index = 0u; index != 8u; ++index)
            animation_type[index] = load_u32(skeleton + 0x28u + index * 4u);
        status = "Animation skeleton does not match this actor";
        if (!function_at<bool (*)(void*, const u32*)>(0x800A72B8u)(
                reinterpret_cast<void*>(controller), animation_type)) break;
        // GC CodeWarrior's speed virtual entry has signed this adjustment at
        // +A8 and the function at +AC. All five retail controller tables bind
        // this slot to nal_anim_controller::set_base_anim_speed (800A7FA8).
        const uptr vtable = load_u32(controller + 0x10u);
        status = "Animation controller has no supported speed setter";
        if ((vtable & 3u) != 0u || !runtime_data_range(vtable + 0xA8u, 8u) ||
            load_u32(vtable + 0xACu) != 0x800A7FA8u) break;
        const s16 adjustment = *reinterpret_cast<volatile const s16*>(vtable + 0xA8u);
        const long long adjusted = static_cast<long long>(controller) + adjustment;
        if (adjusted < main_ram_begin || adjusted >= main_ram_end ||
            !aligned_main_range(static_cast<uptr>(adjusted), 0x2Cu)) break;
        // CodeWarrior returns the 12-byte anim_ctrl_handle through r3.
        u32 handle[3]{};
        function_at<void (*)(void*, void*, const u32*, float, u32, bool)>(0x800A6EA0u)(
            handle, reinterpret_cast<void*>(controller), &hash, 0.0f, 32u, true);
        status = "Native animation playback returned no base-layer handle";
        if (handle[0] != 1u || handle[2] != controller) break;
        function_at<void (*)(void*, float)>(0x800A7FA8u)(
            reinterpret_cast<void*>(static_cast<uptr>(adjusted)), 1.0f);
        started = true;
        status = "Animation started at normal speed";
    } while (false);
    function_at<void* (*)()>(0x8015CD78u)();
    menu::set_interaction_locked(false);
    if (started) menu::close();
    menu::set_status(status);
}

void animation_label(char* output, u32 capacity, uptr clip) {
    // tlFixedString stores its hash at +8 and authored text in the next28
    // bytes. Retail hash dictionaries need not contain animation names.
    if (resource_object(clip, 0x28u) && capacity != 0u) {
        u32 length = 0u;
        for (; length != 28u && length + 1u < capacity; ++length) {
            const char value = *reinterpret_cast<volatile const char*>(clip + 0xCu + length);
            if (value == '\0') {
                output[length] = '\0';
                if (length != 0u) return;
                break;
            }
            if (value < 0x20 || value >= 0x7F) break;
            output[length] = value;
        }
    }
    make_hash_label(output, capacity, load_u32(clip + 8u));
}

void refresh_animation_details(menu::Menu& destination) {
    const menu::Entry* owner = owner_entry(destination);
    const uptr actor = owner != nullptr ? reinterpret_cast<uptr>(owner->target) : 0u;
    const u32 handle = owner != nullptr ? static_cast<u32>(owner->minimum) : 0u;
    if (actor != selected_animation_actor || handle != selected_animation_handle ||
        reinterpret_cast<uptr>(engine::world()) != selected_animation_world) {
        selected_animation_actor = actor; animation_page = 0u;
        selected_animation_world = reinterpret_cast<uptr>(engine::world());
        selected_animation_handle = handle;
    }
    uptr clips[127]{};
    u32 count = 0u;
    if (!actor_live(actor, selected_animation_world, 1) ||
        !aligned_main_range(actor, 0xC0u) || load_u32(actor + 0x1Cu) != handle ||
        !actor_animations(actor, 0u, nullptr, 0u, count)) {
        empty_detail(destination, "Animation actor expired"); return;
    }
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);
    add_page(destination, animation_page, count, 127u);
    if (!actor_animations(actor, animation_page * 127u, clips, 127u, count)) return;
    for (u32 row = 0u; row != 127u && clips[row] != 0u; ++row) {
        const u32 hash = load_u32(clips[row] + 8u);
        animation_label(animation_labels[row], sizeof(animation_labels[row]), clips[row]);
        menu::Entry* entry = menu::add(destination, animation_labels[row], menu::EntryType::action);
        if (entry != nullptr) {
            entry->target = reinterpret_cast<void*>(actor); entry->id = hash;
            entry->minimum = static_cast<s32>(handle);
            entry->reserved = static_cast<u16>(animation_page * 127u + row);
            entry->handler = play_animation;
        }
    }
    restore_selection(destination, old_selection);
}

bool variant_data(uptr actor, uptr& interface, uptr& values, u32& count) {
    if (!aligned_main_range(actor, 0x130u)) return false;
    interface = load_u32(actor + 0x12Cu);
    if (!aligned_main_range(interface, 0x58u) || load_u32(interface) != 0x80344CE0u ||
        load_u32(interface + 4u) != actor) return false;
    values = load_u32(interface + 0xCu);
    count = load_u16(interface + 0x10u);
    return count <= 4096u && (count == 0u || resource_object(values, count * 16u));
}

void apply_variant(menu::Entry& entry, menu::Command command) {
    if (command != menu::Command::activate) return;
    const uptr actor = reinterpret_cast<uptr>(entry.target);
    uptr interface = 0u, values = 0u;
    u32 count = 0u;
    if (!actor_live(actor, selected_variant_world, 256) || !variant_data(actor, interface, values, count) ||
        load_u32(actor + 0x1Cu) != static_cast<u32>(entry.minimum)) {
        menu::set_status("Variant actor expired; reopen Entity Variants"); return;
    }
    for (u32 index = 0u; index != count; ++index) {
        if (load_u32(values + index * 16u) == entry.id) {
            const u32 hash = entry.id;
            function_at<void (*)(void*, const u32*)>(0x800F7670u)(reinterpret_cast<void*>(interface), &hash);
            menu::set_status("Entity variant applied");
            return;
        }
    }
    menu::set_status("Variant no longer present");
}

void refresh_variant_details(menu::Menu& destination) {
    const menu::Entry* owner = owner_entry(destination);
    const uptr actor = owner != nullptr ? reinterpret_cast<uptr>(owner->target) : 0u;
    const u32 handle = owner != nullptr ? static_cast<u32>(owner->minimum) : 0u;
    if (actor != selected_variant_actor || handle != selected_variant_handle ||
        reinterpret_cast<uptr>(engine::world()) != selected_variant_world) {
        selected_variant_actor = actor; variant_page = 0u;
        selected_variant_world = reinterpret_cast<uptr>(engine::world());
        selected_variant_handle = handle;
    }
    uptr interface = 0u, values = 0u;
    u32 count = 0u;
    if (!actor_live(actor, selected_variant_world, 256) || !variant_data(actor, interface, values, count) ||
        load_u32(actor + 0x1Cu) != handle) {
        empty_detail(destination, "Variant actor expired"); return;
    }
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);
    add_page(destination, variant_page, count, 127u);
    for (u32 row = 0u, index = variant_page * 127u; row != 127u && index < count; ++row, ++index) {
        const u32 hash = load_u32(values + index * 16u);
        make_hash_label(variant_labels[row], sizeof(variant_labels[row]), hash);
        menu::Entry* entry = menu::add(destination, variant_labels[row], menu::EntryType::action);
        if (entry != nullptr) {
            entry->target = reinterpret_cast<void*>(actor); entry->id = hash;
            entry->minimum = static_cast<s32>(handle); entry->handler = apply_variant;
        }
    }
    restore_selection(destination, old_selection);
}

EntitySnapshot scan_entity_variants(u32 first = 0u) {
    EntitySnapshot result{};
    result.first = first;
    result.state = ScanState::game_not_ready;
    if (!engine::interactive_world_ready()) return result;
    result.state = ScanState::invalid;
    function_at<s32 (*)(s32)>(find_entities_entry)(256);
    const uptr list = load_u32(found_entities_slot);
    if (!aligned_main_range(list, 8u)) return result;
    const uptr sentinel = load_u32(list + 4u);
    if (!aligned_main_range(sentinel, 12u)) return result;
    uptr previous = sentinel;
    uptr node = load_u32(sentinel);
    for (u32 visited = 0u; node != sentinel; ++visited) {
        if (visited >= maximum_entity_nodes || !aligned_main_range(node, 12u) ||
            load_u32(node + 4u) != previous) return result;
        const uptr actor = load_u32(node + 8u);
        uptr interface = 0u, values = 0u;
        u32 count = 0u;
        if (variant_data(actor, interface, values, count) && count != 0u) {
            ++result.actor_count;
            if (result.actor_count > first && result.record_count < maximum_entity_rows) {
                EntityRecord& record = result.records[result.record_count++];
                record.actor = actor;
                record.actor_hash = load_u32(actor + 0x10u);
                record.actor_handle = load_u32(actor + 0x1Cu);
                record.animation_count = count;
            }
        }
        previous = node;
        node = load_u32(node);
    }
    if (load_u32(sentinel + 4u) == previous) result.state = ScanState::ready;
    return result;
}

void refresh_entity_variants(menu::Menu& destination) {
    const u16 old_selection = destination.selected;
    EntitySnapshot result = scan_entity_variants(variant_root_page * maximum_entity_rows);
    if (result.state != ScanState::ready) {
        empty_detail(destination, result.state == ScanState::game_not_ready
                                      ? "World not ready" : "Entity list invalid");
        return;
    }
    const u32 last = result.actor_count == 0u ? 0u : (result.actor_count - 1u) / maximum_entity_rows;
    if (variant_root_page > last) {
        variant_root_page = last;
        result = scan_entity_variants(variant_root_page * maximum_entity_rows);
    }
    menu::clear_entries(destination);
    add_page(destination, variant_root_page, result.actor_count, maximum_entity_rows);
    // Parent labels must survive rebuilding the detail menu beneath them.
    static char actor_labels[maximum_entity_rows][56];
    for (u32 row = 0u; row < result.record_count; ++row) {
        const EntityRecord& record = result.records[row];
        make_hash_label(actor_labels[row], sizeof(actor_labels[row]), record.actor_hash);
        menu::Entry* entry = menu::add(destination, actor_labels[row], menu::EntryType::submenu);
        if (entry != nullptr) {
            entry->target = reinterpret_cast<void*>(record.actor);
            entry->submenu = variant_detail_menu;
            entry->minimum = static_cast<s32>(record.actor_handle);
        }
    }
    if (result.actor_count == 0u) menu::add(destination, "No actors with variants", menu::EntryType::read_only);
    restore_selection(destination, old_selection);
}

}  // namespace

void configure_ai(menu::Menu& ai) {
    if (ai_detail_menu == nullptr) ai_detail_menu = menu::create("AI core", 64u, refresh_ai_details);
    if (ai_parameter_menu == nullptr) ai_parameter_menu = menu::create("AI parameters", 128u, refresh_ai_parameters);
    selected_ai_core = 0u;
    selected_parameter_block = 0u;
    ai_root_page = 0u;
    ai.refresh = refresh_ai;
    ai_snapshot = AiSnapshot{};
    ai_snapshot.state = ScanState::invalid;  // Force the initial menu build.
    const AiSnapshot initial = scan_ai();
    ai_snapshot = initial;
    rebuild_ai_menu(ai);
}

void configure_memory(menu::Menu& memory,
                      menu::Menu& script_memtrack,
                      menu::Menu& slabs,
                      menu::Menu& full_slabs,
                      menu::Menu& partial_slabs,
                      menu::Menu& allocated_objects,
                      menu::Menu& free_objects) {
    const u16 memory_selection = memory.selected;
    const u16 script_selection = script_memtrack.selected;
    const u16 slabs_selection = slabs.selected;
    const u16 full_selection = full_slabs.selected;
    const u16 partial_selection = partial_slabs.selected;
    const u16 allocated_selection = allocated_objects.selected;
    const u16 free_selection = free_objects.selected;

    menu::clear_entries(memory);
    menu::clear_entries(script_memtrack);
    menu::clear_entries(slabs);
    menu::clear_entries(full_slabs);
    menu::clear_entries(partial_slabs);
    menu::clear_entries(allocated_objects);
    menu::clear_entries(free_objects);

    slabs.refresh = refresh_slabs;
    full_slabs.refresh = refresh_slabs;
    partial_slabs.refresh = refresh_slabs;
    allocated_objects.refresh = refresh_slabs;
    free_objects.refresh = refresh_slabs;

    menu::add_submenu(memory, script_memtrack);
    menu::Entry* entry = menu::add(memory, "Dump MemTrack Data Since Last Checkpoint",
                                   menu::EntryType::action);
    if (entry != nullptr) {
        entry->handler = stripped_memtrack_handler;
        entry->formatter = format_unavailable;
    }
    entry = menu::add(memory, "Set MemTrack Checkpoint", menu::EntryType::action);
    if (entry != nullptr) {
        entry->handler = stripped_memtrack_handler;
        entry->formatter = format_unavailable;
    }
    menu::add_submenu(memory, slabs);

    entry = menu::add(script_memtrack, "Dump Memtrack Info", menu::EntryType::action);
    if (entry != nullptr) {
        entry->handler = stripped_script_memtrack_handler;
        entry->formatter = format_unavailable;
    }

    add_slab_value(slabs, "Total Slabs", slab_total, 0u);
    add_slab_value(slabs, "Free Slabs", slab_free, 0u);
    menu::add_submenu(slabs, full_slabs);
    menu::add_submenu(slabs, partial_slabs);
    menu::add_submenu(slabs, allocated_objects);
    menu::add_submenu(slabs, free_objects);

    for (u32 bucket = 0u; bucket != slab_bucket_count; ++bucket) {
        menu::format_s32(slab_labels[bucket], sizeof(slab_labels[bucket]),
                         static_cast<s32>((bucket + 1u) * 4u));
        menu::append_text(slab_labels[bucket], sizeof(slab_labels[bucket]), " byte");
        add_slab_value(full_slabs, slab_labels[bucket], slab_full, bucket);
        add_slab_value(partial_slabs, slab_labels[bucket], slab_partial, bucket);
        add_slab_value(allocated_objects, slab_labels[bucket], object_allocated, bucket);
        add_slab_value(free_objects, slab_labels[bucket], object_free, bucket);
    }

    slab_snapshot = scan_slabs();
    restore_selection(memory, memory_selection);
    restore_selection(script_memtrack, script_selection);
    restore_selection(slabs, slabs_selection);
    restore_selection(full_slabs, full_selection);
    restore_selection(partial_slabs, partial_selection);
    restore_selection(allocated_objects, allocated_selection);
    restore_selection(free_objects, free_selection);
}

void configure_entity_animations(menu::Menu& entity_animations) {
    if (animation_detail_menu == nullptr)
        animation_detail_menu = menu::create("Actor animations", 128u, refresh_animation_details);
    selected_animation_actor = 0u;
    animation_root_page = 0u;
    entity_animations.refresh = refresh_entity_animations;
    entity_refresh_delay = 0u;
    entity_last_world = 0xFFFFFFFFu;
    entity_snapshot = scan_entity_animations();
    entity_last_world = reinterpret_cast<uptr>(engine::world());
    entity_refresh_delay = 30u;
    rebuild_entity_menu(entity_animations);
}

void configure_entity_variants(menu::Menu& entity_variants) {
    if (variant_detail_menu == nullptr)
        variant_detail_menu = menu::create("Actor variants", 128u, refresh_variant_details);
    selected_variant_actor = 0u;
    variant_root_page = 0u;
    entity_variants.refresh = refresh_entity_variants;
    refresh_entity_variants(entity_variants);
}

}  // namespace usm::gc::diagnostics
