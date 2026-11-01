#!/usr/bin/env python3
"""E2-D4 TEMPORARY instrumentation of src/qp/qp_engine.cpp (scratch only; env-gated).

HVEN_D4_QPDUMP=dir   dump every QP solve that ends kNumericalError or kMaxIter (and every solve
                     when HVEN_D4_DUMPALL is set) as dir/qp<seq>.txt, with a D4DUMP line on stderr.
HVEN_D4_TRACE=1      per-pass walk trace on stderr (D4T lines).
HVEN_D4_FIX=<list>   candidate-rule toggles, comma-separated (see the d4::fix() uses).
Applied after E2-D3's instrument.py (the driver's D3 lines give each dump its context)."""
import sys
p = sys.argv[1] + "/src/qp/qp_engine.cpp"
s = open(p).read()


def sub(old, new, count=1):
    global s
    n = s.count(old)
    if n != count:
        print(f"SKIPPED (anchor count {n}): {old[:90]!r}"); return
    s = s.replace(old, new)


sub('''#include <hven/detail/qp/qp_engine.h>

namespace hven::solvers {
''', '''#include <hven/detail/qp/qp_engine.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

// ===== E2-D4 TEMPORARY DIAGNOSTIC INSTRUMENTATION (scratch only) =====
namespace hven::solvers::d4 {
inline bool trace() { static const bool b = std::getenv("HVEN_D4_TRACE") != nullptr; return b; }
inline const char *dump_dir() { static const char *d = std::getenv("HVEN_D4_QPDUMP"); return d; }
inline bool dump_all() { static const bool b = std::getenv("HVEN_D4_DUMPALL") != nullptr; return b; }
inline bool fix(const char *name) {
    static const char *f = std::getenv("HVEN_D4_FIX");
    if (f == nullptr) return false;
    const std::string all = std::string(",") + f + ",";
    return all.find(std::string(",") + name + ",") != std::string::npos;
}
inline long solve_seq = 0;
inline long dump_seq = 0;
inline void dvec(std::ofstream &o, const char *name, const Eigen::VectorXd &v) {
    o << name << " " << v.size() << "\\n";
    char b[64];
    for (Eigen::Index i = 0; i < v.size(); ++i) { std::snprintf(b, sizeof b, "%.17g\\n", v(i)); o << b; }
}
template <class M> inline void dsp(std::ofstream &o, const char *name, const M &A) {
    o << name << " " << A.rows() << " " << A.cols() << " " << A.nonZeros() << "\\n";
    char b[96];
    for (int k = 0; k < A.outerSize(); ++k)
        for (typename M::InnerIterator it(A, k); it; ++it) {
            std::snprintf(b, sizeof b, "%ld %ld %.17g\\n", (long)it.row(), (long)it.col(), it.value());
            o << b;
        }
}
inline void divec(std::ofstream &o, const char *name, const std::vector<long> &v) {
    o << name << " " << v.size() << "\\n";
    for (long x : v) o << x << "\\n";
}
inline const char *st(BoundState b) {
    return b == BoundState::kFree ? "F" : b == BoundState::kAtLower ? "L" : b == BoundState::kAtUpper ? "U" : "X";
}
} // namespace hven::solvers::d4

namespace hven::solvers {
''')

# ---- per-solve counter + the pass-top trace line
sub('''    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {
        ++counters.minor_iters;
''', '''    const long d4_solve = d4::solve_seq++;
    if (d4::trace()) {
        std::fprintf(stderr, "D4T solve=%ld begin n=%ld me=%ld mi=%ld warm=%d tr=%.6g mu=%.3g delta=%.3g indep=%d algebra=%d cap=%ld k0_reused=%d\\n",
                     d4_solve, (long)n, (long)me, (long)mi, (int)warm, eff_opts.tr_radius, eff_opts.dual_mu, eff_opts.primal_delta,
                     (int)eff_opts.independent_working_set, (int)opts_.ws_algebra, (long)eff_max_iter, (int)counters.k0_reused);
    }
    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {
        ++counters.minor_iters;
''')

