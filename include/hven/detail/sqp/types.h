#pragma once

#include <limits>

#include <Eigen/Dense>
#include <Eigen/SparseCore>

namespace hven::solvers {

// Type aliases
using Index = Eigen::Index;
using SpMatU = Eigen::SparseMatrix<double, Eigen::RowMajor>;
using Vec = Eigen::VectorXd;

// Bound state enumeration
enum class BoundState {
    kFree = 0,
    kAtLower = 1,
    kAtUpper = 2,
    kFixed = 3,
};

// QP solver status enumeration
enum class QpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
};

// Selects which linear-algebra path the QP engine uses to solve each
// working-set's KKT system: kRefactorize re-derives and refactorizes a
// bound-eliminated K (kkt_assembly.h's assemble_kkt) from scratch every
// working-set change; kSchurBorder holds one persistent full-variable K0
// (assemble_kkt_full) and represents working-set changes as Schur-complement
// borders (schur_complement.h) instead of re-factorizing. kSchurBorder is the
// default -- qp_engine.h's QpEngineBorder.BorderModeMatchesRefactorizeMode
// (and the tiny-schur_cap rebuild/recovery batteries alongside it) hold the
// two paths observationally equivalent over the whole fixture set for convex
// H, and border mode is cheaper on the box-shaped (all-bounds-active)
// trust-region case that motivated it. kRefactorize remains selectable and
// stays the equivalence oracle: every border-mode battery is checked against
// it, not the other way around.
//
// "CONVEX H" IS NOT ENOUGH FOR THE TWO MODES TO RETURN THE SAME POINT, and
// the gap is worth naming because a real fixture walked into it (Phase-5
// Task 4, docs/notes/2026-07-31-schur-cap-policy.md section 4). Where H is
// convex but NOT STRICTLY convex the subproblem's optimal set is a FACE, and
// the two paths -- a bordered full-variable K0 versus a bound-eliminated K --
// legitimately return different, equally optimal points of it. Measured on
// tests/test_sqp_restoration.cpp's InfeasibleCircleLineModel, whose Lagrangian
// Hessian is 2*lambda_e(0)*I and is therefore IDENTICALLY ZERO on every major
// whose elastic relaxation is still open (an open relaxation carries no
// multipliers out): both modes land on the same optimal face (x0 + x1 = 2,
// same objective) at different points -- exactly, up to the primal_delta the
// engine adds, which makes the solved system formally strictly convex and the
// "face" a delta-flat set whose selected point is undetermined at O(delta)
// rather than an exact LP face -- and the DRIVER above them then follows
// different trajectories from there -- visible as a status difference only
// under a budget too small for both to finish. So read the equivalence claim
// as "the same optimal VALUE, and the same point wherever that point is
// unique"; it is not a promise about which point of a degenerate optimal face
// comes back. This is a scoping correction to the sentence above, not a
// retraction: every battery that checks the two modes against each other
// solves a strictly convex H and is unaffected.
enum class WorkingSetLinearAlgebra {
    kRefactorize,
    kSchurBorder,
};

