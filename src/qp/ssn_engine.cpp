// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// ssn_engine.cpp -- SsnEngine's safeguarded semismooth-Newton iteration, and
// everything that iteration orchestrates.
//
// This TU holds the DEFINITIONS of every SsnEngine member function except the
// constructor and the two trivial accessors; the class, its data members, its
// documentation and the file's SCOPE/loop contract stay in
// include/hven/detail/qp/ssn_engine.h. The engine is a plain (non-template)
// class, so CLAUDE.md §5's header/templated exception does not apply to it:
// what is here is orchestration -- the attempt loop, the FB classification,
// the KKT pattern cache, the globalization helpers and the exports.
//
// EVERY member travels together, into ONE TU, deliberately. select_branch is
// called once per FB row from the attempt loop's own two row loops, residual()
// walks every block once per attempt and once per line-search trial, and
// for_each_entry's emit lambda is invoked once per stored entry of K from
// sync_matrix -- so the inliner must still see across those sites exactly what
// it saw when they were siblings in the header. Same TU, same visibility, same
// inlining. for_each_entry is a member TEMPLATE over the emit callable, and it
// travels with its only two instantiation sites (sync_matrix's rebuild and
// refresh lambdas) rather than staying behind: the type set is closed by those
// two sites, and splitting it from them is what would cost the inlining.
// The only boundary this carve introduces is engine-to-CALLER -- solve(),
// finish_deferred_certification() and discard_deferred_certification() --
// crossed at most twice per QP subproblem.
//
// The `detail::` free functions and constants above SsnEngine (kSsnInfBound,
// the safeguard constants, ssn_fb, ssn_inertia_verdict, the row/bound/norm
// types) stay inline in the header: tests, bench, src/drivers/sqp_driver.cpp
// and include/hven/detail/warmstart/warm_start.h name them directly, and this
// TU inlines them exactly as before.
//
// FP arithmetic crosses this TU boundary under ONE uniform flag regime on both
// sides; every asserted counter must be bit-identical across the boundary, so
// a counter delta here is a FAILED CARVE, to be reverted or redrawn, never a
// re-derivation.

#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers {

void SsnEngine::solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
                      SsnResult *out) {
    solve(qp, start, sopts, SolveOverrides{}, out);
}

