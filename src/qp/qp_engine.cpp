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
// header: tests, bench and src/drivers/sqp_solver.cpp call them directly, and
// this TU inlines them exactly as before.
//
// FP arithmetic crosses this TU boundary under ONE uniform flag regime on both
// sides; every asserted counter must be bit-identical across the boundary, so
// a counter delta here is a FAILED CARVE, to be reverted or redrawn, never a
// re-derivation.

#include <bit>

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

std::uint64_t options_fingerprint(const QpOptions &opts, int threads) {
    // splitmix64's finalizer as the mixer: a FIELD-WISE fold, never a byte hash
    // over the struct, so padding bytes cannot make two equal values disagree.
    const auto mix = [](std::uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    };
    std::uint64_t h = 0xcbf29ce484222325ULL;
    const auto fold = [&](std::uint64_t v) { h = mix(h ^ mix(v)); };
    const auto fold_double = [&](double d) { fold(std::bit_cast<std::uint64_t>(d)); };

    // EVERY field of QpOptions, in declaration order. A field added there and
    // not folded here is a hole in the hot-handle reuse gate -- see the contract
    // in qp_types.h, and the field-coverage pin in tests/drivers/test_options.cpp
    // that flips each one alone and requires the hash to move.
    fold_double(opts.primal_delta);
    fold_double(opts.dual_mu);
    fold_double(opts.feas_tol);
    fold_double(opts.opt_tol);
    fold(static_cast<std::uint64_t>(opts.max_iter));
    fold(static_cast<std::uint64_t>(opts.schur_cap));
    fold_double(opts.schur_cond_max);
    fold(static_cast<std::uint64_t>(opts.ws_algebra));
    fold_double(opts.tr_radius);
    fold(static_cast<std::uint64_t>(opts.independent_working_set));
    // And the thread count in force, which is not a QpOptions field but IS part
    // of what a factorization was built under.
    fold(static_cast<std::uint64_t>(static_cast<std::int64_t>(threads)));
    return h;
}

std::shared_ptr<const HotState> QpEngine::hot_state() const {
    if (!border_valid_) {
        return nullptr;
    }
    return std::make_shared<const HotState>(
        HotState{border_, border_structural_hash_, border_values_hash_, border_effective_delta_,
                 border_effective_mu_, border_exit_bound_state_, border_exit_active_ineq_,
                 border_kkt_session_id_, border_kkt_epoch_, options_hash_});
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
    // THE EQP-REFINE TEMPORARY, at this engine's thread count (M6 W5 T8.8).
    // It dies inside this call, so no boundary observation can read its count;
    // what covers it is the CONSTRUCTION RULE (kkt_calls.h) plus this line.
    detail::KktFactor kkt(threads_);
    ++out.counters.factorizations;
    // THE PROXIMAL CENTRE for a re-solve OF THE CALLER'S OWN FACE is the point
    // the caller stopped at: this function re-solves the same subproblem from
    // there (eqp_solve.h states the system).
    // WHAT THIS CALL PROMISES IS THE FACE'S EXACT ANSWER, and since M7 W2 E4 fix
    // round 1 solve_eqp delivers it: its UNREGULARIZED POLISH (ruling E4-11,
    // eqp_solve.h's polish_eqp_core) takes the regularization back out of the
    // answer wherever the face's own exact system has one, which is why this
    // function needs nothing of its own for it. `out.counters.eqp_polish_steps`
    // reports how many steps that took -- 0 on a flat face, where the proximal
    // answer is the only answer there is.
    const EqpResult eqp = solve_eqp(qp, ws, face.x, kkt, eff_opts);
    out.counters.eqp_refine_steps += eqp.refine_steps; // identically 0; see eqp_solve.h
    out.counters.eqp_refine_iters += eqp.residual_refine_iters;
    out.counters.eqp_polish_steps += eqp.polish_steps;
    if (eqp.residual_refused) {
        // THE EQP-SITE RESIDUAL CHECK (E4-4): the face's own solve does not solve
        // its own system, so there is no refined point to offer the caller.
        ++out.counters.eqp_residual_refusals;
        return false;
    }
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
    // ADOPT AN EXTERNAL HOT HANDLE. `hot` (SqpWarmStart::hot, opaque) is a
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
    //
    // THE OPTIONS FINGERPRINT IS A FOURTH ADOPTION CONJUNCT (M6 W5 T8.3).
    // Conditions (a)-(e) below fingerprint the PROBLEM and the FACTOR OBJECT and
    // never the ENGINE OPTIONS the K0 was built under, so an engine with a
    // different schur_cap, ws_algebra or regularization would otherwise adopt a
    // matching handle and reuse a factorization built for other settings.
    // Equality is required BEFORE prev_border_valid is snapshotted below, so a
    // refused handle leaves prev_border_valid false, reuse_eligible false,
    // k0_reused 0, and the DETACH branch unreached because nothing was adopted.
    // "kWarm by construction" is exact: the values and the working set still
    // come from `seed`, which is independent of `hot`.
    if (!border_valid_ && hot != nullptr && hot->border != nullptr &&
        hot->engine_options_hash == options_hash_ &&
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
    detail::KktFactor kkt(threads_);
    // The face `kkt` holds, for the verdict-site refinement's reuse guard.
    EliminatedFace face;
    QpCounters counters;
    WalkSeen seen{std::vector<std::uint8_t>(static_cast<std::size_t>(mi), 0),
                  std::vector<std::uint8_t>(static_cast<std::size_t>(n), 0),
                  std::vector<std::uint8_t>(
                      eff_opts.independent_working_set ? static_cast<std::size_t>(mi) : 0, 0),
                  std::vector<std::uint8_t>(
                      eff_opts.independent_working_set ? static_cast<std::size_t>(mi) : 0, 0),
                  std::vector<std::uint8_t>(
                      eff_opts.independent_working_set ? static_cast<std::size_t>(n) : 0, 0),
                  {},
                  0};
    // The certificate behind the add site's rule: exact (residual within the
    // rounding floor) behind a kInfeasible, toleranced behind a kNumericalError.
    QpFarkasCertificate farkas;

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
    // THE EQUALITY DRIFT's invariant (section 4c): whether the walk has met its
    // equality rows yet -- a start point need not.
    bool eq_rows_met = false;

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

    // Working-set independence: the entry detector runs on the first
    // factorization, and again after each removal it makes; `start_pass`
    // stays true until the start repair has had its pass.
    bool entry_check_pending = eff_opts.independent_working_set;
    bool start_pass = true;
    bool recheck_pending = false;
    // The member the last deferred add admitted (kind kEq = none): a re-check
    // that removes it marks it known-dependent at that point (E2 fix1, D3(b)).
    detail::WsMember deferred_member;
    Index shift_adds_checked = 0;
    // THE BORDER RETREAT: set when the add site's no-admissible rule would
    // end the solve on a pass whose factor is the border stack; the rest of
    // the solve runs on the bound-eliminated path (and commits no hot state).
    bool eliminated_only = false;

    QpStatus status = crossed_bounds ? QpStatus::kInfeasible : QpStatus::kMaxIter;
#ifdef HVEN_TESTING
    // M7 W2 M5's DEGENERACY ARMS (qp_degeneracy_arm.h). Clears the published
    // stationarity scale so this solve's first drop can never be judged
    // against the PREVIOUS solve's scale, re-arms an arm a previous solve
    // disarmed, and stamps the solve so no cached decomposition can be reused
    // across two problems; it arms nothing and starts nothing.
    detail::testing::QpDegeneracyArmState::begin_solve();
    // ARM B1's ENTRY REPAIR (amendment A3), AND IT RUNS BEFORE THE OBSERVER
    // OPENS THIS SOLVE, deliberately. `ws` at this point is the set the walk
    // would start from -- the seed ingest and the seeding `refresh_shifts` are
    // both above -- and the repair is the last thing that can change it. Taking
    // the observer's seed snapshot AFTER the repair is what makes the trace's
    // starting working set the set the walk actually starts from; a snapshot
    // taken before it would describe a set no pass ever runs on. Unarmed this
    // does nothing at all, so the ORDER of the three seam calls below is the
    // only thing that changes for an unarmed run, and none of them reads the
    // others' state. Armed, it establishes the INVARIANT every later verdict
    // is conditional on -- and when it cannot (the mandatory rows are
    // themselves dependent) it disarms, so the solve below runs production's
    // statements from here on.
    if (detail::testing::QpDegeneracyArmState::independent_add_on()) {
        detail::testing::b1_entry_repair(qp, ws);
    }
    // THE WALK'S READ-ONLY TEST SEAM (docs/testing.md; declarations in
    // qp_walk_trace.h, which compiles to nothing without HVEN_TESTING).
    // `ws` here is the set the first pass starts from: the seed ingest, the
    // seeding refresh_shifts and arm B1's entry repair are all above.
    detail::testing::QpWalkObserver::begin_solve(warm, n, me, mi, eff_max_iter, ws);
    // M7 W2 M4's ANTI-CYCLING ARMS (qp_anti_cycling_arm.h). With no arm armed
    // this starts nothing and every hook below runs production's statements.
    // Armed, it begins EXPAND's first expanding sequence (§4.2) for this solve.
    detail::testing::QpAntiCycleArmState::begin_solve();
    // M7 W2 M5's POST-DROP INERTIA DIAGNOSTIC owns its OWN factor and its OWN
    // face, and never the engine's. That is the whole of why it is bit-neutral
    // on every algebra path rather than only on the eliminated one: the
    // measurement is taken by calling `eliminated_candidate` directly on a
    // LOCAL `KktFactor` and a LOCAL `EliminatedFace`, so `kkt`, `face` and the
    // live border stack are untouched, and the pass's counters are saved and
    // restored around the call. Created LAZILY, so a run that never arms the
    // diagnostic constructs no backend factor at all -- and `make_unique` here
    // rather than a member or a new `#include`, because `qp_engine.h` already
    // brings in <memory> (`border_` is a shared_ptr) and an include added at
    // the top of THIS file would move the `__LINE__` immediate of the
    // `assert()` above, which is exactly the instruction-stream move the seam's
    // placement rules exist to avoid.
    std::unique_ptr<detail::KktFactor> diag_kkt;
    EliminatedFace diag_face;
#endif
    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {
        ++counters.minor_iters;
#ifdef HVEN_TESTING
        // One declaration covers every exit this loop body has: the record is
        // emitted by this object's DESTRUCTOR, so a `continue`, a `break` and
        // the fall-through all record, and a path added later cannot silently
        // stop recording.
        detail::testing::QpWalkMinorScope walk_obs(ws, x, counters, iter);
#ifdef HVEN_TESTING
        // THE OBJECTIVE AT THIS PASS'S INCOMING ITERATE (qp_walk_trace.h's obj_in),
        // so the ordinary step's DESCENT INVARIANT is decidable from the trace. One
        // mat-vec into storage the loop never reads; nothing here is a decision.
        walk_obs.record().obj_in =
            qp.g.dot(x) + 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x);
#endif

        // ARM E, EXPAND §4.2: `delta_k <- delta_{k-1} + tau` ((4.2)) at the
        // START of every minor, and §4.3's RESET once the expanding sequence
        // has RUN its K iterations -- so the K-th minor runs at delta_K and the
        // reset falls at the (K+1)-th, which is §4.3's own order and was the
        // review's Important 1. `begin_minor()` returns false for every arm but
        // E, and for E it returns true only after a sequence has completed.
        if (detail::testing::QpAntiCycleArmState::begin_minor()) {
            using Ac = detail::testing::QpAntiCycleArmState;
            // §4.3 step (1): "every nonbasic variable that lies within delta_f
            // of a bound is moved exactly onto the bound", counting the
            // adjustments larger than eps^(2/3) as "nontrivial". A variable
            // PINNED in this walk's working set is the active-set analogue of
            // a nonbasic variable; the free ones are what the EQP solves for.
            for (Index i = 0; i < n; ++i) {
                const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
                double b = 0.0;
                if (st == BoundState::kAtUpper) {
                    b = qp.upper(i);
                } else if (st == BoundState::kAtLower || st == BoundState::kFixed) {
                    b = qp.lower(i);
                } else {
                    continue;
                }
                const double moved = std::abs(x(i) - b);
                if (moved <= Ac::params.delta_master) {
                    if (moved > Ac::params.reset_nontrivial) {
                        ++Ac::reset_nontrivial_moves;
                    }
                    x(i) = b;
                }
            }
            // §4.3 step (2) -- "the basic variables are recomputed in terms of
            // the newly adjusted nonbasic variables" -- is exactly what the
            // EQP solve a few lines below does from the re-projected iterate,
            // so no separate recomputation is needed. The shifts are refreshed
            // because x moved, the same way every other x-moving branch of
            // this loop refreshes them.
            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
            Ac::note_reset(); // §4.3 step (3): delta <- delta_0, new sequence
        }
#endif

        // THE ZERO-MOTION CAP: at most 2(n + mi) releases and re-entries at
        // one iterate (each constraint is released at most once there, and a
        // re-entry is drop-exempt).
        if (eff_opts.independent_working_set && seen.pt_events > 2 * (n + mi)) {
            ++counters.zero_motion_cap_hits;
            status = QpStatus::kNumericalError;
#ifdef HVEN_TESTING
            walk_obs.record().action = detail::testing::QpWalkAction::kTerminal;
#endif
            break;
        }

        // Stamp addition numbers on whatever joined the working set since
        // the last iteration, BEFORE anything this iteration changes it,
        // so the record reflects the order the loop actually built the
        // working set in (section 4b's zero-multiplier probe reads it).
        probe.refresh(ws);

        detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
#ifdef HVEN_TESTING
        // Cleared BEFORE the call, so a pass that reaches no verdict site
        // records ABSENCE rather than the previous pass's evidence.
        detail::testing::qp_walk_reset_eqp();
#endif
        const EqpResult eqp =
            eliminated_only
                ? eliminated_candidate(qp, ws, x, kkt, face, counters, verdict, eff_opts)
                : eqp_candidate(qp, ws, x, kkt, face, *border_, counters, verdict, eff_opts);
#ifdef HVEN_TESTING
        // Snapshotted IMMEDIATELY: section 4b's zero-multiplier probe re-enters
        // the same verdict site for HYPOTHETICAL working sets later in this very
        // pass, and what it writes must not be read as this pass's main verdict.
        walk_obs.snapshot_eqp();
        walk_obs.record().inertia_verdict = static_cast<int>(verdict);
        walk_obs.record().primal_delta = eff_opts.primal_delta;
        walk_obs.record().dual_mu = eff_opts.dual_mu;
        walk_obs.record().eqp_row_residual = eqp.row_residual;
        walk_obs.record().eqp_residual_ratio = eqp.residual_ratio;
        walk_obs.record().eqp_residual_iters = eqp.residual_refine_iters;
        walk_obs.record().eqp_residual_refused = eqp.residual_refused ? 1 : 0;
#endif

        // THE EQP-SITE RESIDUAL CHECK's REFUSAL (section 5; eqp_solve.h states the
        // check), and a NON-FINITE answer, which is the same statement at its limit.
        // The answer this pass was handed does not solve the system it came out of;
        // the border path's own retreat to the bound-eliminated form has already been
        // spent inside the call, and E4-4's rule is that no step may be taken toward
        // such an answer.
        //
        // THE RETREAT IS THE ENGINE'S OWN LADDER, not a new exit: an untrustworthy
        // factorization is exactly what section 4b's SUSPECT-STALL escalation
        // answers, and it answers it by CHANGING THE SYSTEM -- one decade of
        // primal_delta per rung, at most kMaxSuspectEscalations rungs, the border
        // state discarded so the new delta reaches K0. That is the right response
        // here for the reason the check itself gives: the residual is large because
        // the free block along a flat face IS delta*I, and a decade more delta is a
        // decade less of that. Measured: without the ladder the same refusal ends
        // five A0_YORKNET cells and two certified Cs_STEENBRB cells at their first
        // refused pass. Only an EXHAUSTED ladder reports kNumericalError, at the
        // iterate the walk stands on -- finite, and where the caller's evidence is.
        // `counters.eqp_residual_refusals` carries the reason.
        //
        // THE STEP'S ROW-DRIFT CHECK (fix round 2) rides the SAME refusal, for
        // the same reason and into the same ladder: an answer that leaves a
        // working row the iterate was already on is an answer no step may be
        // taken toward, and lambda -- the size of the drift -- scales as
        // 1/primal_delta on the flat face that produces it. See
        // eqp_answer_drifts_off_met_rows for the measurement.
        const bool eqp_not_finite = !eqp.x.allFinite();
        const bool eqp_drifted =
            !eqp_not_finite && eqp_answer_drifts_off_met_rows(qp, ws, x, eqp, Aix, ai_row_norm1,
                                                              ae_row_norm1, eff_opts);
        const bool drift_only = eqp_drifted && !eqp.residual_refused && !eqp_not_finite;
        if (eqp.residual_refused || eqp_not_finite || eqp_drifted) {
            if (drift_only) {
                ++counters.eqp_row_drift_refusals;
            }
            if (counters.suspect_escalations < detail::kMaxSuspectEscalations) {
                ++counters.suspect_escalations;
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kSuspectEscalation;
#endif
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
            // THE EXHAUSTED LADDER IS ADVISORY FOR BOTH REFUSALS (fix round 2), and
            // TERMINAL ONLY FOR A NON-FINITE ANSWER.
            //
            // The ladder is the remedy both refusals ask for, and its exhaustion is
            // itself evidence: three decades of primal_delta have not brought the
            // row residual down, so the residual is IRREDUCIBLE -- which is what an
            // INCONSISTENT row set looks like (the canonical pair e_0 = 0 beside
            // e_0 = 1 at dual_mu = 0), not what an untrustworthy factor looks like.
            // Ending the solve kNumericalError there answers the wrong question:
            // the walk's own terminal site already classifies an iterate it cannot
            // leave, and on an inconsistent set it classifies it kInfeasible, which
            // is the answer the base walk gave and the answer the caller needs.
            //
            // MEASURED, and this is why the classification this replaced is gone:
            // reporting kInfeasible HERE, from worst_structural_violation at the
            // refused pass, is a claim about an iterate the walk has not finished
            // with. On an ELASTIC RELAXATION -- feasible by construction -- a
            // mid-walk iterate violates rows it has not reached yet, and the T7
            // feasible census read 16 open relaxations certified infeasible against
            // its own claim of 0. A FALSE CERTIFICATE is not a trade any fix may
            // make, so the classification is withdrawn and the pass falls through
            // to the walk instead: the step is taken, the refusals are counted, and
            // the terminal site decides with the evidence it has always used.
            //
            // AND THE SAME MEASUREMENT SETTLES THE DRIFT HALF: made terminal, it
            // costs `ACOPR57_def`/qp0 its BASE CERTIFICATE on both algebras
            // (opt/183 -> num/162, terminal stationarity 1.24). Advisory, that cell
            // keeps it (opt/191, f at Clarabel's optimum to 2.2e-12 relative),
            // `indefinite_equality_qp` converges on the rungs, and
            // `A0_YORKNET`/qp1 converts (num/329 -> opt/717, equality residual
            // 4.37e-01 -> 1.97e-13).
            //
            // A NON-FINITE ANSWER STAYS TERMINAL. There the complaint is not about
            // accuracy at all: there is no step to take, and kNumericalError at the
            // finite iterate the walk stands on is the honest exit (E4-4's two NaN
            // cells).
            if (eqp_not_finite) {
                status = QpStatus::kNumericalError;
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kTerminal;
#endif
                break;
            }
        }

        // INERTIA GATE / TEMPORARY-VERTEX START REPAIR (section 4b). Only
        // at the start: a trustworthy wrong inertia here means the seed
        // working set is second-order inconsistent and the EQP result
        // just computed is a saddle/maximizer, so it is discarded and the
        // iteration retried from the repaired vertex. kSuspect is
        // deliberately NOT a repair trigger, and a wrong inertia LATER is
        // section 4c's ride to handle.
        // WORKING-SET INDEPENDENCE: the entry detector, and its re-check after
        // any growth no pass factor could test (shift adds, repair pins) or an
        // over-count me + |W| > n.
        if (eff_opts.independent_working_set) {
            // THE INTEGER SCREEN, before any solve: me + |W| > n proves the
            // set dependent.
            const Index ws_count =
                me + static_cast<Index>(ws.active_ineq().size()) + (n - ws.num_free());
            const bool over = ws_count > n;
            if (over) {
                ++counters.ws_count_violations;
            }
            // The add site exchanges rather than admits a dependent blocker,
            // so an over-count comes only from untested growth, which the
            // detector is asked to find.
            const bool untested =
                entry_check_pending || recheck_pending || counters.shift_adds != shift_adds_checked;
            if (untested || over) {
                shift_adds_checked = counters.shift_adds;
                recheck_pending = false;
                const detail::DepFactorView view = dep_factor_view(ws, kkt, face, *border_);
                const detail::EntryDetection det =
                    detail::detect_dependency(qp, ws, view, eff_opts);
                counters.dep_test_solves += det.solves;
                counters.dep_uncertified_trims += det.uncertified_dropped ? 1 : 0;
                counters.dep_uncertified_fallbacks += det.uncertified_fallback ? 1 : 0;
                // A screen-positive set the detector did not resolve is never
                // "independent": counted, and trimmed by count (R6).
                // A miss here (the detector stalls on an over-counted set) is a
                // legitimate, COUNTED outcome, not an invariant: it is counted
                // dep_untested_growth and trimmed by count (R6). It was
                // Debug-asserted until E2 fix3 (settler S27): E2-D4 showed the
                // assert firing on the ACOPR57 dumps at the pre-rule walk, i.e.
                // it asserted a property that is not one; the count is the
                // observation.
                detail::EntryDetection trim;
                if (over && !det.found) {
                    ++counters.dep_untested_growth;
                    trim.found = true;
                    for (Index r = 0; r < me; ++r) {
                        trim.support.push_back({detail::WsMember::Kind::kEq, r});
                    }
                    for (const Index r : ws.active_ineq()) {
                        trim.support.push_back({detail::WsMember::Kind::kRow, r});
                    }
                    for (Index i = 0; i < n; ++i) {
                        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
                            trim.support.push_back({detail::WsMember::Kind::kPin, i});
                        }
                    }
                }
                detail::WsMember removed;
                if ((det.found || trim.found) &&
                    remove_entry_dependency(ws, shift, det.found ? det : trim,
                                            entry_check_pending ? nullptr : &probe,
                                            seen.shift_refused, &removed)) {
                    ++(entry_check_pending ? counters.entry_dependency_removals
                                           : counters.dep_recheck_removals);
                    const bool removed_row = removed.kind == detail::WsMember::Kind::kRow;
                    pt_release(seen, removed_row, removed.idx);
                    if (!entry_check_pending && deferred_member.kind == removed.kind &&
                        deferred_member.idx == removed.idx) {
                        pt_mark(seen, removed_row, removed.idx, kPtKnownDependent);
                    }
                    deferred_member = detail::WsMember{};
                    continue;
                }
                deferred_member = detail::WsMember{};
                if (det.found && entry_check_pending) {
                    ++counters.mandatory_dependent_entries;
                }
                entry_check_pending = false;
            }
        }
        const bool first_pass = start_pass;
        start_pass = false;
        // R-C': a kWrong start verdict is re-read before it may repair.
        if (first_pass && verdict == detail::InertiaVerdict::kWrong &&
            !reread_wrong_inertia(qp, ws, x, counters, eff_opts, verdict) &&
            repair_temporary_vertex(qp, ws, x, kkt, face, *border_, counters, eff_opts)) {
#ifdef HVEN_TESTING
            walk_obs.record().action = detail::testing::QpWalkAction::kStartRepair;
#endif
            degenerate_run = 0; // observation only -- x moved onto bounds
            recheck_pending = eff_opts.independent_working_set;
            pt_clear(seen);
            // Pinning moved x onto bounds, which can violate general
            // rows; the homotopy picks them up exactly as at a start
            // point.
            refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
            continue;
        }

        price(qp, ws, eqp, lambda_e, lambda_i, z);

        // THE EQUALITY DRIFT (section 4c): once the iterate has met its equality
        // rows it keeps meeting them -- a CHECKED invariant, counted. "Met" is
        // the terminal classifier's own test, row by row, on this pass's
        // multipliers: a residual section 5 would not call a violation is not a
        // drift either, and an invariant stated any tighter fires on the accuracy
        // the regularized walk legitimately holds (ACOPR57_def qp0's own optimal
        // iterate sits 6.1e-7 off its rows). Nothing re-projects the iterate.
        if (eff_opts.independent_working_set && me > 0) {
            const Vec eq_resid = qp.Ae * x - qp.be;
            const double xmag = x.lpNorm<Eigen::Infinity>();
            bool eq_structural = false;
            for (Index j = 0; j < me && !eq_structural; ++j) {
                const double v = std::abs(eq_resid(j));
                const double row_scale =
                    std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
                eq_structural = violation_is_structural(v, row_scale, lambda_e(j), eff_opts);
            }
            if (!eq_structural) {
                eq_rows_met = true;
            } else if (eq_rows_met) {
                ++counters.eq_drift_violations;
            }
        }
        // The factor this pass holds for `ws`; valid until `ws` changes.
        const detail::DepFactorView pass_view = eff_opts.independent_working_set
                                                    ? dep_factor_view(ws, kkt, face, *border_)
                                                    : detail::DepFactorView{};

        const Vec p = eqp.x - x;

        // Whatever the previous iteration's drop rule released. Consumed
        // here whether or not the ride below fires, so it can never arm an
        // iteration that did not follow a drop.
        const DropRecord dropped = last_drop;
        last_drop = DropRecord{};

        const double x_inf = x.lpNorm<Eigen::Infinity>();
        const double step_tol = eff_opts.feas_tol * std::max(1.0, x_inf);
        const double p_inf = p.lpNorm<Eigen::Infinity>();

        // THE FREE-BLOCK STATIONARITY RESIDUAL at the iterate this pass started
        // from, with the multipliers `price` just produced, and the largest term
        // that went into it. PRODUCTION since M7 W2 E4: this is the walk's KKT-point
        // test (section 5), not a diagnostic -- the seam below reports these two
        // values rather than recomputing them, so the trace and the branch cannot
        // disagree.
        double stat_scale = 1.0;
        const double stat_free_inf =
            detail::free_block_stationarity(qp, x, ws, lambda_e, lambda_i, stat_scale);
#ifdef HVEN_TESTING
        walk_obs.record().step_tol = step_tol;
        walk_obs.record().p_inf = p.lpNorm<Eigen::Infinity>();
        {
            // THE SEAM'S DISCRIMINATING READ-OUT (FIX-ROUND 1). Everything in
            // this block is computed into the observer's own record from state
            // the loop already holds, by mat-vecs at worst. It performs NO
            // factorization and NO linear solve, touches no engine state and no
            // counter, and nothing below reads any of it.
            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
            r.p_sq = p.squaredNorm();

            // The UNREGULARIZED first-order residual over the free block and its
            // scale, as computed ABOVE, in production, for the KKT-point test.
            // Reported, not recomputed (M7 W2 E4).
            r.stat_free_inf = stat_free_inf;
            r.stat_scale = stat_scale;
            // M7 W2 M5, ARM A: the SAME scale, published for the drop rule.
            // `drop_worst` multiplies `opt_tol` by this rather than deriving a
            // second scale of its own -- the internal-consistency argument the
            // arm rests on is that its threshold IS the quantity
            // `free_block_stationarity` computes, so it has to be that value
            // and not a recomputation of it. The one approximation (this is
            // the scale at the pass's STARTING iterate, while `drop_worst`
            // runs after the KKT-point branch snaps `x = eqp.x`) is bounded
            // and declared in qp_degeneracy_arm.h.
            detail::testing::QpDegeneracyArmState::publish_scale(stat_scale);

            // How far the iterate sits off the constraints the EQP holds at
            // EQUALITY: the equality rows, the WORKING inequality rows (read off
            // the maintained Aix rather than recomputed), and the pinned bounds.
            double res = 0.0;
            if (qp.me() > 0) {
                res = std::max(res, (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>());
            }
            for (const Index row : ws.active_ineq()) {
                res = std::max(res, std::abs(Aix(row) - qp.bi(row)));
            }
            for (Index i = 0; i < qp.n(); ++i) {
                const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
                if (st == BoundState::kAtUpper) {
                    res = std::max(res, std::abs(x(i) - qp.upper(i)));
                } else if (st == BoundState::kAtLower || st == BoundState::kFixed) {
                    res = std::max(res, std::abs(x(i) - qp.lower(i)));
                }
            }
            r.active_res_inf = res;

            // THE CONSTRAINT THE PREVIOUS PASS RELEASED, on every post-drop
            // pass whether or not section 4c's ride is reached below: without
            // it "the direction points back through the bound just released"
            // has to be inferred from the add that follows.
            r.post_drop = dropped.active;
            if (dropped.active) {
                r.released_block = dropped.is_ineq ? detail::testing::QpWalkBlock::kIneq
                                                   : detail::testing::QpWalkBlock::kBound;
                r.released_index = dropped.idx;
                r.released_side = dropped.from;
                r.p_released = dropped.is_ineq ? qp.Ai.row(dropped.idx).dot(p) : p(dropped.idx);
                r.released_directional = dropped_directional(qp, dropped, p);
                // g^T p with g = Hx + g: the SLOPE section 4c's ride_sign reads.
                // Recorded here rather than only inside the ride, so a pass whose
                // curvature gate never fired still reports it.
                r.ride_slope = (qp.H.selfadjointView<Eigen::Upper>() * x + qp.g).dot(p);
            }
        }
#endif
        // ==================================================================
        // THE KKT-POINT TEST (section 5, M7 W2 E4 ruling E4-2). Two disjuncts,
        // and the SECOND is the test:
        //
        //   ZERO MOTION: ||p||inf <= step_tol. Kept exactly as it was, and kept
        //     for exactly what it always was -- the observation that the EQP
        //     answer IS this iterate, so snapping onto it is free. It is no
        //     longer asked to certify convergence on its own, because under the
        //     proximal centring it cannot: on a flat face p = -(1/delta) P_Z g,
        //     so a reduced gradient of 1e-17 produces a step of 2.8e-09 against
        //     a step_tol of 1e-09, and 300 consecutive full steps were measured
        //     around a point whose rows were met to 5e-14 (E3 §3e). delta*||p||
        //     is the reduced gradient's size; ||p|| is not.
        //
        //   STATIONARY ON THE WORKING SET: the free-block stationarity residual
        //     is within opt_tol of its own scale AND no row or bound carries a
        //     STRUCTURAL violation -- the engine's two existing tests, the same
        //     two the terminal classification below decides kOptimal on, asked
        //     here so the loop can RECOGNISE the point it is standing on. Both
        //     halves are needed: the residual alone says the multipliers price
        //     the gradient, and the rows say the iterate is actually ON the face
        //     it priced them over (under the proximal centring a row residual r
        //     contributes a step of order r/delta, so a point off its rows is
        //     exactly the case ||p|| would have caught and no longer does).
        //
        // NOTHING MOVES ON THE SECOND DISJUNCT. The snap `x = eqp.x` is taken
        // only for zero motion, where it moves the iterate by at most step_tol;
        // a stationary point is already the answer, and stepping to the proximal
        // minimiser from it would move x by up to opt_tol*scale/delta with no
        // ratio test having been run. `Aix` and the shifts are therefore still
        // exactly as the previous pass's refresh_shifts left them for this x,
        // which is why no refresh is needed (or taken) on that path.
        // ==================================================================
        const bool zero_motion = p_inf <= step_tol;
        const bool stationary_on_ws =
            !zero_motion && detail::free_block_is_stationary(stat_free_inf, stat_scale, eff_opts) &&
            worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1, lambda_e,
                                       eff_opts) == 0.0;
        if (zero_motion || stationary_on_ws) {
            // A KKT point of the current working set: snap onto it (zero motion
            // only -- see above), then let the multipliers decide.
            if (zero_motion) {
                x = eqp.x;
                refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                               eff_opts);
            }
#ifdef HVEN_TESTING
            // Claimed here and overridden below by whichever branch of the
            // classification actually runs: reaching this line means the pass
            // was a KKT point of the current working set.
            walk_obs.record().action = detail::testing::QpWalkAction::kDropDantzig;
#endif
            double exempt_worst = 0.0;
            const bool dropped_one =
                eff_opts.independent_working_set
                    ? drop_worst(qp, shift, lambda_i, z, ws, last_drop, counters, &seen,
                                 &exempt_worst)
                    : drop_worst(qp, shift, lambda_i, z, ws, last_drop, counters);
            if (dropped_one && eff_opts.independent_working_set) {
                pt_release(seen, last_drop.is_ineq, last_drop.idx);
            }
            if (!dropped_one) {
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kTerminal;
#endif
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
                double worst = worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i,
                                                          ae_row_norm1, lambda_e, eff_opts);
                // THE VERDICT-SITE FACE REFINEMENT, at a would-be kInfeasible
                // only, and on whichever twin the CANDIDATE'S PROVENANCE picks
                // -- section 5, and the two refine_*_for_verdict declarations.
                const bool refined =
                    worst > 0.0 && (eqp.refine_steps > 0
                                        ? refine_face_for_verdict(qp, ws, x, Aix, ai_row_norm1,
                                                                  lambda_i, ae_row_norm1, lambda_e,
                                                                  *border_, counters, eff_opts)
                                        : refine_eliminated_face_for_verdict(
                                              qp, ws, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1,
                                              lambda_e, kkt, face, counters, eff_opts));
                if (refined) {
                    // x moved: restore this loop's invariants, then re-read
                    // the verdict off the refined point -- which is also what
                    // a kInfeasible exit now returns.
                    refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                                   eff_opts);
                    worst = worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i,
                                                       ae_row_norm1, lambda_e, eff_opts);
                }
                // THE TERMINAL MU-FREE REFINEMENT (E2 fix4): before any
                // kInfeasible-by-violation verdict, a residual the dual_mu
                // footprint can explain is re-solved mu-free on the face; an
                // adopted point satisfies every row and bound, carries its
                // refined multipliers, and meets the drop test and every gate
                // below again. See refine_terminal_mu_free.
                if (worst > 0.0 && eff_opts.independent_working_set && eff_opts.dual_mu > 0.0 &&
                    mu_footprint_open(qp, ws, x, ai_row_norm1, lambda_i, ae_row_norm1, lambda_e,
                                      eff_opts) &&
                    refine_terminal_mu_free(qp, ws, x, lambda_e, lambda_i, z, counters, eff_opts)) {
                    refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                                   eff_opts);
                    exempt_worst = 0.0;
                    if (drop_worst(qp, shift, lambda_i, z, ws, last_drop, counters, &seen,
                                   &exempt_worst)) {
                        // The mu-free multipliers release a member the
                        // regularized ones held: the walk goes on from the
                        // refined (feasible) point.
                        pt_release(seen, last_drop.is_ineq, last_drop.idx);
#ifdef HVEN_TESTING
                        walk_obs.record().action = detail::testing::QpWalkAction::kDropDantzig;
#endif
                        continue;
                    }
                    worst = worst_structural_violation(qp, x, Aix, ai_row_norm1, lambda_i,
                                                       ae_row_norm1, lambda_e, eff_opts);
                }
                if (worst > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {
                    status = QpStatus::kNumericalError;
                } else if (verdict == detail::InertiaVerdict::kWrong &&
                           // R-C': re-read; an overturned kWrong goes on to
                           // the suspect-stall gate below.
                           !reread_wrong_inertia(qp, ws, x, counters, eff_opts, verdict)) {
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
                        repair_temporary_vertex(qp, ws, x, kkt, face, *border_, counters,
                                                eff_opts)) {
                        post_probe_restart_spent = true;
                        recheck_pending = eff_opts.independent_working_set;
                        pt_clear(seen);
#ifdef HVEN_TESTING
                        walk_obs.record().action = detail::testing::QpWalkAction::kPostProbeRestart;
#endif
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
                    // THE ZERO-MOTION EXHAUSTION TEST: every remaining drop
                    // candidate is a constraint released and re-entered at
                    // this point. A multiplier above opt_tol * stat_scale is
                    // real, and its release direction re-enters: kNumericalError.
                    if (exempt_worst > 0.0) {
                        double ex_scale = 1.0;
                        (void)detail::free_block_stationarity(qp, x, ws, lambda_e, lambda_i,
                                                              ex_scale);
                        if (exempt_worst > eff_opts.opt_tol * ex_scale) {
                            ++counters.zero_motion_exhausted;
                            status = QpStatus::kNumericalError;
                            break;
                        }
                    }
                    probe.refresh(ws);
                    if (probe_zero_multiplier_drops(qp, x, shift, lambda_i, z, ws, kkt, face,
                                                    *border_, counters, probe, last_drop,
                                                    eff_opts)) {
                        if (eff_opts.independent_working_set) {
                            pt_release(seen, last_drop.is_ineq, last_drop.idx);
                        }
                        // Negative curvature the full labeling hid: the
                        // drop is real. It arms the POST-PROBE RESTART
                        // above, which is what actually answers this case
                        // -- 4c's ride is vacuous for a zero-multiplier
                        // drop.
                        probe_drop_made = true;
                        std::fill(seen.shift_refused.begin(), seen.shift_refused.end(), 0);
#ifdef HVEN_TESTING
                        walk_obs.record().action = detail::testing::QpWalkAction::kDropZeroProbe;
#endif
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
#ifdef HVEN_TESTING
                            walk_obs.record().action =
                                detail::testing::QpWalkAction::kSuspectEscalation;
#endif
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
                        // THE TERMINAL POLISH (M7 W2 E4 fix1, ruling E4-11). This
                        // is the walk's ONE certified exit -- the 4b ladder's
                        // certified rung reaches it too, by falling through the
                        // stationarity gate above -- and it is where the final
                        // point gets the regularization taken back OUT of it.
                        //
                        // WHY IT IS NEEDED AND WHY IT IS HERE. The KKT-point test
                        // of section 5 stops as soon as the free-block
                        // stationarity residual is within opt_tol of its scale;
                        // the walk it replaced kept stepping until ||p|| fell
                        // under step_tol, which on these faces meant grinding
                        // several decades past that tolerance. Stopping at the
                        // tolerance is correct and it leaves the regularization's
                        // O(primal_delta) and O(dual_mu*|lambda|) footprint in the
                        // terminal point -- and dual_mu*|lambda| left in a working
                        // row's slack is dual_mu*lambda^2 in the complementarity a
                        // consumer measures, which is the B-1 term. So the point
                        // is re-solved on THIS face with the regularization backed
                        // out, and the answer is adopted only if it is better by
                        // every one of the three KKT residuals and feasible
                        // (polish_terminal_point states the rule).
                        //
                        // PLACED BEFORE THE CERTIFICATE, so the bound half below
                        // and the certificate seam read the point that is actually
                        // exported. It cannot manufacture a certificate: the
                        // acceptance test refuses any candidate whose rows or
                        // bounds are worse than the incumbent's, or outside
                        // feas_tol where the incumbent met it.
                        //
                        // NOTHING ABOVE IS RE-DECIDED. The polish moves x by at
                        // most the bias it removes, does not touch the working set,
                        // and runs after every drop, probe and gate has had its
                        // say, so no branch of the walk can be reached differently
                        // because of it -- which is also why it moves no minor
                        // count.
                        if (eff_opts.dual_mu > 0.0 || eff_opts.primal_delta > 0.0) {
                            if (polish_footprint_open(qp, ws, x, lambda_e, lambda_i, z, eff_opts)) {
                                (void)polish_terminal_point(qp, ws, eqp, x, lambda_e, lambda_i, z,
                                                            counters, eff_opts);
                            }
                        }
                        // THE CERTIFICATE'S BOUND HALF. worst_structural_violation
                        // reads the rows only, and nothing above reads x against
                        // its own box -- the walk keeps it there, the certificate
                        // must not assume so. Each bound is read as the row e_i
                        // (norm1 = 1, multiplier z(i)) under the rows' own test,
                        // against `qp`'s box, so the TR window is included. A
                        // structural violation is the primal-infeasible exit.
#ifdef HVEN_TESTING
                        // Certificate seam: an armed fixture presents (x, z).
                        detail::testing::qp_certificate_present(x, z, eff_opts);
#endif
                        const double xmag = x.lpNorm<Eigen::Infinity>();
                        double bound_worst = 0.0;
                        for (Index i = 0; i < n; ++i) {
                            const double below = qp.lower(i) - x(i);
                            const double above = x(i) - qp.upper(i);
                            const double v = std::max(below, above);
                            const double bound = below >= above ? qp.lower(i) : qp.upper(i);
                            const double row_scale = std::max({1.0, std::abs(bound), xmag});
                            if (v > 0.0 && violation_is_structural(v, row_scale, z(i), eff_opts)) {
                                bound_worst = std::max(bound_worst, v);
                            }
                        }
                        status = bound_worst > 0.0 ? QpStatus::kInfeasible : QpStatus::kOptimal;
                    }
                }
                break;
            }
