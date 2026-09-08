// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The solver counter contract's types: QpCounters (one QP engine solve),
// SsnCounters (the semismooth-Newton kernel's own work), IpqpCounters (the
// IP-PMM interior-point tier's own work) and SqpCounters (one whole SQP driver
// solve, which aggregates all three).

#include <cstddef>
#include <limits>
#include <type_traits>

#include <hven/core/detail/aggregate_arity.h>
#include <hven/core/start_level.h>

namespace hven::solvers {

/// @brief QP solver performance counters.
///
/// The two EQP refinement counters below count different things: border_refine_steps
/// is the TOTAL steps kept by every solve_bordered_eqp call, the mandatory first
/// included; eqp_refine_steps is the EXTRA steps kept by every solve_eqp call
/// beyond its single unconditional one, and is identically 0 -- the loop it
/// counted was deleted as measured-inert, and the counter stays as that
/// invariant. "KEPT" is operative in both: a candidate step the strict-decrease
/// rule rejects is discarded and NOT counted.
/// @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h
struct QpCounters {
    Index factorizations = 0;
    Index schur_updates = 0;
    Index minor_iters = 0;
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    /// Refinement steps kept by the verdict-site face refinement (qp_engine.h
    /// section 5) across this solve -- a third, disjoint quantity, deliberately
    /// not folded into the two fields above.
    ///
    /// COUNTS: steps KEPT, in their sense -- a candidate the strict-decrease
    /// safeguard rejects, and a refinement whose result the engine then declines
    /// to adopt, contribute nothing.
    ///
    /// EXCLUDES: every refinement step taken inside an EQP solve, and every dead
    /// end whose classification was going to be kOptimal anyway. Nothing is
    /// excluded by algebra: both paths refine at a would-be-kInfeasible dead end,
    /// and which twin runs is decided by the candidate's provenance rather than
    /// by QpOptions::ws_algebra.
    ///
    /// At most detail::kMaxVerdictRefineSteps steps are bought per dead end; a
    /// whole-solve figure is only as large as the fixture's dead ends make it.
    Index verdict_refine_steps = 0;

    /// Rungs of the suspect-stall escalation ladder this solve spent (see
    /// qp_engine.h's section 4b). Each rung multiplies the effective
    /// primal_delta by detail::kSuspectDeltaFactor after a would-be-kOptimal
    /// exit off a kSuspect factorization failed the free-block stationarity
    /// check. Zero on every solve whose linear algebra was trustworthy, which
    /// is the overwhelming majority; a solve that reports kNumericalError with
    /// this at detail::kMaxSuspectEscalations exhausted the ladder, and that
    /// pairing IS the exhaustion diagnostic.
    Index suspect_escalations = 0;

    /// True iff qp_engine.h's border-mode reuse gate (`reuse_eligible` in
    /// QpEngine::run(), conditions (a)-(e)) judged the persisted K0/border cache
    /// trustworthy for THIS solve() call -- whether the engine actually skipped
    /// rebuilding K0, not merely whether `factorizations` reads 0. The two are
    /// different claims: `factorizations == 0` also arises from a reduced system
    /// with nothing to factorize and from a solve that never reached the loop.
    /// Always false under QpOptions::ws_algebra == kRefactorize, where the
    /// border-mode cache does not exist.
    bool k0_reused = false;

    /// Number of backend SYMBOLIC-ANALYSIS calls this solve() call paid for --
    /// the number of times qp_engine.h's rebuild_k0() found the analysis decision
    /// `needed` before calling `factorize_checked()`. Counted at the call site
    /// rather than inside the factor, so it stays a QP-engine-level observable.
    ///
    /// A never-shared solve should read 1 (or 0, if the pattern never needed
    /// rebuilding) regardless of how many `factorizations` it pays; a fixture
    /// that legitimately forces detaches reads more.
    Index symbolic_analyses = 0;

    // ---------------------------------------------------------------------
    // The working-set walk counters (eleven: the five below and the six after).
    //
    // PURELY OBSERVATIONAL. None of the eleven is read by any decision in
    // qp_engine.h, so the walk's trajectory is what it was before they existed.
    //
    // The arithmetic a reader should do with them, for a solve ending at a
    // working set of W members having started from a seed of S:
    //
    //     ws_adds - ws_drops + shift_adds  ==  W - S   (net identity)
    //
    // so ws_drops is the CHURN; ws_drops / minor_iters is the churn fraction and
    // degenerate_steps / minor_iters the degeneracy fraction, and the two are
    // independent.
    //
    // The identity holds on the convex path, with one named exception:
    // qp_engine.h's temporary-vertex start repair writes `ws.bound_state()`
    // directly, without touching QpCounters and without marking the variable in
    // the seen-set. On a solve that ran it, the net identity is off by the pins
    // the repair left standing, `ws_adds_bound`/`ws_drops_bound` exclude its pins
    // and releases, and `distinct_bound_added` does not count a variable whose
    // only admission was a repair pin. The repair runs only at `iter == 0` on an
    // indefinite start vertex, so every convex corpus reports these exactly.
    // @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h

    /// BLOCKING-CONSTRAINT ADDITIONS: one per minor iteration whose ratio
    /// test named a blocker that then joined the working set, counting an
    /// inequality row and a variable bound alike (BlockKind::kIneq /
    /// kBound). The negative-curvature ride's own blocker (section 4c) is
    /// counted here too, since it is the same ratio test reporting the same
    /// kind of event; a minor that took a full unblocked step to the EQP
    /// point (kind == kNone) adds nothing and is counted nowhere.
    Index ws_adds = 0;

    /// WORKING-SET REMOVALS: one per constraint released by the Dantzig drop
    /// rule (drop_worst) plus one per constraint released by the
    /// zero-multiplier probe (probe_zero_multiplier_drops). A probe drop
    /// that is RESTORED because the probe declined it is not counted -- the
    /// working set is the same on both sides of it, so counting it would
    /// break the net identity above.
    Index ws_drops = 0;

    /// HOMOTOPY ADMISSIONS: rows that refresh_shifts() put into the working
    /// set because they are still violated (shift > 0), a structurally
    /// different event from a blocking-constraint add -- the drop rule
    /// deliberately SKIPS shifted rows, so a row admitted this way cannot
    /// leave until its shift reaches zero. INCLUDES the pre-loop
    /// refresh_shifts() call, i.e. the seed's own initial admission, which
    /// is why a healthy solve reports a large value here on its first QP and
    /// near-zero after.
    Index shift_adds = 0;

    /// DEGENERATE PIVOTS: minor iterations that added a blocking constraint
    /// while moving the iterate by no more than the loop's own step tolerance
    /// (alpha * ||p||inf <= feas_tol * max(1, ||x||inf) -- the same threshold the
    /// loop's KKT-point test uses, so the two are consistent by construction).
    /// qp_engine.h implements no anti-cycling rule and this field changes no
    /// decision.
    Index degenerate_steps = 0;

    /// The longest consecutive run of degenerate steps in this solve. A run is
    /// broken by a minor on which the iterate moved, and by nothing else: an
    /// ordinary non-degenerate step, a taken ride and a start repair each reset
    /// it, while a drop iteration and a zero-multiplier probe do not. A large
    /// value therefore reads as "the longest stretch on which the iterate did
    /// not move". `degenerate_steps` above is unaffected by the run logic.
    Index degenerate_run_max = 0;

    // The six fields that make the above traceable rather than inferred:
    // ws_adds and ws_drops merge inequality rows with variable bounds and carry
    // no constraint identity, so re-discovery cannot be concluded from ws_adds
    // alone. These six settle it by direct measurement.

    /// Of `ws_adds`, how many were VARIABLE-BOUND pins (BlockKind::kBound).
    /// The inequality-row half is ws_adds - ws_adds_bound. `shift_adds`
    /// needs no such split: refresh_shifts() only ever adds inequality rows.
    Index ws_adds_bound = 0;

    /// Of `ws_drops`, how many released a variable bound rather than an
    /// inequality row, on both drop routes.
    Index ws_drops_bound = 0;

    /// How many DISTINCT inequality rows this solve ever put into the working
    /// set, by any route. The mean number of times a touched row was admitted is
    ///
    ///     (ws_adds - ws_adds_bound + shift_adds) / distinct_ineq_added
    ///
    /// which is ADMISSION MULTIPLICITY and counts each constraint's first
    /// admission, so RE-admissions are that ratio minus one; a reading must say
    /// which of the two it quotes.
    Index distinct_ineq_added = 0;

    /// How many DISTINCT variables this solve ever put into the working
    /// set, by ANY route; the bound-side analogue of distinct_ineq_added
    /// (admission multiplicity = ws_adds_bound / distinct_bound_added, and
    /// re-admissions are that minus one). Additionally EXCLUDES the start
    /// repair's pins -- see the named exception under the walk-counter
    /// banner above.
    Index distinct_bound_added = 0;

    /// Drop decisions (drop_worst) in which TWO OR MORE candidates fell
    /// inside the rule's relative tie window (detail::kEngineDropTieTol),
    /// i.e. decisions actually settled by the largest-angle/first-index
    /// tie-break rather than by the most-negative multiplier.
    Index drop_ties = 0;

