// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

// W5 T8.7 made both engines' console tables a SINK -- `ConsoleTraceSink` --
// and gave the composition behind "attaching a console never displaces your
// sink" a name of its own, `FanOutTraceSink`. Both are public, and the
// header's own claim is that it needs nothing else: in particular NOT
// `detail/interior/iterate_info.h`, the interior-point iteration record the
// row renderer reads, which arrives by reference through `drivers/trace.h`.
// A consumer that includes only this header and constructs a console must
// compile, and that is what this TU is.

#include "hven/drivers/console_trace_sink.h"

#include <cstdio>

namespace hven_install_smoke {
int standalone_include_console_trace_sink() {
    // TOUCHED, not merely included, and touched through the composition the
    // header exists for: a console over a null FILE that nothing writes to,
    // and a fan-out over it and a null half. Constructing both is what proves
    // the two classes are COMPLETE here -- a forward declaration would compile
    // an include and fail this line.
    //
    // Nothing is emitted: every `on_*` needs an event struct, and the point of
    // this TU is the header's include closure, not the sink's behaviour (which
    // tests/drivers/test_console_sink.cpp pins byte for byte).
    hven::solvers::ConsoleTraceSink console(
        hven::solvers::ConsoleTraceSink::Format{/*wide=*/true, /*print_level=*/3}, stdout);
    hven::solvers::FanOutTraceSink fan(nullptr, &console);
    // The colour function the header declares beside them, on a value that is
    // unambiguously inside the converged band.
    const bool coloured = hven::solvers::ipm_residual_color(1e-12, 1e-6, 1e-2) != fmt::text_style();
    const bool composed = fan.second() == &console && fan.first() == nullptr &&
                          console.format().wide && console.sqp_depth() == 0;
    return (coloured && composed) ? 1 : 0;
}
} // namespace hven_install_smoke