void SsnEngine::solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
                      const SolveOverrides &overrides, SsnResult *out) {
    if (out == nullptr) {
        throw std::invalid_argument("SsnEngine::solve: out must not be null");
    }
    // The deferred-certification contract, enforced rather than
    // documented: a pending certification is EVIDENCE ABOUT K, and this
    // solve is about to overwrite K. Dropping it silently would let a
    // caller believe a certificate nothing ever verified -- the
    // wrong-answer class banner section 7b exists to close.
    if (deferred_pending_) {
        throw std::invalid_argument(
            "SsnEngine::solve: a deferred certification from the previous solve is still "
            "pending (SsnOptions::defer_certification was set and neither "
            "finish_deferred_certification() nor discard_deferred_certification() was "
            "called). The pending verdict is evidence about a matrix this solve is about "
            "to overwrite");
    }
    qp.validate();
    validate_options(sopts);
    validate_overrides(overrides);

    // Resolved once, exactly like QpEngine::run()'s own resolution. The
    // BASE pair is what the caller asked for; eff_* carries the proximal
    // increment on top of it and is what for_each_entry emits.
    const double tr_radius = overrides.tr_radius;
    base_delta_ = overrides.primal_delta < 0.0 ? opts_.primal_delta : overrides.primal_delta;
    base_mu_ = overrides.dual_mu < 0.0 ? opts_.dual_mu : overrides.dual_mu;
    const bool guarded = sopts.safeguards == SsnSafeguards::kFull;
    prox_sigma_ = guarded ? sopts.prox_sigma_init : 0.0;
    // The ladder's OWN monotone state, which is `prox_sigma_` exactly
    // under the shipped rule and its FLOOR under the residual-driven
    // sizing rules.
    ladder_sigma_ = prox_sigma_;
    lm_sigma_ = 0.0;
    eff_delta_ = base_delta_ + prox_sigma_;
    eff_mu_ = base_mu_ + prox_sigma_;

    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    validate_start(qp, start);

    *out = SsnResult{};
    out->x = seed_vector(start.x, n, "x");
    // The bound rows depend on the START POINT through the trust region, so
    // they are built after the seed is resolved and before anything reads
    // bound_rows_ (the multiplier split is the first such reader).
    build_bound_rows(qp, out->x, tr_radius);
    const Index mb = static_cast<Index>(bound_rows_.size());
    out->lambda_e = seed_vector(start.lambda_e, me, "lambda_e");
    out->lambda_i = seed_vector(start.lambda_i, mi, "lambda_i");
    Vec lambda_b = split_bound_multipliers(qp, seed_vector(start.z, n, "z"));

    out->pattern_rebuilds = sync_matrix(qp, n, me, mi, mb) ? 1 : 0;

    // Scratch, sized once.
    Vec resid_x(n), resid_e(me);
    Vec slack_i(mi), slack_b(mb);
    Vec phi_i(mi), phi_b(mb);
    Vec rhs(dim_);
    std::vector<double> alpha(static_cast<std::size_t>(mi + mb));
    std::vector<double> beta(static_cast<std::size_t>(mi + mb));
    std::vector<double> row_resid(static_cast<std::size_t>(mi + mb));
    // The three-set partition, and the PREVIOUS one -- which is the
    // hysteresis state, not just the flip counter's memory.
    std::vector<detail::SsnRowClass> klass(static_cast<std::size_t>(mi + mb),
                                           detail::SsnRowClass::kInactive);
    std::vector<detail::SsnRowClass> prev_klass;
    bool any_klass = false;

    // The LINE SEARCH's trial POINT. A trial step has to be formed
    // somewhere and cannot be formed in place, because a rejected one has
    // to be discarded.
    //
    // **ITS RESIDUAL, HOWEVER, IS MEASURED IN THE WORKING SCRATCH ABOVE.**
    // A second set of residual blocks would be n + me + 2mi + 2mb doubles
    // -- ~40 MB at a bounds-heavy n = 1e6 QP. The price of not paying it
    // is ONE extra O(nnz) residual evaluation per SOLVE (not per attempt):
    // the loop re-evaluates at the returned point just before the activity
    // export, unconditionally, so the export cannot read blocks a trial
    // point left behind.
    Vec t_x(n), t_le(me), t_li(mi), t_lb(mb);

    // Divergence telemetry (SsnEscape::kInfeasibleSuspect). Every field is
    // WINDOWED and every window advances on ACCEPTED STEPS -- the three
    // properties detail::kSsnStallWindow derives, and the three the first
    // implementation did not have.
    double window_ref = std::numeric_limits<double>::infinity();
    double dual_window = 1.0; // ||lambda||inf at the last accepted step that PROGRESSED
    double dual_start = 1.0;  // ||lambda||inf at the seed
    double dual_prev = 1.0;   // ||lambda||inf at the PREVIOUS accepted iterate
    double dual_last = 1.0;   // ||lambda||inf at the current accepted iterate
    Index window_len = 0;
    Index stall_len = 0;    // the window length the stall verdict was taken on
    Index window_iter = -1; // the out->iters this window last advanced on
    bool window_damped = false;
    bool stalled = false;

    // The second-order verification's saved sigma (banner section 7b): >= 0 only
    // while a verification is running with the ladder temporarily dropped
    // to the caller's own regularization.
    double verify_sigma = -1.0;

    // ---- The residual sizing's own state -------------------------------
    //
    // `f_scale` is fixed at the START residual and never moves, so the
    // normalized r_k is a statement about progress rather than about the
    // problem's absolute scaling. `lm_sigma_`/`ladder_sigma_` are members
    // because escalate_prox() and set_prox_sigma() both need them; they are
    // reset per solve at the top of this function.
    const bool lm_sigma_rule = guarded && sopts.sigma_rule != SsnSigmaRule::kLadder;
    double f_scale = 1.0;

    // ---- The watchdog's own state ---------------------------------------
    //
    // All of it is untouched under the shipped kIterationZeroFree rule, and
    // the four vectors are not even sized there.
    const bool watchdog_rule = guarded && sopts.hint_rule == SsnHintRule::kWatchdog;
    bool wd_active = false;
    Index wd_used = 0;
    double wd_ref_merit = 0.0;
    double wd_best_merit = std::numeric_limits<double>::infinity();
    Vec wd_best_x, wd_best_le, wd_best_li, wd_best_lb;

    // ---- The Farkas gate's own snapshots ---------------------------------
    //
    // The DUAL VECTORS at the two reference points the two symptom routes
    // already measure their growth against -- the window's reference (the
    // STANDING route) and the previous accepted iterate (the EXHAUSTION
    // route). Sized and copied only under kFarkasGated.
    const bool farkas_rule =
        guarded && sopts.infeasibility_rule == SsnInfeasibilityRule::kFarkasGated;
    Vec wref_le, wref_li, wref_lb; // duals at the window's reference point
    Vec prev_le, prev_li, prev_lb; // duals at the previous accepted iterate
    Vec last_le, last_li, last_lb; // duals at the current accepted iterate
    if (farkas_rule) {
        wref_le = out->lambda_e;
        wref_li = out->lambda_i;
        wref_lb = lambda_b;
        prev_le = wref_le;
        prev_li = wref_li;
        prev_lb = wref_lb;
        last_le = wref_le;
        last_li = wref_li;
        last_lb = wref_lb;
    }

    for (Index it = 0;; ++it) {
        const detail::SsnNorms nrm = residual(qp, out->x, out->lambda_e, out->lambda_i, lambda_b,
                                              resid_x, resid_e, slack_i, slack_b, phi_i, phi_b);
        out->fb_residual = nrm.inf_norm;
        if (it == 0) {
            // The residual sizing's normalization, fixed once (see detail::kSsnLmSigmaC).
            f_scale = std::max(1.0, nrm.inf_norm);
        }

        // --- convergence, and THE SECOND-ORDER VERIFICATION ----
        //
        // Under kBare this is the bare method's test, unchanged and free.
        // Under kFull it does NOT exit: it opens a verification attempt,
        // which falls through to the classification and factorization
        // below and certifies only on the inertia verdict read THERE.
        // Banner section 7b is the whole contract.
        bool verifying = false;
        if (nrm.inf_norm <= sopts.fb_tol) {
            if (!guarded) {
                out->status = QpStatus::kOptimal;
                out->escape_reason = SsnEscape::kNone;
                break;
            }
            verifying = true;
            if (prox_sigma_ > 0.0) {
                verify_sigma = prox_sigma_;
                set_prox_sigma(0.0, qp, n, me, mi, mb);
            }
        }

        // --- divergence telemetry, BEFORE the budget test -----------
        //
        // Order matters and is deliberate: an infeasible QP that also runs
        // out of budget must report the DIAGNOSIS, not the budget. kBudget
        // says "this budget was too small", which on an infeasible QP is
        // both true and useless, and it is the value a caller would
        // respond to by re-solving with a bigger one.
        double dn = 0.0;
        if (guarded && !verifying) {
            dn = dual_norm(out->lambda_e, out->lambda_i, lambda_b);
            // The window advances once per ACCEPTED STEP. A ladder retry
            // re-enters this loop at the SAME iterate with the same
            // residual and must not be allowed to fill it.
            if (window_iter != out->iters) {
                dual_prev = dual_last;
                dual_last = std::max(dn, 1.0);
                if (farkas_rule) {
                    // The Farkas gate's VECTORS behind dual_prev/dual_last, carried in
                    // lock-step with the scalars so the certificate's
                    // increment and the growth conjunct's ratio are measured
                    // between the same two points.
                    prev_le = last_le;
                    prev_li = last_li;
                    prev_lb = last_lb;
                    last_le = out->lambda_e;
                    last_li = out->lambda_i;
                    last_lb = lambda_b;
                }
                if (window_iter < 0) {
                    dual_start = dual_last;
                }
                window_iter = out->iters;
                if (nrm.inf_norm <= window_ref * detail::kSsnStallImproveFactor) {
                    // Genuine progress: a fresh window, and the STANDING
                    // route's growth reference re-arms with it.
                    window_ref = nrm.inf_norm;
                    dual_window = dual_last;
                    if (farkas_rule) {
                        wref_le = out->lambda_e;
                        wref_li = out->lambda_i;
                        wref_lb = lambda_b;
                    }
                    window_len = 0;
                    stall_len = 0;
                    window_damped = false;
                    stalled = false;
                } else if (++window_len >= detail::kSsnStallWindow) {
                    if (window_damped) {
                        // sigma moved inside this window, so its slow
                        // progress is the safeguard's doing rather than the
                        // problem's. Discard it and measure a clean one.
                        //
                        // A verdict ALREADY DECLARED on a clean window is
                        // NOT withdrawn: it was established on undamped
                        // steps, and only genuine progress -- the re-arm
                        // branch above -- retracts it.
                        window_len = 0;
                        window_damped = false;
                    } else if (!stalled) {
                        stalled = true;
                        stall_len = window_len;
                    }
                }
            }
            // --- THE SYMPTOMS ARM, THE FARKAS CERTIFICATE FIRES -----------
            //
            // Under the shipped kSymptoms rule the two conjuncts below ARE
            // the exit test and `standing_fires` is exactly the shipped
            // condition. Under kFarkasGated they are only the ARMING
            // condition: the accumulated dual direction over this window,
            // projected onto the sign cone and normalized, must
            // additionally be an approximate Farkas direction. A stalled,
            // dual-growing but FEASIBLE QP has no such direction
            // (Hoffman-bound territory), which is exactly the
            // discrimination the symptom pair cannot make.
            bool standing_fires = stalled && dn >= dual_window * detail::kSsnDualGrowthFactor;
            double farkas_resid = 0.0;
            double farkas_gap = 0.0;
            if (standing_fires && farkas_rule &&
                !farkas_certificate(qp, out->lambda_e - wref_le, out->lambda_i - wref_li,
                                    lambda_b - wref_lb, mb, &farkas_resid, &farkas_gap)) {
                standing_fires = false;
                ++out->farkas_refusals;
            }
            if (standing_fires) {
                out->farkas_fired += farkas_rule ? 1 : 0;
                out->status = QpStatus::kInfeasible;
                out->escape_reason = SsnEscape::kInfeasibleSuspect;
                out->escape_detail = fmt::format(
                    "SsnEngine: infeasibility SUSPECTED (not certified) -- ||F||inf is {} "
                    "after {} accepted steps that did not improve on {} by the demanded "
                    "factor {}, while the multiplier norm grew from {} to {} over the same "
                    "steps, a factor {} above the {}x threshold. That pairing is what an "
                    "infeasible QP produces: phi(s, lambda) -> s as lambda -> +inf on a row "
                    "whose slack cannot be made non-negative, so the FB rows can only shrink "
                    "by pushing lambda up forever",
                    nrm.inf_norm, stall_len, window_ref, detail::kSsnStallImproveFactor,
                    dual_window, dn, dn / dual_window, detail::kSsnDualGrowthFactor);
                if (farkas_rule) {
                    // The Farkas certificate's own two numbers, carried out
                    // rather than discarded (this project's rule that a
                    // diagnostic never printed must still fold into what
                    // the caller receives). Under the shipped rule there
                    // is no certificate and nothing is appended, so the
                    // message is byte-identical there.
                    out->escape_detail += fmt::format(
                        ". THE FARKAS CERTIFICATE CONFIRMED IT: the normalized dual "
                        "increment over this window, projected onto the sign cone, has "
                        "relative residual ||A^T y||inf = {} (threshold {}) and relative "
                        "Farkas objective <b, y> = {} (threshold {})",
                        farkas_resid, detail::kSsnFarkasResidualTol, farkas_gap,
                        -detail::kSsnFarkasGapTol);
                }
                break;
            }
        }

        if (!verifying && it >= sopts.hard_budget) {
            out->status = QpStatus::kMaxIter;
            out->escape_reason = SsnEscape::kBudget;
            break;
        }

        // --- soft budget: arm the proximal term ---------------------
        //
        // NOTE. The guard reads `ladder_sigma_`, which IS `prox_sigma_`
        // under the shipped kLadder rule (the two are kept equal there by
        // construction) and is the LADDER's own state under the residual
        // rules -- so a solve whose sigma is nonzero only because the
        // residual sizing put it there still arms the ladder at
        // soft_budget, and `ssn_prox_updates` keeps meaning "the ladder
        // armed" in every configuration.
        if (guarded && !verifying && it >= sopts.soft_budget && ladder_sigma_ <= 0.0) {
            ladder_sigma_ = detail::kSsnProxInit;
            apply_sigma(qp, n, me, mi, mb);
            ++out->counters.ssn_prox_updates;
            window_damped = true;
        }

        // --- THE RESIDUAL-DRIVEN (LEVENBERG-MARQUARDT) SIZING ---
        //
        // Structurally skipped under the shipped kLadder rule. Under the
        // two residual rules sigma is re-sized from the CURRENT residual
        // every attempt, with the monotone ladder underneath it as a floor
        // -- so every escalation trigger still climbs a rung and every
        // escape route is exactly the shipped one.
        //
        // **window_damped IS SET ONLY ON AN INCREASE.** The flag means
        // "slow progress here is the safeguard's doing, so do not read it
        // as a stall"; a sigma that FALLS damps less than the step before
        // it, so a window spanning a fall is not contaminated -- and
        // marking every attempt damped would disable the standing
        // infeasibility route outright under a rule that re-sizes every
        // attempt.
        if (lm_sigma_rule && !verifying &&
            (sopts.sigma_rule == SsnSigmaRule::kResidualAlways || ladder_sigma_ > 0.0)) {
            const double r = nrm.inf_norm / f_scale;
            lm_sigma_ =
                std::min(std::max(detail::kSsnLmSigmaC * std::min(r, r * r), detail::kSsnProxInit),
                         detail::kSsnProxMax);
            // apply_sigma() is the ONE site that combines the ladder floor
            // with the residual size. Recomputing the max here as well
            // would make the floor UNFALSIFIABLE.
            //
            // **AND IT IS CALLED UNCONDITIONALLY, WHICH COSTS AN O(nnz)
            // VALUE REBUILD PER ARMED ATTEMPT**: apply_sigma() ->
            // set_prox_sigma() -> sync_matrix() re-emits K's VALUES even
            // when the combined sigma did not move (the REFRESH path -- no
            // factorization, no symbolic analysis, no counter, no
            // trajectory effect, but real work). Guarding the call on a
            // precomputed max would put the floor back at two sites, so
            // THE TRADE IS DELIBERATE. This is on the LEVER path only
            // (`lm_sigma_rule` is false at the shipped default), so
            // nothing shipped pays it.
            const double before = prox_sigma_;
            apply_sigma(qp, n, me, mi, mb);
            if (prox_sigma_ != before) {
                ++out->counters.ssn_prox_updates;
                window_damped = window_damped || prox_sigma_ > before;
            }
        }

        // --- generalized Jacobian element, per FB row ---------------
        const bool use_hint = (it == 0) && !start.activity_hint.empty();

        // --- THE WATCHDOG'S JUDGEMENT -------------------------------------
        //
        // Structurally skipped under the shipped kIterationZeroFree rule.
        //
        // IT RUNS BEFORE THE FACTORIZATION, deliberately: the merit at the
        // current iterate is all the watchdog needs, so a return-to-best
        // costs NO factorization -- it re-enters the loop at the stored
        // point and lets the ordinary machinery build the step there. A
        // watchdog that judged after the solve would pay for a direction it
        // then threw away, which is not the published rule's cost either.
        //
        // Three outcomes, in the order the rule states them:
        //   * the relaxed window has PAID OFF (Armijo decrease against the
        //     watchdog's own reference) -- close the window, everything
        //     from here is monotone;
        //   * the window has budget left -- take another relaxed step;
        //   * the window is exhausted without decrease -- RETURN TO THE
        //     BEST STORED POINT and close the window, so the next pass is
        //     an ordinary monotone Armijo step from the best point seen.
        bool wd_relaxed = false;
        if (watchdog_rule && !verifying) {
            if (!wd_active && it == 0 && use_hint) {
                wd_active = true;
                wd_used = 0;
                wd_ref_merit = nrm.merit;
                wd_best_merit = nrm.merit;
                wd_best_x = out->x;
                wd_best_le = out->lambda_e;
                wd_best_li = out->lambda_i;
                wd_best_lb = lambda_b;
            }
            if (wd_active) {
                if (nrm.merit < wd_best_merit) {
                    wd_best_merit = nrm.merit;
                    wd_best_x = out->x;
                    wd_best_le = out->lambda_e;
                    wd_best_li = out->lambda_i;
                    wd_best_lb = lambda_b;
                }
                if (wd_used > 0 &&
                    nrm.merit <= (1.0 - 2.0 * detail::kSsnArmijoSigma) * wd_ref_merit) {
                    wd_active = false;
                } else if (wd_used >= sopts.watchdog_q) {
                    wd_active = false;
                    ++out->watchdog_returns;
                    out->x = wd_best_x;
                    out->lambda_e = wd_best_le;
                    out->lambda_i = wd_best_li;
                    lambda_b = wd_best_lb;
                    continue; // re-enter at the best point, monotone from here
                } else {
                    wd_relaxed = true;
                    ++wd_used;
                }
            }
        }
        for (Index k = 0; k < mi; ++k) {
            const bool hinted = use_hint && !start.activity_hint.ineq.empty();
            select_branch(slack_i(k), out->lambda_i(k), phi_i(k), hinted,
                          hinted && start.activity_hint.ineq[static_cast<std::size_t>(k)], guarded,
                          sopts.uncertain_tol, alpha, beta, row_resid, klass, k);
        }
        for (Index r = 0; r < mb; ++r) {
            const detail::SsnBoundRow &br = bound_rows_[static_cast<std::size_t>(r)];
            const bool hinted = use_hint && !start.activity_hint.bounds.empty();
            select_branch(slack_b(r), lambda_b(r), phi_b(r), hinted,
                          hinted && bound_hint_active(start.activity_hint.bounds, br), guarded,
                          sopts.uncertain_tol, alpha, beta, row_resid, klass, mi + r);
        }
        any_klass = true;

        // **THE VERIFICATION ATTEMPT MOVES NO COUNTER BUT factorizations**
        // (banner section 7b). It is not a step, so it cannot be a bulk
        // flip, and its classification is not a partition the iteration
        // ever acted on, so it cannot be an uncertain-set peak. It DOES
        // leave `klass` describing the returned point, which is what the
        // uncertain export then reports.
        if (!verifying) {
            if (!prev_klass.empty() && klass != prev_klass) {
                ++out->counters.ssn_bulk_flips;
            }
            prev_klass = klass;
            Index unc = 0;
            for (const detail::SsnRowClass c : klass) {
                unc += (c == detail::SsnRowClass::kUncertain) ? 1 : 0;
            }
            out->counters.ssn_uncertain_peak = std::max(out->counters.ssn_uncertain_peak, unc);
        }

        // --- diagonal refresh: the ONLY thing that changes in K -----
        double *vals = k_.valuePtr();
        const SpMatRM::StorageIndex *outer = k_.outerIndexPtr();
        for (Index k = 0; k < mi + mb; ++k) {
            const std::size_t kk = static_cast<std::size_t>(k);
            const double alpha_f = std::max(alpha[kk], detail::kSsnAlphaFloor);
            vals[outer[n + me + k]] = -(beta[kk] / alpha_f + eff_mu_);
        }

        // --- THE DEFERRED CERTIFICATION EXIT -----------------------------
        //
        // Everything the verification attempt needs has now been done to K
        // EXCEPT the factorization: the rows are classified at the
        // converged point, sigma has been dropped to the caller's own
        // regularization, and the diagonals hold the values the shipped
        // verification would have factorized. Leaving here rather than
        // falling through is what makes the deferred factorization
        // IDENTICAL to the shipped one rather than merely similar -- the
        // caller's `finish_deferred_certification()` factorizes THIS
        // matrix, unmodified.
        //
        // prox_sigma_ is restored to the ladder's own value for reporting
        // exactly as the two in-loop verification exits below restore it;
        // K stays at the dropped sigma, which is the matrix the pending
        // verdict is about.
        if (verifying && sopts.defer_certification) {
            deferred_pending_ = true;
            deferred_pos_ = n;
            deferred_neg_ = me + mi + mb;
            deferred_attempt_ = it;
            deferred_fb_residual_ = nrm.inf_norm;
            deferred_fb_tol_ = sopts.fb_tol;
            prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
            out->status = QpStatus::kOptimal;
            out->escape_reason = SsnEscape::kNone;
            out->certification_deferred = true;
            break;
        }

        // --- right-hand side ---------------------------------------
        //
        // Skipped on a verification attempt: it takes no step, so the only
        // thing it wants out of K is the inertia.
        if (!verifying) {
            rhs.head(n) = -resid_x;
            rhs.segment(n, me) = -resid_e;
            for (Index k = 0; k < mi + mb; ++k) {
                const std::size_t kk = static_cast<std::size_t>(k);
                const double alpha_f = std::max(alpha[kk], detail::kSsnAlphaFloor);
                rhs(n + me + k) = row_resid[kk] / alpha_f;
            }
        }

        // --- factor and step ---------------------------------------
        Vec dw;
        try {
            const detail::AnalysisDecision analysis = detail::analysis_decision(kkt_, k_);
            if (analysis.needed) {
                ++out->symbolic_analyses;
            }
            detail::factorize_checked(kkt_, k_, analysis);
            ++out->factorizations;
            if (!verifying) {
                dw = detail::solve_vec(kkt_, rhs);
            }
        } catch (const std::exception &e) {
            out->status = QpStatus::kNumericalError;
            out->escape_reason = SsnEscape::kSingular;
            // A throw during a VERIFICATION factorization (banner section
            // 7b) must restore prox_sigma_ to the ladder's own value
            // before falling through to the result assembly, exactly as
            // the two in-loop verification exits do -- otherwise
            // SsnResult::prox_sigma reports the dropped verification value
            // rather than what the ladder was carrying. escape_detail also
            // states that the point was already first-order KKT and the
            // failure was in CERTIFYING it, not in stepping.
            if (verifying) {
                prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                out->escape_detail = fmt::format(
                    "SsnEngine: the point reached at attempt {} satisfies the FB residual "
                    "test (||F||inf = {} <= {}), so it IS a first-order KKT point -- but the "
                    "second-order VERIFICATION factorization (header section 7b) threw "
                    "rather than certifying it: {}",
                    it, nrm.inf_norm, sopts.fb_tol, e.what());
            } else {
                out->escape_detail = e.what();
            }
            break;
        }

        // --- the inertia gate ---------------------------------------
        //
        // K = [H + (delta+sigma) I, A^T; A, -D] with D > 0 has
        //
        //     In(K) = (0, m) + In(H + (delta+sigma) I + A^T D^{-1} A),
        //
        // so its inertia is (n, me+mi+mb) IFF that AUGMENTED block -- the
        // Schur-reduced one, NOT H and not H + (delta+sigma) I alone -- is
        // positive definite (Haynsworth's additivity). That distinction is
        // why the gate certifies a SECOND-ORDER condition ON THE ACTIVE
        // FACE rather than global convexity: an active row's D -> mu
        // supplies a large D^{-1} penalty in a concave direction. kOk on a
        // convex subproblem is therefore a theorem rather than a hope
        // (H PSD makes the augmented block PSD + PSD), and kWrong means
        // exactly one thing: the augmented block is not positive definite,
        // so the step is toward a KKT point that may be a saddle or a
        // maximizer -- what the walk's own inertia ladder exists to
        // prevent (qp_engine.h section 4b), answered the same way.
        //
        // **kSuspect DOES NOT ACT**, matching qp_engine.h exactly: a
        // factorization whose pivots were perturbed reports counts that
        // carry no information, so acting on them would be acting on
        // fabrication.
        if (guarded) {
            // Evidence captured ONCE and consumed by verdict and
            // diagnostics alike, so a message can never describe a
            // different factorization than the verdict judged
            // (docs/retarget-design-sqp.md SS4.2).
            const hven::linear::InertiaEvidence ie = kkt_.factor.inertia();
            const detail::SsnInertia verdict = detail::ssn_inertia_verdict(ie, n, me + mi + mb);
            if (verdict == detail::SsnInertia::kWrong) {
                if (verifying) {
                    // **THE POINT IS FIRST-ORDER KKT AND IS NOT A
                    // MINIMIZER** (banner 7b). No escalation: F is already
                    // inside fb_tol here, so every rung produces the same
                    // (zero) step and would only spend factorizations
                    // reaching the same verdict.
                    //
                    // **AND BOTH VERIFICATION OUTCOMES MUST BREAK**, which
                    // is what makes the sigma drop above safe: falling
                    // through to the escalate-and-retry path below would
                    // escalate FROM the sigma the verification just dropped
                    // to 0, re-enter the convergence test at the same
                    // point, and never terminate.
                    prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                    out->status = QpStatus::kNumericalError;
                    out->escape_reason = SsnEscape::kIndefinite;
                    out->escape_detail = fmt::format(
                        "SsnEngine: the point reached at attempt {} satisfies the FB "
                        "residual test (||F||inf = {} <= {}), so it IS a first-order KKT "
                        "point -- but the KKT factorization THERE reports inertia ({}, {}) "
                        "against the ({}, {}) a positive-definite primal block requires. "
                        "The point is a saddle or a maximizer of this QP, which no "
                        "residual-based test can see, so it is NOT certified",
                        it, nrm.inf_norm, sopts.fb_tol, ie.n_pos, ie.n_neg, n, me + mi + mb);
                    break;
                }
                if (escalate_prox(qp, n, me, mi, mb)) {
                    ++out->counters.ssn_prox_updates;
                    window_damped = true;
                    continue; // same iterate, stiffer Jacobian
                }
                out->status = QpStatus::kNumericalError;
                out->escape_reason = SsnEscape::kSingular;
                out->escape_detail = fmt::format(
                    "SsnEngine: the KKT factorization at attempt {} reports inertia ({}, "
                    "{}) against the ({}, {}) a positive-definite primal block requires, "
                    "and the proximal ladder is at its ceiling (sigma = {}). The Newton "
                    "step is toward a KKT point that need not be a minimizer",
                    it, ie.n_pos, ie.n_neg, n, me + mi + mb, prox_sigma_);
                break;
            }
        }

        // The verification's verdict was kOk or kSuspect -- the only two
        // that reach here -- so the certificate is issued (banner 7b).
        if (verifying) {
            prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
            out->status = QpStatus::kOptimal;
            out->escape_reason = SsnEscape::kNone;
            break;
        }

        if (!dw.allFinite()) {
            out->status = QpStatus::kNumericalError;
            out->escape_reason = SsnEscape::kSingular;
            out->escape_detail =
                fmt::format("SsnEngine: Newton step {} has a non-finite entry -- either the KKT "
                            "matrix is numerically singular at this iterate or the residual fed "
                            "into it was not finite (||F||inf = {})",
                            it, nrm.inf_norm);
            break;
        }

        // --- globalization: the Armijo line search ------------------
        //
        // **ITERATION 0 IS EXEMPT WHEN A HINT GOVERNED IT.** A wrongly
        // hinted first step can RAISE the residual -- the hint is a PDAS
        // step, not an FB Newton step, and nothing promises it descends
        // the FB merit -- so a monotone Armijo rule rejects it, backtracks
        // to the floor, and the solve escapes. A CORRECT hint's step lands
        // at ||F|| ~ 0 and would pass the test anyway, so the exemption
        // gives up no protection on the path it protects.
        //
        // The exemption is as narrow as it can be: iteration 0 AND a hint
        // supplied. An unhinted first step is an ordinary FB Newton step
        // and is line-searched like every other one.
        //
        // THE WATCHDOG REPLACES THE CONDITION, NOT THE MECHANISM. Under
        // the shipped kIterationZeroFree rule `wd_relaxed` is structurally
        // false and `(it == 0 && use_hint)` is the exemption verbatim.
        double step = 1.0;
        bool accepted = false;
        if (!guarded || (watchdog_rule ? wd_relaxed : (it == 0 && use_hint))) {
            accepted = true;
            trial_point(out->x, out->lambda_e, out->lambda_i, lambda_b, dw, 1.0, guarded, n, me, mi,
                        mb, t_x, t_le, t_li, t_lb);
        } else {
            const double m0 = nrm.merit;
            for (;;) {
                trial_point(out->x, out->lambda_e, out->lambda_i, lambda_b, dw, step, guarded, n,
                            me, mi, mb, t_x, t_le, t_li, t_lb);
                const detail::SsnNorms tn = residual(qp, t_x, t_le, t_li, t_lb, resid_x, resid_e,
                                                     slack_i, slack_b, phi_i, phi_b);
                if (std::isfinite(tn.merit) &&
                    tn.merit <= (1.0 - 2.0 * detail::kSsnArmijoSigma * step) * m0) {
                    accepted = true;
                    break;
                }
                if (step * detail::kSsnBacktrackFactor < detail::kSsnMinStep) {
                    break;
                }
                step *= detail::kSsnBacktrackFactor;
                ++out->counters.ssn_backtracks;
            }
        }

        if (!accepted) {
            // **THE DIAGNOSIS OUTRANKS THE REPAIR.** An exhausted Armijo
            // schedule proves the direction produces no descent at all --
            // a strictly stronger "no progress" statement than the stall
            // window's five tepid steps -- so when the DIVERGENCE half of
            // the telemetry is already satisfied, no amount of damping
            // will change the picture. Escalating first would reach the
            // same verdict only by accident: the diagnosis would then
            // depend on how many rungs the ladder happened to have left.
            //
            // **THE EXHAUSTION ROUTE'S SECOND CONJUNCT IS A PER-STEP ONE.**
            // "The duals are 1e4x the start point" alone is satisfied
            // permanently by a feasible QP whose true multipliers merely
            // exceed 1e4, and this route fires BEFORE the ladder gets its
            // chance -- so it also demands the duals are still MOVING, by
            // an order of magnitude across the most recently accepted
            // step. Converging multipliers cannot do that.
            //
            // THE FARKAS GATE GATES THIS ROUTE AS IT GATES THE STANDING
            // ONE, on the increment this route's own growth conjunct is
            // measured over -- the MOST RECENT ACCEPTED STEP. Under the
            // shipped kSymptoms rule `exhaustion_fires` is exactly the
            // shipped conjunction.
            bool exhaustion_fires = guarded && dn >= dual_start * detail::kSsnDualGrowthFactor &&
                                    dn >= dual_prev * detail::kSsnDualStepGrowth;
            double ex_resid = 0.0;
            double ex_gap = 0.0;
            if (exhaustion_fires && farkas_rule &&
                !farkas_certificate(qp, out->lambda_e - prev_le, out->lambda_i - prev_li,
                                    lambda_b - prev_lb, mb, &ex_resid, &ex_gap)) {
                exhaustion_fires = false;
                ++out->farkas_refusals;
            }
            if (exhaustion_fires) {
                out->farkas_fired += farkas_rule ? 1 : 0;
                out->status = QpStatus::kInfeasible;
                out->escape_reason = SsnEscape::kInfeasibleSuspect;
                out->escape_detail = fmt::format(
                    "SsnEngine: infeasibility SUSPECTED (not certified) -- the Armijo "
                    "schedule on 1/2||F||^2 found no descent at all down to step {} at "
                    "attempt {} (merit {}), while the multiplier norm has grown from {} "
                    "to {} (a factor {} above the {}x threshold) and multiplied by {} on "
                    "the most recent accepted step alone (threshold {}x). A direction that "
                    "produces no descent while the duals are still diverging is the "
                    "signature of a QP with no KKT point to converge to",
                    step, it, nrm.merit, dual_start, dn, dn / dual_start,
                    detail::kSsnDualGrowthFactor, dn / dual_prev, detail::kSsnDualStepGrowth);
                if (farkas_rule) {
                    out->escape_detail += fmt::format(
                        ". THE FARKAS CERTIFICATE CONFIRMED IT: the normalized dual "
                        "increment across the most recent accepted step, projected onto the "
                        "sign cone, has relative residual ||A^T y||inf = {} (threshold {}) "
                        "and relative Farkas objective <b, y> = {} (threshold {})",
                        ex_resid, detail::kSsnFarkasResidualTol, ex_gap, -detail::kSsnFarkasGapTol);
                }
                break;
            }
            if (escalate_prox(qp, n, me, mi, mb)) {
                ++out->counters.ssn_prox_updates;
                window_damped = true;
                continue; // same iterate, stiffer Jacobian, fresh schedule
            }
            out->status = QpStatus::kNumericalError;
            out->escape_reason = SsnEscape::kNoContraction;
            out->escape_detail = fmt::format(
                "SsnEngine: the Armijo schedule on 1/2||F||^2 ran down to step {} (floor "
                "{}) at attempt {} without sufficient decrease from merit {}, and the "
                "proximal ladder is at its ceiling (sigma = {}). The Newton direction is "
                "not a descent direction for the FB merit here and damping did not make "
                "it one",
                step, detail::kSsnMinStep, it, nrm.merit, prox_sigma_);
            break;
        }

        out->x = t_x;
        out->lambda_e = t_le;
        out->lambda_i = t_li;
        lambda_b = t_lb;
        ++out->iters;
    }

    out->counters.ssn_iters = out->iters;
    if (out->escape_reason != SsnEscape::kNone) {
        ++out->counters.ssn_escapes;
        // THE ESCAPE-REASON CENSUS, written HERE beside the total it
        // partitions so the two cannot drift: one `switch` over the same
        // value the line above tested, and NO `default`, so a new
        // SsnEscape value raises -Wswitch rather than going silently
        // uncounted. SqpCounters::ssn (sqp_types.h) is what the six fields
        // feed.
        switch (out->escape_reason) {
        case SsnEscape::kBudget:
            ++out->counters.ssn_escape_budget;
            break;
        case SsnEscape::kSingular:
            ++out->counters.ssn_escape_singular;
            break;
        case SsnEscape::kNoContraction:
            ++out->counters.ssn_escape_no_contraction;
            break;
        case SsnEscape::kInfeasibleSuspect:
            ++out->counters.ssn_escape_infeasible_suspect;
            break;
        case SsnEscape::kIndefinite:
            ++out->counters.ssn_escape_indefinite;
            break;
        case SsnEscape::kNone:
            break; // unreachable: guarded by the enclosing test
        }
    }
    out->prox_sigma = prox_sigma_;
    out->z = recombine_bound_multipliers(lambda_b, n);
    // The activity export reads slack_i/slack_b, which must describe the
    // returned point -- and the line search evaluates trial points into
    // those same blocks (see the scratch declaration). One unconditional
    // re-evaluation here makes the export's precondition a local fact
    // rather than an invariant spread over six exit routes.
    // `out->fb_residual` is deliberately NOT overwritten: it is the norm
    // the exit decision was taken on.
    residual(qp, out->x, out->lambda_e, out->lambda_i, lambda_b, resid_x, resid_e, slack_i, slack_b,
             phi_i, phi_b);
    export_activity(out, slack_i, slack_b, lambda_b, n, mi);
    export_uncertain(out, any_klass ? &klass : nullptr, n, mi);
}