#ifdef HVEN_TESTING
            // =================================================================
            // M7 W2 M5's DIAGNOSTIC HALF, post-drop. The working set here has
            // the drop APPLIED -- it is exactly the set the next pass solves
            // over -- so both readings below describe the subspace the drop
            // just opened, which is the object M4c §6 items 1 and 2 say the
            // existing columns cannot reach.
            // =================================================================
            {
                using Adeg = detail::testing::QpDegeneracyArmState;
                // (1) THE INTEGER DEPENDENCY SCREEN (§B.1), free. Equality
                // rows are always in the KKT and each pinned bound removes one
                // degree of freedom exactly as a row does, so `me + |W|` rows
                // in R^n with `me + |W| > n` is an integer PROOF of linear
                // dependence -- sufficient, never necessary, so a 0 verdict is
                // "not proved dependent" and never "independent".
                const Index dep_rows = qp.me() + detail::testing::qp_walk_ws_size(ws);
                detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
                    r.dep_rows_after_drop = dep_rows;
                    r.dep_n = qp.n();
                    r.dep_screen_after_drop = dep_rows > qp.n() ? 1 : 0;
                });

                // (2) THE POST-DROP INERTIA AND RANK EVIDENCE, gated, and the
                // ONE thing in this round that is not free: one extra
                // ASSEMBLE+FACTORIZE per drop pass, on a traced run only.
                //
                // WHY IT IS BIT-NEUTRAL. `eliminated_candidate` is called
                // DIRECTLY rather than through `eqp_candidate`, because
                // `eqp_candidate` branches on `opts_.ws_algebra` -- the ENGINE
                // MEMBER, which no local options copy can override -- and the
                // border path would touch the live, incrementally updated
                // Schur stack. The direct call touches a LOCAL factor and a
                // LOCAL face and never `border_`; the counters it moves are
                // saved and restored; and the observer's EQP scratch is saved,
                // read and restored, so this pass's own recorded evidence (the
                // MAIN call's, snapshotted long before this point) cannot be
                // overwritten. Nothing the walk reads afterwards is touched.
                //
                // WHAT IT BUYS OVER THE EXISTING COLUMNS: the reading is the
                // bound-ELIMINATED K's own backend triple against
                // `(n_free, me + n_working)`, on EVERY configuration -- never
                // K0's triple against a Haynsworth-adjusted expectation, which
                // M4c §6 item 4 names as a limit of what the shipped trace can
                // say.
                if (Adeg::diag_post_drop_inertia && detail::testing::QpWalkObserver::capturing) {
                    if (!diag_kkt) {
                        diag_kkt = std::make_unique<detail::KktFactor>(threads_);
                    }
                    const QpCounters saved_counters = counters;
                    const detail::testing::QpWalkEqpEvidence saved_eqp =
                        detail::testing::QpWalkObserver::eqp;
                    detail::testing::QpWalkObserver::eqp = detail::testing::QpWalkEqpEvidence{};
                    detail::InertiaVerdict diag_verdict = detail::InertiaVerdict::kOk;
                    bool diag_ok = true;
                    try {
                        (void)eliminated_candidate(qp, ws, x, *diag_kkt, diag_face, counters,
                                                   diag_verdict, eff_opts);
                    } catch (...) {
                        // SWALLOWED AND COUNTED. A diagnostic may not end a
                        // solve that would otherwise have finished, and a
                        // hypothetical working set is exactly where an
                        // assembly or factorization is entitled to fail.
                        diag_ok = false;
                        ++Adeg::diag_factorization_failures;
                    }
                    const detail::testing::QpWalkEqpEvidence diag_ev =
                        detail::testing::QpWalkObserver::eqp;
                    detail::testing::QpWalkObserver::eqp = saved_eqp;
                    counters = saved_counters;
                    ++Adeg::diag_factorizations;
                    if (diag_ok) {
                        detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
                            r.post_drop_factorized = true;
                            r.post_drop_verdict = static_cast<int>(diag_verdict);
                            r.post_drop_inertia_state = diag_ev.state;
                            r.post_drop_inertia_pos = diag_ev.n_pos;
                            r.post_drop_inertia_neg = diag_ev.n_neg;
                            r.post_drop_inertia_zero = diag_ev.n_zero;
                            r.post_drop_inertia_zero_derived = diag_ev.zero_is_derived;
                            r.post_drop_inertia_perturbed = diag_ev.perturbed_pivots;
                            r.post_drop_expected_pos = diag_ev.expected_pos;
                            r.post_drop_expected_neg = diag_ev.expected_neg;
                        });
                    }
                }
            }
