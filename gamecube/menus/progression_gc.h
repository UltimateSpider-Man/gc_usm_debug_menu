#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::progression {

constexpr u16 recommended_capacity = 512u;

// Registration can run before the render registry exists; configure attaches
// its persistent records without clearing entries already created by scripts.
void configure(menu::Menu& destination);
void invalidate_instance(void* instance);

}  // namespace usm::gc::progression
