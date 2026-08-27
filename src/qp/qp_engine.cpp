// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// qp_engine.cpp — QpEngine's primal active-set walk, and everything the walk
// orchestrates.
//
// This TU holds the DEFINITIONS of every QpEngine member function except the
// constructor; the class, its data members, its nested types, its
// documentation and its constructor stay in
// include/hven/detail/qp/qp_engine.h, and the file's loop contract stays there
// too. The engine is a plain (non-template) class, so CLAUDE.md §5's
// header/templated exception does not apply to any of it: what is here is
// orchestration -- the walk, its classification, the border-mode linear
// algebra bookkeeping, options resolution and instrumentation.
//
// EVERY member travels together, into ONE TU, deliberately. The walk's
// per-element helpers -- in_working, row_tolerance, has_border,
// dropped_directional, mark_{ineq,bound}_seen -- are called from inside the
// ratio test's and sync_borders' own loops, so the inliner must still see
// across those sites exactly what it saw when they were siblings in the
// header. Same TU, same visibility, same inlining. The only boundary this
// carve introduces is engine-to-CALLER, which is crossed once per QP solve.
//
// The `detail::` free functions above QpEngine (the iteration cap, the inertia
// verdict, the ride constants, the hot-start fingerprints) stay inline in the
// header: tests, bench and src/drivers/sqp_driver.cpp call them directly, and
// this TU inlines them exactly as before.
//
// FP arithmetic crosses this TU boundary under ONE uniform flag regime on both
// sides; every asserted counter must be bit-identical across the boundary, so
// a counter delta here is a FAILED CARVE, to be reverted or redrawn, never a
// re-derivation.

#include <hven/detail/qp/qp_engine.h>

namespace hven::solvers {

void QpEngine::attach_ledger(Ledger *ledger, std::string label_prefix) {
    ledger_ = ledger;
    label_prefix_ = std::move(label_prefix);
    solve_counter_ = 0;
}

QpSolution QpEngine::solve(const QpProblem &qp) const {
    return run(qp, nullptr, false, SolveOverrides{});
}

QpSolution QpEngine::solve(const QpProblem &qp, const QpSolution &seed) const {
    return run(qp, &seed, true, SolveOverrides{});
}

QpSolution QpEngine::solve(const QpProblem &qp, const SolveOverrides &overrides) const {
    return run(qp, nullptr, false, overrides);
}

QpSolution QpEngine::solve(const QpProblem &qp, const QpSolution &seed,
                           const SolveOverrides &overrides) const {
    return run(qp, &seed, true, overrides);
}

QpSolution QpEngine::solve(const QpProblem &qp, const QpSolution &seed,
                           const SolveOverrides &overrides,
                           const std::shared_ptr<const HotState> &hot) const {
    return run(qp, &seed, true, overrides, hot);
}

std::shared_ptr<const HotState> QpEngine::hot_state() const {
    if (!border_valid_) {
        return nullptr;
    }
    return std::make_shared<const HotState>(
        HotState{border_, border_structural_hash_, border_values_hash_, border_effective_delta_,
                 border_effective_mu_, border_exit_bound_state_, border_exit_active_ineq_,
                 border_kkt_session_id_, border_kkt_epoch_});
}

bool QpEngine::refine_on_face(const QpProblem &qp_in, const QpSolution &face,
                              const SolveOverrides &overrides, QpSolution &out) const {
    out = face;
    out.counters = QpCounters{};

    qp_in.validate();
    const Index n = qp_in.n();
    const Index me = qp_in.me();
    const Index mi = qp_in.mi();

    // A face this function cannot read is a face it must not guess at.
    if (face.x.size() != n || static_cast<Index>(face.bound_state.size()) != n ||
        static_cast<Index>(face.ineq_active.size()) != mi || !face.x.allFinite()) {
        return false;
    }

    const QpOptions eff_opts = resolve_effective_options(opts_, overrides);

    // --- The face, transcribed into a WorkingSet ----------------------
    //
    // TRUST-REGION PINS ARE PART OF THE FACE HERE, unlike in
    // `ingest_seed_working_set`, which deliberately IGNORES `tr_active`.
    // This function is re-solving THE SAME subproblem the caller just
    // solved, whose box really does include the radius, so a variable the
    // caller reports TR-pinned is a pinned variable OF THIS FACE -- at
    // the value the caller stopped at, via a local copy of the problem
    // with that variable fixed.
    WorkingSet ws(n, mi);
    const bool tr_enabled = std::isfinite(eff_opts.tr_radius);
    // A `tr_active` flag means nothing without a radius in force -- with
    // the +inf sentinel there is no trust region for a pin to belong to --
    // so the flags are read only when this solve HAS one.
    const bool have_tr = tr_enabled && static_cast<Index>(face.tr_active.size()) == n;
    bool any_tr_pin = false;
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        if (qp_in.lower(i) == qp_in.upper(i)) {
            ws.bound_state()[si] = BoundState::kFixed;
            continue;
        }
        if (have_tr && face.tr_active[si]) {
            any_tr_pin = true;
            continue; // pinned below, against the local copy
        }
        const BoundState st = face.bound_state[si];
        if (st == BoundState::kAtLower && qp_in.lower(i) > -detail::kEngineInfBound) {
            ws.bound_state()[si] = BoundState::kAtLower;
        } else if (st == BoundState::kAtUpper && qp_in.upper(i) < detail::kEngineInfBound) {
            ws.bound_state()[si] = BoundState::kAtUpper;
        }
    }
    QpProblem tr_problem;
    if (any_tr_pin) {
        tr_problem = qp_in;
        for (Index i = 0; i < n; ++i) {
            const auto si = static_cast<std::size_t>(i);
            if (face.tr_active[si]) {
                tr_problem.lower(i) = face.x(i);
                tr_problem.upper(i) = face.x(i);
                ws.bound_state()[si] = BoundState::kFixed;
            }
        }
    }
    const QpProblem &qp = any_tr_pin ? tr_problem : qp_in;
    for (Index r = 0; r < mi; ++r) {
        if (face.ineq_active[static_cast<std::size_t>(r)]) {
            ws.add_ineq(r);
        }
    }

    const Index n_working = static_cast<Index>(ws.active_ineq().size());
    if (ws.num_free() == 0 && me == 0 && n_working == 0) {
        // Every variable pinned and nothing to solve: the face already
        // DETERMINES x and there is no refinement to make (the same
        // empty-system case `eliminated_candidate` short-circuits, and it
        // costs no factorization there either).
        return false;
    }
    if (me + n_working > ws.num_free()) {
        return false; // THE RANK PRE-SCREEN -- see refine_on_face's header note
    }

    // --- One exact solve on that face ---------------------------------
    detail::KktFactor kkt;
    ++out.counters.factorizations;
    const EqpResult eqp = solve_eqp(qp, ws, kkt, eff_opts);
    out.counters.eqp_refine_steps += eqp.refine_steps; // identically 0; see eqp_solve.h
    if (detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(), me + n_working) !=
        detail::InertiaVerdict::kOk) {
        return false;
    }
    if (eqp.x.size() != n || !eqp.x.allFinite()) {
        return false;
    }

    // --- The gate: is this a legal answer to the SUBPROBLEM? ----------
    //
    // The window is the one `run()` would have built for this same
    // subproblem: centred on the clamped origin and intersected with the
    // real box. Compared to `feas_tol`, so no new tolerance is
    // introduced.
    for (Index i = 0; i < n; ++i) {
        const double lo = qp_in.lower(i);
        const double up = qp_in.upper(i);
        double lo_eff = lo;
        double up_eff = up;
        if (tr_enabled) {
            const double c = std::min(std::max(0.0, lo), up);
            lo_eff = std::max(lo, c - eff_opts.tr_radius);
            up_eff = std::min(up, c + eff_opts.tr_radius);
        }
        if (eqp.x(i) < lo_eff - eff_opts.feas_tol || eqp.x(i) > up_eff + eff_opts.feas_tol) {
            return false;
        }
    }
    if (mi > 0) {
        const Vec Aix = qp.Ai * eqp.x;
        for (Index r = 0; r < mi; ++r) {
            if (face.ineq_active[static_cast<std::size_t>(r)]) {
                continue; // on the face: driven to equality by the solve itself
            }
            const double row_scale = std::max(1.0, qp.Ai.row(r).cwiseAbs().sum());
            if (Aix(r) - qp.bi(r) > eff_opts.feas_tol * row_scale) {
                return false;
            }
        }
    }

    // --- Accepted. Price it and report it in the caller's own shape ---
    Vec lambda_e = Vec::Zero(me);
    Vec lambda_i = Vec::Zero(mi);
    Vec z = Vec::Zero(n);
    price(qp, ws, eqp, lambda_e, lambda_i, z);
    if (have_tr) {
        // Section 6's reporting exclusion, applied here for the same
        // reason `run()` applies it: a trust-region pin is not a bound of
        // the caller's problem, so it may not leave a price behind.
        for (Index i = 0; i < n; ++i) {
            if (face.tr_active[static_cast<std::size_t>(i)]) {
                z(i) = 0.0;
            }
        }
    }
    out.x = eqp.x;
    out.lambda_e = std::move(lambda_e);
    out.lambda_i = std::move(lambda_i);
    out.z = std::move(z);
    // bound_state/ineq_active/tr_active are the caller's OWN exports,
    // carried verbatim: this solve did not change the face, it solved it.
    return true;
}

