// sqp_driver.cpp — SqpDriver's major loop, and everything the loop
// orchestrates.
//
// M3 PHASE-C TASK T5. This TU holds the DEFINITIONS of every SqpDriver member
// function except the two constructors; the class, its data members, its
// documentation and its constructors stay in include/hven/drivers/sqp_driver.h,
// which is unchanged in every other respect. Nothing about the algorithm moved:
// the loop's phases, its arithmetic, its branch order, its counters and its
// exit table are the lines that were in the header, verbatim, one indentation
// level shallower.
//
// WHY. CLAUDE.md §5's rule is that orchestration and drivers live in .cpp TUs
// while per-element hot paths that depend on inlining stay header-side, and
// that a TU boundary is drawn "never at measurable runtime cost". solve_impl is
// ~2 150 lines that ran through the front end of EVERY translation unit
// including this header — twenty-odd test objects, four bench objects, the
// library — for a function each of them calls at most once per solve. It is the
// largest single body in the SQP tree and the plan's (§6, T1–T9) biggest
// build-side candidate.
//
// WHAT THE BOUNDARY COSTS, MEASURED RATHER THAN ASSUMED. The plan flagged this
// as the series' highest bench risk because the loop "currently inlines
// eval_nlp, evaluate_kkt, build_subproblem". Read out of the base commit's own
// object files, it does not: in bench_corpus.cpp.o at 718eef4, solve_impl's
// relocation set holds one PLT32 call per SOURCE call site for every one of
// them — eval_nlp 3/3, eval_nlp_values 3/3, evaluate_kkt 3/3, build_subproblem
// 2/2, predicted_decrease 2/2, crash_basis_seed 1/1 — i.e. ZERO of the fourteen
// sites was inlined at -O3 before this carve. They are ordinary calls on both
// sides of the boundary, and they remain inlinable candidates here anyway,
// since sqp_driver.h comes along with this TU. The same is true of QpEngine::run
// (5 relocations) and SsnEngine::solve (1): both were already call boundaries.
//
// The members that the base build DID inline into solve_impl — map_status,
// shrink_hits_floor, shrunk_radius, restoration_restart_radius, ssn_engine(),
// and 3 of finish()'s 10 sites — all MOVE HERE WITH IT, into the same TU, so
// the inliner sees exactly the input it saw before. That is the mechanical
// reason the trust-region UPDATE logic travels with the loop rather than
// staying behind, and it is also what the S3 ruling (S3-RULING-FINAL, folded
// into the phase-C plan's S3 section) requires on separate, design grounds: the
// TR POLICY — the constants kTrShrinkFactor (detail/globalization/sqp/
// trust_region.h) and kRestoreRadiusFactor (…/restoration.h) — stayed in
// globalization at S3 because it is policy; the three functions that CONSUME
// those constants to advance the radius are loop orchestration reading opts_,
// and belong wherever the loop is. They are here.
//
// FP ARITHMETIC CROSSES THIS BOUNDARY. Everything the major loop computes is
// now compiled in this TU rather than in each includer's. Under the ONE uniform
// flag regime U0 established (CLAUDE.md §7), both sides carry the same flags, so
// the expressions are compiled as they were compiled inline — and the PROOF of
// that is bit-identity of every asserted counter across the carve (T5's P-BENCH
// on 36 columns × 17 cells and P-CENSUS on the 57-cell walk corpus), not an
// argument. A counter delta here is a FAILED CARVE, to be reverted or redrawn;
// it is never a re-derivation.

#include <hven/drivers/sqp_driver.h>

namespace hven::solvers {

namespace {

// A shared_ptr that OWNS NOTHING, aimed at a model the CALLER owns.
//
// shared_ptr's aliasing constructor over an EMPTY owner: the stored pointer is
// `&model`, there is no control block, and destruction does nothing at all.
// That is exactly the shape a bridge built for the duration of one solve()
// call needs. This class's model-taking overloads have always taken a
// `const NlpModel &` whose lifetime is the caller's business and which
// outlives the call by their own precondition; wrapping it in an OWNING handle
// would be a claim on that lifetime none of them is entitled to make, and
// copying the model is not on the table (NlpModel is an interface).
//
// The bridge's own null check reads the STORED pointer, not the owner, so a
// borrowed handle is a legal argument -- and `&model` is never null.
std::shared_ptr<const NlpModel> borrow_model(const NlpModel &model) {
    return std::shared_ptr<const NlpModel>(std::shared_ptr<const void>(), &model);
}

// THE BOX IS THE MODEL'S OTHER SIZED RETURN (M3 final review, S-1), and it is
// checked ONCE, here, rather than at each of the several sites that index it.
// lower()/upper() are read coordinate-wise by evaluate_kkt's activity test and
// by the subproblem's l - x .. u - x window, both of which loop i = 0..n-1 with
// no size of their own to compare against; a model returning a short box
// therefore reads out of bounds in Release, where Eigen's own assert is
// compiled out. Two O(1) comparisons per solve, against the model's own n().
//
// AT THE MODEL BOUNDARY NOW, which is a move of one call site and not a
// change of rule. The check used to sit at the top of solve_impl, where a
// `model` was still in scope; the loop now measures against the SEAM, whose
// box is materialized from the bridge's declaration and is n-sized by
// construction, so there is no model return left there to disagree with
// anything. What is being validated is what the MODEL returned, so it is
// validated where a model is handed in -- and BEFORE the bridge is built, so
// that this diagnostic (which names both blocks, both sizes and the driver
// entry the caller actually called) is the one that fires rather than the
// bridge's own single-block message.
void require_declared_box(const NlpModel &model) {
    const Index n = model.n();
    if (model.lower().size() != n || model.upper().size() != n) {
        throw std::invalid_argument(
            fmt::format("SqpDriver::solve: model.lower()/model.upper() are sized ({}, {}), "
                        "expected ({}, {}) (= model.n())",
                        model.lower().size(), model.upper().size(), n, n));
    }
}

} // namespace

SqpSolution SqpDriver::solve(const NlpModel &model) { return solve(model, model.start_point()); }

void SqpDriver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    ledger_ = ledger;
    label_prefix_ = std::move(label_prefix);
    solve_counter_ = 0;
    engine_.attach_ledger(ledger, label_prefix_ + "_qp");
}

// The two model-taking overloads are wrappers. Each validates the model's box,
// borrows it into a bridge that lives exactly as long as the call, and delegates
// to its bridge-taking twin. Nothing about their contract moved: the same
// argument checks fire in the same order with the same messages, and the solve
// they run is the same solve. See docs/notes/2026-08-21-m4-task5-design.md.
//
// The bridge is built per call, deliberately. Laying it walks the model's three
// derivative patterns once (model/nlp_model_aggregate.h) -- real work, outside
// the timed region below because it is setup, and paid per solve rather than
// per major. A caller who solves the same model repeatedly and does not want
// to pay it can hold its own NlpModelAggregate and call the bridge-taking
// overload directly, which is the whole reason that overload is public.
SqpSolution SqpDriver::solve(const NlpModel &model, const Vec &x0) {
    require_declared_box(model);
    NlpModelAggregate bridge{borrow_model(model)};
    return solve(bridge, x0);
}

SqpSolution SqpDriver::solve(const NlpModel &model, const Vec &x0, const WarmStart &warm,
                             Index minor_budget) {
    require_declared_box(model);
    NlpModelAggregate bridge{borrow_model(model)};
    return solve(bridge, x0, warm, minor_budget);
}

SqpSolution SqpDriver::solve(NlpModelAggregate &bridge, const Vec &x0) {
    // The seam is laid ONCE per solve, before the clock starts, for the same
    // reason the bridge is: it is setup, not iteration. The seam binds the
    // claim-stream interface the bridge derives from; the bridge itself stays
    // in this frame and rides into solve_impl for the restoration phase's one
    // Level 1 read.
    AggregateEvalSeam seam{bridge};
    // PHASE-5 TASK 2. Timed around solve_impl ALONE -- never around
    // model construction, x0/warm setup above, or record_solve's own
    // ledger bookkeeping below -- per ledger.h's SqpSolveRecord::
    // wall_seconds note (informational, never asserted).
    const auto t0 = std::chrono::steady_clock::now();
    SqpSolution out = solve_impl(seam, bridge, x0, WarmStart{}, /*minor_budget=*/0);
    const auto t1 = std::chrono::steady_clock::now();
    return record_solve(std::move(out), std::chrono::duration<double>(t1 - t0).count());
}

SqpSolution SqpDriver::solve(NlpModelAggregate &bridge, const Vec &x0, const WarmStart &warm,
                             Index minor_budget) {
    AggregateEvalSeam seam{bridge};
    // PHASE-5 TASK 2. Same timing scope as the 2-arg overload above.
    const auto t0 = std::chrono::steady_clock::now();
    SqpSolution out = solve_impl(seam, bridge, x0, warm, minor_budget);
    const auto t1 = std::chrono::steady_clock::now();
    return record_solve(std::move(out), std::chrono::duration<double>(t1 - t0).count());
}

SqpSolution SqpDriver::record_solve(SqpSolution out, double wall_seconds) {
    // PHASE-7 TASK 5: THE PROXIMAL CARRY, EXPORTED. Stamped here rather
    // than in make_warm_start because make_warm_start is static (it is
    // called from a context with no `SqpDriver&`) while the accumulator is
    // per-driver state, and because this function is the ONE point every
    // public solve() overload funnels through -- solve_impl has a dozen
    // exits and stamping at each would be a dozen chances to miss one.
    //
    // GATED ON A NONZERO SIGMA, which is what makes the whole block free at
    // the shipped default: at `qp_mode == QpMode::kWalk` no SSN subproblem
    // runs, `ssn_prox_sigma_out_` is still 0.0, and the emitted WarmStart
    // is byte-for-byte the one this driver has always emitted -- the two
    // centre vectors stay empty and `has_prox_center` stays false. See
    // warm_start.h's THE COST GATE.
    //
    // THE GATE IS THE SIGMA, NOT THE CENTRE, and since fix round 1 those
    // are two different high-water marks (see the members): a solve whose
    // ladder armed only on subproblems that ESCAPED exports a real sigma
    // with BOTH centre vectors empty. That is the honest reading, and
    // warm_start.h's own field note already admits the shape ("n, or
    // empty").
    if (ssn_prox_sigma_out_ > 0.0) {
        out.warm_start.has_prox_center = true;
        out.warm_start.prox_sigma = ssn_prox_sigma_out_;
        out.warm_start.prox_center_x = ssn_prox_center_x_out_;
        out.warm_start.prox_center_lambda = ssn_prox_center_lambda_out_;
    }
    if (ledger_ != nullptr) {
        SqpSolveRecord rec;
        rec.label = fmt::format("{}_{}", label_prefix_, solve_counter_);
        rec.status = out.status;
        rec.counters = out.counters;
        // PHASE-4 TASK 7. Flat copies of a subset of `counters` above,
        // added for the Phase-6 benchmark's direct field access -- see
        // ledger.h's SqpSolveRecord note for why these duplicate rather
        // than replace `counters` (which remains the complete,
        // authoritative source every one of these is copied from, here,
        // in the same statement list, so the two can never disagree).
        rec.start_level_used = out.counters.start_level_used;
        rec.full_step_majors = out.counters.full_step_majors;
        rec.watchdog_restores = out.counters.watchdog_restores;
        rec.soc_steps = out.counters.soc_steps;
        rec.soc_applied = out.counters.soc_applied;
        rec.border_refine_steps = out.counters.border_refine_steps;
        rec.eqp_refine_steps = out.counters.eqp_refine_steps;
        // FACTORIZATIONS_SAVED (Task 7's one genuinely new field -- see
        // ledger.h for the full definition and why it is derived exactly
        // this way rather than from qp_factorizations directly).
        rec.factorizations_saved = out.counters.start_level_used == StartLevel::kHot ? 1 : 0;
        // PHASE-7 TASK 5. The three SSN columns, copied from the same
        // `out.counters` in the same statement list as the seven above --
        // see ledger.h's SqpSolveRecord for why they duplicate rather than
        // replace `counters.ssn`, which stays the authoritative source and
        // carries all six fields.
        rec.ssn_iters = out.counters.ssn.ssn_iters;
        rec.ssn_bulk_flips = out.counters.ssn.ssn_bulk_flips;
        rec.ssn_escapes = out.counters.ssn.ssn_escapes;
        rec.wall_seconds = wall_seconds;
        ledger_->record(rec);
        ++solve_counter_;
    }
    return out;
}