#endif
            std::fill(seen.shift_refused.begin(), seen.shift_refused.end(), 0);
            continue;
        }

        // ==================================================================
        // THE RUNAWAY GUARD, IN THE ITERATE'S CURRENCY, ON A MOVING PASS
        // (section 5, M7 W2 E4 ruling E4-3). `is_runaway` is the SAME predicate
        // the terminal classification asks -- a free component past
        // detail::unbounded_artifact_scale() on a side it is unbounded on -- and
        // what E4 adds is asking it HERE, on a pass that is about to take
        // another step.
        //
        // WHY IT HAS TO BE ASKED HERE. Under the absolute centring the artifact
        // was self-limiting: the regularized answer at the ridge IS the ridge's
        // fixed point, so the next pass's p vanished, the KKT-point test fired,
        // and the classification below read the runaway off the iterate. Under
        // the proximal centring p is the STEP, so the same 1e+08-sized answer is
        // proposed again from the point it just reached, and again, and the walk
        // burns its cap instead of reporting the artifact (E3 §3h: all three
        // runaway fixtures fail). Asked here, the fixtures' own sequence is
        // restored exactly: the first pass takes the ridge step, and the pass
        // that would take a SECOND one reports kNumericalError at an iterate
        // that has genuinely run away -- which is what those fixtures assert
        // (|x(0)| > 1e7 AND the status), and why the guard is placed after the
        // step rather than before it.
        // ==================================================================
        if (is_runaway(qp, x, ws, eff_opts)) {
            status = QpStatus::kNumericalError;
#ifdef HVEN_TESTING
            walk_obs.record().action = detail::testing::QpWalkAction::kTerminal;
#endif
            break;
        }

        // NEGATIVE-CURVATURE RIDE (section 4c). Armed only on the
        // iteration that follows a drop, and only for a step the loop
        // considers real: on a NEGLIGIBLE p (handled above) the direction
        // is rounding noise and section 4b's certification branch is the
        // right judge.
        //
        // `ride_blocked`: the ride's first blocker tested dependent, and the
        // add site below decides it along `ride.dir` (THE DECLINED RIDE).
        bool ride_blocked = false;
        RideBlocker ride;
        if (dropped.active) {
            const double curvature =
                p.dot(qp.H.selfadjointView<Eigen::Upper>() * p) / p.squaredNorm();
#ifdef HVEN_TESTING
            // The seam's record of section 4c's GATE: the quotient the loop
            // just computed, the threshold it is about to be compared against,
            // and the quotient's numerator taken separately so "the direction
            // is not a negative-curvature one" is decidable without
            // reconstructing it. The numerator is recomputed into observer
            // storage rather than lifted out of the expression above: the
            // loop's own arithmetic is left exactly as it was.
            {
                detail::testing::QpWalkMinorRecord &r = walk_obs.record();
                r.curvature = curvature;
                r.curvature_tol = curvature_tol;
                r.p_hp = p.dot(qp.H.selfadjointView<Eigen::Upper>() * p);
            }
#endif
            if (curvature <= curvature_tol) {
                // The EQP just solved is unbounded below along +/-p within
                // this working set, so its "solution" is a saddle. Ride
                // the direction instead; a refused ride never falls through
                // to a step that re-enters the released constraint (THE
                // DECLINED RIDE, section 4c).
                // p.squaredNorm() is nonzero here: the negligible case
                // returned above.
#ifdef HVEN_TESTING
                // The gate FIRED: the ride was actually offered this direction,
                // which is the fact an ordinary zero-step add cannot establish.
                walk_obs.record().curvature_fired = true;
#endif
                const RideOutcome outcome = ride_negative_curvature(
                    qp, dropped, counters, seen, p, Aix, ws, x,
                    eff_opts.independent_working_set ? &pass_view : nullptr, eff_opts, ride);
#ifdef HVEN_TESTING
                if (outcome == RideOutcome::kStepped || outcome == RideOutcome::kUnbounded) {
                    walk_obs.record().action = outcome == RideOutcome::kStepped
                                                   ? detail::testing::QpWalkAction::kRideStepped
                                                   : detail::testing::QpWalkAction::kRideUnbounded;
                    walk_obs.record().ride = outcome == RideOutcome::kStepped;
                    walk_obs.record().ride_outcome =
                        outcome == RideOutcome::kStepped
                            ? detail::testing::QpWalkRideOutcome::kStepped
                            : detail::testing::QpWalkRideOutcome::kUnbounded;
                }
                // A refusal leaves the outcome to the two hooks inside
                // ride_negative_curvature and ride_sign, which are the only
                // places the THREE refusals are distinguishable at all; a
                // dependent blocker is named here.
                if (outcome == RideOutcome::kDependentBlocker) {
                    walk_obs.record().ride_outcome =
                        detail::testing::QpWalkRideOutcome::kDependentBlocker;
                }
#endif
                if (outcome == RideOutcome::kUnbounded) {
                    // Nothing blocks: genuinely unbounded. Same verdict and
                    // same semantics (final iterate reported, multipliers
                    // cleared below) as section 4b's certification branch.
                    status = QpStatus::kNumericalError;
                    break;
                }
                if (outcome == RideOutcome::kStepped) {
                    // Observation only, and broken by a ride whether or not it
                    // moved (QpCounters::degenerate_run_max).
                    degenerate_run = 0;
                    // A RIDE THAT DID NOT MOVE IS A ZERO-MOTION EVENT (4c,
                    // FINITENESS), on the ordinary add path's own test: the point
                    // memory survives it and the blocker is admitted through it,
                    // so the release/re-entry cap bounds a drop -> zero-length
                    // ride -> drop cycle.
                    if (ride.dx_inf > step_tol) {
                        pt_clear(seen);
                    }
                    (void)pt_admit(seen, counters, ride.kind == BlockKind::kIneq, ride.idx);
                    refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen,
                                   eff_opts);
                    continue;
                }
                if (outcome == RideOutcome::kDependentBlocker) {
                    ride_blocked = true;
                    ++counters.dep_ride_blockers;
                } else if (outcome == RideOutcome::kVetoed && eff_opts.independent_working_set) {
                    // THE DECLINED RIDE, refusals (i) and (ii): the ordinary
                    // step is taken only for a null-space veto whose p leaves
                    // the released constraint; otherwise the drop is undone.
                    const bool sign_veto = ride_stays_in_working_set(qp, ws, p);
                    const double leaves_tol =
                        detail::kEngineDenomTol * dropped_gradient_norm(qp, dropped) * p.norm();
                    if (sign_veto) {
                        ++counters.ride_sign_vetoes;
                    }
                    if (sign_veto || dropped_directional(qp, dropped, p) >= -leaves_tol) {
                        if (dropped.is_ineq) {
                            ws.add_ineq(dropped.idx);
                            mark_ineq_seen(seen, counters, dropped.idx);
                        } else {
                            ws.bound_state()[static_cast<std::size_t>(dropped.idx)] = dropped.from;
                            x(dropped.idx) = dropped.from == BoundState::kAtUpper
                                                 ? qp.upper(dropped.idx)
                                                 : qp.lower(dropped.idx);
                            ++counters.ws_adds_bound;
                            mark_bound_seen(seen, counters, dropped.idx);
                        }
                        ++counters.ws_adds;
                        (void)pt_admit(seen, counters, dropped.is_ineq, dropped.idx);
                        if (sign_veto && ride_drop_is_noise(qp, dropped, p, x)) {
                            // THE FLAT FACE (4c): the drop's own multiplier is
                            // below the resolution of the slope it was meant to
                            // predict, so it does not count as exhausted here.
                            pt_mark(seen, dropped.is_ineq, dropped.idx, kPtNoise);
                        }
                        ++counters.ride_declines_fallthrough_blocked;
#ifdef HVEN_TESTING
                        {
                            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
                            r.action = detail::testing::QpWalkAction::kRideUndone;
                            r.block = dropped.is_ineq ? detail::testing::QpWalkBlock::kIneq
                                                      : detail::testing::QpWalkBlock::kBound;
                            r.block_index = dropped.idx;
                            if (!dropped.is_ineq) {
                                r.block_side = dropped.from;
                            }
                        }
#endif
                        continue;
                    }
                }
            }
        }

        BlockKind kind = BlockKind::kNone;
        Index block_idx = -1;
        BoundState block_state = BoundState::kFree;
        // The step's direction: the EQP step, or a declined ride's (uncapped).
        const Vec &sdir = ride_blocked ? ride.dir : p;
        if (ride_blocked) {
            kind = ride.kind;
            block_idx = ride.idx;
            block_state = ride.state;
        }
#ifdef HVEN_TESTING
        // M7 W2 M5's ARM B1 may re-decide WHICH constraint blocks, and with it
        // the step, so the step is a MUTABLE local on this path; the `#else`
        // arm below is production's statement, `const` and all, token for
        // token, and the call here is the same call.
        double alpha = ride_blocked ? ride.alpha
                                    : ratio_test(qp, ws, x, Aix, p, kind, block_idx, block_state,
                                                 /*cap_at_unit=*/true, counters);
        // =================================================================
        // ARM B1 -- THE INDEPENDENCE-PRESERVING RATIO TEST
        // (`qp_degeneracy_arm.h`'s ARM B1 block, settler ruling R3).
        //
        // IT RUNS HERE AND NOT INSIDE `ratio_test`, deliberately. The
        // dependency test needs the pass's EFFECTIVE options (the `delta`/`mu`
        // the local factor is assembled at) and a `KktFactor` to build it in,
        // neither of which `ratio_test` is given; and the negative-curvature
        // RIDE reaches `ratio_test` too, while the brief's minimal
        // intervention is the ORDINARY add path alone. Placed here, the arm is
        // asked exactly once per pass, on exactly the decision it is about.
        //
        // WHEN THE ARM PASSES NOTHING OVER IT CHANGES NOTHING AT ALL -- not
        // the blocker, not the step, not a single statement below -- so an
        // armed run on a walk with no dependent blocker is production BY
        // CONSTRUCTION.
        detail::testing::QpB1Outcome b1;
        if (detail::testing::QpDegeneracyArmState::independent_add_on() && !ride_blocked &&
            kind != BlockKind::kNone) {
            // THE ORACLE IS THE ARM'S OWN (amendment A1): a dense
            // rank-revealing QR of the UNREGULARIZED active rows, built inside
            // `b1_select_blocker` from `qp` and `ws` alone. No `KktFactor` is
            // constructed, borrowed or passed in here, on any algebra path --
            // which is a stronger neutrality claim than the superseded design
            // could make, since that one shared the post-drop diagnostic's
            // local factor.
            //
            // `eff_opts.dual_mu` and the working multipliers the pass just
            // priced are handed in rather than re-derived, because A5's leak
            // columns are expressed in units of `mu*||lambda_W||_inf` and two
            // derivations of the same quantity can drift.
            b1 = detail::testing::b1_select_blocker(
                qp, ws, x, Aix, p, eff_opts.dual_mu,
                detail::testing::b1_working_multiplier_inf(qp, ws, lambda_e, lambda_i),
                kind == BlockKind::kIneq, block_idx);
            if (b1.switched) {
                kind = !b1.has_blocker ? BlockKind::kNone
                       : b1.is_ineq    ? BlockKind::kIneq
                                       : BlockKind::kBound;
                if (b1.has_blocker) {
                    block_idx = b1.idx;
                    if (!b1.is_ineq) {
                        block_state = b1.state;
                    }
                }
                alpha = b1.alpha;
            }
        }
#else
        double alpha = ride_blocked ? ride.alpha
                                    : ratio_test(qp, ws, x, Aix, p, kind, block_idx, block_state,
                                                 /*cap_at_unit=*/true, counters);
#endif

        // THE ADD SITE (independent_working_set). The blocker is tested
        // against the factor this pass holds for `ws`. Independent: admitted.
        // Dependent: exchanged for a member (ExchangePick); with no admissible
        // member, (N1) a VERIFIED Farkas certificate ends kInfeasible, else
        // (N2) the blocker is passed over when its predicted violation at the
        // end of the step stays within feas_tol * row_scale (the next blocker
        // goes through this same rule), else (N3) the solve ends
        // kNumericalError at the current iterate -- never a step past it.
        ExchangePick xpick;
        if (eff_opts.independent_working_set) {
            enum class AddExit { kNone, kInfeasible, kToleranced, kRejected, kTerminate };
            AddExit add_exit = AddExit::kNone;
            std::vector<std::pair<bool, Index>> passed;
            const double xmag = x.lpNorm<Eigen::Infinity>();
            // The oriented blocker c^T x <= d: a row, an upper hit (e_i), or
            // a lower hit (-e_i); its value at x, its rate along p, d, and
            // its feasibility scale.
            struct Oriented {
                double cx, cp, d, scale;
            };
            const auto oriented = [&](bool is_ineq, Index idx, BoundState side) {
                if (is_ineq) {
                    return Oriented{
                        Aix(idx), qp.Ai.row(idx).dot(sdir), qp.bi(idx),
                        std::max({1.0, std::abs(qp.bi(idx)), ai_row_norm1(idx) * xmag})};
                }
                const double sgn = side == BoundState::kAtUpper ? 1.0 : -1.0;
                const double bound = side == BoundState::kAtUpper ? qp.upper(idx) : qp.lower(idx);
                return Oriented{sgn * x(idx), sgn * sdir(idx), sgn * bound,
                                std::max({1.0, std::abs(bound), xmag})};
            };
            bool undecidable_retreat = false;
            bool ride_first = ride_blocked;
            while (kind != BlockKind::kNone) {
                const bool is_ineq = kind == BlockKind::kIneq;
                const bool moves = alpha * sdir.lpNorm<Eigen::Infinity>() > step_tol;
                // A declined ride's first blocker was tested inside the ride.
                detail::DepTest t = ride_first ? std::move(ride.test)
                                               : test_candidate(qp, ws, pass_view, is_ineq,
                                                                block_idx, counters, eff_opts);
                ride_first = false;
                // A member the previous pass's re-check removed as a dependent
                // deferred add, blocking again at this same point (E2 fix1,
                // D3(b)): the detector's verdict was a proof, so only a
                // DECIDED dependent test (an exchange) may bring it back here.
                const bool known_dependent =
                    !moves && (pt_flags(seen, is_ineq, block_idx) & kPtKnownDependent) != 0;
                if (!t.dependent() && !known_dependent) {
                    break; // the ordinary add
                }
                bool refused = false;
                if (t.verdict == detail::DepVerdict::kUndecidable && pass_view.schur != nullptr) {
                    // (D3(a)) THE BORDER RETREAT for an undecidable test: a
                    // stalled refinement on the border path is the swamped-C
                    // class (E2-F1), so the eliminated path's test decides.
                    undecidable_retreat = true;
                    break;
                }
                if (t.verdict != detail::DepVerdict::kDependent) {
                    // (D3(c)) An exact structural dependency -- a pin that
                    // completes a member row on pinned columns, or a row on
                    // pinned columns only -- decides it, coefficients read
                    // off the row.
                    const detail::StructuralDependency sd =
                        detail::structural_dependency(qp, ws, is_ineq, block_idx);
                    if (sd.found) {
                        t = sd.test;
                        ++counters.dep_structural_exact;
                    }
                }
                if (t.verdict != detail::DepVerdict::kDependent && known_dependent) {
                    // (D3(b)) Never re-admitted undecided at zero motion:
                    // no coefficients, so no exchange and no certificate;
                    // the pass-over (N2) or the end (N3) below.
                    refused = true;
                    ++counters.dep_known_dependent_refusals;
                    xpick = ExchangePick{};
                } else if (t.verdict == detail::DepVerdict::kUndecidable) {
                    // Screen-negative and unresolved (no factor for this
                    // pass's set, or a stalled refinement): nothing proves
                    // it dependent and no coefficient can pick an exchange.
                    // Admitted; the next pass's detector tests the grown set
                    // on its own factor, as for a shift add. Found dependent
                    // there, it is removed under dep_recheck_removals and
                    // marked known-dependent at that point (D3(b)).
                    ++counters.dep_deferred_adds;
                    recheck_pending = true;
                    deferred_member = detail::WsMember{is_ineq ? detail::WsMember::Kind::kRow
                                                               : detail::WsMember::Kind::kPin,
                                                       block_idx};
                    break;
                }
                if (!refused) {
                    const Oriented o = oriented(is_ineq, block_idx, block_state);
                    // The entered-at-point lock applies only on a zero-motion
                    // step: the re-entry rule guards zero-motion cycles, and a
                    // step that moves clears the point memory before the
                    // exchange mutates.
                    xpick = select_exchange(qp, ws, shift, seen, t, is_ineq, block_idx, block_state,
                                            lambda_e, lambda_i, z, moves, /*unlock=*/false);
                    if (xpick.found) {
                        break; // the exchange
                    }
                    // (N1) The certificate, tried whenever no member is
                    // admissible (E2 fix1, D1: a locked positive-coefficient
                    // member leaves cond1 false, and the box-based
                    // certificate never needed it; design amendment in the
                    // E2 README). Verified before it may be returned, its
                    // multipliers refined mu-free first (S25'). Only a
                    // certificate within the rounding floor is a Farkas
                    // certificate for THIS QP => kInfeasible. One above it
                    // dropped a box_part coefficient on a coordinate unbounded
                    // on that side, which can make the omitted term
                    // arbitrarily large: it proves the coefficient-perturbed
                    // QP infeasible, not this one => kNumericalError where
                    // the walk stands, the toleranced certificate kept
                    // (settler S25, S25').
                    QpFarkasCertificate cert;
                    Index cert_solves = 0;
                    const bool verified =
                        verify_farkas(qp, ws, pass_view, t, xpick, is_ineq, block_idx, o.scale,
                                      eff_opts, cert, cert_solves);
                    counters.dep_test_solves += cert_solves;
                    if (verified) {
                        add_exit = cert.residual <= cert.exact_tol ? AddExit::kInfeasible
                                                                   : AddExit::kToleranced;
                        farkas = std::move(cert);
                        break;
                    }
                    // The design's candidate (cond1 and a structural
                    // violation) that failed verification.
                    if (xpick.cond1 && violation_is_structural(xpick.g, o.scale, 0.0, eff_opts)) {
                        add_exit = AddExit::kRejected;
                        break;
                    }
                }
                // (N2) The pass-over: the step with this blocker (and every
                // one passed over before it) excluded, if none of them ends
                // it violated by more than feas_tol * row_scale.
                passed.emplace_back(is_ineq, block_idx);
                BlockKind k2 = BlockKind::kNone;
                Index i2 = -1;
                BoundState s2 = BoundState::kFree;
                const double a2 = ratio_test_excluding(qp, ws, x, Aix, sdir, passed, k2, i2, s2,
                                                       /*cap_at_unit=*/!ride_blocked);
                bool within = true;
                for (const auto &[pi, pidx] : passed) {
                    // A passed-over bound is oriented by the side it would
                    // have been hit on.
                    const BoundState side = pi                 ? BoundState::kFree
                                            : sdir(pidx) > 0.0 ? BoundState::kAtUpper
                                                               : BoundState::kAtLower;
                    const Oriented po = oriented(pi, pidx, side);
                    if (std::max(0.0, po.cx - po.d + a2 * po.cp) > eff_opts.feas_tol * po.scale) {
                        within = false;
                    }
                }
                if (!within) {
                    // R-B: the rule would END the solve here (no admissible
                    // member, no verified certificate, no pass-over): on a
                    // zero-motion step off the border path (the border retreat
                    // comes first there), unlock the members locked only by
                    // having entered at this point (ExchangePick's header).
                    if (!refused && !moves && pass_view.schur == nullptr) {
                        ExchangePick u =
                            select_exchange(qp, ws, shift, seen, t, is_ineq, block_idx, block_state,
                                            lambda_e, lambda_i, z, moves, /*unlock=*/true);
                        if (u.found) {
                            xpick = std::move(u);
                            break; // the exchange
                        }
                    }
                    add_exit = AddExit::kTerminate;
                    break;
                }
                ++counters.dep_passovers;
                kind = k2;
                block_idx = i2;
                block_state = s2;
                alpha = a2;
            }
            if (undecidable_retreat ||
                ((add_exit == AddExit::kTerminate || add_exit == AddExit::kRejected ||
                  add_exit == AddExit::kToleranced) &&
                 pass_view.schur != nullptr)) {
                // THE BORDER RETREAT, once per solve. On the border path the
                // Schur complement of an ill-conditioned K0 can be swamped by
                // rounding: the EQP step leaves its face and the dependency
                // coefficients are inaccurate (a screen-positive test whose
                // residual is far above kDepTol proves it). The pass is redone
                // on the bound-eliminated path the engine already has, before
                // the rule may end the solve.
                eliminated_only = true;
                ++counters.dep_border_retreats;
                farkas = QpFarkasCertificate{}; // a toleranced one is re-derived there
                if (ride_blocked) {
                    last_drop = dropped; // the ride is re-taken on the redone pass
                }
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kEliminatedRetreat;
#endif
                continue;
            }
            if (add_exit != AddExit::kNone) {
                // The iterate the walk stands on: no step past the blocker.
                if (add_exit == AddExit::kInfeasible) {
                    ++counters.dep_infeasible_certificates;
                    status = QpStatus::kInfeasible;
                } else if (add_exit == AddExit::kToleranced) {
                    // Not a proof for this QP (above): conservative, and the
                    // certificate goes out in QpSolution::farkas.
                    ++counters.dep_certificate_toleranced;
                    status = QpStatus::kNumericalError;
                } else {
                    ++(add_exit == AddExit::kRejected ? counters.dep_certificate_rejections
                                                      : counters.dep_blocker_terminations);
                    status = QpStatus::kNumericalError;
                }
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kTerminal;
#endif
                break;
            }
            if (ride_blocked && kind == BlockKind::kNone) {
                // Every blocker passed over and nothing else blocks the ride:
                // unbounded, as section 4c's no-blocker branch.
                status = QpStatus::kNumericalError;
#ifdef HVEN_TESTING
                walk_obs.record().action = detail::testing::QpWalkAction::kRideUnbounded;
#endif
                break;
            }
        }

        if (kind != BlockKind::kNone && alpha * sdir.lpNorm<Eigen::Infinity>() <= step_tol) {
            ++counters.degenerate_steps;
            ++degenerate_run;
            counters.degenerate_run_max = std::max(counters.degenerate_run_max, degenerate_run);
#ifdef HVEN_TESTING
            walk_obs.record().degenerate = true;
#endif
        } else {
            degenerate_run = 0;
        }