    /// Ratio tests whose blocking constraint was chosen from TWO OR MORE
    /// candidates at EXACTLY the same minimum ratio, i.e. settled by the
    /// scan order (inequality rows before bounds, ascending index) rather
    /// than by the ratio. EXACT equality, deliberately: the tie that
    /// matters for degeneracy is several constraints at zero slack, and
    /// those compare bit-equal. NEAR-ties are not measured -- counting them
    /// needs a second pass over every candidate on every minor, a real cost
    /// in a loop that runs half a million times.
    Index ratio_ties = 0;
};

/// @brief Work counters for the semismooth-Newton kernel.
///
/// One instance lives inside SsnResult (ssn_engine.h) describing ONE QP solve; a
/// second lives inside SqpCounters below, aggregated over every subproblem of a
/// whole SQP solve. Purely observational: nothing in ssn_engine.h reads any of
/// them back. `ssn_backtracks`, `ssn_prox_updates` and `ssn_uncertain_peak`
/// belong to the safeguarded iteration alone and are structurally 0 under
/// SsnSafeguards::kBare.
struct SsnCounters {
    /// Newton steps TAKEN. The convergence test runs BEFORE each step, so a
    /// start point already inside fb_tol reports 0.
    ///
    /// It does NOT equal the factorization count: `ssn_iters <= factorizations`,
    /// because an attempt can pay its factorization and take no step, and under
    /// SsnSafeguards::kFull a certifying exit pays one more for the second-order
    /// verification. Under kBare equality holds. Anything that divides one by the
    /// other must say which it means.
    Index ssn_iters = 0;

    /// Newton steps whose implied active set differed from the preceding step's
    /// -- one per such step, not one per constraint that moved. The implied set
    /// is the partition the generalized Jacobian selects, and on the first step
    /// it is the caller's activity hint when one was supplied.
    Index ssn_bulk_flips = 0;

    /// Line-search backtracks (kFull). Structurally 0 under kBare, which
    /// takes full steps unconditionally.
    Index ssn_backtracks = 0;

    /// Proximal-center/sigma updates (kFull's proximal ladder).
    /// Structurally 0 under kBare, which has no proximal term.
    Index ssn_prox_updates = 0;

    /// Solves that ended on an SsnEscape other than kNone (ssn_engine.h).
    /// At the SsnResult scale this is 0 or 1; at the SqpCounters scale it is
    /// the number of subproblems that escaped.
    Index ssn_escapes = 0;

    /// Peak size of the UNCERTAIN SET (rows the safeguarded method declines
    /// to assign to either branch), under kFull. Structurally 0 under
    /// kBare, which has no uncertain set at all.
    Index ssn_uncertain_peak = 0;

    // The tier-3 STABLE-FACE refinement, both polarities: certifying SSN exits
    // whose identified face was re-solved exactly and whose refined point the
    // driver used as the step, and certifying exits where that solve was refused.
    // Their SUM is the number of certifying SSN exits this solve made WHEN
    // SqpOptions::ssn_certify_from_face is off (the shipped default); under that
    // lever it is an upper bound instead.
    //
    // A REFUSAL IS NOT AN ERROR: the caller keeps the certificate the SSN tier
    // already gave it. What it means is that the subproblem's complementarity is
    // back to the `fb_tol * ||lambda||inf` bound. Each refinement costs one
    // factorization, folded into `SqpCounters::factorizations`.

    /// Exact face re-solves whose refined point was used as the step; see
    /// the tier-3 note above for the sum's meaning under both lever states.
    Index ssn_refinements = 0;

    /// Certifying exits where the face re-solve was REFUSED; see the tier-3
    /// note above. Driver-scale only (no SsnResult ever writes it).
    Index ssn_refine_refused = 0;

    // INSTRUMENT ONLY, both driver-scale (no SsnResult writes either), and
    // neither is answerable by arithmetic on the pair above.
    //
    // `ssn_refine_factorizations` -- the factorizations refine_on_face ITSELF
    // paid, over every attempt, accepted or refused. NOT `ssn_refinements +
    // ssn_refine_refused`: refine_on_face short-circuits an empty face and fails
    // its rank pre-screen before any factorization, so a refusal can cost 0.
    //
    // `ssn_refine_neg_duals` -- STRICTLY NEGATIVE inequality multipliers ADOPTED
    // from an ACCEPTED refinement, summed over rows and refinements. NO
    // TOLERANCE: the test is `< 0.0`.

    /// Factorizations refine_on_face itself paid, accepted or refused; see
    /// the instrument-only note above for why it is not the pair sum.
    Index ssn_refine_factorizations = 0;

    /// Strictly negative multipliers adopted from accepted refinements,
    /// summed over rows and refinements; NO TOLERANCE (`< 0.0`); see the
    /// instrument-only note above.
    Index ssn_refine_neg_duals = 0;

    // The sign sweep repairs rather than instruments: it is the one pair here
    // that reports a value this driver changed. It runs in `SqpDriver::finish`,
    // the single export boundary, on the solution's and the warm start's
    // multipliers together, with no tolerance (the test is `< 0.0`).
    //
    // `SqpSolution::kkt` is computed before the sweep, at the multipliers the
    // solver reached, so on a solve with `ssn_sign_swept > 0` the reported
    // stationarity is optimistic by at most `ssn_sign_sweep_max * ||Ji||inf`
    // over the swept rows.
    //
    // Structurally zero under kWalk, where an active-set price is non-negative
    // by the walk's own drop rule, and hence across a restoration fold, whose
    // sub-solve is walk-only. The driver-scale values span the solve and its
    // restoration sub-solve.
    // @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h

    /// Strictly negative inequality prices clamped to 0 at export, summed
    /// over rows and over solves; NO TOLERANCE (`< 0.0`).
    Index ssn_sign_swept = 0;

    /// The largest magnitude clamped by that sweep (0.0 when nothing was
    /// swept). A PEAK, folded with `max` like `ssn_uncertain_peak` and unlike
    /// every summed field beside it -- it is the bound on how far the reported
    /// stationarity can be optimistic (see the R6 note above), which a sum
    /// would destroy.
    double ssn_sign_sweep_max = 0.0;