SqpSolution SqpDriver::solve_impl(AggregateEvalSeam &seam, NlpModelAggregate &bridge, const Vec &x0,
                                  const WarmStart &warm, Index minor_budget) {
    const Index n = seam.n();
    // The seam and the bridge must name ONE provider. Restoration builds its
    // feasibility model from the bridge while every evaluation goes through the
    // seam, so two different providers here would solve one problem and
    // linearize another -- silently, with no crash and no bad status, surfacing
    // only as convergence that makes no sense. One pointer compare per solve.
    if (&seam.aggregate() != &bridge) {
        throw std::invalid_argument(
            "SqpDriver::solve: the evaluation seam and the bridge name different providers; "
            "restoration reads the bridge while every evaluation goes through the seam, so the "
            "two must be laid over one aggregate");
    }
    if (x0.size() != n) {
        throw std::invalid_argument(fmt::format(
            "SqpDriver::solve: x0 has size {}, expected {} (= model.n())", x0.size(), n));
    }
    // A NON-FINITE x0 IS CALLER INPUT, so it is rejected rather than
    // reported as a status -- the same treatment as a NaN tr_init, and
    // for the same reason. Left unchecked it used to be certified
    // kOptimal in zero majors: every model quantity at such a point is
    // NaN and the running-maximum residuals swallow NaN whole (see
    // evaluate_kkt's NON-FINITE ITERATES note). A NaN reached by the
    // ITERATION rather than supplied by the caller is a different thing
    // and is reported as kNumericalError below.
    if (!x0.allFinite()) {
        throw std::invalid_argument(
            "SqpDriver::solve: x0 contains a NaN or infinite entry; the start point must be "
            "finite in every coordinate");
    }
    // The box is checked at the model boundary, not here. See
    // require_declared_box at the top of this file for the S-1 argument and for
    // why the check moved rather than changed: the box this loop reads is
    // `seam.lower()`/`seam.upper()`, materialized from the bridge's declaration
    // and n-sized by construction, so there is nothing left to compare at this
    // point. The model-taking solve() overloads validate the model's own two
    // returns before a bridge is built over them, and a caller who supplies a
    // bridge directly had its box validated when the bridge laid.

    // ONE strategy per solve() call, so funnel state never leaks between
    // solves and solve() stays repeatable. See SqpOptions::make_strategy.
    std::unique_ptr<GlobalizationStrategy> strategy =
        opts_.make_strategy ? opts_.make_strategy() : std::make_unique<FunnelStrategy>();
    if (strategy == nullptr) {
        throw std::invalid_argument(
            "SqpDriver::solve: SqpOptions::make_strategy returned a null strategy; leave it "
            "empty for the default funnel rather than returning nullptr");
    }

    SqpSolution out;

    // PHASE-7 TASK 5. Read once, here, rather than at each of the three
    // sites below that branch on it -- the mode cannot change during a
    // solve, and one named bool keeps "is this an SSN solve" from being
    // three independently maintained expressions.
    const bool ssn_mode = opts_.qp_mode == QpMode::kSsn;
    // The proximal EXPORT accumulator, cleared per solve so nothing leaks
    // from a previous solve() call on this same driver. See the members.
    ssn_prox_sigma_out_ = 0.0;
    ssn_prox_center_sigma_out_ = 0.0;
    ssn_prox_center_x_out_ = Vec();
    ssn_prox_center_lambda_out_ = Vec();

    // --- WARM-START INGEST (Phase-4 Task 3/4; Phase-6 Task 5) ----------
    //
    // Resolves the level `warm` actually earns and, on a kSeeded-or-above
    // resolution, computes the ingested x/duals/seed this solve starts
    // from. THE RULE:
    //
    //   warm.valid == false                -> kCold (nothing trusted).
    //   valid, DIMENSIONS INCOMPATIBLE      -> kCold. x against n,
    //                                         lambda_e/lambda_i against
    //                                         me()/mi(), and
    //                                         qp_working_set's own (n, mi)
    //                                         shape. Nothing in the object
    //                                         can be read at all, so there
    //                                         is nothing to seed with.
    //   valid, sized, NON-FINITE            -> kCold. Any NaN/Inf in the
    //                                         three ingested vectors. This
    //                                         is the caller-input rule x0
    //                                         itself is held to at the top
    //                                         of this function, applied to
    //                                         the other route the same
    //                                         values can arrive by -- with
    //                                         a DEGRADATION rather than a
    //                                         throw, because a `warm`
    //                                         object is documented as safe
    //                                         to feed back even when stale
    //                                         (warm_start.h).
    //   valid, sized, finite, HASH USELESS  -> kSeeded (PHASE-6 TASK 5;
    //     (0 sentinel, or a mismatch)         WAS kCold). "Trusts values,
    //                                         not provenance" --
    //                                         warm_start.h's StartLevel
    //                                         note has the exact take/
    //                                         refuse list, and THE SEEDED
    //                                         LEVEL below has this
    //                                         function's half of it. The
    //                                         0 sentinel reaches here from
    //                                         mesh_transfer.h and
    //                                         from_interior_point, which
    //                                         have no model of THIS
    //                                         solve's to hash and say so
    //                                         by contract; no VALID exit
    //                                         of THIS driver emits one
    //                                         (Phase-5 Task 0 --
    //                                         make_warm_start probes the
    //                                         model when no subproblem was
    //                                         built), and the driver's one
    //                                         remaining 0, the unevaluable
    //                                         start point, is caught by
    //                                         the `warm.valid` line above
    //                                         and never gets here.
    //                                         STRUCTURE-SAFE DEGRADATION
    //                                         IS UNCHANGED IN THE ONE
    //                                         SENSE THAT MATTERS: a stale
    //                                         or foreign `warm` still
    //                                         never throws, and still
    //                                         never authorizes reuse of a
    //                                         factorization built on
    //                                         another matrix -- it now
    //                                         contributes its values
    //                                         instead of nothing.
    //   valid, hash match, warm.hot == null  -> kWarm.
    //   valid, hash match, warm.hot != null  -> kHot, TENTATIVELY (Task
    //                                         4 -- see the note below
    //                                         this block).
    //   SqpOptions::start_level then CAPS the result (that struct's own
    //   note) -- it can only push the level DOWN, never up.
    //
    // THE SEEDED LEVEL, in this function's own terms. `warm_ingest`
    // (x/duals/activity hint) is true from kSeeded up; `warm_state_ingest`
    // (the trust-region radius, the funnel-width re-base, the
    // Kungurtsev-Diehl full-step window) is true only from kWarm up; the
    // hot handle is offered only at kHot. Those three predicates are the
    // whole of the level's behavioural difference, and each refusal has
    // the same one-line justification: a seeded object cannot justify a
    // quantity measured in another problem's units, or a factorization of
    // another problem's matrix, because it cannot say which problem it
    // came from. Its VALUES, by contrast, are exactly as good as whoever
    // produced them -- and the two producers this level exists for
    // (mesh_transfer.h, warm_start.h's from_interior_point) produce good
    // ones, which is what their own notes have said since Phase 4 while
    // having no way to deliver them.
    //
    // THE HASH IS CHECKED AGAINST A PROBE, NOT AGAINST warm.x ITSELF.
    // Building the REAL first subproblem to learn its structural_hash
    // would mean already having decided x0 -- the very thing the hash
    // is deciding -- so the probe is built at the CALLER'S OWN x0
    // (already validated finite and n-sized above) with zero duals,
    // never at warm.x. This is sound because detail::structural_hash
    // (qp_engine.h) hashes H/Ae/Ai's SPARSITY PATTERN ONLY, never
    // values: which entries a Jacobian/Hessian structurally has does
    // not depend on x or on the multiplier values eval_hess is called
    // with, only on the model's own construction -- which is a BINDING
    // PRECONDITION on the model, not an assumption made here (see
    // nlp_model.h's STRUCTURAL PATTERN INVARIANCE note, whose
    // obj_scale/multiplier clause was added on Phase-5 Task 0's review
    // precisely because this probe and make_warm_start's emission probe
    // both rest on it). So the probe's hash
    // equals whatever building at `warm.x` would have hashed, without
    // ever needing warm.x to be the right size for THIS model.
    //
    // THE PROBE IS SKIPPED ENTIRELY (no extra model evaluation at all)
    // unless warm.valid, structure_hash != 0, AND every warm field this
    // ingest would use is already dimensionally consistent with
    // `model` -- x against n, lambda_e/lambda_i against me()/mi(), and
    // qp_working_set's own (n, mi) shape. A dimension mismatch (e.g.
    // Task 3's own StaleWarmIsSafe fixture: a different me()/mi()
    // entirely) is therefore resolved at ZERO extra cost, and this
    // check is additionally the belt-and-braces net against a hash
    // COLLISION on a same-shape-different-structure pair, which the
    // probe's own comparison cannot rule out on dimensions alone.
    //
    // TASK 4's kHot IS TENTATIVE HERE, and corrected after the FIRST
    // subproblem actually runs. This function does not, and per the
    // brief must not, re-derive qp_engine.h's own reuse-eligibility
    // conditions (a)-(e) -- the engine remains the SOLE gate on whether
    // an offered `warm.hot` is actually usable (structural/values
    // hashes, the effective (primal_delta, dual_mu) pair, the seed
    // working set, and -- Fix Round 1's addition -- the shared object's
    // own generation counter). resolved_level is set to kHot here purely
    // on the same evidence kWarm already uses PLUS `warm.hot != nullptr`;
    // the FIRST major's own engine_.solve() call (below) is what
    // actually offers the handle, and out.counters.start_level_used is
    // corrected down to kWarm right there if that call's own
    // `qs.counters.k0_reused` reads false -- i.e. this field always ends
    // up recording what was
    // OBSERVED to happen, never merely what was offered.
    StartLevel resolved_level = StartLevel::kCold;
    // TASK 8 (eval-economics carry): THE CEILING SHORT-CIRCUIT. A NEW
    // conjunct, ADDED here first -- NOT a reordering of any pre-existing
    // check; the ceiling itself has always lived only in the separate,
    // later `if (static_cast<int>(opts_.start_level) < ...)` block below,
    // and still does. This conjunct duplicates that block's condition
    // inline, placed first (cheapest -- a pure read, no model method
    // touched) so the probe never runs when the ceiling was always going
    // to discard its answer: opts_.start_level == kCold means
    // resolved_level is capped straight back to kCold a few lines below
    // NO MATTER WHAT the probe would have found -- SqpOptions::
    // start_level's own note says as much ("force EVERY solve on this
    // driver ... to behave exactly as the 2-arg one does"). Without this
    // conjunct that solve paid the probe's model evaluation for a
    // resolved_level the very next statement was going to discard;
    // docs/notes/2026-07-30-phase-4-close-carries.md named it "one
    // wasted eval per warm-looking call".
    //
    // PHASE-6 TASK 5 SPLIT THAT ONE CONDITION IN TWO, without moving any of
    // its conjuncts. `warm_dims_plausible` is unchanged in MEANING -- the
    // same `valid` + same five size checks -- but the two conjuncts that
    // are about the PROBE rather than about the object (`structure_hash !=
    // 0`, and the ceiling short-circuit) moved down into
    // `probe_is_worth_running` below, because the dimension answer is now
    // needed by the SEEDED gate as well and the seeded gate cares about
    // neither. The ceiling conjunct also GENERALIZED: it is now "the
    // ceiling is below kWarm", so a driver capped at kSeeded skips the
    // probe for exactly the reason a kCold-capped one does -- the probe's
    // only possible product is a level the ceiling is about to discard.
    const bool warm_dims_plausible =
        warm.valid && warm.x.size() == n && warm.lambda_e.size() == seam.me() &&
        warm.lambda_i.size() == seam.mi() && warm.qp_working_set.n() == n &&
        warm.qp_working_set.mi() == seam.mi();
    // FINITENESS GATES EVERY INGEST ROUTE, not just the seeded one (M3 final
    // review, S-3). It used to sit on the kSeeded resolution alone, on the
    // argument that a hash-matching object "came from a solve whose own exits
    // are finite by construction" -- true of every object this library
    // PRODUCES, and not a property of the object a caller HANDS this function.
    // WarmStart is an aggregate with public fields: a hand-assembled or
    // foreign-solver object can carry warm.x(0) = NaN and a structure_hash
    // that matches, resolve kWarm, and be ingested -- which contradicts
    // solver_counters.h's documented contract that a non-finite ingested
    // vector resolves kCold unconditionally. Hoisting the three conjuncts into
    // `ingest_allowed` makes the seeded gate and the probe gate share one
    // answer, which is what that contract says. The three vectors tested are
    // exactly the three that are ingested; `z` is not (this ingest has never
    // read it), and `tr_radius`/`funnel_width` are guarded by their own
    // `>= 0.0` tests, which a NaN fails.
    const bool ingest_allowed = opts_.start_level != StartLevel::kCold && warm_dims_plausible &&
                                warm.x.allFinite() && warm.lambda_e.allFinite() &&
                                warm.lambda_i.allFinite();
    // THE SEEDED GATE (Phase-6 Task 5): dimensions plus FINITENESS, and
    // nothing else -- no hash, no probe, no model evaluation.
    if (ingest_allowed) {
        resolved_level = StartLevel::kSeeded;
    }
    const bool probe_is_worth_running =
        ingest_allowed && warm.structure_hash != 0 &&
        static_cast<int>(opts_.start_level) >= static_cast<int>(StartLevel::kWarm);
    if (probe_is_worth_running) {
        // TASK 8: THE VALUES-ONLY PROBE. structural_hash reads H/Ae/Ai's
        // SPARSITY PATTERN ONLY (this block's own note above), never
        // f/grad/cE/cI's VALUES, so f/cE/cI are fetched through
        // eval_nlp_values (never more expensive than the eval_f/eval_ce/
        // eval_ci the old full eval_nlp call already paid, and cheaper on
        // a model that overrides eval_values) and eval_grad is skipped
        // entirely -- nothing downstream of this block ever reads a
        // gradient. Je/Ji ARE fetched, directly and unconditionally when
        // me()/mi() > 0: they become Ae/Ai, and Ae/Ai's PATTERN is
        // exactly what gets hashed, so those two calls are not the waste
        // this task removes -- only the values were.
        NlpEval probe_ev = seam.eval_nlp_values(x0);
        ++out.counters.evals_values;
        // The Jacobians-alone moment: the two calls this block used to make
        // itself, in one request that names exactly them (see the seam's
        // jacobians_only, which leaves a block the declaration gives no rows
        // alone, precisely as the me()/mi() guards here did).
        seam.jacobians_only(probe_ev, x0);
        const QpProblem probe =
            seam.build_subproblem(probe_ev, x0, Vec::Zero(seam.me()), Vec::Zero(seam.mi()), 1.0);
        if (detail::structural_hash(probe) == warm.structure_hash) {
            resolved_level = warm.hot != nullptr ? StartLevel::kHot : StartLevel::kWarm;
        }
    }
    // A CEILING, never a floor (SqpOptions::start_level's own note).
    if (static_cast<int>(opts_.start_level) < static_cast<int>(resolved_level)) {
        resolved_level = opts_.start_level;
    }
    out.counters.start_level_used = resolved_level;
    // THE TWO INGEST PREDICATES (Phase-6 Task 5). `warm_ingest` keeps its
    // Phase-4 meaning EXACTLY -- "the values were taken" -- and is now true
    // one level lower. `warm_state_ingest` is the new one and carries what
    // `warm_ingest` used to carry alone: the globalization/regularization
    // state a seeded object may not claim. On every pre-Task-5 resolution
    // the two are identical, which is why nothing kWarm or kHot does can
    // move.
    // NOT const: THE SEEDED DUAL CLAMP below can degrade a kSeeded
    // resolution to kCold after the fact, and this predicate must follow.
    bool warm_ingest = resolved_level != StartLevel::kCold;
    const bool warm_state_ingest =
        static_cast<int>(resolved_level) >= static_cast<int>(StartLevel::kWarm);
    out.counters.n_seeded = resolved_level == StartLevel::kSeeded ? 1 : 0;

    Vec x = warm_ingest ? warm.x : x0;
    Vec lambda_e = warm_ingest ? warm.lambda_e : Vec::Zero(seam.me());
    Vec lambda_i = warm_ingest ? warm.lambda_i : Vec::Zero(seam.mi());

    // AN INGEST SEEDS THE ENGINE'S WORKING SET from warm.qp_working_set,
    // through the SAME seed path every other major uses
    // (engine_.solve(qp, seed, overrides), below) -- never a bespoke
    // ingestion. seed.x is ZEROED for exactly the reason the WARM
    // SEEDING note gives for every other seed: the trust region
    // centers on p = 0, not on a remembered primal point. A seeded
    // bound/row that no longer fits the FIRST subproblem's own window
    // is silently dropped by qp_engine.h's WINDOW-CONSISTENCY RULE --
    // this ingest leans on that rule rather than duplicating it.
    //
    // AT kSeeded AN EMPTY HINT IS NO HINT, AND THAT IS A RULING RATHER THAN
    // AN ACCIDENT (Phase-6 Task 5; the crash-basis interaction the brief
    // asked to be decided and documented). warm_start.h's own field notes
    // say an ALL-ZERO ineq_active/bound_active means "no activity was
    // attributed", NOT "nothing is active" -- a solve that built no
    // subproblem has no QpSolution to read activity off, and a mesh
    // transfer of such an object propagates the same emptiness. At kWarm
    // and kHot that distinction has never mattered, because the object's
    // provenance is confirmed and an empty working set is still THAT
    // solve's answer; at kSeeded it does, because the seeded level is
    // precisely where objects with nothing to say about activity arrive.
    // So: a kSeeded object whose working set pins no row and no bound is
    // treated as carrying NO activity hint at all, and the first subproblem
    // is left unseeded -- which in turn re-arms SqpOptions::crash_basis
    // below, since a cold-quality activity guess derived from the real
    // first linearization is strictly better information than an empty
    // one. A kSeeded object that DOES carry a hint suppresses the crash
    // basis exactly as a kWarm one does. THE kCold/kWarm/kHot BEHAVIOUR IS
    // BYTE-IDENTICAL EITHER WAY: at kWarm/kHot `have_seed` is
    // unconditionally true, at kCold unconditionally false, so
    // `!have_seed` below equals the `!warm_ingest` this line used to read.
    QpSolution seed;
    bool have_seed = false;
    if (warm_ingest) {
        const bool hint_is_empty = resolved_level == StartLevel::kSeeded &&
                                   warm.qp_working_set.active_ineq().empty() &&
                                   warm.qp_working_set.num_free() == n;
        if (!hint_is_empty) {
            seed.x = Vec::Zero(n);
            seed.bound_state = warm.qp_working_set.bound_state();
            seed.ineq_active.assign(static_cast<std::size_t>(seam.mi()), false);
            for (Index row : warm.qp_working_set.active_ineq()) {
                seed.ineq_active[static_cast<std::size_t>(row)] = true;
            }
            have_seed = true;
            // PHASE-7 TASK 0: the hint's own size, counted at kSeeded only
            // (SqpCounters::ip_activity_inferred says why, and why the
            // bound half is not folded in). Write-only: nothing below
            // reads it, so this line cannot move a trajectory.
            if (resolved_level == StartLevel::kSeeded) {
                out.counters.ip_activity_inferred =
                    static_cast<Index>(warm.qp_working_set.active_ineq().size());
            }
        }
    }

    // THE CRASH BASIS (Phase-6 Task 4, SqpOptions::crash_basis). Armed
    // here and consumed at the FIRST subproblem below, where `qp` exists
    // and the seed can be read off it without a model evaluation (see
    // crash_basis_seed's own note). Armed ONLY when no working-set seed was
    // ingested -- `!have_seed` is exactly that condition, and through
    // Phase 5 it was spelled `!warm_ingest`, which meant the same thing
    // because every ingest built a seed. Phase-6 Task 5's ONE new case is
    // the hint-less kSeeded object described just above; on every other
    // resolution this is the same arming decision Task 4 shipped. It can
    // still never displace, reorder or interact with an ingested working
    // set (the two are mutually exclusive by construction), nor with the
    // Task-7b B-1 clear or the zero-major hand-off.
    bool crash_pending = opts_.crash_basis && !have_seed;
    QpSolution crash_seed;

    // THE RADIUS, and the two pieces of loop state that go with it: the
    // consecutive-rejection count at the CURRENT iterate (the funnel's
    // kRestore gate reads it -- globalization.h conjunct (e)) and the
    // subproblem itself, which is rebuilt only when the iterate MOVES.
    // kWarm INGEST (Task 3): warm.tr_radius clamped to
    // [opts_.tr_min, opts_.tr_init] -- warm.tr_radius < 0 is
    // warm_start.h's own "never populated" sentinel and is treated as
    // absent (opts_.tr_init stands, exactly as on a cold solve).
    // `warm_state_ingest`, NOT `warm_ingest` (Phase-6 Task 5): a radius is
    // a step scale on a SPECIFIC problem's variables, so a seeded object --
    // which cannot say which problem it came from -- may not set it. Note
    // mesh_transfer.h DOES carry a radius and argues correctly that a
    // radius is mesh-invariant; that argument is about the transfer, and
    // the refusal here is about provenance, so the two do not conflict.
    double delta = opts_.tr_init;
    if (warm_state_ingest && warm.tr_radius >= 0.0) {
        delta = std::clamp(warm.tr_radius, opts_.tr_min, opts_.tr_init);
    }
    // PHASE-7 TASK 5: THE PROXIMAL CARRY, INGESTED. `warm_state_ingest`,
    // NOT `warm_ingest`, and for exactly the reason the radius immediately
    // above uses it: a proximal level is a statement about how hard ONE
    // model's subproblems were, so an object that cannot say which model it
    // came from may not set it. That puts this behind the same hash gate as
    // every other state field -- warm_start.h's `prox_sigma` note states
    // the rule from the other side.
    //
    // SPENT ONCE, at the first SSN subproblem (see its consumption site).
    // It is read even at `qp_mode == QpMode::kWalk` -- reading a double
    // costs nothing and nothing at kWalk ever consumes it -- so this line
    // needs no mode gate to be inert there.
    double ssn_prox_ingested =
        (opts_.ssn_prox_carry && warm_state_ingest && warm.has_prox_center) ? warm.prox_sigma : 0.0;
    // PHASE-7 TASK 5, FIX ROUND 1. THE PROBE BUDGET'S SSN CHARGE.
    //
    // THE DEFECT THIS CLOSES. The probe budget (the 4-argument solve()'s
    // own THE PROBE BUDGET note, clause 5) is denominated in
    // `qp_minor_iters`, and an SSN-solved subproblem contributes ZERO to
    // that counter by design (ssn_result_to_qp_solution's counter-mapping
    // note -- the currency every published figure in this repository is
    // quoted in must not be corrupted, and that separation stands
    // unchanged). The consequence was that under kSsn the budget could not
    // trip at all: a failed warm probe could spend up to `max_iter`
    // subproblems times the SSN's own hard budget of 25 factorizations each
    // and still report 0 against its budget, silently voiding Phase-6
    // Task 1's failed-proposal detector -- the mechanism Phase 6 named its
    // own priority #1 -- in the one mode Phase 7 exists to measure.
    //
    // THE CURRENCY IS FACTORIZATIONS, which is this phase's own asserted
    // cost unit and the ONE quantity both kernels agree on --
    // ssn_result_to_qp_solution carries it across for exactly that reason.
    // What is charged is every factorization a kSsn subproblem paid that
    // the walk currency cannot see: the SSN kernel's own, plus the tier-3
    // refinement's. The walk's share of a HANDED-OFF subproblem is NOT
    // charged here -- it already lands in `qp_minor_iters`, and charging it
    // twice would make a hand-off cost more than the same subproblem solved
    // by the walk from the start.
    //
    // ONE MINOR IS NOT ONE FACTORIZATION and this does not claim it is. The
    // budget is a STOPPING RULE on a probe, not a published figure, so it
    // needs a currency that grows with work rather than one commensurable
    // across kernels. `SqpCounters::qp_minor_iters` is untouched by this
    // accumulator: it is a separate local, added only inside the budget
    // test.
    //
    // IDENTICALLY ZERO AT kWalk -- written only inside the `ssn_mode`
    // branch of the dispatch -- so the budget test below reduces to the
    // expression it has had since Phase-6 Task 1.
    Index ssn_budget_charge = 0;
    Index rejections_at_iterate = 0;
    // CONSECUTIVE failed subproblems, reset by any solve that reaches
    // kOptimal. Caps the shrink-retry of SUBPROBLEM FAILURE ROUTING at one
    // attempt per failure, so a subproblem that fails the same way at the
    // shrunken radius propagates instead of halving forever.
    Index qp_failures_in_a_row = 0;
    QpProblem qp;
    bool subproblem_is_stale = true;
    // ONE RESTORATION PER SOLVE (this header's RESTORATION PHASE note).
    bool restoration_used = false;
    // Written by the restoration closure below on the exits it decides,
    // read only by the three `return finish(...)` sites that follow it.
    SqpStatus restoration_exit_status = SqpStatus::kInfeasible;
    SqpKkt restoration_exit_kkt;
    double restoration_exit_f = std::numeric_limits<double>::quiet_NaN();

    // WARM-START POPULATION (Phase-4, Task 2). Three pieces of state
    // `make_warm_start` (below) reads at every exit, none of which
    // otherwise has a home in this loop:
    //   qp_built        true once `qp` holds a REAL subproblem (false
    //                   only before the very first one is ever built --
    //                   an immediate convergence at iter 0, a zero
    //                   budget, or the non-finite-x0 exit). On the FIRST
    //                   TWO of those, make_warm_start hashes a PROBE of
    //                   the model instead of `qp`; see its own THE
    //                   ZERO-MAJOR PROBE note for why the hash is
    //                   computable there and what it costs. THE THIRD IS
    //                   THE EXCEPTION: the non-finite-x0 exit passes a
    //                   null probe and emits a COLD object (valid =
    //                   false, hash 0) -- it stands at a point the model
    //                   could not evaluate, which is not a hand-off any
    //                   later solve may be fed. See THE UNEVALUABLE EXIT,
    //                   in the same note.
    //   last_dual_mu    the EFFECTIVE dual_mu the most recently attempted
    //                   subproblem was solved with (row.mu's own value,
    //                   mirrored out here so every exit -- not only the
    //                   row that computed it -- can read it). Starts at
    //                   the engine's own default, which is exactly what
    //                   it would resolve to if no subproblem ever runs.
    //   restoration_moved_x  true iff the restoration closure below
    //                   actually moved `x` to a RESTORED point (the
    //                   "not feasible enough" outcome). On every other
    //                   restoration outcome x is untouched, so the
    //                   CURRENT trial's own QP solution still describes
    //                   the region around the x a restoration-related
    //                   exit is about to report; on this one outcome it
    //                   does not, and make_warm_start is told to fall
    //                   back to "no activity known" rather than attribute
    //                   a stale working set to a point it does not
    //                   describe. Reset at the top of every
    //                   enter_restoration() call, since one solve may
    //                   call it more than once (a resumed restoration
    //                   followed by a second request).
    bool qp_built = false;
    double last_dual_mu = opts_.qp.dual_mu;
    bool restoration_moved_x = false;

    // FULL-STEP-FIRST (Phase-4 Task 5). See this header's FULL-STEP-FIRST
    // WARM RULE note for every decision below; only the state is here.
    //
    // NON-NULL IFF THE MODE IS ARMED, and it is the SAME object
    // `strategy` owns -- the loop's mirror of the strategy's own mode
    // flag. A POINTER rather than a bool because the watchdog needs the
    // funnel's WIDTH to clamp its re-base (fix round 1's F3), and because
    // one piece of state that answers both "is the mode on" and "on
    // which funnel" cannot disagree with itself. Cleared exactly where
    // the strategy's own flag is (the watchdog exit and a restoration
    // resume, both via resume_from_restoration).
    FunnelStrategy *full_step_funnel = nullptr;
    // The best iterate seen under the mode, by ||KKT||inf -- the watchdog
    // restores exactly this. `fs_best_ev` is a COPY of the NlpEval already
    // in hand, so a restore costs no model evaluation.
    Vec fs_best_x, fs_best_lambda_e, fs_best_lambda_i;
    NlpEval fs_best_ev;
    double fs_best_residual = std::numeric_limits<double>::infinity();
    // W1: the best iterate's own `duals_ingested` reading, so that a
    // restore puts the complementarity gate back exactly as it puts the
    // multipliers back. Without it a solve refused at iter 0 could certify
    // the identical (x, lambda) at iter k.
    bool fs_best_duals_ingested = false;
    // Consecutive majors whose residual GREW, and majors since the last
    // new best -- the two watchdog signals. fs_prev_residual is the
    // previous major's reading, +inf before the first (so the first can
    // never register as a growth).
    Index fs_growth_in_a_row = 0;
    Index fs_majors_since_best = 0;
    double fs_prev_residual = std::numeric_limits<double>::infinity();

    // PHASE-4 TASK 6. BUDGETED MODE's own best-iterate tracking -- see
    // sqp_types.h's SqpOptions::budget_mode note for the ordering
    // (feasibility-first: min h, tie-break min f) and why it is a
    // DIFFERENT ranking from fs_best_* just above (that one answers
    // "closest to a KKT point"; this one answers "best to hand a
    // continuation driver"). Only maintained when opts_.budget_mode is
    // set -- a budget_mode=false solve touches none of this and stays
    // byte-identical to Phase 3. `mb_best_kkt` is a COPY of the `kkt`
    // already computed this pass, so tracking costs no extra model or
    // KKT evaluation.
    Vec mb_best_x, mb_best_lambda_e, mb_best_lambda_i;
    SqpKkt mb_best_kkt;
    double mb_best_h = std::numeric_limits<double>::infinity();
    double mb_best_f = std::numeric_limits<double>::infinity();

    // ONE model evaluation per ITERATE, shared by the convergence test and
    // the subproblem (see NlpEval). After the first, every iterate's
    // evaluation is the TRIAL evaluation of the step that reached it.
    //
    // FULL, NOT VALUES-ONLY (Task 8): this `ev` feeds the B-1 gate's
    // stationarity computation below (which needs Je/Ji), the first
    // convergence test (same), and the first subproblem (H/Je/Ji) --
    // none of Task 8's three call sites, per this phase's brief.
    NlpEval ev = seam.eval_nlp(x, lambda_e, lambda_i);
    ++out.counters.evals_full;

    // B-1 REPAIR (Phase-5 Task 7b). Clear every ingested lambda_i whose
    // row is not GEOMETRICALLY ACTIVE at the ingested x, so that the
    // convergence test below -- which runs before this solve has any
    // subproblem of its own -- reads a set of multipliers that is
    // complementary by construction. See this header's THE INGESTED
    // MULTIPLIERS ARE MADE COMPLEMENTARY note for the defect, the choice
    // among the repair menu's four options, and the exact blast radius.
    //
    // The activity test is `cI_j(x) >= -feas_tol`, the same distance test
    // (and the same tolerance) evaluate_kkt already applies to bounds. A
    // NaN row compares false and is therefore NOT cleared, which leaves it
    // to the non-finite exit at the top of the loop rather than quietly
    // dropping it -- the same discipline constraint_violation_l1 states
    // for its own max().
    if (warm_ingest) {
        for (Index j = 0; j < seam.mi(); ++j) {
            if (ev.ci(j) < -opts_.feas_tol) {
                lambda_i(j) = 0.0;
            }
        }
    }

    // THE SEEDED DUAL CLAMP (Phase-6 Task 5) -- SECOND, BY CONTRACT. See
    // this header's THE SEEDED DUAL CLAMP note above for the value's
    // derivation and for why this order (B-1 clear FIRST, sign enforcement
    // SECOND) is normative rather than incidental. Scoped to kSeeded: every
    // route to kWarm/kHot is hash-gated and its producers are non-negative
    // or bounded by 1e-9 relative, so extending it there would move pinned
    // trajectories with no reachable defect to show for it.
    //
    // A NaN lambda_i(j) cannot reach here -- the seeded gate above rejects
    // a non-finite lambda_i outright -- so the two comparisons below
    // partition every value this loop can see.
    if (resolved_level == StartLevel::kSeeded) {
        bool degrade_to_cold = false;
        for (Index j = 0; j < seam.mi(); ++j) {
            if (lambda_i(j) >= 0.0) {
                continue;
            }
            if (lambda_i(j) >= -kSeededDualClampTol) {
                lambda_i(j) = 0.0;
                ++out.counters.seeded_clamped;
            } else {
                degrade_to_cold = true;
                break;
            }
        }
        if (degrade_to_cold) {
            // THE DEGRADATION UNWINDS THE INGEST COMPLETELY, so that what
            // runs from here is a genuine cold solve of `x0` and not a
            // half-ingested hybrid. It costs ONE EXTRA FULL MODEL
            // EVALUATION (the `ev` computed above was taken at `warm.x`,
            // which this solve is no longer standing on) -- that is the
            // price of the contractual ordering, since the B-1 clear that
            // must run first needs cI at the ingested point, and it is paid
            // only on an object already established to be malformed.
            //
            // `delta` needs no unwinding: a seeded ingest never set it
            // (`warm_state_ingest` was false), so it is still opts_.tr_init.
            // Nor do the funnel/full-step blocks, which have not run yet and
            // read `warm_state_ingest` rather than `warm_ingest`.
            resolved_level = StartLevel::kCold;
            out.counters.start_level_used = StartLevel::kCold;
            out.counters.n_seeded = 0;
            out.counters.seeded_clamped = 0;
            out.counters.ip_activity_inferred = 0;
            warm_ingest = false;
            x = x0;
            lambda_e = Vec::Zero(seam.me());
            lambda_i = Vec::Zero(seam.mi());
            have_seed = false;
            crash_pending = opts_.crash_basis;
            ev = seam.eval_nlp(x, lambda_e, lambda_i);
            ++out.counters.evals_full;
        }
    }

    bool funnel_started = false;

    // W1 (Phase-6 final fix wave). TRUE while lambda_e/lambda_i are still
    // the multipliers this solve INGESTED -- i.e. while no subproblem or
    // restoration of THIS problem has re-priced them. It arms the
    // convergence test's complementarity conjunct and nothing else. See
    // this header's THE INGESTED CERTIFICATE IS GATED ON COMPLEMENTARITY
    // note for the tolerance derivation and for why this scope, rather than
    // "the first test" or "every major", is the defensible one.
    //
    // Declared AFTER the seeded clamp so a degrade-to-kCold (which has
    // already zeroed the duals and unwound the ingest) leaves it false.
    bool duals_ingested = warm_ingest;

    for (Index iter = 0;; ++iter) {
        // NOT const FROM TASK 5: the full-step watchdog below may restore
        // an EARLIER iterate before this pass records or tests anything,
        // and the row and the convergence test must then describe the
        // point the driver is actually standing on. Re-measuring is free
        // -- this overload of evaluate_kkt reads an NlpEval already in
        // hand and calls the model not at all.
        SqpKkt kkt = evaluate_kkt(seam, ev, x, lambda_e, lambda_i, opts_.feas_tol);

        SqpIterate row;
        row.trial = iter;
        // The fields describing THE ITERATE (as opposed to the subproblem
        // solved from it), factored out so the watchdog can re-take them
        // after a restore. Reads ev/kkt/delta, all by reference.
        auto measure_iterate = [&] {
            row.f = ev.f;
            row.stationarity = kkt.stationarity;
            row.feasibility = kkt.feasibility;
            row.complementarity = kkt.complementarity;
            row.kkt_residual = kkt.residual();
            row.violation_l1 = constraint_violation_l1(ev);
            row.tr_radius = delta;
        };
        measure_iterate();

        // NON-FINITE ITERATE. The model cannot be evaluated at x (or
        // returned NaN there); nothing at this point has been measured, so
        // it can be neither certified nor stepped from. Reported as
        // kNumericalError with the multipliers CLEARED -- unlike the
        // QP-failure exit below, which retains them, because there the
        // last successful subproblem's prices are still meaningful
        // evidence and here nothing is.
        //
        // FROM TASK 6 ON THIS IS A START-POINT CONDITION, not something
        // the iteration can walk into: a trial point at which the model is
        // non-finite produces a non-finite StepContext, which
        // globalization.h rejects up front, so the driver shrinks the
        // radius and stays where it is instead of moving onto the NaN.
        // Only an x0 the model cannot evaluate (x0 itself is validated
        // finite by the caller check above) reaches here. That is a strict
        // improvement over Task 4, where the same walk ended the solve --
        // see the test of the same name.
        if (!kkt.finite) {
            out.history.push_back(row);
            out.status = SqpStatus::kNumericalError;
            out.x = x;
            out.lambda_e = Vec::Zero(seam.me());
            out.lambda_i = Vec::Zero(seam.mi());
            out.z = Vec::Zero(n);
            out.f = ev.f;
            // No subproblem has ever been built here (this is a
            // start-point condition -- see the caller's own note above),
            // so there is no activity to attribute -- AND NO USABLE
            // HAND-OFF AT ALL. The null `probe_ev`/`probe_x` is this
            // exit's own signal to make_warm_start (its THE UNEVALUABLE
            // EXIT note): no structure probe is attempted at a point the
            // model has just reported it cannot evaluate, and the object
            // comes back COLD (valid = false, hash 0) so that a caller
            // retrying from a CORRECTED x0 gets its own point honoured
            // instead of being pinned back onto this one. The fields
            // overwritten below are still populated, for a caller that
            // wants to inspect what the failed solve stood on.
            out.warm_start =
                make_warm_start(seam, /*activity=*/nullptr, qp,
                                /*qp_built=*/qp_built, /*probe_ev=*/nullptr,
                                /*probe_x=*/nullptr, delta, last_dual_mu, opts_.qp.primal_delta,
                                strategy.get(), engine_.hot_state());
            out.warm_start.x = out.x;
            out.warm_start.lambda_e = out.lambda_e;
            out.warm_start.lambda_i = out.lambda_i;
            out.warm_start.z = out.z;
            return out;
        }

        // THE FUNNEL IS SEEDED FROM THE FIRST MEASURABLE ITERATE, i.e.
        // here rather than before the loop: reset() rejects a non-finite
        // h0 (globalization.h), and whether h(x0) is finite is exactly
        // what the check above has just decided.
        if (!funnel_started) {
            strategy->reset(row.violation_l1);
            // kWarm INGEST (Task 3): re-base the funnel from the PRIOR
            // solve's own width rather than leaving it at Eq. (9)'s
            // absolute floor, reusing the SAME Eq.-13-style blend
            // resume_from_restoration already implements for the
            // restoration-resume case (globalization.h) -- warm.
            // funnel_width stands in for "the funnel's own remembered
            // width" and this row's own h0 (violation_l1) stands in for
            // "the point being re-entered at", both being simultaneously
            // the FIRST reading this solve ever takes. warm.funnel_width
            // is CLAMPED BELOW by kappa_bar * h0 first -- Eq. (9)'s own
            // floor term, applied to the CURRENT problem's own h0 -- so
            // a stale or optimistic prior width can never leave the
            // funnel tighter than fresh evidence at this x would
            // justify. warm.funnel_width < 0 is warm_start.h's own
            // "never populated" sentinel and is skipped, exactly like an
            // absent qp_working_set is above.
            // `warm_state_ingest` (Phase-6 Task 5): a funnel width is
            // measured in the units of h(x) on the problem it was recorded
            // on -- mesh_transfer.h's section 4 declines to transfer one for
            // exactly that reason -- so a seeded object may not re-base this
            // solve's funnel and the Eq.-(9) seed at its own first h0
            // stands, as on a cold solve.
            if (warm_state_ingest && warm.funnel_width >= 0.0) {
                if (auto *funnel = dynamic_cast<FunnelStrategy *>(strategy.get())) {
                    const double floor = kFunnelKappaBar * row.violation_l1;
                    funnel->resume_from_restoration(std::max(warm.funnel_width, floor));
                }
            }
            // FULL-STEP-FIRST (Task 5): the three engagement conditions,
            // checked exactly here because this is the first point at
            // which the funnel exists (begin_full_step requires it -- its
            // exit re-bases a width) and the first at which the iterate
            // is known measurable. AFTER the width re-basing above, so the
            // mode is armed over the width the ingest chose. See this
            // header's FULL-STEP-FIRST WARM RULE note.
            // `warm_state_ingest` (Phase-6 Task 5): the Kungurtsev-Diehl
            // full-step-first rule's justification is the local convergence
            // of a warm start ON THE SAME PROBLEM, which is precisely the
            // claim a seeded object cannot make, so the window is never
            // armed from kSeeded.
            if (warm_state_ingest && opts_.warm_full_step) {
                if (auto *funnel = dynamic_cast<FunnelStrategy *>(strategy.get())) {
                    funnel->begin_full_step();
                    full_step_funnel = funnel;
                }
            }
            funnel_started = true;
        }

        // THE FULL-STEP WATCHDOG (Task 5). Placed BEFORE the convergence
        // test so that a restored iterate is the one this pass records,
        // tests and steps from -- and placed AFTER the non-finite exit so
        // `kkt` is known measurable (the mode never steps onto a
        // non-finite point: globalization.h's judge() still rejects such a
        // trial while the mode is armed). It runs on the ENGAGING pass
        // too, which is how the warm start point itself becomes the first
        // "best iterate".
        if (full_step_funnel != nullptr) {
            // THE RANKING IS kkt.residual(), i.e. max(stationarity,
            // feasibility) -- COMPLEMENTARITY IS DELIBERATELY NOT IN IT
            // (fix round 1's F4), and the reason is that the watchdog must
            // rank iterates by exactly the quantity the CONVERGENCE TEST
            // gates on. That test is `kkt.stationarity <= kkt_tol &&
            // kkt.feasibility <= feas_tol` (just below), and SqpKkt::
            // residual() is its scalar form; complementarity is MEASURED
            // BUT NOT GATED throughout this driver (see the CONVERGENCE
            // TEST note's WHAT IS MEASURED BUT NOT GATED paragraph, and
            // SqpIterate::complementarity's own "RECORDED, NOT GATED").
            // A watchdog ranking on a third term could prefer an iterate
            // the convergence test rates WORSE, i.e. restore away from the
            // point this solve is closest to certifying -- which is the
            // one thing the restore exists not to do.
            const double residual = kkt.residual();
            if (residual < fs_best_residual) {
                fs_best_residual = residual;
                fs_best_x = x;
                fs_best_lambda_e = lambda_e;
                fs_best_lambda_i = lambda_i;
                fs_best_duals_ingested = duals_ingested; // W1; see its note
                fs_best_ev = ev;                         // a COPY; no model evaluation
                fs_majors_since_best = 0;
                fs_growth_in_a_row = 0;
            } else {
                ++fs_majors_since_best;
                // STRICT growth only: a major that repeats the same
                // residual (the routed-QP-failure shape, where the iterate
                // did not move at all) is a STALL, not a divergence, and
                // is caught by the window instead.
                fs_growth_in_a_row = residual > fs_prev_residual ? fs_growth_in_a_row + 1 : 0;
            }
            fs_prev_residual = residual;

            if (fs_growth_in_a_row >= kWarmResidualGrowthMax ||
                fs_majors_since_best >= kWarmFullStepWindow) {
                // RESTORE THE BEST ITERATE, then hand the solve back to
                // ordinary funnel globalization AT THAT POINT. The order
                // is the restoration resume's order, and for the same
                // reasons -- see that path and the header note.
                x = std::move(fs_best_x);
                lambda_e = std::move(fs_best_lambda_e);
                lambda_i = std::move(fs_best_lambda_i);
                duals_ingested = fs_best_duals_ingested; // W1; see its note
                ev = std::move(fs_best_ev);
                kkt = evaluate_kkt(seam, ev, x, lambda_e, lambda_i, opts_.feas_tol);
                measure_iterate();
                // KLV Eq. (13) re-basing, which ALSO clears the strategy's
                // own mode flag (globalization.h). Both flags fall
                // together; nothing re-enters the mode.
                //
                // CLAMPED FROM ABOVE BY THE CURRENT WIDTH (fix round 1's
                // F3). Eq. (13) gives tau_+ = (1-kappa)h + kappa*tau,
                // which is <= tau IFF h <= tau -- so an UNCLAMPED re-base
                // at a restored point lying outside its own funnel would
                // WIDEN it, and the monotonically non-increasing width is
                // exactly what KLV Thm. 1 case 1 sums. The other two
                // callers of this hook are each protected already and
                // neither protection is available here: the restoration
                // resume only calls it when h_restored <= feas_tol (so
                // h < tau with room to spare), and the Task-3 ingest
                // clamps its argument from BELOW because there the
                // argument is a remembered WIDTH rather than an h. The
                // watchdog's argument is a genuine h at a point chosen by
                // ||KKT||inf ALONE, and nothing about that choice bounds
                // its l1 violation -- residual() is an inf-norm measure,
                // so on a model with many rows an iterate can be the best
                // by residual and still carry h above the width.
                //
                // THE CLAMP LIVES HERE AND NOT IN THE HOOK, deliberately:
                // globalization.h states that Eq. (13) is transcribed
                // UNCONDITIONALLY there so that a caller resuming from
                // outside its own funnel is reported rather than hidden.
                // This is that caller taking responsibility for its own
                // argument.
                //
                // IT DOES BIND, RARELY, AND IS PINNED. A sweep of 3240
                // (problem, radius, dual poison, warm-x) combinations over
                // all 27 shipped HS problems produced 330 watchdog
                // restores, of which 6 -- on 3 distinct fixtures -- had
                // h_best above the width. Rare because after any warm
                // ingest tau >= kFunnelTauBar/2 = 50, so binding needs an
                // iterate that is simultaneously the BEST by inf-norm
                // residual and carrying an l1 violation above 50.
                // tests/test_warm_start.cpp's WatchdogRebaseNeverWidens-
                // TheFunnel is one of the three, measured at h_best = 330
                // against a width of 65.6 (an unclamped re-base there
                // widens to 198).
                strategy->resume_from_restoration(
                    std::min(row.violation_l1, full_step_funnel->width()));
                full_step_funnel = nullptr;
                rejections_at_iterate = 0;
                // The iterate moved (or, on the stall exit, may not have
                // -- either way the subproblem in hand was built at some
                // other point or at some other multipliers, so it is
                // stale).
                subproblem_is_stale = true;
                // THE SEED IS KEPT, unlike the restoration resume's (which
                // drops it because it belongs to a DIFFERENT engine and a
                // different problem). Here it is this engine's own, and it
                // carries an ACTIVE-SET GUESS and nothing else -- seed.x
                // is always zeroed (WARM SEEDING) -- so a guess taken at
                // the abandoned iterate is exactly as admissible as the
                // guess every accepted step passes forward. qp_engine.h's
                // WINDOW-CONSISTENCY RULE drops whatever no longer fits.
                //
                // `delta` IS ALSO KEPT, and this is the second deliberate
                // difference from the restoration resume (which sets it to
                // restoration_restart_radius()). That reset exists because
                // a restoration is ENTERED from a collapsed radius -- the
                // floor route arrives with Delta at tr_min by definition,
                // and that number is evidence about a model the phase is
                // no longer solving. NONE OF THAT APPLIES HERE: under the
                // mode the radius is never shrunk by a rejection (there are
                // none), so what the watchdog is holding is the ingested
                // warm radius, possibly grown -- [KD]'s carried-over
                // globalization state, which is the thing the rule exists
                // to keep rather than re-derive. It is also the radius the
                // BEST iterate's own subproblem was solved at whenever the
                // mode never grew it, which is the common case. The funnel
                // takes over from there and shrinks it on its own evidence,
                // as it does everywhere else.
                ++out.counters.watchdog_restores;
                // PHASE-4 TASK 7 (closing Task 5's F5 carry): label THIS
                // row -- the one `measure_iterate()` just re-took above,
                // so `row` already describes the restored point -- as the
                // one the watchdog rebased. See SqpIterate::
                // watchdog_restored's own note for why the history needs
                // this at all.
                row.watchdog_restored = true;
            }
        }

        // PHASE-4 TASK 6. BUDGETED MODE's best-iterate tracking (see the
        // declaration note above for the ordering and sqp_types.h's
        // SqpOptions::budget_mode for the full contract). Runs on EVERY
        // measured pass, AFTER the full-step watchdog above -- so if this
        // pass just restored an earlier point, the candidate considered
        // here is that restored point, exactly the one `row` now
        // describes and the one about to be pushed to history below,
        // never the abandoned pre-restore point (which leaves no row of
        // its own on that path -- see the watchdog block's own note).
        if (opts_.budget_mode) {
            const bool better = row.violation_l1 < mb_best_h ||
                                (row.violation_l1 == mb_best_h && row.f < mb_best_f);
            if (better) {
                mb_best_h = row.violation_l1;
                mb_best_f = row.f;
                mb_best_x = x;
                mb_best_lambda_e = lambda_e;
                mb_best_lambda_i = lambda_i;
                mb_best_kkt = kkt;
            }
        }

        // THE BUDGET IS SHARED WITH THE RESTORATION PHASE (sqp_types.h's
        // SqpOptions note): max_iter bounds subproblems solved across
        // BOTH, so majors already spent restoring are not available here.
        //
        // THE THIRD CONJUNCT (W1) IS ARMED ONLY WHILE THE MULTIPLIERS ARE
        // THE INGESTED ONES. Once a subproblem or a restoration of this
        // problem has re-priced them, `duals_ingested` is false and the
        // test is character-for-character the Phase-3 one -- which is why
        // every cold trajectory in the corpus is untouched. The tolerance
        // is kkt_tol, absolute; see this header's THE INGESTED CERTIFICATE
        // IS GATED ON COMPLEMENTARITY note for that derivation, for why no
        // ||lambda_i||-relative form can do the job, and for the scope.
        const bool converged = kkt.stationarity <= opts_.kkt_tol &&
                               kkt.feasibility <= opts_.feas_tol &&
                               (!duals_ingested || kkt.complementarity <= opts_.kkt_tol);
        // PHASE-6 TASK 1. THE CALLER'S PROBE BUDGET (the 4-argument
        // solve()'s own note has the contract). Evaluated HERE, beside
        // the max_iter test and after `converged` is already known, which
        // is what gives the budget its two defining properties: it never
        // costs an answer already found (converged wins in the
        // disjunction below), and it never buys another major (this pass
        // has not built a subproblem yet).
        const bool probe_exhausted =
            !converged && minor_budget > 0 &&
            out.counters.qp_minor_iters + ssn_budget_charge >= minor_budget;
        if (converged || probe_exhausted ||
            iter + out.counters.restoration_iters >= opts_.max_iter) {
            out.history.push_back(row);
            if (probe_exhausted) {
                // The one field that tells this exit apart from an
                // ordinary max_iter one. Set BEFORE finish() so it
                // reaches the returned counters and the ledger record.
                out.counters.probe_budget_stops = 1;
            }
            // `!probe_exhausted` (Phase-6 Task 1): a probe-budget stop
            // takes the ORDINARY kMaxIter exit below even under budgeted
            // mode, because the two budgets promise opposite things --
            // see the 4-argument solve()'s note, part 4.
            if (!converged && !probe_exhausted && opts_.budget_mode) {
                // BUDGETED MODE (Task 6): report the best-by-(h, f)
                // iterate rather than the last one. `seed` is only this
                // best point's own activity when the best iterate IS the
                // current one (row.violation_l1/row.f just tied the
                // tracked best on THIS pass) -- otherwise it describes a
                // point the returned x has moved away from, exactly the
                // restoration_moved_x reasoning elsewhere in this loop.
                const bool best_is_current = row.violation_l1 == mb_best_h && row.f == mb_best_f;
                return finish(
                    std::move(out), SqpStatus::kBudgetExhausted, mb_best_x, mb_best_lambda_e,
                    mb_best_lambda_i, mb_best_kkt, mb_best_f,
                    make_warm_start(seam, (best_is_current && have_seed) ? &seed : nullptr, qp,
                                    qp_built, &ev, &x, delta, last_dual_mu, opts_.qp.primal_delta,
                                    strategy.get(), engine_.hot_state()));
            }
            // No FRESH subproblem was solved at this exact iterate this
            // pass (the check runs before build_subproblem); `seed` (if
            // any) is the most recent QP solved AT this same x -- see
            // the WARM SEEDING note for why it always still describes
            // this x rather than some earlier one.
            return finish(std::move(out), converged ? SqpStatus::kOptimal : SqpStatus::kMaxIter, x,
                          lambda_e, lambda_i, kkt, row.f,
                          make_warm_start(seam, have_seed ? &seed : nullptr, qp, qp_built, &ev, &x,
                                          delta, last_dual_mu, opts_.qp.primal_delta,
                                          strategy.get(), engine_.hot_state()));
        }

        // THE RESTORATION PHASE (Task 9). See this header's RESTORATION
        // PHASE note for every design choice below; only the mechanics are
        // here. Defined at this point in the loop because it reads `kkt`
        // and `ev` (the requesting iterate's own measurement, used on the
        // exits where the phase does not run at all), and called from the
        // THREE request sites further down -- the funnel's kRestore
        // verdict, the elastic tier's exhaustion, and the radius floor.
        //
        // Returns true iff the main loop is to RESUME, in which case every
        // piece of loop state it touches (x, ev, the multipliers, the
        // radius, the counts and the seed) has already been updated and
        // the caller's only job is `continue`. On false it has written
        // restoration_exit_{status,kkt,f} and, where the phase actually
        // ran, x/lambda_e/lambda_i, for the caller's `return finish(...)`.
        auto enter_restoration = [&]() -> bool {
            // Reset every call: a solve may enter restoration more than
            // once (a resumed restoration followed by a second request),
            // and this flag must describe only THIS call's outcome. See
            // its declaration above for what it is for.
            restoration_moved_x = false;

            // Majors already spent, on both phases, at this point.
            const Index spent = iter + 1 + out.counters.restoration_iters;

            if (!allow_restoration_ || restoration_used) {
                // The nested case (a restoration solve raising its own
                // request) and the once-per-solve cap take the SAME exit,
                // for the same reason: there is a restoration request and
                // no admissible way to service it. counters.
                // restoration_iters tells the two apart (0 for the nested
                // case, nonzero for the cap).
                restoration_exit_status = SqpStatus::kInfeasible;
                restoration_exit_kkt = kkt;
                restoration_exit_f = ev.f;
                return false;
            }
            if (spent >= opts_.max_iter) {
                // No budget left to restore with. This is a BUDGET
                // outcome, not an infeasibility one, and is reported as
                // such rather than borrowing the verdict the phase never
                // got to render.
                restoration_exit_status = SqpStatus::kMaxIter;
                restoration_exit_kkt = kkt;
                restoration_exit_f = ev.f;
                return false;
            }
            restoration_used = true;

            // THE ONE MODEL IDENTITY READ ON THIS PATH, and it is not an
            // evaluation. RestorationModel is a Level 1 WRAPPER in the
            // variables (x, sp, sm, si) built AROUND the problem being solved
            // -- a different NlpModel, with no aggregate form of its own -- so
            // it is constructed from the model behind the bridge, taken from
            // the caller's own handle rather than back out of the seam -- the
            // seam binds the claim-stream interface, which carries no model.
            // The nested sub-solve below then goes through the model-taking
            // solve() overload, which builds the wrapper its OWN bridge and
            // seam for the duration of that call, exactly as this solve's entry
            // point did. `feasibility` outlives that call: it is this scope's
            // local and the sub-solve returns before the scope ends.
            const RestorationModel feasibility(bridge.model(), x, ev);
            SqpOptions ropts = opts_;
            // The caller's strategy factory is NOT carried; the radius is.
            // See the header note's WHAT IS CARRIED IN.
            ropts.make_strategy = {};
            // PHASE-4 TASK 6. BUDGETED MODE NEVER PROPAGATES TO THE
            // RESTORATION SUB-SOLVE, even when the caller's own solve has
            // it on: sqp_types.h's SqpOptions::budget_mode SCOPE note
            // says the lever governs only the MAIN loop's own max_iter
            // exhaustion, and the sub-solve exhausting its OWN slice of
            // the shared budget is deliberately still reported as
            // kMaxIter just below (the "no budget left to restore with"
            // exit, when spent >= opts_.max_iter, is the same answer).
            // Forcing this off is what keeps rs.status below able to stay
            // exhaustive without a reachable kBudgetExhausted arm.
            ropts.budget_mode = false;
            // PHASE-7 TASK 5, FIX ROUND 1. THE RESTORATION SUB-SOLVE RUNS
            // THE WALK, WHATEVER THE CALLER'S MODE IS. Until this line
            // existed the copy above carried `qp_mode` in with everything
            // else, so a kSsn caller silently ran the semismooth kernel
            // through the whole restoration phase -- flatly contradicting
            // this header's own WHAT IS NOT ROUTED THROUGH SSN note, which
            // says the restoration sub-solve stays on the walk
            // UNCONDITIONALLY, and doing it on a wrapper model (original
            // plus slack variables, a subgradient-selector objective) that
            // no fixture in this repository has ever run that kernel
            // against. The note was the design; this is the enforcement.
            //
            // MEASURED, not merely asserted: `rs.counters.ssn` is folded
            // below, so deleting this line MOVES a pinned number
            // (tests/test_sqp_driver.cpp's
            // RestorationStaysOnTheWalkUnderKSsn -- 131 SSN iterations and
            // 234 factorizations with the reset, 140 and 247 without it;
            // post-escaped-factorization-accumulation figures, matching the
            // test's pins).
            ropts.qp_mode = QpMode::kWalk;
            // THE RADIUS IS CARRIED, BUT NOT BELOW THE RESTART VALUE.
            // Carrying it is the brief's rule and the right default -- it
            // is what the driver currently trusts the model over. But when
            // the request came FROM the floor, the radius that arrives
            // here is by definition collapsed, and it is collapsed as
            // evidence about the OPTIMALITY model, which is not the model
            // about to be solved. Starting the feasibility problem there
            // costs one doubling per order of magnitude before it can move
            // at all: MEASURED on this file's own circle/line fixture in
            // refactorize mode, restoration entered at Delta = 1.16e-10
            // spent 35 majors where the same phase entered at Delta = 1
            // spends 4. The floor is the same one the resumed loop
            // restarts at, so the two rules agree.
            ropts.tr_init =
                std::isfinite(delta) ? std::max(delta, restoration_restart_radius()) : opts_.tr_max;
            ropts.tr_max = std::max(opts_.tr_max, ropts.tr_init);
            ropts.max_iter = opts_.max_iter - spent;
            SqpDriver sub(ropts, /*allow_restoration=*/false);
            const SqpSolution rs = sub.solve(feasibility, feasibility.start_point());

            // EVERY WORK counter the sub-solve moved is folded in: the
            // work was spent (the SOC/elastic convention, ported). Its
            // majors are the ONE work quantity that gets its own field
            // rather than joining major_iters -- see SqpCounters.
            //
            // steps_accepted AND rejected_steps ARE DELIBERATELY NOT
            // FOLDED, and that is not an omission: they do not measure
            // work, they describe WHAT HAPPENED TO THE CALLER'S ITERATE
            // (how many times it moved, how many times the radius shrank
            // under it). The sub-solve's acceptances move a point in the
            // WRAPPER's variables, on a different objective; adding them
            // would make steps_accepted stop being "the number of times
            // x moved" -- which is exactly what SqpCounters promises and
            // what the major_iters identity there is written against.
            out.counters.restoration_iters += rs.counters.major_iters;
            out.counters.qp_minor_iters += rs.counters.qp_minor_iters;
            out.counters.factorizations += rs.counters.factorizations;
            out.counters.eqp_refine_steps += rs.counters.eqp_refine_steps;
            out.counters.border_refine_steps += rs.counters.border_refine_steps;
            out.counters.suspect_escalations += rs.counters.suspect_escalations;
            out.counters.symbolic_analyses += rs.counters.symbolic_analyses;
            out.counters.soc_steps += rs.counters.soc_steps;
            out.counters.soc_applied += rs.counters.soc_applied;
            out.counters.soc_qp_infeasible += rs.counters.soc_qp_infeasible;
            out.counters.soc_rejected += rs.counters.soc_rejected;
            out.counters.elastic_activations += rs.counters.elastic_activations;
            out.counters.elastic_escalations += rs.counters.elastic_escalations;
            // TASK 8's two counters ARE folded, exactly like every other
            // WORK counter above (qp_minor_iters, factorizations, ...):
            // the sub-solve's own solve_impl runs through the SAME three
            // call sites this task changed, so its values-only/full
            // split is real work spent evaluating `feasibility`, not a
            // property of the WRAPPER's variables the way steps_accepted/
            // rejected_steps are (see the note just below for that
            // distinction).
            out.counters.evals_full += rs.counters.evals_full;
            out.counters.evals_values += rs.counters.evals_values;
            // PHASE-6 TASK 4's two counters ARE folded, for the same
            // reason Task 8's are and NOT for the reason Task 5's are
            // omitted below. The sub-solve inherits `crash_basis` from
            // `opts_` like every other lever in ropts, it is COLD by
            // construction (a fresh 2-arg solve()), and its own first
            // subproblem therefore gets a crash basis of its own -- real
            // seeding work, on the feasibility wrapper's geometry, that
            // this solve caused. It is NOT identically zero and folding a
            // live quantity is the only honest option: the restoration
            // wrapper's slack columns start AT their own lower bound of
            // 0, so a restoration entered with the lever on seeds every
            // one of them.
            out.counters.crash_seeded_rows += rs.counters.crash_seeded_rows;
            out.counters.crash_seeded_bounds += rs.counters.crash_seeded_bounds;
            // PHASE-7 TASK 5's SSN counters ARE folded, and for the exact
            // OPPOSITE of the "identically zero, so folding it would only
            // suggest it might not be" argument the next paragraph makes
            // about Phase-4 Task 5's pair. They are identically zero too --
            // `ropts.qp_mode` is reset to kWalk above, so the sub-solve has
            // no SSN kernel to run -- and folding them is what makes that
            // reset OBSERVABLE FROM OUTSIDE. With the fold in place a kSsn
            // solve that entered restoration reports its main loop's SSN
            // work and nothing else, and any future change that let the
            // mode leak back in shows up here as a number instead of as
            // silence. The fold also keeps the aggregate honest against the
            // factorization fold above, which does NOT discriminate by
            // kernel: without it, restoration-nested SSN work would
            // contribute factorizations that `ssn_iters` denied, breaking
            // sqp_types.h's `ssn_iters <= factorizations` in the direction
            // that hides work.
            //
            // DELETING THIS LINE IS AN EQUIVALENT MUTANT, AND SAYING SO IS
            // NOT A HOLE IN THE TESTING -- it is the same statement as
            // "the reset above works", read from the other side. Because
            // `ropts.qp_mode` is kWalk, `rs.counters.ssn` is all-zero, so
            // removing this fold changes no observable of the SHIPPED
            // code. What it removes is a MEASUREMENT: it is only with this
            // fold present that letting the mode leak back in moves
            // `ssn_iters` (131 -> 140). The two mutations are therefore
            // not independent, and the one that matters -- deleting the
            // reset -- dies on this fixture with or without this line,
            // since the folded FACTORIZATIONS move too (234 -> 247).
            accumulate_ssn_counters(out.counters.ssn, rs.counters.ssn);
            // TASK 5's two counters are deliberately absent from this
            // fold, and they are absent because they are IDENTICALLY ZERO
            // on the sub-solve: it runs the 2-arg solve() on a fresh
            // driver, which is cold by construction, and the full-step
            // mode never engages on a cold solve. Folding a guaranteed
            // zero would only suggest it might not be one.

            const Vec x_r = feasibility.original_x(rs.x);
            NlpEval ev_r =
                x_r.allFinite() ? seam.eval_nlp(x_r, rs.lambda_e, rs.lambda_i) : NlpEval{};
            if (x_r.allFinite()) {
                ++out.counters.evals_full;
            }
            const SqpKkt kkt_r = x_r.allFinite() ? evaluate_kkt(seam, ev_r, x_r, rs.lambda_e,
                                                                rs.lambda_i, opts_.feas_tol)
                                                 : SqpKkt{};
            if (!x_r.allFinite() || !kkt_r.finite) {
                // The restoration walked somewhere the main model cannot
                // be measured. The iterate is NOT moved there -- the
                // requesting point is returned instead, with the
                // multipliers cleared, exactly as the non-finite-iterate
                // exit above does and for the same reason: nothing was
                // measured, so nothing may be reported as evidence.
                lambda_e.setZero();
                lambda_i.setZero();
                restoration_exit_status = SqpStatus::kNumericalError;
                restoration_exit_kkt = kkt;
                restoration_exit_f = ev.f;
                return false;
            }
            const double h_r = constraint_violation_l1(ev_r);

            if (h_r <= opts_.feas_tol) {
                // RESUME. The order here is the header note's order.
                x = x_r;
                ev = std::move(ev_r);
                // The multipliers in hand price the WRAPPER's constraints
                // (they are subgradient selectors in [-1,1], not NLP
                // prices); the first main-loop QP re-estimates them.
                lambda_e.setZero();
                lambda_i.setZero();
                // W1 (round 2, N4): the ingested duals are gone here too,
                // so the ingest-scoped complementarity gate disarms with
                // them. Behaviourally this line is inert -- complementarity
                // is max_j |lambda_i(j) cI_j| and lambda_i has just been
                // zeroed, so the armed conjunct would pass anyway -- but
                // the flag's declared invariant ("TRUE while lambda are
                // still the multipliers this solve INGESTED") must be TRUE
                // rather than merely harmless, which is the same doc-vs-code
                // discipline W7 exists to enforce.
                duals_ingested = false;
                // KLV Algorithm 2's re-basing, NOT reset() -- see
                // globalization.h::resume_from_restoration. It also CLEARS
                // the full-step mode on the strategy (Task 5), so the
                // driver's own mirror of that flag is cleared with it: a
                // restoration is definitive evidence against the mode's
                // premise, and the solve comes back out fully globalized.
                strategy->resume_from_restoration(h_r);
                full_step_funnel = nullptr;
                delta = restoration_restart_radius();
                rejections_at_iterate = 0;
                qp_failures_in_a_row = 0;
                subproblem_is_stale = true;
                // The outer engine's seed describes an active set at the
                // ABANDONED iterate; the sub-solve ran on its own engine
                // and its working set is a different problem's.
                have_seed = false;
                return true;
            }

            // NOT FEASIBLE ENOUGH: the phase's other outcome. The restored
            // point and the restoration's own multipliers are what is
            // returned -- on the kOptimal arm they are the SUBGRADIENT
            // CERTIFICATE (sqp_types.h's SqpSolution note); on the others
            // they are the best evidence available at that point and not a
            // certificate. The status does not encode which, deliberately:
            // a caller that needs the distinction checks the certificate
            // (four lines -- tests/test_sqp_restoration.cpp does exactly
            // that), and recording it as a field belongs to Task 12's
            // formalization of SqpIterate rather than to a bool bolted on
            // here.
            x = x_r;
            // The point being reported is now the RESTORED point, which
            // no in-loop QpSolution (qs/seed) was ever solved at -- see
            // this flag's own declaration note.
            restoration_moved_x = true;
            lambda_e = rs.lambda_e;
            lambda_i = rs.lambda_i;
            // W1: the ingested duals are gone -- these are the restoration
            // sub-solve's own subgradient selectors -- so the ingest-scoped
            // complementarity gate disarms with them.
            duals_ingested = false;
            restoration_exit_kkt = kkt_r;
            // THE BOUND PRICE MUST COME FROM THE RESTORATION PROBLEM, not
            // from kkt_r. evaluate_kkt reports z = grad L at an active
            // bound, and grad L carries grad f -- which is exactly the
            // term a subgradient certificate of h must NOT contain. The
            // sub-solve's own z is the price of the SAME box in the
            // feasibility problem, whose objective has no x-block, so its
            // x-part is precisely the normal-cone component of
            // Je^T lambda_e + Ji^T lambda_i. Without this the certificate
            // fails on every problem whose infeasible stationary point
            // sits on a bound -- MEASURED: the box-blocked equality
            // fixture reports z = 0 and a residual of 1 where the
            // certificate calls for z = -1 and 0.
            restoration_exit_kkt.z = feasibility.original_x(rs.z);
            restoration_exit_f = ev_r.f;
            switch (rs.status) {
            case SqpStatus::kOptimal:
                // THE CERTIFIED EXIT, AND THE ONLY ONE: the feasibility
                // problem's own KKT test passed (residual <= kkt_tol)
                // while h > feas_tol. Byrd-Curtis-Nocedal rapid
                // detection. This is the single place the certified flag
                // is ever set, and it is set on nothing else -- see
                // sqp_types.h's SqpSolution note for why the status,
                // the counters and the history cannot carry this fact.
                restoration_exit_status = SqpStatus::kInfeasible;
                out.infeasibility_certified = true;
                break;
            case SqpStatus::kInfeasible:
                // The sub-solve raised a restoration request of its own,
                // which nothing can service (nested restoration is not
                // allowed): the feasibility problem is itself stuck. NO
                // CLAIM is made about the model.
                restoration_exit_status = SqpStatus::kInfeasible;
                break;
            case SqpStatus::kMaxIter:
                restoration_exit_status = SqpStatus::kMaxIter;
                break;
            case SqpStatus::kNumericalError:
                restoration_exit_status = SqpStatus::kNumericalError;
                break;
            case SqpStatus::kBudgetExhausted:
                // UNREACHABLE: ropts.budget_mode is forced false just
                // above, so this sub-solve never runs budgeted and
                // rs.status can never be this value -- kept only to
                // preserve the switch's exhaustiveness (the map_status
                // convention, ported: an arm that cannot fire is still
                // enumerated rather than left to a default). Mapped to
                // kMaxIter defensively, exactly what it would have been
                // reported as had budget_mode not been forced off.
                restoration_exit_status = SqpStatus::kMaxIter;
                break;
            }
            return false;
        };

        // REBUILT ONLY WHEN THE ITERATE MOVED. A rejection re-solves this
        // very object -- same H/g bytes, so the engine's hot-start reuse
        // key survives the retry (see WARM SEEDING) and no eval_hess is
        // paid for it.
        if (subproblem_is_stale) {
            qp = seam.build_subproblem(ev, x, lambda_e, lambda_i, 1.0);
            subproblem_is_stale = false;
            qp_built = true;
        }

        SolveOverrides overrides;
        overrides.tr_radius = delta;
        // ADAPTIVE DUAL REGULARIZATION (Task 10, see the header note
        // above). `kkt` is this loop pass's measurement of the CURRENT
        // iterate, taken before this subproblem is built -- exactly "the
        // previous major's" residual by the time this trial is solved.
        //
        // PHASE-7 TASK 5 ADDED THE SECOND CONJUNCT, and it is a MODE gate,
        // not a reordering: at `qp_mode == QpMode::kWalk` -- the shipped
        // default -- `ssn_mode` is false and this expression is exactly the
        // `opts_.adaptive_mu` it has always been. Under kSsn the schedule is
        // off for the whole solve; see this header's WHY THE ADAPTIVE-mu
        // SCHEDULE IS OFF UNDER kSsn note for the derivation and for why the
        // scope is the solve rather than the SSN call alone.
        const bool adaptive_mu_active = opts_.adaptive_mu && !ssn_mode;
        if (adaptive_mu_active) {
            const double mu_raw = (iter == 0)
                                      ? kAdaptiveMuMax
                                      : std::clamp(kAdaptiveMuKappa * std::pow(kkt.residual(), 1.5),
                                                   kAdaptiveMuMin, kAdaptiveMuMax);
            overrides.dual_mu = std::pow(10.0, std::round(std::log10(mu_raw)));
        }
        row.mu = adaptive_mu_active ? overrides.dual_mu : opts_.qp.dual_mu;
        last_dual_mu = row.mu; // this trial's EFFECTIVE dual_mu; see the declaration note

        // PHASE-4 TASK 4: the hot handle is offered ONLY on this solve's
        // very FIRST subproblem (iter == 0) -- every later major on this
        // same engine_ already benefits from its OWN instance-level
        // border_ persistence (qp_engine.h's HOT-START REUSE note),
        // unaffected by this task, so re-offering `warm.hot` there would
        // be a no-op at best. Whether the engine actually reuses
        // anything is entirely its own call (conditions (a)-(e), the
        // fifth added in Fix Round 1); this driver never inspects the
        // handle's contents, only whether it is present.
        const bool offer_hot = iter == 0 && resolved_level == StartLevel::kHot;
        // THE CRASH BASIS, CONSUMED (Phase-6 Task 4). One-shot, at the
        // FIRST subproblem this solve builds and nowhere else: from the
        // second subproblem on, `have_seed` is true and the seed is the
        // previous QP's own answer, which is strictly better information
        // than any estimate read off the geometry. `crash_pending` is
        // cleared whether or not the seed found anything, so a solve whose
        // first subproblem is retried at a shrunken radius does not
        // re-derive it.
        bool use_crash = false;
        if (crash_pending) {
            crash_pending = false;
            // ACCUMULATED, not assigned: a restoration sub-solve's own
            // seed counts fold into these same two fields (see the fold
            // block above), so writing them would make this site's
            // ordering relative to that fold load-bearing. It is not,
            // today -- every enter_restoration() call site sits after this
            // solve -- and this keeps it from becoming so.
            Index seeded_rows = 0;
            Index seeded_bounds = 0;
            use_crash = !have_seed && crash_basis_seed(qp, opts_.feas_tol, crash_seed, seeded_rows,
                                                       seeded_bounds);
            out.counters.crash_seeded_rows += seeded_rows;
            out.counters.crash_seeded_bounds += seeded_bounds;
        }

        // --- THE QP KERNEL DISPATCH (Phase-7 Task 5) --------------------
        //
        // See this header's THE SEMISMOOTH-NEWTON TIER note for the whole
        // contract; only the mechanics are here.
        //
        // AT kWalk THIS BLOCK IS INERT AND STRUCTURALLY SO: `ssn_mode` is
        // false, so `walk_owns_this_qp` is true from its initializer, the
        // SSN branch is not entered, no SsnEngine is ever constructed, and
        // the walk call below is the same four-way select that has stood
        // here since Phase-6 Task 4 with the same arguments in the same
        // order.
        QpSolution qs;
        bool walk_owns_this_qp = !ssn_mode;
        if (ssn_mode) {
            // THE PROXIMAL CARRY, spent HERE and only here -- first
            // subproblem of the solve, once. `ssn_prox_ingested` is
            // consumed (set to 0) whether or not the ladder used it, so a
            // first subproblem retried at a shrunken radius does not
            // re-apply it. Same one-shot discipline as the crash basis
            // above, and for the same reason.
            const SsnOptions sopts = ssn_options(ssn_prox_ingested);
            ssn_prox_ingested = 0.0;
            SolveOverrides ssn_overrides;
            ssn_overrides.tr_radius = delta;
            // primal_delta/dual_mu are LEFT AT THEIR SENTINELS, which
            // ssn_engine.h resolves to the engine's own opts_ pair -- i.e.
            // to exactly `opts_.qp`, the same pair the walk is constructed
            // with. The adaptive schedule is already off for this solve
            // (see `adaptive_mu_active` above), so `overrides.dual_mu` is
            // its sentinel too and the two kernels would agree even if this
            // line forwarded it.
            SsnResult sres;
            ssn_engine().solve(qp, ssn_start_from_qp_seed(have_seed ? &seed : nullptr), sopts,
                               ssn_overrides, &sres);
            // --- R5 (Phase-7 Task 6b Phase B): GOULD'S LEMMA ------------
            //
            // INERT AND STRUCTURALLY SO at the shipped default:
            // `SqpOptions::ssn_certify_from_face` is false, so
            // `sopts.defer_certification` is false, so no SsnResult can
            // ever carry `certification_deferred` and this whole block is
            // one predictable false branch on a path that has already paid
            // a sparse factorization.
            //
            // WITH THE LEVER ON the tier-3 face solve is hoisted to HERE --
            // ahead of the usability gate's own consumers -- because it is
            // now the thing that decides whether the certificate stands:
            //   * accepted -> the face KKT's inertia gate inside
            //     refine_on_face IS the second-order evidence, and the
            //     deferred verification is never paid;
            //   * refused -> the deferred verification is paid, on the
            //     matrix ssn_engine.h left in place, at the shipped cost
            //     and for the shipped verdict. It may WITHDRAW the
            //     certificate (kIndefinite / kSingular), which rewrites
            //     `sres` into an escape that the gate below then routes to
            //     the walk exactly as an in-loop escape is routed;
            //   * the exit was not usable anyway (the trust-region gate) ->
            //     nothing is being certified, so the pending evidence is
            //     discarded unread and the subproblem hands off. This is
            //     the one path on which the lever saves a factorization
            //     WITHOUT a refinement behind it; it is structurally
            //     unreachable on the certifying path (see this header's
            //     kSsnTrViolationFactor note) and therefore contributes
            //     nothing on the shipped corpora.
            //
            // The face solve must be hoisted rather than duplicated: a
            // second refine_on_face call would pay a second factorization
            // and destroy the very saving being measured.
            QpSolution r5_refined;
            bool r5_have = false;
            bool r5_took = false;
            if (sres.certification_deferred) {
                if (ssn_exit_is_a_usable_step(sres, sopts.fb_tol)) {
                    const QpSolution r5_face = ssn_result_to_qp_solution(sres);
                    r5_took = engine_.refine_on_face(qp, r5_face, ssn_overrides, r5_refined);
                    r5_have = true;
                    if (r5_took) {
                        // GOULD'S LEMMA, SPENT. The face EQP's own KKT
                        // system was factorized inside refine_on_face and
                        // passed its inertia gate -- (n_f, m_f, 0), i.e.
                        // the reduced Hessian on the identified face is
                        // positive definite -- which IS the second-order
                        // condition a certifying exit on that face wants.
                        // The pending evidence is therefore SUPERSEDED,
                        // not skipped, and dropping it unread is the whole
                        // of the saving.
                        ssn_engine().discard_deferred_certification();
                    } else {
                        (void)ssn_engine().finish_deferred_certification(&sres);
                    }
                } else {
                    ssn_engine().discard_deferred_certification();
                }
            }
            accumulate_ssn_counters(out.counters.ssn, sres.counters);
            // THE PROBE BUDGET'S SSN CHARGE (see `ssn_budget_charge`'s own
            // declaration note for the rule). Paid whether this subproblem
            // certifies or hands off -- an escape's factorizations were
            // spent either way, and they are precisely the work the budget
            // could not previously see.
            ssn_budget_charge += sres.factorizations;
            // THE EXPORT SIDE OF THE PROXIMAL CARRY: the MAX over this
            // solve's SSN subproblems. warm_start.h's `prox_sigma` note
            // says why the max rather than the last. THE LEVEL IS
            // PROVENANCE-FREE and is taken over EVERY subproblem, escaped
            // ones included -- an exhausted ladder is exactly the evidence
            // "this solve needed damping" that the carry exists to
            // transmit. THE CENTRE IS NOT, and is stamped below, AFTER the
            // usability gate; see its own note there.
            ssn_prox_sigma_out_ = std::max(ssn_prox_sigma_out_, sres.prox_sigma);
            if (ssn_exit_is_a_usable_step(sres, sopts.fb_tol)) {
                // THE CENTRE IS STAMPED ONLY FROM A USABLE EXIT (fix round
                // 1). It used to be stamped beside the sigma above, i.e.
                // BEFORE this gate -- and the subproblem that sets the max
                // sigma is typically an EXHAUSTED LADDER, i.e. an ESCAPED
                // solve, whose x ssn_engine.h measures at 133x-160x the
                // trust region and whose export certifies nothing. The
                // carried centre was therefore, by construction, usually a
                // DIVERGED POINT -- shipped on the WarmStart interface
                // under a field documented as "the point it was reached
                // at". No live defect (the centres are unread today, Task-4
                // deviation D5), which is exactly why it had to be repaired
                // before a reader starts trusting them. The centre now
                // carries its OWN high-water mark, so it is the largest
                // sigma among CERTIFYING subproblems; when none certified,
                // the sigma carries alone and both vectors stay empty.
                if (sres.prox_sigma > ssn_prox_center_sigma_out_) {
                    ssn_prox_center_sigma_out_ = sres.prox_sigma;
                    ssn_prox_center_x_out_ = sres.x;
                    ssn_prox_center_lambda_out_ = Vec(sres.lambda_e.size() + sres.lambda_i.size());
                    ssn_prox_center_lambda_out_ << sres.lambda_e, sres.lambda_i;
                }
                qs = ssn_result_to_qp_solution(sres);
                // --- TIER 3: THE STABLE-FACE REFINEMENT ----------------
                //
                // See this header's THE SEMISMOOTH-NEWTON TIER note and
                // QpEngine::refine_on_face's own contract for the design;
                // only the mechanics are here. ONE exact solve on the face
                // the SSN just identified, which is what restores the
                // walk-exact SUBPROBLEM complementarity identity that this
                // header's WHAT IS MEASURED BUT NOT GATED note is written
                // against -- an FB kernel stopping at |phi| <= fb_tol
                // cannot supply that identity on its own, and the note now
                // states the per-mode guarantee explicitly.
                //
                // ITS COST -- one factorization, paid whether or not the
                // result is used -- is folded into `qs.counters` BEFORE the
                // common accumulation below, so it lands in
                // SqpCounters::factorizations with every other
                // factorization and needs no accumulation site of its own.
                // `qs.counters` (the SSN's own cost) is preserved across
                // the swap: refine_on_face reports only what ITS call paid.
                // R5: `r5_have` says the face solve already ran, above, as
                // the certificate itself -- so it is CONSUMED here rather
                // than re-run. At the shipped default r5_have is false and
                // this is the same call in the same place it has always
                // been.
                QpSolution refined;
                bool took;
                if (r5_have) {
                    took = r5_took;
                    refined = std::move(r5_refined);
                } else {
                    took = engine_.refine_on_face(qp, qs, ssn_overrides, refined);
                }
                const Index refine_facts = refined.counters.factorizations;
                const Index refine_steps = refined.counters.eqp_refine_steps;
                if (took) {
                    // TASK-6 INSTRUMENT (re-review NF-1), and it is counted
                    // HERE, before the move, because `refined` is the only
                    // object that holds the ADOPTED face prices at this
                    // point and it is about to be moved from. Strictly
                    // negative only -- see the counter's own note in
                    // sqp_types.h for why no tolerance appears here.
                    for (Index j = 0; j < refined.lambda_i.size(); ++j) {
                        if (refined.lambda_i(j) < 0.0) {
                            ++out.counters.ssn.ssn_refine_neg_duals;
                        }
                    }
                    refined.counters = qs.counters;
                    qs = std::move(refined);
                    ++out.counters.ssn.ssn_refinements;
                } else {
                    ++out.counters.ssn.ssn_refine_refused;
                }
                // TASK-6 INSTRUMENT. The refinement's OWN cost, charged
                // alongside the fold into qs.counters.factorizations below
                // so the two can never disagree: this field is the numerator
                // of the "what does tier 3 cost at corpus scale" ratio, and
                // it is read against SqpCounters::factorizations, which the
                // very next line feeds.
                out.counters.ssn.ssn_refine_factorizations += refine_facts;
                qs.counters.factorizations += refine_facts;
                qs.counters.eqp_refine_steps += refine_steps;
                ssn_budget_charge += refine_facts;
            } else {
                // THE HAND-OFF. Every escape, and the driver's own
                // trust-region refusal, land here identically: the SSN
                // iterates are DISCARDED (nothing below reads `sres` again)
                // and the walk re-solves this same subproblem from the same
                // seed it would have had at kWalk. One hand-off per QP --
                // there is no second SSN attempt on this subproblem, and
                // from here the walk owns it, elastic tier and all.
                walk_owns_this_qp = true;
                // R5's HOISTED FACE SOLVE, CHARGED HERE WHEN THE EXIT IT
                // WAS PAID FOR IS ABANDONED (fix round, review F3). See
                // charge_refused_face_refinement's own note for the path
                // and for why the fields are these four.
                //
                // `r5_have` HERE IMPLIES `r5_took == false`, and that is a
                // structural fact rather than an observation: an ACCEPTED
                // refinement discards the deferral unread and leaves `sres`
                // untouched, and the usability gate is a pure function of
                // `sres` -- so it still reads the true it read at the
                // hoist, and control is in the certifying branch above, not
                // here. The only way to reach this line with a hoisted
                // solve behind it is refusal followed by WITHDRAWAL, which
                // is exactly the refusal this charges.
                if (r5_have) {
                    charge_refused_face_refinement(out.counters, r5_refined, ssn_budget_charge);
                }
                // `ssn_escapes` at the driver scale counts SUBPROBLEMS
                // HANDED OFF (accumulate_ssn_counters' own note). The engine
                // already reported 1 for a genuine escape; this adds the one
                // case it cannot know about -- a certifying exit the
                // trust-region gate refused, where its own count was 0.
                if (sres.escape_reason == SsnEscape::kNone) {
                    ++out.counters.ssn.ssn_escapes;
                    // PHASE-7 TASK 6b (docket D6): and it is the ONE census
                    // bucket with no SsnEscape value behind it, written at
                    // the same site and under the same condition as the
                    // total it partitions. See SqpCounters::ssn's census
                    // note (sqp_types.h) for why the sixth bucket exists.
                    ++out.counters.ssn.ssn_escape_gate_refused;
                }
                // AND ITS COST IS STILL PAID (fix round 1). `qs` below is
                // the WALK's solution, so the escaped attempt's own
                // factorizations reach no other accumulation site -- they
                // were simply LOST, which is the same "vanishing work"
                // class as the restoration fold's, and it broke
                // sqp_types.h's documented `ssn_iters <= factorizations`
                // outright: measured 131 SSN iterations against 128
                // factorizations on tests/test_sqp_driver.cpp's
                // RestorationStaysOnTheWalkUnderKSsn before this line
                // existed. Charged HERE and only here: on the CERTIFYING
                // path the same two fields travel across inside
                // ssn_result_to_qp_solution, so this branch is the one
                // place they would otherwise disappear, and there is no
                // double count.
                charge_ssn_subproblem_cost(out.counters, sres);
            }
        }
        if (walk_owns_this_qp) {
            qs = offer_hot   ? engine_.solve(qp, seed, overrides, warm.hot)
                 : have_seed ? engine_.solve(qp, seed, overrides)
                 : use_crash ? engine_.solve(qp, crash_seed, overrides)
                             : engine_.solve(qp, overrides);
        }
        if (offer_hot) {
            // start_level_used RECORDS WHAT WAS OBSERVED, not merely
            // what was offered (see the WARM-START INGEST note above).
            // FIX ROUND 1 (Q5): this reads qs.counters.k0_reused --
            // qp_engine.h's OWN report of whether its reuse gate
            // (conditions (a)-(e)) actually judged the cache
            // trustworthy -- rather than inferring reuse from
            // `qp_factorizations == 0`, which the review showed can be
            // zero for reasons that have nothing to do with hot-start
            // reuse at all (an empty reduced system with nothing to
            // factorize, or a crossed-bounds box reported kInfeasible
            // before the loop ever runs). Any OTHER outcome -- a
            // values-hash mismatch, a different effective
            // (primal_delta, dual_mu) pair, a seed working set the
            // engine's own border stack did not already match, or a
            // stale generation (Fix Round 1's new condition (e)) --
            // silently degrades to kWarm here, never throws and never
            // mis-reports.
            //
            // PHASE-7 TASK 5 NEEDED NO EDIT HERE, and that is worth stating
            // rather than leaving to be rediscovered. A subproblem the SSN
            // tier certified was never offered the hot handle at all
            // (SsnEngine has no hot-state seam), and
            // ssn_result_to_qp_solution leaves `k0_reused` at its default
            // false -- so such a solve degrades to kWarm right here, which
            // is precisely what was OBSERVED: no cached factorization was
            // reused, because none was consulted.
            out.counters.start_level_used =
                qs.counters.k0_reused ? StartLevel::kHot : StartLevel::kWarm;
        }

        out.counters.major_iters = iter + 1;
        // TASK 5. Counted here, alongside major_iters and on exactly the
        // same event (a subproblem was solved), so full_step_majors <=
        // major_iters holds by construction -- see its own note in
        // sqp_types.h for why a routed failure counts too.
        if (full_step_funnel != nullptr) {
            ++out.counters.full_step_majors;
        }
        out.counters.qp_minor_iters += qs.counters.minor_iters;
        out.counters.factorizations += qs.counters.factorizations;
        out.counters.eqp_refine_steps += qs.counters.eqp_refine_steps;
        out.counters.border_refine_steps += qs.counters.border_refine_steps;
        out.counters.suspect_escalations += qs.counters.suspect_escalations;
        out.counters.symbolic_analyses += qs.counters.symbolic_analyses;

        // THE ELASTIC TIER (Task 8). See this header's ELASTIC TIER note
        // for every design choice below; only the mechanics are here.
        // This is the ONLY consumer of QpStatus::kInfeasible in the
        // driver, and it replaces Task 6b's immediate propagation.
        bool elastic_applied = false;
        if (qs.status == QpStatus::kInfeasible) {
            ++out.counters.elastic_activations;

            // The window the ORIGINAL solve was given, folded into the
            // elastic problem's own box: `delta` is what the driver
            // passed through SolveOverrides, and opts_.qp.tr_radius is
            // what the engine would have resolved the +inf sentinel to.
            // Both are +inf in the ordinary configuration.
            const double window = std::min(delta, opts_.qp.tr_radius);
            ElasticQp elastic =
                build_elastic_subproblem(qp, window, kElasticRhoInit, opts_.feas_tol);
            QpSolution seed_elastic = elastic_seed(elastic, qs);

            QpSolution qs_e;
            // THE STALL EARLY-EXIT's own state (Phase-5 Task 10): the
            // PREVIOUS rung's augmented solution, valid once
            // has_prev_rung is true (i.e. from the second solve on), so
            // it can be compared against the CURRENT rung's -- see THE
            // STALL EARLY-EXIT note above for the derivation.
            QpSolution qs_e_prev;
            bool has_prev_rung = false;
            double rho = kElasticRhoInit;
            for (;;) {
                // DEFAULT OVERRIDES: the +inf tr_radius sentinel, because
                // the radius is already in the box above -- passing it
                // here would cap the SLACKS at Delta too.
                const SolveOverrides elastic_overrides;
                qs_e = engine_.solve(elastic.qp, seed_elastic, elastic_overrides);
                out.counters.qp_minor_iters += qs_e.counters.minor_iters;
                out.counters.factorizations += qs_e.counters.factorizations;
                out.counters.eqp_refine_steps += qs_e.counters.eqp_refine_steps;
                out.counters.border_refine_steps += qs_e.counters.border_refine_steps;
                out.counters.suspect_escalations += qs_e.counters.suspect_escalations;
                out.counters.symbolic_analyses += qs_e.counters.symbolic_analyses;
                if (qs_e.status != QpStatus::kOptimal) {
                    break;
                }
                // MATERIALLY NONZERO IS MEASURED ON THE VIOLATION, not on
                // the scaled variable: feas_tol is a tolerance on
                // constraint violation, and sigma_j is a change of units.
                const Vec v = elastic.slack_violations(qs_e.x);
                const double v_max = v.size() > 0 ? v.maxCoeff() : 0.0;
                if (v_max <= opts_.feas_tol || !(rho < kElasticRhoMax)) {
                    break;
                }
                // THE STALL EARLY-EXIT (Phase-5 Task 10). This rung left
                // the augmented solution where the PREVIOUS one left it
                // -- so, per THE STALL EARLY-EXIT note above, escalating
                // further only re-solves the same reduced system at a
                // larger rho it never reads. Stop here instead of paying
                // for rungs whose answer is already in hand. Compared on
                // the FULL augmented x (original block AND slacks), not
                // just the slack violations `v` above: a stall is "this
                // rung changed nothing", and the slacks alone cannot rule
                // out a p that moved while s happened not to.
                if (opts_.elastic_ladder_early_exit && has_prev_rung &&
                    (qs_e.x - qs_e_prev.x).lpNorm<Eigen::Infinity>() <=
                        kElasticStallScale * std::max(1.0, qs_e_prev.x.lpNorm<Eigen::Infinity>())) {
                    break;
                }
                rho = std::min(rho * kElasticRhoFactor, kElasticRhoMax);
                set_elastic_penalty(elastic, rho);
                ++out.counters.elastic_escalations;
                qs_e_prev = qs_e;
                has_prev_rung = true;
                // CHAIN THE SEED (fix round 1). Only g changes between
                // rungs, so H/Ae/Ai's hashes and the effective
                // (primal_delta, dual_mu) pair are already unchanged --
                // but qp_engine.h's HOT-START REUSE condition (b) needs
                // the seed working set to equal the IMMEDIATELY PRECEDING
                // solve's exit working set, and re-seeding every rung from
                // the original kInfeasible solve fails it on rung 2 and
                // after. Measured: one K0 rebuild per rung (7
                // factorizations for the ladder) against 1 with the chain.
                // seed.x is zeroed for the standing reason (see WARM
                // SEEDING): it is the engine's window CENTER.
                seed_elastic = qs_e;
                seed_elastic.x.setZero();
            }

            // THE EXHAUSTION SIGNATURE (see the note): the ladder is
            // spent and the tier has nothing to offer -- the relaxation
            // is still materially open, no admissible step reduces the
            // LINEARIZED violation, and the model promises no objective
            // decrease either.
            const Vec s_final =
                qs_e.status == QpStatus::kOptimal ? elastic.slack_violations(qs_e.x) : Vec::Zero(0);
            const double slack_l1 = s_final.size() > 0 ? s_final.lpNorm<1>() : 0.0;
            const Vec p_elastic =
                qs_e.status == QpStatus::kOptimal ? Vec(qs_e.x.head(n)) : Vec::Zero(n);
            const bool closed = slack_l1 <= opts_.feas_tol;
            const bool reduced = slack_l1 <= elastic.violation_l1 - opts_.feas_tol;
            // EXACT ZERO, DELIBERATELY: see THE KNIFE-EDGE RULING above
            // (Phase-5 Task 10) for why a tolerance was considered and
            // not added here.
            const bool promises_f =
                qs_e.status == QpStatus::kOptimal && predicted_decrease(qp, p_elastic) > 0.0;
            const bool usable =
                qs_e.status == QpStatus::kOptimal && (closed || reduced || promises_f);

            if (!usable) {
                row.qp_solved = true;
                row.elastic_applied = true;
                row.qp_status = qs_e.status;
                row.qp_minor_iters = qs_e.counters.minor_iters;
                row.qp_factorizations = qs_e.counters.factorizations;
                // DIAGNOSTIC ONLY, exactly as on a routed-failure row: no
                // step was taken from here.
                row.step_norm = p_elastic.size() > 0 ? p_elastic.lpNorm<Eigen::Infinity>() : 0.0;
                row.verdict = StepVerdict::kRestore;
                out.history.push_back(row);
                // KLV Algorithm 5's authoritative trigger, serviced (Task
                // 9). Through Task 8 this was the interim exit.
                if (enter_restoration()) {
                    continue;
                }
                // qs_e (the elastic re-solve) is in the AUGMENTED
                // (original + slack) variable space, so it is never a
                // valid activity source here -- fall back to `seed`
                // (unless restoration moved x away from what it
                // describes; see restoration_moved_x's own note).
                return finish(
                    std::move(out), restoration_exit_status, x, lambda_e, lambda_i,
                    restoration_exit_kkt, restoration_exit_f,
                    make_warm_start(seam, (!restoration_moved_x && have_seed) ? &seed : nullptr, qp,
                                    qp_built, &ev, &x, delta, last_dual_mu, opts_.qp.primal_delta,
                                    strategy.get(), engine_.hot_state()));
            }

            qs = elastic_project(elastic, qp, qs_e, /*carry_multipliers=*/closed);
            elastic_applied = true;
        }

        row.qp_solved = true;
        row.elastic_applied = elastic_applied;
        row.qp_status = qs.status;
        row.qp_minor_iters = qs.counters.minor_iters;
        row.qp_factorizations = qs.counters.factorizations;
        row.step_norm = qs.x.size() > 0 ? qs.x.lpNorm<Eigen::Infinity>() : 0.0;
        row.tr_binding =
            std::any_of(qs.tr_active.begin(), qs.tr_active.end(), [](bool b) { return b; });

        if (qs.status != QpStatus::kOptimal) {
            // SUBPROBLEM FAILURE ROUTING (see this header's note). A
            // failure that still handed back a usable iterate is a
            // property of THIS SUBPROBLEM AT THIS RADIUS, and the loop
            // already owns the instrument that changes it without moving
            // the iterate. Note what is NOT done: the returned step is
            // never taken and never judged -- the strategy is not
            // consulted at all, because there is no certified step to
            // judge -- so `row.verdict` keeps its kReject default, which
            // is the accurate description of what happened to this trial.
            if (qp_failures_in_a_row == 0 && qp_failure_is_retryable(qp, qs, opts_.feas_tol)) {
                ++qp_failures_in_a_row;
                ++rejections_at_iterate;
                ++out.counters.rejected_steps;
                out.history.push_back(row);
                // THE RADIUS FLOOR (Task 9) applies to a ROUTED failure
                // exactly as to a judged rejection: both are "the radius
                // was shrunk here and the iterate did not move", and the
                // floor is a statement about the radius, not about which
                // mechanism last lowered it.
                if (shrink_hits_floor(delta)) {
                    if (enter_restoration()) {
                        continue;
                    }
                    // `qs` is THIS failed solve's own activity, in the
                    // ORIGINAL variable space, describing the region
                    // around x -- see WARM SEEDING above for why that is
                    // meaningful even though the solve failed. Not used
                    // if restoration moved x away from it.
                    return finish(std::move(out), restoration_exit_status, x, lambda_e, lambda_i,
                                  restoration_exit_kkt, restoration_exit_f,
                                  make_warm_start(seam, restoration_moved_x ? nullptr : &qs, qp,
                                                  qp_built, &ev, &x, delta, last_dual_mu,
                                                  opts_.qp.primal_delta, strategy.get(),
                                                  engine_.hot_state()));
                }
                delta = shrunk_radius(delta);
                // Seeded from the FAILED solve's ACTIVE SET. On
                // kNumericalError the engine clears the multipliers
                // (qp_engine.h's exit semantics), so the seed carries
                // activity only -- which is all warm seeding uses, and
                // which on this path is the point: the engine's own
                // start-of-solve inertia repair sees a working set the
                // failed solve had to walk to, and can act on it at
                // iter == 0 where it could not mid-solve.
                seed = std::move(qs);
                seed.x.setZero(); // never re-center the radius; see WARM SEEDING
                have_seed = true;
                continue;
            }
            out.history.push_back(row);
            // No restoration was consulted on this path -- x is still
            // the pre-trial iterate `qs` was solved at, so its own
            // activity (bound_state/ineq_active) is exactly the region
            // around the point being returned.
            return finish(std::move(out), map_status(qs.status), x, lambda_e, lambda_i, kkt, row.f,
                          make_warm_start(seam, &qs, qp, qp_built, &ev, &x, delta, last_dual_mu,
                                          opts_.qp.primal_delta, strategy.get(),
                                          engine_.hot_state()));
        }
        qp_failures_in_a_row = 0;

        // THE TRIAL POINT, VALUES ONLY FIRST (Task 8, the eval-economics
        // carry). globalization.h's judge() reads StepContext, and every
        // field of it (f_old/f_new/h_old/h_new/pred_df/tr_active/
        // rejections_at_iterate -- confirmed against globalization.h's
        // own struct) comes from f/cE/cI or from the QP already solved,
        // NEVER from a gradient or a Jacobian; build_soc_subproblem below
        // reads only ev.ce/ev.ci and ev_trial.ce/ev_trial.ci for the same
        // reason. So the trial is evaluated through eval_nlp_values here,
        // and stays that way through every REJECTED outcome (including a
        // rejected-then-SOC-rejected one) -- on a rejection-heavy
        // fixture this is the entire saving, since the rejected majority
        // never pays for a gradient or a Jacobian it was always going to
        // throw away. THE ONE EXCEPTION IS AN ACCEPTANCE: an accepted
        // trial's `ev_trial` becomes the next iterate's `ev` (see MODEL
        // EVALUATION), which the convergence test and the next
        // subproblem both need in full -- so the direct-accept branch
        // below (the ACCEPTED... section's `else`) upgrades it in place
        // (upgrade_to_full) the moment that is known, and a SOC-promoted
        // acceptance upgrades `ev_soc` instead, right where its own
        // promotion is decided -- `ev_trial` in that case is never
        // upgraded at all, because nothing downstream ever reads it
        // again.
        const Vec x_trial = x + qs.x;
        NlpEval ev_trial = seam.eval_nlp_values(x_trial);

        StepContext ctx;
        ctx.f_old = ev.f;
        ctx.f_new = ev_trial.f;
        ctx.h_old = row.violation_l1;
        ctx.h_new = constraint_violation_l1(ev_trial);
        ctx.pred_df = predicted_decrease(qp, qs.x);
        ctx.tr_active = row.tr_binding;
        ctx.rejections_at_iterate = rejections_at_iterate;

        // KLV ALGORITHM 2'S ||d|| = 0 SHORT-CIRCUIT (see kZeroStepScale
        // for why this is here and not left to the convergence test at
        // the top of the loop). The trust-region conjunct is not
        // decoration: a step held AT zero by a radius that has shrunk to
        // nothing is not evidence of a KKT point, and its z would be the
        // radius talking (qp_problem.h's STATIONARITY CAVEAT). A step
        // that is zero because the SUBPROBLEM's answer is zero has the
        // radius strictly inactive.
        const double x_scale = std::max(1.0, x.lpNorm<Eigen::Infinity>());
        const bool zero_step = !row.tr_binding && row.step_norm <= kZeroStepScale * x_scale;

        StepVerdict verdict = zero_step ? StepVerdict::kAcceptF : strategy->judge(ctx);

        // SECOND-ORDER CORRECTION (Task 7). See this header's note for
        // the derivation and every design choice below; only the
        // mechanics are here. Gated on the ORIGINAL verdict, never on a
        // routed QP failure (this code is unreached there -- see the
        // note).
        bool soc_applied = false;
        NlpEval ev_soc;    // filled iff soc_applied
        QpSolution qs_soc; // filled iff soc_applied
        Vec x_soc;         // filled iff soc_applied
        if (verdict == StepVerdict::kReject && opts_.enable_soc && !elastic_applied &&
            ctx.h_new > ctx.h_old) {
            ++out.counters.soc_steps;

            // See build_soc_subproblem's own doc for the formula; ev.ce/
            // ev.ci (cE(x)/cI(x)) and ev_trial.ce/ev_trial.ci (cE(x+p)/
            // cI(x+p)) are both already in hand (see MODEL EVALUATION),
            // so constructing the rhs shift costs NO EXTRA MODEL
            // EVALUATION -- it is the RE-SOLVE below, on acceptance,
            // that costs one (see the header note's cost accounting).
            const QpProblem soc_qp = build_soc_subproblem(qp, ev, ev_trial, qs.x, qs.ineq_active);

            // Warm-started from the REJECTED solve's own activity, at the
            // SAME radius -- see the note for why this is a hot start.
            QpSolution seed_soc = qs; // COPY: `qs` is still needed below
            seed_soc.x.setZero();
            SolveOverrides soc_overrides;
            soc_overrides.tr_radius = delta;
            qs_soc = engine_.solve(soc_qp, seed_soc, soc_overrides);

            out.counters.qp_minor_iters += qs_soc.counters.minor_iters;
            out.counters.factorizations += qs_soc.counters.factorizations;
            out.counters.eqp_refine_steps += qs_soc.counters.eqp_refine_steps;
            out.counters.border_refine_steps += qs_soc.counters.border_refine_steps;
            out.counters.suspect_escalations += qs_soc.counters.suspect_escalations;
            out.counters.symbolic_analyses += qs_soc.counters.symbolic_analyses;

            if (qs_soc.status == QpStatus::kOptimal) {
                x_soc = x + qs_soc.x; // NOT x_trial + qs_soc.x; see the note
                // VALUES ONLY FIRST, same reasoning as x_trial above:
                // soc_ctx below reads only f/h, so judge() never needs a
                // derivative at x_soc either.
                ev_soc = seam.eval_nlp_values(x_soc);

                StepContext soc_ctx;
                soc_ctx.f_old = ev.f;
                soc_ctx.f_new = ev_soc.f;
                soc_ctx.h_old = row.violation_l1;
                soc_ctx.h_new = constraint_violation_l1(ev_soc);
                soc_ctx.pred_df = ctx.pred_df; // UNCHANGED; see the note
                soc_ctx.tr_active = row.tr_binding;
                soc_ctx.rejections_at_iterate = rejections_at_iterate;

                const StepVerdict soc_verdict = strategy->judge(soc_ctx);
                if (soc_verdict == StepVerdict::kAcceptF || soc_verdict == StepVerdict::kAcceptH) {
                    soc_applied = true;
                    verdict = soc_verdict;
                    ctx = soc_ctx;
                    ++out.counters.soc_applied;
                    // UPGRADE TO FULL (Task 8): promoted, so `ev_soc`
                    // becomes the next iterate's `ev` below (the
                    // `soc_applied` branch of ACCEPTED), which needs the
                    // derivatives the values-only evaluation above
                    // skipped -- in place, not a second eval_f/eval_ce/
                    // eval_ci (see upgrade_to_full's own note).
                    seam.refresh_derivatives(ev_soc, x_soc);
                    ++out.counters.evals_full;
                } else {
                    // The corrected point is ALSO not accepted (a
                    // kReject, or -- not promoted -- a kRestore); fall
                    // through with the ORIGINAL verdict below. `ev_soc`
                    // is discarded values-only (Task 8) -- it never
                    // becomes anyone's `ev`, so it never earns the
                    // upgrade above.
                    ++out.counters.soc_rejected;
                    ++out.counters.evals_values;
                }
            } else {
                // The SOC re-solve itself failed (qs_soc.status !=
                // kOptimal); fall through with the ORIGINAL verdict
                // below. qs_soc/ev_soc/x_soc are unused.
                ++out.counters.soc_qp_infeasible;
            }
        }

        // TASK 8: `ev_trial`'s OWN FATE is now decided -- verdict and
        // soc_applied are both final. It is UPGRADED (evals_full, at the
        // direct-accept branch further below) only when this trial is
        // accepted WITHOUT a promoted SOC correction; every other
        // outcome -- rejected, restored, or accepted THROUGH SOC (whose
        // OWN ev_soc, evaluated at a DIFFERENT point, becomes the next
        // `ev` instead) -- leaves `ev_trial` values-only forever.
        if (soc_applied ||
            !(verdict == StepVerdict::kAcceptF || verdict == StepVerdict::kAcceptH)) {
            ++out.counters.evals_values;
        }

        row.verdict = verdict;
        row.soc_applied = soc_applied;
        out.history.push_back(row);

        if (verdict == StepVerdict::kReject) {
            // SHRINK AND RE-SOLVE THE SAME ITERATE. The multipliers this
            // subproblem priced are DISCARDED -- they belong to a step
            // that was not taken, and using them would move the Hessian
            // (and so the model) at an iterate that never moved. ONE
            // rejection is charged for the pair even when SOC was
            // attempted above -- see the note.
            ++rejections_at_iterate;
            ++out.counters.rejected_steps;
            // THE RADIUS FLOOR (Task 9) -- KLV Algorithm 4's alpha_min
            // trigger in trust-region form. The shrink is NOT taken and
            // NOT clamped: crossing the floor IS the request. See RADIUS
            // MANAGEMENT.
            if (shrink_hits_floor(delta)) {
                if (enter_restoration()) {
                    continue;
                }
                // `qs` is the REJECTED trial's own (kOptimal) solution,
                // still describing the region around x (x has not moved
                // -- unless restoration itself moved it; see
                // restoration_moved_x's own note).
                return finish(std::move(out), restoration_exit_status, x, lambda_e, lambda_i,
                              restoration_exit_kkt, restoration_exit_f,
                              make_warm_start(seam, restoration_moved_x ? nullptr : &qs, qp,
                                              qp_built, &ev, &x, delta, last_dual_mu,
                                              opts_.qp.primal_delta, strategy.get(),
                                              engine_.hot_state()));
            }
            delta = shrunk_radius(delta);
            seed = std::move(qs);
            seed.x.setZero(); // never re-center the radius; see WARM SEEDING
            have_seed = true;
            continue;
        }

        if (verdict == StepVerdict::kRestore) {
            // The funnel's own restoration signature (globalization.h's
            // five conjuncts), serviced (Task 9). Through Task 8 this was
            // the interim exit.
            if (enter_restoration()) {
                continue;
            }
            // Same reasoning as the kReject/floor exit just above: `qs`
            // still describes x unless restoration moved it.
            return finish(std::move(out), restoration_exit_status, x, lambda_e, lambda_i,
                          restoration_exit_kkt, restoration_exit_f,
                          make_warm_start(seam, restoration_moved_x ? nullptr : &qs, qp, qp_built,
                                          &ev, &x, delta, last_dual_mu, opts_.qp.primal_delta,
                                          strategy.get(), engine_.hot_state()));
        }

        // ACCEPTED (kAcceptF or kAcceptH), possibly via SOC. The radius
        // grows only on the evidence described in RADIUS MANAGEMENT, and
        // it is computed BEFORE the move so `ev` is still the old
        // iterate's. ctx.pred_df/row.tr_binding are ALWAYS the ORIGINAL
        // QP's -- see the SECOND-ORDER CORRECTION note on why pred_df is
        // not recomputed from p_soc -- so this growth test is unaffected
        // by whether SOC fired; only ctx.f_new (hence actual_df) differs.
        //
        // std::isfinite(delta) is not decoration: at the +inf "no trust
        // region" setting min(+inf * 2, tr_max) is tr_max, so an unguarded
        // growth rule would SHRINK the radius the one time it is not
        // supposed to touch it (see the constructor's tr_max note).
        const double actual_df = ev.f - ctx.f_new;
        if (row.tr_binding && std::isfinite(delta) && ctx.pred_df > 0.0 &&
            actual_df >= kTrGrowThreshold * ctx.pred_df) {
            delta = std::min(delta * kTrGrowFactor, opts_.tr_max);
        }

        if (soc_applied) {
            x = x_soc;
            ev = std::move(ev_soc); // already upgraded to full -- see the promotion above
            lambda_e = qs_soc.lambda_e;
            lambda_i = qs_soc.lambda_i;
            duals_ingested = false; // W1: re-priced by this solve's own QP
            seed = std::move(qs_soc);
        } else {
            // UPGRADE TO FULL (Task 8): a DIRECT acceptance (no SOC, or
            // SOC ran and was not promoted -- either way `verdict` is
            // kAcceptF/kAcceptH and `ev_trial` is what becomes `ev`
            // below), so the values-only evaluation above is not enough
            // -- fill in its derivatives in place (upgrade_to_full;
            // f/cE/cI are NOT recomputed). This is the one place this
            // task's saving is given back: one eval_grad/eval_jac_e/
            // eval_jac_i on every ACCEPTED trial (never a rejected one,
            // which is the overwhelming majority on a rejection-heavy
            // fixture), because nlp_model.h deliberately adds no third
            // "derivatives only, values already known" entry point.
            x = x_trial;
            seam.refresh_derivatives(ev_trial, x_trial);
            ++out.counters.evals_full;
            ev = std::move(ev_trial);
            lambda_e = qs.lambda_e;
            lambda_i = qs.lambda_i;
            duals_ingested = false; // W1: re-priced by this solve's own QP
            seed = std::move(qs);
        }
        ++out.counters.steps_accepted;
        rejections_at_iterate = 0;
        subproblem_is_stale = true;
        // Active set only -- never the previous step. See this header's
        // WARM SEEDING note: seed.x is the engine's trust-region CENTER,
        // and in step variables that center must be p = 0.
        seed.x.setZero();
        have_seed = true;
    }
}