QpOptions QpEngine::resolve_effective_options(const QpOptions &opts,
                                              const SolveOverrides &overrides) {
    if (std::isnan(overrides.tr_radius) || overrides.tr_radius < 0.0) {
        throw std::invalid_argument(fmt::format(
            "QpEngine::solve: SolveOverrides.tr_radius must be >= 0, or the +inf sentinel "
            "for \"use opts_.tr_radius\" (qp_types.h) -- got {}",
            overrides.tr_radius));
    }
    if (!std::isfinite(overrides.tr_radius) &&
        (std::isnan(opts.tr_radius) || opts.tr_radius < 0.0)) {
        throw std::invalid_argument(
            fmt::format("QpEngine::solve: QpOptions.tr_radius must be >= 0, or the +inf value that "
                        "disables the trust region (qp_types.h) -- got {}",
                        opts.tr_radius));
    }
    if (std::isnan(overrides.primal_delta)) {
        throw std::invalid_argument(
            "QpEngine::solve: SolveOverrides.primal_delta must not be NaN (negative is the "
            "documented sentinel for \"use opts_.primal_delta\" -- see qp_types.h)");
    }
    if (std::isnan(overrides.dual_mu)) {
        throw std::invalid_argument(
            "QpEngine::solve: SolveOverrides.dual_mu must not be NaN (negative is the "
            "documented sentinel for \"use opts_.dual_mu\" -- see qp_types.h)");
    }
    // The guard on each of these two is the EXACT NEGATION of the
    // corresponding resolution ternary's condition below, so "checked" and
    // "selected" cannot drift apart if a ternary is ever rewritten. (NaN
    // overrides are already rejected above, so `!(x >= 0.0)` is here just
    // the sentinel test `x < 0.0` spelled in the ternary's own terms.)
    if (!(overrides.primal_delta >= 0.0) &&
        (std::isnan(opts.primal_delta) || opts.primal_delta < 0.0)) {
        throw std::invalid_argument(fmt::format(
            "QpEngine::solve: QpOptions.primal_delta must be >= 0 (0 means no primal "
            "regularization; negative is SolveOverrides' sentinel, never a stored value "
            "-- qp_types.h) -- got {}",
            opts.primal_delta));
    }
    if (!(overrides.dual_mu >= 0.0) && (std::isnan(opts.dual_mu) || opts.dual_mu < 0.0)) {
        throw std::invalid_argument(fmt::format(
            "QpEngine::solve: QpOptions.dual_mu must be >= 0 (0 means no dual "
            "regularization; negative is SolveOverrides' sentinel, never a stored value "
            "-- qp_types.h) -- got {}",
            opts.dual_mu));
    }
    QpOptions eff = opts;
    eff.tr_radius = std::isfinite(overrides.tr_radius) ? overrides.tr_radius : opts.tr_radius;
    eff.primal_delta = overrides.primal_delta >= 0.0 ? overrides.primal_delta : opts.primal_delta;
    eff.dual_mu = overrides.dual_mu >= 0.0 ? overrides.dual_mu : opts.dual_mu;
    return eff;
}