bool SsnEngine::finish_deferred_certification(SsnResult *out) {
    if (out == nullptr) {
        throw std::invalid_argument(
            "SsnEngine::finish_deferred_certification: out must not be null");
    }
    if (!deferred_pending_) {
        throw std::invalid_argument(
            "SsnEngine::finish_deferred_certification: no deferred certification is pending "
            "-- the last solve either did not set SsnOptions::defer_certification, did not "
            "reach a certifying exit, or has already been closed");
    }
    deferred_pending_ = false;
    try {
        const detail::AnalysisDecision analysis = detail::analysis_decision(kkt_, k_);
        if (analysis.needed) {
            ++out->symbolic_analyses;
        }
        detail::factorize_checked(kkt_, k_, analysis);
        ++out->factorizations;
    } catch (const std::exception &e) {
        out->status = QpStatus::kNumericalError;
        out->escape_reason = SsnEscape::kSingular;
        out->escape_detail = e.what();
        ++out->counters.ssn_escapes;
        ++out->counters.ssn_escape_singular;
        return false;
    }
    const hven::linear::InertiaEvidence ie = kkt_.factor.inertia();
    if (detail::ssn_inertia_verdict(ie, deferred_pos_, deferred_neg_) ==
        detail::SsnInertia::kWrong) {
        out->status = QpStatus::kNumericalError;
        out->escape_reason = SsnEscape::kIndefinite;
        out->escape_detail = fmt::format(
            "SsnEngine: the point reached at attempt {} satisfies the FB residual test "
            "(||F||inf = {} <= {}), so it IS a first-order KKT point -- but the KKT "
            "factorization THERE reports inertia ({}, {}) against the ({}, {}) a "
            "positive-definite primal block requires. The point is a saddle or a maximizer "
            "of this QP, which no residual-based test can see, so it is NOT certified "
            "(deferred verification, SsnOptions::defer_certification)",
            deferred_attempt_, deferred_fb_residual_, deferred_fb_tol_, ie.n_pos, ie.n_neg,
            deferred_pos_, deferred_neg_);
        ++out->counters.ssn_escapes;
        ++out->counters.ssn_escape_indefinite;
        return false;
    }
    return true;
}

