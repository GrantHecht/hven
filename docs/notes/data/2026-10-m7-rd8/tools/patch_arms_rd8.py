#!/usr/bin/env python3
"""RD8 SCRATCH ARMS (NOT FOR MERGE), applied to a COPY of src/qp/qp_engine.cpp. Env-gated; unset == production.
 RD8_RBUDGET=N    start-repair pin budget override
 RD8_RBATCH=1     start repair probes after pin counts 1,2,4,8,... (doubling) and at the end; no release pass
 RD8_FACE_LEAK=k  CYC-C1: in-face allowance k * dual_mu * |lambda_j| on the row tests of ride_stays_in_working_set
usage: patch_arms_rd8.py <src/qp/qp_engine.cpp>"""
import sys
QE = sys.argv[1]
t = open(QE).read()
if "RD8_RBATCH" in t:
    raise SystemExit("already patched")


def sub1(a, b, w):
    global t
    n = t.count(a)
    if n != 1:
        raise SystemExit("ANCHOR %s x%d" % (w, n))
    t = t.replace(a, b)


sub1("#include <utility>\n", r'''#include <utility>
#include <cstdlib>
namespace hven::solvers::rd8 {
inline double env_d(const char *k, double dflt) {
    const char *v = std::getenv(k);
    return v ? std::atof(v) : dflt;
}
inline long env_l(const char *k, long dflt) {
    const char *v = std::getenv(k);
    return v ? std::atol(v) : dflt;
}
inline bool rbatch() { static const bool b = env_l("RD8_RBATCH", 0) != 0; return b; }
inline long rbatch_mode() { static const long b = env_l("RD8_RBATCH", 0); return b; }
inline bool meet() { static const bool b = env_l("RD8_MEET", 0) != 0; return b; }
inline double face_leak() { static const double v = env_d("RD8_FACE_LEAK", 0.0); return v; }
inline thread_local const Eigen::VectorXd *lam_e = nullptr;
inline thread_local const Eigen::VectorXd *lam_i = nullptr;
inline thread_local double mu = 0.0;
} // namespace hven::solvers::rd8
''', "includes")
sub1("        const bool first_pass = start_pass;\n        start_pass = false;\n",
     "        const bool first_pass = start_pass;\n        start_pass = false;\n"
     "        if (first_pass && verdict == detail::InertiaVerdict::kWrong) rd8_start_wrong = true;\n", "firstpass")
sub1("    bool start_pass = true;\n", "    bool start_pass = true;\n    bool rd8_start_wrong = false;\n    bool rd8_met_done = false;\n", "startpass")
sub1("        price(qp, ws, eqp, lambda_e, lambda_i, z);\n", '''        if (rd8::meet() && rd8_start_wrong && !rd8_met_done && qp.me() > 0) {
            // RD8 ARM: THE FACE-MEETING STEP -- once, on a solve whose start face had the wrong
            // inertia: the minimum-norm correction onto the equality + working rows (pins held),
            // ratio-tested at the unit step, no add; the pass is spent.
            rd8_met_done = true;
            const Vec re = qp.be - qp.Ae * x;
            if (re.lpNorm<Eigen::Infinity>() > 0.0) {
                QpProblem qpi = qp;
                SpMatRM I(qp.n(), qp.n());
                I.setIdentity();
                qpi.H = I;
                QpOptions oi = eff_opts;
                oi.primal_delta = 0.0;
                const KktAssembly ai = assemble_kkt(qpi, ws, oi);
                const Index nf = static_cast<Index>(ai.free_of_full.size());
                const std::vector<Index> &aw = ws.active_ineq();
                Vec rhs = Vec::Zero(ai.K.rows());
                for (Index r = 0; r < qp.me(); ++r) rhs(nf + r) = re(r);
                for (std::size_t k = 0; k < aw.size(); ++k)
                    rhs(nf + qp.me() + static_cast<Index>(k)) = qp.bi(aw[k]) - Aix(aw[k]);
                detail::KktFactor fi(threads_);
                detail::factorize_checked(fi, ai.K);
                ++counters.factorizations;
                const Vec y = detail::solve_vec(fi, rhs);
                Vec d = Vec::Zero(qp.n());
                for (Index k = 0; k < nf; ++k) d(ai.free_of_full[static_cast<std::size_t>(k)]) = y(k);
                QpCounters scratch;
                BlockKind bk = BlockKind::kNone;
                Index bi_ = -1;
                BoundState bs_ = BoundState::kFree;
                const double a = ratio_test(qp, ws, x, Aix, d, bk, bi_, bs_, /*cap_at_unit=*/true, scratch);
                x += a * d;
                std::fprintf(stderr, "RD8_MEET r=%.3e alpha=%.3e dinf=%.3e after=%.3e\\n", re.lpNorm<Eigen::Infinity>(), a,
                             d.lpNorm<Eigen::Infinity>(), (qp.be - qp.Ae * x).lpNorm<Eigen::Infinity>());
                pt_clear(seen);
                refresh_shifts(qp, x, ai_row_norm1, lambda_i, ws, Aix, shift, counters, seen, eff_opts);
                continue;
            }
        }
        price(qp, ws, eqp, lambda_e, lambda_i, z);
        rd8::lam_e = &lambda_e;
        rd8::lam_i = &lambda_i;
        rd8::mu = eff_opts.dual_mu;
''', "price")
sub1("    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {\n",
     "    rd8::lam_e = nullptr;\n    rd8::lam_i = nullptr;\n"
     "    for (Index iter = 0; !crossed_bounds && iter < eff_max_iter; ++iter) {\n", "loop")
