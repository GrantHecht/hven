// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The IPQP tier's test seam -- the same convention as
// hven/detail/linear/fault_injection.h, applied one layer up. See
// docs/testing.md for the full design rationale; the summary that matters
// here:
//
// WHY A SECOND HEADER RATHER THAN A ROW IN THE FIRST. fault_injection.h is the
// LINEAR LAYER's seam: everything it declares lives in
// `hven::linear::detail::testing` and is consumed by the two backend adapter
// TUs. What this file declares is consumed by the QP tier
// (src/qp/ipqp_engine.cpp) and is about the tier's own reading of an
// already-computed factorization, not about a backend call. Putting a
// `hven::solvers` injector inside a `hven::linear` header would make the
// linear layer's seam header depend on a consumer above it. The CONVENTION is
// the shared thing, and it is followed exactly (CLAUDE.md section 8: "use this
// convention for any new need rather than inventing another").
//
// WHERE THE HOOK GOES: src/qp/ipqp_engine.cpp -- an APACHE-2.0 FILE THIS
// REPOSITORY WROTE, at the one line where the tier reads
// `KktFactorization::inertia_evidence()`. CLAUDE.md section 6's boundary
// preference is satisfied WITHOUT a deviation: the fact being injected (what
// evidence the tier believes it received) is observable exactly at that
// boundary, no session file is touched, and nothing MPL-derived is involved.
// This seam therefore adds NO new entry to notices/ and no new sanctioned
// inside-the-session-file deviation; docs/testing.md records it as a third
// injection point under the existing convention.
//
// WHY IT IS NEEDED. Task 4 could pin neither terminal inertia state the
// specification's evidence-failure policy is written for:
//
//   * a terminal PERTURBED-pivot report -- MKL's static pivot perturbation
//     fires on matrices the ladder's own `delta` growth resolves first, so no
//     legal subproblem reaches the ladder's ceiling still perturbed;
//   * an `InertiaEvidence::State` other than `kObserved` -- no MKL path
//     declines to report inertia at all, and the `kUnavailable` case the
//     specification names is an ACCELERATE path this machine cannot run
//     (CLAUDE.md section 6's never-fabricate rule: that arm stays UNOBSERVED
//     until real Mac hardware runs it).
//
// Section 2.2's evidence-failure policy -- a step at a conservative `rho`
// floor plus a whole-solve certificate downgrade -- is therefore code no legal
// fixture can reach. That is exactly the coverage gap this convention exists
// for.
//
// It compiles to NOTHING unless HVEN_TESTING is defined, so including it from
// a normal build of any TU is provably inert: the production `hven` library
// target's compiled `ipqp_engine.cpp.o` is byte-for-byte what it would be if
// this header did not exist, which docs/testing.md records the measurement
// for. HVEN_TESTING is defined ONLY target-wide on the standalone
// hven_ipqp_seam_tests executable (tests/CMakeLists.txt), which recompiles the
// tier's own sources a second time and does NOT link hven::hven.

#ifdef HVEN_TESTING

#include <limits>

#include <hven/core/types.h>
#include <hven/linear/symmetric_factor.h>

namespace hven::solvers::detail::testing {

// Substitutes the inertia evidence the tier READS for a factorization that
// really ran. The factorization itself is untouched -- the backend session,
// the factor, and `KktFactorization::info()` are all exactly what the real
// call produced.
//
// WHAT IS AND IS NOT FAITHFUL, stated per scenario rather than claimed for all
// of them (co-review CM-1). The convention's own rule is that an injector is
// faithful only where the injection is indistinguishable from the real thing
// on every observable being asserted, and that is worked out per fault path,
// never assumed to transfer:
//
//   * `kQueryFailed` / `kUnavailable` -- FAITHFUL END TO END. The scenario IS
//     "a real factor whose inertia could not be reported", so leaving the
//     factor untouched and changing only what the query returns reproduces it
//     exactly. Nothing about a real occurrence would look different.
//   * An OBSERVED reading with different counts, or with a perturbed-pivot
//     report, on a factor that is really convex and really unperturbed --
//     NOT a backend end-to-end witness. Such evidence is deliberately
//     INCONSISTENT with the factor the tier is holding, which no backend
//     would produce. These are POLICY AND CLASSIFICATION witnesses: they pin
//     what the tier DOES with a reading of that shape (which class it
//     assigns, which census bucket it lands in, what it does to the
//     certificate), and they say nothing about whether a backend correlates
//     evidence with factors correctly. The tests that use them say so at
//     their own sites, and docs/testing.md records the limit beside the
//     failed-factorization one.
//
// It is deliberately NOT able to fake a FAILED factorization: that state is
// read off `info()`, which this injector does not touch, and faking it here
// would produce a scenario no backend can present (a successful factor whose
// status says otherwise). The failed-factorization path has its own seam one
// layer down -- `hven::linear::detail::testing::FactorizeFaultInjector`.
struct IpqpInertiaEvidenceInjector {
    static inline bool active = false;