void SsnEngine::discard_deferred_certification() {
    if (!deferred_pending_) {
        throw std::invalid_argument(
            "SsnEngine::discard_deferred_certification: no deferred certification is "
            "pending");
    }
    deferred_pending_ = false;
}

void SsnEngine::validate_overrides(const SolveOverrides &o) {
    if (std::isnan(o.tr_radius) || o.tr_radius < 0.0) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: overrides.tr_radius must be >= 0 (or the +inf "
                        "sentinel), got {}",
                        o.tr_radius));
    }
    if (std::isnan(o.primal_delta)) {
        throw std::invalid_argument("SsnEngine::solve: overrides.primal_delta must not be NaN");
    }
    if (std::isnan(o.dual_mu)) {
        throw std::invalid_argument("SsnEngine::solve: overrides.dual_mu must not be NaN");
    }
}

void SsnEngine::validate_options(const SsnOptions &s) {
    if (s.hard_budget < 0) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: hard_budget must be >= 0, got {}", s.hard_budget));
    }
    if (s.soft_budget < 0) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: soft_budget must be >= 0, got {}", s.soft_budget));
    }
    if (!(s.fb_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: fb_tol must be > 0, got {}", s.fb_tol));
    }
    if (!(s.prox_sigma_init >= 0.0)) {
        throw std::invalid_argument(fmt::format(
            "SsnEngine::solve: prox_sigma_init must be >= 0, got {}", s.prox_sigma_init));
    }
    // The upper bound is 1 EXCLUSIVE, and it is a real boundary:
    // |alpha - beta| = 1 at a strictly active or strictly inactive row, so
    // a threshold of 1 would classify the two PURE states as uncertain and
    // the partition would carry no information at all.
    if (!(s.uncertain_tol >= 0.0) || !(s.uncertain_tol < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SsnEngine::solve: uncertain_tol must be in [0, 1), got {}", s.uncertain_tol));
    }
    // q = 0 would be a watchdog granting no relaxed step at all -- a plain
    // monotone rule wearing the name of a nonmonotone one, whose
    // measurement would be attributed to the wrong mechanism. The shipped
    // rule is reached by hint_rule, not by q = 0.
    if (s.hint_rule == SsnHintRule::kWatchdog && s.watchdog_q < 1) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: watchdog_q must be >= 1 under "
                        "SsnHintRule::kWatchdog (a watchdog with no relaxed step is a "
                        "monotone rule -- select SsnHintRule::kIterationZeroFree for that), "
                        "got {}",
                        s.watchdog_q));
    }
}