SqpStatus SqpDriver::map_status(QpStatus qp_status) {
    switch (qp_status) {
    case QpStatus::kInfeasible:
        return SqpStatus::kInfeasible;
    case QpStatus::kOptimal:
    case QpStatus::kMaxIter:
    case QpStatus::kNumericalError:
        break;
    }
    return SqpStatus::kNumericalError;
}

bool SqpDriver::shrink_hits_floor(double delta) const {
    return std::isfinite(delta) && delta * kTrShrinkFactor < opts_.tr_min;
}

double SqpDriver::shrunk_radius(double delta) const {
    return std::isfinite(delta) ? delta * kTrShrinkFactor : opts_.tr_max;
}

double SqpDriver::restoration_restart_radius() const {
    const double base = std::isfinite(opts_.tr_init) ? opts_.tr_init : opts_.tr_max;
    return std::max(opts_.tr_min, kRestoreRadiusFactor * base);
}

SsnEngine &SqpDriver::ssn_engine() {
    if (ssn_engine_ == nullptr) {
        ssn_engine_ = std::make_unique<SsnEngine>(opts_.qp);
    }
    return *ssn_engine_;
}

SsnOptions SqpDriver::ssn_options(double prox_sigma_init) const {
    SsnOptions sopts;
    sopts.fb_tol = ssn_fb_tol_for(opts_.kkt_tol, opts_.feas_tol);
    sopts.prox_sigma_init = prox_sigma_init;
    // PHASE-7 TASK 6b PHASE B. The four research levers, each carried from
    // its own SqpOptions field and each at the value that reproduces the
    // shipped kernel bit for bit when the field is at its default.
    sopts.defer_certification = opts_.ssn_certify_from_face;
    sopts.sigma_rule = opts_.ssn_sigma_rule;
    sopts.hint_rule = opts_.ssn_hint_rule;
    sopts.infeasibility_rule = opts_.ssn_infeasibility_rule;
    return sopts;
}