// QP solver options
struct QpOptions {
    double primal_delta = 1e-8;
    double dual_mu = 1e-8;
    double feas_tol = 1e-9;
    double opt_tol = 1e-9;
    // MINOR-ITERATION CAP FOR ONE QP SOLVE. Two meanings, by sign:
    //
    //     > 0   an EXPLICIT, ABSOLUTE cap. Wins outright, at any size.
    //    <= 0   THE SENTINEL, AND THE SHIPPED DEFAULT: derive the cap from
    //           this subproblem's own size, as
    //               max(kQpMaxIterFloor, kQpMaxIterCoeff * (n + mi + #bounded))
    //           -- qp_engine.h's detail::effective_qp_max_iter, which carries
    //           the derivation, the calibration and the reasoning in full.
    //
    // PHASE-6 TASK 4 (M6) CHANGED THIS DEFAULT FROM A FIXED 500, on a ruling,
    // and the argument is a LOWER-bound argument rather than a tuning
    // preference. The minors ONE subproblem needs grows with the problem's
    // constraint count (docs/notes/2026-08-03-identification-stall-study.md
    // Sec. 3's cost law), so ANY fixed constant is eventually below the
    // demand: 500 is already below it at n = 250 on the F7 collocation family
    // (docs/notes/2026-07-30-scale-study-cold.md Sec. 5), and 6 of the 15
    // instances of the F7 cold grid were recorded as SQP-engine FAILURES that
    // are nothing but this cap sitting under a demand of 850-7341 minors
    // (docs/notes/2026-08-03-identification-stall-study.md Sec. 5, which
    // recovers three of them outright by raising it and nothing else).
    //
    // THE PRECEDENCE RULE IS THE ESCAPE HATCH, and it is what makes this a
    // safe default change: a caller who sets this field to any positive value
    // gets exactly that value, so every fixture that deliberately caps at 1
    // or 2 to force a kMaxIter exit, and every battery that pins a
    // cap-signature counter, keeps its old behaviour by construction. Only
    // solves that (a) leave this at its default AND (b) actually reach the
    // cap can change at all -- a healthy solve never touches it, which is why
    // the pins that move under M6 are the cap-signature ones and no others.
    //
    // A CAPPED SUBPROBLEM REMAINS AN HONEST FAILURE, unchanged: it reports
    // QpStatus::kMaxIter with minor_iters == the effective cap, and the
    // driver above it routes that as it always has. Raising the cap never
    // converts a wrong answer into a right one; it only stops refusing
    // subproblems that were going to finish.
    Index max_iter = 0;
    Index schur_cap = 128;
    double schur_cond_max = 1e9;
    WorkingSetLinearAlgebra ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;

    // l-infinity trust-region radius Delta, applied about the SOLVE'S OWN
    // start point x0 (there is no separate configurable center -- see
    // qp_engine.h's "Section 6" note): effective bounds
    // lo_eff = max(lower, x0 - Delta), up_eff = min(upper, x0 + Delta) are
    // computed once, at the top of solve(), and used in place of `lower`/
    // `upper` for the rest of that solve. Default +inf disables the feature
    // (lo_eff == lower, up_eff == upper exactly, and the engine takes a path
    // that materializes nothing new -- see qp_engine.h for the bit-identical
    // claim this default is load-bearing for).
    //
    // PER-INSTANCE, CONST: like every other QpOptions field, this is fixed
    // for the lifetime of the owning QpEngine (see its constructor) and
    // cannot vary across solve() calls on one instance THROUGH THIS FIELD.
    // A caller that needs a DIFFERENT radius per call -- e.g. a Phase-3
    // shrink-radius retry loop re-solving a rejected SQP step at a smaller
    // Delta -- uses SolveOverrides::tr_radius (below) instead: it is
    // resolved once per solve() call and never touches this const field, so
    // one QpEngine instance now serves an entire retry loop and keeps its
    // hot-start K0/border reuse across it (see qp_engine.h's PHASE-3 DESIGN
    // FRICTION note, updated for this resolved state, and its HOT-START
    // REUSE note for why tr_radius -- unlike primal_delta/dual_mu below --
    // never has to join the reuse key: bounds never enter K0).
    double tr_radius = std::numeric_limits<double>::infinity();
};