void SsnEngine::validate_start(const QpProblem &qp, const SsnStart &start) const {
    check_size(start.x, qp.n(), "x");
    check_size(start.lambda_e, qp.me(), "lambda_e");
    check_size(start.lambda_i, qp.mi(), "lambda_i");
    check_size(start.z, qp.n(), "z");
    check_size(start.slacks, qp.mi(), "slacks");
    check_size(start.prox_center_x, qp.n(), "prox_center_x");
    check_size(start.prox_center_lambda, qp.me() + qp.mi(), "prox_center_lambda");
    const SsnActivityHint &h = start.activity_hint;
    if (!h.ineq.empty() && static_cast<Index>(h.ineq.size()) != qp.mi()) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: activity_hint.ineq has size {}, expected {} "
                        "(= qp.mi()) or 0",
                        h.ineq.size(), qp.mi()));
    }
    if (!h.bounds.empty() && static_cast<Index>(h.bounds.size()) != qp.n()) {
        throw std::invalid_argument(
            fmt::format("SsnEngine::solve: activity_hint.bounds has size {}, expected {} "
                        "(= qp.n()) or 0",
                        h.bounds.size(), qp.n()));
    }
}

void SsnEngine::check_size(const Vec &v, Index want, const char *what) {
    if (v.size() != 0 && v.size() != want) {
        throw std::invalid_argument(fmt::format(
            "SsnEngine::solve: start.{} has size {}, expected {} or 0", what, v.size(), want));
    }
}