WarmStart SqpDriver::make_warm_start(AggregateEvalSeam &seam, const QpSolution *activity,
                                     const QpProblem &qp, bool qp_built, const NlpEval *probe_ev,
                                     const Vec *probe_x, double delta, double dual_mu_eff,
                                     double primal_delta_eff, const GlobalizationStrategy *strategy,
                                     std::shared_ptr<const HotState> hot) {
    WarmStart w;
    // TRUE ON EVERY EXIT BUT ONE -- see THE UNEVALUABLE EXIT above, and
    // warm_start.h's `valid` note, which documents that exception as the
    // boundary of its "a failed solve's point is still safe evidence"
    // contract. A null `probe_ev` IS that exit.
    w.valid = probe_ev != nullptr;
    w.hot = std::move(hot);

    const Index n = seam.n();
    const Index mi = seam.mi();
    w.qp_working_set = WorkingSet(n, mi);
    w.ineq_active.assign(static_cast<std::size_t>(mi), 0);
    w.bound_active.assign(static_cast<std::size_t>(n), 0);
    if (activity != nullptr) {
        w.qp_working_set.bound_state() = activity->bound_state;
        for (std::size_t j = 0; j < activity->ineq_active.size(); ++j) {
            if (activity->ineq_active[j]) {
                w.qp_working_set.add_ineq(static_cast<Index>(j));
                w.ineq_active[j] = 1;
            }
        }
        for (std::size_t i = 0; i < activity->bound_state.size(); ++i) {
            switch (activity->bound_state[i]) {
            case BoundState::kAtLower:
                w.bound_active[i] = -1;
                break;
            case BoundState::kAtUpper:
            case BoundState::kFixed: // see warm_start.h's bound_active note
                w.bound_active[i] = 1;
                break;
            case BoundState::kFree:
                w.bound_active[i] = 0;
                break;
            }
        }
    }

    // Only a FunnelStrategy exposes a width to read (globalization.h's
    // GlobalizationStrategy has no such virtual); a caller-supplied
    // strategy of another type, or one not yet reset(), leaves the
    // "unset" sentinel.
    if (const auto *funnel = dynamic_cast<const FunnelStrategy *>(strategy)) {
        if (funnel->initialized()) {
            w.funnel_width = funnel->width();
        }
    }
    w.tr_radius = delta;
    w.primal_delta = primal_delta_eff;
    w.dual_mu = dual_mu_eff;
    // THE ZERO-MAJOR PROBE -- see this function's own note above for why
    // the middle branch is sound, what it costs, and which exits pay it.
    // The last branch is THE UNEVALUABLE EXIT: no probe, and the hash
    // stays at warm_start.h's sentinel on an object already marked cold.
    if (qp_built) {
        w.structure_hash = detail::structural_hash(qp);
    } else if (probe_ev != nullptr) {
        const QpProblem probe = seam.build_subproblem(*probe_ev, *probe_x, Vec::Zero(seam.me()),
                                                      Vec::Zero(seam.mi()), /*obj_scale=*/1.0);
        w.structure_hash = detail::structural_hash(probe);
    }
    return w;
}

SqpSolution SqpDriver::finish(SqpSolution out, SqpStatus status, const Vec &x, const Vec &lambda_e,
                              const Vec &lambda_i, const SqpKkt &kkt, double f, WarmStart warm) {
    out.status = status;
    out.x = x;
    out.lambda_e = lambda_e;
    out.lambda_i = lambda_i;
    out.z = kkt.z;
    out.f = f;
    // The point/multipliers make_warm_start's caller reports are exactly
    // the ones being finished here -- see WarmStart's own note on why a
    // failed solve's point is still safe to carry.
    warm.x = x;
    warm.lambda_e = lambda_e;
    warm.lambda_i = lambda_i;
    warm.z = kkt.z;
    out.warm_start = std::move(warm);
    return out;
}

} // namespace hven::solvers