sub('''        const EqpResult eqp =
            eliminated_only
                ? eliminated_candidate(qp, ws, kkt, face, counters, verdict, eff_opts)
                : eqp_candidate(qp, ws, kkt, face, *border_, counters, verdict, eff_opts);
''', '''        const EqpResult eqp =
            eliminated_only
                ? eliminated_candidate(qp, ws, kkt, face, counters, verdict, eff_opts)
                : eqp_candidate(qp, ws, kkt, face, *border_, counters, verdict, eff_opts);
        if (d4::trace()) {
            const Index d4_pins = n - ws.num_free();
            const Vec d4_p = eqp.x - x;
            const double d4_f = qp.g.dot(x) + 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x);
            std::fprintf(stderr, "D4T it=%ld PASS rows=%zu pins=%ld cnt-n=%ld verdict=%d elim_only=%d pinf=%.4e f=%.12g ptevents=%ld\\n",
                         (long)iter, ws.active_ineq().size(), (long)d4_pins,
                         (long)(me + (Index)ws.active_ineq().size() + d4_pins - n), (int)verdict, (int)eliminated_only,
                         d4_p.lpNorm<Eigen::Infinity>(), d4_f, (long)seen.pt_events);
        }
''')

# ---- detector removal
sub('''                    ++(entry_check_pending ? counters.entry_dependency_removals
                                           : counters.dep_recheck_removals);
''', '''                    ++(entry_check_pending ? counters.entry_dependency_removals
                                           : counters.dep_recheck_removals);
                    if (d4::trace())
                        std::fprintf(stderr, "D4T it=%ld DETECT_REMOVE %s kind=%d idx=%ld found=%d trim=%d null_res=%.3e over=%d\\n",
                                     (long)iter, entry_check_pending ? "entry" : "recheck", (int)removed.kind, (long)removed.idx,
                                     (int)det.found, (int)trim.found, det.null_residual, (int)over);
''')

sub('''        if (first_pass && verdict == detail::InertiaVerdict::kWrong &&
            repair_temporary_vertex(qp, ws, x, kkt, face, *border_, counters, eff_opts)) {
''', '''        if (d4::trace() && first_pass && verdict == detail::InertiaVerdict::kWrong)
            std::fprintf(stderr, "D4T it=%ld START_KWRONG attempting repair\\n", (long)iter);
        if (first_pass && verdict == detail::InertiaVerdict::kWrong &&
            repair_temporary_vertex(qp, ws, x, kkt, face, *border_, counters, eff_opts)) {
            if (d4::trace())
                std::fprintf(stderr, "D4T it=%ld START_REPAIR ok pins_now=%ld\\n", (long)iter, (long)(n - ws.num_free()));
''')

# ---- KKT branch: drop and terminal classification
sub('''            if (dropped_one && eff_opts.independent_working_set) {
                pt_release(seen, last_drop.is_ineq, last_drop.idx);
            }
''', '''            if (dropped_one && eff_opts.independent_working_set) {
                pt_release(seen, last_drop.is_ineq, last_drop.idx);
            }
            if (d4::trace()) {
                if (dropped_one) {
                    const double m = last_drop.is_ineq ? lambda_i(last_drop.idx) : z(last_drop.idx);
                    std::fprintf(stderr, "D4T it=%ld DROP %s %ld from=%s mult=%.4e exempt_worst=%.3e\\n", (long)iter,
                                 last_drop.is_ineq ? "row" : "pin", (long)last_drop.idx, d4::st(last_drop.from), m, exempt_worst);
                } else {
                    std::fprintf(stderr, "D4T it=%ld NODROP exempt_worst=%.3e\\n", (long)iter, exempt_worst);
                }
            }
''')

sub('''                if (worst > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {
''', '''                if (d4::trace())
                    std::fprintf(stderr, "D4T it=%ld TERMINAL worst=%.3e refined=%d runaway=%d verdict=%d probe_drop_made=%d\\n", (long)iter,
                                 worst, (int)refined, (int)(worst > 0.0 ? 0 : is_runaway(qp, x, ws, eff_opts)), (int)verdict, (int)probe_drop_made);
                if (worst > 0.0) {
                    status = QpStatus::kInfeasible;
                } else if (is_runaway(qp, x, ws, eff_opts)) {
''')

# ---- ride outcome
sub('''                if (outcome == RideOutcome::kUnbounded) {
                    // Nothing blocks: genuinely unbounded.''', '''                if (d4::trace())
                    std::fprintf(stderr, "D4T it=%ld RIDE outcome=%d curvature=%.3e\\n", (long)iter, (int)outcome, curvature);
                if (outcome == RideOutcome::kUnbounded) {
                    // Nothing blocks: genuinely unbounded.''')