QpSolution QpEngine::run(const QpProblem &qp_in, const QpSolution *seed, bool warm,
                         const SolveOverrides &overrides,
                         const std::shared_ptr<const HotState> &hot) const {
    // ADOPT AN EXTERNAL HOT HANDLE. `hot` (WarmStart::hot, opaque) is a
    // snapshot possibly produced by a DIFFERENT QpEngine instance's
    // hot_state(). Adopted ONLY when this engine's OWN instance-level
    // cache is not already trustworthy (border_valid_ false). Adoption
    // makes this engine's border_/hash/exit-state members look exactly
    // like the handle's snapshot; the reuse-eligibility check below
    // (conditions (a)-(e)) is the only thing that decides whether
    // adopting it actually skips a factorization. ws_algebra ==
    // kRefactorize never adopts.
    //
    // The identity pair is copied in alongside the four problem-shaped
    // fields: it is what lets condition (e) tell whether the OBJECT
    // `border_` now points at still carries the numerics this identity
    // describes, not merely whether the PROBLEM still looks the same.
    if (!border_valid_ && hot != nullptr && hot->border != nullptr &&
        opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
        border_ = hot->border;
        border_structural_hash_ = hot->structural_hash;
        border_values_hash_ = hot->values_hash;
        border_effective_delta_ = hot->effective_delta;
        border_effective_mu_ = hot->effective_mu;
        border_exit_bound_state_ = hot->exit_bound_state;
        border_exit_active_ineq_ = hot->exit_active_ineq;
        border_kkt_session_id_ = hot->kkt_session_id;
        border_kkt_epoch_ = hot->kkt_epoch;
        border_valid_ = true;
    }

    // HOT-START REUSE bookkeeping (border mode only). Snapshot whatever
    // the previous solve() call left behind, then pessimistically
    // invalidate the persisted cache for the ENTIRE duration of this
    // call, so a failed or thrown solve can never leave a poisoned cache
    // a later solve() mistakes for reusable state. Re-armed only at the
    // very bottom of this function, on a clean kOptimal exit.
    const bool prev_border_valid = border_valid_;
    const std::uint64_t prev_border_structural_hash = border_structural_hash_;
    const std::uint64_t prev_border_values_hash = border_values_hash_;
    const double prev_border_effective_delta = border_effective_delta_;
    const double prev_border_effective_mu = border_effective_mu_;
    // This engine's own last-trusted (session_id, epoch) pair for whatever
    // object `border_` currently names (possibly just adopted above).
    // Compared at the reuse gate below against that object's OWN live
    // identity, so a mutation any OTHER holder made to the shared object
    // is detected even though every problem-shaped fingerprint matches.
    const std::uint64_t prev_border_kkt_session_id = border_kkt_session_id_;
    const std::uint64_t prev_border_kkt_epoch = border_kkt_epoch_;
    border_valid_ = false;

    // Resolve this solve's SolveOverrides against opts_ ONCE, into the
    // effective values every tr_radius/primal_delta/dual_mu read site
    // below consults from here on. `eff_opts` is opts_ verbatim except
    // for these three fields.
    //
    // VALIDATE FIRST. tr_radius must be the +inf sentinel or >= 0: a
    // negative, non-sentinel Delta silently crosses lo_eff/up_eff, and
    // the assert guarding section 6's CROSSED EFFECTIVE BOUNDS CANNOT
    // HAPPEN proof is compiled OUT under NDEBUG. primal_delta/dual_mu
    // keep their negative-means-sentinel convention (qp_types.h); only
    // NaN is rejected for them. The SAME domain is enforced on
    // opts_.tr_radius whenever the +inf sentinel selects it.
    QpOptions eff_opts = resolve_effective_options(opts_, overrides);

    qp_in.validate();
    const Index n = qp_in.n();
    const Index me = qp_in.me();
    const Index mi = qp_in.mi();

    // Step 0: an empty box (lower(i) > upper(i) beyond feas_tol), checked
    // before the working set / start point are even built;
    // `crossed_bounds` gates the main loop below and the shared
    // finalization tail handles the rest. Always against the caller's
    // REAL bounds: a trust region cannot manufacture a crossed bound, and
    // start_center()'s clamp needs a real box regardless of tr_radius.
    bool crossed_bounds = false;
    for (Index i = 0; i < n; ++i) {
        if (qp_in.lower(i) > qp_in.upper(i) + eff_opts.feas_tol) {
            crossed_bounds = true;
            break;
        }
    }

    WorkingSet ws(n, mi);
    // Step 1a ONLY: the clamped seed primal. The seed's WORKING SET is
    // ingested further down, after section 6 has computed the window --
    // see start_center/ingest_seed_working_set and section 6's
    // WINDOW-CONSISTENCY RULE.
    Vec x = start_center(qp_in, seed, ws);

    // --- Section 6: trust-region effective bounds (see header contract) ---
    //
    // tr_active is materialized unconditionally (QpSolution's contract:
    // size n, always) but everything ELSE here -- the two Vecs and the
    // QpProblem copy -- is skipped entirely when tr_radius is +inf, which
    // is what makes that path bit-identical to a trust-region-free solve.
    std::vector<bool> tr_active(static_cast<std::size_t>(n), false);
    QpProblem tr_problem;
    const bool tr_enabled = std::isfinite(eff_opts.tr_radius) && !crossed_bounds;
    if (tr_enabled) {
        // TODO(perf): this deep-copies H/Ae/Ai/g/be/bi every solve purely
        // to get a place to put lo_eff/up_eff, none of which ever differ
        // from qp_in's. Cheaper directions if it ever shows up in a
        // profile: thread lo_eff/up_eff as separate parameters into the
        // functions that read bounds, or give the engine a persistent
        // scratch QpProblem refilled in place.
        tr_problem = qp_in;
        Vec lo_eff(n);
        Vec up_eff(n);
        for (Index i = 0; i < n; ++i) {
            lo_eff(i) = std::max(qp_in.lower(i), x(i) - eff_opts.tr_radius);
            up_eff(i) = std::min(qp_in.upper(i), x(i) + eff_opts.tr_radius);
            // start_center() clamped x(i) into [qp_in.lower(i),
            // qp_in.upper(i)] just above, so lo_eff(i) <= x(i) <=
            // up_eff(i) always -- crossed effective bounds cannot happen
            // by construction (see section 6).
            assert(lo_eff(i) <= up_eff(i));
            if (lo_eff(i) == up_eff(i) &&
                ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFixed) {
                // Delta == 0 (or a start point already pinned exactly
                // Delta from one side with the other just as tight):
                // pins x(i) here as a temporary vertex of one, exactly
                // as a genuinely fixed real bound would, and TR-caused,
                // so tr_active is set. A variable already kFixed from a
                // real lower(i) == upper(i) is left untouched above.
                ws.bound_state()[static_cast<std::size_t>(i)] = BoundState::kFixed;
                x(i) = lo_eff(i);
                tr_active[static_cast<std::size_t>(i)] = true;
            }
        }
        tr_problem.lower = std::move(lo_eff);
        tr_problem.upper = std::move(up_eff);
    }
    // Shadows qp_in for the rest of this function: every qp.lower(i)/
    // qp.upper(i) read below -- here or through eqp_candidate into
    // kkt_assembly.h/eqp_solve.h/bordered_eqp.h -- sees the effective
    // bounds. That is the entire implementation of section 6's "TR bounds
    // participate exactly like real bounds".
    const QpProblem &qp = tr_enabled ? tr_problem : qp_in;

    // Step 1b: NOW ingest the seed's working set, against the effective
    // bounds just computed -- deliberately after section 6, so a seeded
    // pin can never move the center the window was built around; a pin
    // outside that window is dropped (WINDOW-CONSISTENCY RULE). With
    // tr_enabled == false this is bit-identical to ingesting in step 1.
    ingest_seed_working_set(qp_in, qp, seed, ws, x);

    // Row magnitudes of Ai/Ae, used to scale the feasibility tolerances.
    Vec ai_row_norm1 = Vec::Zero(mi);
    for (Index j = 0; j < mi; ++j) {
        ai_row_norm1(j) = qp.Ai.row(j).cwiseAbs().sum();
    }
    Vec ae_row_norm1 = Vec::Zero(me);
    for (Index j = 0; j < me; ++j) {
        ae_row_norm1(j) = qp.Ae.row(j).cwiseAbs().sum();
    }

    Vec lambda_e = Vec::Zero(me);
    Vec lambda_i = Vec::Zero(mi);
    Vec z = Vec::Zero(n);

    Vec shift = Vec::Zero(mi);
    Vec Aix = Vec::Zero(mi);

    // `counters` is declared BEFORE the seeding refresh_shifts() below
    // because that call is itself a source of counters.shift_adds, which
    // QpCounters documents as included. Nothing else reads it this early.
    detail::KktFactor kkt;
    QpCounters counters;
    WalkSeen seen{std::vector<std::uint8_t>(static_cast<std::size_t>(mi), 0),
                  std::vector<std::uint8_t>(static_cast<std::size_t>(n), 0)};

    refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);

    // Decide whether border_ may be trusted for THIS solve, per the header
    // contract's HOT-START REUSE conditions (a)-(e). `ws` here is exactly
    // the seed working set condition (b) is stated over: start_center()
    // plus ingest_seed_working_set() plus the refresh_shifts() above. On
    // any failed condition border_ is DETACHED onto a brand-new
    // BorderState -- see the detach comment below.
    //
    // current_structural_hash/current_values_hash are computed at most
    // ONCE per solve() call and reused verbatim by the exit-commit block
    // near the end of this function; each hash walks all of H/Ae/Ai.
    std::uint64_t current_structural_hash = 0;
    std::uint64_t current_values_hash = 0;
    if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
        current_structural_hash = detail::structural_hash(qp);
        current_values_hash = detail::values_hash(qp);
        // Condition (d): the EFFECTIVE (primal_delta, dual_mu) pair this
        // solve resolved to must match the previous trustworthy solve's.
        //
        // Condition (e), the factor-identity conjunct: `border_`'s
        // factor's own live (session_id, epoch) must equal what THIS
        // engine last saw as trustworthy for whatever object `border_`
        // currently names. Conditions (a)-(d) describe the PROBLEM, never
        // WHICH OBJECT `border_` names, and only a live read on every
        // solve() call catches a different engine having mutated a shared
        // object since.
        //
        // The USABLE-NUMERICS conjunct: an epoch does not advance on a
        // FAILED factorize, so a stale pair can still MATCH an object
        // whose numerics are invalidated. `inertia().state == kObserved`
        // closes that gap.
        const bool reuse_eligible =
            prev_border_valid && current_structural_hash == prev_border_structural_hash &&
            current_values_hash == prev_border_values_hash &&
            eff_opts.primal_delta == prev_border_effective_delta &&
            eff_opts.dual_mu == prev_border_effective_mu &&
            ws.bound_state() == border_exit_bound_state_ &&
            ws.active_ineq() == border_exit_active_ineq_ &&
            border_->kkt.factor.session_id() == prev_border_kkt_session_id &&
            border_->kkt.factor.epoch() == prev_border_kkt_epoch &&
            border_->kkt.factor.inertia().state == hven::linear::InertiaEvidence::State::kObserved;
        // The direct "did the gate pass" signal -- see
        // QpCounters::k0_reused's own note for why a caller must read THIS
        // rather than infer reuse from `factorizations == 0`.
        counters.k0_reused = reuse_eligible;
        if (!reuse_eligible) {
            // DETACH rather than wipe in place: `border_` may be the
            // SAME shared object another engine still holds, and the next
            // rebuild_k0() would refactorize THAT SHARED OBJECT for THIS
            // engine's problem, silently destroying what the other holder
            // relies on. DETACH ONLY WHEN ACTUALLY SHARED, since a fresh
            // BorderState pays a full symbolic re-analysis on every
            // value-changing major.
            //
            // `border_.use_count()` is exactly the discriminator the
            // correctness argument needs (HotState's OWNERSHIP note): a
            // shared object may only ever be safely mutated by an engine
            // whose OWN gate just passed for it (never true in this
            // branch). Reliable here only under the header contract's
            // THREAD SAFETY note (sequential use).
            if (border_.use_count() > 1) {
                border_ = std::make_shared<BorderState>();
            } else {
                border_->schur.reset();
                border_->latched = false;
                border_->k0_rows.clear();
                border_->ledger.clear();
            }
        }
    }

    // Section 4c: the curvature threshold, computed once (hessian_scale
    // walks H's stored nonzeros) and the record of what the previous
    // iteration's drop rule released, which arms the check.
    const double curvature_tol = detail::kCurvatureTolFactor * detail::hessian_scale(qp);
    DropRecord last_drop;

    // Section 4b's zero-multiplier probe: working-set addition order, and
    // the per-solve exemption set that bounds it. Both are per-SOLVE, not
    // per-engine -- a warm re-solve starts with a clean exemption set.
    ProbeState probe(n, mi);

    // Section 4b's POST-PROBE RESTART, whose whole content is these two
    // bits: `probe_drop_made` ARMS it (only a probe-driven drop can leave
    // the loop in the dead end it answers) and `post_probe_restart_spent`
    // CONSUMES it, once per solve. Per-solve for the same reason the
    // exemption set is: a warm re-solve is a new caller decision and gets
    // its own budget.
    bool probe_drop_made = false;
    bool post_probe_restart_spent = false;

    // Observation only (see QpCounters::degenerate_run_max): the length of
    // the degenerate-step run in progress. A run is broken by any minor on
    // which THE ITERATE MOVED -- reset in exactly three places, and the
    // omissions are deliberate: an ordinary non-degenerate step, a taken
    // RIDE, and a START REPAIR (all of which move x). NOT reset by a drop
    // iteration or a zero-multiplier probe: both snap x onto an EQP point
    // already within step_tol of where it was.
    Index degenerate_run = 0;

    // The cap this solve runs at (detail::effective_qp_max_iter). Resolved
    // off `qp_in` (the caller's problem, real bounds) rather than `qp`
    // (the trust-region-narrowed one), deliberately.
    const Index eff_max_iter = detail::effective_qp_max_iter(qp_in, opts_.max_iter);

    QpStatus status = crossed_bounds ? QpStatus::kInfeasible : QpStatus::kMaxIter;
    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {
        ++counters.minor_iters;

        // Stamp addition numbers on whatever joined the working set since
        // the last iteration, BEFORE anything this iteration changes it,
        // so the record reflects the order the loop actually built the
        // working set in (section 4b's zero-multiplier probe reads it).
        probe.refresh(ws);

        detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
        const EqpResult eqp = eqp_candidate(qp, ws, kkt, *border_, counters, verdict, eff_opts);

        // INERTIA GATE / TEMPORARY-VERTEX START REPAIR (section 4b). Only
        // at the start: a trustworthy wrong inertia here means the seed
        // working set is second-order inconsistent and the EQP result
        // just computed is a saddle/maximizer, so it is discarded and the
        // iteration retried from the repaired vertex. kSuspect is
        // deliberately NOT a repair trigger, and a wrong inertia LATER is
        // section 4c's ride to handle.
        if (iter == 0 && verdict == detail::InertiaVerdict::kWrong &&
            repair_temporary_vertex(qp, ws, x, kkt, *border_, counters, eff_opts)) {
            degenerate_run = 0; // observation only -- x moved onto bounds
            // Pinning moved x onto bounds, which can violate general
            // rows; the homotopy picks them up exactly as at a start
            // point.
            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
            continue;
        }

        price(qp, ws, eqp, lambda_e, lambda_i, z);

        const Vec p = eqp.x - x;

        // Whatever the previous iteration's drop rule released. Consumed
        // here whether or not the ride below fires, so it can never arm an
        // iteration that did not follow a drop.
        const DropRecord dropped = last_drop;
        last_drop = DropRecord{};

        const double step_tol = eff_opts.feas_tol * std::max(1.0, x.lpNorm<Eigen::Infinity>());
        if (p.lpNorm<Eigen::Infinity>() <= step_tol) {
            // A KKT point of the current working set: snap onto it, then
            // let the multipliers decide.
            x = eqp.x;
            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
            if (!drop_worst(qp, shift, lambda_i, z, ws, last_drop, counters)) {
                // Nothing left to leave the working set, so this point is
                // final. Classify it: a STRUCTURAL violation in either
                // block, else a runaway free component, else a TRUSTED
                // wrong inertia (a saddle or maximizer, not an optimum).
                //
                // ORDERING CAVEAT ON `verdict`: it was computed by
                // eqp_candidate at the TOP of this iteration, against the
                // working set BEFORE the refresh_shifts above, which can
                // ADD rows. Conservative in the direction that matters
                // (adding rows only shrinks the null space, so a kOk read
                // cannot become kWrong), but a kWrong read can be one
                // iteration stale.
                if (worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1,
                                               lambda_e, eff_opts) > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {
                    status = QpStatus::kNumericalError;
                } else if (verdict == detail::InertiaVerdict::kWrong) {
                    // THE POST-PROBE RESTART (section 4b). Reaching here
                    // after a probe-driven drop means the drop exposed
                    // real negative curvature but its multiplier was zero,
                    // so 4c's arming direction p is identically zero and
                    // the ride is never offered anything. The
                    // temporary-vertex repair (normally gated on
                    // iter == 0) is spent ONE time here instead, after
                    // every cheaper way out has been exhausted; if it
                    // fails, or the budget is gone, the original verdict
                    // stands.
                    //
                    // `probe_drop_made` is STICKY PER SOLVE rather than
                    // per drop, so a later dead end from a drop_worst drop
                    // whose ride declined can also spend the restart --
                    // wider than the narrowest intent, sound either way.
                    if (probe_drop_made && !post_probe_restart_spent &&
                        repair_temporary_vertex(qp, ws, x, kkt, *border_, counters, eff_opts)) {
                        post_probe_restart_spent = true;
                        // Pinning moved x onto bounds, exactly as at the
                        // start-of-solve repair: general rows may now be
                        // violated and the homotopy picks them up.
                        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters,
                                       seen, eff_opts);
                        continue;
                    }
                    status = QpStatus::kNumericalError;
                } else {
                    // Everything above passed, so this is a would-be
                    // kOptimal exit -- the last place a WEAKLY ACTIVE
                    // constraint can still be hiding the critical cone
                    // from the gate. refresh_shifts above may have just
                    // added rows, so the addition record is brought up to
                    // date before probing (section 4b's ZERO-MULTIPLIER
                    // PROBE).
                    probe.refresh(ws);
                    if (probe_zero_multiplier_drops(qp, shift, lambda_i, z, ws, kkt, *border_,
                                                    counters, probe, last_drop, eff_opts)) {
                        // Negative curvature the full labeling hid: the
                        // drop is real. It arms the POST-PROBE RESTART
                        // above, which is what actually answers this case
                        // -- 4c's ride is vacuous for a zero-multiplier
                        // drop.
                        probe_drop_made = true;
                        continue;
                    }
                    // SECTION 4b's SUSPECT-STALL GATE. `verdict` describes
                    // the system this iterate was actually solved from, so
                    // a kSuspect here means the numbers that produced this
                    // point came out of a factorization with a fabricated
                    // or dropped pivot. Certify only after checking the QP
                    // model's own first-order condition on the free block.
                    double stat_scale = 1.0;
                    const double free_stat = verdict == detail::InertiaVerdict::kSuspect
                                                 ? detail::free_block_stationarity(
                                                       qp, x, ws, lambda_e, lambda_i, stat_scale)
                                                 : 0.0;
                    // Asks "is it stationary" and NEGATES, rather than
                    // asking "is it a violation": see
                    // free_block_is_stationary for why the two are not
                    // interchangeable when the residual is NaN.
                    if (!detail::free_block_is_stationary(free_stat, stat_scale, eff_opts)) {
                        if (counters.suspect_escalations < detail::kMaxSuspectEscalations) {
                            // Half (i): change the system rather than
                            // re-solve it. Discarding the border state is
                            // what makes the new primal_delta reach K0 --
                            // the same use_count()-guarded DETACH-or-wipe
                            // the hot-start ineligibility branch above
                            // uses. Needed because kSuspect has sources
                            // border_candidate does not already rebuild on
                            // (counts that do not sum to n, a border stack
                            // past needs_refactorization); without it the
                            // escalated delta would never reach K0 there.
                            ++counters.suspect_escalations;
                            eff_opts.primal_delta *= detail::kSuspectDeltaFactor;
                            if (border_.use_count() > 1) {
                                border_ = std::make_shared<BorderState>();
                            } else {
                                border_->schur.reset();
                                border_->latched = false;
                                border_->k0_rows.clear();
                                border_->ledger.clear();
                            }
                            continue;
                        }
                        // Ladder exhausted. Same verdict and semantics as
                        // the runaway/saddle exits: report the iterate,
                        // clear the multipliers, and let
                        // counters.suspect_escalations carry the reason.
                        status = QpStatus::kNumericalError;
                    } else {
                        status = QpStatus::kOptimal;
                    }
                }
                break;
            }
            continue;
        }

        // NEGATIVE-CURVATURE RIDE (section 4c). Armed only on the
        // iteration that follows a drop, and only for a step the loop
        // considers real: on a NEGLIGIBLE p (handled above) the direction
        // is rounding noise and section 4b's certification branch is the
        // right judge.
        if (dropped.active) {
            const double curvature =
                p.dot(qp.H.selfadjointView<Eigen::Upper>() * p) / p.squaredNorm();
            if (curvature <= curvature_tol) {
                // The EQP just solved is unbounded below along +/-p within
                // this working set, so its "solution" is a saddle. Ride
                // the direction instead -- unless the ride declines, in
                // which case fall through to the ordinary step.
                // p.squaredNorm() is nonzero here: the negligible case
                // returned above.
                const RideOutcome outcome =
                    ride_negative_curvature(qp, dropped, counters, seen, p, Aix, ws, x);
                if (outcome == RideOutcome::kUnbounded) {
                    // Nothing blocks: genuinely unbounded. Same verdict and
                    // same semantics (final iterate reported, multipliers
                    // cleared below) as section 4b's certification branch.
                    status = QpStatus::kNumericalError;
                    break;
                }
                if (outcome == RideOutcome::kStepped) {
                    degenerate_run = 0; // observation only -- the ride moved x
                    refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                                   eff_opts);
                    continue;
                }
            }
        }

        BlockKind kind = BlockKind::kNone;
        Index block_idx = -1;
        BoundState block_state = BoundState::kFree;
        const double alpha = ratio_test(qp, ws, x, Aix, p, kind, block_idx, block_state,
                                        /*cap_at_unit=*/true, counters);

        // Observation only (see QpCounters' own note): a blocking add
        // whose step moved the iterate by no more than the SAME step_tol
        // the KKT-point test above used is a degenerate pivot. Read
        // before x is overwritten, since ||p||inf is the pre-step
        // direction's magnitude.
        if (kind != BlockKind::kNone && alpha * p.lpNorm<Eigen::Infinity>() <= step_tol) {
            ++counters.degenerate_steps;
            ++degenerate_run;
            counters.degenerate_run_max = std::max(counters.degenerate_run_max, degenerate_run);
        } else {
            degenerate_run = 0;
        }
        if (kind != BlockKind::kNone) {
            ++counters.ws_adds;
            if (kind == BlockKind::kBound) {
                ++counters.ws_adds_bound;
                mark_bound_seen(seen, counters, block_idx);
            } else {
                mark_ineq_seen(seen, counters, block_idx);
            }
        }

        x = (alpha >= 1.0) ? eqp.x : Vec(x + alpha * p);
        if (kind == BlockKind::kIneq) {
            ws.add_ineq(block_idx);
        } else if (kind == BlockKind::kBound) {
            ws.bound_state()[static_cast<std::size_t>(block_idx)] = block_state;
            x(block_idx) =
                (block_state == BoundState::kAtUpper) ? qp.upper(block_idx) : qp.lower(block_idx);
        }
        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
    }

    // Commit the hot-start reuse cache iff this solve reached a genuine KKT
    // point (INVALIDATION POLICY). kMaxIter/kInfeasible/kNumericalError
    // all leave border_valid_ false: border_ may still hold a good K0
    // factorization, but the working set that produced it was never
    // certified. Reuses current_structural_hash/current_values_hash
    // computed above rather than re-hashing H/Ae/Ai.
    if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder && status == QpStatus::kOptimal) {
        border_structural_hash_ = current_structural_hash;
        border_values_hash_ = current_values_hash;
        // Condition (d): commit the EFFECTIVE pair this solve resolved
        // to, not opts_'s own primal_delta/dual_mu -- see the header
        // contract's HOT-START REUSE note and this function's top.
        border_effective_delta_ = eff_opts.primal_delta;
        border_effective_mu_ = eff_opts.dual_mu;
        border_exit_bound_state_ = ws.bound_state();
        border_exit_active_ineq_ = ws.active_ineq();
        // Condition (e): commit `border_`'s factor's OWN live (session_id,
        // epoch) identity as of THIS solve's exit, whether unchanged
        // (reuse granted) or freshly advanced (a rebuild_k0 ran). The
        // NEXT solve's reuse gate compares this snapshot against a live
        // re-read, which is what notices another holder rebuilding the
        // object.
        border_kkt_session_id_ = border_->kkt.factor.session_id();
        border_kkt_epoch_ = border_->kkt.factor.epoch();
        border_valid_ = true;
    }

    // On the infeasible and runaway paths the multipliers are pure
    // regularization artifacts of size ~1/dual_mu (1e8 at the defaults)
    // that mean nothing. Report the final iterate -- the caller's evidence
    // of what could not be satisfied -- but do not let those numbers leak
    // out as if they were prices.
    if (status == QpStatus::kInfeasible || status == QpStatus::kNumericalError) {
        lambda_e.setZero();
        lambda_i.setZero();
        z.setZero();
    }

    // THE EXPORT INVARIANT: A FREE VARIABLE CARRIES NO BOUND PRICE.
    // `price()` is the sole producer of `z` and already imposes this rule,
    // but it runs at the TOP of a minor iteration while the working set
    // can still be mutated afterward (drop_worst, the zero-multiplier
    // probe, the ratio test). On a converging exit the last mutation is
    // always followed by another `price()`; on a `kMaxIter` exit it is
    // not. Re-imposing the rule at the point of export rather than
    // re-labelling `bound_state` is the direction that ADDS no
    // information -- it cannot invent activity -- and `bound_state` is
    // read far more widely.
    //
    // UNCONDITIONAL rather than `if (status == kMaxIter)`: the invariant
    // is a property of the EXPORT, not of a status. Runs BEFORE section
    // 6's TR exclusion, which only clears MORE of `z`.
    for (Index i = 0; i < n; ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
            z(i) = 0.0;
        }
    }

    // Section 6's reporting exclusions, against the FINAL working set only:
    // finalize tr_active for whichever side a bound ended up pinned at
    // (the zero-radius kFixed flip already set its own indices), then
    // report bound_state kFree and z == 0 for every TR-pinned index.
    // border_exit_bound_state_ above deliberately read ws.bound_state()
    // BEFORE this rewrite: the reuse ledger must see the real, internal
    // pin state, not this caller-facing view.
    std::vector<BoundState> reported_bound_state = ws.bound_state();
    if (tr_enabled) {
        for (Index i = 0; i < n; ++i) {
            const auto si = static_cast<std::size_t>(i);
            const BoundState st = reported_bound_state[si];
            if (st == BoundState::kAtLower) {
                tr_active[si] = tr_problem.lower(i) > qp_in.lower(i);
            } else if (st == BoundState::kAtUpper) {
                tr_active[si] = tr_problem.upper(i) < qp_in.upper(i);
            }
            if (tr_active[si]) {
                reported_bound_state[si] = BoundState::kFree;
                z(si) = 0.0;
            }
        }
    }

    QpSolution out;
    out.status = status;
    out.x = std::move(x);
    out.lambda_e = std::move(lambda_e);
    out.lambda_i = std::move(lambda_i);
    out.z = std::move(z);
    out.bound_state = std::move(reported_bound_state);
    out.tr_active = std::move(tr_active);
    out.ineq_active.assign(static_cast<std::size_t>(mi), false);
    for (Index row : ws.active_ineq()) {
        out.ineq_active[static_cast<std::size_t>(row)] = true;
    }
    out.counters = counters;

    // Emit a record to the ledger if attached.
    if (ledger_ != nullptr) {
        SolveRecord rec;
        rec.label = label_prefix_ + "_" + std::to_string(solve_counter_);
        rec.warm = warm;
        rec.status = status;
        rec.counters = counters;
        ledger_->record(rec);
        ++solve_counter_;
    }

    return out;
}

