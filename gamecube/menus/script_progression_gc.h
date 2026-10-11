#pragma once

#include "debug_menu_core.h"

namespace usm::gc::script_progression {

// ABI facts verified against the supplied NTSC-U retail main.dol.  They are
// intentionally exposed as constants instead of C++ object definitions: the
// game owns these objects and treating an incomplete mirror as a real type is
// an easy way to corrupt the VM.
constexpr uptr vm_stack_base_offset = 0x180u;
constexpr uptr vm_stack_pointer_offset = 0x184u;
constexpr uptr vm_stack_thread_offset = 0x188u;
constexpr uptr vm_thread_instance_offset = 0x0Cu;
constexpr uptr vm_thread_executable_offset = 0x10u;
constexpr uptr vm_thread_stack_offset = 0x20u;
constexpr uptr vm_executable_owner_offset = 0x00u;
constexpr uptr vm_executable_parameter_stack_size_offset = 0x0Cu;
constexpr uptr script_object_functions_offset = 0x20u;
constexpr uptr script_object_function_count_offset = 0x24u;

constexpr uptr script_instance_add_thread = 0x801B2E40u;
constexpr uptr script_instance_add_thread_with_parameter = 0x801B2EF0u;
constexpr uptr script_instance_add_thread_extended = 0x801B2F60u;
constexpr uptr script_object_find_function = 0x801B37C8u;
constexpr uptr string_hash_initialize = 0x8015BF1Cu;
constexpr uptr vm_thread_install_temp_string = 0x801B75A4u;
constexpr uptr construct_client_script_libraries = 0x801B9228u;

// The call at 0x801B9238 in construct_client_script_libraries targets this
// one-instruction no-op.  gc_address.txt instead labels 0x7F112A24, which is
// in the middle of an unrelated debug-menu selection routine in this DOL.
constexpr uptr retail_construct_debug_menu_library = 0x7F112934u;
constexpr u32 retail_construct_debug_menu_library_instruction = 0x4E800020u;  // blr

// The global-library constructor still registers all three retail creators.
// Their stub bodies discard their arguments and return a zero handle. The
// Progression operator vtable slot is patched to our native VM bridge.
constexpr uptr retail_create_debug_entry_stub = 0x7F17057Cu;
constexpr uptr retail_create_debug_entry_with_handler_stub = 0x7F1705D8u;
constexpr uptr retail_create_progression_entry_stub = 0x7F170634u;

// Configure the native Kill Hero action and dynamic Progression entries.
// The character-lineup module appends its separate native actions afterward.
void populate_diagnostics(menu::Menu& script, menu::Menu& progression);

constexpr bool dynamic_entries_supported() {
    return true;
}

}  // namespace usm::gc::script_progression
