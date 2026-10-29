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
#include <hven/core/solver_status.h>
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
/// rule rejects is discarded and NOT counted. The EQP-site residual check's own
/// solves (M7 W2 E4) are a fourth, disjoint quantity in `eqp_refine_iters` and
/// are deliberately NOT folded into eqp_refine_steps, whose identically-0
/// invariant is therefore unchanged by it.
/// @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h
struct QpCounters {
    /// THE ACCOUNTING CONTRACT (ruling E4-17, M7 W2 E4 fix round 2). This field is
    /// the ACTIVE-SET SEARCH's cost and nothing else: every budget is written
    /// against it, the hot-handle contract is stated as `qp_factorizations == 0`
    /// on a reused K0 (the WarmStart pins read exactly that), and the HS battery's
    /// cost tripwire reads it. A factorization that searches nothing and reuses
    /// nothing therefore belongs in its OWN field, and the terminal polish's
    /// stage-2 re-solve is charged to `terminal_polish_solves` for that reason.
    /// The TOTAL a solve spent is `factorizations + terminal_polish_solves`.
    /// `terminal_mu_refines`' factorization predates this ruling and IS in this
    /// field; the asymmetry is recorded at both fields and deliberately left
    /// alone.
    Index factorizations = 0;
    Index schur_updates = 0;
    Index minor_iters = 0;
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    /// THE EQP-SITE RESIDUAL CHECK (M7 W2 E4; eqp_solve.h's own header section).
    /// Solves SPENT driving the CONSTRAINT-row residual of an EQP answer against
    /// its OWN regularized system down to detail::kEqpResidualRefuseTol, summed
    /// over every EQP solve of this QP solve, on either algebra.
    ///
    /// COUNTS SOLVES SPENT, not steps kept -- deliberately unlike the three
    /// refinement counters above, because what this field exists to bound is the
    /// COST of a check that fires only on a factor that failed to solve its own
    /// system. A candidate the strict-decrease safeguard rejects is discarded
    /// (the answer is not moved) and still counted here: it was paid for.
    /// 0 on every pass whose factor solved its system, which is the overwhelming
    /// majority.
    Index eqp_refine_iters = 0;

    /// EQP answers REFUSED by that check -- the row residual still above the
    /// tolerance after the budget above was spent, so the answer does not solve
    /// the system it came from. Each refusal routes the pass to a retreat the
    /// engine already has: the border path retreats to the bound-eliminated one
    /// (as for any factor it cannot trust), and an eliminated-path refusal rides
    /// the suspect-stall ladder -- no step toward the answer WHILE THE LADDER HAS
    /// RUNGS, and an EXHAUSTED ladder is ADVISORY: the walk steps and the terminal
    /// site classifies the point it stops at, only a NON-FINITE answer being
    /// terminal (qp_engine.h section 5 states the rule). So a solve can read many
    /// refusals and still kOptimal. A NaN residual is a refusal.
    Index eqp_residual_refusals = 0;

    /// EQP answers refused by THE STEP'S ROW-DRIFT CHECK (M7 W2 E4 fix round 2;
    /// qp_engine.h's eqp_answer_drifts_off_met_rows) -- the answer solved its own
    /// REGULARIZED system, so `eqp_residual_refusals` has nothing to say about
    /// it, and it nonetheless leaves a working row the INCOMING iterate was
    /// already on, by exactly the dual regularization's own dual_mu*|lambda|
    /// term. Disjoint from `eqp_residual_refusals` by construction: a pass the
    /// residual check already refused is not counted here as well.
    ///
    /// Each refusal routes the pass to the SAME suspect-stall ladder, for the
    /// reason the arithmetic gives -- on the flat face that produces this,
    /// |lambda| scales as 1/primal_delta, so one decade more primal_delta is one
    /// decade less drift. 0 on every solve whose steps keep their closed rows
    /// closed, which is the overwhelming majority.
    ///
    /// ADVISORY ONCE THE LADDER IS EXHAUSTED -- as the residual refusal is, on
    /// the same rule (qp_engine.h section 5): the walk takes the step it would
    /// have taken before the check existed and the terminal site classifies the
    /// point it stops at. This is its OWN field because the CAUSE differs, not
    /// the response: the answer counted here DOES solve its system and the
    /// complaint is a quality one. So a nonzero count here is a REPORT about
    /// how much regularization footprint the walk had to carry, never by itself
    /// a failure, and a solve can read several refusals and still kOptimal.
    Index eqp_row_drift_refusals = 0;

    /// THE UNREGULARIZED POLISH at the EQP site (M7 W2 E4 fix round 1, ruling
    /// E4-11; eqp_solve.h's polish_eqp_core). Refinement steps KEPT against the
    /// face's OWN exact KKT system -- both regularizations backed out -- summed
    /// over every EQP solve of this QP solve, on either algebra.
    ///
    /// "KEPT" as in the three fields above: a step the strict-decrease safeguard
    /// rejects is discarded and not counted, which is why this counter is also
    /// the measurement of HOW OFTEN the face's exact system has an answer at all.
    /// 0 on a flat face -- the restoration LPs, every PSD-singular face -- where
    /// the proximal answer is the only one there is, and 1..kEqpPolishSteps on a
    /// face with curvature along its null space. It costs no factorization: the
    /// steps run on the factor the solve already paid for.
    Index eqp_polish_steps = 0;

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
    /// zero-multiplier probe (probe_zero_multiplier_drops) plus one per
    /// member an exchange released (ws_exchanges). A probe drop
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

    // Working-set independence (qp/ws_independence.h). Solves are triangular
    // solves on the pass's own factor; none factorizes. The terminal certificate's solves (the
    // pass's factor, or a local eliminated factor whose factorization lands in `factorizations`)
    // also land here.
    Index dep_test_solves = 0;
    Index dep_tests = 0;
    Index dep_dependent = 0; // incl. undecidable
    Index dep_undecidable = 0;
    Index entry_dependency_removals = 0;
    Index mandatory_dependent_entries = 0;
    // Detector re-checks after untested growth. On the eliminated path the
    // support carries the pins' coefficients (E2 fix1), so a dependency an
    // optional pin creates removes that pin.
    Index dep_recheck_removals = 0;
    // Negative-curvature ride blockers that tested dependent, resolved by the
    // add site's rule along the ride (qp_engine.h 4c, THE DECLINED RIDE; M7 W2
    // NC1 -- they declined to the ordinary step before, as dep_ride_declines).
    Index dep_ride_blockers = 0;
    Index ride_exchanges = 0; // ... of which exchanged; the ride is re-armed
    // A vetoed ride (ride_sign, or the null-space veto with a p that does not
    // leave the released constraint) whose drop was undone, the released
    // constraint re-entering drop-exempt, instead of the ordinary step.
    Index ride_declines_fallthrough_blocked = 0;
    // ride_sign's veto (flat slope or sign disagreement) on the independent
    // working set; at a KKT point of an independent set only a noise-level
    // multiplier can produce one (qp_engine.h 4c).
    Index ride_sign_vetoes = 0;
    // M7 W2 NC2, THE FLAT FACE (qp_engine.h 4c): rides whose sign the drop's own
    // multiplier decided, the measured g'p being below its own resolution.
    Index ride_predicted_signs = 0;
    // THE EQUALITY DRIFT's checked invariant (qp_engine.h 4c): passes at which the
    // iterate carried a STRUCTURAL equality violation after having met the rows.
    // The "has met them" flag never clears, so a walk that leaves the rows counts
    // EVERY later pass: this is a count of PASSES off the rows, not of drift
    // EVENTS. Nothing re-projects the iterate; the count is the observation.
    Index eq_drift_violations = 0;
    // THE SADDLE EXIT (qp_engine.h 4c, M7 W2 NC3): trusted-kWrong dead ends that took a
    // VERIFIED negative-curvature direction of the face; dead ends where none was found and
    // the kNumericalError exit stood; and the triangular solves the search cost (its one
    // factorization per dead end is counted in `factorizations`). The search's face solves
    // also side-charge the solve_eqp counters (`eqp_polish_steps`, `eqp_refine_*`), so a
    // miss moves those by the search's own steps.
    Index saddle_exits = 0;
    Index saddle_exit_misses = 0;
    Index saddle_direction_solves = 0;
    // THE CONVEX CERTIFICATE (qp_engine.h 4c, M7 W2 NC4): trusted-kWrong dead ends whose
    // kWrong was overruled because H is structurally convex (nonnegative diagonal, LP
    // included); the point then faces the ordinary walk's first-order tests.
    Index convex_certificates = 0;
    // THE ASCENT GUARD (qp_engine.h 4c, M7 W2 W1): passes whose EQP direction held the
    // working set and ascended with nonpositive curvature (an indefinite face's saddle),
    // ridden along -p instead of stepped along p.
    Index ascent_rides = 0;
    Index dep_pin_skips = 0;
    Index ws_count_violations = 0; // passes with me + |W| > n (the integer screen)
    Index dep_untested_growth = 0; // screen-positive checks the detector did not resolve
    // The screen called a candidate dependent while the mandatory rows were
    // themselves dependent (mandatory_dependent_entries > 0): a possible false
    // dependent, the safe direction. With E1's basis on it cannot occur.
    Index dep_screen_under_mandatory_dependence = 0;

