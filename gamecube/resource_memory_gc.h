#pragma once

#include "gc_types.h"

namespace usm::gc::resource_memory {

// Read-only mashed resources may live in the native ARAM pager, rather than
// MEM1. Never use this check to authorize writes to game/actor/VM state.
// The extent must belong to a READY slot still present in a live partition.
// Verified: partition lookup8015D968, slot directory801961F4 and resource
// translation8015FFD4. Directory+88 is the paged byte count (not an end offset).
namespace detail {
constexpr uptr manager_global = 0x8045F214u;
constexpr uptr main_begin = 0x80000000u;
constexpr uptr main_end = 0x81800000u;
constexpr uptr paged_begin = 0xF0000000u;
constexpr u32 partition_count = 8u;
constexpr u32 maximum_slots = 512u;

inline bool main_range(uptr address, u32 bytes) {
    return address >= main_begin && address < main_end && bytes <= main_end - address;
}
inline bool main_object(uptr address, u32 bytes) {
    return (address & 3u) == 0u && main_range(address, bytes);
}
inline u32 word(uptr address) { return *reinterpret_cast<const volatile u32*>(address); }

struct Owner {
    uptr manager;
    uptr partitions;
    uptr partition;
    uptr slot;
    uptr directory;
    uptr begin;
    uptr end;
    u32 partition_index;
    u32 slot_index;
};

inline bool slot_vector(uptr partition, uptr& begin, uptr& end) {
    if (!main_object(partition, 0x10u)) return false;
    begin = word(partition + 8u);
    end = word(partition + 12u);
    return begin != 0u && end >= begin && (end - begin) % 4u == 0u &&
           (end - begin) / 4u <= maximum_slots && main_object(begin, end - begin);
}

inline bool capture(uptr manager, uptr partitions, uptr partition,
                    u32 partition_index, u32 slot_index, Owner& owner) {
    uptr slots = 0u, end = 0u;
    if (!slot_vector(partition, slots, end) || slot_index >= (end - slots) / 4u) return false;
    const uptr slot = word(slots + slot_index * 4u);
    // GC slot has no PC vtable word: state+8, directory+18.
    if (!main_object(slot, 0x1Cu) || word(slot + 8u) != 4u) return false;
    const uptr directory = word(slot + 0x18u);
    if (!main_object(directory, 0x8Cu) || word(directory + 0x78u) != slot) return false;
    const uptr base = word(directory + 0x80u);
    const u32 bytes = word(directory + 0x88u);
    if (base < paged_begin || bytes == 0u || bytes > 0xFFFFFFFFu - base) return false;
    owner = {manager, partitions, partition, slot, directory, base, base + bytes,
             partition_index, slot_index};
    return true;
}

inline bool current(const Owner& owner) {
    if (word(manager_global) != owner.manager || !main_object(owner.manager, 4u) ||
        word(owner.manager) != owner.partitions ||
        !main_object(owner.partitions, partition_count * 4u) ||
        owner.partition_index >= partition_count ||
        word(owner.partitions + owner.partition_index * 4u) != owner.partition) return false;
    Owner latest{};
    return capture(owner.manager, owner.partitions, owner.partition,
                   owner.partition_index, owner.slot_index, latest) &&
           latest.slot == owner.slot && latest.directory == owner.directory &&
           latest.begin == owner.begin && latest.end == owner.end;
}

inline bool paged_extent(uptr address, uptr& end) {
    // A small positive cache avoids scanning every slot for every field or
    // string byte. Every hit revalidates membership, READY state and extent.
    static Owner cache[16];
    static u32 next;
    for (const Owner& owner : cache) {
        if (owner.begin <= address && address < owner.end && current(owner)) {
            end = owner.end;
            return true;
        }
    }
    const uptr manager = word(manager_global);
    if (!main_object(manager, 4u)) return false;
    const uptr partitions = word(manager);
    if (!main_object(partitions, partition_count * 4u)) return false;
    for (u32 index = 0u; index < partition_count; ++index) {
        const uptr partition = word(partitions + index * 4u);
        uptr slots = 0u, slots_end = 0u;
        if (!slot_vector(partition, slots, slots_end)) continue;
        for (u32 slot_index = 0u; slot_index < (slots_end - slots) / 4u; ++slot_index) {
            Owner owner{};
            if (capture(manager, partitions, partition, index, slot_index, owner) &&
                owner.begin <= address && address < owner.end) {
                cache[next++ % 16u] = owner;
                end = owner.end;
                return true;
            }
        }
    }
    return false;
}
} // namespace detail

inline bool readable_extent(uptr address, uptr& end) {
    end = 0u;
    if (detail::main_range(address, 1u)) { end = detail::main_end; return true; }
    return address >= detail::paged_begin && detail::paged_extent(address, end);
}

inline bool readable_range(const void* pointer, u32 bytes) {
    const uptr address = reinterpret_cast<uptr>(pointer);
    uptr end = 0u;
    return readable_extent(address, end) && bytes <= end - address;
}

inline bool readable_object(const void* pointer, u32 bytes) {
    return (reinterpret_cast<uptr>(pointer) & 3u) == 0u && readable_range(pointer, bytes);
}
} // namespace usm::gc::resource_memory
