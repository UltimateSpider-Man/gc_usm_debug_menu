#pragma once

#include "../gc_types.h"

namespace usm::gc::ngl_features {

// Payload implementations for the PC performance displays removed from
// retail NGL. The original native performance-bar/screenshot flags remain
// owned by the engine bridge.
bool handles(u32 catalog_index);
s32 get(u32 catalog_index);
void set(u32 catalog_index, s32 value);
void render();

}  // namespace usm::gc::ngl_features