Vec QpEngine::start_center(const QpProblem &qp, const QpSolution *seed, WorkingSet &ws) {
    const Index n = qp.n();
    const bool seed_x = seed != nullptr && seed->x.size() == n;

    Vec x(n);
    for (Index i = 0; i < n; ++i) {
        const double lo = qp.lower(i);
        const double up = qp.upper(i);
        x(i) = std::min(std::max(seed_x ? seed->x(i) : 0.0, lo), up);
        if (lo == up) {
            ws.bound_state()[static_cast<std::size_t>(i)] = BoundState::kFixed;
            x(i) = lo;
        }
    }
    return x;
}

void QpEngine::ingest_seed_working_set(const QpProblem &qp_real, const QpProblem &qp_eff,
                                       const QpSolution *seed, WorkingSet &ws, Vec &x) {
    const Index n = qp_real.n();
    const Index mi = qp_real.mi();
    const bool seed_bounds = seed != nullptr && static_cast<Index>(seed->bound_state.size()) == n;
    const bool seed_rows = seed != nullptr && static_cast<Index>(seed->ineq_active.size()) == mi;

    if (seed_bounds) {
        for (Index i = 0; i < n; ++i) {
            const auto si = static_cast<std::size_t>(i);
            // Already pinned by something this function may not override:
            // a genuinely fixed real bound, or section 6's zero-width
            // window flip.
            if (ws.bound_state()[si] == BoundState::kFixed) {
                continue;
            }
            const bool at_lower = seed->bound_state[si] == BoundState::kAtLower;
            const bool at_upper = seed->bound_state[si] == BoundState::kAtUpper;
            if (!at_lower && !at_upper) {
                continue;
            }
            const double bound = at_lower ? qp_real.lower(i) : qp_real.upper(i);
            if (at_lower ? !(bound > -detail::kEngineInfBound)
                         : !(bound < detail::kEngineInfBound)) {
                continue; // no real bound on that side to pin at
            }
            if (bound < qp_eff.lower(i) || bound > qp_eff.upper(i)) {
                continue; // WINDOW-CONSISTENCY RULE: outside the window, dropped
            }
            ws.bound_state()[si] = at_lower ? BoundState::kAtLower : BoundState::kAtUpper;
            x(i) = bound;
        }
    }
    if (seed_rows) {
        for (Index r = 0; r < mi; ++r) {
            if (seed->ineq_active[static_cast<std::size_t>(r)]) {
                ws.add_ineq(r);
            }
        }
    }
}