    // The escape-reason census. Observational, a count per reason rather than a
    // last-reason label. The six PARTITION `ssn_escapes` exactly, and that is an
    // asserted invariant. Five map one-to-one onto the non-`kNone` values of
    // `SsnEscape` (ssn_engine.h); the sixth has no `SsnEscape` value:
    //
    //   `ssn_escape_gate_refused` -- the DRIVER's own refusal, where a subproblem
    //   whose kernel reported `escape_reason == kNone` had its exit declined by
    //   `ssn_exit_is_a_usable_step` and went to the walk like an escaped one.
    //   Driver-scale only, and STRUCTURALLY ZERO on the shipped path: the gate's
    //   bound is derived from the kernel's own `|phi| <= fb_tol`, so no
    //   certifying exit reaches it. A zero here is not "the gate never refused".
    //
    // NO IN-Memory absent sentinel: the CSV boundary stamps `-1` on these six for
    // an artifact that never measured them, but these are plain `Index`, so a
    // `-1`-stamped SsnCounters folded into a running total would sum rather than
    // signal "unmeasured". A caller that folds a CSV-sourced SsnCounters must
    // check for the sentinel first.
    Index ssn_escape_budget = 0;
    Index ssn_escape_singular = 0;
    Index ssn_escape_no_contraction = 0;
    Index ssn_escape_infeasible_suspect = 0;
    Index ssn_escape_indefinite = 0;
    Index ssn_escape_gate_refused = 0;
};

// ===========================================================================
// The field tables -- one X-macro per aggregate
// ===========================================================================
// The trace's `sqp.solve.end` counters object is GENERATED from these, in table
// order, so the JSON's key order is the declaration order and a field added to a
// struct without a table entry does not silently vanish from the stream.
//
// EACH TABLE IS PINNED TO ITS STRUCT by an aggregate-arity static_assert: the
// largest N for which the struct is brace-initializable with N arguments. Not
// sizeof, which is padding-dependent.
//
// TO ADD A COUNTER: declare it in the struct, add its `X(name)` here IN THE SAME
// POSITION, and the assert passes again.

#define HVEN_SSN_COUNTERS_FIELDS(X)                                                                \
    X(ssn_iters, counter_never_absent)                                                             \
    X(ssn_bulk_flips, counter_never_absent)                                                        \
    X(ssn_backtracks, counter_never_absent)                                                        \
    X(ssn_prox_updates, counter_never_absent)                                                      \
    X(ssn_escapes, counter_never_absent)                                                           \
    X(ssn_uncertain_peak, counter_never_absent)                                                    \
    X(ssn_refinements, counter_never_absent)                                                       \
    X(ssn_refine_refused, counter_never_absent)                                                    \
    X(ssn_refine_factorizations, counter_never_absent)                                             \
    X(ssn_refine_neg_duals, counter_never_absent)                                                  \
    X(ssn_sign_swept, counter_never_absent)                                                        \
    X(ssn_sign_sweep_max, counter_never_absent)                                                    \
    X(ssn_escape_budget, counter_never_absent)                                                     \
    X(ssn_escape_singular, counter_never_absent)                                                   \
    X(ssn_escape_no_contraction, counter_never_absent)                                             \
    X(ssn_escape_infeasible_suspect, counter_never_absent)                                         \
    X(ssn_escape_indefinite, counter_never_absent)                                                 \
    X(ssn_escape_gate_refused, counter_never_absent)

// --- the tables' second column: ABSENCE ----------------------------------
// Absence is `null`, never a value, and that needs a per-FIELD answer: a
// counter's own doc says whether a reading is a value or a sentinel. These
// predicates are that answer, carried IN the table so the serializer has no
// special cases. Three fields are absent-capable: `ipqp_alpha_p_min` and
// `ipqp_alpha_d_min` at `+infinity`, and `ipqp_tier_retired_after` at `0`.
// `ipqp_final_inertia_read`'s `3` is NOT one -- it is a categorical outcome the
// census depends on.
inline bool counter_never_absent(Index) { return false; }
inline bool counter_never_absent(double) { return false; }
inline bool counter_never_absent(StartLevel) { return false; }

/// `+infinity` as "never measured" (`ipqp_alpha_p_min`, `ipqp_alpha_d_min`).
inline bool counter_absent_at_pos_inf(double v) {
    return v == std::numeric_limits<double>::infinity();
}

/// `0` as "no such major" (`ipqp_tier_retired_after`, recorded 1-based).
inline bool counter_absent_at_zero(Index v) { return v == 0; }

/// Turns one table entry into `+1`, so each table's own entry count is the
/// table itself rather than a hand-kept number beside it.
#define HVEN_COUNTERS_COUNT_ONE(f, absent) +1

/// @brief Are the offsets strictly increasing across a table's entries?
///
/// The count assert cannot see a reorder: a field inserted mid-struct whose
/// `X()` entry is appended at the tail keeps the count right and silently emits
/// a JSON key order that is no longer declaration order.
///
/// @param offsets The table's member offsets, in table order.
/// @param count   How many entries `offsets` holds.
/// @return True iff every offset exceeds the one before it.
constexpr bool counters_offsets_increase(const std::size_t *offsets, std::size_t count) {
    for (std::size_t i = 1; i < count; ++i) {
        if (!(offsets[i] > offsets[i - 1])) {
            return false;
        }
    }
    return true;
}

/// @brief `HVEN_SSN_COUNTERS_FIELDS`' entry count.
inline constexpr std::size_t kSsnCountersFieldCount =
    0 HVEN_SSN_COUNTERS_FIELDS(HVEN_COUNTERS_COUNT_ONE);
static_assert(::hven::detail::kAggregateArity<SsnCounters> == kSsnCountersFieldCount,
              "SsnCounters and HVEN_SSN_COUNTERS_FIELDS disagree: give the new field an X() entry "
              "in its declaration position (and the trace's golden line moves with it).");

/// The SsnCounters table's field offsets, in table order. `offsetof` is only
/// portable on a standard-layout type, so that is asserted first.
static_assert(std::is_standard_layout_v<SsnCounters>,
              "SsnCounters must stay standard-layout for the "
              "offsetof order check below to be portable.");
inline constexpr std::size_t kOffsetsSsnCounters[] = {
#define HVEN_COUNTERS_OFFSET_ONE(f, absent) offsetof(SsnCounters, f),
    HVEN_SSN_COUNTERS_FIELDS(HVEN_COUNTERS_OFFSET_ONE)
#undef HVEN_COUNTERS_OFFSET_ONE
};
static_assert(counters_offsets_increase(kOffsetsSsnCounters, kSsnCountersFieldCount),
              "SsnCounters and HVEN_SSN_COUNTERS_FIELDS disagree about ORDER: an entry is out of "
              "declaration position. The JSON key order is the table's, so this must be the "
              "struct's.");

/// @brief Work counters for the IP-PMM interior-point tier.
///
/// One instance describes ONE tier subproblem solve, exactly as
/// QpCounters/SsnCounters do; a second lives inside SqpCounters as `ipqp`,
/// aggregated over every subproblem by `accumulate_ipqp_counters`.
///
/// FOLD RULE, stated once here and restated at each exception's own field: every
/// `Index` field SUMS across subproblems, with four exceptions. Two per-subproblem
/// STATUS fields are OVERWRITTEN (the `SqpCounters::start_level_used` convention
/// for a categorical rather than additive reading): `ipqp_rho_demanded_last` and
/// `ipqp_final_inertia_read`. One `double` PEAK pair is max-folded
/// (`ipqp_rho_demanded_max`, `ipqp_restart_shift_max`) and one min-folded
/// (`ipqp_alpha_p_min`, `ipqp_alpha_d_min`). And one `Index` MARKER is max-folded:
/// `ipqp_tier_retired_after`, a once-per-solve major index rather than a count.
///
/// DNF SENTINEL: a sweep row for a subproblem that did not finish records its
/// double-valued fields as `1e6`. That is a convention of the sweep/CSV boundary,
/// not logic this struct implements; a live solve's counters never carry it.
struct IpqpCounters {
    /// IPQP iterations taken: one predictor+corrector pair each (spec
    /// section 3.1's Mehrotra predictor-corrector: one factorization, an
    /// affine solve, a corrector solve). Excludes a predictor attempt that
    /// fails before its corrector runs -- section 3.1 does not address the
    /// partial case, and the natural boundary is a completed pair only
    /// (T4 confirms).
    Index ipqp_iters = 0;

    /// Numeric factorizations paid. `>= ipqp_iters`, exceeding it by
    /// regularization-ladder rungs plus the section 2.2 required final
    /// unregularized inertia read. Excludes the tier-3 `refine_on_face`
    /// hand-off's own factorization, which lands in
    /// `SqpCounters::factorizations` like every other QP-engine refinement.
    Index ipqp_factorizations = 0;

    /// Symbolic analyses paid. `1` per SQP solve under the section 4.1
    /// cross-major hoisting rule while `AggregateEvalSeam::epoch()` stays
    /// unchanged. Excludes analyses paid
    /// by any other QP kernel (walk, SSN) in the same solve.
    Index ipqp_symbolic_analyses = 0;

    /// Backend triangular solves: EXACTLY 2 per completed iteration (predictor
    /// RHS, corrector RHS, both against that iteration's one numeric
    /// factorization). Regularization-ladder rungs and the final inertia read add
    /// FACTORIZATIONS but no solves, so `ipqp_solves == 2 * ipqp_iters` on a
    /// solve that completed every iteration it started, and the gap against
    /// `2 * ipqp_factorizations` is the ladder-plus-certification cost. Excludes
    /// triangular solves paid by any other QP kernel or by the tier-3 hand-off.
    Index ipqp_solves = 0;

    /// Backend pattern-verify calls (mirrors
    /// `SymmetricFactor::Counters::pattern_verify_count`).
    ///
    /// THE CONTRACT IS "exactly one of {1 verify, 1 analyze} per tier entry", not
    /// "exactly 1 verify": an entry that RE-ENTERS on an already laid pattern pays
    /// the one-time O(nnz) verify and 0 analyses, while the entry that LAYS the
    /// pattern pays 1 analysis and 0 verifies. `IpqpOptions::ipqp_hoist_symbolic
    /// == false` forces every entry into the second state. Either way every later
    /// factorization in the entry runs `kAssumeAnalyzed` and moves neither count.
    /// Excludes pattern-verify calls paid by any other QP kernel.
    Index ipqp_pattern_verifies = 0;

    /// The inertia-demanded modification `rho_dem`'s High-water mark across every
    /// ladder rung this subproblem paid. Max-folded across subproblems, so the
    /// SqpCounters-scale reading is the largest modification any subproblem was
    /// forced to. Excludes `delta`, which carries no high-water field, and the
    /// schedule `rho_sched`, which is not a modification.
    ///
    /// A high-water mark is NOT the level the solve ended at: the ladder retries a
    /// smaller shift at every iteration, so a solve routinely settles below its own
    /// peak. Structurally `0.0` on a convex subproblem, where the ladder never arms.
    double ipqp_rho_demanded_max = 0.0;

    /// The inertia-demanded modification at the LAST ladder rung this subproblem
    /// paid -- the shift the last successful modified factorization ran at, or, on
    /// an exhausted ladder, the ceiling rung the tier was refused at. An iteration
    /// that succeeded on the unmodified system pays no rung and leaves this
    /// untouched. OVERWRITTEN rather than summed or folded: it is a per-solve
    /// categorical reading, and summing many subproblems' "last rho" would report
    /// a quantity with no meaning. Excludes `delta`'s own last value, which carries
    /// no field.
    double ipqp_rho_demanded_last = 0.0;

    /// Factorizations REJECTED on wrong OR evidence-invalid inertia (the section
    /// 2.2 gate), which the gate refuses alike: a WRONG reading (observed,
    /// disagreed with the required signature) and a PERTURBED one (observed, but
    /// describing a matrix the backend perturbed rather than the one assembled)
    /// both cost a factorization the tier could not use. The ladder's last,
    /// ceiling-terminated factorization counts too. Excludes the required final
    /// read, whose outcome is `ipqp_final_inertia_read`, even when that read comes
    /// back wrong.
    Index ipqp_inertia_retries = 0;

    /// Iterations whose step was TAKEN with a nonzero inertia-demanded
    /// modification in force (`rho_dem > 0`), measured AFTER that iteration's
    /// ladder settles, so the iteration in which the ladder first demands a
    /// modification is counted. Excludes iterations whose step ran on the
    /// unmodified system, which is every iteration of every convex subproblem.
    Index ipqp_iters_at_elevated_rho = 0;

    /// LADDER RECLIMBS: iterations on which a `rho_dem_last / kIpqpLadderDown`
    /// value was REJECTED on a wrong inertia and the ladder had to re-escalate
    /// past it. One per iteration, never per rung -- the quantity is "how often
    /// did the memory's guess come back too small", not how far the climb went.
    /// Both routes to that value are charged: as an iteration's first trial once
    /// `kIpqpLadderSkipAfter` consecutive iterations have needed a modification,
    /// and as the rung that answers a refused zero-trial before then.
    ///
    /// AN EXPECTED COST, NOT A FAULT: the ladder deliberately re-tries a smaller
    /// shift than the one that last worked, so near the threshold the accepted
    /// values cycle with a reclimb every second iteration or so. Read it against
    /// `ipqp_reg_increases` and `ipqp_reg_decreases`.
    ///
    /// Structurally 0 on a convex subproblem. Excludes the FIRST climb of a solve
    /// (no memory to have guessed with), an iteration whose value was accepted,
    /// every inertia-demanded increase as such, and a PERTURBED-DRIVEN escalation
    /// -- which is not a statement about curvature and is visible instead through
    /// `ipqp_pivot_reroute_primal`.
    Index ipqp_ladder_reclimbs = 0;