Vec SsnEngine::seed_vector(const Vec &v, Index want, const char *) {
    return v.size() == want ? v : Vec::Zero(want);
}

void SsnEngine::build_bound_rows(const QpProblem &qp, const Vec &x0, double tr_radius) {
    bound_rows_.clear();
    const Index n = qp.n();
    // The QP's OWN bounds, kept for the export's structurally-fixed test
    // (l == u), which must not see the trust region.
    real_lower_.assign(static_cast<std::size_t>(n), 0.0);
    real_upper_.assign(static_cast<std::size_t>(n), 0.0);
    for (Index j = 0; j < n; ++j) {
        real_lower_[static_cast<std::size_t>(j)] = qp.lower(j);
        real_upper_[static_cast<std::size_t>(j)] = qp.upper(j);
    }
    const bool has_tr = std::isfinite(tr_radius);
    for (Index j = 0; j < n; ++j) {
        double lo = qp.lower(j);
        double up = qp.upper(j);
        bool lo_from_tr = false;
        bool up_from_tr = false;
        if (has_tr) {
            const double tr_lo = x0(j) - tr_radius;
            const double tr_up = x0(j) + tr_radius;
            if (tr_lo > lo) {
                lo = tr_lo;
                lo_from_tr = true;
            }
            if (tr_up < up) {
                up = tr_up;
                up_from_tr = true;
            }
        }
        if (lo > -detail::kSsnInfBound) {
            bound_rows_.push_back(detail::SsnBoundRow{j, -1.0, -lo, lo_from_tr});
        }
        if (up < detail::kSsnInfBound) {
            bound_rows_.push_back(detail::SsnBoundRow{j, 1.0, up, up_from_tr});
        }
    }
}

bool SsnEngine::bound_hint_active(const std::vector<BoundState> &hint,
                                  const detail::SsnBoundRow &br) {
    const BoundState st = hint[static_cast<std::size_t>(br.var)];
    if (st == BoundState::kFixed) {
        return true;
    }
    return br.sign < 0.0 ? st == BoundState::kAtLower : st == BoundState::kAtUpper;
}

Vec SsnEngine::split_bound_multipliers(const QpProblem &qp, const Vec &z) const {
    const Index n = qp.n();
    for (Index j = 0; j < n; ++j) {
        const double zj = z(j);
        if (zj > 0.0 && !(qp.lower(j) > -detail::kSsnInfBound)) {
            throw std::invalid_argument(fmt::format(
                "SsnEngine::solve: start.z({}) = {} prices variable {}'s LOWER bound, but "
                "that bound is absent (lower = {}, at or beyond the +/-{} infinity "
                "sentinel) -- there is no row for that multiplier",
                j, zj, j, qp.lower(j), detail::kSsnInfBound));
        }
        if (zj < 0.0 && !(qp.upper(j) < detail::kSsnInfBound)) {
            throw std::invalid_argument(fmt::format(
                "SsnEngine::solve: start.z({}) = {} prices variable {}'s UPPER bound, but "
                "that bound is absent (upper = {}, at or beyond the +/-{} infinity "
                "sentinel) -- there is no row for that multiplier",
                j, zj, j, qp.upper(j), detail::kSsnInfBound));
        }
    }
    Vec lb = Vec::Zero(static_cast<Index>(bound_rows_.size()));
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        const detail::SsnBoundRow &br = bound_rows_[r];
        if (br.from_tr) {
            continue; // TR duals are internal; a caller's z never prices one
        }
        const double zj = z(br.var);
        lb(static_cast<Index>(r)) = br.sign < 0.0 ? std::max(zj, 0.0) : std::max(-zj, 0.0);
    }
    return lb;
}

Vec SsnEngine::recombine_bound_multipliers(const Vec &lb, Index n) const {
    Vec z = Vec::Zero(n);
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        const detail::SsnBoundRow &br = bound_rows_[r];
        if (br.from_tr) {
            continue;
        }
        z(br.var) += (br.sign < 0.0 ? 1.0 : -1.0) * lb(static_cast<Index>(r));
    }
    return z;
}

template <typename Emit>
void SsnEngine::for_each_entry(const QpProblem &qp, Index n, Index me, Index mi, Index mb,
                               Emit emit) const {
    const Index eq_off = n;
    const Index ineq_off = n + me;
    const Index bnd_off = n + me + mi;

    // H is already upper-triangular (qp.validate() enforces row <= col).
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            emit(i, it.col(), it.value());
        }
    }
    for (Index i = 0; i < n; ++i) {
        emit(i, i, eff_delta_);
    }
    for (Index r = 0; r < me; ++r) {
        for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
            emit(it.col(), eq_off + r, it.value());
        }
        emit(eq_off + r, eq_off + r, -eff_mu_);
    }
    for (Index k = 0; k < mi; ++k) {
        for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
            emit(it.col(), ineq_off + k, it.value());
        }
        emit(ineq_off + k, ineq_off + k, -1.0); // placeholder
    }
    for (Index r = 0; r < mb; ++r) {
        const detail::SsnBoundRow &br = bound_rows_[static_cast<std::size_t>(r)];
        emit(br.var, bnd_off + r, br.sign);
        emit(bnd_off + r, bnd_off + r, -1.0); // placeholder
    }
}