#ifdef HVEN_TESTING
        {
            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
            r.action = kind == BlockKind::kNone ? detail::testing::QpWalkAction::kMoveOnly
                                                : detail::testing::QpWalkAction::kAdd;
            r.alpha = alpha;
            r.dx_inf = alpha * sdir.lpNorm<Eigen::Infinity>();
            if (kind != BlockKind::kNone) {
                r.block = kind == BlockKind::kBound ? detail::testing::QpWalkBlock::kBound
                                                    : detail::testing::QpWalkBlock::kIneq;
                r.block_index = block_idx;
                if (kind == BlockKind::kBound) {
                    r.block_side = block_state;
                    r.bound_gap = qp.upper(block_idx) - qp.lower(block_idx);
                }
            }
            // =============================================================
            // M7 W2 M5's DIAGNOSTIC HALF, per ADD. In exact arithmetic on an
            // unregularized KKT a constraint linearly DEPENDENT on the working
            // set has `a^T p == 0` exactly -- `p` lies in the working set's
            // null space -- so it cannot block and `kEngineDenomTol` would
            // screen it out. It blocks here anyway on 68.5 % of M4c's core
            // units, and these three numbers are what decide WHY:
            //
            //   `add_a_dot_p`     the blocker's own a^T p;
            //   `add_leak_bound`  `dual_mu * ||lambda||inf`, the size of the
            //                     leak the regularized second block row
            //                     `A_W p = mu * lambda` opens through a
            //                     dependent row;
            //   `add_round_floor` `eps * ||a||2 * ||p||2`, below which a^T p
            //                     says nothing at all.
            //
            // A reader compares them; this hook claims nothing. Everything
            // here is a dot product or a norm over state the loop already
            // holds -- no factorization and no solve.
            if (kind != BlockKind::kNone) {
                const double a_dot_p =
                    kind == BlockKind::kBound ? sdir(block_idx) : qp.Ai.row(block_idx).dot(sdir);
                const double a_norm = kind == BlockKind::kBound ? 1.0 : qp.Ai.row(block_idx).norm();
                double lam_inf = 0.0;
                if (lambda_e.size() > 0) {
                    lam_inf = std::max(lam_inf, lambda_e.lpNorm<Eigen::Infinity>());
                }
                if (lambda_i.size() > 0) {
                    lam_inf = std::max(lam_inf, lambda_i.lpNorm<Eigen::Infinity>());
                }
                const Index dep_rows = qp.me() + detail::testing::qp_walk_ws_size(ws);
                r.add_a_dot_p = a_dot_p;
                r.add_leak_bound = eff_opts.dual_mu * lam_inf;
                r.add_round_floor = std::numeric_limits<double>::epsilon() * a_norm * sdir.norm();
                r.add_dep_rows = dep_rows;
                // The screen is asked of the set WITH the blocker in it, which
                // is the set the add would create: `me + |W| + 1 > n`.
                r.add_dep_screen = dep_rows + 1 > qp.n() ? 1 : 0;
            }
            // ARM B1's OWN RECORD OF THE SCAN THAT REACHED THAT BLOCKER. Only
            // the pass the arm actually ran on fills these; `b1.ran` is false
            // whenever the arm is unarmed, or refused the pass on its own
            // self-check, and every column below then stays absent.
            if (b1.mismatch) {
                r.add_b1_mismatch = 1;
            }
            if (b1.ran) {
                r.add_b1_mismatch = 0;
                r.add_b1_examined = b1.examined;
                r.add_b1_passed_over = b1.passed_over;
                r.add_b1_tie_size = b1.tie_size;
                r.add_b1_switched = b1.switched ? 1 : 0;
                r.add_b1_no_blocker = b1.has_blocker ? 0 : 1;
                r.add_dep_residual = b1.chosen_dep_residual;
                r.add_dep_verdict = b1.chosen_verdict;
                r.dep_n = qp.n();
                // AMENDMENT A2's READING, A8's THREE-MECHANISM SPLIT and
                // A11's COST, per pass.
                r.add_b1_rank_rows = b1.rows;
                r.add_b1_rank = b1.rank;
                // RULE.md AMENDMENT 1, C4's statistic: the QR's own
                // conditioning read for the decomposition this pass decided
                // against. NaN when no decomposition was taken, which is
                // `kAbsent`, so the assignment carries absence too.
                r.add_b1_cond_r = b1.cond_r;
                r.add_b1_qr_builds = b1.qr_builds;
                r.add_b1_screen_qr_passed = b1.screen_and_qr_passed;
                r.add_b1_qr_only_passed = b1.qr_only_passed;
                r.add_b1_disarmed = b1.disarmed ? 1 : 0;
                // AMENDMENT A5's STRENGTHENED LEAK COLUMNS, for the candidate
                // the arm ADMITTED plus the pass's own denominators. The
                // per-candidate columns describe the chosen candidate; the
                // pass-level ones (`max |A_W p|`, `mu*||lambda_W||_inf`) are
                // what every candidate on the pass is read against.
                r.add_b1_max_retained_leak = b1.max_retained_leak;
                r.add_b1_leak_bound = b1.leak_bound;
                if (!b1.passed.empty()) {
                    const detail::testing::QpB1Candidate &first = b1.passed.front();
                    r.add_b1_passed_a_dot_p = first.a_dot_p;
                    r.add_b1_passed_a_dot_p_norm = first.a_dot_p_normalized;
                    r.add_b1_passed_leak_ratio = first.leak_ratio;
                    r.add_b1_passed_coeff_residual = first.coeff_residual;
                    r.add_b1_passed_residual = first.dep_residual;
                }
                if (b1.switched) {
                    r.add_b1_prod_index = b1.prod_idx;
                    r.add_b1_prod_is_ineq = b1.prod_is_ineq ? 1 : 0;
                }
            }
        }
#endif
        if (kind != BlockKind::kNone) {
            ++counters.ws_adds;
            if (kind == BlockKind::kBound) {
                ++counters.ws_adds_bound;
                mark_bound_seen(seen, counters, block_idx);
            } else {
                mark_ineq_seen(seen, counters, block_idx);
            }
        }

        if (ride_blocked) {
            x += alpha * sdir; // a ride step: no landing point, uncapped
        } else {
            x = (alpha >= 1.0) ? eqp.x : Vec(x + alpha * p);
        }
        if (eff_opts.independent_working_set) {
            if (alpha * sdir.lpNorm<Eigen::Infinity>() > step_tol) {
                pt_clear(seen); // the iterate moved
            }
            if (xpick.found) {
                // THE EXCHANGE: remove j, then the blocker joins below; no
                // ride is armed (the null space is unchanged) unless the step
                // is a declined ride's, which is re-armed. Multipliers per
                // Result 3.4(b): m - sigma v, and sigma on the blocker.
                const std::vector<Index> &aw = ws.active_ineq();
                for (std::size_t k = 0; k < aw.size(); ++k) {
                    lambda_i(aw[k]) -= xpick.sigma * xpick.v_w(static_cast<Index>(k));
                }
                if (me > 0) {
                    lambda_e -= xpick.sigma * xpick.v_e;
                }
                for (Index i = 0; i < n; ++i) {
                    const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
                    if (st != BoundState::kFree) {
                        // m = -z on an upper or fixed pin, z on a lower one.
                        z(i) += (st == BoundState::kAtLower ? -1.0 : 1.0) * xpick.sigma *
                                xpick.v_pin(i);
                    }
                }
                if (xpick.j_is_ineq) {
                    ws.drop_ineq(xpick.j);
                    lambda_i(xpick.j) = 0.0;
                    if (xpick.j_shifted) {
                        ++counters.ws_exchanges_shifted;
                        if (!seen.shift_refused.empty()) {
                            seen.shift_refused[static_cast<std::size_t>(xpick.j)] = 1;
                        }
                    }
                } else {
                    ws.bound_state()[static_cast<std::size_t>(xpick.j)] = BoundState::kFree;
                    z(xpick.j) = 0.0;
                }
                if (kind == BlockKind::kIneq) {
                    lambda_i(block_idx) = xpick.sigma;
                } else {
                    z(block_idx) = block_state == BoundState::kAtUpper ? -xpick.sigma : xpick.sigma;
                }
                ++counters.ws_exchanges;
                if (xpick.unlocked) {
                    ++counters.dep_unlocks;
                }
                if (kind == BlockKind::kBound || !xpick.j_is_ineq) {
                    ++counters.ws_exchanges_bound;
                }
                // The release is a drop for the net identity (QpCounters).
                ++counters.ws_drops;
                if (!xpick.j_is_ineq) {
                    ++counters.ws_drops_bound;
                }
                pt_release(seen, xpick.j_is_ineq, xpick.j);
                if (ride_blocked) {
                    // THE DECLINED RIDE (iii): the exchanged set has the same
                    // null space, so the ride is re-armed and re-taken there
                    // on a fresh factor.
                    ++counters.ride_exchanges;
                    last_drop = dropped;
                }
            }
            if (kind != BlockKind::kNone) {
                (void)pt_admit(seen, counters, kind == BlockKind::kIneq, block_idx);
            }
        }
        if (kind == BlockKind::kIneq) {
            ws.add_ineq(block_idx);
        } else if (kind == BlockKind::kBound) {
            ws.bound_state()[static_cast<std::size_t>(block_idx)] = block_state;
#ifdef HVEN_TESTING
            // ARM E, §4.1's Cases 2 and 3: when the step taken was the
            // `alpha_min` FLOOR the blocking variable is DELIBERATELY left
            // violating its bound (by at most tau), and §3.3 is the reason --
            // moving it onto the bound here is an extra, unmodelled step
            // `x <- x + mu e_r` with |mu| <= delta, which is both an error of
            // order delta in the constraints and, at a degenerate vertex,
            // exactly what makes the drop/re-add pair move the iterate by
            // nothing at all. THE QUESTION IS ASKED OF THE ONE SHARED
            // PREDICATE (`qp_anti_cycling_arm.h`) that the RIDE's add path
            // below asks too, so the two cannot answer it differently. The
            // guard is written so that the statement below is PRODUCTION'S,
            // unchanged, token for token.
            if (!detail::testing::QpAntiCycleArmState::expand_skips_exact_repin())
#endif
                x(block_idx) = (block_state == BoundState::kAtUpper) ? qp.upper(block_idx)
                                                                     : qp.lower(block_idx);
        }
#ifdef HVEN_TESTING
        // ARM B1's TWO POST-STEP READ-OUTS, taken HERE -- after the working
        // set and the iterate have been mutated, and BEFORE `refresh_shifts`
        // can add a row of its own -- so both describe what the ARM did and
        // not what the pass did afterwards.
        //
        //   * THE HARD INVARIANT (settler ruling R6): `me + |W| - n <= 0`
        //     after every add the arm admitted. A positive value STOPS the
        //     arm; it is recorded and counted here, never absorbed.
        //   * THE PASSED-OVER CONSTRAINTS' VIOLATION at the iterate the walk
        //     actually moved to -- the price of refusing them, measured
        //     rather than hidden.
        if (b1.ran) {
            using Adeg = detail::testing::QpDegeneracyArmState;
            detail::testing::QpWalkMinorRecord &r = walk_obs.record();
            if (kind != BlockKind::kNone) {
                // THE INTEGER WITNESS, kept because it is free and because a
                // positive value is a PROOF. It is no longer the invariant --
                // amendment A2 made that `rank(R) == me + |W|`, which
                // `admit_if_independent` reads off the decomposition on the
                // NEXT pass and which disarms the solve under A9 if it fails.
                // Recording the count here and the rank there is deliberate:
                // the count after an add is decidable without a factorization,
                // and the rank is not.
                const Index rows_after = qp.me() + detail::testing::qp_walk_ws_size(ws);
                r.add_b1_rows_after = rows_after;
                Adeg::indep_rows_minus_n_max =
                    std::max(Adeg::indep_rows_minus_n_max, rows_after - qp.n());
            }
            // THE PASSED-OVER CONSTRAINTS' VIOLATION at the iterate the walk
            // actually moved to, and AMENDMENT A10's PRE-REGISTERED BOUND on
            // it, applied per constraint. A violation the leak cannot account
            // for means the pass-over was WRONG, which is a finding about the
            // rule; it is counted, not absorbed.
            Index exceeded = 0;
            const double viol = detail::testing::b1_passed_over_violation(qp, x, b1, &exceeded);
            r.add_b1_passed_violation = viol;
            r.add_b1_leak_bound_exceeded = exceeded;
            Adeg::indep_max_passed_violation = std::max(Adeg::indep_max_passed_violation, viol);
        }
#endif
        refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
    }
#ifdef HVEN_TESTING
    detail::testing::QpWalkObserver::end_solve(static_cast<int>(status));
#endif

    // Commit the hot-start reuse cache iff this solve reached a genuine KKT
    // point (INVALIDATION POLICY). kMaxIter/kInfeasible/kNumericalError
    // all leave border_valid_ false: border_ may still hold a good K0
    // factorization, but the working set that produced it was never
    // certified. Reuses current_structural_hash/current_values_hash
    // computed above rather than re-hashing H/Ae/Ai.
    if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder && status == QpStatus::kOptimal &&
        !eliminated_only) {
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
    if (farkas.present &&
        (status == QpStatus::kInfeasible || status == QpStatus::kNumericalError)) {
        out.farkas = std::move(farkas);
    }

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
            if (!seen.shift_refused.empty() &&
                seen.shift_refused[static_cast<std::size_t>(j)] != 0) {
                continue;
            }
#ifdef HVEN_TESTING
            // ARM B1 AT THE SHIFT ADD SITE (amendment A3, site 4 of six).
            //
            // The homotopy puts every row carrying a positive shift into the
            // working set, and it does so with no ratio test and no
            // independence question asked -- which is one of the ways a
            // dependent row enters in the first place. Under the arm a
            // dependent shift row is NOT added. Nothing else changes: the
            // shift stays positive, so the row is offered again on the next
            // refresh, and `counters.shift_adds` counts additions MADE, so a
            // refusal leaves it where it was rather than counting an add that
            // did not happen.
            if (detail::testing::QpDegeneracyArmState::independent_add_on()) {
                const detail::testing::QpB1Admission adm = detail::testing::admit_if_independent(
                    qp, ws, detail::testing::b1_site_rank(), /*is_ineq=*/true, j,
                    detail::testing::QpB1Site::kShiftAdd);
                if (!adm.admit && adm.rank_ok) {
                    ++detail::testing::QpDegeneracyArmState::indep_shift_refusals;
                    continue;
                }
            }
#endif
            ws.add_ineq(j);
            ++counters.shift_adds; // observation only -- see QpCounters
            mark_ineq_seen(seen, counters, j);
            if (!seen.pt_ineq.empty()) {
                (void)pt_admit(seen, counters, /*is_ineq=*/true, j);
            }
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

// See the declaration for what this is for and why it is silent on a
// restoration pass. The lambdas priced here are the CANDIDATE's own
// (eqp.lambda_e / eqp.lambda_w), because the drift being measured is the one
// THOSE multipliers produce.
bool QpEngine::eqp_answer_drifts_off_met_rows(const QpProblem &qp, const WorkingSet &ws,
                                              const Vec &x, const EqpResult &eqp, const Vec &Aix,
                                              const Vec &ai_row_norm1, const Vec &ae_row_norm1,
                                              const QpOptions &opts) const {
    const double xmag = std::max(x.lpNorm<Eigen::Infinity>(), eqp.x.lpNorm<Eigen::Infinity>());
    const auto &aw = ws.active_ineq();
    const Vec r_in_e = qp.me() > 0 ? Vec(qp.Ae * x - qp.be) : Vec();
    const Vec r_out_e = qp.me() > 0 ? Vec(qp.Ae * eqp.x - qp.be) : Vec();
    // THE CLOSED-FACE QUALIFIER, and it is what keeps the check off an
    // INCONSISTENT row set. The invariant being defended is "a step keeps the
    // rows the iterate already met"; it is only defensible when a point meeting
    // ALL of them exists, and the walk's own evidence for that is that the
    // incoming iterate meets all of them. On the canonical inconsistent pair
    // (e_0 = 0 beside e_0 = 1) the incoming iterate meets one row and cannot
    // meet the other, the step MUST leave the one it is on, and the walk's job
    // there is to reach its verdict rather than to hold a row closed. MEASURED:
    // without this qualifier the pre-E2 arm of
    // T7Fix2AFailedStartRepairDoesNotRefineThroughTheProbedFactorization exits
    // the ladder at x0 = 0 instead of walking to the least-squares x0 = 0.5, and
    // ObjectiveInflatedMultiplier's option-off arm loses its point too. Both
    // rows still read kInfeasible, so nothing was wrong -- but a check that
    // speaks where it has nothing to defend is a check that costs answers.
    bool face_closed = true;
    for (Index j = 0; j < qp.me() && face_closed; ++j) {
        const double row_scale = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
        face_closed = std::abs(r_in_e(j)) <= face_row_target(row_scale, eqp.lambda_e(j), opts);
    }
    for (std::size_t k = 0; k < aw.size() && face_closed; ++k) {
        const Index j = aw[k];
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        face_closed = std::abs(Aix(j) - qp.bi(j)) <=
                      face_row_target(row_scale, eqp.lambda_w(static_cast<Index>(k)), opts);
    }
    if (!face_closed) {
        return false;
    }
    // THE TRIGGER IS A GROSS ONE, and deliberately: `face_row_target` is the
    // classifier's threshold (~1e-05 of a row's scale) and a drift that merely
    // clears it is the ordinary accuracy of a regularized solve, not the pathology
    // this check exists for. What it exists for is a drift of ORDER ONE relative to
    // the row's scale -- dual_mu * |lambda| with |lambda| ~ 1/primal_delta -- and
    // firing on anything less buys nothing and costs the whole rest of the solve a
    // raised primal_delta, because the ladder it routes into is global by design.
    // kEqpRowDriftFloor is that floor, MEASURED: see its own note.
    const double gross = detail::kEqpRowDriftFloor;
    for (Index j = 0; j < qp.me(); ++j) {
        const double row_scale = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
        if (std::abs(r_out_e(j)) >
            std::max(face_row_target(row_scale, eqp.lambda_e(j), opts), gross * row_scale)) {
            return true;
        }
    }
    for (std::size_t k = 0; k < aw.size(); ++k) {
        const Index j = aw[k];
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        if (std::abs(qp.Ai.row(j).dot(eqp.x) - qp.bi(j)) >
            std::max(face_row_target(row_scale, eqp.lambda_w(static_cast<Index>(k)), opts),
                     gross * row_scale)) {
            return true;
        }
    }
    return false;
}

double QpEngine::face_row_target(double row_scale, double lambda, const QpOptions &opts) const {
    return std::max(detail::kInfeasibilityMarginFactor * row_tolerance(row_scale, lambda, opts),
                    detail::kVerdictRefineRelFloor * row_scale);
}

double QpEngine::working_face_measure(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                      const Vec &Aix, const Vec &ai_row_norm1, const Vec &lambda_i,
                                      const Vec &ae_row_norm1, const Vec &lambda_e,
                                      const QpOptions &opts) const {
    const double xmag = x.lpNorm<Eigen::Infinity>();
    double worst = 0.0;
    if (qp.me() > 0) {
        const Vec resid = qp.Ae * x - qp.be;
        for (Index j = 0; j < qp.me(); ++j) {
            const double row_scale = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
            worst =
                std::max(worst, std::abs(resid(j)) / face_row_target(row_scale, lambda_e(j), opts));
        }
    }
    for (const Index j : ws.active_ineq()) {
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        worst = std::max(worst, std::abs(Aix(j) - qp.bi(j)) /
                                    face_row_target(row_scale, lambda_i(j), opts));
    }
    return worst;
}

// Only x is refined: the multipliers stay the incumbent's by design, so the
// per-row target is built from the same lambda the classifier prices with and
// the target IS the classifier's own threshold. See the declaration.
bool QpEngine::refine_face_for_verdict(const QpProblem &qp, const WorkingSet &ws, Vec &x,
                                       const Vec &Aix, const Vec &ai_row_norm1, const Vec &lambda_i,
                                       const Vec &ae_row_norm1, const Vec &lambda_e,
                                       BorderState &border, QpCounters &counters,
                                       const QpOptions &opts) const {
    if (!border.schur.has_value() || border.schur->needs_refactorization()) {
        return false; // nothing trustworthy to re-form -- see the declaration
    }
    const Index n = qp.n();
    // THE PROXIMAL CENTRE at the verdict site is the point the walk is standing
    // on RIGHT NOW (the terminal branch has already snapped `x` onto the EQP
    // answer), because what this refinement is asked for is the nearest point of
    // the same face that closes its rows -- and because the declaration's rule is
    // that this site re-forms THE SAME system the pass's own solve posed, which is
    // a system centred at the iterate.
    const BorderedSystem sys =
        build_bordered_system(qp, border.k0, border.k0_rows, border.ledger, ws, x, opts);

    // solve_bordered_eqp's own scatter, and where the residue enters: the
    // exact bound is written over a pin the solve satisfies only to
    // O(dual_mu*|y|) -- see bordered_eqp.h's note on pinned variables.
    const auto point_of = [&](const Vec &sol) {
        Vec xc = sol.head(n);
        for (Index i = 0; i < n; ++i) {
            const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
            if (st != BoundState::kFree) {
                xc(i) = pinned_value(qp, st, i);
            }
        }
        return xc;
    };
    const auto measure_of = [&](const Vec &xc) {
        const Vec Ai_xc = qp.mi() > 0 ? Vec(qp.Ai * xc) : Vec(Aix);
        return working_face_measure(qp, ws, xc, Ai_xc, ai_row_norm1, lambda_i, ae_row_norm1,
                                    lambda_e, opts);
    };

    Vec best;
    Vec best_x;
    double best_measure = 0.0;
    Index steps = 0;
    try {
        // Replay the incumbent: schur.solve plus the loop solve_bordered_eqp
        // itself ran, so this starts exactly where the walk is and every step
        // below is one the shipped stopping rule declined to take.
        best = border.schur->solve(sys.rhs);
        best = refine_bordered_solve_iterated(border.k0.K, n, sys.border_v, sys.border_d,
                                              *border.schur, sys.rhs, best, opts);
        best_x = point_of(best);
        best_measure = measure_of(best_x);
        for (Index step = 0; step < detail::kMaxVerdictRefineSteps && best_measure > 1.0; ++step) {
            const Vec r =
                bordered_residual(border.k0.K, n, sys.border_v, sys.border_d, sys.rhs, best, opts);
            const Vec candidate = best + border.schur->solve(r);
            Vec candidate_x = point_of(candidate);
            const double candidate_measure = measure_of(candidate_x);
            if (!(candidate_measure < best_measure)) {
                break; // stagnated, diverging, or NaN -- keep the better point
            }
            best = candidate;
            best_x = std::move(candidate_x);
            best_measure = candidate_measure;
            ++steps;
        }
    } catch (const std::runtime_error &) {
        // A singular Schur complement is a degradation this branch answers by
        // declining to refine; anything else is a genuine backend fault the
        // engine must not hide -- border_solve_or_fall_back's discriminator.
        if (!border.schur->needs_refactorization()) {
            throw;
        }
        return false;
    }
    if (steps == 0) {
        return false; // the face was already inside its target, or would not move
    }
    // CLOSED, OR NOTHING: a refinement that ran out of budget with the face
    // still open has shown the face is not closable, which is evidence FOR the
    // classifier -- see refine_face_for_verdict's declaration for the cost.
    if (!(best_measure <= 1.0)) {
        return false;
    }
    const double before =
        working_face_measure(qp, ws, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1, lambda_e, opts);
    if (!(best_measure < before)) {
        return false; // strict decrease against the WALK's own point, or nothing
    }
    x = std::move(best_x);
    counters.verdict_refine_steps += steps;
    return true;
}

bool QpEngine::refine_eliminated_face_for_verdict(const QpProblem &qp, const WorkingSet &ws, Vec &x,
                                                  const Vec &Aix, const Vec &ai_row_norm1,
                                                  const Vec &lambda_i, const Vec &ae_row_norm1,
                                                  const Vec &lambda_e, const detail::KktFactor &kkt,
                                                  const EliminatedFace &face, QpCounters &counters,
                                                  const QpOptions &opts) const {
    const Index n = qp.n();
    const Index me = qp.me();
    const std::vector<Index> &aw = ws.active_ineq();
    const Index n_working = static_cast<Index>(aw.size());
    if (!face.factorized) {
        // Nothing this iteration solved through `kkt`: only the empty-system
        // short-circuit reaches here, a bordered candidate being the border
        // twin's by the caller's provenance test. See the declaration.
        return false;
    }
    if (ws.num_free() == 0 && me == 0 && aw.empty()) {
        return false; // the empty system eliminated_candidate short-circuits
    }

    // THE SYSTEM solve_eqp SOLVES, re-derived for the CURRENT working set. The rhs below is
    // eqp_solve.h's, and the two must stay in step: a divergence would refine a system the walk
    // never solved, which the strict-decrease rule would then reject rather than adopt.
    const KktAssembly assembly = assemble_kkt(qp, ws, opts);
    const Index n_free = static_cast<Index>(assembly.free_of_full.size());
    const Index dim = n_free + me + n_working;

    Vec x_fixed = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        const BoundState st = ws.bound_state()[static_cast<std::size_t>(i)];
        if (st != BoundState::kFree) {
            x_fixed(i) = pinned_value(qp, st, i);
        }
    }
    // THE SAME rhs solve_eqp SOLVED, proximal term included -- this site's own
    // declaration says the two must stay in step, and the centre is the iterate
    // the walk stands on (the border twin's note above states why).
    Vec rhs(dim);
    for (Index k = 0; k < n_free; ++k) {
        const Index i = assembly.free_of_full[static_cast<std::size_t>(k)];
        rhs(k) = -qp.g(i) - assembly.rhs_shift(k) + opts.primal_delta * x(i);
    }
    for (Index r = 0; r < me; ++r) {
        rhs(n_free + r) = qp.be(r) - assembly.rhs_shift(n_free + r);
    }
    for (Index k = 0; k < n_working; ++k) {
        rhs(n_free + me + k) =
            qp.bi(aw[static_cast<std::size_t>(k)]) - assembly.rhs_shift(n_free + me + k);
    }

    // The DUAL regularization only: the primal delta is the proximal term of the
    // model on this path too (eqp_solve.h's Iterative refinement section).
    Vec reg = Vec::Zero(dim);
    reg.segment(n_free, me + n_working).setConstant(-opts.dual_mu);
    const auto residual_of = [&](const Vec &yy) {
        const Vec Kyy = assembly.K.template selfadjointView<Eigen::Upper>() * yy;
        return Vec((rhs - Kyy) + reg.cwiseProduct(yy));
    };
    const auto point_of = [&](const Vec &yy) {
        Vec xc = x_fixed;
        for (Index k = 0; k < n_free; ++k) {
            xc(assembly.free_of_full[static_cast<std::size_t>(k)]) = yy(k);
        }
        return xc;
    };
    const auto measure_of = [&](const Vec &xc) {
        const Vec Ai_xc = qp.mi() > 0 ? Vec(qp.Ai * xc) : Vec(Aix);
        return working_face_measure(qp, ws, xc, Ai_xc, ai_row_norm1, lambda_i, ae_row_norm1,
                                    lambda_e, opts);
    };

    // THE REUSE GUARD: the key carries the working set AND the factor's own
    // identity, so a moved working set and a re-factorized `kkt` both miss.
    // See EliminatedFace and the declaration.
    detail::KktFactor fresh(threads_);
    const bool reuse = face.holds(ws, kkt);
    const detail::KktFactor &fac = reuse ? kkt : fresh;
    if (!reuse) {
        // rebuild_k0's accounting rule: the decision is taken BEFORE the
        // factorize and handed to it, so one factorization costs one pattern
        // hash. `fresh` has never been analyzed, so the analysis is needed.
        const detail::AnalysisDecision analysis = detail::analysis_decision(fresh, assembly.K);
        if (analysis.needed) {
            ++counters.symbolic_analyses;
        }
        // Charged for the ATTEMPT, eliminated_candidate's convention.
        ++counters.factorizations;
        detail::factorize_checked(fresh, assembly.K, analysis);
    }
    // NOTHING IS SWALLOWED ON EITHER BRANCH: no backend this engine runs on
    // reports a singular K as a status, so a throw from here is a genuine
    // backend fault the engine must not hide. See the declaration.
    //
    // Replay the CURRENT face's own incumbent -- solve_eqp's solve plus its one
    // mandatory step -- so every step below is one the walk declined.
    Vec best = detail::solve_vec(fac, rhs);
    best = best + detail::solve_vec(fac, residual_of(best));
    Vec best_x = point_of(best);
    double best_measure = measure_of(best_x);
    Index steps = 0;
    for (Index step = 0; step < detail::kMaxVerdictRefineSteps && best_measure > 1.0; ++step) {
        const Vec candidate = best + detail::solve_vec(fac, residual_of(best));
        Vec candidate_x = point_of(candidate);
        const double candidate_measure = measure_of(candidate_x);
        if (!(candidate_measure < best_measure)) {
            break; // stagnated, diverging, or NaN -- keep the better point
        }
        best = candidate;
        best_x = std::move(candidate_x);
        best_measure = candidate_measure;
        ++steps;
    }
    if (steps == 0 || !(best_measure <= 1.0)) {
        return false; // CLOSED, OR NOTHING -- the border twin's rule, verbatim
    }
    const double before =
        working_face_measure(qp, ws, x, Aix, ai_row_norm1, lambda_i, ae_row_norm1, lambda_e, opts);
    if (!(best_measure < before)) {
        return false; // strict decrease against the WALK's own point, or nothing
    }
    x = std::move(best_x);
    counters.verdict_refine_steps += steps;
    return true;
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

EqpResult QpEngine::eqp_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                  detail::KktFactor &kkt, EliminatedFace &face, BorderState &border,
                                  QpCounters &counters, detail::InertiaVerdict &verdict,
                                  const QpOptions &opts) const {
    // Invalidated HERE, so only the path that actually factorizes `kkt` this
    // iteration re-arms it: the verdict site refuses to reuse a factorization
    // no call of this iteration produced.
    face.factorized = false;
    if (opts_.ws_algebra == WorkingSetLinearAlgebra::kSchurBorder) {
        return border_candidate(qp, ws, x_k, kkt, face, border, counters, verdict, opts);
    }
    return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
}

