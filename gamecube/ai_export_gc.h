#pragma once

#include "gc_types.h"

namespace usm::gc::ai_export {

using Serializer = bool (*)(char*, u32, u32&);

// The serializer runs synchronously only when a fresh companion is connected
// and no earlier snapshot is pending. Output is an exact byte count, not a
// NUL-terminated string. The companion acknowledges only after saving its copy.
bool request_export(Serializer serializer);
void frame_advance();

}  // namespace usm::gc::ai_export