# ---- add-site loop: each test
sub('''                const bool known_dependent =
                    !moves && (pt_flags(seen, is_ineq, block_idx) & kPtKnownDependent) != 0;
''', '''                const bool known_dependent =
                    !moves && (pt_flags(seen, is_ineq, block_idx) & kPtKnownDependent) != 0;
                if (d4::trace()) {
                    const double rate = is_ineq ? qp.Ai.row(block_idx).dot(p) : p(block_idx);
                    const double anorm = is_ineq ? qp.Ai.row(block_idx).norm() : 1.0;
                    std::fprintf(stderr, "D4T it=%ld BLOCK %s %ld side=%s alpha=%.4e moves=%d rate=%.4e rate_rel=%.3e test=%d rho=%.3e screen=%d path=%s known_dep=%d ptflags=%d\\n",
                                 (long)iter, is_ineq ? "row" : "pin", (long)block_idx, d4::st(block_state), alpha, (int)moves, rate,
                                 rate / (anorm * std::max(p.norm(), 1e-300)), (int)t.verdict, t.rho, (int)t.screen,
                                 pass_view.schur != nullptr ? "border" : pass_view.eliminated != nullptr ? "elim" : "none", (int)known_dependent,
                                 (int)pt_flags(seen, is_ineq, block_idx));
                }
''')

sub('''                    xpick = select_exchange(qp, ws, shift, seen, t, is_ineq, block_idx, block_state,
                                            lambda_e, lambda_i, z, moves);
                    if (xpick.found) {
                        break; // the exchange
                    }
''', '''                    xpick = select_exchange(qp, ws, shift, seen, t, is_ineq, block_idx, block_state,
                                            lambda_e, lambda_i, z, moves);
                    if (d4::trace()) {
                        std::fprintf(stderr, "D4T it=%ld XPICK found=%d j=%s%ld sigma=%.4e cond1=%d g=%.4e shifted=%d\\n", (long)iter,
                                     (int)xpick.found, xpick.j_is_ineq ? "row" : "pin", (long)xpick.j, xpick.sigma, (int)xpick.cond1, xpick.g,
                                     (int)xpick.j_shifted);
                        // every member with a positive scaled coefficient, and whether it is locked
                        double vmax = 0.0;
                        for (Index r = 0; r < me; ++r) vmax = std::max(vmax, std::abs(xpick.v_e(r)) * qp.Ae.row(r).norm());
                        const auto &aw = ws.active_ineq();
                        for (std::size_t k = 0; k < aw.size(); ++k)
                            vmax = std::max(vmax, std::abs(xpick.v_w((Index)k)) * qp.Ai.row(aw[k]).norm());
                        for (Index i = 0; i < n; ++i) vmax = std::max(vmax, std::abs(xpick.v_pin(i)));
                        const double tol = detail::kDepCoeffTol * vmax;
                        int shown = 0;
                        for (std::size_t k = 0; k < aw.size() && shown < 40; ++k) {
                            const double sc = xpick.v_w((Index)k) * qp.Ai.row(aw[k]).norm();
                            if (std::abs(sc) > tol) {
                                ++shown;
                                std::fprintf(stderr, "D4T it=%ld   MEMBER row %ld v=%.4e scaled=%.4e m=%.4e ptflags=%d shift=%.3e\\n", (long)iter,
                                             (long)aw[k], xpick.v_w((Index)k), sc, lambda_i(aw[k]), (int)pt_flags(seen, true, aw[k]), shift(aw[k]));
                            }
                        }
                        for (Index i = 0; i < n && shown < 80; ++i) {
                            const BoundState bst = ws.bound_state()[static_cast<std::size_t>(i)];
                            if (bst == BoundState::kFree) continue;
                            if (std::abs(xpick.v_pin(i)) > tol) {
                                ++shown;
                                const double m = bst == BoundState::kAtLower ? z(i) : -z(i);
                                std::fprintf(stderr, "D4T it=%ld   MEMBER pin %ld %s v=%.4e m=%.4e ptflags=%d\\n", (long)iter, (long)i, d4::st(bst),
                                             xpick.v_pin(i), m, (int)pt_flags(seen, false, i));
                            }
                        }
                        int eqs = 0;
                        for (Index r = 0; r < me; ++r)
                            if (std::abs(xpick.v_e(r)) * qp.Ae.row(r).norm() > tol) ++eqs;
                        std::fprintf(stderr, "D4T it=%ld   MEMBERS eq_support=%d vmax=%.3e\\n", (long)iter, eqs, vmax);
                    }
                    if (xpick.found) {
                        break; // the exchange
                    }
''')