void QpEngine::mark_ineq_seen(WalkSeen &seen, QpCounters &counters, Index j) {
    auto &flag = seen.ineq[static_cast<std::size_t>(j)];
    if (flag == 0) {
        flag = 1;
        ++counters.distinct_ineq_added;
    }
}

void QpEngine::mark_bound_seen(WalkSeen &seen, QpCounters &counters, Index i) {
    auto &flag = seen.bound[static_cast<std::size_t>(i)];
    if (flag == 0) {
        flag = 1;
        ++counters.distinct_bound_added;
    }
}

void QpEngine::refresh_shifts(const QpProblem &qp, const Vec &x, const Vec &ai_row_norm1,
                              const Vec &lambda_i, WorkingSet &ws, Vec &Aix, Vec &shift,
                              QpCounters &counters, WalkSeen &seen, const QpOptions &opts) const {
    const Index mi = qp.mi();
    if (mi == 0) {
        return;
    }
    Aix = qp.Ai * x;
    const double xmag = x.lpNorm<Eigen::Infinity>();
    for (Index j = 0; j < mi; ++j) {
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        const double s = Aix(j) - qp.bi(j);
        shift(j) = (s > row_tolerance(row_scale, lambda_i(j), opts)) ? s : 0.0;
        if (shift(j) > 0.0 && !in_working(ws, j)) {
            ws.add_ineq(j);
            ++counters.shift_adds; // observation only -- see QpCounters
            mark_ineq_seen(seen, counters, j);
        }
    }
}

double QpEngine::row_tolerance(double row_scale, double lambda, const QpOptions &opts) const {
    return std::max(
        opts.feas_tol * row_scale,
        std::min(opts.dual_mu * std::abs(lambda), detail::kInfeasibilityAbsorbTol * row_scale));
}

bool QpEngine::violation_is_structural(double v, double row_scale, double lambda,
                                       const QpOptions &opts) const {
    if (v <= detail::kInfeasibilityMarginFactor * row_tolerance(row_scale, lambda, opts)) {
        return false; // (a) within tolerance, nothing to explain
    }
    return v >= detail::kStructuralResidualFrac * opts.dual_mu * std::abs(lambda);
}

double QpEngine::worst_structural_violation(const QpProblem &qp, const Vec &x, const Vec &Aix,
                                            const Vec &ai_row_norm1, const Vec &lambda_i,
                                            const Vec &ae_row_norm1, const Vec &lambda_e,
                                            const QpOptions &opts) const {
    const double xmag = x.lpNorm<Eigen::Infinity>();
    double worst = 0.0;
    for (Index j = 0; j < qp.mi(); ++j) {
        const double v = Aix(j) - qp.bi(j);
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        if (v > 0.0 && violation_is_structural(v, row_scale, lambda_i(j), opts)) {
            worst = std::max(worst, v);
        }
    }
    if (qp.me() > 0) {
        const Vec resid = qp.Ae * x - qp.be;
        for (Index j = 0; j < qp.me(); ++j) {
            const double v = std::abs(resid(j));
            const double row_scale = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
            if (violation_is_structural(v, row_scale, lambda_e(j), opts)) {
                worst = std::max(worst, v);
            }
        }
    }
    return worst;
}

bool QpEngine::is_runaway(const QpProblem &qp, const Vec &x, const WorkingSet &ws,
                          const QpOptions &opts) const {
    const double limit = detail::unbounded_artifact_scale(opts);
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            continue; // pinned at a bound: it did not run away
        }
        const bool grew_up = x(i) > 0.0 && qp.upper(i) > limit;
        const bool grew_down = x(i) < 0.0 && qp.lower(i) < -limit;
        if ((grew_up || grew_down) && std::abs(x(i)) > limit) {
            return true;
        }
    }
    return false;
}

bool QpEngine::in_working(const WorkingSet &ws, Index row) {
    const auto &aw = ws.active_ineq();
    return std::binary_search(aw.begin(), aw.end(), row);
}

EqpResult QpEngine::eqp_candidate(const QpProblem &qp, const WorkingSet &ws, detail::KktFactor &kkt,
                                  BorderState &border, QpCounters &counters,
                                  detail::InertiaVerdict &verdict, const QpOptions &opts) const {
    if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
        return border_candidate(qp, ws, kkt, border, counters, verdict, opts);
    }
    return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
}

EqpResult QpEngine::eliminated_candidate(const QpProblem &qp, const WorkingSet &ws,
                                         detail::KktFactor &kkt, QpCounters &counters,
                                         detail::InertiaVerdict &verdict,
                                         const QpOptions &opts) const {
    const bool empty_system = ws.num_free() == 0 && qp.me() == 0 && ws.active_ineq().empty();
    if (empty_system) {
        // Nothing was factorized, and a 0x0 system trivially has the
        // expected (0, 0, 0) inertia: every variable is pinned, so there
        // is no free direction for negative curvature to live in.
        verdict = detail::InertiaVerdict::kOk;
        Vec xf(qp.n());
        for (Index i = 0; i < qp.n(); ++i) {
            xf(i) = (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kAtUpper)
                        ? qp.upper(i)
                        : qp.lower(i);
        }
        return EqpResult{std::move(xf), Vec::Zero(0), Vec::Zero(0)};
    }
    ++counters.factorizations;
    EqpResult res = solve_eqp(qp, ws, kkt, opts);
    // EXTRA steps only, i.e. identically 0 (solver_counters.h): the eliminated path
    // takes its one mandatory step and has no iterated loop.
    counters.eqp_refine_steps += res.refine_steps;
    // solve_eqp leaves `kkt` holding the factorization of the
    // bound-ELIMINATED K, whose expected inertia is (n_free, me +
    // n_working, 0) -- the reduced form of the gate in section 4b.
    verdict = detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(),
                                      qp.me() + static_cast<Index>(ws.active_ineq().size()));
    return res;
}

EqpResult QpEngine::border_candidate(const QpProblem &qp, const WorkingSet &ws,
                                     detail::KktFactor &kkt, BorderState &border,
                                     QpCounters &counters, detail::InertiaVerdict &verdict,
                                     const QpOptions &opts) const {
    // LATCHED: bordering has been abandoned for this working-set shape
    // (see border_solve_or_fall_back). Do not sync -- the whole point is
    // that every border added here would be discarded unused, and adding
    // them anyway is what drove dim() past schur_cap and made the wasted
    // work the dominant cost.
    if (border.latched) {
        if (latch_still_holds(ws)) {
            return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
        }
        // The pin count fell back to schur_cap or below, so bordering is
        // possible again. The stale stack cannot be incrementally repaired
        // (it was never updated, and the working ROWS may have moved
        // arbitrarily far during the latch -- see latch_still_holds), so
        // resume with a clean K0 built from the CURRENT working set.
        border.latched = false;
        rebuild_k0(qp, ws, border, counters, opts);
        sync_borders(qp, ws, border, counters, opts);
        return border_solve_or_fall_back(qp, ws, kkt, border, counters, verdict,
                                         /*rebuilt=*/true, opts);
    }

    // A K0 factorization the backend had to perturb pivots in is
    // untrustworthy whatever the border stack does, so that check comes
    // BEFORE the sync: re-adding borders onto a factorization already
    // known to be discarded is pure waste. This reads the PREVIOUS
    // solve's factorization evidence; `schur.has_value()` implies
    // rebuild_k0 completed. Rebuilds on non-kObserved evidence
    // (defensive, unreachable today), on a present-and-nonzero
    // perturbed-pivot count (MKL), or on a NATIVELY-OBSERVED nonzero zero
    // class (Accelerate; inert on MKL, where it is derived).
    bool rebuilt = false;
    bool k0_untrusted = false;
    if (border.schur.has_value()) {
        const hven::linear::InertiaEvidence e = border.kkt.factor.inertia();
        k0_untrusted = e.state != hven::linear::InertiaEvidence::State::kObserved ||
                       (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) ||
                       (!e.zero_is_derived && e.n_zero != 0);
    }
    if (!border.schur.has_value() || k0_untrusted) {
        rebuild_k0(qp, ws, border, counters, opts); // the seed working set's K0
        rebuilt = true;
    }
    sync_borders(qp, ws, border, counters, opts);
    return border_solve_or_fall_back(qp, ws, kkt, border, counters, verdict, rebuilt, opts);
}

