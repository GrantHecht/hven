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
    /// The SQP engine reads it today (SqpOptions::start_level, which this field
    /// replaces at T8.10); on the interior-point engine it is CARRIED BUT
    /// UNREAD in T8.3 -- T8.5 is where the interior-point engine's warm-start
    /// entry starts consulting it.
    StartLevel start_level = StartLevel::kWarm;
};

} // namespace hven::solvers
