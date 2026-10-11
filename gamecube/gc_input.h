#pragma once

#include "gc_types.h"

namespace usm::gc::input {

// Replacement for the game's sole PADRead call site. It caches raw input and
// latches a Z press at PAD sampling time so a short press cannot be lost
// before render_ui consumes it. The controller that opens the menu keeps
// navigation focus; all four GameCube ports can activate it.
u32 read_for_game(PadStatus* pads, bool menu_has_focus);

struct Frame {
    bool toggle_menu;
    bool up;
    bool down;
    bool left;
    bool right;
    bool accept;
    bool cancel;
    bool fast;
    bool connected;
};

Frame poll();
bool connected(u32 channel);

}  // namespace usm::gc::input