EqpResult QpEngine::border_solve_or_fall_back(const QpProblem &qp, const WorkingSet &ws,
                                              detail::KktFactor &kkt, BorderState &border,
                                              QpCounters &counters, detail::InertiaVerdict &verdict,
                                              bool rebuilt, const QpOptions &opts) const {
    if (border.schur->needs_refactorization()) {
        // Rebuilding re-derives K0 from the CURRENT working set, folding
        // the working ROWS that triggered it back into K0. It cannot do
        // the same for PINS: assemble_kkt_full eliminates nothing, so
        // rebuilding when the live borders are all pins over K0's own
        // rows reproduces the identical state -- a permanent trigger that
        // is quadratic in n on a box-shaped QP.
        //
        // So: when a rebuild would be a no-op, LATCH -- serve this and
        // subsequent iterations from the elimination path, where pins ARE
        // eliminable, and stop maintaining a border stack nothing reads.
        if (rebuild_would_be_noop(ws, border)) {
            border.latched = true;
            return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
        }
        // A rebuild that has already been spent this iteration, or one
        // that did not clear the flag, also falls back -- but only latches
        // if the state it left behind is the pins-only dead end.
        if (rebuilt) {
            return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
        }
        rebuild_k0(qp, ws, border, counters, opts);
        sync_borders(qp, ws, border, counters, opts);
        if (border.schur->needs_refactorization()) {
            border.latched = rebuild_would_be_noop(ws, border);
            return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
        }
    }

    // SchurComplement::solve throws std::runtime_error on an exactly
    // singular C. QpEngine::solve owes a QpStatus for every solvable input
    // and must not leak a linear-algebra exception, so a singular factor
    // that slips through the checks above degrades to the elimination path
    // like any other untrustworthy border stack.
    //
    // That must NOT swallow every std::runtime_error: the KKT factor
    // throws the same type for a failed backend factorization and for a
    // solve before any successful factorize, which are genuine faults
    // kRefactorize mode reports and border mode must not hide. The
    // discriminator is the state the throw leaves behind -- a singular C
    // sets SchurComplement's singular_ flag, so needs_refactorization() is
    // necessarily true afterward. If it is not, this was some other
    // failure and it is rethrown untouched.
    try {
        // The gate reads the system that is about to be solved: K0's
        // pardiso inertia plus the live border stack's own contribution.
        // Evaluated BEFORE the solve so a throw below cannot leave a
        // verdict describing a system that was never solved.
        verdict = border_inertia_verdict(qp, border);
        EqpResult res = solve_bordered_eqp(qp, border.k0, border.k0_rows, *border.schur,
                                           border.ledger, ws, opts);
        // TOTAL steps, mandatory first one included (solver_counters.h).
        counters.border_refine_steps += res.refine_steps;
        return res;
    } catch (const std::runtime_error &) {
        if (!border.schur->needs_refactorization()) {
            throw;
        }
        return eliminated_candidate(qp, ws, kkt, counters, verdict, opts);
    }
}

detail::InertiaVerdict QpEngine::border_inertia_verdict(const QpProblem &qp,
                                                        const BorderState &border) const {
    const SchurComplement &schur = *border.schur;
    if (schur.needs_refactorization()) {
        // C's inertia is either unreadable (an exactly singular factor
        // makes expected_neg_eigs_delta() THROW) or not worth reading
        // (past the cap / condition limit). Either way the bordered
        // system's inertia is unknown, not wrong.
        return detail::InertiaVerdict::kSuspect;
    }
    const Index dim = schur.dim();
    const Index neg_c = schur.expected_neg_eigs_delta();
    if (neg_c < 0 || neg_c > dim) {
        return detail::InertiaVerdict::kSuspect;
    }

    // STRUCTURAL expectation for the whole bordered matrix: K0 supplies
    // (n, me + n_w0) and each border supplies one negative eigenvalue,
    // except a kRowDelete, which pairs with the K0 row it kills into an
    // indefinite 2x2 and so supplies one positive and one negative.
    Index deletes = 0;
    for (const BorderLedgerEntry &e : border.ledger) {
        if (e.kind == BorderLedgerEntry::Kind::kRowDelete) {
            ++deletes;
        }
    }
    const Index n_w0 = static_cast<Index>(border.k0_rows.size());
    const Index expected_pos = qp.n() + deletes;
    const Index expected_neg = qp.me() + n_w0 + dim - deletes;

    // ACTUAL inertia = K0's (backend evidence) + C's (Haynsworth).
    // Rather than adding C's contribution to the backend's counts, back
    // it out of the expectation and let the shared helper compare K0's
    // own numbers -- same test, and it keeps the perturbed-pivot policy
    // in one place.
    const Index extra_neg = neg_c;
    const Index extra_pos = dim - neg_c;
    return detail::inertia_verdict(border.kkt.factor.inertia(), expected_pos - extra_pos,
                                   expected_neg - extra_neg);
}

detail::InertiaVerdict QpEngine::probe_inertia(const QpProblem &qp, const WorkingSet &ws,
                                               detail::KktFactor &kkt, BorderState &border,
                                               QpCounters &counters, const QpOptions &opts) const {
    detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
    (void)eqp_candidate(qp, ws, kkt, border, counters, verdict, opts);
    return verdict;
}

bool QpEngine::probe_zero_multiplier_drops(const QpProblem &qp, const Vec &shift,
                                           const Vec &lambda_i, const Vec &z, WorkingSet &ws,
                                           detail::KktFactor &kkt, BorderState &border,
                                           QpCounters &counters, ProbeState &probe,
                                           DropRecord &dropped, const QpOptions &opts) const {
    struct Candidate {
        bool is_ineq = false;
        Index idx = -1;
        Index seq = -1;
    };
    std::vector<Candidate> cands;
    for (const Index row : ws.active_ineq()) {
        const auto sr = static_cast<std::size_t>(row);
        // A row the homotopy is still driving to feasibility is not
        // WEAKLY active -- x does not sit on it at all, so it excludes no
        // direction of the critical cone (the same reason drop_worst skips
        // it), and refresh_shifts would re-add it next iteration anyway.
        if (shift(row) > 0.0) {
            continue;
        }
        if (probe.ineq_exempt[sr] || std::abs(lambda_i(row)) > opts.opt_tol) {
            continue;
        }
        cands.push_back({true, row, probe.ineq_seq[sr]});
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const auto si = static_cast<std::size_t>(i);
        const BoundState st = ws.bound_state()[si];
        // kFixed is lower == upper: there is no feasible direction off it
        // to expose, and the drop rule does not release it either.
        if (st != BoundState::kAtLower && st != BoundState::kAtUpper) {
            continue;
        }
        if (probe.bound_exempt[si] || std::abs(z(i)) > opts.opt_tol) {
            continue;
        }
        cands.push_back({false, i, probe.bound_seq[si]});
    }
    if (cands.empty()) {
        return false;
    }
    // Most recently added first: the newest member of the working set is
    // the one the loop has done the least to justify, and trying it first
    // keeps the common case (one weakly active constraint, just added) to
    // a single probe.
    std::stable_sort(cands.begin(), cands.end(),
                     [](const Candidate &a, const Candidate &b) { return a.seq > b.seq; });

    for (const Candidate &c : cands) {
        const auto si = static_cast<std::size_t>(c.idx);
        const BoundState held = c.is_ineq ? BoundState::kFree : ws.bound_state()[si];
        if (c.is_ineq) {
            ws.drop_ineq(c.idx);
        } else {
            ws.bound_state()[si] = BoundState::kFree;
        }

        const detail::InertiaVerdict verdict = probe_inertia(qp, ws, kkt, border, counters, opts);
        if (verdict == detail::InertiaVerdict::kWrong) {
            dropped = DropRecord{};
            dropped.active = true;
            dropped.is_ineq = c.is_ineq;
            dropped.idx = c.idx;
            dropped.from = held;
            // Observation only (see QpCounters): counted HERE, on the
            // branch that KEEPS the drop, never on the restore path
            // below -- a probed-and-restored constraint leaves the
            // working set unchanged and counting it would break the
            // net identity QpCounters states.
            ++counters.ws_drops;
            if (!c.is_ineq) {
                ++counters.ws_drops_bound;
            }
            // The exemption is keyed by CONSTRAINT IDENTITY -- an Ai row
            // index, or a VARIABLE index for a bound -- never by which
            // SIDE a bound was pinned at, so a variable dropped from
            // kAtLower and later re-pinned at kAtUpper inherits it.
            // Deliberately conservative: the two sides share the one
            // direction e_i that a drop frees, so the exemption costs at
            // most a missed drop, never a wrong one, while erring the
            // other way would put an alternating lower/upper pin back
            // inside the cycle this set exists to break.
            if (c.is_ineq) {
                probe.ineq_exempt[si] = true;
            } else {
                probe.bound_exempt[si] = true;
            }
            return true;
        }
        // kOk or kSuspect: put back the CONSTRAINT. Border-mode state is
        // deliberately left as the probe left it -- both `border.ledger`
        // (now describing the REDUCED working set) and `border.latched`.
        // Neither is restored, and neither needs to be: both are DERIVED
        // views that border_candidate reconciles against `ws`
        // unconditionally on its next call (sync_borders re-adds what this
        // restore put back; latch_still_holds re-decides from the current
        // pin count). The only observable consequence is a few extra
        // schur_updates.
        if (c.is_ineq) {
            ws.add_ineq(c.idx);
        } else {
            ws.bound_state()[si] = held;
        }
    }
    return false;
}

