// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.5 PROMOTED `SqpWarmStart` out of `detail/` into `warmstart/` as
// `SqpWarmStart` -- the LABELLED SQP-ONLY native warm-start entry beside the
// shared `WarmStartData` payload. That move makes a header that was internal
// into a header a consumer includes, and it drags `detail/qp/working_set.h`
// (held BY VALUE) and `qp/qp_types.h` onto the installed surface with it. The
// export contract's header install is a glob, so nothing there had to be told
// about the new file; THIS TU is the proof that the glob actually shipped what
// the header needs, compiled against the INSTALLED prefix the way a consumer
// sees it.

#include "hven/warmstart/sqp_warm_start.h"

namespace hven_install_smoke {
int standalone_include_sqp_warm_start() {
    // TOUCHED, not merely included: default-construct the object, read the
    // cold default that gives the whole type its meaning, and touch the
    // by-value WorkingSet member, which is the one field whose own header had
    // to come along for the ride.
    hven::solvers::SqpWarmStart warm;
    const bool cold = !warm.valid && warm.structure_hash == 0 && warm.hot == nullptr;
    const bool working_set_is_reachable = warm.qp_working_set.n() == 0;
    // The `SqpWarmStart` alias is deliberately NOT checked here: it lives in
    // `detail/warmstart/warm_start.h`, and including that would defeat the
    // point of this TU, which is that the PUBLIC header stands alone.
    return (cold && working_set_is_reachable) ? 1 : 0;
}
} // namespace hven_install_smoke