    // The exchange and the no-admissible-member rule at the ordinary add site
    // (Gill & Wong 2015, Result 3.4(b)).
    Index ws_exchanges = 0;
    Index ws_exchanges_bound = 0;   // the blocker or the released member is a pin
    Index ws_exchanges_shifted = 0; // the released member was a shift-driven row (last resort)
    // A verified EXACT Farkas certificate (multipliers refined mu-free, residual
    // within the rounding floor QpFarkasCertificate::exact_tol) => kInfeasible;
    // tried whenever no member is admissible (E2 fix1: not only under cond1).
    Index dep_infeasible_certificates = 0;
    // A verified TOLERANCED certificate (residual above the floor: a box_part
    // coefficient dropped on an unbounded coordinate) is not a proof for the
    // QP => kNumericalError at the current iterate, the certificate kept in
    // QpSolution::farkas (E2 fix2, settler S25/S25').
    Index dep_certificate_toleranced = 0;
    Index dep_certificate_rejections =
        0; // a cond1 candidate failed verification => kNumericalError
    Index dep_passovers = 0;
    Index dep_blocker_terminations = 0; // no admissible member, no certificate, no pass-over
    // An undecidable (screen-negative, unresolved) blocker admitted with the
    // next pass's detector as its test -- on the eliminated path only, and
    // only when no structural dependency decides it (E2 fix1). A deferred add
    // that detector later finds dependent is removed and counted in
    // dep_recheck_removals (or, if the detector does not resolve an
    // over-count, in dep_untested_growth); this counter is not decremented.
    Index dep_deferred_adds = 0;
    // The rule would have ended the solve on a border-stack pass, or the
    // blocker's test there was undecidable (E2 fix1): the solve continued on
    // the bound-eliminated path instead (once per solve).
    Index dep_border_retreats = 0;
    // THE STANDSTILL RETREAT (qp_engine.h): border-stack passes that stood still (zero
    // motion, or a second consecutive full step) off a structurally violated equality row
    // with the inertia reading kWrong, and moved the rest of the solve to the
    // bound-eliminated path with a re-check armed. Not counted in dep_border_retreats.
    Index standstill_retreats = 0;
    // THE BORDERED ANSWER'S CERTIFICATE (bordered_eqp.h, M7 W2 BPS): border-stack passes
    // whose refinement stopped above its own target, and took the bound-eliminated twin
    // for that pass.
    Index border_uncertified_fallbacks = 0;
    // An undecidable blocker decided EXACTLY by a structural dependency (a
    // pin completing a member row on pinned columns, or a row on pinned
    // columns only), coefficients read off the row (E2 fix1).
    Index dep_structural_exact = 0;
    // A member the re-check removed as a dependent deferred add, blocking
    // again at the same point on an undecided test: refused (no coefficients,
    // so the pass-over or the end) instead of re-admitted (E2 fix1).
    Index dep_known_dependent_refusals = 0;
    // Detections (entry or re-check) that left out a removable support member
    // y_hat does not certify: ||A_W^T y_hat|| > kDepRemovalTol * |y_k| ||a_k||
    // (E2 fix1b). A member in the support by a small coefficient only.
    Index dep_uncertified_trims = 0;
    // ... of which no removable member was certified, so the uncertified one
    // with the largest scaled coefficient was offered alone: a HEURISTIC
    // fallback, not a certificate (E2 fix1b; renamed from
    // dep_certified_fallbacks in E2 fix2, settler S24).
    Index dep_uncertified_fallbacks = 0;
    // Candidate tests whose refinement stalled with rho >= kDepUndecidable,
    // admitted as independent (the landed contract, E2 fix2, settler S23; a
    // stall below kDepUndecidable is dep_undecidable instead). Telemetry: the
    // stall class is truly independent on the measured corpus (R-D retired,
    // settler S34).
    Index dep_stall_admits = 0;
    // Candidate tests that DID NOT RUN -- no factor held this pass's working
    // set, so no solve was made (E2 fix7, settler E4-37(b)). Their verdict is
    // the integer screen's alone: positive it is an exact proof of dependence,
    // negative the candidate is admitted as the pre-E2 walk admits it, and
    // never on the strength of an uncomputed rho. Telemetry, and the count of
    // the growth this walk could not test numerically; the screen-positive
    // half is also counted dep_dependent as any other dependent verdict is.
    Index dep_unrun_tests = 0;
    // E2 fix8, THE BOUNDED REPAIR (qp_engine.h, the repair's BUDGET). Candidate pins
    // `repair_temporary_vertex` TRIED although the integer screen had fired. The screen
    // proves {A_W, e_i} dependent as a family, not e_i in span(A_W): equal only for an
    // independent A_W, which a rank-deficient equality Jacobian denies. Skipping such a
    // pin is the UNSAFE direction there -- it can abort the repair short of the pin that
    // cuts the negative curvature (ACOPR118's majors 17-20: aborted at 70, repairs at 75)
    // -- so only a test that decided THIS candidate skips, and these are the pins the old
    // rule would have skipped on the count.
    Index dep_repair_screen_pins = 0;
    // Repairs that stopped on the pin budget (qp_engine.h, repair_pin_budget) rather than
    // on a verdict or on running out of candidates. Such a repair FAILS (it undoes itself),
    // which is the same outcome the screen's accidental bound produced. 0 over every repair
    // the budget was derived from (838 replayed from dumps, 20 in-cell).
    Index dep_repair_budget_stops = 0;
    // Passes SPENT after a start repair that undid itself (E2 fix8, F7-1). The repair
    // restores `ws` and `x` but leaves `face` and the border stack describing the last set
    // it probed, so the pass it was called from would run its add site's dependency tests
    // with no factor that holds its working set. When the pass HELD a usable factor view
    // before the repair and does not after it, the walk spends the pass instead, and the
    // next one re-establishes the factor by the ordinary route (a fresh factorization on
    // the eliminated path; sync_borders on the border path). independent_working_set only:
    // a stale `face` costs a DEPENDENCY TEST with nothing to test against, and only that
    // walk takes one.
    Index dep_repair_restarts = 0;
    // THE IMPLIED BLOCKER (M7 W2 QC-8; qp_engine.h, blocker_implied_by_face): pass-overs
    // that SURVIVE their sequence -- of a blocker of an ordinary step proved dependent on the
    // working set whose face satisfies it (g + rho ||a|| ||x_face,free|| <= feas_tol *
    // row_scale), its rate along p the step's leak, not motion -- each within its members'
    // leak allowance (R1). A sequence that falls back to the exchange it set aside (R3) undoes
    // the pass-overs it had taken, and they are not counted. Not counted in dep_passovers.
    // Appended at the end of the add-site group (the layout tests derive their ranges with
    // offsetof).
    Index dep_implied_passovers = 0;
    // THE DEAD-END RANK REPAIR (M7 W2 RDV; qp_engine.h, Degeneracy; ws_independence.h,
    // rank_repair): add-site dead ends (no admissible member, no verified certificate, no
    // pass-over) at which the working set itself was numerically rank-deficient and the members
    // on its null directions were released (rank_repairs), the members so released, counted
    // also in ws_drops (rank_repair_releases), and the analyses that released nothing -- a full
    // rank set, the mandatory rows' own consistent dependence, an inconsistent direction with no
    // member of the right orientation, or the linear algebra declining (rank_repair_misses).
    // Appended at the end of the add-site group.
    Index rank_repairs = 0;
    Index rank_repair_releases = 0;
    Index rank_repair_misses = 0;
    // THE REFUSED-BLOCKER EXCHANGE (M7 W2 YRK; qp_engine.h, Degeneracy): exchanges taken where
    // the add-site rule would end the solve (N3, after the rank repair) for the pass-over
    // sequence's first blocker, refused by D3(b), on coefficients derived mu-free for it.
    // Appended at the end of the add-site group.
    Index dep_refused_exchanges = 0;

    // The zero-motion re-entry rule (per fixed iterate).
    Index reentry_exemptions = 0;
    Index zero_motion_exhausted = 0; // an exempt member's multiplier is real
    Index zero_motion_cap_hits = 0;  // > 2(n + mi) releases/re-entries at one point

