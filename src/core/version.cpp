#include "hven/core/version.h"

namespace hven {

// Source of truth for this literal is the root CMakeLists.txt `project(hven
// VERSION ...)` declaration; keep the two in sync by hand until a configured
// header is worth the extra build-graph edge.
const char *version() noexcept { return "0.1.0"; }

} // namespace hven
