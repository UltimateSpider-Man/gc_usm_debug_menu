#include "progression_gc.h"

#include "../gc_engine.h"
#include "../resource_memory_gc.h"
#include "character_spawn_gc.h"
#include "hero_select_gc.h"
#include "mission_level_gc.h"

namespace usm::gc::progression {
namespace {

constexpr uptr hash_setter = 0x8015BF68u;
constexpr uptr add_thread_with_parameters = 0x801B2EF0u;
constexpr uptr install_temp_string = 0x801B75A4u;
constexpr uptr find_function_by_name = 0x801B5FF0u;
constexpr uptr exec_map_slot = 0x8045F5B8u;  // r13 - 0x74C8
constexpr uptr run_instance_callbacks = 0x801B3508u;
constexpr u32 maximum_functions = 4096u;

struct Record {
    bool active;
    u32 serial;
    uptr instance;
    uptr object;
    uptr executable;
    u32 instance_name;
    u32 handler_hash;
    s32 function_index;
    float id;
    float value;
    float value_type;
    float minimum;
    float maximum;
    float step;
    float scale;
    char label[64];
    char handler[192];
};

Record records[recommended_capacity];
u32 next_serial;

bool readable(uptr pointer, u32 size) {
    return (pointer & 3u) == 0u && pointer >= 0x80000000u &&
           pointer < 0x81800000u && size <= 0x81800000u - pointer;
}

bool resource_readable(uptr pointer, u32 size) {
    return resource_memory::readable_object(reinterpret_cast<const void*>(pointer), size);
}

bool bytecode_readable(uptr pointer) {
    return (pointer & 1u) == 0u &&
           resource_memory::readable_range(reinterpret_cast<const void*>(pointer), 2u);
}

u32 load(uptr pointer) {
    return *reinterpret_cast<volatile const u32*>(pointer);
}

void store(uptr pointer, u32 value) {
    *reinterpret_cast<volatile u32*>(pointer) = value;
}

bool copy_string(char* output, u32 capacity, uptr source, bool truncate = false) {
    uptr end = 0u;
    if (capacity == 0u || !resource_memory::readable_extent(source, end)) return false;
    for (u32 index = 0u; index + 1u < capacity; ++index) {
        // Resolve ownership once, rather than scanning pack slots per byte.
        if (index >= end - source) return false;
        const char value = *reinterpret_cast<volatile const char*>(source + index);
        output[index] = value;
        if (value == '\0') return true;
        if (static_cast<u8>(value) < 0x20u || static_cast<u8>(value) >= 0x7Fu) return false;
    }
    output[capacity - 1u] = '\0';
    return truncate;
}

bool same_string(const char* left, const char* right) {
    for (u32 index = 0u;; ++index) {
        if (left[index] != right[index]) return false;
        if (left[index] == '\0') return true;
    }
}

u32 string_hash(const char* text) {
    u32 hash = 0u;
    reinterpret_cast<void (*)(u32*, const char*)>(hash_setter)(&hash, text);
    return hash;
}

bool finite(float value) {
    return value == value && value >= -3.4028234e38f && value <= 3.4028234e38f;
}

// vm_stack owns a 384-byte buffer, followed by buffer/SP/thread at180/184/188.
// Verified ctor801B7224 and init801B7280 use the object itself as the buffer.
bool arguments(void* stack, u32 bytes, uptr& args) {
    const uptr address = reinterpret_cast<uptr>(stack);
    if (!readable(address, 0x18Cu) || load(address + 0x180u) != address) return false;
    const uptr top = load(address + 0x184u);
    if ((top & 3u) != 0u || top < address + bytes || top > address + 0x180u) return false;
    args = top - bytes;
    return true;
}

void finish(void* stack, uptr args, bool result, u32 value = 0u) {
    if (result) store(args, value);
    store(reinterpret_cast<uptr>(stack) + 0x184u, args + (result ? 4u : 0u));
}

bool context(void* stack, uptr& instance, uptr& object) {
    const uptr thread = load(reinterpret_cast<uptr>(stack) + 0x188u);
    if (!readable(thread, 0x14u)) return false;
    instance = load(thread + 0xCu);
    if (!readable(instance, 0x48u)) return false;
    object = load(instance + 0x2Cu);
    const uptr executable = load(thread + 0x10u);
    return resource_readable(object, 0x34u) && resource_readable(executable, 0x24u) && load(executable) == object;
}

bool owner_live(const Record& record) {
    return record.active && readable(record.instance, 0x48u) &&
           load(record.instance + 0x2Cu) == record.object &&
           load(record.instance + 0xCu) == record.instance_name && resource_readable(record.object, 0x34u);
}

bool function_array(uptr object, uptr& functions, u32& count) {
    if (!resource_readable(object, 0x34u)) return false;
    count = load(object + 0x24u);
    functions = load(object + 0x20u);
    return count <= maximum_functions && (count == 0u || resource_readable(functions, count * 4u));
}

bool bind(Record& record, uptr instance, uptr object, const char* handler) {
    const u32 hash = string_hash(handler);
    uptr functions = 0u;
    u32 count = 0u;
    if (!function_array(object, functions, count)) return false;
    record.instance = instance;
    record.object = object;
    record.instance_name = load(instance + 0xCu);
    record.handler_hash = hash;
    record.executable = 0u;
    record.function_index = -1;
    menu::copy_text(record.handler, sizeof(record.handler), handler);
    for (u32 index = 0u; index != count; ++index) {
        const uptr executable = load(functions + index * 4u);
        if (resource_readable(executable, 0x24u) && load(executable) == object && load(executable + 4u) == hash) {
            record.executable = executable;
            record.function_index = static_cast<s32>(index);
            break;
        }
    }
    return true;
}

bool handler_live(const Record& record) {
    if (!owner_live(record) || record.function_index < 0) return false;
    uptr functions = 0u;
    u32 count = 0u;
    if (!function_array(record.object, functions, count) ||
        static_cast<u32>(record.function_index) >= count ||
        load(functions + static_cast<u32>(record.function_index) * 4u) != record.executable ||
        !resource_readable(record.executable, 0x24u)) return false;
    return load(record.executable) == record.object &&
           load(record.executable + 4u) == record.handler_hash &&
           (load(record.executable + 0x1Cu) & 2u) != 0u &&
           bytecode_readable(load(record.executable + 0x10u));
}

Record* from_handle(uptr handle) {
    // Script threads can retain an entry after it is destroyed. Tokens do not
    // alias a newly created entry when its fixed storage slot is reused.
    if (handle == 0u) return nullptr;
    for (Record& record : records)
        if (record.serial == handle && owner_live(record)) return &record;
    return nullptr;
}

Record* from_entry(const menu::Entry& entry) {
    const uptr address = reinterpret_cast<uptr>(entry.target);
    const uptr first = reinterpret_cast<uptr>(records);
    if (address < first || address >= first + sizeof(records) || (address - first) % sizeof(Record) != 0u)
        return nullptr;
    auto* record = reinterpret_cast<Record*>(address);
    return record->serial == entry.id && owner_live(*record) ? record : nullptr;
}

bool create(void* stack) {
    uptr args = 0u;
    if (!arguments(stack, 8u, args)) {
        menu::set_status("Progression: invalid VM stack");
        return true;
    }
    char label[64]{};
    char handler[192]{};
    uptr instance = 0u, object = 0u;
    if (!copy_string(label, sizeof(label), load(args), true) ||
        !copy_string(handler, sizeof(handler), load(args + 4u)) ||
        !context(stack, instance, object)) {
        menu::set_status("Progression: invalid script entry");
        finish(stack, args, true);
        return true;
    }
    Record* free = nullptr;
    const u32 hash = string_hash(handler);
    for (Record& record : records) {
        if (record.active && !owner_live(record)) record.active = false;
        if (!record.active) {
            if (free == nullptr) free = &record;
        } else if (record.instance == instance && record.handler_hash == hash && same_string(record.label, label)) {
            finish(stack, args, true);  // Re-running setup does not duplicate rows.
            return true;
        }
    }
    if (free == nullptr) menu::set_status("Progression entry pool exhausted");
    else {
        *free = Record{};
        free->serial = ++next_serial;
        if (free->serial == 0u) free->serial = ++next_serial;
        free->minimum = 0.0f;
        free->maximum = 1.0f;
        free->step = 0.1f;
        free->scale = 10.0f;
        menu::copy_text(free->label, sizeof(free->label), label);
        free->active = bind(*free, instance, object, handler);
    }
    // Retail and PC Progression creators return zero, rather than an entry
    // handle. The optional handler parameter is supplied at activation time.
    finish(stack, args, true);
    return true;
}

void format_record(const menu::Entry& entry, char* output, u32 capacity) {
    const Record* record = from_entry(entry);
    if (record == nullptr) menu::copy_text(output, capacity, "owner expired");
    else if (record->function_index < 0) menu::copy_text(output, capacity, "handler not found");
    else if (!handler_live(*record)) menu::copy_text(output, capacity, "handler expired");
    else if (load(record->executable + 0xCu) != 0u && load(record->executable + 0xCu) != 4u)
        menu::copy_text(output, capacity, "invalid handler parameters");
    else if (record->value_type != 0.0f && record->value_type != 1.0f)
        menu::format_float(output, capacity, record->value);
    else output[0] = '\0';
}

bool prepare_script_launch() {
    if (hero_select::busy() || mission_level::busy()) {
        menu::set_status("Progression waits for the active hero or mission transition");
        return false;
    }
    if (character_spawn::has_resources()) {
        character_spawn::prepare_transition();
        menu::close();
        menu::set_status("Character cleanup queued; select Progression again when complete");
        return false;
    }
    return true;
}

void activate(menu::Entry& entry, menu::Command command) {
    Record* record = from_entry(entry);
    if (record == nullptr) {
        menu::set_status("Progression script owner expired");
        return;
    }
    if (command == menu::Command::increment || command == menu::Command::decrement) {
        if (record->value_type == 0.0f || record->value_type == 1.0f || record->minimum > record->maximum) return;
        const float factor = menu::change_multiplier() == 1 ? 1.0f : record->scale;
        float value = record->value + record->step * factor *
                      (command == menu::Command::increment ? 1.0f : -1.0f);
        if (!finite(value)) return;
        if (value < record->minimum) value = record->minimum;
        if (value > record->maximum) value = record->maximum;
        record->value = value;
        return;
    }
    if (command != menu::Command::activate) return;
    if (!engine::interactive_world_ready() || !handler_live(*record)) {
        menu::set_status("Progression requires a live script handler");
        return;
    }
    const u32 size = load(record->executable + 0xCu);
    if (size != 0u && size != 4u) {
        menu::set_status("Progression handler requires unsupported arguments");
        return;
    }
    if (!prepare_script_launch()) return;
    const u32 handle = record->serial;
    using Add = void* (*)(void*, void*, const void*);
    void* thread = reinterpret_cast<Add>(add_thread_with_parameters)(
        reinterpret_cast<void*>(record->instance), reinterpret_cast<void*>(record->executable),
        size == 4u ? &handle : nullptr);
    if (thread == nullptr) menu::set_status("Progression could not start its script thread");
    else menu::close();
}

void build_entries(menu::Entry&, menu::Command command) {
    if (command != menu::Command::activate) return;
    if (!engine::interactive_world_ready() || !readable(load(exec_map_slot), 8u)) {
        menu::set_status("Progression requires loaded scripts");
        return;
    }
    // This exact function/signature is present in the supplied GC dictionary
    // (hash8AB458D6). Run the game's setup routine if a script did not call it.
    const u32 hash = string_hash("setup_progression_debug_menu()");
    const uptr executable = reinterpret_cast<uptr>(
        reinterpret_cast<void* (*)(const u32*)>(find_function_by_name)(&hash));
    if (!resource_readable(executable, 0x24u) || load(executable + 0xCu) != 0u ||
        (load(executable + 0x1Cu) & 2u) == 0u || !bytecode_readable(load(executable + 0x10u))) {
        menu::set_status("Loaded scripts have no Progression setup function");
        return;
    }
    const uptr object = load(executable);
    if (!resource_readable(object, 0x34u)) return;
    uptr instance = load(object + 8u);
    if (!readable(instance, 0x48u) || load(instance + 0x2Cu) != object) {
        const uptr list = load(object + 0x2Cu);
        instance = readable(list, 12u) ? load(list) : 0u;
    }
    if (!readable(instance, 0x48u) || load(instance + 0x2Cu) != object) {
        menu::set_status("Progression setup has no live script instance");
        return;
    }
    if (!prepare_script_launch()) return;
    using Add = void* (*)(void*, void*, const void*);
    if (reinterpret_cast<Add>(add_thread_with_parameters)(
            reinterpret_cast<void*>(instance), reinterpret_cast<void*>(executable), nullptr) != nullptr)
        menu::close();
    else menu::set_status("Progression setup could not start");
}

void refresh(menu::Menu& destination) {
    const u16 old_selection = destination.selected;
    menu::clear_entries(destination);
    for (Record& record : records) {
        if (record.active && !owner_live(record)) record.active = false;
        if (!record.active) continue;
        menu::Entry* entry = menu::add(destination, record.label, menu::EntryType::custom);
        if (entry != nullptr) {
            entry->target = &record;
            entry->id = record.serial;
            entry->handler = activate;
            entry->formatter = format_record;
        }
    }
    if (destination.count == 0u) {
        menu::Entry* entry = menu::add(destination, "Build Progression entries", menu::EntryType::action);
        if (entry != nullptr) entry->handler = build_entries;
    }
    destination.selected = old_selection < destination.count ? old_selection : 0u;
}

enum class Property : u8 { name, handler, id, value, value_type, minimum, maximum, step, scale };

float& number(Record& record, Property property) {
    switch (property) {
    case Property::id: return record.id;
    case Property::value_type: return record.value_type;
    case Property::minimum: return record.minimum;
    case Property::maximum: return record.maximum;
    case Property::step: return record.step;
    case Property::scale: return record.scale;
    default: return record.value;
    }
}

bool property(void* stack, Property field, bool setter) {
    uptr args = 0u;
    if (!arguments(stack, setter ? 8u : 4u, args)) return true;
    Record* record = from_handle(load(args));
    if (field == Property::name || field == Property::handler) {
        if (setter) {
            if (record != nullptr) {
                if (field == Property::name) {
                    char name[64]{};
                    if (copy_string(name, sizeof(name), load(args + 4u), true))
                        menu::copy_text(record->label, sizeof(record->label), name);
                } else {
                    char handler[192]{};
                    uptr instance = 0u, object = 0u;
                    if (copy_string(handler, sizeof(handler), load(args + 4u)) && context(stack, instance, object))
                        bind(*record, instance, object, handler);
                }
            }
            finish(stack, args, false);
        } else {
            const char* text = record == nullptr ? "" : field == Property::name ? record->label : record->handler;
            const uptr thread = load(reinterpret_cast<uptr>(stack) + 0x188u);
            const char* result = readable(thread, 0x14u)
                ? reinterpret_cast<const char* (*)(void*, const char*)>(install_temp_string)(reinterpret_cast<void*>(thread), text)
                : nullptr;
            finish(stack, args, true, reinterpret_cast<uptr>(result));
        }
    } else if (setter) {
        const float value = *reinterpret_cast<volatile const float*>(args + 4u);
        if (record != nullptr && finite(value) &&
            ((field != Property::step && field != Property::scale) || value >= 0.0f) &&
            (field != Property::value_type || (value >= 0.0f && value <= 9.0f && value == static_cast<float>(static_cast<u32>(value)))))
            number(*record, field) = value;
        finish(stack, args, false);
    } else {
        float value = record != nullptr ? number(*record, field) :
                      field == Property::maximum ? 1.0f : field == Property::step ? 0.1f :
                      field == Property::scale ? 10.0f : 0.0f;
        union { float floating; u32 bits; } result{value};
        finish(stack, args, true, result.bits);
    }
    return true;
}

bool destroy(void* stack) {
    uptr args = 0u;
    if (!arguments(stack, 4u, args)) return true;
    Record* record = from_handle(load(args));
    if (record != nullptr) record->active = false;
    finish(stack, args, false);
    return true;
}

}  // namespace

void configure(menu::Menu& destination) {
    destination.refresh = refresh;
    refresh(destination);
}

void invalidate_instance(void* instance) {
    for (Record& record : records)
        if (record.active && record.instance == reinterpret_cast<uptr>(instance)) record.active = false;
}

}  // namespace usm::gc::progression