    // WHICH READS THE INJECTION APPLIES TO. The tier takes two KINDS of
    // inertia reading and section 2.2 gives them different policies, so a
    // seam that could not tell them apart could not pin either: an
    // iteration/ladder reading feeds the evidence-failure policy (a step at a
    // conservative floor), while the section 2.2 item 4 final certification
    // reading feeds `ipqp_final_inertia_read` and takes no step at all.
    static inline bool on_iteration_reads = true;
    static inline bool on_final_read = true;

    // Let this many ELIGIBLE reads through untouched before injecting. Exists
    // so a fixture can let a solve converge normally and then corrupt only
    // the reading that decides its certificate.
    static inline Index skip_first = 0;

    // Stop injecting after this many replacements; negative means unlimited.
    // With `skip_first` this makes a WINDOW, which is what a fixture needs to
    // pin a fault the tier RECOVERS from rather than one it dies on: a fault
    // injected forever can only ever be observed at a terminal state, and the
    // interesting contracts (which quantity a recovery step was built from,
    // how many factorizations a bounded re-route spends before it gives up)
    // are about what happens AFTER the reading comes good again. Added in M6
    // W1 T4b fix round 1 for exactly two such pins; it costs the production
    // build nothing, like the rest of this header.
    static inline Index max_injections = -1;

    // What the tier is told it read.
    static inline hven::linear::InertiaEvidence evidence{};

    // How many reads were actually replaced -- a fixture asserts this so an
    // injection that silently stopped applying fails its own pin rather than
    // passing as a clean solve.
    static inline Index injections = 0;

    static void reset() {
        active = false;
        on_iteration_reads = true;
        on_final_read = true;
        skip_first = 0;
        max_injections = -1;
        evidence = hven::linear::InertiaEvidence{};
        injections = 0;
    }
};

// NOT a fault injector -- a pure OBSERVER riding the same seam, the
// `PardisoIparmObserver` arrangement one layer up. It records WHAT THE TIER
// READ, which nothing outside the solve can otherwise see: `IpqpResult`
// carries counters and a classification, never the evidence behind them.
//
// It exists for one claim the counters cannot make on their own -- section
// 2.2's "the counts are never zero-filled or inferred". An ABSENT
// perturbed-pivot count (`std::nullopt`, Accelerate's honest state) must reach
// the tier's classifier AS ABSENT and not as the integer 0, which on a backend
// that does count pivots means "none were perturbed". The difference is
// invisible in every output the tier produces, so it is observed here.
struct IpqpInertiaReadObserver {
    static inline bool active = false;
    static inline Index reads = 0;
    static inline Index final_reads = 0;
    static inline hven::linear::InertiaEvidence last{};
    static inline hven::linear::InertiaEvidence last_final{};

    // The last evidence that was SUBSTITUTED, kept apart from `last` because
    // a fixture that injects only the iteration reads still has a REAL final
    // read overwriting `last` afterwards -- and the claim being checked (that
    // an injected `-1`/`nullopt` reaches the classifier unaltered) is about
    // the substituted one.
    static inline hven::linear::InertiaEvidence last_injected{};

    static void reset() {
        active = false;
        reads = 0;
        final_reads = 0;
        last = hven::linear::InertiaEvidence{};
        last_final = hven::linear::InertiaEvidence{};
        last_injected = hven::linear::InertiaEvidence{};
    }
};

// THE FIRST ITERATE, VERBATIM -- an observer, not an injector (T9, registered
// by T7). `IpqpResult` publishes only the FINAL iterate, so T7's staged-split
// ingest proof was an invariant about repair shifts; this makes it bitwise.
struct IpqpFirstIterateObserver {
    static inline bool active = false;
    static inline Index captures = 0;
    static inline Vec x{}, s{}, ye{}, yi{}, zl{}, zu{};
    static inline double mu = 0.0;

    static void reset() {
        active = false;
        captures = 0;
        x = Vec{};
        s = Vec{};
        ye = Vec{};
        yi = Vec{};
        zl = Vec{};
        zu = Vec{};
        mu = 0.0;
    }
};

// GATE 9's PER-STEP FORM (T9, registered as T4b C8/I6): the EXECUTED map must
// have no fixed point the stopping gate cannot see. Neither the direction nor
// the regularized residual leaves the solve, so both are observed here.
struct IpqpStepObserver {
    static inline bool active = false;
    static inline Index steps = 0;
    static inline double min_step_inf = std::numeric_limits<double>::infinity();
    static inline double res_at_min_step = std::numeric_limits<double>::infinity();
    static inline bool met_target_at_min_step = false;
    static inline double last_step_inf = std::numeric_limits<double>::infinity();
    static inline double last_res = std::numeric_limits<double>::infinity();

    static void reset() {
        active = false;
        steps = 0;
        min_step_inf = std::numeric_limits<double>::infinity();
        res_at_min_step = std::numeric_limits<double>::infinity();
        met_target_at_min_step = false;
        last_step_inf = std::numeric_limits<double>::infinity();
        last_res = std::numeric_limits<double>::infinity();
    }
};

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