    // E2 fix3 (the E2-D4 rules; qp_engine.h, Degeneracy).
    // R-A: a numerical "independent" candidate test overturned by an exact
    // structural dependency, at any growth site (add, repair pin, ride add).
    Index dep_structural_first = 0;
    // R-B (E2 fix5): zero-motion exchanges that released a member locked only
    // by having entered at this point, taken where the rule would otherwise end
    // the solve (no admissible member, no verified certificate, no pass-over).
    Index dep_unlocks = 0;
    // R-C': kWrong inertia verdicts re-read on a mu-free eliminated factor (at
    // the start repair and the terminal check), and how many replaced the
    // kWrong (a kOk mu-free read, or a kSuspect one confirmed by a kOk read at
    // the pass's dual_mu).
    Index dep_inertia_rereads = 0;
    Index dep_inertia_reread_flips = 0;
    // R-E (E2 fix6): CONVERGED candidate tests read independent in the grey
    // band kDepTol < rho < kDepUndecidable, re-read with the refinement run to
    // its end on the same factor (no factorization; the solves in
    // dep_test_solves), and how many of those the re-read certified DEPENDENT
    // (rho' + its rounding floor <= kDepTol).
    Index dep_grey_rereads = 0;
    Index dep_grey_reread_dependent = 0;

    // E2 fix4 (qp_engine.h, refine_terminal_mu_free): terminal points that
    // would read kInfeasible by a structural violation while a working row's
    // dual_mu * |lambda| footprint exceeds its tolerance, re-solved mu-free on
    // the face (one factorization each, also in `factorizations`), and how many
    // of those refined points satisfied every row and bound and were adopted.
    //
    // THE ASYMMETRY, STATED AND NOT REPAIRED (ruling E4-17): this site's
    // factorization IS in `factorizations` while the terminal polish's stage-2
    // re-solve is NOT. Both are one local solve at a terminal point, so the
    // charging differs for history rather than for a reason -- it predates the
    // ruling, every pin that reads it was derived with it there, and moving it
    // would be a silent break of those pins for a cosmetic gain.
    Index terminal_mu_refines = 0;
    Index terminal_mu_refine_rescues = 0;

    /// THE TERMINAL POLISH (M7 W2 E4 fix round 1, ruling E4-11;
    /// qp_engine.h's section 5 and QpEngine::polish_terminal_point).
    ///
    /// `terminal_polish_solves`: would-be kOptimal exits at which the final point
    /// was RE-SOLVED on its FINAL WORKING SET with the dual regularization out of
    /// the system -- the polish's stage 2. Reached only at a certified exit whose
    /// footprint is open (polish_footprint_open) and whose free stage-1 candidate
    /// was refused; 0 on a solve that ends any other way.
    ///
    /// ONE FACTORIZATION EACH, AND IT IS **NOT** ALSO IN `factorizations` --
    /// deliberately unlike terminal_mu_refines, whose own note says it is. The
    /// argument is at polish_terminal_point: `factorizations` is the active-set
    /// SEARCH's cost, which every budget, the hot-handle `qp_factorizations == 0`
    /// contract and the battery's cost tripwire are all written against, and this
    /// solve searches nothing. The total cost of a solve is
    /// `factorizations + terminal_polish_solves`.
    ///
    /// `terminal_polish_rejected`: of those, the candidates NOT adopted --
    /// non-finite, or a row/bound outside feas_tol that the incumbent met, or a
    /// worse stationarity / row / complementarity residual, or nothing strictly
    /// better to show. A rejection leaves x and every multiplier exactly as the
    /// walk had them, so it is a cost and never a risk;
    /// `terminal_polish_solves - terminal_polish_rejected` is the adoption
    /// count.
    ///
    /// `terminal_polish_adopted`: candidates ADOPTED, over both stages. Stage 1
    /// is FREE -- the pass's own EQP answer, which solve_eqp has already taken
    /// the regularization out of -- so `terminal_polish_adopted` exceeds
    /// `terminal_polish_solves` on any solve whose terminal face the free
    /// candidate closed, and the factorizations spent are
    /// `terminal_polish_solves` alone.
    Index terminal_polish_solves = 0;
    Index terminal_polish_rejected = 0;
    Index terminal_polish_adopted = 0;

    // M7 W2 SDL (qp_engine.h, section 5 and THE SADDLE EXIT). Appended.
    // `certificate_sign_refusals`: would-be kOptimal exits whose exported multipliers carried a
    // wrong sign past the KKT-point test's relative tolerance (the certificate's SIGN HALF);
    // each ends kNumericalError.
    // `saddle_shifted_searches`: saddle-exit searches that found nothing on the face's own
    // factor and were repeated on the face shifted by kSaddleShiftFactor * hessian_scale (one
    // factorization each, also in `factorizations`); `saddle_shifted_directions`: of those, the
    // ones that returned a verified direction.
    Index certificate_sign_refusals = 0;
    Index saddle_shifted_searches = 0;
    Index saddle_shifted_directions = 0;
    // `terminal_polish_sign_reverts`: adopted terminal polishes undone because the polished
    // multipliers failed the sign half; the incumbent (pre-polish x and multipliers) is then
    // read by both halves again. Counts RESTORATIONS, not the certifications that follow them.
    Index terminal_polish_sign_reverts = 0;
    // M7 W2 SDL-4. Appended. `certificate_bound_refusals`: would-be kOptimal exits whose point
    // violated its own box structurally (the certificate's BOUND HALF); each ends
    // kNumericalError, never kInfeasible (routing: see sqp_solver.cpp).
    // `terminal_polish_bound_reverts`: adopted terminal polishes undone because the polished
    // point failed the bound half; the incumbent is then read by both halves. Counts
    // RESTORATIONS, as terminal_polish_sign_reverts (which a bound failure takes precedence over).
    Index certificate_bound_refusals = 0;
    Index terminal_polish_bound_reverts = 0;
    // M7 W2 SDL-6. Appended. The certificate's ROW HALF (a terminal point with a structural row
    // violation): `terminal_farkas_certificates` exits kInfeasible on a VERIFIED Farkas
    // certificate (QpEngine::verify_terminal_farkas); `certificate_row_refusals` exits
    // kNumericalError without one (routing: see sqp_solver.cpp).
    Index terminal_farkas_certificates = 0;
    Index certificate_row_refusals = 0;
    // M7 W2 SNX. Appended. THE STAGNATION READING (qp_engine.h, section 4): `stagnation_drops`
    // counts the drops it made; `stagnation_stops` the walks it ended kMaxIter before the minor
    // cap.
    Index stagnation_drops = 0;
    Index stagnation_stops = 0;
    // M7 W2 RLA. Appended. THE LATCH AND ITS REASON (qp_engine.h, section 4): border mode's
    // latches onto the elimination path, by reason -- `border_latches_cap` with more pins than
    // schur_cap, `border_latches_conditioning` with the pins under it and C ill-conditioned or
    // singular; `border_latch_releases` the latches released (each a K0 rebuild, also in
    // `factorizations`); `border_latch_holds` the border_candidate entries (passes and probes) on
    // which a conditioning latch held under the cap.
    Index border_latches_cap = 0;
    Index border_latches_conditioning = 0;
    Index border_latch_releases = 0;
    Index border_latch_holds = 0;
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ssn_refinements = 0;

    /// Certifying exits where the face re-solve was REFUSED; see the tier-3
    /// note above. Driver-scale only (no SsnResult ever writes it).
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ssn_refine_factorizations = 0;

    /// Strictly negative multipliers adopted from accepted refinements,
    /// summed over rows and refinements; NO TOLERANCE (`< 0.0`); see the
    /// instrument-only note above.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ssn_refine_neg_duals = 0;

    // The sign sweep repairs rather than instruments: it is the one pair here
    // that reports a value this driver changed. It runs in `SqpSolver::finish`,
    // the single export boundary, on the solution's and the warm start's
    // multipliers together, with no tolerance (the test is `< 0.0`).
    //
    // `SqpResult::kkt` is computed before the sweep, at the multipliers the
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
    // `ssn_escape_indefinite` below is a curvature reading: the kernel's second-order
    // verification was taken at the converged point and disagreed.
    // CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    // convex B, in which case this certifies B, not the NLP.
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
inline bool counter_never_absent(RestorationSubStatus) { return false; }

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
    /// cross-major hoisting rule while `AssemblyEvalSeam::epoch()` stays
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    double ipqp_rho_demanded_max = 0.0;

    /// The inertia-demanded modification at the LAST ladder rung this subproblem
    /// paid -- the shift the last successful modified factorization ran at, or, on
    /// an exhausted ladder, the ceiling rung the tier was refused at. An iteration
    /// that succeeded on the unmodified system pays no rung and leaves this
    /// untouched. OVERWRITTEN rather than summed or folded: it is a per-solve
    /// categorical reading, and summing many subproblems' "last rho" would report
    /// a quantity with no meaning. Excludes `delta`'s own last value, which carries
    /// no field.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    double ipqp_rho_demanded_last = 0.0;

