// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_options.cpp -- the SqpOptions boundary validation SqpDriver's
// constructor runs, carved out of the class body in sqp_driver.h.
//
// M3 PHASE-C T3. CLAUDE.md section 5 homes "orchestration, drivers, options,
// printing, and instrumentation" in .cpp translation units regardless of how
// hot the surrounding loop is; this block is options validation and runs
// exactly ONCE per driver construction, so nothing about it depends on
// inlining through a template parameter. Before this carve it was an inline
// body inside `class SqpDriver`, so six comparison chains and six
// `fmt::format` call chains were parsed and code-generated in every TU that
// included sqp_driver.h -- and the SQP tree is header-only today, so that is
// every test, bench and library TU that touches the driver at all.
//
// CLAUDE.md section 4 IS THE CONTRACT THIS FILE IMPLEMENTS, verbatim: validate
// sizes and bounds at API boundaries; never print a diagnostic that is not
// also folded into the thrown exception's message; never construct an
// exception without throwing it; never call exit(). Every rejection below is a
// single `throw std::invalid_argument(fmt::format(...))` naming the option the
// caller actually set and the value it actually had.
//
// ---------------------------------------------------------------------------
// THE `!(x > 0.0)` IDIOM IS LOAD-BEARING AND IT IS A FLAG PREMISE, NOT A STYLE
// CHOICE. Read this before "simplifying" any predicate in this file.
//
// Every check is written as the NEGATION of the acceptance condition --
// `!(kkt_tol > 0.0)`, `!(tr_init > 0.0)`, `!(tr_max >= tr_init)`,
// `!(tr_min <= tr_init)` -- because that is the form that rejects NaN. A NaN
// compares false against everything, so `!(x > 0.0)` is TRUE for NaN and the
// option is rejected, whereas the "obvious" complement `x <= 0.0` is FALSE for
// NaN and would let a NaN tolerance or a NaN radius through into the solve.
//
// That reasoning only holds if the compiler is not permitted to assume NaN
// cannot occur. hven builds with `-ffast-math`, which alone implies
// `-ffinite-math-only`, under which the compiler MAY rewrite `!(x > 0.0)` into
// `x <= 0.0` -- the two are equivalent exactly when NaN is excluded by
// assumption. hven therefore appends `-fno-finite-math-only` immediately after
// `-ffast-math` in the SAFER_FAST FP mode that is this library's default and,
// since M3 phase-C U0, its ONE uniform regime
// (cmake/hven_compile_options.cmake, the HVEN_FP_MODE block: `-ffast-math`
// then `-fno-finite-math-only`; on Windows `/fp:fast` plus the probed spelling
// of the same). SAFER_FAST is exactly this: fast math WITHOUT the finiteness
// assumption.
//
// **THIS PREMISE IS PINNED BY DISASSEMBLY, NOT BY ASSERTION.** T3's proof
// battery disassembles this TU's object and checks that the NaN-catching
// comparisons survive as written -- that the emitted branch is the
// unordered-aware form (`comisd` + `jbe`/`ja`, i.e. a branch that takes the
// reject path when the compare sets PF) and not the finite-math complement.
// The evidence excerpt and the exact flags line are recorded with the commit
// and in the task report. If a future flag change drops
// `-fno-finite-math-only`, that pin is what turns a silent NaN admission into
// a caught one.
//
// ---------------------------------------------------------------------------
// FP-ARITHMETIC-FREE. There is not one floating-point operation in this file:
// every use of a double is a COMPARISON (`>`, `>=`, `<=`), a call to
// `std::isinf`, or an argument handed to `fmt::format` for rendering. `fmt`
// reads a double and formats its value; it does not compute with it. So
// nothing here can be re-associated, contracted into an FMA, or otherwise
// re-shaped by the codegen flags -- which, together with the pin above, is why
// this carve could land as an early phase-C split rather than waiting behind
// the FP-carrying ones.

#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/drivers/sqp_types.h>

namespace hven::solvers {

void validate_sqp_options(const SqpOptions &opts) {
    if (!(opts.kkt_tol > 0.0) || !(opts.feas_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: kkt_tol ({}) and feas_tol ({}) must both be > 0", opts.kkt_tol,
                        opts.feas_tol));
    }
    if (opts.max_iter < 0) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: max_iter ({}) must be >= 0", opts.max_iter));
    }
    if (!(opts.tr_init > 0.0)) { // catches NaN, negatives and exactly 0
        throw std::invalid_argument(fmt::format(
            "SqpDriver: tr_init ({}) must be > 0 (use +inf for no trust region; 0 would "
            "pin every step to zero and stall until max_iter)",
            opts.tr_init));
    }
    // tr_max is READ from Task 6 on (it caps the growth rule), so a
    // ceiling below the floor is now a contradiction rather than an
    // ignored field: it would silently mean "the radius may never grow,
    // and the very first subproblem already violates the cap".
    //
    // tr_init == +inf IS EXEMPT FROM THIS PARTICULAR CHECK, because at
    // that value there is no starting radius for tr_max to be a ceiling
    // ON. It is no longer exempt from tr_max MATTERING: from Task 9 the
    // FIRST rejection lands the radius on tr_max (see sqp_driver.h's RADIUS
    // MANAGEMENT), so the pair is meaningful after all -- what +inf now means
    // is "unbounded until the method finds it needs a bound, then tr_max".
    // The growth rule is still skipped while the radius is +inf
    // (min(+inf*2, tr_max) would REDUCE it, which no growth rule may do).
    if (!std::isinf(opts.tr_init) && !(opts.tr_max >= opts.tr_init)) { // catches NaN too
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_max ({}) must be >= tr_init ({}); it is the ceiling "
                        "the trust-region growth rule expands toward",
                        opts.tr_max, opts.tr_init));
    }
    // THE FLOOR (Task 9). It must be positive (0 would make it
    // unreachable, since the radius only ever halves, and the restoration
    // trigger it exists to arm would be dead), and it must not exceed
    // either end of the range it floors -- tr_min > tr_init would put the
    // FIRST subproblem below the floor, i.e. request restoration before
    // the solve has tried anything, and tr_min > tr_max would do the same
    // to the radius a +inf tr_init lands on. Both forms catch NaN.
    if (!(opts.tr_min > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_min ({}) must be > 0; it is the radius floor whose crossing "
                        "raises a restoration request, and 0 can never be crossed by halving",
                        opts.tr_min));
    }
    if (!(opts.tr_min <= opts.tr_init) || !(opts.tr_min <= opts.tr_max)) {
        throw std::invalid_argument(
            fmt::format("SqpDriver: tr_min ({}) must be <= both tr_init ({}) and tr_max ({}); "
                        "a floor above the starting radius requests restoration before the "
                        "first trial is judged",
                        opts.tr_min, opts.tr_init, opts.tr_max));
    }
}

} // namespace hven::solvers
