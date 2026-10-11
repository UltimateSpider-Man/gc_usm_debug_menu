#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::hero_select {

constexpr u16 hero_count = 10u;

// Replaces the destination's entries with the ten Hero Select actions from
// the PC debug menu.  Selection only queues a request; frame_advance() owns
// every engine mutation.
void configure(menu::Menu& destination);

// Advance the deferred remove/add/HUD-repair sequence once per game-update
// hook, after the stock game::frame_advance work and outside render_ui.
void frame_advance();
bool busy();

}  // namespace usm::gc::hero_select