    /// Factorizations REJECTED on wrong OR evidence-invalid inertia (the section
    /// 2.2 gate), which the gate refuses alike: a WRONG reading (observed,
    /// disagreed with the required signature) and a PERTURBED one (observed, but
    /// describing a matrix the backend perturbed rather than the one assembled)
    /// both cost a factorization the tier could not use. The ladder's last,
    /// ceiling-terminated factorization counts too. Excludes the required final
    /// read, whose outcome is `ipqp_final_inertia_read`, even when that read comes
    /// back wrong.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ipqp_inertia_retries = 0;

    /// Iterations whose step was TAKEN with a nonzero inertia-demanded
    /// modification in force (`rho_dem > 0`), measured AFTER that iteration's
    /// ladder settles, so the iteration in which the ladder first demands a
    /// modification is counted. Excludes iterations whose step ran on the
    /// unmodified system, which is every iteration of every convex subproblem.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ipqp_iters_at_elevated_rho = 0;

    /// LADDER RECLIMBS: iterations on which a `rho_dem_last / kIpqpLadderDown`
    /// value was REJECTED on a wrong inertia and the ladder had to re-escalate
    /// past it. One per iteration, never per rung -- the quantity is "how often
    /// did the memory's guess come back too small", not how far the climb went.
    /// Both routes to that value are charged: as an iteration's first trial once
    /// `IpqpOptions::ipqp_ladder_skip_after` consecutive iterations have needed a modification,
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
    Index ipqp_escape_indefinite = 0;

    /// Escapes that stop the tier for a non-convergence reason other than budget,
    /// stall or infeasible-suspect: a factorization failure, an UNREADABLE inertia
    /// reading (`ipqp_final_inertia_read == 2`), or a non-finite
    /// iterate/residual/step. The complement of `ipqp_escape_indefinite` within
    /// "the reading was inertia-related": indefinite means a reading was taken and
    /// disagreed, numerical means no reading could be taken at all or the failure
    /// was not an inertia reading. Excludes an indefinite-certificate escape.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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

    // A2 (M7-37): the fire counters of `IpqpOptions::ipqp_mu0_rule` and
    // `ipqp_mu_residual_floor`, all 0 at their defaults. Additive fold. Design:
    // docs/notes/2026-10-m7-w2-a2-mu0-design.md section 5 clause 4.

    /// += 1 per solve whose cold start took the derived `mu_0` (`kComplementarity` accepted).
    Index ipqp_mu0_derived = 0;

    /// += 1 per solve where `kComplementarity` ran and fell back to the `kFixed` start.
    Index ipqp_mu0_fallbacks = 0;

    /// += iterations whose corrector target the residual floor raised above `sigma * mu`.
    Index ipqp_mu_floor_binds = 0;

    /// A3 (M7-38): += iterations on which the skip rule (`IpqpOptions::ipqp_ladder_skip_after`)
    /// LICENSED starting the ladder at the memory value (the `skip_licensed()` predicate). Counts
    /// licensed skips: when the evidence floor stands, the first trial may be that floor instead.
    /// Non-zero at the default on a nonconvex subproblem; structurally 0 on a convex one. Additive
    /// fold. Design: docs/notes/2026-10-m7-w2-a3-ipqp-levers-design.md section 3.2. CURVATURE
    /// SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted convex B, in
    /// which case this reads B, not the NLP.
    Index ipqp_ladder_zero_trial_skips = 0;
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
    X(ipqp_read_barrier_noise_sides, counter_never_absent)                                         \
    X(ipqp_mu0_derived, counter_never_absent)                                                      \
    X(ipqp_mu0_fallbacks, counter_never_absent)                                                    \
    X(ipqp_mu_floor_binds, counter_never_absent)                                                   \
    X(ipqp_ladder_zero_trial_skips, counter_never_absent)

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
/// ON A Terminal restoration exit, SqpResult::x is NOT the point the last
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
/// EXACT-HESSIAN-SPECIFIC (M9 label): this identity holds under SqpHessianMode::kExact only;
/// `eval_hess_calls` is the measured count, and it is 0 under kLbfgsDamped unless
/// `SqpOptions::restoration_curvature == kRegularized`, whose phase runs the exact-Hessian
/// path on the regularized wrapper (those folded calls never reach the caller's Hessian).
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
/// the pairing with SolveStatus::kInfeasible that says otherwise.
/// @see docs/notes/2026-09-header-prose-archive.md §solver_counters.h
struct SqpCounters {
    Index major_iters = 0;
    Index qp_minor_iters = 0;
    /// The same accounting contract QpCounters::factorizations states (ruling
    /// E4-17): the active-set SEARCH's cost, summed over every QP solve this
    /// driver spent. The terminal polish's stage-2 re-solves are in
    /// `terminal_polish_solves` and the total spend is the sum of the two;
    /// `terminal_mu_refines`' factorization is in THIS field, asymmetrically and
    /// on purpose.
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
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian, or a caller-asserted
    /// convex B, in which case this certifies B, not the NLP.
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
    /// clamp DEGRADED the object, or when SqpOptions::common.start_level caps it there.
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
    // REJECTED trial's evaluation, the warm-resolution probe's f/cE/cI fetch, and
    // the column-scaling rule's Jacobian probe (SqpOptions::column_scaling != kOff,
    // the default kAuto included, unless the warm start carries the decision;
    // derivatives without values, charged here as the warm probe's Jacobians are).
    // One exception: a rejected trial that restoration UPGRADED at the request site
    // moves to evals_full and out of evals_values, so the partition stays exact.
    //
    // evals_values is identically zero on a solve that never rejected a trial,
    // never ran the warm-resolution probe and ran no column rule (kOff, or a carried
    // decision).
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
    /// SolveStatus::kMaxIter at that iterate. The test runs BETWEEN majors, so the
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

    /// @brief How many times this solve IGNORED a warm-start payload's
    ///        `"hven.ipm.polish.v1"` extension because the payload was the
    ///        MULTIPLIERS-ONLY form (M6 W5 T8.5).
    ///
    /// The seed form carries an EMPTY `primal_`, so the solve starts at the
    /// caller's `x0`. The polish extension's bound duals and inequality values
    /// are stated at the EXPORTER's point, which is not `x0`; feeding them into
    /// the crossover's activity rule would attribute activity nothing measured
    /// at the point this solve actually stands on. They are therefore dropped,
    /// and dropped LOUDLY: this counter is how a caller who attached an
    /// extension and expected it to be used finds out it was not.
    ///
    /// 0 or 1 in practice -- one payload per solve. Always 0 on the NATIVE
    /// route (a `SqpWarmStart` carries no extensions), 0 on a cold solve, and 0
    /// on a full payload, whose extension IS consumed.
    ///
    /// DECLARED LAST, after the two nested aggregates, so that every field
    /// offset this struct already had is unmoved.
    Index polish_ignored = 0;

    /// @brief How many components of the caller's `x0` lay STRICTLY OUTSIDE the
    ///        declared variable box at this solve's entry (M7 W2 M4).
    ///
    /// THE DEFINITION IS THE PROPERTY OF THE POINT, NOT OF THE REPAIR:
    /// `x_j < l_j` or `x_j > u_j`, counted before anything moves the point, with
    /// a fixed variable (`l_j == u_j`) sitting off its value counted ONCE. It is
    /// deliberately NOT "components the engine moved": the two agree on the SQP
    /// engine's clamp, which moves exactly the outside ones, and they do not
    /// agree on an interior push, which moves in-box coordinates too -- so a
    /// count defined as "moved" would mean different things on the two engines
    /// and could not be compared across a row. Defined this way, the IPM twin
    /// (registered, not built -- see `SqpOptions::project_start_point` and
    /// `IpmSolver::push_initial_point_interior`) would report the same number
    /// for the same start point.
    ///
    /// A COMPONENT COUNT, not a distance: it says how many coordinates were
    /// outside `[l, u]`, and says nothing about how far. A caller who set `x0`
    /// deliberately just outside a bound reads this to find out that the solve
    /// did not start where it said.
    ///
    /// Written only under `SqpOptions::project_start_point`. At the toggle's OFF
    /// state nothing writes here and the field is identically 0 -- which is also
    /// what it reads at ON on every `x0` already inside its own box, and the two
    /// zeros mean the same thing: the solve started at the point the caller
    /// named.
    ///
    /// SCOPED TO THIS SOLVE'S OWN ENTRY. The restoration sub-solve starts from
    /// its feasibility model's `start_point()`, which is inside that model's box
    /// by construction, so nothing is folded in from it.
    ///
    /// STATED AGAINST THE `x0` ARGUMENT: on a WARM ingest the count describes
    /// the argument handed to `solve()` while the loop starts at `warm.x`, so
    /// the two are the same point only on a cold solve (M7 W2 M4 fix1, the
    /// sentence carried here at M7 W2 M1; `docs/counters.md` has the mechanism).
    ///
    /// DECLARED LAST, after `polish_ignored`, for that field's own reason.
    Index start_point_clamps = 0;

