#pragma once

namespace usm::gc::registry {

// Builds only menu topology and static entries. Safe from the render hook: it
// does not enumerate gameplay objects or invoke resource/REL discovery code.
void initialize_for_render();

// Completes dynamic menu discovery from the game-update hook. Also performs
// the render-safe initialization first when required.
void initialize();
bool initialized();
// Performance displays remain visible after closing the menu.
void render_overlays();

}  // namespace usm::gc::registry
