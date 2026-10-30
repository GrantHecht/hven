#!/usr/bin/env python3
"""E2-D4 candidate rules as env toggles (HVEN_D4_FIX=comma list), applied after instrument_d4.py.

  f5      (class B, D5) a kWrong inertia verdict is re-read on a LOCAL eliminated factor with dual_mu = 0
          (delta kept); if the mu-free read is kOk the pass proceeds as kOk (start repair skipped / the
          terminal saddle exit not taken). One extra factorization per kWrong pass.
  f5s     (with f5) a mu-free read of kSuspect (unverifiable: a zero reduced eigenvalue) also replaces kWrong, so the
          existing suspect-stall gate (free-block stationarity) decides the terminal point.
  structfirst (class F1) every blocker the numerical test calls independent is checked for an exact structural dependency first (any rho).
  greys   (class F1) a blocker the numerical test calls INDEPENDENT with rho < kDepUndecidable (the grey
          zone) is checked for an exact structural dependency; found => the exact dependent test.
  greyd   (with greys) a grey-zone blocker with no structural dependency is DEPENDENT, with the test's own v.
  greyu   (with greys) a grey-zone blocker with no structural dependency is treated as UNDECIDABLE
          (border path: retreat; eliminated path: the existing structural/deferral handling).
  unlock  (class A) when no admissible member exists on a zero-motion step, members locked only because
          they ENTERED at this point (never released here: no kPtExempt) become admissible, same
          min-ratio rule, ties to the lowest index; counted.
  coefrho (class F1) an exchange member is admissible only if its scaled coefficient exceeds max(kDepCoeffTol, 10*rho)*max:
          a coefficient below the test's own residual is noise.
  unlockall (with unlock) re-entered (exempt) members too, as the last resort; the zero-motion cap bounds it.
  screenadmit a blocker proved dependent only by the count (rho > kDepTol), with no admissible member and no verified
          certificate, is admitted as a deferred add (pre-E2 behaviour; the next pass re-checks).
  leakpass (class A/F, N2 variant) a DEPENDENT blocker whose rate along p is within the leak bound
          |a'p| <= 10 * dual_mu * ||lambda_W||_inf * ||a|| + eps-floor is passed over as a non-blocker
          (its true rate on null(A_W) is zero) without the within-tolerance violation test.
"""
import sys
p = sys.argv[1] + "/src/qp/qp_engine.cpp"
s = open(p).read()


def sub(old, new, count=1):
    global s
    n = s.count(old)
    if n != count:
        sys.exit(f"anchor count {n} != {count}: {old[:90]!r}")
    s = s.replace(old, new)


sub('''inline long solve_seq = 0;''', '''inline long solve_seq = 0;
inline long n_f5_reads = 0, n_f5_flips = 0, n_grey_struct = 0, n_grey_undec = 0, n_unlock = 0, n_leakpass = 0;''')

# ---- f5: mu-free re-read of a kWrong verdict, right after the pass's EQP
sub('''        if (d4::trace()) {
            const Index d4_pins = n - ws.num_free();''', '''        if (d4::fix("f5") && verdict == detail::InertiaVerdict::kWrong) {
            detail::KktFactor k2(threads_);
            EliminatedFace f2;
            detail::InertiaVerdict v2 = detail::InertiaVerdict::kSuspect;
            QpOptions o2 = eff_opts;
            o2.dual_mu = 0.0;
            try {
                (void)eliminated_candidate(qp, ws, k2, f2, counters, v2, o2);
            } catch (...) {
                v2 = detail::InertiaVerdict::kSuspect;
            }
            ++d4::n_f5_reads;
            if (d4::trace())
                std::fprintf(stderr, "D4T it=%ld F5 reread verdict=%d (mu-free)\\n", (long)iter, (int)v2);
            if (v2 == detail::InertiaVerdict::kOk ||
                (v2 == detail::InertiaVerdict::kSuspect && d4::fix("f5s"))) {
                verdict = v2;
                ++d4::n_f5_flips;
            }
        }
        if (d4::trace()) {
            const Index d4_pins = n - ws.num_free();''')