    // -----------------------------------------------------------------------
    // THE SUBPROBLEM-EXIT GROUP (M7 W2 M1, signals S2 and S3). Five fields that
    // say WHY a subproblem failed and what the driver did about it. Before them
    // a capped subproblem and a broken factorization were one number in every
    // consumer column: `map_status` folds `QpStatus::kMaxIter` and
    // `QpStatus::kNumericalError` into the same `SolveStatus::kNumericalError`
    // (src/drivers/sqp_solver.cpp), and `rejected_steps` conflates a routed
    // failure with a strategy rejection BY DESIGN (see its note above).
    //
    // ALL FIVE ARE COUNTED AT ONE SITE -- the SUBPROBLEM FAILURE ROUTING block
    // in `run_major`, which is the same block `map_status` reads its verdict
    // from, so the counters and the returned status cannot disagree. The
    // elastic ladder's own exhaustion exit does NOT reach that block and is not
    // counted here; `elastic_activations` and the ladder's own fields describe
    // it.
    //
    // DECLARED LAST, after `start_point_clamps`, for that field's own reason.
    // -----------------------------------------------------------------------

    /// @brief Subproblem exits with `QpStatus::kMaxIter` -- the subproblem spent
    ///        its whole minor budget (M7 W2 M1).
    ///
    /// A BUDGET FACT, NOT A NUMERICAL ONE. The cap is `QpOptions::max_iter`
    /// resolved for this subproblem -- the size-derived
    /// `max(500, 5*(n + mi + #bounded))` at the shipped sentinel, or the
    /// caller's own positive value -- and a solve that reaches it returns the
    /// last iterate it walked to. That exit is reported as
    /// `SolveStatus::kNumericalError` like every other subproblem failure, so
    /// THIS COUNTER IS THE ONLY PLACE THE DISTINCTION SURVIVES into
    /// `SqpResult::counters`; the per-major fact is on the row
    /// (`SqpIterate::qp_status`).
    ///
    /// COUNTED WHERE THE DRIVER ACTS ON THE EXIT, which is the routing site --
    /// so an attempt that `SqpOptions::qp_cap_retry` RESCUED is not counted
    /// here at all: the retry replaced it as the major's answer before the
    /// routing ran, and `qp_cap_retries` is what records it. The two together
    /// are exact for the capped subproblem exits that REACHED THE ROUTING SITE:
    /// `qp_maxiter_exits + qp_cap_retries`, each counted once -- a retried
    /// attempt in the second field, every other capped exit in the first, and a
    /// retry that hit the doubled cap too in both (its own exit reaches the
    /// routing site). It is NOT a census of every capped subproblem the solve
    /// ran: the SOC re-solve and the elastic ladder's rungs never reach this
    /// site -- a non-`kOptimal` SOC simply means "SOC not applied", and an
    /// unusable rung leaves through the restoration seed -- so a capped one of
    /// either is in NEITHER field.
    ///
    /// WHICH KERNEL'S EXIT: whichever kernel answered the major, because the
    /// routing site is common to all three. At the shipped default
    /// (`qp_mode == QpMode::kWalk`) every one of them is the walk's; under
    /// `kSsn`/`kIpm` a tier exit that reaches the same block is counted the same
    /// way, which is the fold `map_status` already makes.
    ///
    /// FOLDED from the restoration sub-solve, on the `qp_minor_iters`
    /// argument: it counts QP solves, and the sub-solve's subproblems are QP
    /// solves this solve caused.
    ///
    /// A walk the STAGNATION READING ended (`QpCounters::stagnation_stops`)
    /// exits `QpStatus::kMaxIter` and is counted here; `qp_cap_retry` never
    /// retries it.
    Index qp_maxiter_exits = 0;

    /// @brief Subproblem exits with `QpStatus::kNumericalError` -- the
    ///        subproblem's own linear algebra failed (M7 W2 M1).
    ///
    /// The complement of `qp_maxiter_exits` at the same site, with the same
    /// per-exit counting, the same kernel scope and the same restoration fold.
    /// `QpStatus::kInfeasible` is NOT counted by either field: it is consumed by
    /// the elastic tier one block earlier and never reaches the routing site.
    Index qp_numerical_exits = 0;

    /// @brief Subproblem failures the driver ROUTED -- shrank the radius and
    ///        retried -- rather than ending the solve on (M7 W2 M1).
    ///
    /// One per failure the routing gate admitted:
    /// `qp_failures_in_a_row < SqpOptions::qp_failure_budget` and the failed
    /// solve handed back a usable iterate (`qp_failure_is_retryable`). At the
    /// shipped budget of 3 that gate admits up to three failures IN A ROW at one
    /// iterate and refuses the fourth, and
    /// `qp_maxiter_exits + qp_numerical_exits - qp_failures_routed` is the
    /// number of failures that ended a solve or were refused as unusable. At a
    /// budget of 1 -- the engine before M7 W2 M1 -- it is the count of first
    /// failures.
    ///
    /// IT IS A SUBSET OF `rejected_steps`, which is incremented at the same
    /// site and keeps its meaning unchanged: every routed failure is a major
    /// spent shrinking the radius at an iterate that did not move.
    ///
    /// NOT FOLDED from the restoration sub-solve, on `rejected_steps`' own
    /// argument: it describes what happened to the CALLER's iterate, and the
    /// sub-solve's radius shrinks happen to a point in the feasibility
    /// wrapper's variables.
    Index qp_failures_routed = 0;

    /// @brief The largest run of CONSECUTIVE routed subproblem failures this
    ///        solve reached (M7 W2 M1).
    ///
    /// A PEAK, folded with `max` like `active_set_delta_peak`. The driver's own
    /// `qp_failures_in_a_row` counts failures since the last `kOptimal`
    /// subproblem and is cleared by one; this is its high-water mark. It is
    /// bounded by `SqpOptions::qp_failure_budget` by construction, so at the
    /// shipped budget of 3 it is 0, 1, 2 or 3, and at a budget of 1 -- the
    /// engine before M7 W2 M1 -- it is 0 or 1.
    ///
    /// It counts ADMITTED failures, so the failure that ENDS the solve (the one
    /// the gate refuses) is not in it: a solve that ends on its second
    /// consecutive failure reports a peak of 1.
    ///
    /// NOT FOLDED from the restoration sub-solve, for `qp_failures_routed`'s
    /// reason.
    Index qp_failures_in_a_row_peak = 0;

    /// @brief Capped subproblems re-solved ONCE at a doubled cap under
    ///        `SqpOptions::qp_cap_retry` (M7 W2 M1).
    ///
    /// THE FIRED-COUNTER of that toggle, and identically 0 at its OFF state --
    /// which is the whole of what it is for: a lever whose firing cannot be
    /// read from `SqpResult::counters` does not ship (the W2 signals contract,
    /// docs/notes/2026-09-m7-w2-a0-signals-design.md section 2 clause 4).
    ///
    /// One per RETRY ATTEMPTED, whatever its outcome: the retry's own minors
    /// and factorizations fold into `qp_minor_iters` and `factorizations` like
    /// any other subproblem's, and its exit -- if it too is a failure -- is
    /// counted in the two exit fields above, so a solve with
    /// `qp_cap_retries == 1` and `qp_maxiter_exits == 2` is one subproblem that
    /// hit the cap, was given twice the budget, and hit that too.
    ///
    /// FOLDED from the restoration sub-solve, on `qp_maxiter_exits`' argument:
    /// the sub-solve inherits the toggle and its retries are real work.
    Index qp_cap_retries = 0;

