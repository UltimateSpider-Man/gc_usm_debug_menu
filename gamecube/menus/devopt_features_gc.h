#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::devopt_features {

// Uses the GC executable's own name tables and 32-bit option layout. PC
// ordinals differ after ALLOC_ARAM and the renderer allocation options.
void configure_devopts(menu::Menu& destination);
// Attach live handlers to the existing Game controls without adding rows.
void configure_game_extras(menu::Menu& destination);
void render();

}  // namespace usm::gc::devopt_features
