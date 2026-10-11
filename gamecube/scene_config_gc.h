#pragma once

#include "gc_types.h"

namespace usm::gc::scene_config {

// Update the native SCENE_NAME and publish a persistent-file request for the
// Dolphin companion. A GameCube disc itself is read-only during gameplay.
bool select_scene(const char* internal_name);
// City requests a saved INI and restarts only after the companion confirms
// both the external INI and mounted debug ISO were flushed successfully.
bool select_scene_and_reboot(const char* internal_name);
void frame_advance();
void format_persistence(char* output, u32 capacity);

}  // namespace usm::gc::scene_config