# ---- grey zone at the add site
sub('''                detail::DepTest t =
                    test_candidate(qp, ws, pass_view, is_ineq, block_idx, counters, eff_opts);
''', '''                detail::DepTest t =
                    test_candidate(qp, ws, pass_view, is_ineq, block_idx, counters, eff_opts);
                if (t.verdict == detail::DepVerdict::kIndependent &&
                    ((d4::fix("greys") && t.rho < detail::kDepUndecidable) || d4::fix("structfirst"))) {
                    const detail::StructuralDependency sdg =
                        detail::structural_dependency(qp, ws, is_ineq, block_idx);
                    if (sdg.found) {
                        t = sdg.test;
                        ++d4::n_grey_struct;
                        if (d4::trace()) std::fprintf(stderr, "D4T it=%ld GREY structural exact\\n", (long)iter);
                    } else if (d4::fix("greyd") && t.rho < detail::kDepUndecidable) {
                        t.verdict = detail::DepVerdict::kDependent;
                        ++d4::n_grey_undec;
                        if (d4::trace()) std::fprintf(stderr, "D4T it=%ld GREY -> dependent\n", (long)iter);
                    } else if (d4::fix("greyu") && t.rho < detail::kDepUndecidable) {
                        t.verdict = detail::DepVerdict::kUndecidable;
                        ++d4::n_grey_undec;
                        if (d4::trace()) std::fprintf(stderr, "D4T it=%ld GREY -> undecidable\\n", (long)iter);
                    }
                }
''')

# ---- leak pass-over: a dependent blocker blocking only by the regularization leak
sub('''                if (!t.dependent() && !known_dependent) {
                    break; // the ordinary add
                }
''', '''                if (!t.dependent() && !known_dependent) {
                    break; // the ordinary add
                }
                if (d4::fix("leakpass") && t.verdict == detail::DepVerdict::kDependent) {
                    double lam = 0.0;
                    for (const Index r : ws.active_ineq()) lam = std::max(lam, std::abs(lambda_i(r)));
                    for (Index i = 0; i < n; ++i)
                        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) lam = std::max(lam, std::abs(z(i)));
                    if (me > 0) lam = std::max(lam, lambda_e.lpNorm<Eigen::Infinity>());
                    const double anorm = is_ineq ? qp.Ai.row(block_idx).norm() : 1.0;
                    const double rate = std::abs(is_ineq ? qp.Ai.row(block_idx).dot(p) : p(block_idx));
                    const double leak = 10.0 * eff_opts.dual_mu * lam * anorm +
                                        64.0 * std::numeric_limits<double>::epsilon() * anorm * p.norm();
                    if (rate <= leak) {
                        ++d4::n_leakpass;
                        if (d4::trace()) std::fprintf(stderr, "D4T it=%ld LEAKPASS rate=%.3e leak=%.3e\\n", (long)iter, rate, leak);
                        passed.emplace_back(is_ineq, block_idx);
                        BlockKind k2 = BlockKind::kNone;
                        Index i2 = -1;
                        BoundState s2 = BoundState::kFree;
                        const double a2 = ratio_test_excluding(qp, ws, x, Aix, p, passed, k2, i2, s2);
                        kind = k2;
                        block_idx = i2;
                        block_state = s2;
                        alpha = a2;
                        continue;
                    }
                }
''')

