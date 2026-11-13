// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why: this directory's
// CMakeLists.txt.

// M8 AP-5 G1: the stop source and token a caller hands to one solve through
// SolveBudget::stop. hven's own type, so no <stop_token> reaches a consumer.

#include "hven/core/stop.h"

namespace hven_install_smoke {
int standalone_include_core_stop() {
    hven::StopSource source;
    const hven::StopToken token = source.token();
    const hven::StopToken empty;
    const bool before = token.stop_requested();
    source.request_stop();
    return (!before && token.stop_requested() && token.stop_possible() && !empty.stop_possible() &&
            !empty.stop_requested())
               ? 1
               : 0;
}
} // namespace hven_install_smoke