EqpResult QpEngine::eliminated_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                         detail::KktFactor &kkt, EliminatedFace &face,
                                         QpCounters &counters, detail::InertiaVerdict &verdict,
                                         const QpOptions &opts) const {
    const bool empty_system = ws.num_free() == 0 && qp.me() == 0 && ws.active_ineq().empty();
    if (empty_system) {
        // Nothing was factorized, and a 0x0 system trivially has the
        // expected (0, 0, 0) inertia: every variable is pinned, so there
        // is no free direction for negative curvature to live in.
        verdict = detail::InertiaVerdict::kOk;
#ifdef HVEN_TESTING
        // The seam's record of WHICH system this pass's direction came out of.
        // A 0x0 one has no backend evidence at all, which is why the state
        // stays ABSENT rather than being filled with an observed (0, 0, 0).
        detail::testing::qp_walk_note_eqp([](detail::testing::QpWalkEqpEvidence &s) {
            s.path = detail::testing::QpWalkEqpPath::kEmptySystem;
            s.expected_pos = 0;
            s.expected_neg = 0;
            s.refine_steps = 0;
        });
#endif
        Vec xf(qp.n());
        for (Index i = 0; i < qp.n(); ++i) {
            xf(i) = (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kAtUpper)
                        ? qp.upper(i)
                        : qp.lower(i);
        }
        return EqpResult{std::move(xf), Vec::Zero(0), Vec::Zero(0)};
    }
    ++counters.factorizations;
    EqpResult res = solve_eqp(qp, ws, x_k, kkt, opts);
    // Armed only now: solve_eqp has returned, so `kkt` really does hold this
    // face's factorization and the verdict site may solve through it.
    face.capture(ws, kkt);
    // EXTRA steps only, i.e. identically 0 (solver_counters.h): the eliminated path
    // takes its one mandatory step and has no iterated loop.
    counters.eqp_refine_steps += res.refine_steps;
    // THE EQP-SITE RESIDUAL CHECK's cost and verdict (E4-4). A refusal here is
    // terminal for the pass -- the eliminated path IS the retreat the border path
    // falls back to, so there is nothing further to fall back to -- and the walk
    // reads it off the returned result (section 5).
    counters.eqp_refine_iters += res.residual_refine_iters;
    counters.eqp_polish_steps += res.polish_steps;
    if (res.residual_refused) {
        ++counters.eqp_residual_refusals;
    }
    // solve_eqp leaves `kkt` holding the factorization of the
    // bound-ELIMINATED K, whose expected inertia is (n_free, me +
    // n_working, 0) -- the reduced form of the gate in section 4b.
#ifdef HVEN_TESTING
    // SINGLE READ (fix round 2). `inertia()` is not universally a cheap reread
    // of cached session fields: on MKL it is, but Accelerate's accessor makes a
    // genuine, independently fallible `SparseGetInertia` call. A record-only
    // seam may not add one, and evidence read a second time is not provably the
    // evidence that decided the verdict. So the walk's own read happens ONCE,
    // into a local, and the verdict and the trace consume that same object.
    // The two statements below are the production statement above the `#else`,
    // split only by which expression supplies the evidence; nothing is
    // reordered, hoisted out of, or re-evaluated on the production path, which
    // is the `#else` arm verbatim.
    const hven::linear::InertiaEvidence eliminated_inertia = kkt.factor.inertia();
    verdict = detail::inertia_verdict(eliminated_inertia, ws.num_free(),
                                      qp.me() + static_cast<Index>(ws.active_ineq().size()));
    // The seam's record of the ELIMINATED path's own verdict evidence: the
    // backend's triple for the bound-eliminated K, and the expectation the gate
    // compared it against. The SAME object the line above compared -- not a
    // second query, and nothing recomputed or factorized.
    {
        const Index expected_neg_e = qp.me() + static_cast<Index>(ws.active_ineq().size());
        detail::testing::qp_walk_note_inertia(eliminated_inertia, ws.num_free(), expected_neg_e);
        const Index steps = res.refine_steps;
        detail::testing::qp_walk_note_eqp([steps](detail::testing::QpWalkEqpEvidence &s) {
            s.path = detail::testing::QpWalkEqpPath::kEliminated;
            s.refine_steps = steps;
        });
    }
#else
    verdict = detail::inertia_verdict(kkt.factor.inertia(), ws.num_free(),
                                      qp.me() + static_cast<Index>(ws.active_ineq().size()));
#endif
    return res;
}