    /// @brief Restoration phases that RAN and were measured -- the entries the
    ///        four excursion fields below describe (M7 W2 M2, signal S5).
    ///
    /// One per `enter_restoration` call that ran the feasibility sub-solve AND
    /// could measure its returned point on the caller's own model: the RESUME
    /// arm and the not-feasible-enough arm, which are the two the four fields
    /// below are written at. The three REFUSALS never enter the phase at all (a
    /// nested restoration, the entry rule (`SqpOptions::restoration_max_entries`), and no budget
    /// left to restore with), and the NON-FINITE exit -- where the restored point or its KKT
    /// measurement is not finite -- ran the phase but measured nothing, so it moves neither this
    /// count nor the four fields, on the driver's own rule at that exit: nothing was measured, so
    /// nothing may be reported as evidence.
    ///
    /// NOT `restoration_iters`, which counts the sub-solve's MAJORS. One entry
    /// of sixteen iterations reports `restoration_entries == 1` and
    /// `restoration_iters == 16`, and the ratio of the two is the only way to
    /// read how many phases a `restoration_iters` of 16 was spread over.
    ///
    /// NOT FOLDED from the restoration sub-solve: a nested restoration is
    /// refused, so a sub-solve's own count is structurally 0 and folding it
    /// would add nothing to anything.
    ///
    /// AND IT IS THE ABSENCE READING FOR THE FOUR FIELDS BELOW. They default to
    /// `0.0`, which is a legal observed value for three of them (a violation of
    /// exactly 0, an objective of exactly 0), so a reader asks this field, not
    /// them, whether a reading exists: `restoration_entries == 0` means the
    /// four hold none.
    Index restoration_entries = 0;

    /// @brief The l1 constraint violation at the iterate that REQUESTED the
    ///        last restoration entry (M7 W2 M2, signal S5).
    ///
    /// `h_in` of the excursion record. The quantity is the globalization's own
    /// h -- `sum |c_e| + sum max(0, c_i)`, `sqp_solver.h`'s
    /// `constraint_violation_l1` -- read at `x_k`, the point the request was
    /// raised at, BEFORE anything of the phase has run.
    ///
    /// OVERWRITTEN per entry rather than summed or folded, on
    /// `ipqp_rho_demanded_last`'s own precedent: it is a per-solve categorical
    /// reading, and summing several phases' entry violations would report a
    /// quantity with no meaning. A solve with `restoration_entries == 2`
    /// therefore describes its SECOND phase here and says so through that
    /// count; the per-entry series is the registered `sqp.restoration` trace
    /// event's, not this field's.
    ///
    /// NOT the point the phase STARTED from, which may be a seed candidate the
    /// entry guard preferred (`SqpIterate::restoration_seed_used`). The
    /// excursion this record measures is the one the CALLER's iterate made, so
    /// both `in` fields are read at the requesting iterate whether or not a
    /// seed was taken.
    ///
    /// IN THE ENGINE'S OWN UNITS, which are the caller's unless
    /// `SqpOptions::enable_scaling` is on -- the units the resume test is taken
    /// in, so that the record is stated in the units of the decision it
    /// describes.
    ///
    /// NOT FOLDED from the restoration sub-solve, for `restoration_entries`'
    /// reason.
    double restoration_h_in = 0.0;

    /// @brief The l1 constraint violation at the point the last restoration
    ///        entry RETURNED (M7 W2 M2, signal S5).
    ///
    /// `h_out` of the record: the same quantity as `restoration_h_in`, at the
    /// restored point. NOT the resume test's reading since M7 W2 SNR: that test
    /// reads the inf-norm `restoration_feas_out`, which this l1 sum bounds from
    /// above on the rows, so `restoration_h_out <= feas_tol` on an in-box point
    /// implies the entry resumed but not conversely. The resume reading is
    /// `restoration_feas_out`'s.
    ///
    /// Same overwrite rule, same units, same non-fold as `restoration_h_in`.
    double restoration_h_out = 0.0;

    /// @brief The objective at the iterate that REQUESTED the last restoration
    ///        entry (M7 W2 M2, signal S5).
    ///
    /// `f_in` of the record, read at `x_k` beside `restoration_h_in`.
    ///
    /// AN ABSOLUTE, NOT A RATIO, and deliberately: an objective ratio is
    /// undefined at `f_in` near 0 and meaningless across a sign change, and the
    /// objectives this record is aimed at include negative ones. A consumer
    /// that wants a ratio forms it and owns its domain; this field reports what
    /// was observed.
    ///
    /// Same overwrite rule, same units, same non-fold as `restoration_h_in`.
    double restoration_f_in = 0.0;

    /// @brief The objective at the point the last restoration entry RETURNED
    ///        (M7 W2 M2, signal S5).
    ///
    /// `f_out` of the record. With `restoration_f_in` it is the EXCURSION the
    /// A-SQP read's section M2 measured and could not see from the currency: a
    /// restoration that buys feasibility at eleven orders of objective reports
    /// it here, on a phase that produces no history row by contract
    /// (`SqpIterate`'s note).
    ///
    /// ON THE RESUME ARM this is the `f` the solve carries forward -- the
    /// restored point becomes the iterate -- so the next history row's `f` is
    /// this value. On the other arm it is the `f` of the point the result
    /// reports.
    ///
    /// Same overwrite rule, same units, same non-fold as `restoration_h_in`.
    double restoration_f_out = 0.0;

    /// @brief The feasibility SUB-SOLVE's own terminal status at the last
    ///        measured restoration entry, or `kNotRun` (M7 W2 M3a).
    ///
    /// `sub_status` of the excursion record: the `SqpResult::status` the nested
    /// restoration driver returned, mapped through
    /// `to_restoration_sub_status` and read BEFORE the parent decides how its
    /// own exit is spelled. It is the field that tells the phase's exit ARMS
    /// apart directly rather than by inference:
    ///
    ///   `kNotRun`          NO restoration sub-solve was measured on this
    ///                      solve. The DEFAULT, and the only value here that is
    ///                      not a reading (M7 W2 M3a fix1 -- see below).
    ///   `kOptimal`         the sub-solve converged on the feasibility problem.
    ///                      On this value, and only on it, the parent puts the
    ///                      CERTIFICATE gate to the returned point, so
    ///                      `SqpResult::infeasibility_certified` carries a
    ///                      decision rather than a structural `false`.
    ///   `kInfeasible`      the feasibility problem ITSELF raised a restoration
    ///                      request, which nothing can service (a nested
    ///                      restoration is refused). Before the M7 W2 ladder
    ///                      round (L1); that refusal now reads `kNumericalError`
    ///                      here, and the parent reports `kNumericalError`
    ///                      either way, making NO CLAIM about the model.
    ///   `kMaxIter`         the sub-solve spent the majors the parent lent it.
    ///   `kInterrupted`     the caller's callback stopped the sub-solve.
    ///   `kNumericalError`  the sub-solve failed numerically.
    ///
    /// Those five and `kNotRun` are what this field has ever held. The enum
    /// carries SolveStatus's remaining four (`kAcceptable`, `kStalled`,
    /// `kDiverging`, `kBudgetExhausted`) so that the map into it is TOTAL and
    /// needs no default arm; the SQP engine reports the first three on no exit
    /// at all, and the sub-solve runs with `budget_mode` forced false.
    ///
    /// WHAT IT DOES NOT SAY IS WHETHER THE PARENT RESUMED (M7 W2 M3a fix1).
    /// The resume decision is `restoration_feas_out <= SqpOptions::feas_tol`
    /// (`restoration_h_out` until M7 W2 SNR) and NOTHING ELSE -- `enter_restoration` takes it
    /// before it looks at the sub-solve's status at all (the M7 W2 P1E sufficient-reduction resume,
    /// off by default, is the one exception: it reads the status, see
    /// `restoration_reduction_resumes`) -- so a sub-solve that returned a feasible
    /// enough point with `kMaxIter` or `kInterrupted` resumes exactly as a
    /// `kOptimal` one does. It is CERTIFICATION that requires `kOptimal`. An
    /// earlier draft of this note said the resume was reachable from `kOptimal`
    /// alone, which was a statement about the documentation and not about the
    /// code. (Whether resume SHOULD be gated on the sub-status is an open
    /// engine question, registered with the M7 W2 M3a artifact; this field
    /// describes today's behaviour and changes none of it.)
    ///
    /// WHY IT IS HERE. M7 W2 M2b could not say which of those arms its
    /// `kInfeasible` rows took: the parent's status is arm-faithful only up to
    /// the two `kInfeasible` arms, which it spells identically, and the row
    /// shape had to be read for the difference
    /// (`docs/notes/data/2026-09-m7-w2-m2b-proximity/README.md`, finding 1 and
    /// its "what the evidence does not support"). This field is the
    /// observation that inference stood in for.
    ///
    /// OVERWRITTEN per entry, NOT folded, and written at exactly the two arms
    /// the four absolutes above are written at -- the same `record` call. The
    /// three REFUSALS and the NON-FINITE exit write nothing here either, for
    /// their own reasons, so the record stays one record.
    ///
    /// AND ITS ABSENCE IS ITS OWN VALUE, unlike the four absolutes above. They
    /// have no spare reading to spend -- `0.0` is a legal violation and a legal
    /// objective -- so absence for THEM stays `restoration_entries == 0`. This
    /// field is an enum with a value to spare, so it spends it: `kNotRun` is
    /// the default, it is what a never-restored solve reports here, and it is
    /// what the trace spells `not_run`. The first draft defaulted to the
    /// enum's zero `kOptimal` and left absence to the count, which made a clean
    /// solve print `optimal` for a sub-solve it never ran; the review called
    /// that a consumer hazard and the settler withdrew it.
    RestorationSubStatus restoration_sub_status = RestorationSubStatus::kNotRun;