# ---- unlock: a second scan in select_exchange
sub('''    for (std::size_t k = 0; k < aw.size(); ++k) {
        const auto kk = static_cast<Index>(k);
        const Index row = aw[k];
        const bool removable = moves || (pt_flags(seen, true, row) & kPtEntered) == 0;
        consider(true, row, pk.v_w(kk), pk.v_w(kk) * row_norm(kk), lambda_i(row), removable,
                 shift(row) > 0.0);
    }
    for (Index i = 0; i < n; ++i) {
        const BoundState st = bs[static_cast<std::size_t>(i)];
        if (st == BoundState::kFree || st == BoundState::kFixed) {
            continue; // kFixed is free-signed: mandatory
        }
        const double m = st == BoundState::kAtLower ? z(i) : -z(i);
        const bool removable = moves || (pt_flags(seen, false, i) & kPtEntered) == 0;
        consider(false, i, pk.v_pin(i), pk.v_pin(i), m, removable);
    }
''', '''    const auto scan = [&](bool relax) {
        const auto rem = [&](bool row, Index idx) {
            const std::uint8_t f = pt_flags(seen, row, idx);
            if (moves || (f & kPtEntered) == 0) return true;
            return relax && (f & kPtExempt) == 0;
        };
        for (std::size_t k = 0; k < aw.size(); ++k) {
            const auto kk = static_cast<Index>(k);
            const Index row = aw[k];
            consider(true, row, pk.v_w(kk), pk.v_w(kk) * row_norm(kk), lambda_i(row), rem(true, row),
                     shift(row) > 0.0);
        }
        for (Index i = 0; i < n; ++i) {
            const BoundState st = bs[static_cast<std::size_t>(i)];
            if (st == BoundState::kFree || st == BoundState::kFixed) {
                continue; // kFixed is free-signed: mandatory
            }
            const double m = st == BoundState::kAtLower ? z(i) : -z(i);
            consider(false, i, pk.v_pin(i), pk.v_pin(i), m, rem(false, i));
        }
    };
    scan(false);
    if (!pk.found && !sh.found && !moves && d4::fix("unlock")) {
        const bool c1 = pk.cond1;
        pk.cond1 = true;
        scan(true);
        if (pk.found || sh.found) {
            ++d4::n_unlock;
        } else {
            pk.cond1 = c1 && pk.cond1;
        }
    }
''')

sub('''    if (d4::trace())
        std::fprintf(stderr, "D4T solve=%ld end status=%d minors=%ld facts=%ld\\n", d4_solve, (int)status,
                     (long)counters.minor_iters, (long)counters.factorizations);''', '''    if (d4::trace())
        std::fprintf(stderr, "D4T solve=%ld end status=%d minors=%ld facts=%ld | f5 reads=%ld flips=%ld grey_struct=%ld grey_undec=%ld unlock=%ld leakpass=%ld\\n", d4_solve, (int)status,
                     (long)counters.minor_iters, (long)counters.factorizations, d4::n_f5_reads, d4::n_f5_flips, d4::n_grey_struct,
                     d4::n_grey_undec, d4::n_unlock, d4::n_leakpass);''')
sub('''                    // The design's candidate (cond1 and a structural
                    // violation) that failed verification.
                    if (xpick.cond1 && violation_is_structural(xpick.g, o.scale, 0.0, eff_opts)) {''', '''                    // E2-D4 screenadmit: dependence proved by the count alone (the test's own
                    // residual says independent), no admissible member, no verified certificate.
                    if (d4::fix("screenadmit") && t.screen && t.rho > detail::kDepTol) {
                        ++d4::n_screenadmit;
                        if (d4::trace()) std::fprintf(stderr, "D4T it=%ld SCREENADMIT rho=%.3e\\n", (long)iter, t.rho);
                        ++counters.dep_deferred_adds;
                        recheck_pending = true;
                        xpick = ExchangePick{};
                        break;
                    }
                    // The design's candidate (cond1 and a structural
                    // violation) that failed verification.
                    if (xpick.cond1 && violation_is_structural(xpick.g, o.scale, 0.0, eff_opts)) {''')
s = s.replace('n_unlock = 0, n_leakpass = 0;', 'n_unlock = 0, n_leakpass = 0, n_screenadmit = 0;')
s = s.replace('unlock=%ld leakpass=%ld\\n", d4_solve', 'unlock=%ld leakpass=%ld screenadmit=%ld\\n", d4_solve')
s = s.replace('d4::n_grey_undec, d4::n_unlock, d4::n_leakpass);', 'd4::n_grey_undec, d4::n_unlock, d4::n_leakpass, d4::n_screenadmit);')
sub('''    const double tol = detail::kDepCoeffTol * vmax;
    pk.cond1 = true;''', '''    const double tol = (d4::fix("coefrho") && std::isfinite(t.rho))
                           ? std::max(detail::kDepCoeffTol, 10.0 * t.rho) * vmax
                           : detail::kDepCoeffTol * vmax;
    pk.cond1 = true;''')
sub('''            return relax && (f & kPtExempt) == 0;''', '''            return relax && ((f & kPtExempt) == 0 || d4::fix("unlockall"));''')
open(p, "w").write(s)
print("fixes_d4: ok")