// Per-solve overrides for the three QpOptions fields a Phase-3 SQP driver
// needs to vary call-to-call on ONE QpEngine instance: the trust-region
// radius (shrink-retry loops) and the primal/dual regularization (adaptive
// regularization). Passed to QpEngine::solve()'s override-taking overloads;
// the plain overloads forward a default-constructed SolveOverrides, which
// resolves to every field's opts_ value -- see qp_engine.h's per-field
// resolution rule -- so they are byte-identical to solving with no override
// support at all.
//
// SENTINEL CONVENTION: a field at its default means "no override -- use the
// owning QpEngine's opts_ value for this solve", exactly like QpOptions::
// tr_radius's own default already means "no radius" when read directly.
// tr_radius's sentinel is +inf (same value QpOptions::tr_radius defaults to,
// so there is nothing to encode specially); primal_delta/dual_mu's sentinel
// is any negative value (both are physically positive regularization
// constants, so negative is otherwise meaningless). A caller that needs
// tr_radius genuinely disabled on an engine whose opts_.tr_radius is finite
// can construct that engine's opts_ with tr_radius = +inf in the first
// place -- SolveOverrides has no way to request "+inf, overriding a finite
// opts_ default" because it cannot be told apart from "no override", the same
// single-sentinel tradeoff QpOptions::tr_radius itself already makes.
//
// REUSE-KEY CONSEQUENCE (see qp_engine.h's HOT-START REUSE note): the
// EFFECTIVE (primal_delta, dual_mu) pair this resolves to enters K0's values
// (assemble_kkt_core bakes both onto the regularized diagonal), so it joins
// the hot-start reuse key -- a warm re-solve whose effective pair differs
// from the previous solve's forces a K0 rebuild. The effective tr_radius
// does NOT join that key: it only ever tightens bounds into lo_eff/up_eff,
// and bounds never enter K0 (see qp_engine.h for the full argument).
//
// PRECONDITION, VALIDATED AT SOLVE START (QpEngine::solve throws
// std::invalid_argument otherwise, before anything else in that call can run
// -- see run()'s VALIDATE FIRST block): tr_radius must be either the +inf
// sentinel above or a value >= 0 -- NEVER a negative, non-sentinel Delta and
// NEVER NaN. A negative Delta would silently cross lo_eff/up_eff (section 6's
// CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN proof assumes Delta >= 0), which the
// engine can only catch with an `assert` that a Release (NDEBUG) build
// compiles out entirely -- so this is checked explicitly rather than left to
// that assert, and it is checked because tr_radius is now per-solve DRIVER
// INPUT (a Task-6 shrink-retry loop's own arithmetic), not a value only ever
// set once, by hand, in opts_. primal_delta/dual_mu keep the negative-means-
// sentinel convention above unchanged -- a negative value is valid and
// meaningful for them -- but NaN is rejected for both, for the same reason:
// it is not a value either field's resolution or any downstream arithmetic
// can absorb.
struct SolveOverrides {
    double tr_radius = std::numeric_limits<double>::infinity(); // +inf = use opts_.tr_radius
    double primal_delta = -1.0;                                 // <0 = use opts_.primal_delta
    double dual_mu = -1.0;                                      // <0 = use opts_.dual_mu
};

// QP solver performance counters
//
// THE TWO REFINEMENT COUNTERS COUNT DIFFERENT THINGS ON PURPOSE, because the
// two paths have different mandatory baselines and the interesting quantity
// on each is "steps that would not have been taken before":
//
//   border_refine_steps  TOTAL refinement steps KEPT by every
//                        solve_bordered_eqp call in this solve, INCLUDING the
//                        mandatory first one -- the same accounting
//                        detail::kMaxBorderRefineSteps uses ("total,
//                        including the mandatory first"). So a solve that
//                        goes through the border path at all reports at least
//                        one step per bordered EQP solve, and the excess over
//                        that baseline is what Task 11b's iterated loop
//                        bought.
//   eqp_refine_steps     EXTRA refinement steps kept by every solve_eqp call
//                        in this solve, BEYOND the single unconditional step
//                        solve_eqp takes. IT IS NOW IDENTICALLY ZERO, and is
//                        kept as an instrumented invariant rather than as a
//                        live measurement: the flag-gated iterated loop it
//                        counted (QpOptions::eqp_refine) was DELETED once both
//                        shipped backends measured it inert -- 0 steps across
//                        27 HS problems x 2 tolerance regimes x 2 algebra
//                        modes plus 13 adversarial probes, on MKL and on
//                        Accelerate independently, with a structural identity
//                        (r_k = diag(reg)*c) explaining why a path whose first
//                        solve is a genuine regularized solve cannot fire the
//                        shared stopping rule. See
//                        docs/notes/2026-07-29-eqp-refinement-ab.md's
//                        DISPOSITION section and the Phase-C verdict in
//                        docs/notes/2026-07-29-accelerate-audit-results.md.
//                        A nonzero reading here would mean solve_eqp grew a
//                        second refinement step, which is a change, not a
//                        measurement -- which is exactly why the counter and
//                        its assertions stayed behind when the loop went.
//
// "KEPT" is the operative word in both: a candidate step rejected by the
// strict-decrease acceptance rule is discarded and NOT counted, so these
// count steps that moved the answer, not solves attempted.
struct QpCounters {
    Index factorizations = 0;
    Index schur_updates = 0;
    Index minor_iters = 0;
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    // Rungs of the SUSPECT-STALL ESCALATION LADDER this solve spent (see
    // qp_engine.h's section 4b). Each rung multiplies the effective
    // primal_delta by detail::kSuspectDeltaFactor after a would-be-kOptimal
    // exit off a kSuspect factorization failed the free-block stationarity
    // check. Zero on every solve whose linear algebra was trustworthy, which
    // is the overwhelming majority; a solve that reports kNumericalError with
    // this at detail::kMaxSuspectEscalations exhausted the ladder, and that
    // pairing IS the exhaustion diagnostic.
    Index suspect_escalations = 0;