    /// THE QP ENGINE'S E4 GROUP, summed over every QP solve this driver spent --
    /// subproblems, SOC re-solves, elastic rungs, tier-3 face re-solves and the
    /// restoration sub-solve alike, exactly like qp_minor_iters/factorizations and
    /// the refinement counters above. Each carries its QpCounters meaning
    /// UNCHANGED (solver_counters.h's own notes there), and they are here because
    /// a driver-level reader -- the harness lane, the ledger, a bench row -- cannot
    /// otherwise see that a subproblem's factor failed to solve its own system, or
    /// what the regularization cost to take back out.
    ///
    /// `eqp_residual_refusals` is the one to WATCH: nonzero means some EQP solve's
    /// answer did not solve the system it came from. It is not an error by itself
    /// (the engine's retreats answer it), and a solve that reports both a refusal
    /// and kOptimal took one of them successfully. `eqp_row_drift_refusals` is its
    /// twin in the STEP's currency -- an answer that did solve its system and
    /// still left a closed working row -- and the two are disjoint.
    Index eqp_refine_iters = 0;
    Index eqp_residual_refusals = 0;
    Index eqp_polish_steps = 0;
    Index terminal_polish_solves = 0;
    Index terminal_polish_rejected = 0;
    Index terminal_polish_adopted = 0;

    /// LAST in the struct and last in the trace table, on the declared
    /// additive-trailing-key route (docs/trace-schema-v0.md section 8), so no
    /// existing key's value or bytes move. Its place in `QpCounters` is beside
    /// `eqp_residual_refusals`, where the meaning reads.
    Index eqp_row_drift_refusals = 0;

    // THE FAILED-SUBPROBLEM STEP GROUP (M7 W2 R2, `SqpOptions::qp_failure_step`).
    // Appended last, on the additive-trailing-key route.