    /// Ladder rungs taken because a PERTURBED-PIVOT report arrived while the
    /// primal ladder was ARMED, i.e. answered by escalating `rho_dem` rather than
    /// `delta`. Counts RUNGS, not iterations: the question is how much
    /// factorization budget the re-route spends.
    ///
    /// The re-route exists because the modification is a uniform shift of the
    /// Ruiz-scaled system, whose dominant diagonal Ruiz normalizes to almost
    /// exactly -1 while `rho_dem = 1` is an early rung -- so a rung can ANNIHILATE
    /// a scaled pivot, and that singularity is PRIMAL.
    ///
    /// Structurally 0 on any solve whose ladder never arms, and on any backend
    /// that never reports a perturbed pivot. Excludes a perturbed report received
    /// at `rho_dem == 0` (the ordinary dual route, counted nowhere), the rungs the
    /// fallback then takes, and every wrong-inertia rung.
    Index ipqp_pivot_reroute_primal = 0;

    /// Times the bounded primal re-route above GAVE UP and fell back to escalating
    /// `delta` -- `kIpqpPivotReroutePrimalMax` consecutive primal escalations
    /// failed to clear the perturbation report. Counts FALLBACK EVENTS, one per
    /// exhausted run, not the dual rungs that follow.
    ///
    /// The bound is an approximation of pivot provenance, and this field measures
    /// its accuracy: `hven::linear::InertiaEvidence` carries pivot counts and no
    /// pivot locations, so two consecutive failed primal escalations stands in for
    /// asking which block was perturbed. A high ratio of fallbacks to primal
    /// re-routes says the heuristic is guessing wrong often enough to replace.
    ///
    /// Structurally 0 wherever `ipqp_pivot_reroute_primal` is 0.
    Index ipqp_pivot_reroute_dual_fallback = 0;

    /// Iterations that ARMED the ladder (`rho_dem > 0` at the settled reading,
    /// step taken) and on which the section 3.2 gate did NOT advance -- the direct
    /// instrument for the negative-curvature walk, whose residual is not monotone:
    /// along a direction where `H + rho_sched I + Sigma` still has negative
    /// curvature the residual grows, so the contraction gate is silent for the
    /// whole walk and then advances in one step when the curvature turns.
    ///
    /// Structurally 0 on a convex subproblem. Excludes an armed iteration on which
    /// the gate did advance, an unarmed iteration either way, and any iteration
    /// whose step was not taken.
    Index ipqp_iters_ladder_armed_no_advance = 0;

    /// Outcome of the REQUIRED final unregularized inertia read on a certifying
    /// exit. A per-solve categorical reading: OVERWRITTEN by
    /// `accumulate_ipqp_counters`, the `SqpCounters::start_level_used` convention,
    /// not an additive quantity. The values map onto the escape census exactly:
    ///
    ///   `0` the reading was taken and AGREED: the certificate stands, no escape.
    ///   `1` the reading was taken and DISAGREED -- at the final certification
    ///       factorization, or with the ladder at `ipqp_reg_max` and the reading
    ///       still wrong: a saddle-suspect downgrade, `ipqp_escape_indefinite`.
    ///   `2` UNREADABLE: the read was ATTEMPTED and no usable evidence came back,
    ///       including a perturbed-pivot report, which is not evidence about the
    ///       assembled matrix and so not a disagreement either --
    ///       `ipqp_escape_numerical`.
    ///   `3` NOT PERFORMED: no factorization was taken for the read, because
    ///       `IpqpOptions::ipqp_require_final_inertia` is false or because
    ///       `ipqp_max_factorizations` refused it. Always a downgrade and never its
    ///       own escape -- `kNone` on the option-off path, `kBudget` on the
    ///       cap-refused one. Kept distinct from `2` so the census cannot confuse a
    ///       read that never ran with one that ran and failed.
    ///
    /// Structurally `0` on a subproblem that never reached a certifying exit, since
    /// the read is paid only there. Excludes every inertia read paid mid-ladder,
    /// which is `ipqp_inertia_retries`.
    ///
    /// `0` Says this read agreed, which is not by itself that the certificate
    /// stands: the evidence-failure policy downgrades a certificate for the whole
    /// solve when any earlier factorization could not report usable evidence, so
    /// `0` can pair with `certificate_downgraded == true`. The certificate is read
    /// off `certificate_downgraded`, never off this field alone.
    Index ipqp_final_inertia_read = 0;

    /// `(rho, delta)` schedule GATED decreases actually applied: +1 per gated
    /// advance that moved EITHER `rho` or `delta`. Never more than 1 per advance,
    /// so it is bounded by `ipqp_prox_center_updates`, and
    ///
    ///     ipqp_prox_center_updates == ipqp_reg_decreases
    ///                                 + (advances that moved nothing)
    ///
    /// where the remaining class is a decrease refused by the ABSOLUTE
    /// `ipqp_reg_floor`. The inertia ladder contributes to neither side: it moves
    /// `rho_dem`, which the schedule never sees.
    Index ipqp_reg_decreases = 0;

    /// `(rho, delta)` schedule inertia-demanded increases. Excludes the
    /// initial `rho_0`/`delta_0` assignment at subproblem start (section
    /// 3.2), which is not an increase.
    Index ipqp_reg_increases = 0;

    /// Proximal-estimate (`zeta`/`lambda_est`) advances. Excludes the
    /// initial `zeta_0 = x_0`, `lambda_est_0 = y_0` assignment (section
    /// 3.2), which is not an advance.
    Index ipqp_prox_center_updates = 0;

    /// Warm restarts (section 5.2) whose repair moved at least one
    /// component of the ingested seed (a strict-positivity clamp or the
    /// two-scalar shift). Excludes a warm restart whose seed needed no
    /// repair at all.
    Index ipqp_restart_repairs = 0;

    /// The LARGEST repair shift (section 5.2's `(delta_p, delta_d)`) applied
    /// to any component of any warm restart's seed; `0.0` when no restart
    /// was ever repaired. MAX-FOLDED across subproblems in
    /// `accumulate_ipqp_counters` (model: `ssn_sign_sweep_max`) -- the
    /// honest-magnitude field, not a sum, exactly like that field. Excludes
    /// a cold-started subproblem, which pays no repair and never touches
    /// this field.
    double ipqp_restart_shift_max = 0.0;

    /// `1` iff the payload `mu` raised `mu_0` off the measured floor (the clamp
    /// `mu_0 = clamp(max(mu_meas, kappa*mu_payload), min, init)`, with the payload
    /// term binding), else `0`. A per-subproblem flag SUMMED like a count, the
    /// `SqpCounters::n_seeded` convention. Excludes a cold-started subproblem,
    /// which has no payload `mu` and is structurally `0`. A statement about the
    /// CLAMP, not the trajectory: an adoption that binds can still leave the solve
    /// bit-identical.
    Index ipqp_mu_adopted = 0;

    /// `1` iff the section 5.5 warm-kill fired on this subproblem (a warm
    /// restart overran its clamped budget and was restarted cold exactly
    /// once), else `0`. Excludes a cold-started subproblem (structurally
    /// `0`, no warm restart to abandon) and a warm restart that stayed
    /// inside its budget.
    Index ipqp_warm_restart_abandoned = 0;

    /// Subproblems the section 2.3/4b domain gate declined pre-solve because
    /// the effective box (`IpqpBounds`) contained a zero-width pair.
    /// A decline is not an escape: the tier never
    /// ran, so this never counts toward `ipqp_escapes` or the K=3
    /// retirement threshold, and the walk solves the declined subproblem
    /// exactly. Excludes every subproblem the tier actually entered,
    /// however it then concluded.
    Index ipqp_declined_pinned = 0;

    /// The major at which K = 3 consecutive escapes retired the tier for the
    /// remainder of this solve; `0` if the tier was never retired.
    ///
    /// A MARKER, NOT A COUNT: retirement fires at most once per solve, and "any
    /// success resets the count" resets only the consecutive-escape tally toward a
    /// future retirement. FOLDED BY MAX, the peak fields' discipline:
    /// order-independent, `0` is the fold identity, and summing two nonzero
    /// readings would report an impossible major.
    ///
    /// Driver-scale only: no per-subproblem read ever carries it nonzero, since
    /// retiring the tier is bookkeeping ACROSS subproblems. Max-folded anyway, for
    /// the one real call site (a restoration sub-solve's totals folding onto an
    /// already-populated running total). Excludes every major before retirement
    /// fired, and a solve whose tier was declined-pinned throughout without ever
    /// accumulating three consecutive genuine escapes.
    Index ipqp_tier_retired_after = 0;

    /// Rows/bounds the section 2.3 ratio rule left UNCERTAIN (neither
    /// classification test satisfied), rather than asserted either way.
    /// ACCUMULATED ON EVERY EXIT, including escapes: the classifier runs on
    /// the returned point whatever the outcome was, so this is the population
    /// the RULE left uncertain, and the population `refine_on_face` was
    /// actually handed is the subset that reached it (the routing's rows 2a-2c
    /// -- an escaped subproblem's face is never handed on). Excludes
    /// rows/bounds the ratio rule classified definitively active or inactive.
    Index ipqp_face_uncertain = 0;

    /// Tier-3 `refine_on_face` hand-offs ACCEPTED as the step. EVERY usable
    /// tier exit is handed on (section 2.3 item 3 -- tier 3 owns the last two
    /// decades, whether or not the ratio rule left anything uncertain), so
    /// this excludes only a subproblem that never produced a usable exit: a
    /// decline, a retired-tier major, or a genuine escape. A refusal is
    /// `ipqp_refine_refused` instead.
    Index ipqp_refine_accepted = 0;

