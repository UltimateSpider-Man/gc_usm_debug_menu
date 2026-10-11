#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::diagnostics {

// Populate the GameCube-only diagnostic menus.  The caller owns all Menu
// objects; these functions only replace their entries and refresh callbacks.
void configure_ai(menu::Menu& ai);

void configure_memory(menu::Menu& memory,
                      menu::Menu& script_memtrack,
                      menu::Menu& slabs,
                      menu::Menu& full_slabs,
                      menu::Menu& partial_slabs,
                      menu::Menu& allocated_objects,
                      menu::Menu& free_objects);

void configure_entity_animations(menu::Menu& entity_animations);

void configure_entity_variants(menu::Menu& entity_variants);

}  // namespace usm::gc::diagnostics