    /// @brief Failed walk subproblems whose own step was JUDGED by the
    ///        globalization strategy (the gate passed: usable p != 0, and a
    ///        predicted decrease or a halved violation). 0 with the option off.
    ///        FOLDED from the restoration sub-solve, like `soc_steps`.
    Index qp_failure_steps_judged = 0;
    /// @brief Of those, the steps the strategy ACCEPTED and the driver took.
    ///        `judged - accepted` were ordinary rejections. FOLDED likewise.
    Index qp_failure_steps_accepted = 0;
    /// @brief Subproblems built with a non-finite H, g, A or b entry (a model
    ///        whose derivatives are not finite at the iterate). No kernel is
    ///        called; the solve ends `kNumericalError` at that iterate -- the
    ///        third source of that status beside `qp_maxiter_exits` and
    ///        `qp_numerical_exits`. At most one per driver, FOLDED from the
    ///        restoration sub-solve (so up to two in a solve that restored).
    Index nonfinite_subproblems = 0;
    /// @brief THE BOX-ROUNDING PROJECTION (M7 W2 W1): non-finite subproblems built at
    ///        an iterate outside its box only by the rounding of a step onto a bound,
    ///        re-evaluated at the box projection before the guard above reads them.
    ///        Appended last; FOLDED from the restoration sub-solve.
    Index nonfinite_box_projections = 0;
    /// @brief THE LAST OPTIMAL RUNG (M7 W2 P1E, `SqpOptions::elastic_last_optimal_rung`):
    ///        ladders whose failing rung was replaced by the last solved one. 0 with the
    ///        option off. Appended last; FOLDED from the restoration sub-solve.
    Index elastic_last_rung_fallbacks = 0;
    /// @brief THE SUFFICIENT-REDUCTION RESUME (M7 W2 P1E,
    ///        `SqpOptions::restoration_resume_reduction`): restoration entries that
    ///        resumed with `restoration_feas_out > feas_tol`. 0 with the option off. Not
    ///        folded: the sub-solve never restores.
    Index restoration_reduction_resumes = 0;
    /// @brief THE CURVATURE SOURCE'S BILL (M7 QN stage 3): Lagrangian-Hessian
    ///        evaluations this solve made -- one per subproblem build and one per
    ///        warm-start structure probe under `SqpHessianMode::kExact`, and 0 under
    ///        kLbfgsDamped -- except that under `SqpOptions::restoration_curvature ==
    ///        kRegularized` the phase's sub-solve runs the exact-Hessian path on the
    ///        regularized wrapper, whose eval_hess calls are counted here and never
    ///        reach the caller's model. Appended last; FOLDED from the
    ///        restoration sub-solve. NOT COUNTED, in either mode: the assembly
    ///        bridge's lay-time pattern walk, which reads the Hessian's structure
    ///        through one eval_hess per lay -- so one call per solve and one per
    ///        restoration entry still reaches the model under kLbfgsDamped.
    Index eval_hess_calls = 0;
    /// @brief THE QUASI-NEWTON GROUP (M7 QN stage 3; all 0 under kExact). Pairs
    ///        accepted into the L-BFGS window, and of those the Powell-damped ones.
    ///        FOLDED from the restoration sub-solve, which runs its own instance.
    Index qn_updates = 0;
    Index qn_damped = 0;
    /// @brief Pairs offered and skipped, by reason (lbfgs_damped.h SkipReason):
    ///        non-finite data, a zero step, rounding-level curvature, conditioning
    ///        after damping. FOLDED.
    Index qn_skipped_nonfinite = 0;
    Index qn_skipped_zero_step = 0;
    Index qn_skipped_curvature = 0;
    Index qn_skipped_conditioning = 0;
    /// @brief Pairs evicted early by the window's rank test. FOLDED.
    Index qn_rank_dropped = 0;
    /// @brief Window resets, by reason: consecutive non-zero-step skips, the radius
    ///        floor (a shrink below tr_min), a restoration resume. FOLDED.
    Index qn_resets_consecutive_skips = 0;
    Index qn_resets_radius_floor = 0;
    Index qn_resets_restoration_resume = 0;
    /// @brief PEAKS, folded with max: the most pairs the window held, and the
    ///        widest mixed lift (r+ + r- auxiliaries) any subproblem carried.
    Index qn_pairs_peak = 0;
    Index qn_lift_rank_peak = 0;
    /// @brief Lifted subproblem solves whose engine reported a wrong-inertia reading
    ///        (walk: a re-read, convex certificate or saddle exit; IPQP: an inertia
    ///        retry; SSN: an indefinite escape). A PD B makes these a bug flag: counted,
    ///        never acted on (ruling C30 NOTE: no reset on kWrong). FOLDED.
    Index qn_kwrong_seen = 0;
    /// @brief Lifts built while the quasi-Newton model did not read positive definite
    ///        (rounding can push the eigen form's smallest eigenvalue to or below zero).
    ///        The second bug flag beside qn_kwrong_seen: counted, never acted on. FOLDED.
    Index qn_nonpd_seen = 0;
    /// @brief The number of variables the quasi-Newton model carries curvature for: the
    ///        structural count, or the nonlinear variables only under
    ///        SqpOptions::qn_nonlinear_only. A level, not a sum: max-folded; 0 under kExact.
    Index qn_dimension = 0;
    /// @brief THE NON-FINITE STEP ROLLBACK (M7 W2 R2-1): accepted steps rejected after
    ///        the fact because the committed point's gradient, Jacobian or Hessian was
    ///        not finite. The previous iterate is restored at a shrunk radius instead of
    ///        the solve ending; a rolled-back step stays counted in `steps_accepted`.
    ///        `nonfinite_subproblems` counts only the guard's exits, which this leaves
    ///        to the radius floor and the full-step mode. Appended last; FOLDED from
    ///        the restoration sub-solve.
    Index nonfinite_step_rollbacks = 0;
    /// @brief THE RESUME TEST'S READING (M7 W2 SNR): the inf-norm violation, rows and
    ///        box, at the point the last restoration entry RETURNED -- the measure the
    ///        convergence test gates on (`SqpKkt::feasibility`), in the engine's units.
    ///        `restoration_feas_out <= feas_tol` iff that entry resumed, unless it is
    ///        counted in `restoration_reduction_resumes`. Same overwrite rule as
    ///        `restoration_h_out`; appended last; not folded.
    double restoration_feas_out = 0.0;
    /// @brief THE CALLER-UNIT FEASIBILITY GUARD (M7 W2 SCL): measured iterates at which
    ///        the convergence test passed in the engine's scaled units and was refused
    ///        because the feasibility in the CALLER's units -- each row residual over its
    ///        row factor, each box violation times its column factor -- exceeded
    ///        feas_tol. 0 on a solve that ran unscaled. Appended last; not folded (the
    ///        restoration sub-solve runs unscaled and has no guard). Where the
    ///        caller-unit rounding floor exceeds feas_tol, every iterate is refused
    ///        and the solve ends kMaxIter: a refusal streak to max_iter is the
    ///        expected spelling of that case, and of the stationarity ceiling below
    ///        when its caller-unit floor exceeds 1. ALSO COUNTS (M7 W2 SCW) a
    ///        column-scaled iterate refused by the caller-unit STATIONARITY ceiling
    ///        (<= 1, IPOPT's unscaled dual_inf_tol), which a column factor read where
    ///        its column nearly vanishes can otherwise hide.
    Index scaled_convergence_refusals = 0;
    /// @brief THE RESTORATION PHASE'S CURVATURE SWITCHES (M7 W2 RST): restoration
    ///        sub-solves on the exact model that met a switch trigger
    ///        (SqpOptions::restoration_curvature == kModelThenRegularized): their first
    ///        failed walk subproblem, or an exhausted elastic tier's nested restoration
    ///        request. Counted at the stop; the phase then continues from that point on
    ///        the regularized model unless its budget is spent or the model is not
    ///        finite there, when the stopped sub-solve's exit stands. At most one per
    ///        restoration entry; 0 under every other curvature mode and under
    ///        kLbfgsDamped. Appended last; FOLDED from the restoration sub-solve, where
    ///        it is counted.
    Index restoration_curvature_switches = 0;
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
    X(near_active_peak, counter_never_absent)                                                      \
    X(polish_ignored, counter_never_absent)                                                        \
    X(start_point_clamps, counter_never_absent)                                                    \
    X(qp_maxiter_exits, counter_never_absent)                                                      \
    X(qp_numerical_exits, counter_never_absent)                                                    \
    X(qp_failures_routed, counter_never_absent)                                                    \
    X(qp_failures_in_a_row_peak, counter_never_absent)                                             \
    X(qp_cap_retries, counter_never_absent)                                                        \
    X(restoration_entries, counter_never_absent)                                                   \
    X(restoration_h_in, counter_never_absent)                                                      \
    X(restoration_h_out, counter_never_absent)                                                     \
    X(restoration_f_in, counter_never_absent)                                                      \
    X(restoration_f_out, counter_never_absent)                                                     \
    X(restoration_sub_status, counter_never_absent)                                                \
    X(eqp_refine_iters, counter_never_absent)                                                      \
    X(eqp_residual_refusals, counter_never_absent)                                                 \
    X(eqp_polish_steps, counter_never_absent)                                                      \
    X(terminal_polish_solves, counter_never_absent)                                                \
    X(terminal_polish_rejected, counter_never_absent)                                              \
    X(terminal_polish_adopted, counter_never_absent)                                               \
    X(eqp_row_drift_refusals, counter_never_absent)                                                \
    X(qp_failure_steps_judged, counter_never_absent)                                               \
    X(qp_failure_steps_accepted, counter_never_absent)                                             \
    X(nonfinite_subproblems, counter_never_absent)                                                 \
    X(nonfinite_box_projections, counter_never_absent)                                             \
    X(elastic_last_rung_fallbacks, counter_never_absent)                                           \
    X(restoration_reduction_resumes, counter_never_absent)                                         \
    X(eval_hess_calls, counter_never_absent)                                                       \
    X(qn_updates, counter_never_absent)                                                            \
    X(qn_damped, counter_never_absent)                                                             \
    X(qn_skipped_nonfinite, counter_never_absent)                                                  \
    X(qn_skipped_zero_step, counter_never_absent)                                                  \
    X(qn_skipped_curvature, counter_never_absent)                                                  \
    X(qn_skipped_conditioning, counter_never_absent)                                               \
    X(qn_rank_dropped, counter_never_absent)                                                       \
    X(qn_resets_consecutive_skips, counter_never_absent)                                           \
    X(qn_resets_radius_floor, counter_never_absent)                                                \
    X(qn_resets_restoration_resume, counter_never_absent)                                          \
    X(qn_pairs_peak, counter_never_absent)                                                         \
    X(qn_lift_rank_peak, counter_never_absent)                                                     \
    X(qn_kwrong_seen, counter_never_absent)                                                        \
    X(qn_nonpd_seen, counter_never_absent)                                                         \
    X(qn_dimension, counter_never_absent)                                                          \
    X(nonfinite_step_rollbacks, counter_never_absent)                                              \
    X(restoration_feas_out, counter_never_absent)                                                  \
    X(scaled_convergence_refusals, counter_never_absent)                                           \
    X(restoration_curvature_switches, counter_never_absent)

/// @brief `HVEN_SQP_COUNTERS_FIELDS`' entry count -- the DIRECT fields only.
inline constexpr std::size_t kSqpCountersFieldCount =
    0 HVEN_SQP_COUNTERS_FIELDS(HVEN_COUNTERS_COUNT_ONE);
#undef HVEN_COUNTERS_COUNT_ONE

// PLUS TWO: `ssn` and `ipqp` are nested aggregates, each counted as ONE
// initializer by the arity trick and serialized through its own table above, so
// the table holds the 77 direct fields and the struct takes 79 initializers.
// (37 -> 38 at M6 W5 T8.5, which appended `polish_ignored` after both nested
// aggregates -- last in declaration order, so the table order below is still
// the struct's and every pre-existing offset is unmoved. 38 -> 39 at M7 W2 M4,
// which appended `start_point_clamps` the same way and for the same reason.
// 39 -> 44 at M7 W2 M1, which appended the five-field subproblem-exit group the
// same way and for the same reason. 44 -> 49 at M7 W2 M2, which appended the
// five-field restoration excursion record the same way and for the same reason
// -- and which is where the table first carries `double` entries outside the
// two nested aggregates; they are 8-byte members like every `Index` above them,
// so the offsets stay monotone and the check below is unchanged. 49 -> 50 at
// M7 W2 M3a, which appended `restoration_sub_status` to that record the same
// way and for the same reason. It is the SECOND enum in the table, after
// `start_level_used`, and the first field NARROWER than 8 bytes: a 4-byte
// `RestorationSubStatus` last in the struct, which the struct's own 8-byte
// alignment then pads, so the offsets stay monotone and `sizeof` moves
// 848 -> 856. Its type was a bare `SolveStatus` until M7 W2 M3a fix1 replaced
// it with the ten-valued enum that has an explicit `kNotRun` -- the SAME
// underlying type and therefore the SAME width, so that re-derivation moved
// `sizeof` no further and no pre-existing offset at all. 50 -> 56 at M7 W2 E4
// fix round 1, which appended the QP engine's six-field E4 group -- the EQP-site
// residual check's two, the unregularized polish's one and the terminal polish's
// three -- the same way and for the same reason. They are `Index` again, so they
// land 8-byte-aligned after `restoration_sub_status`'s 4 bytes of padding and the
// offsets stay monotone. 56 -> 57 at M7 W2 E4 fix round 2, which made that group
// SEVEN with `eqp_row_drift_refusals` -- appended LAST in the struct and last in
// the table on the declared additive-trailing-key route, so no pre-existing offset
// moves and `sizeof` goes 904 -> 912. 57 -> 60 at M7 W2 R2, which appended the
// failed-subproblem step group and `nonfinite_subproblems` the same way and for the
// same reason: three `Index` fields last, `sizeof` 912 -> 936. 60 -> 61 at M7 W2 W1,
// `nonfinite_box_projections`, the same way: `sizeof` 936 -> 944. 61 -> 63 at M7 W2
// P1E, `elastic_last_rung_fallbacks` and `restoration_reduction_resumes`, the same way:
// `sizeof` 944 -> 960. 63 -> 77 at M7 QN stage 3, `eval_hess_calls` and the thirteen-field
// quasi-Newton group, the same way: `sizeof` 960 -> 1072. 77 -> 79 at the QN review and
// riders, `qn_nonpd_seen` and `qn_dimension`; 79 -> 80 at M7 W2 R2-1,
// `nonfinite_step_rollbacks`, the same way: `sizeof` 1088 -> 1096. 80 -> 81 at M7 W2 SNR,
// `restoration_feas_out`, the same way: `sizeof` 1096 -> 1104. 81 -> 82 at M7 W2 SCL,
// `scaled_convergence_refusals`, the same way: `sizeof` 1104 -> 1112. 82 -> 83 at M7 W2 RST,
// `restoration_curvature_switches`, the same way: `sizeof` 1112 -> 1120.)
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