    /// Tier-3 `refine_on_face` hand-offs REFUSED (empty/rank-deficient face,
    /// a failed inertia gate, or the refined point leaving the box/TR/
    /// inactive rows) -- the certificate the tier already had stands; see
    /// `SsnCounters::ssn_refine_refused` for the identical convention on the
    /// SSN tier. Excludes an acceptance, which is `ipqp_refine_accepted`
    /// instead, and a subproblem that never reached tier-3.
    Index ipqp_refine_refused = 0;

    /// Subproblems the routing chain handed to the tier-3 `refine_on_face`
    /// step (section 2.3 item 3) -- the FIRST destination of every usable tier
    /// exit, which is exactly `ipqp_refine_accepted + ipqp_refine_refused`.
    ///
    /// It exists to close the routing partition. Without it the group has no
    /// term for the refinement destination, so "every consulted subproblem
    /// went somewhere" cannot be stated as arithmetic -- and two rows falsify
    /// the two-term version: a converged `kBudget` exit whose section 2.2 item
    /// 4 read the factorization budget refused is counted in
    /// `ipqp_escape_budget` and routed HERE, not to the walk. Excludes a
    /// declined or retired-major subproblem (the tier produced no exit to
    /// refine) and a genuine escape (`ipqp_to_walk`, or `ipqp_to_ssn` for the
    /// saddle-suspect one).
    Index ipqp_to_refine = 0;

    /// Subproblems handed to the SSN warm-grade path -- section 2.3 item 4's
    /// TWO feeders, so this is exactly `ipqp_refine_refused +
    /// ipqp_escape_indefinite`: a `refine_on_face` refusal, and a
    /// saddle-suspect (`IpqpEscape::kIndefinite`) exit, which goes to SSN
    /// directly without a refinement attempt. Excludes a `refine_on_face`
    /// acceptance (`ipqp_refine_accepted`), which is the step and reaches no
    /// further routing step.
    Index ipqp_to_ssn = 0;

    /// Routing outcomes handed to the walk: a genuine tier escape, which goes COLD
    /// with the iterate discarded, or a declined-pinned subproblem re-routed
    /// pre-solve, which goes to the ORDINARY seeded walk because the tier never
    /// ran. Excludes a hand-off to SSN or to the refinement, a SADDLE-SUSPECT
    /// escape (which goes to SSN), and the converged `kBudget` exit that is counted
    /// in the escape census and routed to the refinement -- so
    /// `ipqp_escapes - ipqp_to_walk` is not a meaningful quantity on its own.
    ///
    /// The closed statement is over first destinations, and it is not the raw
    /// three-term sum:
    ///
    ///     ipqp_to_refine + ipqp_escape_indefinite
    ///                    + (ipqp_to_walk - ipqp_declined_pinned)
    ///         == the subproblems the tier was CONSULTED on
    ///
    /// `ipqp_to_ssn` cannot stand in it -- a refinement refusal reaches SSN as a
    /// SECOND destination and is already in `ipqp_to_refine` -- and
    /// `ipqp_declined_pinned` is subtracted because a decline is counted here
    /// without the tier having run. `tests/sqp/support/ipqp_test_support.h`'s
    /// `assert_ipqp_routing_partition` asserts exactly this.
    Index ipqp_to_walk = 0;

    /// Subproblems the tier ESCAPED (any of the five reasons below), summed
    /// across the whole solve. Excludes declined-pinned subproblems -- see
    /// `ipqp_declined_pinned` above, which the tier never entered.
    Index ipqp_escapes = 0;

    // The five-way escape census. The five MUST SUM TO `ipqp_escapes`: each
    // subproblem escape increments exactly one of the five and `ipqp_escapes`
    // together. Each field states its own count and what it excludes -- always
    // "the other four", stated once here rather than five times.
    //
    // `ipqp_escape_stall` is escape-COUNT only: its three conjunct values travel
    // in the stall escape's own evidence block, never as counters here.

    /// Escapes via the section 6.1 hard iteration cap, `IpqpEscape::kBudget`
    /// -- of LAST RESORT (section 6.1: the stall test below should fire
    /// first on anything genuinely stuck). Excludes an escape whose stall
    /// test fired first, which is `ipqp_escape_stall` instead (section 6.1
    /// states budget is of last resort precisely so stall pre-empts it),
    /// and excludes the section 5.5 warm-kill budget -- a warm restart's
    /// own budget, not the tier's iteration cap, and tracked separately as
    /// `ipqp_warm_restart_abandoned`, which is not an escape at all.
    Index ipqp_escape_budget = 0;

    /// Escapes via the section 6.2 early-stall test (the `mu`/residual/
    /// min-alpha window, all three conjuncts required). Excludes the
    /// DROPPED `ipqp_stall_reason_mu/_residual/_alpha` sub-counters (note
    /// b) -- the three conjunct values travel in the stall escape's own
    /// evidence block instead, never as counters here.
    Index ipqp_escape_stall = 0;

    /// Escapes via `IpqpEscape::kIndefinite`: an inertia reading was READ (an
    /// evidence state was observed) and DISAGREED with the required signature --
    /// at the final certification factorization on an otherwise-converged point,
    /// or with the ladder at `ipqp_reg_max` and the reading still wrong. This is
    /// `ipqp_final_inertia_read == 1`, a saddle-suspect downgrade that routes to
    /// the SSN warm grade. Excludes an inertia-gate failure mid-ladder
    /// (`ipqp_inertia_retries`, which retries rather than escaping) and an
    /// UNREADABLE reading, which is `ipqp_escape_numerical`.
    Index ipqp_escape_indefinite = 0;

    /// Escapes that stop the tier for a non-convergence reason other than budget,
    /// stall or infeasible-suspect: a factorization failure, an UNREADABLE inertia
    /// reading (`ipqp_final_inertia_read == 2`), or a non-finite
    /// iterate/residual/step. The complement of `ipqp_escape_indefinite` within
    /// "the reading was inertia-related": indefinite means a reading was taken and
    /// disagreed, numerical means no reading could be taken at all or the failure
    /// was not an inertia reading. Excludes an indefinite-certificate escape.
    Index ipqp_escape_numerical = 0;

    /// Escapes via `IpqpEscape::kInfeasibleSuspect` (section 6.3's
    /// two-conjunct test: primal residual flat on a positive floor AND
    /// `||(y, z)||` growth over the window), carried with its own evidence
    /// block (`IpqpResult::infeasibility_evidence`). Excludes a
    /// certificate: the tier never returns `QpStatus::kInfeasible` (section
    /// 6.3) -- only this escape signature.
    Index ipqp_escape_infeasible_suspect = 0;

    /// The smallest PRIMAL fraction-to-boundary step taken, across every iteration
    /// of every subproblem. MIN-FOLDED across subproblems. Defaults to
    /// `+infinity`, NOT `0.0`: valid steps lie in `(0, 1]`, so a `0.0` default
    /// would be indistinguishable from an observed step and `std::min` would never
    /// move off it. `+infinity` reads as "no step observed yet". Excludes a
    /// declined-pinned subproblem, which the tier never enters.
    double ipqp_alpha_p_min = std::numeric_limits<double>::infinity();

    /// The DUAL-side counterpart of `ipqp_alpha_p_min`: same fold and same
    /// `+infinity` default, for the same reason. Excludes a declined-pinned
    /// subproblem (`ipqp_declined_pinned`), which the tier never enters and
    /// so never takes a fraction-to-boundary step -- the same exclusion
    /// `ipqp_alpha_p_min` states for itself.
    double ipqp_alpha_d_min = std::numeric_limits<double>::infinity();

    /// KEPT bound sides in the final read's disclosure band. Additive fold. A
    /// 0/0 corpus reading is expected on near-equality-constrained first QPs;
    /// the field is meaningful on activity-taxonomy and path-bound cells.
    Index ipqp_read_kept_tight_sides = 0;