    // PHASE-4 TASK 4 FIX ROUND 1. True iff qp_engine.h's border-mode reuse
    // gate (`reuse_eligible` in QpEngine::run(), conditions (a)-(e)) judged
    // the persisted K0/border cache trustworthy for THIS solve() call --
    // i.e. whether the engine actually skipped rebuilding K0 on the
    // strength of its OWN or an ADOPTED hot-start handle's history, not
    // merely whether `factorizations` happens to read 0. The two are NOT
    // the same claim: `factorizations == 0` can also arise from a reduced
    // system with nothing to factorize at all (every variable pinned, no
    // equalities, no working rows -- the elimination path's empty-system
    // short-circuit, reachable from border mode too) or from a solve that
    // never reached the loop (a crossed-bounds box, reported kInfeasible),
    // NEITHER of which says anything about the reuse cache. This field is
    // the direct signal a caller (sqp_driver.h's SqpCounters::
    // start_level_used correction) should read instead of inferring reuse
    // from the factorization count. Always false under
    // QpOptions::ws_algebra == kRefactorize, where the border-mode cache
    // does not exist.
    bool k0_reused = false;

    // PHASE-4 TASK 4 FIX ROUND 2. Number of backend SYMBOLIC-ANALYSIS
    // calls this solve() call actually paid for -- i.e. the number
    // of times qp_engine.h's rebuild_k0() found `needs_analysis(kkt, K)`
    // before calling `factorize_checked()` (which itself skips
    // the analysis whenever the sparsity pattern is unchanged from the last
    // analyzed matrix -- kkt_calls.h). Counted here, at the call site,
    // rather than inside the factor, so this stays a QP-engine-level
    // observable like every other QpCounters field and touches no
    // MKL-adjacent code. This is the direct regression net for the
    // re-review's finding 2c: detaching onto a FRESH BorderState (a fresh
    // KktFactor, with no cached pattern) on every value-changing major of an
    // ordinary, NEVER-SHARED solve paid a full re-analysis per major --
    // measured at 48 on HS38's own 48-major solve, vs 1 for a solve whose
    // sole-owner rebuilds reuse the SAME KktFactor's cached pattern. A
    // never-shared solve should show this at 1 (or 0, if the pattern never
    // needed rebuilding at all) regardless of how many `factorizations` it
    // pays; a solve whose fixture legitimately forces detaches (the
    // sharing/identity-mismatch tests) is expected to show more.
    Index symbolic_analyses = 0;