std::uint64_t SsnEngine::structure_hash(const QpProblem &qp, Index n, Index me, Index mi,
                                        Index mb) const {
    Fnv1a h;
    h.feed_index(n);
    h.feed_index(me);
    h.feed_index(mi);
    h.feed_index(mb);
    feed_pattern(h, qp.H);
    feed_pattern(h, qp.Ae);
    feed_pattern(h, qp.Ai);
    return h.value();
}

bool SsnEngine::bound_rows_match_cached() const {
    if (bound_rows_.size() != structure_bound_key_.size()) {
        return false;
    }
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        const detail::SsnBoundRow &br = bound_rows_[r];
        if (br.var != structure_bound_key_[r].first || br.sign != structure_bound_key_[r].second) {
            return false;
        }
    }
    return true;
}

bool SsnEngine::sync_matrix(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
    dim_ = n + me + mi + mb;
    const std::uint64_t key = structure_hash(qp, n, me, mi, mb);

    if (has_structure_ && key == structure_key_ && bound_rows_match_cached() && k_.rows() == dim_) {
        double *vals = k_.valuePtr();
        std::fill(vals, vals + k_.nonZeros(), 0.0);
        std::size_t t = 0;
        const std::size_t expected = value_pos_.size();
        for_each_entry(qp, n, me, mi, mb, [&](Index, Index, double v) {
            if (t >= expected) {
                throw std::runtime_error(
                    fmt::format("SsnEngine: structure-key collision detected -- the reused pattern "
                                "expects {} entries but this QP emits more; refusing to write past "
                                "the cached position map",
                                expected));
            }
            vals[value_pos_[t++]] += v;
        });
        if (t != expected) {
            throw std::runtime_error(fmt::format(
                "SsnEngine: structure-key collision detected -- the reused pattern expects "
                "{} entries but this QP emitted {}",
                expected, t));
        }
        return false;
    }

    // Structure changed (or this is the first solve): full rebuild.
    has_structure_ = false;
    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(static_cast<std::size_t>(qp.H.nonZeros() + n + qp.Ae.nonZeros() + me +
                                           qp.Ai.nonZeros() + mi + 2 * mb));
    for_each_entry(qp, n, me, mi, mb,
                   [&trips](Index r, Index c, double v) { trips.emplace_back(r, c, v); });

    k_ = SpMatRM(dim_, dim_);
    k_.setFromTriplets(trips.begin(), trips.end());
    k_.makeCompressed();

    // Record where each emitted entry landed. Every row's inner indices are
    // sorted after makeCompressed(), so this is a binary search per entry.
    value_pos_.assign(trips.size(), 0);
    const SpMatRM::StorageIndex *outer = k_.outerIndexPtr();
    const SpMatRM::StorageIndex *inner = k_.innerIndexPtr();
    for (std::size_t t = 0; t < trips.size(); ++t) {
        const Index row = trips[t].row();
        const Index col = trips[t].col();
        const SpMatRM::StorageIndex *begin = inner + outer[row];
        const SpMatRM::StorageIndex *end = inner + outer[row + 1];
        const SpMatRM::StorageIndex *hit =
            std::lower_bound(begin, end, static_cast<SpMatRM::StorageIndex>(col));
        if (hit == end || *hit != static_cast<SpMatRM::StorageIndex>(col)) {
            // Unreachable for a matrix just built from these very triplets;
            // checked rather than asserted because a Release build compiles
            // an assert out entirely and a wrong position would corrupt K
            // silently on every later reuse.
            throw std::runtime_error(fmt::format(
                "SsnEngine: internal error -- entry ({}, {}) is missing from the matrix "
                "just assembled from it",
                row, col));
        }
        value_pos_[t] = static_cast<std::size_t>(hit - inner);
    }

    structure_key_ = key;
    structure_bound_key_.clear();
    structure_bound_key_.reserve(bound_rows_.size());
    for (const detail::SsnBoundRow &br : bound_rows_) {
        structure_bound_key_.emplace_back(br.var, br.sign);
    }
    has_structure_ = true;
    return true;
}

void SsnEngine::export_activity(SsnResult *out, const Vec &slack_i, const Vec &slack_b,
                                const Vec &lambda_b, Index n, Index mi) const {
    out->ineq_active.assign(static_cast<std::size_t>(mi), false);
    for (Index k = 0; k < mi; ++k) {
        out->ineq_active[static_cast<std::size_t>(k)] = out->lambda_i(k) > slack_i(k);
    }
    out->bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
    out->tr_active.assign(static_cast<std::size_t>(n), false);
    out->tr_violation = 0.0;
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        const detail::SsnBoundRow &br = bound_rows_[r];
        const Index rr = static_cast<Index>(r);
        // The TR EXIT CONTRACT, measured on the same slacks the partition
        // is read from: a TR row's slack is negative exactly when the
        // returned point is outside the radius on that side, and by
        // exactly how much.
        if (br.from_tr && slack_b(rr) < 0.0) {
            out->tr_violation = std::max(out->tr_violation, -slack_b(rr));
        }
        if (!(lambda_b(rr) > slack_b(rr))) {
            continue;
        }
        // **THE TR/REAL SPLIT, qp_problem.h's contract verbatim**: a
        // variable held by a TR-tight effective bound is reported in
        // tr_active and reports kFree in bound_state -- which is a
        // REAL-BOUND-ONLY view, so a caller cannot mistake a radius for a
        // constraint. Its z is 0 for the same reason (see
        // recombine_bound_multipliers). A driver detects a binding radius
        // by reading tr_active, never z or bound_state.
        if (br.from_tr) {
            out->tr_active[static_cast<std::size_t>(br.var)] = true;
            continue;
        }
        BoundState &st = out->bound_state[static_cast<std::size_t>(br.var)];
        const BoundState here = br.sign < 0.0 ? BoundState::kAtLower : BoundState::kAtUpper;
        st = (st == BoundState::kFree) ? here : BoundState::kFixed;
    }
    // A STRUCTURALLY FIXED VARIABLE (l == u) READS kFixed, whichever of
    // its two rows won the partition. The partition rule alone reports
    // kAtUpper there, which is a correct reading of the partition but NOT
    // the project's convention: kkt_assembly.h treats kFixed as a
    // structural fact about the variable, and the walk reports kFixed
    // here. An export that disagreed with the walk on a whole variable
    // class would trap the driver's re-ingest, so the structural fact wins
    // over the partition. TR rows are excluded, since a radius does not
    // make a variable fixed.
    for (Index j = 0; j < n; ++j) {
        const std::size_t jj = static_cast<std::size_t>(j);
        if (out->bound_state[jj] != BoundState::kFree && real_lower_[jj] == real_upper_[jj]) {
            out->bound_state[jj] = BoundState::kFixed;
        }
    }
}

void SsnEngine::export_uncertain(SsnResult *out, const std::vector<detail::SsnRowClass> *klass,
                                 Index n, Index mi) const {
    out->ineq_uncertain.assign(static_cast<std::size_t>(mi), false);
    out->bound_uncertain.assign(static_cast<std::size_t>(n), false);
    if (klass == nullptr) {
        return;
    }
    for (Index k = 0; k < mi; ++k) {
        out->ineq_uncertain[static_cast<std::size_t>(k)] =
            (*klass)[static_cast<std::size_t>(k)] == detail::SsnRowClass::kUncertain;
    }
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        if ((*klass)[static_cast<std::size_t>(mi) + r] == detail::SsnRowClass::kUncertain) {
            out->bound_uncertain[static_cast<std::size_t>(bound_rows_[r].var)] = true;
        }
    }
}

double SsnEngine::dual_norm(const Vec &le, const Vec &li, const Vec &lb) {
    double m = 0.0;
    if (le.size() > 0) {
        m = std::max(m, le.cwiseAbs().maxCoeff());
    }
    if (li.size() > 0) {
        m = std::max(m, li.cwiseAbs().maxCoeff());
    }
    if (lb.size() > 0) {
        m = std::max(m, lb.cwiseAbs().maxCoeff());
    }
    return m;
}

