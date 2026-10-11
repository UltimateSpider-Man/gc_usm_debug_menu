#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::replay {

constexpr u16 recommended_capacity = 6u;
constexpr u32 maximum_samples = 3600u;

// The retail RTDT implementation is absent. Record the actual game-facing PAD
// samples instead, keeping physical Z outside playback for menu control.
void configure_replay(menu::Menu& destination);

// Call after the input layer observes physical input, before it strips Z or
// neutralizes gameplay input for the menu. The render-only PAD poll must not
// call this: a sample represents a game input update, not a rendered frame.
void process_game_pads(PadStatus* pads, bool menu_has_focus);

// Assign the stock update's duration to input samples just consumed by that
// update. Run before deferred menu commands, from the existing update hook.
void frame_advance(float time_inc);

}  // namespace usm::gc::replay