sub('''                    if (verify_farkas(qp, ws, xpick, is_ineq, block_idx, o.scale, eff_opts, cert)) {
                        farkas = std::move(cert);
                        add_exit = AddExit::kInfeasible;
                        break;
                    }
''', '''                    const bool d4_ver = verify_farkas(qp, ws, xpick, is_ineq, block_idx, o.scale, eff_opts, cert);
                    if (d4::trace()) {
                        QpFarkasCertificate c2;
                        // re-run only to read implied/limit of a failed certificate
                        (void)c2;
                        std::fprintf(stderr, "D4T it=%ld FARKAS verified=%d cond1=%d g=%.4e structural_g=%d\\n", (long)iter, (int)d4_ver,
                                     (int)xpick.cond1, xpick.g, (int)violation_is_structural(xpick.g, o.scale, 0.0, eff_opts));
                    }
                    if (d4_ver) {
                        farkas = std::move(cert);
                        add_exit = AddExit::kInfeasible;
                        break;
                    }
''')

sub('''                if (!within) {
                    add_exit = AddExit::kTerminate;
                    break;
                }
''', '''                if (d4::trace()) {
                    double worstv = 0.0;
                    for (const auto &[pi, pidx] : passed) {
                        const BoundState side = pi ? BoundState::kFree : p(pidx) > 0.0 ? BoundState::kAtUpper : BoundState::kAtLower;
                        const Oriented po = oriented(pi, pidx, side);
                        worstv = std::max(worstv, std::max(0.0, po.cx - po.d + a2 * po.cp) / (eff_opts.feas_tol * po.scale));
                    }
                    std::fprintf(stderr, "D4T it=%ld PASSOVER within=%d next=%s%ld a2=%.4e worst_viol/tol=%.3e\\n", (long)iter, (int)within,
                                 k2 == BlockKind::kIneq ? "row" : k2 == BlockKind::kBound ? "pin" : "none", (long)i2, a2, worstv);
                }
                if (!within) {
                    add_exit = AddExit::kTerminate;
                    break;
                }
''')

sub('''                eliminated_only = true;
                ++counters.dep_border_retreats;
''', '''                eliminated_only = true;
                ++counters.dep_border_retreats;
                if (d4::trace())
                    std::fprintf(stderr, "D4T it=%ld BORDER_RETREAT undecidable=%d add_exit=%d\\n", (long)iter, (int)undecidable_retreat, (int)add_exit);
''')

sub('''            if (add_exit != AddExit::kNone) {
                // The iterate the walk stands on: no step past the blocker.
''', '''            if (d4::trace() && add_exit != AddExit::kNone)
                std::fprintf(stderr, "D4T it=%ld ADD_EXIT %d (1=infeasible 2=rejected 3=terminate)\\n", (long)iter, (int)add_exit);
            if (add_exit != AddExit::kNone) {
                // The iterate the walk stands on: no step past the blocker.
''')

# ---- the step taken (R2: re-anchored onto NC1's ride_blocked branch)
sub('''        if (ride_blocked) {
            x += alpha * sdir; // a ride step: no landing point, uncapped
''', '''        if (d4::trace()) {
            const Vec &d4_dir = ride_blocked ? sdir : p;
            std::fprintf(stderr, "D4T it=%ld STEP kind=%s %ld side=%s alpha=%.4e dx=%.4e ride_blocked=%d exch=%d%s%ld sigma=%.3e\\n", (long)iter,
                         kind == BlockKind::kIneq ? "row" : kind == BlockKind::kBound ? "pin" : "none", (long)block_idx, d4::st(block_state),
                         alpha, alpha * d4_dir.lpNorm<Eigen::Infinity>(), (int)ride_blocked, (int)xpick.found, xpick.j_is_ineq ? " out=row" : " out=pin",
                         (long)xpick.j, xpick.sigma);
        }
        if (ride_blocked) {
            x += alpha * sdir; // a ride step: no landing point, uncapped
''')

