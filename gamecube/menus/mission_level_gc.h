#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::mission_level {

// The PC menu can expose considerably more entries than the placeholder menus
// originally reserved.  These are recommended capacities for callers creating
// the two menus; the implementation always honours the actual Menu::capacity.
constexpr u16 recommended_mission_capacity = 8u;
constexpr u16 recommended_level_capacity = 64u;

// Replace the destination's placeholder entries, install the refresh callback,
// and enumerate the currently loaded GameCube mission tables.
void attach_missions(menu::Menu& destination);

// Replace the destination's placeholder entries, install the refresh callback,
// enumerate the GameCube level descriptors, then append the native GameCube
// reboot action and the supplied Hero Select submenu. Hero switching is
// implemented by hero_select_gc and runs from the deferred game-update hook.
void attach_level_select(menu::Menu& destination, menu::Menu& hero_select);

// Public refresh callbacks, useful when menus are created directly with
// menu::create().  attach_* calls these once and installs them automatically.
// Advance the guarded native unload/hero-switch/force sequence after the stock update.
void frame_advance();
bool busy();
void refresh_missions(menu::Menu& destination);
void refresh_level_select(menu::Menu& destination);

}  // namespace usm::gc::mission_level