Vec QpEngine::hessian_diagonal(const QpProblem &qp) {
    Vec d = Vec::Zero(qp.n());
    for (Index i = 0; i < qp.n(); ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            if (it.col() == i) {
                d(i) = it.value();
            }
        }
    }
    return d;
}

bool QpEngine::pin_at_best_bound(const QpProblem &qp, WorkingSet &ws, Vec &x, const Vec &grad,
                                 Index i, double hii) const {
    const double lo = qp.lower(i);
    const double up = qp.upper(i);
    const bool lo_ok = lo > -detail::kEngineInfBound;
    const bool up_ok = up < detail::kEngineInfBound;
    if (!lo_ok && !up_ok) {
        return false;
    }
    const auto si = static_cast<std::size_t>(i);
    const double tol = opts_.feas_tol * std::max(1.0, std::abs(x(i)));
    // Already sitting on a bound: pin it exactly where it is, so a warm
    // start's own vertex is never disturbed by the repair.
    if (lo_ok && std::abs(x(i) - lo) <= tol) {
        ws.bound_state()[si] = BoundState::kAtLower;
        x(i) = lo;
        return true;
    }
    if (up_ok && std::abs(x(i) - up) <= tol) {
        ws.bound_state()[si] = BoundState::kAtUpper;
        x(i) = up;
        return true;
    }
    // Otherwise take the bound that lowers the objective more along e_i.
    auto delta_f = [&](double bound) {
        const double t = bound - x(i);
        return grad(i) * t + 0.5 * hii * t * t;
    };
    const bool take_lower =
        lo_ok && (!up_ok || delta_f(lo) <= delta_f(up)); // ties -> lower, deterministically
    ws.bound_state()[si] = take_lower ? BoundState::kAtLower : BoundState::kAtUpper;
    x(i) = take_lower ? lo : up;
    return true;
}

bool QpEngine::repair_temporary_vertex(const QpProblem &qp, WorkingSet &ws, Vec &x,
                                       detail::KktFactor &kkt, BorderState &border,
                                       QpCounters &counters, const QpOptions &opts) const {
    const Index n = qp.n();
    const Vec hdiag = hessian_diagonal(qp);

    // Most negative H diagonal first -- that is where the negative
    // curvature is concentrated -- with index order breaking ties, so the
    // repair is deterministic. stable_sort over an already index-ordered
    // list gives that tie-break for free.
    std::vector<Index> order;
    for (Index i = 0; i < n; ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
            order.push_back(i);
        }
    }
    std::stable_sort(order.begin(), order.end(),
                     [&](Index a, Index b) { return hdiag(a) < hdiag(b); });

    const std::vector<BoundState> saved_state = ws.bound_state();
    const Vec saved_x = x;

    std::vector<Index> pinned;
    detail::InertiaVerdict verdict = detail::InertiaVerdict::kWrong;
    for (const Index i : order) {
        Vec grad = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
        if (!pin_at_best_bound(qp, ws, x, grad, i, hdiag(i))) {
            continue; // unbounded both ways: nothing to pin against
        }
        pinned.push_back(i);
        verdict = probe_inertia(qp, ws, kkt, border, counters, opts);
        if (verdict != detail::InertiaVerdict::kWrong) {
            break; // repaired, or no longer verifiable
        }
    }
    if (verdict != detail::InertiaVerdict::kOk) {
        // Either we ran out of variables to pin, or the gate stopped being
        // readable partway through (kSuspect). Neither leaves us able to
        // certify the repaired working set, so undo it entirely. The
        // border stack is left alone: sync_borders reconciles it against
        // ws unconditionally on the next iteration.
        ws.bound_state() = saved_state;
        x = saved_x;
        return false;
    }

    // Release pass: a pin added before the one that actually fixed the
    // inertia may not have been needed. Reverse order (newest first) so
    // the pin most likely to be load-bearing is tried last.
    for (auto it = pinned.rbegin(); it != pinned.rend(); ++it) {
        const auto si = static_cast<std::size_t>(*it);
        const BoundState held = ws.bound_state()[si];
        ws.bound_state()[si] = BoundState::kFree;
        if (probe_inertia(qp, ws, kkt, border, counters, opts) != detail::InertiaVerdict::kOk) {
            ws.bound_state()[si] = held; // negative curvature is still there: keep the pin
        }
    }
    return true;
}

bool QpEngine::rebuild_would_be_noop(const WorkingSet &ws, const BorderState &border) {
    if (ws.active_ineq() != border.k0_rows) {
        return false;
    }
    return std::all_of(border.ledger.begin(), border.ledger.end(), [](const BorderLedgerEntry &e) {
        return e.kind == BorderLedgerEntry::Kind::kVarPin;
    });
}

bool QpEngine::latch_still_holds(const WorkingSet &ws) const {
    const Index pinned = ws.n() - ws.num_free();
    return pinned > opts_.schur_cap;
}

void QpEngine::rebuild_k0(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                          QpCounters &counters, const QpOptions &opts) const {
    KktAssembly k0 = assemble_kkt_full(qp, ws, opts);
    std::vector<Index> k0_rows = ws.active_ineq();
    // The analysis decision (QpCounters::symbolic_analyses): decided BEFORE
    // the factorize, which is the only way to know whether THIS call is
    // about to pay the backend's symbolic analysis. TAKEN here and HANDED
    // to factorize_checked() rather than taken twice, so one factorization
    // costs one pattern hash at this layer (kkt_calls.h's
    // AnalysisDecision).
    const detail::AnalysisDecision analysis = detail::analysis_decision(border.kkt, k0.K);
    if (analysis.needed) {
        ++counters.symbolic_analyses;
    }
    detail::factorize_checked(border.kkt, k0.K, analysis);
    ++counters.factorizations;

    // Commit, against a factorization that already succeeded: the two
    // members assigned here are built from locals above, so their
    // allocations are still paid before the factorize and only the
    // (non-throwing) hand-over is deferred.
    border.k0 = std::move(k0);
    border.k0_rows = std::move(k0_rows);
    border.ledger.clear();
    // Constructed AFTER the factorization: add_border caches K0^-1 v
    // against whatever `border.kkt` currently holds. SchurComplement reads
    // only schur_cap/schur_cond_max from `opts` -- both unaffected by
    // SolveOverrides -- so passing `opts` rather than opts_ changes
    // nothing observable; it keeps one consistent options value.
    border.schur.emplace(border.kkt, opts);
}

void QpEngine::sync_borders(const QpProblem &qp, const WorkingSet &ws, BorderState &border,
                            QpCounters &counters, const QpOptions &opts) const {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index n0 = border.k0.K.rows();
    const std::vector<BoundState> &bs = ws.bound_state();
    std::vector<BorderLedgerEntry> &ledger = border.ledger;

    for (Index b = static_cast<Index>(ledger.size()) - 1; b >= 0; --b) {
        const BorderLedgerEntry &e = ledger[static_cast<std::size_t>(b)];
        bool live = true;
        switch (e.kind) {
        case BorderLedgerEntry::Kind::kVarPin:
            live = bs[static_cast<std::size_t>(e.target)] != BoundState::kFree;
            break;
        case BorderLedgerEntry::Kind::kRowDelete:
            // Re-activating a row K0 owns is the exact INVERSE of
            // deactivating it: drop the delete border and K0's own row
            // (with its own dual) is live again -- no add_ineq_row
            // duplicate of a row K0 already carries.
            live = !in_working(ws, border.k0_rows[static_cast<std::size_t>(e.target - me)]);
            break;
        case BorderLedgerEntry::Kind::kIneqRow:
            live = in_working(ws, e.target);
            break;
        }
        if (!live) {
            border.schur->drop_border(b);
            ledger.erase(ledger.begin() + static_cast<std::ptrdiff_t>(b));
            ++counters.schur_updates;
        }
    }

    for (Index i = 0; i < n; ++i) {
        if (bs[static_cast<std::size_t>(i)] == BoundState::kFree ||
            has_border(ledger, BorderLedgerEntry::Kind::kVarPin, i)) {
            continue;
        }
        // RESERVE BEFORE THE SCHUR MUTATION, in all three add loops. The
        // ledger and the border stack are one structure kept in two
        // places, matched by add-order POSITION, so an add_border() that
        // succeeds followed by a ledger.push_back() that throws on its
        // growth allocation leaves a border the ledger cannot name --
        // undroppable forever, and shifting every later drop by one.
        // Reserving here puts the only throw AHEAD of the border stack
        // mutation. (The add_border side is closed in
        // schur_complement.h: a throwing add leaves the stack as it was.)
        ledger.reserve(ledger.size() + 1);
        border.schur->add_border(BorderOps::pin_variable(i, n0), -opts.dual_mu);
        ledger.push_back({BorderLedgerEntry::Kind::kVarPin, i});
        ++counters.schur_updates;
    }
    for (std::size_t p = 0; p < border.k0_rows.size(); ++p) {
        const Index k = me + static_cast<Index>(p); // K0 constraint-row index
        if (in_working(ws, border.k0_rows[p]) ||
            has_border(ledger, BorderLedgerEntry::Kind::kRowDelete, k)) {
            continue;
        }
        ledger.reserve(ledger.size() + 1); // see the pin loop above
        border.schur->add_border(BorderOps::delete_k0_row(k, me, n, n0), 0.0);
        ledger.push_back({BorderLedgerEntry::Kind::kRowDelete, k});
        ++counters.schur_updates;
    }
    for (const Index row : ws.active_ineq()) {
        if (std::binary_search(border.k0_rows.begin(), border.k0_rows.end(), row) ||
            has_border(ledger, BorderLedgerEntry::Kind::kIneqRow, row)) {
            continue;
        }
        ledger.reserve(ledger.size() + 1); // see the pin loop above
        border.schur->add_border(BorderOps::add_ineq_row(qp, row, n0), -opts.dual_mu);
        ledger.push_back({BorderLedgerEntry::Kind::kIneqRow, row});
        ++counters.schur_updates;
    }
}