# ---- the dump, just before the ledger record
sub('''    // Emit a record to the ledger if attached.
    if (ledger_ != nullptr) {
        SolveRecord rec;''', '''    if (d4::trace())
        std::fprintf(stderr, "D4T solve=%ld end status=%d minors=%ld facts=%ld\\n", d4_solve, (int)status,
                     (long)counters.minor_iters, (long)counters.factorizations);
    if (const char *dd = d4::dump_dir();
        dd != nullptr && (d4::dump_all() || status == QpStatus::kNumericalError || status == QpStatus::kMaxIter)) {
        const long seq = d4::dump_seq++;
        std::ofstream o(std::string(dd) + "/qp" + std::to_string(seq) + ".txt");
        o << "D4QP 1\\n";
        o << "dims " << n << " " << me << " " << mi << "\\n";
        char b[512];
        std::snprintf(b, sizeof b, "opts %.17g %.17g %.17g %.17g %ld %ld %.17g %d %.17g %d %d\\n", opts_.primal_delta, opts_.dual_mu,
                      opts_.feas_tol, opts_.opt_tol, (long)opts_.max_iter, (long)opts_.schur_cap, opts_.schur_cond_max, (int)opts_.ws_algebra,
                      opts_.tr_radius, (int)opts_.independent_working_set, threads_);
        o << b;
        std::snprintf(b, sizeof b, "overrides %.17g %.17g %.17g\\n", overrides.tr_radius, overrides.primal_delta, overrides.dual_mu);
        o << b;
        o << "warm " << (int)warm << " seed " << (seed != nullptr ? 1 : 0) << " hot " << (hot != nullptr ? 1 : 0) << " k0_reused "
          << (int)counters.k0_reused << "\\n";
        std::snprintf(b, sizeof b, "result status=%d minors=%ld facts=%ld blk_term=%ld cert=%ld cert_rej=%ld zmcap=%ld zmexh=%ld retreats=%ld exch=%ld deferred=%ld undec=%ld maxiter=%ld\\n",
                      (int)status, (long)counters.minor_iters, (long)counters.factorizations, (long)counters.dep_blocker_terminations,
                      (long)counters.dep_infeasible_certificates, (long)counters.dep_certificate_rejections, (long)counters.zero_motion_cap_hits,
                      (long)counters.zero_motion_exhausted, (long)counters.dep_border_retreats, (long)counters.ws_exchanges,
                      (long)counters.dep_deferred_adds, (long)counters.dep_undecidable, (long)eff_max_iter);
        o << b;
        d4::dsp(o, "H", qp_in.H);
        d4::dvec(o, "g", qp_in.g);
        d4::dsp(o, "Ae", qp_in.Ae);
        d4::dvec(o, "be", qp_in.be);
        d4::dsp(o, "Ai", qp_in.Ai);
        d4::dvec(o, "bi", qp_in.bi);
        d4::dvec(o, "lower", qp_in.lower);
        d4::dvec(o, "upper", qp_in.upper);
        if (seed != nullptr) {
            d4::dvec(o, "seed_x", seed->x);
            std::vector<long> sb, sa;
            for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
            for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
            d4::divec(o, "seed_bound_state", sb);
            d4::divec(o, "seed_ineq_active", sa);
        }
        d4::dvec(o, "x_final", out.x);
        std::vector<long> fb, fa;
        for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
        for (Index r : ws.active_ineq()) fa.push_back((long)r);
        d4::divec(o, "final_bound_state", fb);
        d4::divec(o, "final_active_ineq", fa);
        std::fprintf(stderr, "D4DUMP seq=%ld solve=%ld status=%d n=%ld me=%ld mi=%ld warm=%d tr=%.6g minors=%ld facts=%ld blk_term=%ld cert_rej=%ld zmcap=%ld retreats=%ld k0_reused=%d\\n",
                     seq, d4_solve, (int)status, (long)n, (long)me, (long)mi, (int)warm, eff_opts.tr_radius, (long)counters.minor_iters,
                     (long)counters.factorizations, (long)counters.dep_blocker_terminations, (long)counters.dep_certificate_rejections,
                     (long)counters.zero_motion_cap_hits, (long)counters.dep_border_retreats, (int)counters.k0_reused);
    }

    // Emit a record to the ledger if attached.
    if (ledger_ != nullptr) {
        SolveRecord rec;''')

open(p, "w").write(s)
print("instrument_d4: ok")