#define USM_VM_HOOK(name) extern "C" __attribute__((used, noinline, section(".text." #name))) \
    bool name(const void*, void* stack, usm::gc::u32)
#define USM_PROPERTY_HOOK(name, field, setter) USM_VM_HOOK(name) { \
    return usm::gc::progression::property(stack, usm::gc::progression::Property::field, setter); }

USM_VM_HOOK(usm_gc_progression_create) { return usm::gc::progression::create(stack); }
USM_VM_HOOK(usm_gc_progression_destroy) { return usm::gc::progression::destroy(stack); }
USM_PROPERTY_HOOK(usm_gc_progression_get_name, name, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_name, name, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_handler, handler, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_handler, handler, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_id, id, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_id, id, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_value, value, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_value, value, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_value_type, value_type, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_value_type, value_type, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_min_value, minimum, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_min_value, minimum, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_max_value, maximum, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_max_value, maximum, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_step_size, step, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_step_size, step, true)
USM_PROPERTY_HOOK(usm_gc_progression_get_step_scale, scale, false)
USM_PROPERTY_HOOK(usm_gc_progression_set_step_scale, scale, true)

extern "C" __attribute__((used, noinline, section(".text.usm_gc_progression_instance_callbacks")))
void usm_gc_progression_instance_callbacks(void* instance, usm::gc::u32 reason, void* thread) {
    // The patched call is exclusively the first run_callbacks in the native
    // instance destructor. Preserve existing callbacks while this is intact,
    // then invalidate before the native destructor releases any threads/data.
    reinterpret_cast<void (*)(void*, usm::gc::u32, void*)>(usm::gc::progression::run_instance_callbacks)(instance, reason, thread);
    usm::gc::progression::invalidate_instance(instance);
}
