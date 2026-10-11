#pragma once

#include "../debug_menu_core.h"

namespace usm::gc::character_spawn {

constexpr u32 character_count = 66u;
// Appends teardown controls, Spawn All, and the exact requested catalogue.
void populate(menu::Menu& destination);
// Call once at the game-update boundary, outside other engine transactions.
void frame_advance();
bool pending();
// Mission/level/hero state changes must wait for owned actors and packs.
bool has_resources();
// Cancels creation and starts paced native cleanup. Call before acquiring a
// transition lock; frame_advance must keep running until this returns true.
bool prepare_transition();
// Cancels queued creation without forgetting actors/packs still owned here.
void cancel_pending();
const char* character_name(u32 index);

}  // namespace usm::gc::character_spawn
