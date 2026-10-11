// Ultimate Spider-Man debug menu payload for Nintendo GameCube.
//
// This translation unit intentionally contains only the injected hook wrappers.
// Platform ABI declarations, engine bridges, input and menu implementations
// live under gamecube. It is freestanding PowerPC EABI code: no Windows
// DLL entrypoint, C runtime startup, STL, heap allocation, or constructors.

#include "gamecube/debug_menu_core.h"
#include "gamecube/gc_engine.h"
#include "gamecube/gc_input.h"
#include "gamecube/menu_registry.h"
#include "gamecube/scene_config_gc.h"
#include "gamecube/ai_export_gc.h"
#include "gamecube/menus/character_spawn_gc.h"
#include "gamecube/menus/hero_select_gc.h"
#include "gamecube/menus/mission_level_gc.h"
#include "gamecube/menus/replay_gc.h"

extern "C" __attribute__((used, noinline)) void usm_debug_menu_hook() {
    // The original call at 0x8016E55C was nglListEndScene(). Add menu draw
    // commands to the current NGL scene, then make that original call exactly
    // once so the surrounding game function keeps its normal control flow.
    // Bootstrap here as well as from game::frame_advance: this render path can
    // be reached before the update hook has initialized the registry.
    usm::gc::registry::initialize_for_render();
    if (usm::gc::registry::initialized()) {
        const usm::gc::input::Frame frame = usm::gc::input::poll();
        usm::gc::menu::update(frame);
        usm::gc::registry::render_overlays();
        usm::gc::menu::render();
    }
    usm::gc::engine::finish_scene();
}

// Replaces the final game_button::update() call in game::frame_advance().
// Running deferred world mutations here mirrors the updated PC port's
// post-frame-advance queue and keeps them out of game::render_ui().
extern "C" __attribute__((used, noinline, section(".text.usm_debug_menu_update_hook")))
void usm_debug_menu_update_hook(void* button, float time_inc) {
    usm::gc::engine::update_game_button(button, time_inc);
    usm::gc::replay::frame_advance(time_inc);
    usm::gc::registry::initialize();
    usm::gc::menu::dispatch_deferred();
    usm::gc::hero_select::frame_advance();
    usm::gc::mission_level::frame_advance();
    usm::gc::character_spawn::frame_advance();
    usm::gc::scene_config::frame_advance();
    usm::gc::ai_export::frame_advance();
}

extern "C" __attribute__((used, noinline, section(".text.usm_debug_menu_pad_read_hook")))
usm::gc::u32 usm_debug_menu_pad_read_hook(usm::gc::PadStatus* pads) {
    return usm::gc::input::read_for_game(pads, usm::gc::menu::owns_input());
}