bool QpEngine::has_border(const std::vector<BorderLedgerEntry> &ledger,
                          BorderLedgerEntry::Kind kind, Index target) {
    return std::any_of(ledger.begin(), ledger.end(), [&](const BorderLedgerEntry &e) {
        return e.kind == kind && e.target == target;
    });
}

void QpEngine::price(const QpProblem &qp, const WorkingSet &ws, const EqpResult &eqp, Vec &lambda_e,
                     Vec &lambda_i, Vec &z) {
    lambda_e = eqp.lambda_e;
    lambda_i.setZero();
    const auto &aw = ws.active_ineq();
    for (std::size_t k = 0; k < aw.size(); ++k) {
        lambda_i(aw[k]) = eqp.lambda_w(static_cast<Index>(k));
    }

    Vec r = qp.H.selfadjointView<Eigen::Upper>() * eqp.x + qp.g;
    if (qp.me() > 0) {
        r += qp.Ae.transpose() * lambda_e;
    }
    if (qp.mi() > 0) {
        r += qp.Ai.transpose() * lambda_i;
    }
    z.setZero();
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            z(i) = r(i);
        }
    }
}

bool QpEngine::drop_worst(const QpProblem &qp, const Vec &shift, const Vec &lambda_i, const Vec &z,
                          WorkingSet &ws, DropRecord &dropped, QpCounters &counters) const {
    struct Candidate {
        bool is_ineq = false;
        Index idx = -1;
        double violation = 0.0;
        double angle = 0.0; // violation / ||constraint gradient||
    };

    std::vector<Candidate> cands;
    for (Index row : ws.active_ineq()) {
        if (shift(row) > 0.0) {
            continue; // still being driven to feasibility; sign says nothing
        }
        const double viol = -lambda_i(row);
        if (viol <= opts_.opt_tol) {
            continue;
        }
        const double nrm = qp.Ai.row(row).norm();
        cands.push_back({true, row, viol, viol / std::max(nrm, detail::kEngineDenomTol)});
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
        double viol = 0.0;
        if (st == BoundState::kAtLower) {
            viol = -z(i);
        } else if (st == BoundState::kAtUpper) {
            viol = z(i);
        } else {
            continue; // kFree has no multiplier, kFixed is not sign-constrained
        }
        if (viol > opts_.opt_tol) {
            cands.push_back({false, i, viol, viol}); // ||e_i|| == 1
        }
    }
    if (cands.empty()) {
        return false;
    }

    // Dantzig: most negative multiplier; exact ties go to the largest
    // angle, then to the first candidate (inequalities before bounds,
    // both in ascending index order).
    double best_viol = 0.0;
    for (const auto &c : cands) {
        best_viol = std::max(best_viol, c.violation);
    }
    const double tie_window = best_viol * detail::kEngineDropTieTol;
    const Candidate *pick = nullptr;
    Index in_window = 0; // observation only -- see QpCounters::drop_ties
    for (const auto &c : cands) {
        if (c.violation < best_viol - tie_window) {
            continue;
        }
        ++in_window;
        if (pick == nullptr || c.angle > pick->angle) {
            pick = &c;
        }
    }
    if (in_window > 1) {
        ++counters.drop_ties; // observation only -- see QpCounters
    }

    dropped = DropRecord{};
    dropped.active = true;
    dropped.is_ineq = pick->is_ineq;
    dropped.idx = pick->idx;
    ++counters.ws_drops; // observation only -- see QpCounters
    if (!pick->is_ineq) {
        ++counters.ws_drops_bound;
    }
    if (pick->is_ineq) {
        ws.drop_ineq(pick->idx);
    } else {
        const auto si = static_cast<std::size_t>(pick->idx);
        dropped.from = ws.bound_state()[si];
        ws.bound_state()[si] = BoundState::kFree;
    }
    return true;
}

double QpEngine::dropped_directional(const QpProblem &qp, const DropRecord &dropped,
                                     const Vec &d) const {
    if (dropped.is_ineq) {
        return qp.Ai.row(dropped.idx).dot(d);
    }
    const double di = d(dropped.idx);
    return (dropped.from == BoundState::kAtUpper) ? di : -di;
}

bool QpEngine::ride_stays_in_working_set(const QpProblem &qp, const WorkingSet &ws,
                                         const Vec &p) const {
    const double pn = p.norm();
    for (Index j = 0; j < qp.me(); ++j) {
        if (std::abs(qp.Ae.row(j).dot(p)) > detail::kRideNullspaceTol * qp.Ae.row(j).norm() * pn) {
            return false;
        }
    }
    for (const Index row : ws.active_ineq()) {
        if (std::abs(qp.Ai.row(row).dot(p)) >
            detail::kRideNullspaceTol * qp.Ai.row(row).norm() * pn) {
            return false;
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree &&
            std::abs(p(i)) > detail::kRideNullspaceTol * pn) {
            return false; // ||e_i|| == 1
        }
    }
    return true;
}

int QpEngine::ride_sign(const QpProblem &qp, const DropRecord &dropped, const Vec &p,
                        const Vec &x) const {
    const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
    const double slope = gx.dot(p);
    const double slope_tol = detail::kEngineDenomTol * gx.norm() * p.norm();
    const double leaves = -dropped_directional(qp, dropped, p);
    const double leaves_tol =
        detail::kEngineDenomTol * dropped_gradient_norm(qp, dropped) * p.norm();

    // DESCENT picks the sign, and only strict descent qualifies -- a flat
    // direction has no descending sign and declines.
    int sign = 0;
    if (slope < -slope_tol) {
        sign = 1;
    } else if (slope > slope_tol) {
        sign = -1;
    }
    // FEASIBILITY vetoes it: the descending sign must not step back into
    // the constraint the drop rule just released.
    if (sign == 0 || static_cast<double>(sign) * leaves < -leaves_tol) {
        return 0; // no admissible sign: decline
    }
    return sign;
}

double QpEngine::dropped_gradient_norm(const QpProblem &qp, const DropRecord &dropped) {
    return dropped.is_ineq ? qp.Ai.row(dropped.idx).norm() : 1.0; // ||e_i|| == 1
}

QpEngine::RideOutcome QpEngine::ride_negative_curvature(const QpProblem &qp,
                                                        const DropRecord &dropped,
                                                        QpCounters &counters, WalkSeen &seen,
                                                        const Vec &p, const Vec &Aix,
                                                        WorkingSet &ws, Vec &x) const {
    if (!ride_stays_in_working_set(qp, ws, p)) {
        return RideOutcome::kDeclined;
    }
    const int sign = ride_sign(qp, dropped, p, x);
    if (sign == 0) {
        return RideOutcome::kDeclined;
    }
    const Vec dir = (sign < 0) ? Vec(-p) : p;

    BlockKind kind = BlockKind::kNone;
    Index block_idx = -1;
    BoundState block_state = BoundState::kFree;
    const double alpha = ratio_test(qp, ws, x, Aix, dir, kind, block_idx, block_state,
                                    /*cap_at_unit=*/false, counters);
    if (kind == BlockKind::kNone) {
        return RideOutcome::kUnbounded;
    }

    x += alpha * dir;
    ++counters.ws_adds; // observation only -- see QpCounters
    if (kind == BlockKind::kBound) {
        ++counters.ws_adds_bound;
        mark_bound_seen(seen, counters, block_idx);
    } else {
        mark_ineq_seen(seen, counters, block_idx);
    }
    if (kind == BlockKind::kIneq) {
        ws.add_ineq(block_idx);
    } else {
        ws.bound_state()[static_cast<std::size_t>(block_idx)] = block_state;
        x(block_idx) =
            (block_state == BoundState::kAtUpper) ? qp.upper(block_idx) : qp.lower(block_idx);
    }
    return RideOutcome::kStepped;
}

double QpEngine::ratio_test(const QpProblem &qp, const WorkingSet &ws, const Vec &x, const Vec &Aix,
                            const Vec &p, BlockKind &kind, Index &block_idx,
                            BoundState &block_state, bool cap_at_unit, QpCounters &counters) {
    double best = std::numeric_limits<double>::infinity();
    kind = BlockKind::kNone;
    Index tied = 0;

    if (qp.mi() > 0) {
        const Vec Aip = qp.Ai * p;
        for (Index j = 0; j < qp.mi(); ++j) {
            if (in_working(ws, j) || Aip(j) <= detail::kEngineDenomTol) {
                continue;
            }
            // No shift term is needed here: refresh_shifts() puts every
            // row carrying a positive shift INTO the working set, and
            // working rows are skipped above, so a row reaching this line
            // always has shift == 0. Were that invariant ever broken the
            // omission is the safe direction -- slack clamps to 0, the
            // row blocks at alpha = 0 and is added, which restores it.
            const double slack = std::max(0.0, qp.bi(j) - Aix(j));
            const double ratio = slack / Aip(j);
            if (ratio < best) {
                best = ratio;
                kind = BlockKind::kIneq;
                block_idx = j;
                tied = 1; // observation only -- see this function's note
            } else if (ratio == best) {
                ++tied; // observation only
            }
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            continue;
        }
        double ratio = std::numeric_limits<double>::infinity();
        BoundState hit = BoundState::kFree;
        if (p(i) > detail::kEngineDenomTol && qp.upper(i) < detail::kEngineInfBound) {
            ratio = std::max(0.0, qp.upper(i) - x(i)) / p(i);
            hit = BoundState::kAtUpper;
        } else if (p(i) < -detail::kEngineDenomTol && qp.lower(i) > -detail::kEngineInfBound) {
            ratio = std::min(0.0, qp.lower(i) - x(i)) / p(i);
            hit = BoundState::kAtLower;
        }
        if (ratio < best) {
            best = ratio;
            kind = BlockKind::kBound;
            block_idx = i;
            block_state = hit;
            tied = 1; // observation only
        } else if (hit != BoundState::kFree && ratio == best) {
            ++tied; // observation only
        }
    }
    if (kind != BlockKind::kNone && tied > 1) {
        ++counters.ratio_ties; // observation only -- see QpCounters
    }

    if (!cap_at_unit) {
        return best; // infinite exactly when kind == kNone
    }
    if (kind != BlockKind::kNone && best > 1.0 + detail::kEngineStepTieTol) {
        kind = BlockKind::kNone; // the full step is interior: nothing joins
    }
    return std::min(1.0, best);
}

} // namespace hven::solvers
