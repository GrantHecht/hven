// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/core/version.h"

#ifndef HVEN_VERSION_STRING
#error "HVEN_VERSION_STRING must be defined by the build (see CMakeLists.txt)"
#endif

namespace hven {

// HVEN_VERSION_STRING is defined by the build -- target_compile_definitions(
// hven PRIVATE HVEN_VERSION_STRING="${PROJECT_VERSION}") in CMakeLists.txt,
// from project(hven VERSION ...) -- the single source of truth, not a
// hand-copied literal.
const char *version() noexcept { return HVEN_VERSION_STRING; }

} // namespace hven