sub1("    const Index pin_budget = detail::repair_pin_budget(n);\n",
     "    const Index pin_budget = rd8::env_l(\"RD8_RBUDGET\", detail::repair_pin_budget(n));\n    std::size_t rd8_next = 1;\n",
     "budget")
sub1('''        pinned.push_back(i);
        verdict = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
        if (verdict != detail::InertiaVerdict::kWrong) {
            break; // repaired, or no longer verifiable
        }
    }
''', '''        pinned.push_back(i);
        if (rd8::rbatch() && pinned.size() < rd8_next) {
            continue;
        }
        rd8_next = 2 * pinned.size();
        verdict = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
        if (verdict != detail::InertiaVerdict::kWrong) {
            break; // repaired, or no longer verifiable
        }
    }
    if (rd8::rbatch() && !pinned.empty() && verdict == detail::InertiaVerdict::kWrong &&
        2 * pinned.size() != rd8_next) {
        verdict = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
    }
    if (rd8::rbatch_mode() == 2 && pinned.size() > 2 && verdict != detail::InertiaVerdict::kWrong) {
        // bisect the prefix length between the last kWrong probe and this one
        std::vector<BoundState> pin_state;
        std::vector<double> pin_x;
        for (const Index i : pinned) {
            pin_state.push_back(ws.bound_state()[static_cast<std::size_t>(i)]);
            pin_x.push_back(x(i));
        }
        const auto set_prefix = [&](std::size_t m) {
            for (std::size_t k = 0; k < pinned.size(); ++k) {
                const auto si = static_cast<std::size_t>(pinned[k]);
                ws.bound_state()[si] = k < m ? pin_state[k] : saved_state[si];
                x(pinned[k]) = k < m ? pin_x[k] : saved_x(pinned[k]);
            }
        };
        std::size_t hi = pinned.size();
        std::size_t lo = std::max<std::size_t>(1, rd8_next / 4); // the last doubling probe (kWrong)
        if (lo >= hi) lo = hi - 1;
        detail::InertiaVerdict hi_verdict = verdict;
        int probes = 0;
        while (hi - lo > 1) {
            const std::size_t mid = lo + (hi - lo) / 2;
            set_prefix(mid);
            const detail::InertiaVerdict v = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
            ++probes;
            if (v == detail::InertiaVerdict::kWrong) {
                lo = mid;
            } else {
                hi = mid;
                hi_verdict = v;
            }
        }
        set_prefix(hi);
        pinned.resize(hi);
        verdict = probe_inertia(qp, ws, x, kkt, face, border, counters, opts);
        std::fprintf(stderr, "RD8_BISECT hi=%zu probes=%d hi_verdict=%d final=%d\\n", hi, probes, (int)hi_verdict, (int)verdict);
    }
    std::fprintf(stderr, "RD8_REPAIR pinned=%zu verdict=%d budget=%ld\\n", pinned.size(), (int)verdict, (long)pin_budget);
''', "batch")
sub1('''    for (auto it = pinned.rbegin(); it != pinned.rend(); ++it) {''',
     '''    for (auto it = pinned.rbegin(); !rd8::rbatch() && it != pinned.rend(); ++it) {''', "release")
sub1('''    const double pn = p.norm();
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
    }''', '''    const double pn = p.norm();
    const double kap = rd8::face_leak();
    const auto leak_e = [&](Index j) {
        return (kap > 0.0 && rd8::lam_e && rd8::lam_e->size() == qp.me()) ? kap * rd8::mu * std::abs((*rd8::lam_e)(j)) : 0.0;
    };
    const auto leak_i = [&](Index j) {
        return (kap > 0.0 && rd8::lam_i && rd8::lam_i->size() == qp.mi()) ? kap * rd8::mu * std::abs((*rd8::lam_i)(j)) : 0.0;
    };
    for (Index j = 0; j < qp.me(); ++j) {
        if (std::abs(qp.Ae.row(j).dot(p)) > detail::kRideNullspaceTol * qp.Ae.row(j).norm() * pn + leak_e(j)) {
            return false;
        }
    }
    for (const Index row : ws.active_ineq()) {
        if (std::abs(qp.Ai.row(row).dot(p)) >
            detail::kRideNullspaceTol * qp.Ai.row(row).norm() * pn + leak_i(row)) {
            return false;
        }
    }''', "leak")
open(QE, "w").write(t)
print("patched arms", QE)
