// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The options both engines share. Each engine's options struct embeds exactly
// one of these, so a knob that means the same thing on both is spelled the same
// way on both: the backend thread count, the console verbosity, and the ceiling
// on how much of a warm start a solve is willing to trust.
//
// The DEFAULTS here are the SQP engine's; the interior-point engine's
// IpmOptions default-initialises its own `common` to the interior-point
// engine's historical values instead (see ipm_solver_types.h). Neither engine
// changed behaviour when the fields moved (M6 W5 T8.3).

#include <hven/core/start_level.h>

namespace hven::solvers {

/// @brief The options both engines share, embedded once in each engine's own
///        options struct.
struct CommonOptions {
    /// Backend thread count for this solver's factorizations. 0 means "leave
    /// the backend's own default alone", which is what the SQP engine has
    /// always done -- the process-wide MKL_NUM_THREADS pin is what makes its
    /// runs reproducible. The interior-point engine defaults this to
    /// HVEN_DEFAULT_QP_THREADS, the value its Settings::qp_threads_ carried.
    ///
    /// T8.3 MOVES the field and changes nothing about how it is read: the
    /// interior-point engine applies it exactly where it applied
    /// qp_threads_, and the SQP engine still does not apply it at all. T8.8 is
    /// where a non-zero count starts reaching every SQP factor path.
    int threads = 0;

    /// Console verbosity, on the interior-point engine's scale in both
    /// engines:
    ///   0  -- full output (stats + iteration table + exit + timing)
    ///   1  -- no iteration table (phase banners + timing summary)
    ///   2  -- exit status and warnings only
    ///   3+ -- fully silent
    ///
    /// The default 3 is therefore SILENT, which is what the SQP engine has
    /// always been; the interior-point engine's IpmOptions defaults it to 0,
    /// which is what Settings::print_level_ defaulted to. Must be
    /// non-negative. T8.7 is where the SQP engine gains a console table of its
    /// own at these levels.
    int print_level = 3;

    /// The ceiling on how much of an offered warm start a solve will trust.
    ///
    /// The SQP engine reads its own `SqpOptions::start_level` today (which
    /// this field replaces at T8.10). THE INTERIOR-POINT ENGINE READS THIS ONE,
    /// from M6 W5 T8.5, on its PAYLOAD route -- `solve(model, x0, warm,
    /// budget)` -- with four rungs (design 2.6):
    ///
    ///   kCold    the payload is ignored entirely and COUNTED
    ///            (`IpmResult::payload_ignored`). Its block lengths and its
    ///            declaration stamp are still checked: a ceiling says what of a
    ///            payload to apply, never that a foreign one is acceptable.
    ///   kSeeded  the MULTIPLIERS only, through the engine's own floor/cap; the
    ///            point and the polish extension are ignored, the extension
    ///            counted (`IpmResult::polish_ignored`). `x0` is the start.
    ///   kWarm    the whole payload -- point, multipliers, polish.
    ///   kHot     IDENTICAL to kWarm. The interior-point engine has no hot
    ///            handle to adopt (its cross-call factorization reuse is keyed
    ///            on the program's own analysis identity, never on a payload),
    ///            so the top rung is documented as equal to kWarm rather than
    ///            refused.
    ///
    /// A CEILING, never a floor, on both engines: it can only lower what a
    /// value would otherwise have resolved to.
    StartLevel start_level = StartLevel::kWarm;
};

} // namespace hven::solvers