    // ---------------------------------------------------------------------
    // PHASE-6 TASK 3 -- THE WORKING-SET WALK COUNTERS (five here, six more
    // below in fix round 1 -- eleven in total).
    //
    // PURELY OBSERVATIONAL. None of the ELEVEN walk counters (the five here
    // and the six added in fix round 1, below) is read by any decision in
    // qp_engine.h; they are written and never consulted, so the walk's
    // trajectory is bit-for-bit what it was before they existed (verified:
    // `hven_sqp_bench --self-check` byte-exact and the full suite green in
    // both build configurations at the commit that added them).
    //
    // WHY THEY EXIST. Phase 5 closed with the F7 wide-window minor-stall as
    // an explicitly-owned open carry whose MECHANISM was unknown
    // (docs/notes/2026-08-01-phase-5-results.md Sec. 12). The counters that
    // existed then could not discriminate its candidate mechanisms, and the
    // reason is a level mismatch rather than a missing measurement: every
    // pre-existing field above counts LINEAR-ALGEBRA events (factorizations,
    // Schur updates, symbolic analyses, refinement steps), while every
    // candidate mechanism -- a degenerate stall, an add/drop churn cycle,
    // homotopy thrash, or a genuinely long monotone identification walk --
    // is a statement about the COMBINATORIAL walk, which nothing observed.
    // `minor_iters` alone cannot tell a solve that spent 510165 minors
    // cycling from one that spent them making progress, and
    // docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2 says so directly
    // ("the pathology is entirely in HOW MANY, not in how expensive each
    // one is"). These five close exactly that gap and nothing wider; the six
    // added in fix round 1 (below) close a second one the review found inside
    // it -- these five cannot tell an inequality ROW from a variable BOUND.
    //
    // THE ARITHMETIC A READER SHOULD DO WITH THEM. For a solve that ends at
    // a working set of W members having started from a seed of S:
    //
    //     ws_adds - ws_drops + shift_adds  ==  W - S   (net identity)
    //
    // so `ws_drops` is the CHURN: a monotone identification walk that never
    // backtracks has ws_drops == 0 and ws_adds ~ W - S, while a walk that
    // pays k re-discoveries of the same rows has ws_adds ~ (W - S) + k and
    // ws_drops ~ k. The ratio ws_drops / minor_iters is therefore the direct
    // churn fraction, and degenerate_steps / minor_iters the direct
    // degeneracy fraction; the two are independent and a stall can be either,
    // both, or neither (in which case the walk is simply long).
    //
    // **THE IDENTITY HOLDS ON THE CONVEX PATH, AND HAS EXACTLY ONE NAMED
    // EXCEPTION** (final fix wave, W8; the exception was always there and no
    // note said so). qp_engine.h's TEMPORARY-VERTEX START REPAIR (section 4b,
    // `repair_temporary_vertex` -> `pin_at_best_bound`) writes
    // `ws.bound_state()` DIRECTLY -- it pins variables onto bounds, and its
    // release pass unpins them -- WITHOUT touching QpCounters and without
    // marking the variable in the seen-set. So on any solve that ran the
    // repair:
    //   - the net identity above is off by the number of pins the repair left
    //     standing (S, the seed, is effectively enlarged by them);
    //   - `ws_adds_bound`/`ws_drops_bound` do not include the repair's pins or
    //     its releases;
    //   - `distinct_bound_added` does not count a variable whose ONLY
    //     admission was a repair pin, so the bound-repeat ratio below is
    //     computed over a set that excludes that route.
    // The repair runs only at `iter == 0` and only on a subproblem whose
    // inertia probe returned kWrong -- i.e. an INDEFINITE start vertex -- so
    // every convex corpus in this project reports these fields exactly, and
    // that is why the omission has never shown up in a measurement. It is
    // documented rather than plumbed deliberately: the counters exist to
    // describe the COMBINATORIAL WALK, and the repair is a pre-walk vertex
    // construction, not a step of it -- adding it to `ws_adds` would make the
    // churn fraction read a start-point property as walk churn. A reading that
    // needs the repair's pins must say so and count them separately.
    //
    // ws_adds        BLOCKING-CONSTRAINT ADDITIONS: one per minor iteration
    //                whose ratio test named a blocker that then joined the
    //                working set, counting an inequality row and a variable
    //                bound alike (BlockKind::kIneq / kBound). The
    //                negative-curvature ride's own blocker (section 4c) is
    //                counted here too, since it is the same ratio test
    //                reporting the same kind of event; a minor that took a
    //                full unblocked step to the EQP point (kind == kNone)
    //                adds nothing and is counted nowhere.
    //
    // ws_drops       WORKING-SET REMOVALS: one per constraint released by
    //                the Dantzig drop rule (drop_worst) plus one per
    //                constraint released by the zero-multiplier probe
    //                (probe_zero_multiplier_drops). A probe drop that is
    //                RESTORED because the probe declined it is not counted --
    //                the working set is the same on both sides of it, so
    //                counting it would break the net identity above.
    //
    // shift_adds     HOMOTOPY ADMISSIONS: rows that refresh_shifts() put
    //                into the working set because they are still violated
    //                (shift > 0), which is a structurally different event
    //                from a blocking-constraint add -- the drop rule
    //                deliberately SKIPS shifted rows, so a row admitted this
    //                way cannot leave until its shift reaches zero. INCLUDES
    //                the pre-loop refresh_shifts() call, i.e. the seed's own
    //                initial admission, which is why a healthy solve reports
    //                a large value here on its first QP and near-zero after.
    //
    // degenerate_steps  DEGENERATE PIVOTS: minor iterations that added a
    //                blocking constraint while moving the iterate by no more
    //                than the loop's own step tolerance (alpha * ||p||inf <=
    //                feas_tol * max(1, ||x||inf) -- the SAME threshold the
    //                loop's KKT-point test uses one line earlier, so the two
    //                are consistent by construction rather than by a new
    //                tolerance). This is the textbook degeneracy signal: the
    //                working set grew but the objective could not have
    //                improved. qp_engine.h implements NO anti-cycling rule
    //                (no Bland, no Harris, no EXPAND, no perturbation) and
    //                says so; this field is the first observation of whether
    //                that omission ever costs anything on a real corpus.
    //
    // degenerate_run_max  The LONGEST CONSECUTIVE run of degenerate steps in
    //                this solve, where "consecutive" means back-to-back minor
    //                iterations. **A RUN IS BROKEN BY A MINOR ON WHICH THE
    //                ITERATE MOVED, NOT BY ANY MINOR THAT MERELY DIFFERS IN
    //                KIND** -- this line used to say "an ordinary step, a
    //                drop, a probe, a ride" all reset it, and the drop and the
    //                probe never have (final fix wave, W7; the doc was wrong,
    //                not the code). qp_engine.h resets the run in exactly
    //                three places and states the omissions deliberately: a
    //                non-degenerate ordinary step, a taken RIDE, and a START
    //                REPAIR. A DROP iteration and a ZERO-MULTIPLIER PROBE do
    //                NOT reset it, because both snap x onto an EQP point
    //                already within step_tol of where it was -- the iterate
    //                did not move, and a run that spans them is still one
    //                stall. **SO A LARGE VALUE HERE IS "the longest stretch on
    //                which the iterate did not move", NOT "an unbroken run of
    //                back-to-back degenerate ADDS"**, and any reading of a
    //                measured figure has to say which of the two it means.
    //                (`degenerate_steps` above is unaffected: it counts the
    //                degenerate adds themselves and no run logic enters it.)
    //                This is the field that tells the two
    //                degeneracy pictures apart, and they call for different
    //                remedies: degenerate steps SPRINKLED through a healthy
    //                walk are the ordinary cost of a vertex with more active
    //                constraints than dimensions and want nothing done about
    //                them, while a long unbroken run is an iterate that has
    //                STOPPED MOVING while the working set keeps changing --
    //                the cycling class, and the only class an anti-cycling
    //                rule (Bland/Harris/EXPAND) would address. It is reported
    //                rather than acted on: qp_engine.h still implements no
    //                anti-cycling rule and this field changes no decision.
    //
    // ---------------------------------------------------------------------
    // PHASE-6 TASK 3, FIX ROUND 1 -- THE SIX FIELDS THAT MAKE THE ABOVE
    // TRACEABLE RATHER THAN INFERRED. The review found the study's central
    // "the walk RE-DISCOVERS its rows" claim was an inference from
    // ws_adds >> |W*| plus a pigeonhole argument that does not actually
    // close: ws_adds and ws_drops MERGE inequality rows with variable bounds
    // and carry no constraint IDENTITY, so a walk touching many DISTINCT
    // bounds once each -- the brief's own "bound-flip" alternative -- fits
    // every number equally well. These six close it by direct measurement.
    //
    // ws_adds_bound   Of `ws_adds`, how many were VARIABLE-BOUND pins
    //                 (BlockKind::kBound). The inequality-row half is
    //                 ws_adds - ws_adds_bound. `shift_adds` needs no such
    //                 split: refresh_shifts() only ever adds inequality rows.
    // ws_drops_bound  Of `ws_drops`, how many released a variable bound
    //                 rather than an inequality row, on both drop routes.
    //
    // distinct_ineq_added / distinct_bound_added
    //                 How many DISTINCT inequality rows / DISTINCT variables
    //                 this solve ever put into the working set, by ANY route
    //                 (blocking add, ride, or homotopy admission). These are
    //                 the fields that settle re-discovery without an
    //                 argument: the mean number of times a touched
    //                 constraint was admitted is
    //
    //                   (ws_adds - ws_adds_bound + shift_adds) / distinct_ineq_added
    //
    //                 for rows and ws_adds_bound / distinct_bound_added for
    //                 bounds. **THAT RATIO IS ADMISSION MULTIPLICITY, WHICH
    //                 COUNTS EACH CONSTRAINT'S FIRST ADMISSION** -- so the
    //                 number of RE-admissions is the ratio MINUS ONE, and a
    //                 reading must say which of the two it quotes (final fix
    //                 wave, W3: published prose had called the multiplicity a
    //                 re-admission rate). A value near 1 means "touched once,
    //                 never repeated" (the bound-flip picture); a value well
    //                 above 1 means re-discovery, per constraint class
    //                 separately. `distinct_bound_added` additionally excludes
    //                 the start repair's pins -- see the named exception in
    //                 THE ARITHMETIC A READER SHOULD DO WITH THEM above.
    //
    // drop_ties       Drop decisions (drop_worst) in which TWO OR MORE
    //                 candidates fell inside the rule's relative tie window
    //                 (detail::kEngineDropTieTol), i.e. decisions actually
    //                 settled by the largest-angle/first-index tie-break
    //                 rather than by the most-negative multiplier.
    // ratio_ties      Ratio tests whose blocking constraint was chosen from
    //                 TWO OR MORE candidates at EXACTLY the same minimum
    //                 ratio, i.e. settled by the scan order (inequality rows
    //                 before bounds, ascending index) rather than by the
    //                 ratio. EXACT equality, deliberately: the tie that
    //                 matters for degeneracy is several constraints at zero
    //                 slack, and those compare bit-equal. NEAR-ties are not
    //                 measured -- counting them needs a second pass over
    //                 every candidate on every minor, which is a real cost in
    //                 a loop that runs half a million times.
    //
    // Together, drop_ties and ratio_ties are what let the tie-break question
    // the Phase-6 brief named as a deliverable ("ties?") be answered from a
    // measurement instead of from the degeneracy zero, which is a statement
    // about STEP LENGTH and says nothing about tie multiplicity.
    Index ws_adds = 0;
    Index ws_drops = 0;
    Index shift_adds = 0;
    Index degenerate_steps = 0;
    Index degenerate_run_max = 0;
    Index ws_adds_bound = 0;
    Index ws_drops_bound = 0;
    Index distinct_ineq_added = 0;
    Index distinct_bound_added = 0;
    Index drop_ties = 0;
    Index ratio_ties = 0;
};

} // namespace hven::solvers