void SsnEngine::trial_point(const Vec &x, const Vec &le, const Vec &li, const Vec &lb,
                            const Vec &dw, double step, bool project, Index n, Index me, Index mi,
                            Index mb, Vec &t_x, Vec &t_le, Vec &t_li, Vec &t_lb) {
    t_x = x + step * dw.head(n);
    t_le = le + step * dw.segment(n, me);
    t_li = li + step * dw.segment(n + me, mi);
    t_lb = lb + step * dw.segment(n + me + mi, mb);
    if (project) {
        if (mi > 0) {
            t_li = t_li.cwiseMax(0.0);
        }
        if (mb > 0) {
            t_lb = t_lb.cwiseMax(0.0);
        }
    }
}

void SsnEngine::set_prox_sigma(double sigma, const QpProblem &qp, Index n, Index me, Index mi,
                               Index mb) {
    prox_sigma_ = sigma;
    eff_delta_ = base_delta_ + sigma;
    eff_mu_ = base_mu_ + sigma;
    (void)sync_matrix(qp, n, me, mi, mb);
}

void SsnEngine::apply_sigma(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
    set_prox_sigma(std::max(ladder_sigma_, lm_sigma_), qp, n, me, mi, mb);
}

bool SsnEngine::escalate_prox(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
    if (ladder_sigma_ >= detail::kSsnProxMax * (1.0 - detail::kSsnProxCapSlack)) {
        return false;
    }
    double next = ladder_sigma_ <= 0.0
                      ? detail::kSsnProxInit
                      : std::min(ladder_sigma_ * detail::kSsnProxGrowth, detail::kSsnProxMax);
    // The TOP rung is the cap EXACTLY, for the same reason the guard above
    // carries a slack: repeated multiplication reaches 999999.9999999998,
    // and a ceiling a caller can read back (SsnResult::prox_sigma) should
    // be the documented 1e6 rather than the accumulated rounding of it.
    if (next >= detail::kSsnProxMax * (1.0 - detail::kSsnProxCapSlack)) {
        next = detail::kSsnProxMax;
    }
    ladder_sigma_ = next;
    apply_sigma(qp, n, me, mi, mb);
    return true;
}

bool SsnEngine::farkas_certificate(const QpProblem &qp, const Vec &dle, const Vec &dli,
                                   const Vec &dlb, Index mb, double *resid_out,
                                   double *gap_out) const {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    // --- project onto the sign cone, and normalize --------------------
    Vec ye = dle;
    Vec yi = mi > 0 ? Vec(dli.cwiseMax(0.0)) : Vec(0);
    Vec yb = mb > 0 ? Vec(dlb.cwiseMax(0.0)) : Vec(0);
    const double scale = dual_norm(ye, yi, yb);
    if (!(scale > 0.0) || !std::isfinite(scale)) {
        return false;
    }
    ye /= scale;
    if (mi > 0) {
        yi /= scale;
    }
    if (mb > 0) {
        yb /= scale;
    }

    // --- the two matvecs, accumulated in one pass ---------------------
    Vec r = Vec::Zero(n);
    Vec r_abs = Vec::Zero(n);
    double gap = 0.0;
    double gap_abs = 0.0;
    for (Index k = 0; k < me; ++k) {
        const double y = ye(k);
        for (SpMatRM::InnerIterator it(qp.Ae, k); it; ++it) {
            r(it.col()) += it.value() * y;
            r_abs(it.col()) += std::abs(it.value() * y);
        }
        gap += qp.be(k) * y;
        gap_abs += std::abs(qp.be(k) * y);
    }
    for (Index k = 0; k < mi; ++k) {
        const double y = yi(k);
        for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
            r(it.col()) += it.value() * y;
            r_abs(it.col()) += std::abs(it.value() * y);
        }
        gap += qp.bi(k) * y;
        gap_abs += std::abs(qp.bi(k) * y);
    }
    for (std::size_t s = 0; s < bound_rows_.size(); ++s) {
        const detail::SsnBoundRow &br = bound_rows_[s];
        const double y = yb(static_cast<Index>(s));
        r(br.var) += br.sign * y;
        r_abs(br.var) += std::abs(br.sign * y);
        gap += br.rhs * y;
        gap_abs += std::abs(br.rhs * y);
    }

    const double rel_resid =
        n > 0 ? r.cwiseAbs().maxCoeff() / std::max(1.0, r_abs.maxCoeff()) : 0.0;
    const double rel_gap = gap / std::max(1.0, gap_abs);
    *resid_out = rel_resid;
    *gap_out = rel_gap;
    if (!std::isfinite(rel_resid) || !std::isfinite(rel_gap)) {
        return false;
    }
    return rel_resid <= detail::kSsnFarkasResidualTol && rel_gap <= -detail::kSsnFarkasGapTol;
}

detail::SsnNorms SsnEngine::residual(const QpProblem &qp, const Vec &x, const Vec &le,
                                     const Vec &li, const Vec &lb, Vec &resid_x, Vec &resid_e,
                                     Vec &slack_i, Vec &slack_b, Vec &phi_i, Vec &phi_b) const {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    resid_x = qp.g;
    for (Index i = 0; i < n; ++i) { // H stores the upper triangle only
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            const Index j = it.col();
            resid_x(i) += it.value() * x(j);
            if (j != i) {
                resid_x(j) += it.value() * x(i);
            }
        }
    }
    for (Index r = 0; r < me; ++r) {
        double ax = 0.0;
        for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
            ax += it.value() * x(it.col());
            resid_x(it.col()) += it.value() * le(r);
        }
        resid_e(r) = ax - qp.be(r);
    }
    for (Index k = 0; k < mi; ++k) {
        double ax = 0.0;
        for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
            ax += it.value() * x(it.col());
            resid_x(it.col()) += it.value() * li(k);
        }
        slack_i(k) = qp.bi(k) - ax;
        phi_i(k) = detail::ssn_fb(slack_i(k), li(k));
    }
    for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
        const detail::SsnBoundRow &br = bound_rows_[r];
        const Index rr = static_cast<Index>(r);
        resid_x(br.var) += br.sign * lb(rr);
        slack_b(rr) = br.rhs - br.sign * x(br.var);
        phi_b(rr) = detail::ssn_fb(slack_b(rr), lb(rr));
    }

    detail::SsnNorms out;
    double sq = 0.0;
    auto acc = [&out, &sq](const Vec &v) {
        if (v.size() > 0) {
            out.inf_norm = std::max(out.inf_norm, v.cwiseAbs().maxCoeff());
            sq += v.squaredNorm();
        }
    };
    acc(resid_x);
    acc(resid_e);
    acc(phi_i);
    acc(phi_b);
    out.merit = 0.5 * sq;
    return out;
}

void SsnEngine::select_branch(double s, double lam, double phi, bool hinted, bool hint_active,
                              bool guarded, double tau, std::vector<double> &alpha,
                              std::vector<double> &beta, std::vector<double> &row_resid,
                              std::vector<detail::SsnRowClass> &klass, Index slot) {
    const std::size_t k = static_cast<std::size_t>(slot);
    if (hinted) {
        if (hint_active) {
            alpha[k] = 1.0;
            beta[k] = 0.0;
            row_resid[k] = s; // drive the slack to zero
        } else {
            alpha[k] = detail::kSsnAlphaFloor;
            beta[k] = 1.0;
            row_resid[k] = lam; // drive the multiplier to zero
        }
        klass[k] = hint_active ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
        return;
    }
    const double rho = std::sqrt(s * s + lam * lam);
    if (rho < detail::kSsnRhoFloor) {
        alpha[k] = detail::kSsnDegenerateFbDeriv;
        beta[k] = detail::kSsnDegenerateFbDeriv;
    } else {
        alpha[k] = 1.0 - s / rho;
        beta[k] = 1.0 - lam / rho;
    }
    row_resid[k] = phi;

    if (!guarded) {
        klass[k] =
            alpha[k] > beta[k] ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
        return;
    }

    const double margin = alpha[k] - beta[k];
    const double mag = std::abs(margin);
    const bool was_uncertain = klass[k] == detail::SsnRowClass::kUncertain;
    const double gate = was_uncertain ? tau * detail::kSsnUncertainLeaveRatio : tau;
    // tau == 0 disables the set OUTRIGHT, including for the exact tie that
    // "mag <= 0" would otherwise capture -- the field's documented meaning
    // is "no uncertain set", not "an uncertain set of measure zero".
    if (tau > 0.0 && mag <= gate) {
        klass[k] = detail::SsnRowClass::kUncertain;
    } else {
        klass[k] = margin > 0.0 ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
    }
    if (klass[k] == detail::SsnRowClass::kUncertain) {
        alpha[k] = detail::kSsnDegenerateFbDeriv;
        beta[k] = detail::kSsnDegenerateFbDeriv;
    }
}

} // namespace hven::solvers