    /// Band-counted sides `ipqp_classify_barrier_noise` (ipqp_math.h) classifies
    /// `kSuspect`; `0` on the band-only fallback. Additive fold, and the same
    /// corpus caveat as `ipqp_read_kept_tight_sides` above.
    Index ipqp_read_barrier_noise_sides = 0;
};

#define HVEN_IPQP_COUNTERS_FIELDS(X)                                                               \
    X(ipqp_iters, counter_never_absent)                                                            \
    X(ipqp_factorizations, counter_never_absent)                                                   \
    X(ipqp_symbolic_analyses, counter_never_absent)                                                \
    X(ipqp_solves, counter_never_absent)                                                           \
    X(ipqp_pattern_verifies, counter_never_absent)                                                 \
    X(ipqp_rho_demanded_max, counter_never_absent)                                                 \
    X(ipqp_rho_demanded_last, counter_never_absent)                                                \
    X(ipqp_inertia_retries, counter_never_absent)                                                  \
    X(ipqp_iters_at_elevated_rho, counter_never_absent)                                            \
    X(ipqp_ladder_reclimbs, counter_never_absent)                                                  \
    X(ipqp_pivot_reroute_primal, counter_never_absent)                                             \
    X(ipqp_pivot_reroute_dual_fallback, counter_never_absent)                                      \
    X(ipqp_iters_ladder_armed_no_advance, counter_never_absent)                                    \
    X(ipqp_final_inertia_read, counter_never_absent)                                               \
    X(ipqp_reg_decreases, counter_never_absent)                                                    \
    X(ipqp_reg_increases, counter_never_absent)                                                    \
    X(ipqp_prox_center_updates, counter_never_absent)                                              \
    X(ipqp_restart_repairs, counter_never_absent)                                                  \
    X(ipqp_restart_shift_max, counter_never_absent)                                                \
    X(ipqp_mu_adopted, counter_never_absent)                                                       \
    X(ipqp_warm_restart_abandoned, counter_never_absent)                                           \
    X(ipqp_declined_pinned, counter_never_absent)                                                  \
    X(ipqp_tier_retired_after, counter_absent_at_zero)                                             \
    X(ipqp_face_uncertain, counter_never_absent)                                                   \
    X(ipqp_refine_accepted, counter_never_absent)                                                  \
    X(ipqp_refine_refused, counter_never_absent)                                                   \
    X(ipqp_to_refine, counter_never_absent)                                                        \
    X(ipqp_to_ssn, counter_never_absent)                                                           \
    X(ipqp_to_walk, counter_never_absent)                                                          \
    X(ipqp_escapes, counter_never_absent)                                                          \
    X(ipqp_escape_budget, counter_never_absent)                                                    \
    X(ipqp_escape_stall, counter_never_absent)                                                     \
    X(ipqp_escape_indefinite, counter_never_absent)                                                \
    X(ipqp_escape_numerical, counter_never_absent)                                                 \
    X(ipqp_escape_infeasible_suspect, counter_never_absent)                                        \
    X(ipqp_alpha_p_min, counter_absent_at_pos_inf)                                                 \
    X(ipqp_alpha_d_min, counter_absent_at_pos_inf)                                                 \
    X(ipqp_read_kept_tight_sides, counter_never_absent)                                            \
    X(ipqp_read_barrier_noise_sides, counter_never_absent)

/// @brief `HVEN_IPQP_COUNTERS_FIELDS`' entry count.
inline constexpr std::size_t kIpqpCountersFieldCount =
    0 HVEN_IPQP_COUNTERS_FIELDS(HVEN_COUNTERS_COUNT_ONE);
static_assert(::hven::detail::kAggregateArity<IpqpCounters> == kIpqpCountersFieldCount,
              "IpqpCounters and HVEN_IPQP_COUNTERS_FIELDS disagree: give the new field an X() "
              "entry in its declaration position (and the trace's golden line moves with it).");

/// The IpqpCounters table's field offsets, in table order. `offsetof` is only
/// portable on a standard-layout type, so that is asserted first.
static_assert(std::is_standard_layout_v<IpqpCounters>,
              "IpqpCounters must stay standard-layout for the "
              "offsetof order check below to be portable.");
inline constexpr std::size_t kOffsetsIpqpCounters[] = {
#define HVEN_COUNTERS_OFFSET_ONE(f, absent) offsetof(IpqpCounters, f),
    HVEN_IPQP_COUNTERS_FIELDS(HVEN_COUNTERS_OFFSET_ONE)
#undef HVEN_COUNTERS_OFFSET_ONE
};
static_assert(counters_offsets_increase(kOffsetsIpqpCounters, kIpqpCountersFieldCount),
              "IpqpCounters and HVEN_IPQP_COUNTERS_FIELDS disagree about ORDER: an entry is out of "
              "declaration position. The JSON key order is the table's, so this must be the "
              "struct's.");

/// @brief Aggregate work counters for a whole solve.
///
/// major_iters counts SUBPROBLEMS SOLVED -- not iterates evaluated, and not steps
/// accepted: a subproblem that fails is counted, because the work was spent. A
/// solve that converges immediately reports 0 and a one-entry history. A SOC
/// re-solve is not one of these subproblems: it is tied to the trial that
/// triggered it and its cost is folded into qp_minor_iters/factorizations.
///
/// The history length depends on where the solve stopped, and a consumer indexing
/// into it must branch on that:
/// - stopped AT an iterate (kOptimal, kMaxIter, the non-finite-iterate
///   kNumericalError): history.size() == major_iters + 1, the last row has
///   qp_solved == false;
/// - stopped ON a subproblem (a QP-propagated kNumericalError, or any of the three
///   kRestore routes): history.size() == major_iters, every row has qp_solved ==
///   true, and the last row carries the failing qp_status -- except on the
///   kRestore routes, where `verdict` carries the reason.
/// So `history[counters.major_iters]` is in bounds on the first family and out of
/// bounds on the second. Use history.back() and read qp_solved.
///
/// ON A Terminal restoration exit, SqpSolution::x is NOT the point the last
/// history row describes: the row is the iterate that RAISED the request, while
/// x/f/lambda_* are the RESTORED point the phase ended at, which has no row of its
/// own. The two coincide only when the phase never moved the iterate.
///
/// qp_minor_iters and factorizations are plain SUMS of the corresponding
/// QpCounters fields over every subproblem solved, carrying that header's
/// semantics unchanged; schur_updates is deliberately NOT aggregated. Both sums
/// can EXCEED sum(history[k].qp_*): a SOC re-solve's cost is folded in here but
/// deliberately not into the triggering row, whose qp_* fields stay the ORIGINAL
/// solve's, so the difference is exactly the SOC re-solve's own cost.
///
/// rejected_steps counts the majors spent shrinking the radius at an iterate that
/// did not move: a strategy-rejected trial, and a subproblem that failed but
/// returned a usable iterate. Tell them apart by the row's qp_status.
/// steps_accepted counts the complementary case -- trials the strategy accepted,
/// which is the number of times the iterate moved on the caller's own problem, and,
/// with one exception, the number of eval_hess calls the optimality phase makes.
///
/// THE EXCEPTION: a solve that exits without ever building a subproblem pays ONE
/// eval_hess in make_warm_start, so the exact optimality-phase count is
/// steps_accepted + [major_iters == 0]. It is also not the number of eval_hess
/// calls the solve makes on the MODEL: a restoration major costs one too, so a
/// consumer budgeting Hessian evaluations should use
/// steps_accepted + restoration_iters + 1 as the unconditional upper bound.
///
/// BOTH ACCEPT/Reject counters are counted explicitly because the obvious identity
/// IS FALSE. In general
///     major_iters = steps_accepted + rejected_steps + (kRestore rows),
/// where the last term counts both the terminal request and every request the
/// phase RESUMED from.
///
/// soc_steps counts Second-order correction attempts -- once per qualifying
/// kReject trial, whatever the outcome -- and soc_applied/soc_qp_infeasible/
/// soc_rejected break that number down by outcome, so
///     soc_steps == soc_applied + soc_qp_infeasible + soc_rejected
/// on every solve: applied when the re-solve reached kOptimal and the strategy
/// accepted the corrected point, qp_infeasible when the re-solve itself did not
/// reach kOptimal, rejected when it did and the strategy still turned the point
/// down. All four fold in from a restoration sub-solve.
///
/// elastic_activations counts subproblems reformulated elastically -- once per
/// trial whose QP returned kInfeasible -- so it is also an exact count of the
/// kInfeasible subproblems the WALK route met. The certified fallback reformulates
/// on the interior-point tier's evidence instead, with no kInfeasible QP in front
/// of it, and `elastic_from_ipqp_escape` counts those, so the two split this total.
///
/// elastic_escalations counts rho ESCALATIONS of the SAME elastic subproblem,
/// summed over activations -- not the number of elastic solves, which is
/// elastic_activations + elastic_escalations. Bounded by 6 per activation, or
/// fewer when evidence places the first rung higher, and an UPPER bound only once
/// SqpOptions::elastic_ladder_early_exit is opted in. Their
/// qp_minor_iters/factorizations fold into the aggregate sums.
///
/// restoration_iters counts major iterations spent inside the restoration phase --
/// subproblems solved on the feasibility problem. These majors are NOT in
/// major_iters and have no history rows; the two counters partition the QP solves
/// a driver spends on models, and they share the max_iter budget. The sub-solve's
/// qp_minor_iters and factorizations ARE folded into this struct's aggregates.
///
/// A nonzero restoration_iters does not imply a failed solve: a solve that
/// restores and then converges reports kOptimal with restoration_iters > 0. It is
/// the pairing with SqpStatus::kInfeasible that says otherwise.
/// @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h
struct SqpCounters {
    Index major_iters = 0;
    Index qp_minor_iters = 0;
    Index factorizations = 0;
    Index steps_accepted = 0;
    Index rejected_steps = 0;
    Index soc_steps = 0;
    Index soc_applied = 0;
    Index soc_qp_infeasible = 0;
    Index soc_rejected = 0;
    Index elastic_activations = 0;
    Index elastic_escalations = 0;
    Index restoration_iters = 0;

    // The certified fallback'S PARTITION. The counters below are written by
    // `certified_feasibility_fallback`, the one judge and the only site that
    // writes any of them, and partition its ENTRIES:
    //
    //     ipqp_suspicion_disproved + <relaxed> + <exhausted> + ipqp_fallback_rung_b
    //         == the fallback entries whose evidence block FIRED
    //         == elastic_from_ipqp_escape - elastic_floor_retries
    //
    // where <relaxed> and <exhausted> carry no counter of their own and are read
    // off the returned `qp_status`. An entry whose block never fired charges none
    // of them: it is a single cold walk, outside the partition by construction.

    /// Elastic ACTIVATIONS this solve owes to the certified fallback rather than
    /// to a walk `kInfeasible`: every ladder the fallback ran, an entry's first
    /// attempt and its floor retry alike. Both routes increment
    /// `elastic_activations`, so
    ///
    ///     elastic_activations == <walk-route activations> + elastic_from_ipqp_escape
    ///
    /// and the walk-route count is that difference. Excludes every activation the
    /// driver's own elastic branch raised on a walk `kInfeasible`, and an entry
    /// whose block never fired. It counts ACTIVATIONS, not entries.
    Index elastic_from_ipqp_escape = 0;