EqpResult QpEngine::border_candidate(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                     detail::KktFactor &kkt, EliminatedFace &face,
                                     BorderState &border, QpCounters &counters,
                                     detail::InertiaVerdict &verdict, const QpOptions &opts) const {
#ifdef HVEN_TESTING
    // The seam's record that BORDER MODE was entered at all: an eliminated
    // path recorded together with this flag is border mode FALLING BACK, which
    // is a different fact from the engine being configured for elimination.
    {
        const bool latched_now = border.latched;
        detail::testing::qp_walk_note_eqp([latched_now](detail::testing::QpWalkEqpEvidence &s) {
            s.border_attempted = true;
            s.latched = latched_now;
        });
    }
#endif
    // LATCHED: bordering has been abandoned for this working-set shape
    // (see border_solve_or_fall_back). Do not sync -- the whole point is
    // that every border added here would be discarded unused, and adding
    // them anyway is what drove dim() past schur_cap and made the wasted
    // work the dominant cost.
    if (border.latched) {
        if (latch_still_holds(ws)) {
            return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
        }
        // The pin count fell back to schur_cap or below, so bordering is
        // possible again. The stale stack cannot be incrementally repaired
        // (it was never updated, and the working ROWS may have moved
        // arbitrarily far during the latch -- see latch_still_holds), so
        // resume with a clean K0 built from the CURRENT working set.
        border.latched = false;
        rebuild_k0(qp, ws, border, counters, opts);
        sync_borders(qp, ws, border, counters, opts);
        return border_solve_or_fall_back(qp, ws, x_k, kkt, face, border, counters, verdict,
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
    return border_solve_or_fall_back(qp, ws, x_k, kkt, face, border, counters, verdict, rebuilt,
                                     opts);
}

EqpResult QpEngine::border_solve_or_fall_back(const QpProblem &qp, const WorkingSet &ws,
                                              const Vec &x_k, detail::KktFactor &kkt,
                                              EliminatedFace &face, BorderState &border,
                                              QpCounters &counters, detail::InertiaVerdict &verdict,
                                              bool rebuilt, const QpOptions &opts) const {
#ifdef HVEN_TESTING
    // The seam's record of the border stack as this call FOUND it: "marked for
    // rebuild" is the fact the review asked for, and it is only observable
    // here, before the branch below acts on it.
    {
        const bool marked = border.schur->needs_refactorization();
        detail::testing::qp_walk_note_eqp(
            [marked](detail::testing::QpWalkEqpEvidence &s) { s.needs_refactorization = marked; });
    }
#endif
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
            return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
        }
        // A rebuild that has already been spent this iteration, or one
        // that did not clear the flag, also falls back -- but only latches
        // if the state it left behind is the pins-only dead end.
        if (rebuilt) {
            return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
        }
        rebuild_k0(qp, ws, border, counters, opts);
        sync_borders(qp, ws, border, counters, opts);
        if (border.schur->needs_refactorization()) {
            border.latched = rebuild_would_be_noop(ws, border);
            return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
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
                                           border.ledger, ws, x_k, opts);
        // TOTAL steps, mandatory first one included (solver_counters.h).
        counters.border_refine_steps += res.refine_steps;
        counters.eqp_refine_iters += res.residual_refine_iters;
        counters.eqp_polish_steps += res.polish_steps;
        if (res.residual_refused) {
            // THE EQP-SITE RESIDUAL CHECK REFUSED the bordered answer (E4-4): it
            // does not solve the system the Schur stack was handed. That is the
            // same class of failure -- a factor whose answer cannot be trusted --
            // this path already answers by degrading to the bound-eliminated form,
            // so it takes THAT retreat rather than inventing one, and the
            // eliminated twin gets its own residual check on its own factor.
            ++counters.eqp_residual_refusals;
            return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
        }
#ifdef HVEN_TESTING
        // The seam's record that the direction THIS pass used came through the
        // BORDER/SCHUR path, with the live stack's dimension and this call's
        // own refinement count. Placed after the solve so a throw cannot leave
        // a path recorded for a solve that never returned.
        {
            const Index dim_now = border.schur->dim();
            const Index steps = res.refine_steps;
            detail::testing::qp_walk_note_eqp(
                [dim_now, steps](detail::testing::QpWalkEqpEvidence &s) {
                    s.path = detail::testing::QpWalkEqpPath::kBorderSchur;
                    s.border_dim = dim_now;
                    s.refine_steps = steps;
                });
        }
#endif
        return res;
    } catch (const std::runtime_error &) {
        if (!border.schur->needs_refactorization()) {
            throw;
        }
        return eliminated_candidate(qp, ws, x_k, kkt, face, counters, verdict, opts);
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
#ifdef HVEN_TESTING
    // SINGLE READ (fix round 2), for the reason given at the eliminated path's
    // own verdict site: one `inertia()` call, one object, both consumers. The
    // seam's record of the BORDER path's verdict evidence is therefore K0's own
    // backend triple AS COMPARED, with the expectation C's Haynsworth
    // contribution has already been backed out of. The `#else` arm is the
    // production statement, unchanged.
    const hven::linear::InertiaEvidence k0_inertia = border.kkt.factor.inertia();
    detail::testing::qp_walk_note_inertia(k0_inertia, expected_pos - extra_pos,
                                          expected_neg - extra_neg);
    return detail::inertia_verdict(k0_inertia, expected_pos - extra_pos, expected_neg - extra_neg);
#else
    return detail::inertia_verdict(border.kkt.factor.inertia(), expected_pos - extra_pos,
                                   expected_neg - extra_neg);
#endif
}

detail::InertiaVerdict QpEngine::probe_inertia(const QpProblem &qp, const WorkingSet &ws,
                                               const Vec &x_k, detail::KktFactor &kkt,
                                               EliminatedFace &face, BorderState &border,
                                               QpCounters &counters, const QpOptions &opts) const {
    detail::InertiaVerdict verdict = detail::InertiaVerdict::kOk;
    // THE LOOP'S OWN KEY, not a throwaway: this call re-factorizes `kkt` for a
    // HYPOTHETICAL working set, so the key must follow it there -- disarmed on
    // the border path, re-armed on the PROBED set otherwise. See EliminatedFace.
    (void)eqp_candidate(qp, ws, x_k, kkt, face, border, counters, verdict, opts);
    return verdict;
}

bool QpEngine::probe_zero_multiplier_drops(const QpProblem &qp, const Vec &x_k, const Vec &shift,
                                           const Vec &lambda_i, const Vec &z, WorkingSet &ws,
                                           detail::KktFactor &kkt, EliminatedFace &face,
                                           BorderState &border, QpCounters &counters,
                                           ProbeState &probe, DropRecord &dropped,
                                           const QpOptions &opts) const {
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

        const detail::InertiaVerdict verdict =
            probe_inertia(qp, ws, x_k, kkt, face, border, counters, opts);
        if (verdict == detail::InertiaVerdict::kWrong) {
#ifdef HVEN_TESTING
            // The seam's per-minor record of the OTHER drop route -- the one
            // QpCounters::ws_drops merges with the Dantzig rule's.
            detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
                r.drop_route = detail::testing::QpWalkDropRoute::kZeroProbe;
                r.block = c.is_ineq ? detail::testing::QpWalkBlock::kIneq
                                    : detail::testing::QpWalkBlock::kBound;
                r.block_index = c.idx;
                if (!c.is_ineq) {
                    r.block_side = held;
                    r.bound_gap = qp.upper(c.idx) - qp.lower(c.idx);
                }
                r.decision_multiplier = c.is_ineq ? std::abs(lambda_i(c.idx)) : std::abs(z(c.idx));
                r.decision_tol = opts.opt_tol;
                r.drop_candidates = static_cast<Index>(cands.size());
            });
#endif
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
#ifdef HVEN_TESTING
        // ARM B1 AT THE ZERO-PROBE'S RESTORATION (amendment A3, site 5 of
        // six) -- AND THIS ONE IS CHECK-ONLY, DELIBERATELY.
        //
        // The statement below puts BACK a member the probe removed for a
        // trial. Refusing it would not change an ADD RULE; it would change the
        // probe's contract, and the walk would silently lose a constraint it
        // never decided to drop. So the predicate is asked, its verdict is
        // COUNTED, and the restoration proceeds either way. What stands behind
        // this site is amendment A2's rank check at the next predicate call:
        // if the restoration really did leave the set dependent, the next add
        // site reads it and A9 disarms the solve.
        //
        // It is also declared UNCOVERED-BY-FIXTURE in `qp_degeneracy_arm.h`:
        // reaching it deliberately needs a probe that removes a member and
        // then rejects the reduced set, which a constructed QP does not
        // produce reliably.
        if (detail::testing::QpDegeneracyArmState::independent_add_on()) {
            const detail::testing::QpB1Admission adm = detail::testing::admit_if_independent(
                qp, ws, detail::testing::b1_site_rank(), c.is_ineq, c.idx,
                detail::testing::QpB1Site::kProbeRestore);
            if (!adm.admit && adm.rank_ok) {
                ++detail::testing::QpDegeneracyArmState::indep_probe_restore_dependent;
            }
        }
#endif
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
#ifdef HVEN_TESTING
    // ARM B1 AT THE REPAIR PINS (amendment A3, site 6 of six).
    //
    // `repair_temporary_vertex` pins variables until the inertia gate is
    // satisfied, and a pin is a unit row joining the working set with no
    // independence question asked. Under the arm a pin whose unit row is
    // dependent on the set is REFUSED -- and the refusal needs no new path,
    // because `false` here already means "nothing to pin against" and the
    // caller's own loop moves on to the next variable in its order.
    //
    // Declared UNCOVERED-BY-FIXTURE in `qp_degeneracy_arm.h`: the repair fires
    // on a trustworthy WRONG inertia verdict at iteration 0, which a
    // constructed QP does not produce on demand.
    if (detail::testing::QpDegeneracyArmState::independent_add_on()) {
        const detail::testing::QpB1Admission adm = detail::testing::admit_if_independent(
            qp, ws, detail::testing::b1_site_rank(), /*is_ineq=*/false, i,
            detail::testing::QpB1Site::kRepairPin);
        if (!adm.admit && adm.rank_ok) {
            ++detail::testing::QpDegeneracyArmState::indep_repair_pin_refusals;
            return false;
        }
    }
#endif
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

bool QpEngine::reread_wrong_inertia(const QpProblem &qp, const WorkingSet &ws, const Vec &x_k,
                                    QpCounters &counters, const QpOptions &opts,
                                    detail::InertiaVerdict &verdict) const {
    if (!opts.independent_working_set || verdict != detail::InertiaVerdict::kWrong) {
        return false;
    }
    ++counters.dep_inertia_rereads;
    const auto read = [&](double mu) {
        detail::KktFactor local(threads_);
        EliminatedFace local_face;
        QpOptions o = opts;
        o.dual_mu = mu;
        detail::InertiaVerdict v = detail::InertiaVerdict::kWrong;
        try {
            (void)eliminated_candidate(qp, ws, x_k, local, local_face, counters, v, o);
        } catch (const std::runtime_error &) {
            v = detail::InertiaVerdict::kWrong; // no read: the kWrong stands
        }
        return v;
    };
    detail::InertiaVerdict reread = read(0.0);
    // R-C' (settler S26): a kSuspect mu-free read says nothing about the sign
    // of the curvature -- a perturbed zero pivot at mu = 0 reads kSuspect on a
    // genuinely indefinite face too -- so it replaces the kWrong only when the
    // eliminated factor at the pass's own mu reads kOk (the border path's K0
    // verdict was the untrustworthy one).
    if (reread == detail::InertiaVerdict::kSuspect &&
        read(opts.dual_mu) != detail::InertiaVerdict::kOk) {
        reread = detail::InertiaVerdict::kWrong;
    }
    if (reread == detail::InertiaVerdict::kWrong) {
        return false;
    }
    ++counters.dep_inertia_reread_flips;
    verdict = reread;
    return true;
}

bool QpEngine::mu_footprint_open(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                 const Vec &ai_row_norm1, const Vec &lambda_i,
                                 const Vec &ae_row_norm1, const Vec &lambda_e,
                                 const QpOptions &opts) const {
    const double xmag = x.lpNorm<Eigen::Infinity>();
    for (Index j = 0; j < qp.me(); ++j) {
        const double row_scale = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
        if (opts.dual_mu * std::abs(lambda_e(j)) > opts.feas_tol * row_scale) {
            return true;
        }
    }
    for (const Index j : ws.active_ineq()) {
        const double row_scale = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
        if (opts.dual_mu * std::abs(lambda_i(j)) > opts.feas_tol * row_scale) {
            return true;
        }
    }
    return false;
}

bool QpEngine::refine_terminal_mu_free(const QpProblem &qp, const WorkingSet &ws, Vec &x,
                                       Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                       const QpOptions &opts) const {
    ++counters.terminal_mu_refines;
    const Index n = qp.n();
    const Index me = qp.me();
    QpOptions o = opts;
    o.dual_mu = 0.0;

    // The mu-free EQP of the CURRENT face: one factorization (charged by
    // eliminated_candidate) plus solve_eqp's mandatory unregularized step.
    detail::KktFactor local(threads_);
    EliminatedFace local_face;
    detail::InertiaVerdict v = detail::InertiaVerdict::kWrong;
    EqpResult r;
    try {
        r = eliminated_candidate(qp, ws, x, local, local_face, counters, v, o);
    } catch (const std::runtime_error &) {
        return false; // no mu-free solve: the existing classification stands
    }

    // Up to kTerminalMuRefineSteps further steps against the residual of the
    // UNREGULARIZED system (primal_delta removed too), each kept only while
    // that residual strictly falls. THE SHARED PRIMITIVE (M7 W2 E4 fix1):
    // eqp_solve.h's polish_eqp_unregularized, which this block was factored
    // into when the terminal polish needed the identical loop -- `o` already
    // carries dual_mu = 0, so what it backs out here is the primal delta, as
    // before, on the same factor and with the same strict-decrease safeguard.
    if (local_face.factorized) {
        (void)polish_eqp_unregularized(qp, ws, local, o, detail::kTerminalMuRefineSteps, r);
    }

    // ADOPTION: every row and every bound of `qp` within feas_tol * row_scale,
    // and nothing non-finite.
    if (!r.x.allFinite() || !r.lambda_e.allFinite() || !r.lambda_w.allFinite()) {
        return false;
    }
    const double xmag = r.x.lpNorm<Eigen::Infinity>();
    if (me > 0) {
        const Vec resid = qp.Ae * r.x - qp.be;
        for (Index j = 0; j < me; ++j) {
            const double row_scale =
                std::max({1.0, std::abs(qp.be(j)), qp.Ae.row(j).cwiseAbs().sum() * xmag});
            if (std::abs(resid(j)) > opts.feas_tol * row_scale) {
                return false;
            }
        }
    }
    if (qp.mi() > 0) {
        const Vec resid = qp.Ai * r.x - qp.bi;
        for (Index j = 0; j < qp.mi(); ++j) {
            const double row_scale =
                std::max({1.0, std::abs(qp.bi(j)), qp.Ai.row(j).cwiseAbs().sum() * xmag});
            if (resid(j) > opts.feas_tol * row_scale) {
                return false;
            }
        }
    }
    for (Index i = 0; i < n; ++i) {
        const double below = qp.lower(i) - r.x(i);
        const double above = r.x(i) - qp.upper(i);
        const double bound = below >= above ? qp.lower(i) : qp.upper(i);
        const double row_scale = std::max({1.0, std::abs(bound), xmag});
        if (std::max(below, above) > opts.feas_tol * row_scale) {
            return false;
        }
    }
    x = r.x;
    price(qp, ws, r, lambda_e, lambda_i, z);
    ++counters.terminal_mu_refine_rescues;
    return true;
}

bool QpEngine::adopt_polished_candidate(const QpProblem &qp, const WorkingSet &ws,
                                        const EqpResult &cand, const detail::TerminalKkt &before,
                                        const QpOptions &opts, Vec &x, Vec &lambda_e, Vec &lambda_i,
                                        Vec &z) const {
    if (cand.x.size() != qp.n() || !cand.x.allFinite() || !cand.lambda_e.allFinite() ||
        !cand.lambda_w.allFinite()) {
        return false;
    }
    Vec cand_le = Vec::Zero(qp.me());
    Vec cand_li = Vec::Zero(qp.mi());
    Vec cand_z = Vec::Zero(qp.n());
    price(qp, ws, cand, cand_le, cand_li, cand_z);
    const detail::TerminalKkt after =
        detail::terminal_kkt(qp, ws, cand.x, cand_le, cand_li, cand_z);

    // ACCEPTANCE, written as a chain of !(good) tests so a NaN anywhere rejects.
    // FEASIBILITY IS KEPT: no row and no bound outside feas_tol that the incumbent
    // met, and nothing worse than the incumbent either way. Then no residual may
    // worsen and at least one must strictly improve -- so a candidate that is
    // merely different is refused, and `terminal_polish_rejected` counts a cost
    // and never a risk.
    //
    // THE SLACK ON BOTH HALVES (fix round 2): kTerminalPolishWorseFactor, whose own
    // note carries the measurement. "No worse" means no worse by more than that
    // factor and "strictly better" means better BY that factor, so the decision is
    // a margin on each side instead of a tie-break that one ulp of one residual can
    // settle differently in two builds of the same expression.
    //
    // THE SLACK IS ON THE DUAL RESIDUALS ONLY, AND THE ROWS STAY EXACT. MEASURED:
    // with the slack on the row half as well, a candidate may be up to 4x LESS
    // feasible than the incumbent, and on an elastic relaxation whose incumbent
    // rows sit at 1e-3 that is enough to make the exported point read a structural
    // violation -- the T7 feasible census went to 16 open relaxations certified
    // infeasible against its own claim of 0. Feasibility is the one thing the
    // polish may never trade, so `after.rows` is compared exactly on both halves.
    const double slack = detail::kTerminalPolishWorseFactor;
    const bool feasible = after.rows <= std::max(opts.feas_tol, before.rows);
    const bool no_worse = after.stationarity <= slack * before.stationarity &&
                          after.rows <= before.rows &&
                          after.complementarity <= slack * before.complementarity;
    const bool strictly_better = slack * after.stationarity < before.stationarity ||
                                 after.rows < before.rows ||
                                 slack * after.complementarity < before.complementarity;
    if (!after.all_finite() || !feasible || !no_worse || !strictly_better) {
        return false;
    }
    x = cand.x;
    lambda_e = std::move(cand_le);
    lambda_i = std::move(cand_li);
    z = std::move(cand_z);
    return true;
}

bool QpEngine::polish_footprint_open(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                     const Vec &lambda_e, const Vec &lambda_i, const Vec &z,
                                     const QpOptions &opts) const {
    // TWO DISJUNCTS, both on quantities detail::terminal_kkt reports at THIS point,
    // both written as !(v <= t) so a NaN is "open" -- the acceptance test refuses a
    // non-finite candidate anyway, so the cost of being wrong here is one
    // factorization and never an answer.
    const detail::TerminalKkt now = detail::terminal_kkt(qp, ws, x, lambda_e, lambda_i, z);
    // (a) THE COMPLEMENTARITY PRODUCT, which is what a consumer reads; the
    // threshold's own note calibrates it.
    if (!(now.complementarity <= detail::kTerminalPolishComplFactor * opts.feas_tol)) {
        return true;
    }
    // (b) THE STATIONARITY RESIDUAL, relative to its own scale. The KKT-point test
    // (section 5) admits a point at opt_tol * scale and stops there, by design;
    // where the walk it replaced kept stepping until the answer stopped moving it
    // reached several decades further. That difference is real and it is visible at
    // the driver's own KKT columns, so a point with decades left to gain gets the
    // polish -- which reaches them in ONE solve instead of the extra passes the old
    // walk spent.
    return !(now.stationarity <= detail::kTerminalPolishStatFactor * opts.opt_tol);
}

bool QpEngine::polish_terminal_point(const QpProblem &qp, const WorkingSet &ws,
                                     const EqpResult &pass_answer, Vec &x, Vec &lambda_e,
                                     Vec &lambda_i, Vec &z, QpCounters &counters,
                                     const QpOptions &opts) const {
    const detail::TerminalKkt before = detail::terminal_kkt(qp, ws, x, lambda_e, lambda_i, z);

    // STAGE 1, FREE: the pass's OWN EQP answer, which solve_eqp has already taken
    // the regularization back out of (eqp_solve.h's UNREGULARIZED POLISH). On a
    // face with curvature along its null space that answer IS the face's exact KKT
    // point, and the terminal iterate is not: on the stationary-on-working-set
    // disjunct of the KKT-point test nothing moved, deliberately, so the point the
    // walk stands on still carries whatever the previous step left. Adopting the
    // answer here costs NO factorization and no solve -- and it is admissible here
    // and not in the loop because the acceptance test below reads every row and
    // every bound of the problem, which is what the ratio test the walk skips
    // would have protected, and because nothing happens afterwards.
    if (adopt_polished_candidate(qp, ws, pass_answer, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_adopted;
        return true;
    }

    // STAGE 2, ONE FACTORIZATION: the face re-solved with the DUAL regularization
    // out of the SYSTEM rather than only out of the refinement target. What stage 1
    // cannot reach is a point whose working-row slack is dual_mu*|lambda| because
    // the system that produced it had -dual_mu on those rows -- the answer is then
    // the right answer to the wrong system, and no refinement of it against the
    // right one converges from a factor whose pivots came from the other. That
    // slack is what the NLP-level complementarity check squares (the B-1 term), so
    // it is worth a factorization at a point the walk has certified.
    ++counters.terminal_polish_solves;
    QpOptions o = opts;
    o.dual_mu = 0.0;

    // The mu-free EQP of the FINAL face, in a LOCAL factor and a LOCAL face:
    // the pass's own factor and border stack are untouched, and the answer is
    // the bound-ELIMINATED one on both algebras, so the polished point a border
    // solve reports and the one a refactorize solve reports come out of the same
    // system.
    //
    // ITS FACTORIZATION IS CHARGED TO `terminal_polish_solves` AND NOT TO
    // `factorizations`, which is the one place this function departs from
    // refine_terminal_mu_free's accounting, and the reason is what
    // `factorizations` MEANS to its consumers. It is the cost of the active-set
    // SEARCH: every budget is written against it, the hot-handle contract is
    // stated as `qp_factorizations == 0` on a reused K0 (WarmStart's own pins),
    // and the battery's cost tripwire reads it. This solve searches nothing and
    // reuses nothing -- it re-solves a face the walk has already finished with,
    // after the answer is fixed -- so charging it there would make a
    // zero-factorization hot reuse report one factorization and destroy the pin's
    // ability to see a LOST hot reuse at all. Nothing is hidden: the count is
    // `terminal_polish_solves`, one per solve entered, asserted and aggregated
    // into SqpCounters like every other counter, and a consumer that wants the
    // total forms `factorizations + terminal_polish_solves`. REGISTERED for the
    // settler as an accounting choice, with this as the argument.
    //
    // The EQP-level fields the local solve moves ARE folded back below: they
    // describe the solve's own arithmetic, which no other counter reports.
    detail::KktFactor local(threads_);
    EliminatedFace local_face;
    detail::InertiaVerdict v = detail::InertiaVerdict::kWrong;
    QpCounters local_counters;
    EqpResult r;
    try {
        r = eliminated_candidate(qp, ws, x, local, local_face, local_counters, v, o);
    } catch (const std::runtime_error &) {
        counters.eqp_refine_iters += local_counters.eqp_refine_iters;
        counters.eqp_residual_refusals += local_counters.eqp_residual_refusals;
        counters.eqp_polish_steps += local_counters.eqp_polish_steps;
        ++counters.terminal_polish_rejected;
        return false; // no mu-free solve: the walk's own point stands
    }
    counters.eqp_refine_iters += local_counters.eqp_refine_iters;
    counters.eqp_residual_refusals += local_counters.eqp_residual_refusals;
    counters.eqp_polish_steps += local_counters.eqp_polish_steps;
    // AND THE PRIMAL DELTA BACKED OUT OF THE ANSWER, which is consistent exactly
    // here: at a terminal point the step is going to zero, so the proximal
    // penalty's own term delta*(x - x_k) is going to zero with it and the face's
    // unregularized KKT system is the one the point should satisfy.
    if (local_face.factorized) {
        (void)polish_eqp_unregularized(qp, ws, local, o, detail::kTerminalPolishSteps, r);
    }
    if (r.x.size() != qp.n() || !r.x.allFinite() || !r.lambda_e.allFinite() ||
        !r.lambda_w.allFinite()) {
        ++counters.terminal_polish_rejected;
        return false;
    }

    if (!adopt_polished_candidate(qp, ws, r, before, opts, x, lambda_e, lambda_i, z)) {
        ++counters.terminal_polish_rejected;
        return false;
    }
    ++counters.terminal_polish_adopted;
    return true;
}

bool QpEngine::repair_temporary_vertex(const QpProblem &qp, WorkingSet &ws, Vec &x,
                                       detail::KktFactor &kkt, EliminatedFace &face,
                                       BorderState &border, QpCounters &counters,
                                       const QpOptions &opts) const {
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
        // A pin dependent on the set leaves null(A_W), and so the inertia,
        // unchanged: skip it. With no factor for `ws` the run re-checks.
        if (opts.independent_working_set) {
            // The integer screen first; the solve only on a screen-negative set.
            const bool screen = qp.me() + static_cast<Index>(ws.active_ineq().size()) +
                                    (qp.n() - ws.num_free()) + 1 >
                                qp.n();
            const detail::DepFactorView view = dep_factor_view(ws, kkt, face, border);
            if (screen && counters.mandatory_dependent_entries > 0) {
                ++counters.dep_screen_under_mandatory_dependence;
            }
            if (screen ||
                (view.usable() &&
                 test_candidate(qp, ws, view, /*is_ineq=*/false, i, counters, opts).dependent())) {
                ++counters.dep_pin_skips;
                continue;
            }
        }
        Vec grad = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
        if (!pin_at_best_bound(qp, ws, x, grad, i, hdiag(i))) {
            continue; // unbounded both ways: nothing to pin against
        }
        pinned.push_back(i);
        verdict = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
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
        if (probe_inertia(qp, ws, x, kkt, face, border, counters, opts) !=
            detail::InertiaVerdict::kOk) {
            ws.bound_state()[si] = held; // negative curvature is still there: keep the pin
        }
    }
    return true;
}

detail::DepFactorView QpEngine::dep_factor_view(const WorkingSet &ws, const detail::KktFactor &kkt,
                                                const EliminatedFace &face,
                                                const BorderState &border) {
    detail::DepFactorView v;
    if (face.holds(ws, kkt)) {
        v.eliminated = &kkt;
    } else if (border.schur.has_value() && !border.schur->needs_refactorization() &&
               border_holds(ws, border)) {
        v.k0 = &border.k0;
        v.k0_rows = &border.k0_rows;
        v.ledger = &border.ledger;
        v.schur = &*border.schur;
    }
    return v;
}

bool QpEngine::border_holds(const WorkingSet &ws, const BorderState &border) {
    const Index me = border.k0.K.rows() - ws.n() - static_cast<Index>(border.k0_rows.size());
    std::vector<std::uint8_t> row_deleted(border.k0_rows.size(), 0);
    std::vector<Index> rows;
    Index pins = 0;
    for (const BorderLedgerEntry &e : border.ledger) {
        switch (e.kind) {
        case BorderLedgerEntry::Kind::kVarPin:
            if (ws.bound_state()[static_cast<std::size_t>(e.target)] == BoundState::kFree) {
                return false;
            }
            ++pins;
            break;
        case BorderLedgerEntry::Kind::kRowDelete: {
            const Index p = e.target - me;
            if (p < 0 || p >= static_cast<Index>(row_deleted.size())) {
                return false;
            }
            row_deleted[static_cast<std::size_t>(p)] = 1;
            break;
        }
        case BorderLedgerEntry::Kind::kIneqRow:
            rows.push_back(e.target);
            break;
        }
    }
    if (pins != ws.n() - ws.num_free()) {
        return false;
    }
    for (std::size_t p = 0; p < border.k0_rows.size(); ++p) {
        if (row_deleted[p] == 0) {
            rows.push_back(border.k0_rows[p]);
        }
    }
    std::sort(rows.begin(), rows.end());
    return rows == ws.active_ineq();
}

detail::DepTest QpEngine::test_candidate(const QpProblem &qp, const WorkingSet &ws,
                                         const detail::DepFactorView &view, bool is_ineq, Index idx,
                                         QpCounters &counters, const QpOptions &opts) {
    Vec a = Vec::Zero(qp.n());
    if (is_ineq) {
        a = qp.Ai.row(idx).transpose();
    } else {
        a(idx) = 1.0; // the side does not change dependence
    }
    detail::DepTest t = detail::dependency_test(qp, ws, view, a, opts);
    ++counters.dep_tests;
    // R-A (E2 fix3): an exact structural dependency overrules a numerical
    // "independent" at every growth site (qp_engine.h, Degeneracy).
    if (t.verdict == detail::DepVerdict::kIndependent) {
        const detail::StructuralDependency sd = detail::structural_dependency(qp, ws, is_ineq, idx);
        if (sd.found) {
            const Index solves = t.solves;
            t = sd.test;
            t.solves = solves;
            ++counters.dep_structural_first;
        }
    }
    // R-E (E2 fix6): a CONVERGED "independent" in the grey band is re-read with
    // the refinement run to its end on the same factor; a certified rho' makes it
    // dependent with the re-read's coefficients (qp_engine.h, Degeneracy).
    if (t.verdict == detail::DepVerdict::kIndependent && !t.stall_admit &&
        t.rho < detail::kDepUndecidable) {
        ++counters.dep_grey_rereads;
        detail::DepTest r = detail::certificate_coefficients(qp, ws, view, a, opts);
        const Index solves = t.solves + r.solves;
        const double rho_floor =
            r.rho <= detail::kDepTol ? detail::dependency_rounding_floor(qp, ws, r, a) : 0.0;
        if (r.rho + rho_floor <= detail::kDepTol) {
            t = std::move(r);
            t.verdict = detail::DepVerdict::kDependent;
            t.stall_admit = false;
            ++counters.dep_grey_reread_dependent;
        }
        t.solves = solves;
    }
    // The screen alone decided, with the mandatory rows dependent: possibly a
    // false dependent (the safe direction), counted.
    if (t.screen && !(t.rho <= detail::kDepTol) && counters.mandatory_dependent_entries > 0) {
        ++counters.dep_screen_under_mandatory_dependence;
    }
    counters.dep_test_solves += t.solves;
    if (t.dependent()) {
        ++counters.dep_dependent;
    }
    if (t.verdict == detail::DepVerdict::kUndecidable) {
        ++counters.dep_undecidable;
    }
    if (t.stall_admit) {
        ++counters.dep_stall_admits;
    }
    return t;
}

bool QpEngine::remove_entry_dependency(WorkingSet &ws, const Vec &shift,
                                       const detail::EntryDetection &det, const ProbeState *seq,
                                       std::vector<std::uint8_t> &refused,
                                       detail::WsMember *removed) {
    using Kind = detail::WsMember::Kind;
    const auto mandatory = [&](const detail::WsMember &m) {
        return m.kind == Kind::kEq ||
               (m.kind == Kind::kPin &&
                ws.bound_state()[static_cast<std::size_t>(m.idx)] == BoundState::kFixed);
    };
    // Lower key goes first. Entry: 0 seed row, 1 seed pin, 2 shifted row.
    // Re-check: newest first, shifted rows last -- a shifted row is violated,
    // and refusing it would leave the walk no way to restore it (a false
    // kInfeasible on a pinned vertex).
    const auto key = [&](const detail::WsMember &m) -> Index {
        const auto si = static_cast<std::size_t>(m.idx);
        if (seq != nullptr) {
            const bool shifted = m.kind == Kind::kRow && shift(m.idx) > 0.0;
            constexpr Index kShiftedLast = Index{1} << 40;
            return (shifted ? kShiftedLast : 0) -
                   (m.kind == Kind::kPin ? seq->bound_seq[si] : seq->ineq_seq[si]);
        }
        if (m.kind == Kind::kPin) {
            return 1;
        }
        return shift(m.idx) > 0.0 ? 2 : 0;
    };
    const detail::WsMember *pick = nullptr;
    Index pick_key = 0;
    for (const detail::WsMember &m : det.support) {
        if (mandatory(m)) {
            continue;
        }
        const Index k = key(m);
        if (pick == nullptr || k < pick_key || (k == pick_key && m.idx > pick->idx)) {
            pick = &m;
            pick_key = k;
        }
    }
    if (pick == nullptr) {
        return false;
    }
    if (removed != nullptr) {
        *removed = *pick;
    }
    if (pick->kind == Kind::kRow) {
        ws.drop_ineq(pick->idx);
        if (!refused.empty() && shift(pick->idx) > 0.0) {
            refused[static_cast<std::size_t>(pick->idx)] = 1;
        }
    } else {
        ws.bound_state()[static_cast<std::size_t>(pick->idx)] = BoundState::kFree;
    }
    return true;
}

std::uint8_t QpEngine::pt_flags(const WalkSeen &seen, bool is_ineq, Index idx) {
    const std::vector<std::uint8_t> &v = is_ineq ? seen.pt_ineq : seen.pt_bound;
    return v.empty() ? std::uint8_t{0} : v[static_cast<std::size_t>(idx)];
}

void QpEngine::pt_mark(WalkSeen &seen, bool is_ineq, Index idx, std::uint8_t flag) {
    std::vector<std::uint8_t> &v = is_ineq ? seen.pt_ineq : seen.pt_bound;
    if (v.empty()) {
        return;
    }
    std::uint8_t &f = v[static_cast<std::size_t>(idx)];
    if (f == 0) {
        seen.pt_touched.push_back(is_ineq ? idx : -(idx + 1));
    }
    f = static_cast<std::uint8_t>(f | flag);
}

void QpEngine::pt_clear(WalkSeen &seen) {
    for (const Index t : seen.pt_touched) {
        if (t >= 0) {
            seen.pt_ineq[static_cast<std::size_t>(t)] = 0;
        } else {
            seen.pt_bound[static_cast<std::size_t>(-t - 1)] = 0;
        }
    }
    seen.pt_touched.clear();
    seen.pt_events = 0;
}

void QpEngine::pt_release(WalkSeen &seen, bool is_ineq, Index idx) {
    if (seen.pt_ineq.empty() && seen.pt_bound.empty()) {
        return;
    }
    pt_mark(seen, is_ineq, idx, kPtReleased);
    ++seen.pt_events;
}

bool QpEngine::pt_admit(WalkSeen &seen, QpCounters &counters, bool is_ineq, Index idx) {
    if (seen.pt_ineq.empty() && seen.pt_bound.empty()) {
        return false;
    }
    const bool reentry = (pt_flags(seen, is_ineq, idx) & kPtReleased) != 0;
    pt_mark(seen, is_ineq, idx, static_cast<std::uint8_t>(kPtEntered | (reentry ? kPtExempt : 0)));
    if (reentry) {
        ++seen.pt_events;
        ++counters.reentry_exemptions;
    }
    return reentry;
}

QpEngine::ExchangePick QpEngine::select_exchange(const QpProblem &qp, const WorkingSet &ws,
                                                 const Vec &shift, const WalkSeen &seen,
                                                 const detail::DepTest &t, bool is_ineq, Index idx,
                                                 BoundState side, const Vec &lambda_e,
                                                 const Vec &lambda_i, const Vec &z, bool moves,
                                                 bool unlock) {
    (void)lambda_e;
    const Index n = qp.n();
    const std::vector<Index> &aw = ws.active_ineq();
    const std::vector<BoundState> &bs = ws.bound_state();
    ExchangePick pk;
    pk.sign = (!is_ineq && side == BoundState::kAtLower) ? -1.0 : 1.0;
    pk.v_e = pk.sign * t.v_e;
    pk.v_w = pk.sign * t.v_w;
    pk.v_pin = Vec::Zero(n);

    // Scaled coefficients |v_k| ||c_k|| and g = sum_k v_k d_k + v_e^T be - d_r.
    double vmax = 0.0;
    double g = 0.0;
    for (Index r = 0; r < qp.me(); ++r) {
        vmax = std::max(vmax, std::abs(pk.v_e(r)) * qp.Ae.row(r).norm());
        g += pk.v_e(r) * qp.be(r);
    }
    Vec row_norm(static_cast<Index>(aw.size()));
    for (std::size_t k = 0; k < aw.size(); ++k) {
        const auto kk = static_cast<Index>(k);
        row_norm(kk) = qp.Ai.row(aw[k]).norm();
        vmax = std::max(vmax, std::abs(pk.v_w(kk)) * row_norm(kk));
        g += pk.v_w(kk) * qp.bi(aw[k]);
    }
    for (Index i = 0; i < n; ++i) {
        const BoundState st = bs[static_cast<std::size_t>(i)];
        if (st == BoundState::kFree) {
            continue;
        }
        // c = -e_i on a lower pin (d = -l_i); +e_i on an upper or fixed pin.
        const bool lower = st == BoundState::kAtLower;
        pk.v_pin(i) = (lower ? -1.0 : 1.0) * pk.sign * t.v_pin(i);
        vmax = std::max(vmax, std::abs(pk.v_pin(i)));
        g += pk.v_pin(i) *
             (lower ? -qp.lower(i) : (st == BoundState::kAtUpper ? qp.upper(i) : qp.lower(i)));
    }
    g -= is_ineq ? qp.bi(idx) : (side == BoundState::kAtUpper ? qp.upper(idx) : -qp.lower(idx));
    pk.g = g;

    const double tol = detail::kDepCoeffTol * vmax;
    pk.cond1 = true;
    double best_ratio = 0.0;
    double best_m = 0.0;
    double best_scaled = 0.0;
    // The last-resort pool: admissible shift-driven rows, same rule.
    ExchangePick sh;
    bool unshifted_admissible = false; // S1: shifted rows rank last
    double sh_ratio = 0.0;
    double sh_m = 0.0;
    double sh_scaled = 0.0;
    // `bland`: the unlock scan's rule (R-B) -- min ratio, ties to the first
    // candidate in scan order (the lowest index).
    bool bland = false;
    const auto consider = [&](bool row, Index member, double v, double scaled, double m,
                              bool removable, bool shifted = false) {
        if (!(scaled > tol)) {
            return;
        }
        pk.cond1 = false; // a sign-constrained member carries a positive coefficient
        if (!removable) {
            return;
        }
        const double ratio = std::max(m, 0.0) / v;
        if (shifted) {
            if (!sh.found || ratio < sh_ratio ||
                (!bland && ratio == sh_ratio && (m < sh_m || (m == sh_m && scaled > sh_scaled)))) {
                sh.found = true;
                sh.j = member;
                sh_ratio = ratio;
                sh_m = m;
                sh_scaled = scaled;
            }
            return;
        }
        unshifted_admissible = true;
        // Candidates arrive rows first, then pins, each ascending, so a full
        // tie keeps the earlier one.
        const bool better = !pk.found || ratio < best_ratio ||
                            (!bland && ratio == best_ratio &&
                             (m < best_m || (m == best_m && scaled > best_scaled)));
        if (better) {
            pk.found = true;
            pk.j_is_ineq = row;
            pk.j = member;
            best_ratio = ratio;
            best_m = m;
            best_scaled = scaled;
        }
    };
    const auto scan = [&](bool unlock) {
        const auto removable = [&](bool row, Index member) {
            const std::uint8_t f = pt_flags(seen, row, member);
            if (moves || (f & kPtEntered) == 0) {
                return true;
            }
            return unlock && (f & (kPtReleased | kPtExempt)) == 0;
        };
        for (std::size_t k = 0; k < aw.size(); ++k) {
            const auto kk = static_cast<Index>(k);
            const Index row = aw[k];
            consider(true, row, pk.v_w(kk), pk.v_w(kk) * row_norm(kk), lambda_i(row),
                     removable(true, row), shift(row) > 0.0);
        }
        for (Index i = 0; i < n; ++i) {
            const BoundState st = bs[static_cast<std::size_t>(i)];
            if (st == BoundState::kFree || st == BoundState::kFixed) {
                continue; // kFixed is free-signed: mandatory
            }
            const double m = st == BoundState::kAtLower ? z(i) : -z(i);
            consider(false, i, pk.v_pin(i), pk.v_pin(i), m, removable(false, i));
        }
    };
    // THE LOCKED-VERTEX UNLOCK (R-B; ExchangePick's header comment) scans with
    // the locked members made removable and the Bland tie rule.
    bland = unlock && !moves;
    scan(bland);
    pk.unlocked = bland && (pk.found || sh.found);
    if (pk.found) {
        pk.sigma = best_ratio;
    } else if (sh.found) {
        pk.found = true;
        pk.j_is_ineq = true;
        pk.j_shifted = true;
        pk.j = sh.j;
        pk.sigma = sh_ratio;
    }
    // A shifted row is never chosen while an unshifted admissible member exists.
    assert(!pk.j_shifted || !unshifted_admissible);
    (void)unshifted_admissible;
    return pk;
}

bool QpEngine::verify_farkas(const QpProblem &qp, const WorkingSet &ws,
                             const detail::DepFactorView &view, const detail::DepTest &t,
                             const ExchangePick &pick, bool is_ineq, Index idx, double row_scale,
                             const QpOptions &opts, QpFarkasCertificate &cert,
                             Index &solves) const {
    const Index n = qp.n();
    const std::vector<Index> &aw = ws.active_ineq();
    cert = QpFarkasCertificate{};
    cert.present = true;
    cert.blocker_is_ineq = is_ineq;
    cert.blocker = idx;
    cert.sign = pick.sign;
    Vec c = Vec::Zero(n);
    if (is_ineq) {
        c = qp.Ai.row(idx).transpose();
    } else {
        c(idx) = 1.0; // unoriented, as the dependency test saw it
    }
    // The multipliers, refined mu-free (S25'); rho == 0 is exact already.
    const bool refine = view.usable() && t.rho > 0.0;
    const detail::DepTest rt =
        refine ? detail::certificate_coefficients(qp, ws, view, c, opts) : detail::DepTest{};
    solves += refine ? rt.solves : 0;
    Vec v_e = refine ? rt.v_e : t.v_e;
    Vec v_w = refine ? rt.v_w : t.v_w;
    const double sign = pick.sign;
    const auto oriented_yi = [&](const Vec &vw) {
        Vec yi = Vec::Zero(qp.mi());
        for (std::size_t k = 0; k < aw.size(); ++k) {
            yi(aw[k]) = std::min(sign * vw(static_cast<Index>(k)), 0.0);
        }
        return yi;
    };
    // sum of box_part^2 over coordinates with no finite bound on the needed side.
    const auto open_sq = [&](const Vec &ve, const Vec &vw) {
        Vec b = sign * c;
        if (qp.me() > 0) {
            b -= qp.Ae.transpose() * Vec(sign * ve);
        }
        if (qp.mi() > 0) {
            b -= qp.Ai.transpose() * oriented_yi(vw);
        }
        double sq = 0.0;
        for (Index i = 0; i < n; ++i) {
            const double bound = b(i) > 0.0 ? qp.lower(i) : qp.upper(i);
            if (b(i) != 0.0 && !(std::abs(bound) < detail::kEngineInfBound)) {
                sq += b(i) * b(i);
            }
        }
        return sq;
    };
    // A single-row dependency (one member's scaled coefficient above
    // kDepCoeffTol of the largest): its EXACT coefficient, c's least-squares
    // fit on that row over the free columns, kept when it lowers the residual
    // (S25'; the refinement's contraction is ~ mu ||H|| / ||a||^2 per step,
    // slow on a short row).
    {
        const auto &bs = ws.bound_state();
        double smax = 0.0;
        for (Index r = 0; r < qp.me(); ++r) {
            smax = std::max(smax, std::abs(v_e(r)) * qp.Ae.row(r).norm());
        }
        for (std::size_t k = 0; k < aw.size(); ++k) {
            smax = std::max(smax, std::abs(v_w(static_cast<Index>(k))) * qp.Ai.row(aw[k]).norm());
        }
        Index count = 0;
        bool one_eq = false;
        Index one = -1;
        for (Index r = 0; r < qp.me(); ++r) {
            if (std::abs(v_e(r)) * qp.Ae.row(r).norm() > detail::kDepCoeffTol * smax) {
                ++count;
                one_eq = true;
                one = r;
            }
        }
        for (std::size_t k = 0; k < aw.size(); ++k) {
            const auto kk = static_cast<Index>(k);
            if (std::abs(v_w(kk)) * qp.Ai.row(aw[k]).norm() > detail::kDepCoeffTol * smax) {
                ++count;
                one_eq = false;
                one = kk;
            }
        }
        if (smax > 0.0 && count == 1) {
            const Vec row = one_eq ? Vec(qp.Ae.row(one).transpose())
                                   : Vec(qp.Ai.row(aw[static_cast<std::size_t>(one)]).transpose());
            double ac = 0.0;
            double aa = 0.0;
            for (Index i = 0; i < n; ++i) {
                if (bs[static_cast<std::size_t>(i)] == BoundState::kFree) {
                    ac += row(i) * c(i);
                    aa += row(i) * row(i);
                }
            }
            if (aa > 0.0) {
                Vec e_exact = Vec::Zero(qp.me());
                Vec w_exact = Vec::Zero(static_cast<Index>(aw.size()));
                (one_eq ? e_exact : w_exact)(one) = ac / aa;
                if (open_sq(e_exact, w_exact) < open_sq(v_e, v_w)) {
                    v_e = std::move(e_exact);
                    v_w = std::move(w_exact);
                }
            }
        }
    }
    c *= sign;
    cert.y_e = sign * v_e;
    cert.y_i = oriented_yi(v_w);
    // The rounding floor (kCertExactKappa): ||y||_1 with c's unit coefficient,
    // max_k ||a_k|| over c and the rows the certificate uses, m = me + |W|.
    double y1 = 1.0 + cert.y_e.lpNorm<1>() + cert.y_i.lpNorm<1>();
    double amax = c.norm();
    for (Index r = 0; r < qp.me(); ++r) {
        if (cert.y_e(r) != 0.0) {
            amax = std::max(amax, qp.Ae.row(r).norm());
        }
    }
    for (const Index r : aw) {
        if (cert.y_i(r) != 0.0) {
            amax = std::max(amax, qp.Ai.row(r).norm());
        }
    }
    cert.exact_tol = detail::kCertExactKappa *
                     static_cast<double>(n + qp.me() + static_cast<Index>(aw.size())) *
                     std::numeric_limits<double>::epsilon() * y1 * amax;
    cert.box_part = c;
    double implied = 0.0;
    if (qp.me() > 0) {
        cert.box_part -= qp.Ae.transpose() * cert.y_e;
        implied += cert.y_e.dot(qp.be);
    }
    if (qp.mi() > 0) {
        cert.box_part -= qp.Ai.transpose() * cert.y_i;
        implied += cert.y_i.dot(qp.bi);
    }
    // A box_part entry on a coordinate with no finite bound on its side is
    // the dependency residual (a dependent test accepted rho <= kDepTol): at
    // or below kDepTol * ||c|| it is dropped and its norm kept in `residual`;
    // above it the certificate needs an infinite bound and fails. A dropped
    // entry above the rounding floor (exact_tol) makes the certificate
    // TOLERANCED -- a proof for the QP with that coefficient zeroed, not for
    // this one -- and the caller ends it kNumericalError, not kInfeasible.
    const double drop_tol = detail::kDepTol * c.norm();
    double dropped_sq = 0.0;
    for (Index i = 0; i < n; ++i) {
        const double b = cert.box_part(i);
        if (b == 0.0) {
            continue;
        }
        const double bound = b > 0.0 ? qp.lower(i) : qp.upper(i);
        if (!(std::abs(bound) < detail::kEngineInfBound)) {
            if (std::abs(b) <= drop_tol) {
                dropped_sq += b * b;
                continue;
            }
            implied = -std::numeric_limits<double>::infinity();
            break;
        }
        implied += b * bound;
    }
    cert.residual = std::sqrt(dropped_sq);
    cert.implied = implied;
    cert.limit = is_ineq ? qp.bi(idx) : (pick.sign > 0.0 ? qp.upper(idx) : -qp.lower(idx));
    cert.margin = detail::kInfeasibilityMarginFactor * row_tolerance(row_scale, 0.0, opts);
    const bool verified = std::isfinite(implied) && implied - cert.limit > cert.margin;
    if (!verified) {
        cert = QpFarkasCertificate{};
    }
    return verified;
}

double QpEngine::ratio_test_excluding(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                      const Vec &Aix, const Vec &p,
                                      const std::vector<std::pair<bool, Index>> &skip,
                                      BlockKind &kind, Index &block_idx, BoundState &block_state,
                                      bool cap_at_unit) {
    const auto skipped = [&](bool is_ineq, Index idx) {
        return std::find(skip.begin(), skip.end(), std::make_pair(is_ineq, idx)) != skip.end();
    };
    double best = std::numeric_limits<double>::infinity();
    kind = BlockKind::kNone;
    if (qp.mi() > 0) {
        const Vec Aip = qp.Ai * p;
        for (Index j = 0; j < qp.mi(); ++j) {
            if (in_working(ws, j) || Aip(j) <= detail::kEngineDenomTol || skipped(true, j)) {
                continue;
            }
            const double ratio = std::max(0.0, qp.bi(j) - Aix(j)) / Aip(j);
            if (ratio < best) {
                best = ratio;
                kind = BlockKind::kIneq;
                block_idx = j;
            }
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree ||
            skipped(false, i)) {
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
        }
    }
    if (!cap_at_unit) {
        return best; // infinite exactly when kind == kNone
    }
    if (kind != BlockKind::kNone && best > 1.0 + detail::kEngineStepTieTol) {
        kind = BlockKind::kNone;
    }
    return std::min(1.0, best);
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
#ifdef HVEN_TESTING
    // The seam's record that K0 was FACTORIZED AFRESH on this pass, as opposed
    // to a retained K0 that only had borders synced onto it. One hook here
    // covers every call site the walk has.
    detail::testing::qp_walk_note_eqp(
        [](detail::testing::QpWalkEqpEvidence &s) { s.k0_rebuilt = true; });
#endif
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
    // The dense border factor takes this engine's thread count too (M6 W5
    // T8.8), so the walk's Schur updates run at the width the sparse factors
    // do rather than at whatever MKL's process default happens to be.
    border.schur.emplace(border.kkt, opts, threads_);
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
                          WorkingSet &ws, DropRecord &dropped, QpCounters &counters,
                          const WalkSeen *pt, double *exempt_worst) const {
    // The zero-motion re-entry rule's filter: a drop-exempt member is no
    // candidate; its violation is reported for the exhaustion test.
    const auto exempt = [&](bool is_ineq, Index idx, double viol) {
        if (pt == nullptr || (pt_flags(*pt, is_ineq, idx) & kPtExempt) == 0) {
            return false;
        }
        // A drop this point already undid on ride_sign's veto does not count
        // toward exhaustion: its multiplier is below the slope's own resolution
        // (4c, THE FLAT FACE).
        if (exempt_worst != nullptr && (pt_flags(*pt, is_ineq, idx) & kPtNoise) == 0) {
            *exempt_worst = std::max(*exempt_worst, viol);
        }
        return true;
    };
    struct Candidate {
        bool is_ineq = false;
        Index idx = -1;
        double violation = 0.0;
        double angle = 0.0; // violation / ||constraint gradient||
    };

#ifdef HVEN_TESTING
    // M7 W2 M5, ARM A -- THE SCALED DROP TOLERANCE. M4c §4.2 read YORKNET's
    // whole recurrent tail as this file's own documented asymmetry firing: the
    // two tests below compare a GRADIENT RESIDUAL against `opt_tol`
    // ABSOLUTELY, while `free_block_stationarity` / `free_block_is_stationary`
    // one header away compare the same class of residual RELATIVELY and say
    // why in their own comment. Under the arm the two tests below become
    // `viol > opt_tol * scale`, with `scale` the value THAT function just
    // computed for this pass -- shared through the arm state, never
    // re-derived. Papers: GMW (1981) §8.3, N&W 2nd ed. §16.5, GMSW (1989) §2.
    //
    // THE GUARDS ARE ADDITIVE. Every production statement below is left token
    // for token as it was; the arm can only remove a candidate the absolute
    // test already admitted, and with the arm off no guard is evaluated at
    // all. The COUNT and the DECISION are the same call
    // (`QpDegeneracyArmState::admits`), asked at both sweeps, so the two can
    // neither diverge from each other nor disagree between the row block and
    // the bound block.
    using Adeg = detail::testing::QpDegeneracyArmState;
    const bool arm_a_on = Adeg::scaled_drop_on();
    const double arm_a_scale = arm_a_on ? Adeg::drop_scale() : 1.0;
    const double arm_a_tol = opts_.opt_tol * arm_a_scale;
    const Index arm_a_refused_base = Adeg::scaled_candidates_refused;
    double arm_a_pass_max_ratio = 0.0;
#endif
    std::vector<Candidate> cands;
    for (Index row : ws.active_ineq()) {
        if (shift(row) > 0.0) {
            continue; // still being driven to feasibility; sign says nothing
        }
        const double viol = -lambda_i(row);
        if (viol <= opts_.opt_tol || exempt(true, row, viol)) {
            continue;
        }
#ifdef HVEN_TESTING
        if (arm_a_on && !Adeg::admits(viol, arm_a_tol, arm_a_scale)) {
            arm_a_pass_max_ratio = std::max(arm_a_pass_max_ratio, viol / arm_a_scale);
            continue;
        }
#endif
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
        if (viol > opts_.opt_tol && !exempt(false, i, viol)) {
#ifdef HVEN_TESTING
            if (arm_a_on && !Adeg::admits(viol, arm_a_tol, arm_a_scale)) {
                arm_a_pass_max_ratio = std::max(arm_a_pass_max_ratio, viol / arm_a_scale);
                continue;
            }
#endif
            cands.push_back({false, i, viol, viol}); // ||e_i|| == 1
        }
    }
#ifdef HVEN_TESTING
    // The arm's per-pass read-out, recorded whether or not an arm is armed:
    // an OFF trace carries the scale and the scaled tolerance the arm WOULD
    // have used, which is what makes an OFF/ON comparison a comparison of one
    // decision rather than of two differently instrumented runs. The refusal
    // COUNT is DIFFERENCED off the arm's own cumulative counter rather than
    // tallied a second time here, so a candidate can only be counted by the
    // call that refused it.
    {
        const Index refused_here = Adeg::scaled_candidates_refused - arm_a_refused_base;
        // `drop_prevented_by_scale` is the arm's whole claim on YORKNET: not
        // that a different constraint was released, but that the release did
        // not happen. It needs BOTH halves -- an empty candidate set AND at
        // least one refusal -- because an empty set with no refusal is simply
        // a KKT point.
        const bool prevented = cands.empty() && refused_here > 0;
        if (prevented) {
            ++Adeg::scaled_drops_prevented;
        }
        const double published = Adeg::stat_scale;
        detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
            r.drop_scale = published;
            r.drop_scaled_tol = opts_.opt_tol * published;
            r.drop_refused_by_scale = refused_here;
            if (refused_here > 0) {
                r.drop_max_refused_ratio = arm_a_pass_max_ratio;
            }
            r.drop_prevented_by_scale = prevented;
        });
    }
#endif
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

#ifdef HVEN_TESTING
    // ARM L, the drop half. Bland, "New finite pivoting rules for the simplex
    // method", Mathematics of Operations Research 2 (1977) 103-107: fix ONE
    // total order on the constraint index set and always take the smallest
    // eligible index; a cycle would have to re-enter its own largest-index
    // constraint, which the rule forbids. The order need only be FIXED, which
    // is what licenses one order over two index spaces (inequality rows and
    // variable bounds).
    //
    // THE FORM THAT RUNS BY DEFAULT IS THE TIE-BREAK, not the least-index
    // choice: the order decides only among the Dantzig TIE WINDOW. That is
    // strictly weaker than the rule above, which needs the least index over the
    // whole eligible set; `lex_full_pricing` selects that stronger form and is
    // OFF by default. AND NEITHER FORM IS BLAND'S RULE (FIX ROUND 1, review
    // Important 3): the full-pricing form is a BLAND-STYLE ANALOGUE of the two
    // least-index choices, and Bland's finiteness theorem -- stated for a
    // simplex BASIS moving by a single pivot -- does NOT cover this walk, which
    // has a working set over two index spaces, an EQP re-solve and a snap every
    // minor, a negative-curvature ride and recovery branches. The gap is
    // declared in qp_anti_cycling_arm.h and in the round's README; nothing here
    // is reported as a finiteness guarantee.
    if (detail::testing::QpAntiCycleArmState::lex_on()) {
        using Ac = detail::testing::QpAntiCycleArmState;
        const Candidate *lex = nullptr;
        Ac::LexKey best_key{};
        for (const auto &c : cands) {
            if (!Ac::params.lex_full_pricing && c.violation < best_viol - tie_window) {
                continue;
            }
            Ac::LexKey key{c.idx, 0, c.is_ineq ? 0 : 1};
            if (!c.is_ineq) {
                key.side = ws.bound_state()[static_cast<std::size_t>(c.idx)] == BoundState::kAtUpper
                               ? 1
                               : 0;
            }
            if (lex == nullptr || key < best_key) {
                lex = &c;
                best_key = key;
            }
        }
        if (lex != nullptr) {
            if (lex != pick) {
                ++Ac::lex_drop_switches;
            }
            pick = lex;
        }
    }

    // The seam's per-minor record of WHY this constraint left: the Dantzig
    // violation that picked it, the tolerance a candidate had to clear, the
    // best violation in the set, and the candidate/tie counts behind
    // QpCounters::drop_ties. Read-only -- nothing above or below depends on it.
    detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
        r.drop_route = detail::testing::QpWalkDropRoute::kDantzig;
        r.block = pick->is_ineq ? detail::testing::QpWalkBlock::kIneq
                                : detail::testing::QpWalkBlock::kBound;
        r.block_index = pick->idx;
        if (!pick->is_ineq) {
            r.block_side = ws.bound_state()[static_cast<std::size_t>(pick->idx)];
            r.bound_gap = qp.upper(pick->idx) - qp.lower(pick->idx);
        }
        r.decision_multiplier = pick->violation;
        r.best_multiplier = best_viol;
        r.decision_tol = opts_.opt_tol;
        r.drop_candidates = static_cast<Index>(cands.size());
        r.drop_tied = in_window;

        // THE CANDIDATE SET ITSELF (FIX-ROUND 1), largest violation first, up
        // to the record's fixed capacity. The Dantzig rule picks by violation
        // ALONE, with no regard for whether the post-drop subspace admits a
        // usable direction -- so which OTHER constraints it could have released
        // is a candidate cause, and the identities and violations are the part
        // of that observable WITHOUT the extra factorization a predicted
        // post-drop inertia would need. Sorted on a COPY of the indices:
        // `cands` itself is untouched and the pick above is already made.
        std::vector<std::size_t> order(cands.size());
        for (std::size_t k = 0; k < order.size(); ++k) {
            order[k] = k;
        }
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return cands[a].violation > cands[b].violation;
        });
        const std::size_t keep =
            std::min(order.size(), detail::testing::QpWalkMinorRecord::kMaxRecordedCandidates);
        for (std::size_t k = 0; k < keep; ++k) {
            const Candidate &c = cands[order[k]];
            detail::testing::QpWalkDropCandidate &slot = r.candidates[k];
            slot.block = c.is_ineq ? detail::testing::QpWalkBlock::kIneq
                                   : detail::testing::QpWalkBlock::kBound;
            slot.index = c.idx;
            slot.side =
                c.is_ineq ? BoundState::kFree : ws.bound_state()[static_cast<std::size_t>(c.idx)];
            slot.violation = c.violation;
        }
        r.candidates_recorded = static_cast<Index>(keep);
    });
    if (arm_a_on) {
        // Counted on the branch that KEEPS the drop, never before it: the
        // function returns `true` from here on, so a drop counted here is a
        // drop the arm made.
        ++Adeg::scaled_drops_made;
    }
#endif
    dropped = DropRecord{};
    dropped.active = true;
    dropped.is_ineq = pick->is_ineq;
    dropped.idx = pick->idx;
    dropped.violation = pick->violation;
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

int QpEngine::ride_sign(const QpProblem &qp, const DropRecord &dropped, const Vec &p, const Vec &x,
                        bool *predicted) const {
    const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
    const double slope = gx.dot(p);
    const double slope_tol = detail::kEngineDenomTol * gx.norm() * p.norm();
    const double leaves = -dropped_directional(qp, dropped, p);
    const double leaves_tol =
        detail::kEngineDenomTol * dropped_gradient_norm(qp, dropped) * p.norm();

    // DESCENT picks the sign, and only strict descent qualifies -- a flat
    // direction has no descending sign and declines. THE FLAT FACE (4c): the
    // MEASURED slope is the exact directional derivative of the objective and
    // decides WHENEVER IT RESOLVES; the drop's own PREDICTED slope decides only
    // below that resolution, where the measured one says nothing. A ride
    // therefore never ascends a slope the arithmetic could read.
    const double s_pred = opts_.independent_working_set
                              ? dropped.violation * dropped_directional(qp, dropped, p)
                              : 0.0;
    int sign = 0;
    if (slope < -slope_tol) {
        sign = 1;
    } else if (slope > slope_tol) {
        sign = -1;
    } else if (s_pred < -slope_tol || s_pred > slope_tol) {
        sign = s_pred < 0.0 ? 1 : -1;
        if (predicted != nullptr) {
            *predicted = true;
        }
    }
#ifdef HVEN_TESTING
    // The OTHER TWO of section 4c's three refusals, and the four numbers that
    // separate them: a FLAT slope (no descending sign exists) is not the same
    // failure as descent and release-feasibility DISAGREEING, and kDeclined
    // says neither. The classification duplicates the test on the next line
    // READ-ONLY -- it does not replace it, and nothing below reads it.
    {
        const bool flat = sign == 0;
        const bool disagree = !flat && static_cast<double>(sign) * leaves < -leaves_tol;
        detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
            r.ride_slope = slope;
            r.ride_slope_tol = slope_tol;
            r.ride_leaves = leaves;
            r.ride_leaves_tol = leaves_tol;
            r.ride_sign_chosen = (flat || disagree) ? 0 : sign;
            if (flat) {
                r.ride_outcome = detail::testing::QpWalkRideOutcome::kSlopeFlatVeto;
            } else if (disagree) {
                r.ride_outcome = detail::testing::QpWalkRideOutcome::kSignDisagreementVeto;
            }
        });
    }