    /// Fallback entries whose rung A came back with CLOSED slacks -- the suspicion was FALSE and
    /// the elastic answer IS the unrelaxed subproblem's (the l1 exact-penalty property). The one
    /// number that says whether spec section 6.3's two-conjunct detector is calibrated:
    /// `ipqp.ipqp_escape_infeasible_suspect` counts the SUSPICION, this counts its FATE.
    ///
    /// EXCLUDES the other three arms of the partition above -- in particular a rung B that
    /// disproves the suspicion on its own (its walk solves the original QP and returns
    /// kOptimal), which this counter cannot see and `ipqp_fallback_rung_b` counts instead.
    Index ipqp_suspicion_disproved = 0;

    /// Fallback entries that fell through to RUNG B, the cold walk: rung A was DECLINED by the
    /// engine (a non-kOptimal ladder exit), and -- when the placement was above the floor -- so
    /// was its one retry there. The refusal path's own frequency, and the fourth arm of the
    /// partition above.
    ///
    /// EXCLUDES an entry whose evidence never FIRED (rung B runs, but rung A was never entered,
    /// so the entry is outside the partition) and the other three arms.
    Index ipqp_fallback_rung_b = 0;

    /// Ladders entered at a CLAMPED first rung -- `ElasticLadderReport::
    /// rho0_ceiling_hit` summed over every activation, on both routes into
    /// `run_elastic_ladder`. A clamp means the evidence priced the violation above
    /// what the placement rule allows: the escalation headroom cap
    /// `kElasticRhoMax / kElasticRhoFactor`, or the dual-regularization safety cap
    /// `kElasticRhoDualMuSafety / dual_mu`. Excludes the ordinary placements, and
    /// is identically 0 on the no-evidence route, which is placed at the floor.
    Index elastic_rho0_ceiling_hits = 0;

    /// Rung-A RETRIES AT THE FLOOR: entries where a DECLINED rung A placed above
    /// `kElasticRhoInit` was re-run once at the floor before rung B was considered. Each retry
    /// is a second, real activation and is counted in `elastic_activations` and in
    /// `elastic_from_ipqp_escape` too -- the extra cost the rule is worth reporting -- while the
    /// ENTRY partition above counts the RETRY's own outcome, never the declined attempt as well.
    ///
    /// EXCLUDES a decline already AT the floor (there is nothing to retry) and every rung A the
    /// engine did not decline. Bounded by 1 per fallback entry.
    Index elastic_floor_retries = 0;

    /// EQP refinement work, summed over every QP solve this driver spent --
    /// subproblems, SOC re-solves, elastic rungs and the restoration sub-solve
    /// alike, exactly like qp_minor_iters/factorizations. The two fields carry
    /// QpCounters' meanings unchanged, including that eqp_refine_steps is
    /// identically 0.
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    /// Verdict-site face refinement steps kept, summed over the same set of QP
    /// solves as the two fields above; QpCounters::verdict_refine_steps
    /// carries the meaning unchanged, COUNTS and EXCLUDES included.
    Index verdict_refine_steps = 0;

    /// Rungs of the QP engine's Suspect-stall escalation ladder (qp_engine.h
    /// section 4b), summed over the same set of QP solves as the refinement
    /// counters above and named identically to QpCounters::suspect_escalations.
    ///
    /// Zero is the expected reading and a nonzero one is a finding, not a
    /// statistic: a rung is spent only when a would-be-kOptimal exit off a suspect
    /// factorization failed the free-block stationarity check. Nonzero-but-solved
    /// means the engine escalated its way out; a kNumericalError paired with
    /// detail::kMaxSuspectEscalations rungs is the exhaustion diagnostic, and it is
    /// the only place the driver's caller can see why the subproblem was refused.
    Index suspect_escalations = 0;

    /// Pardiso phase-11 (symbolic analysis) calls paid through the border/rebuild
    /// PATH, summed over the same set of QP solves as the counters above and named
    /// identically to QpCounters::symbolic_analyses.
    ///
    /// Not a count of every phase-11 call this solve paid: the elimination path
    /// constructs its own per-solve KktFactor and pays its own analyses through it,
    /// which this field does not see and so under-reports on a solve that uses it.
    /// An ordinary driver keeping a single unshared BorderState should see this
    /// stay small (1, or 0 on an immediate-convergence solve) across the whole
    /// solve regardless of how many factorizations were paid; a large value on an
    /// otherwise-ordinary solve is the regression signal it exists to catch.
    Index symbolic_analyses = 0;

    /// The RESOLVED warm-start level this solve actually used -- not the level a
    /// caller merely requested by populating `warm`.
    ///
    /// Always kCold on the 2-argument solve(model, x0) overload. On the 3-argument
    /// overload: kCold when `warm.valid` is false, when `warm` is not dimensionally
    /// compatible, when any ingested vector is non-finite, when the seeded dual
    /// clamp DEGRADED the object, or when SqpOptions::start_level caps it there.
    ///
    /// A HASH MISMATCH -- the `0` "no model was seen" sentinel included -- lands on
    /// kSeeded, which takes the object's values and refuses its
    /// provenance-dependent state; a `start_level` ceiling of kSeeded produces the
    /// same from an object that would otherwise have earned more. Then kWarm when a
    /// structural match was confirmed and the ceiling allows it but either
    /// `warm.hot` was null or the first subproblem did not actually reuse the
    /// cache; kHot exactly when the match was confirmed, the ceiling allows it,
    /// `warm.hot` was non-null AND that subproblem's own QpCounters::k0_reused
    /// reads true. This field records what WAS OBSERVED to happen, not what was
    /// offered, and reading `k0_reused` rather than inferring reuse from a zero
    /// factorization count is deliberate.
    StartLevel start_level_used = StartLevel::kCold;

    /// Majors solved while globalization.h's FULL-STEP MODE was armed
    /// (SqpOptions::warm_full_step). It counts the same events major_iters does --
    /// subproblems solved -- so it is always <= major_iters, and 0 on every cold
    /// solve, every solve with the lever off and every solve whose `warm` did not
    /// resolve. It is not a count of UNIT STEPS taken: a trial the mode declined to
    /// accept (only a non-finite trial point does that) is counted here too.
    Index full_step_majors = 0;

    /// WATCHDOG RESTORES: how many times the driver ended the full-step mode by
    /// restoring the best iterate it had seen under it (by ||KKT||inf) and handing
    /// the solve back to funnel globalization. Bounded by 1 today -- the mode is
    /// entered once per solve -- so the useful reading is BINARY: 0 means the mode
    /// never engaged or ran to the convergence test, 1 means it was cut short.
    /// A restore is counted even when it moves nothing: the EVENT is what this
    /// counts.
    Index watchdog_restores = 0;

    // EVAL ECONOMICS. evals_full is a query that fetched derivatives (whether or
    // not it also read values first); evals_values is a query that fetched f/cE/cI
    // only and never got upgraded. The two partition every model query this solve
    // made, folded in from a restoration sub-solve like the work counters.
    //
    // evals_full COUNTS: the first iterate's evaluation, every ACCEPTED trial
    // (direct or via a promoted SOC correction, exactly one per acceptance), a
    // restoration exit's re-evaluation of the main model at a finite restored
    // point, and the restoration seed's guard query at the exhausted-ladder site,
    // charged whether the guard passes or fails. evals_values COUNTS: every
    // REJECTED trial's evaluation, and the warm-resolution probe's f/cE/cI fetch.
    // One exception: a rejected trial that restoration UPGRADED at the request site
    // moves to evals_full and out of evals_values, so the partition stays exact.
    //
    // evals_values is identically zero on a solve that never rejected a trial and
    // never ran the warm-resolution probe.
    //
    // What they do not count: both are incremented from the driver's own
    // control-flow DECISION at each call site, never from observing which NlpModel
    // method executed. Today the two agree, because every increment sits next to
    // the call it describes -- but an edit that changes which function a site
    // invokes without updating the increment would desynchronize them silently.
    // These fields are not a self-verifying trace: a claim built on them should be
    // corroborated against an independent model-call count on at least one fixture.
    Index evals_full = 0;
    Index evals_values = 0;

    /// Did THIS solve stop because the caller's own Minor-iteration budget ran out?
    /// 0 or 1 -- never more, because the budget test is an exit. A flag kept as an
    /// Index so it SUMS over a ledger.
    ///
    /// WHAT IT COUNTS: the 4-argument solve() overload was given a positive budget,
    /// this solve reached the top of a major having already spent at least that
    /// many qp_minor_iters, and it was not converged there, so it returned
    /// SqpStatus::kMaxIter at that iterate. The test runs BETWEEN majors, so the
    /// minors actually spent are >= the budget.
    ///
    /// What it does NOT count: an ordinary max_iter exhaustion (also kMaxIter, this
    /// field 0); a kBudgetExhausted exit under SqpOptions::budget_mode, a different
    /// budget with a different contract; and a solve that spent more than the
    /// budget and CONVERGED anyway, which is reported kOptimal with this field 0.
    /// A solve given no budget leaves this identically 0.
    Index probe_budget_stops = 0;