#endif
    // FEASIBILITY vetoes it: the descending sign must not step back into
    // the constraint the drop rule just released.
    if (sign == 0 || static_cast<double>(sign) * leaves < -leaves_tol) {
        return 0; // no admissible sign: decline
    }
    return sign;
}

bool QpEngine::ride_drop_is_noise(const QpProblem &qp, const DropRecord &dropped, const Vec &p,
                                  const Vec &x) const {
    // THE FLAT FACE (4c). g'p = s + rho'p with rho the point's own first-order
    // residual, so the difference of the two slopes ride_sign already has IS
    // that residual along p -- no extra solve, and nothing to estimate. The
    // resolution of the slope is the larger of the dot product's own floor and
    // that residual; a multiplier at or below it did not predict anything.
    const Vec gx = qp.H.selfadjointView<Eigen::Upper>() * x + qp.g;
    const double slope = gx.dot(p);
    const double s_pred = dropped.violation * dropped_directional(qp, dropped, p);
    const double resolution =
        std::max(detail::kEngineDenomTol * gx.norm() * p.norm(), std::abs(slope - s_pred));
    return std::abs(s_pred) <= resolution;
}

double QpEngine::dropped_gradient_norm(const QpProblem &qp, const DropRecord &dropped) {
    return dropped.is_ineq ? qp.Ai.row(dropped.idx).norm() : 1.0; // ||e_i|| == 1
}

QpEngine::RideOutcome QpEngine::ride_negative_curvature(
    const QpProblem &qp, const DropRecord &dropped, QpCounters &counters, WalkSeen &seen,
    const Vec &p, const Vec &Aix, WorkingSet &ws, Vec &x, const detail::DepFactorView *dep,
    const QpOptions &opts, RideBlocker &blocked) const {
    if (!ride_stays_in_working_set(qp, ws, p)) {
#ifdef HVEN_TESTING
        // The FIRST of section 4c's three refusals: `p` leaves the RETAINED
        // working set's nullspace. QpEngine::RideOutcome folds all three into
        // kDeclined, so this is the only place this one is observable.
        detail::testing::qp_walk_note([](detail::testing::QpWalkMinorRecord &r) {
            r.ride_outcome = detail::testing::QpWalkRideOutcome::kNullspaceVeto;
        });
#endif
        return RideOutcome::kVetoed;
    }
    bool predicted = false;
    const int sign = ride_sign(qp, dropped, p, x, &predicted);
    if (sign == 0) {
        return RideOutcome::kVetoed;
    }
    if (predicted) {
        ++counters.ride_predicted_signs;
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
#ifdef HVEN_TESTING
    // ARM B1 AT THE RIDE'S ADD SITE (amendment A3, site 2 of six).
    //
    // IT IS ASKED HERE, BEFORE THE RIDE MUTATES ANYTHING. `x`, the counters
    // and the working set are all still untouched at this line, so a refusal
    // leaves no half-applied ride behind -- it returns `kDeclined`, a verdict
    // production already has and already handles (the caller falls through to
    // the ordinary step). That is why this hook refuses by DECLINING rather
    // than by skipping the two mutation statements below: skipping them would
    // leave `x` moved along a direction whose blocker never joined, which is
    // not a rule anyone pre-registered.
    //
    // THE RIDE TAKES ITS BLOCKER FROM THE SAME `ratio_test` the ordinary add
    // path uses, so an independence rule that covered one and not the other
    // would be a rule with a hole in exactly the place section 4c's ride
    // exists to reach.
    if (detail::testing::QpDegeneracyArmState::independent_add_on()) {
        const detail::testing::QpB1Admission adm = detail::testing::admit_if_independent(
            qp, ws, detail::testing::b1_site_rank(), kind == BlockKind::kIneq, block_idx,
            detail::testing::QpB1Site::kRideAdd);
        if (!adm.admit && adm.rank_ok) {
            // NO `ride_outcome` IS WRITTEN HERE. The three values that enum
            // carries are production's own three refusals, and a fourth
            // reason wearing one of their labels would corrupt every existing
            // read-out of it. The refusal is recorded where it belongs: in the
            // arm's own counter.
            ++detail::testing::QpDegeneracyArmState::indep_ride_refusals;
            return RideOutcome::kDeclined;
        }
    }
#endif
    if (dep != nullptr) {
        detail::DepTest t =
            test_candidate(qp, ws, *dep, kind == BlockKind::kIneq, block_idx, counters, opts);
        if (t.dependent()) {
            // Not admitted and nothing mutated: the caller's add-site rule
            // decides it (section 4c, THE DECLINED RIDE).
            blocked.kind = kind;
            blocked.idx = block_idx;
            blocked.state = block_state;
            blocked.alpha = alpha;
            blocked.dir = dir;
            blocked.test = std::move(t);
            return RideOutcome::kDependentBlocker;
        }
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
#ifdef HVEN_TESTING
        // ARM E, §4.1's Cases 2 and 3 ON THE RIDE (FIX ROUND 1, review
        // Important 2). The ride reaches its blocker through the SAME
        // `ratio_test` the ordinary add path uses, so armed with E it can be
        // handed an `alpha_min` FLOOR step here exactly as that path can; and
        // re-pinning the blocker exactly would undo the positive move the
        // floor exists to create. The predicate is the one the ordinary path
        // asks, not a copy of it. The guard is written so that the statement
        // below is PRODUCTION'S, unchanged, token for token.
        if (!detail::testing::QpAntiCycleArmState::expand_skips_exact_repin())
#endif
            x(block_idx) =
                (block_state == BoundState::kAtUpper) ? qp.upper(block_idx) : qp.lower(block_idx);
    }
    // WHAT THE RIDE DID, for the caller's point memory (4c, FINITENESS): the
    // blocker it admitted and the motion it made. `dir` is deliberately not
    // copied -- the ride has already taken it.
    blocked.kind = kind;
    blocked.idx = block_idx;
    blocked.state = block_state;
    blocked.alpha = alpha;
    blocked.dx_inf = alpha * dir.lpNorm<Eigen::Infinity>();
#ifdef HVEN_TESTING
    // The blocker a RIDE admitted. QpCounters folds it into ws_adds with every
    // other addition (that field's own doc says so), which is one of the six
    // mechanisms M3a addendum F could not separate.
    detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
        r.block = kind == BlockKind::kBound ? detail::testing::QpWalkBlock::kBound
                                            : detail::testing::QpWalkBlock::kIneq;
        r.block_index = block_idx;
        r.alpha = alpha;
        r.p_inf = dir.lpNorm<Eigen::Infinity>();
        r.dx_inf = alpha * dir.lpNorm<Eigen::Infinity>();
        if (kind == BlockKind::kBound) {
            r.block_side = block_state;
            r.bound_gap = qp.upper(block_idx) - qp.lower(block_idx);
        }
    });
#endif
    return RideOutcome::kStepped;
}

#ifdef HVEN_TESTING
// ===========================================================================
// M7 W2 M4's TWO ANTI-CYCLING ARMS, entirely inside this `#ifdef` so a
// production translation unit sees no token of them. Both are OFF by default;
// the switch and the parameters are in
// `include/hven/detail/qp/qp_anti_cycling_arm.h`, which states what each arm
// is, which paper it comes from, and what it is NOT claimed to prove.
//
// Every rule below carries the citation of the rule it transcribes:
//   EXPAND = Gill, Murray, Saunders and Wright, "A practical anti-cycling
//   procedure for linearly constrained optimization", Mathematical
//   Programming 45 (1989) 437-474;
//   Bland = Bland, "New finite pivoting rules for the simplex method",
//   Mathematics of Operations Research 2 (1977) 103-107.
// ===========================================================================
namespace {

using AcState = detail::testing::QpAntiCycleArmState;
using AcKey = detail::testing::QpAntiCycleArmState::LexKey;

/// EXPAND §4.5's `t` DEFAULTS TO hven's OWN engine threshold, BY CONSTRUCTION
/// (FIX ROUND 1, review Important 5). `QpAntiCycleParams::negligible_pivot` is
/// now READ by `arm_candidates` below, so it has to be true, and not merely
/// documented, that a default-parameter arm considers exactly the constraints
/// production considers. `qp_anti_cycling_arm.h` cannot name
/// `detail::kEngineDenomTol` in its initializer -- `qp_engine.h` includes that
/// header, through `qp_walk_trace.h`, ABOVE its own declaration of the constant
/// -- so the two literals are tied together HERE instead, where both are
/// visible, and a future edit to either one fails the build.
static_assert(detail::testing::QpAntiCycleParams{}.negligible_pivot == detail::kEngineDenomTol,
              "ARM E's default negligible-pivot tolerance must BE hven's own engine threshold: "
              "at the default the arms' candidate set is production's candidate set");

/// AND THE SAME TIE FOR M7 W2 M5's ARM B1, for the same reason and in the same
/// place. `qp_degeneracy_arm.h` cannot name these three constants either --
/// `qp_engine.h` includes that file, through `qp_walk_trace.h`, ABOVE its own
/// declarations of them -- so the literals are tied together HERE, where both
/// are visible. b1 re-enumerates the ratio test's candidates; if its
/// eligibility constants ever drifted from the engine's, it would be scanning
/// a DIFFERENT candidate set than the one it claims to re-order, and its own
/// per-pass self-check against production's blocker would start refusing
/// passes for a reason no counter could name. A future edit to either side
/// fails the build instead.
static_assert(detail::testing::QpDegeneracyParams{}.negligible_pivot == detail::kEngineDenomTol,
              "ARM B1's candidate enumeration must use hven's own ratio-test pivot threshold");
static_assert(detail::testing::QpDegeneracyParams{}.inf_bound == detail::kEngineInfBound,
              "ARM B1's candidate enumeration must use hven's own infinite-bound threshold");
static_assert(detail::testing::QpDegeneracyParams{}.step_tie_tol == detail::kEngineStepTieTol,
              "ARM B1's cap-at-unit convention must use hven's own step-tie tolerance");

/// `QpEngine::BlockKind`'s three values, re-declared here because that enum is
/// a PRIVATE nested type of `QpEngine` and these helpers are free functions.
/// Declaring a friend, or moving the helpers into the class, would mean
/// editing `qp_engine.h` -- which would move the DWARF line programme of every
/// inline function below the edit in the three production objects that include
/// it, for no gain. The two call sites translate, and the translation is
/// exhaustive.
enum class ArmBlock : int { kNone = 0, kIneq = 1, kBound = 2 };

/// One candidate of the ratio test, carrying everything BOTH arms need: the
/// step production itself would compute, the "textbook" exact step of EXPAND
/// (3.1)'s `step` function, the relaxed step of its first pass (3.2), the
/// pivot element (3.3a) selects on, and ARM L's lexicographic key.
struct ArmCandidate {
    bool is_ineq = false;
    Index idx = -1;
    BoundState state = BoundState::kFree; ///< the side a BOUND would be pinned at
    double prod_step = 0.0;               ///< production's own ratio, clamps included
    double step_exact = 0.0;              ///< EXPAND (3.1): the step to the EXACT bound
    double step_relaxed = 0.0;            ///< EXPAND (3.2): the step to the bound widened by delta
    double pivot = 0.0;                   ///< |p_r|, the pivot element
    AcKey key{};
};

/// EVERY CONSTRAINT PRODUCTION'S RATIO TEST WOULD CONSIDER, in production's own
/// scan order (inequality rows ascending, then bounds ascending), with the two
/// extra steps and the pivot attached.
///
/// The eligibility tests are PRODUCTION'S at the DEFAULT parameters -- the
/// working-set skip, and the negligible-pivot test against EXPAND §4.5's `t`,
/// which the arm reads from `QpAntiCycleParams::negligible_pivot` and whose
/// default is `detail::kEngineDenomTol` by the `static_assert` below. So at the
/// default neither arm can block on a constraint production would not have
/// considered; what an arm may do is choose a DIFFERENT one of them, and take
/// a different step. A leg that sets a NON-default `t` is deliberately changing
/// the eligible set, which is §4.5's own parameter doing §4.5's own job (FIX
/// ROUND 1, review Important 5: the field used to be printed and ignored).
std::vector<ArmCandidate> arm_candidates(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                         const Vec &Aix, const Vec &p, double delta) {
    std::vector<ArmCandidate> out;
    if (qp.mi() > 0) {
        const Vec Aip = qp.Ai * p;
        const auto &aw = ws.active_ineq();
        for (Index j = 0; j < qp.mi(); ++j) {
            if (std::binary_search(aw.begin(), aw.end(), j) ||
                Aip(j) <= AcState::params.negligible_pivot) {
                continue;
            }
            ArmCandidate c;
            c.is_ineq = true;
            c.idx = j;
            c.pivot = Aip(j);
            c.prod_step = std::max(0.0, qp.bi(j) - Aix(j)) / Aip(j);
            c.step_exact = (qp.bi(j) - Aix(j)) / Aip(j);
            c.step_relaxed = (qp.bi(j) + delta - Aix(j)) / Aip(j);
            c.key = AcKey{j, 0, 0};
            out.push_back(c);
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            continue;
        }
        ArmCandidate c;
        if (p(i) > AcState::params.negligible_pivot && qp.upper(i) < detail::kEngineInfBound) {
            c.state = BoundState::kAtUpper;
            c.prod_step = std::max(0.0, qp.upper(i) - x(i)) / p(i);
            c.step_exact = (qp.upper(i) - x(i)) / p(i);
            c.step_relaxed = (qp.upper(i) + delta - x(i)) / p(i);
            c.key = AcKey{i, 1, 1};
        } else if (p(i) < -AcState::params.negligible_pivot &&
                   qp.lower(i) > -detail::kEngineInfBound) {
            c.state = BoundState::kAtLower;
            // The SIGNED-ZERO site the census named: with x(i) exactly on its
            // lower bound this is `min(0, 0)/negative` == -0.0, which neither
            // beats nor is beaten by a +0.0 earlier in the scan, so the just-
            // released bound blocks at a zero step and is re-pinned.
            c.prod_step = std::min(0.0, qp.lower(i) - x(i)) / p(i);
            c.step_exact = (qp.lower(i) - x(i)) / p(i);
            c.step_relaxed = (qp.lower(i) - delta - x(i)) / p(i);
            c.key = AcKey{i, 0, 1};
        } else {
            continue;
        }
        c.is_ineq = false;
        c.idx = i;
        c.pivot = std::abs(p(i));
        out.push_back(c);
    }
    return out;
}

/// ARM E — the EXPAND procedure of §4.1, transcribed.
///
///   (first pass)   `(alpha_1, r_1) <- ratio_test(x, p, l - delta e, u + delta e, t)`
///                  — EXPAND §4.1's pseudo-code line 1, which is Harris's (3.2).
///   (second pass)  `r <- arg max |p_j|` over `j` with `step(x_j, p_j, l_j, u_j) <= alpha_1`,
///                  `alpha_2 <- step(x_r, p_r, l_r, u_r)` — (3.3a) and (3.3b).
///   (floor)        `alpha_min <- tau/|p_r|`; `alpha <- max{alpha_2, alpha_min}`
///                  — EXPAND §4.1's last two pseudo-code lines, and §8.1's
///                  `alpha_blk = max{alpha_min, alpha_2}` for the linearly
///                  constrained / QP context this walk is.
///
/// `alpha_min > 0` strictly, so ARM E NEVER returns the zero step production's
/// signed-zero ratio returns, and a constraint whose exact step is zero or
/// negative is handled by §4.1's Cases 2 and 3 rather than by a re-pin.
double expand_ratio_test(const QpProblem &qp, const WorkingSet &ws, const Vec &x, const Vec &Aix,
                         const Vec &p, ArmBlock &kind, Index &block_idx, BoundState &block_state,
                         bool cap_at_unit, QpCounters &counters) {
    kind = ArmBlock::kNone;
    AcState::last_step_was_floor = false;
    const std::vector<ArmCandidate> cands = arm_candidates(qp, ws, x, Aix, p, AcState::delta);
    if (cands.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    // FIRST PASS (3.2). alpha_1 is an UPPER BOUND on the allowable step: a
    // step of alpha_1 cannot violate any individual bound by more than delta.
    double alpha1 = std::numeric_limits<double>::infinity();
    for (const ArmCandidate &c : cands) {
        alpha1 = std::min(alpha1, c.step_relaxed);
    }

    // SECOND PASS (3.3a): the largest pivot among the constraints whose EXACT
    // step does not exceed alpha_1. Production's own pick and its tie count
    // are computed in the same sweep, so `pivot_switches` measures how often
    // the arm actually moved the choice and `counters.ratio_ties` keeps
    // meaning what it means on the OFF leg.
    const ArmCandidate *pick = nullptr;
    const ArmCandidate *prod_pick = nullptr;
    double prod_best = std::numeric_limits<double>::infinity();
    Index tied = 0;
    for (const ArmCandidate &c : cands) {
        if (c.prod_step < prod_best) {
            prod_best = c.prod_step;
            prod_pick = &c;
            tied = 1;
        } else if (c.prod_step == prod_best) {
            ++tied;
        }
        if (c.step_exact <= alpha1 && (pick == nullptr || c.pivot > pick->pivot)) {
            pick = &c;
        }
    }
    if (pick == nullptr) {
        // Unreachable: the candidate attaining alpha_1 has step_exact <
        // step_relaxed == alpha_1, because delta > 0 and its pivot is
        // non-negligible. Kept so that a future change to either step cannot
        // make this function return an uninitialized pick.
        return std::numeric_limits<double>::infinity();
    }

    const double alpha2 = pick->step_exact;                       // (3.3b)
    const double alpha_min = AcState::params.tau() / pick->pivot; // §4.1
    double alpha = std::max(alpha2, alpha_min);
    if (alpha > alpha2) {
        // §4.1 Cases 2 and 3: the blocking constraint moves a total distance
        // of tau and ends the minor violating its bound by at most delta.
        AcState::last_step_was_floor = true;
        ++AcState::min_steps;
    }

    kind = pick->is_ineq ? ArmBlock::kIneq : ArmBlock::kBound;
    block_idx = pick->idx;
    if (!pick->is_ineq) {
        block_state = pick->state;
    }
    if (tied > 1) {
        ++counters.ratio_ties; // observation only -- see QpCounters
    }
    if (prod_pick != nullptr &&
        (prod_pick->is_ineq != pick->is_ineq || prod_pick->idx != pick->idx)) {
        ++AcState::pivot_switches;
    }

    detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
        r.raw_ratio = alpha;
        r.ratio_tied = tied;
        r.ratio_capped = cap_at_unit;
    });

    // hven's own cap-at-unit convention, applied to the arm's step exactly as
    // production applies it to its own.
    if (!cap_at_unit) {
        return alpha;
    }
    if (alpha > 1.0 + detail::kEngineStepTieTol) {
        kind = ArmBlock::kNone; // the full step is interior: nothing joins
    }
    return std::min(1.0, alpha);
}

/// ARM L applied to the ratio test. Among every constraint that ATTAINS the
/// minimum ratio production just computed -- signed zeros included, since
/// `-0.0 == 0.0` compares true -- take the lexicographically smallest
/// `(index, side, block)`. Bland (1977) requires only that the order be FIXED;
/// this one is index-major, which is deliberately not production's block-major
/// scan order, since a rule that reproduced that order would change nothing.
void lex_retie_ratio(const QpProblem &qp, const WorkingSet &ws, const Vec &x, const Vec &Aix,
                     const Vec &p, double best, ArmBlock &kind, Index &block_idx,
                     BoundState &block_state) {
    if (kind == ArmBlock::kNone) {
        return;
    }
    const std::vector<ArmCandidate> cands = arm_candidates(qp, ws, x, Aix, p, 0.0);
    const ArmCandidate *pick = nullptr;
    for (const ArmCandidate &c : cands) {
        if (c.prod_step != best) {
            continue;
        }
        if (pick == nullptr || c.key < pick->key) {
            pick = &c;
        }
    }
    if (pick == nullptr) {
        return;
    }
    const ArmBlock new_kind = pick->is_ineq ? ArmBlock::kIneq : ArmBlock::kBound;
    if (new_kind != kind || pick->idx != block_idx) {
        ++AcState::lex_ratio_switches;
    }
    kind = new_kind;
    block_idx = pick->idx;
    if (!pick->is_ineq) {
        block_state = pick->state;
    }
}

} // namespace
#endif // HVEN_TESTING

double QpEngine::ratio_test(const QpProblem &qp, const WorkingSet &ws, const Vec &x, const Vec &Aix,
                            const Vec &p, BlockKind &kind, Index &block_idx,
                            BoundState &block_state, bool cap_at_unit, QpCounters &counters) {
#ifdef HVEN_TESTING
    // ARM E takes the whole ratio test: the two-pass EXPAND procedure is not
    // production's single pass with a parameter changed, and writing it as one
    // would make the OFF path a specialization rather than the production code
    // itself. With no arm on, control never reaches here.
    if (AcState::expand_on()) {
        ArmBlock arm_kind = ArmBlock::kNone;
        const double arm_alpha = expand_ratio_test(qp, ws, x, Aix, p, arm_kind, block_idx,
                                                   block_state, cap_at_unit, counters);
        kind = arm_kind == ArmBlock::kIneq    ? BlockKind::kIneq
               : arm_kind == ArmBlock::kBound ? BlockKind::kBound
                                              : BlockKind::kNone;
        return arm_alpha;
    }
#endif
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
#ifdef HVEN_TESTING
    // ARM L, the ratio-test half: re-decide the TIE -- and only the tie --
    // by the fixed lexicographic order. `best` is the uncapped minimum, so the
    // re-tie happens before hven's cap-at-unit convention below, exactly where
    // production made its own choice.
    if (AcState::lex_on()) {
        ArmBlock arm_kind = kind == BlockKind::kIneq    ? ArmBlock::kIneq
                            : kind == BlockKind::kBound ? ArmBlock::kBound
                                                        : ArmBlock::kNone;
        lex_retie_ratio(qp, ws, x, Aix, p, best, arm_kind, block_idx, block_state);
        kind = arm_kind == ArmBlock::kIneq    ? BlockKind::kIneq
               : arm_kind == ArmBlock::kBound ? BlockKind::kBound
                                              : BlockKind::kNone;
    }
    // The seam's per-minor reading of the ratio test: the UNCAPPED minimum and
    // its exact multiplicity, before the cap-at-unit clamp below can hide
    // either. Read-only; `best`, `tied` and `kind` are not touched.
    detail::testing::qp_walk_note([&](detail::testing::QpWalkMinorRecord &r) {
        r.raw_ratio = best;
        r.ratio_tied = kind == BlockKind::kNone ? 0 : tied;
        r.ratio_capped = cap_at_unit;
    });
#endif

    if (!cap_at_unit) {
        return best; // infinite exactly when kind == kNone
    }
    if (kind != BlockKind::kNone && best > 1.0 + detail::kEngineStepTieTol) {
        kind = BlockKind::kNone; // the full step is interior: nothing joins
    }
    return std::min(1.0, best);
}

} // namespace hven::solvers