    /// How many inequality ROWS and variable BOUNDS SqpOptions::crash_basis seeded
    /// into the FIRST QP subproblem's working set on this solve. Identically 0 with
    /// the lever off, on every warm or hot ingest (the seed is cold-only), and on a
    /// solve that never builds a subproblem.
    ///
    /// COUNTED SEPARATELY because the two halves have different ceilings and
    /// folding them would hide the one that matters: a row seeded well can remove
    /// churn, while a bound seeded well can only remove a one-shot transit.
    ///
    /// SEEDS OFFERED, Not seeds honoured: qp_engine.h's window-consistency rule may
    /// silently drop a seeded bound outside the first trust-region window, and a
    /// dropped hint is still counted here.
    ///
    /// FOLDED FROM A Restoration sub-solve, which inherits the lever and is cold by
    /// construction, so a solve that restored can report contributions from TWO
    /// first subproblems. Read a nonzero value as "seeds this solve proposed",
    /// never as "seeds the caller's own x0 supplied".
    Index crash_seeded_rows = 0;
    Index crash_seeded_bounds = 0;

    /// 1 iff THIS solve's warm-start resolution landed on StartLevel::kSeeded, else
    /// 0 -- a flag kept as an Index so it SUMS over a ledger. Not redundant with
    /// `start_level_used`, which answers the same question for one solve and cannot
    /// be summed. Identically 0 on the 2-argument solve() overload, on every
    /// kCold/kWarm/kHot resolution, and on a seeded object that degraded.
    Index n_seeded = 0;

    /// How many `lambda_i` entries the seeded `lambda_i >= 0` enforcement ZEROED on
    /// this solve. Bounded by model.mi(). Identically 0 at every other level, since
    /// the clamp is scoped to kSeeded, and 0 on a well-formed seed: a nonzero
    /// reading means the producer handed over at least one negative price on a row
    /// the destination model reports GEOMETRICALLY ACTIVE. Strictly slack rows
    /// never reach the clamp -- the geometric complementarity clear has already
    /// zeroed them, and it runs first by contract.
    ///
    /// Neither this field nor `n_seeded` counts a seeded object DEGRADED to kCold
    /// by a beyond-the-band negative price: such a solve ingested nothing at all,
    /// and reports both as 0.
    Index seeded_clamped = 0;

    /// How many inequality ROWS the INGESTED activity hint proposed active on this
    /// solve -- the size of the working set a kSeeded object handed the first
    /// subproblem, and on the crossover route exactly what `from_interior_point`
    /// inferred.
    ///
    /// WRITE-ONLY: nothing in the driver, the engine or the globalization reads it.
    /// It exists so a corpus row can tag a crossover cell with the quality of the
    /// hint it was given, which is otherwise invisible in the counters.
    ///
    /// SCOPED TO kSeeded, and identically 0 at kCold/kWarm/kHot -- not because a
    /// kWarm object carries no hint, but because at kWarm the hint's provenance is
    /// confirmed and its size answers no question. Named for its motivating
    /// producer, not scoped to it: a mesh transfer or a hand-built object resolving
    /// kSeeded is counted identically.
    ///
    /// ROWS ONLY -- the bound half is deliberately not folded in, on the
    /// crash_seeded_rows/bounds argument. SEEDS OFFERED, Not seeds honoured, exactly
    /// like that pair.
    Index ip_activity_inferred = 0;

    /// Sum of `SqpIterate::active_set_delta` over this solve's REPORTING majors --
    /// the total working-set churn the QP sequence went through, counting the first
    /// major's census against the empty set. Instrumentation only.
    ///
    /// Not folded from the restoration sub-solve, on the `steps_accepted` argument
    /// rather than the `qp_minor_iters` one: an active-set delta is a statement
    /// about which constraints of THIS problem were in the working set, and the
    /// sub-solve's QP lives in the feasibility wrapper's own variables and rows.
    Index active_set_delta_total = 0;
    /// The largest `SqpIterate::active_set_delta` any single major of this
    /// solve reported. A PEAK, folded with `max` like `ssn_uncertain_peak`, and
    /// not folded from the restoration sub-solve for the reason above.
    Index active_set_delta_peak = 0;
    /// The largest `SqpIterate::weak_active_rows` any single major reported --
    /// how close to a degenerate (non-strictly-complementary) active set this
    /// solve's QP sequence ever got. A PEAK; see `kWeakActivityMargin`.
    Index weak_active_peak = 0;
    /// The largest `SqpIterate::near_active_rows` any single major reported --
    /// how many rows were sitting on the boundary WITHOUT being in the working
    /// set. A PEAK; see `kWeakActivityMargin`.
    Index near_active_peak = 0;

    /// The semismooth-Newton kernel's work, summed over every subproblem of
    /// this solve -- the same aggregation set qp_minor_iters and
    /// factorizations above use, and empty for the same reason they would
    /// be if no subproblem were solved.
    ///
    /// **Zero on every solve run at the shipped default**
    /// (`SqpOptions::qp_mode == QpMode::kWalk`): no SSN subproblem is
    /// solved there, so nothing writes here. Populated when the solve runs
    /// `QpMode::kSsn`.
    SsnCounters ssn;

    /// The IP-PMM interior-point tier's work, folded over every subproblem of this
    /// solve by `accumulate_ipqp_counters` -- the `ssn` field's own aggregation,
    /// for the third QP kernel. See `IpqpCounters` for the fold rule.
    ///
    /// Zero on every solve run at the shipped default (`SqpOptions::qp_mode ==
    /// QpMode::kWalk`): no IPQP subproblem is solved there, structurally rather than
    /// by arithmetic -- the dispatch's kWalk arm constructs no `IpqpEngine`. At
    /// `QpMode::kIpm` every field is written from a real solve. Two routing fields
    /// are DRIVER-SCALE and have no per-subproblem contribution at all
    /// (`ipqp_tier_retired_after`, `ipqp_declined_pinned`).
    IpqpCounters ipqp;
};

#define HVEN_SQP_COUNTERS_FIELDS(X)                                                                \
    X(major_iters, counter_never_absent)                                                           \
    X(qp_minor_iters, counter_never_absent)                                                        \
    X(factorizations, counter_never_absent)                                                        \
    X(steps_accepted, counter_never_absent)                                                        \
    X(rejected_steps, counter_never_absent)                                                        \
    X(soc_steps, counter_never_absent)                                                             \
    X(soc_applied, counter_never_absent)                                                           \
    X(soc_qp_infeasible, counter_never_absent)                                                     \
    X(soc_rejected, counter_never_absent)                                                          \
    X(elastic_activations, counter_never_absent)                                                   \
    X(elastic_escalations, counter_never_absent)                                                   \
    X(restoration_iters, counter_never_absent)                                                     \
    X(elastic_from_ipqp_escape, counter_never_absent)                                              \
    X(ipqp_suspicion_disproved, counter_never_absent)                                              \
    X(ipqp_fallback_rung_b, counter_never_absent)                                                  \
    X(elastic_rho0_ceiling_hits, counter_never_absent)                                             \
    X(elastic_floor_retries, counter_never_absent)                                                 \
    X(eqp_refine_steps, counter_never_absent)                                                      \
    X(border_refine_steps, counter_never_absent)                                                   \
    X(verdict_refine_steps, counter_never_absent)                                                  \
    X(suspect_escalations, counter_never_absent)                                                   \
    X(symbolic_analyses, counter_never_absent)                                                     \
    X(start_level_used, counter_never_absent)                                                      \
    X(full_step_majors, counter_never_absent)                                                      \
    X(watchdog_restores, counter_never_absent)                                                     \
    X(evals_full, counter_never_absent)                                                            \
    X(evals_values, counter_never_absent)                                                          \
    X(probe_budget_stops, counter_never_absent)                                                    \
    X(crash_seeded_rows, counter_never_absent)                                                     \
    X(crash_seeded_bounds, counter_never_absent)                                                   \
    X(n_seeded, counter_never_absent)                                                              \
    X(seeded_clamped, counter_never_absent)                                                        \
    X(ip_activity_inferred, counter_never_absent)                                                  \
    X(active_set_delta_total, counter_never_absent)                                                \
    X(active_set_delta_peak, counter_never_absent)                                                 \
    X(weak_active_peak, counter_never_absent)                                                      \
    X(near_active_peak, counter_never_absent)

/// @brief `HVEN_SQP_COUNTERS_FIELDS`' entry count -- the DIRECT fields only.
inline constexpr std::size_t kSqpCountersFieldCount =
    0 HVEN_SQP_COUNTERS_FIELDS(HVEN_COUNTERS_COUNT_ONE);
#undef HVEN_COUNTERS_COUNT_ONE

// PLUS TWO: `ssn` and `ipqp` are nested aggregates, each counted as ONE
// initializer by the arity trick and serialized through its own table above, so
// the table holds the 37 direct fields and the struct takes 39 initializers.
static_assert(::hven::detail::kAggregateArity<SqpCounters> == kSqpCountersFieldCount + 2,
              "SqpCounters and HVEN_SQP_COUNTERS_FIELDS disagree: give the new field an X() entry "
              "in its declaration position, or -- if it is a new NESTED aggregate -- give it a "
              "table of its own and raise the + 2 here.");

/// The SqpCounters table's field offsets, in table order. `offsetof` is only
/// portable on a standard-layout type, so that is asserted first.
static_assert(std::is_standard_layout_v<SqpCounters>,
              "SqpCounters must stay standard-layout for the "
              "offsetof order check below to be portable.");
inline constexpr std::size_t kOffsetsSqpCounters[] = {
#define HVEN_COUNTERS_OFFSET_ONE(f, absent) offsetof(SqpCounters, f),
    HVEN_SQP_COUNTERS_FIELDS(HVEN_COUNTERS_OFFSET_ONE)
#undef HVEN_COUNTERS_OFFSET_ONE
};
static_assert(counters_offsets_increase(kOffsetsSqpCounters, kSqpCountersFieldCount),
              "SqpCounters and HVEN_SQP_COUNTERS_FIELDS disagree about ORDER: an entry is out of "
              "declaration position. The JSON key order is the table's, so this must be the "
              "struct's.");

} // namespace hven::solvers
