p='src/qp/walk/walk_engine.cpp'
s=open(p).read()
def rep(old,new,count=1):
    global s
    assert s.count(old)==count, (old[:80], s.count(old))
    s=s.replace(old,new)

rep('''namespace hven {

void QpEngine::attach_ledger(''','''namespace hven {

// P1SCRATCH (p1a-diag, never shipped): the diagnosis's variant switches, read once per run().
namespace p1s {
enum Mode { kZero = 0, kLive = 1, kEvery = 2, kFace = 3, kSeedOnly = 4, kTrig = 5 };
struct Env {
    int mode = kZero;
    bool keep = false;      // a refused update keeps the previous centre (else reset to 0)
    bool seed = false;      // pass-0 centre from the seed's multipliers
    bool norung = false;    // release rung + WA-1 footprint ride/rung/decline retired
    bool nosafe = false;    // no safeguard
    bool facefull = false;  // kFace also commits at a full unblocked step
    bool strictsafe = false;// safeguard reads mu*|lambda - c| (else mu*|lambda|)
    double ymax = 0.0;      // centre bound (0 = none)
    int trace = 0;
};
inline Env read_env() {
    Env e;
    const auto get = [](const char *k) { return std::getenv(k); };
    if (get("HVEN_P1_GLOBAL") != nullptr) {
        e.mode = kLive;
    }
    if (const char *m = get("HVEN_P1_MODE")) {
        const std::string v(m);
        e.mode = v == "live" ? kLive : v == "every" ? kEvery : v == "face" ? kFace
               : v == "seedonly" ? kSeedOnly : v == "trig" ? kTrig : kZero;
    }
    e.keep = get("HVEN_P1_KEEP") != nullptr;
    e.seed = get("HVEN_P1_SEED") != nullptr;
    e.norung = get("HVEN_P1_NORUNG") != nullptr;
    e.nosafe = get("HVEN_P1_NOSAFE") != nullptr;
    e.facefull = get("HVEN_P1_FACEFULL") != nullptr;
    e.strictsafe = get("HVEN_P1_STRICTSAFE") != nullptr;
    if (const char *y = get("HVEN_P1_YMAX")) {
        e.ymax = std::atof(y);
    }
    if (const char *t = get("HVEN_P1_TRACE")) {
        e.trace = std::atoi(t);
    }
    return e;
}
} // namespace p1s

void QpEngine::attach_ledger(''')

rep('''    bool dual_centring = std::getenv("HVEN_P1_GLOBAL") != nullptr; // P1SCRATCH
    EqpDualCentre dual_k{};
''','''    const p1s::Env p1 = p1s::read_env(); // P1SCRATCH
    bool dual_centring = p1.mode == p1s::kLive;
    EqpDualCentre dual_k{};
    // Copy-centre state (kEvery / kFace / kSeedOnly).
    Vec ce = Vec::Zero(me);
    Vec ci = Vec::Zero(mi);
    Vec cz = Vec::Zero(n);
    bool c_on = false;
    bool c_pending_ok = false;
    long p1_upd = 0, p1_ref = 0, p1_reset = 0, p1_readm = 0, p1_relrung = 0;
    double p1_dlam_max = 0.0, p1_lam_max = 0.0;
    const bool p1_copy = p1.mode == p1s::kEvery || p1.mode == p1s::kFace || p1.mode == p1s::kSeedOnly;
    if (p1.seed && seed != nullptr && seed->lambda_e.size() == me && seed->lambda_i.size() == mi) {
        ce = seed->lambda_e;
        for (Index r = 0; r < mi; ++r) {
            const bool on = static_cast<Index>(seed->ineq_active.size()) == mi &&
                            seed->ineq_active[static_cast<std::size_t>(r)];
            ci(r) = on ? seed->lambda_i(r) : 0.0;
        }
        if (seed->z.size() == n) {
            cz = seed->z;
        }
        c_on = p1_copy && ce.allFinite() && ci.allFinite() && cz.allFinite();
    }
    const auto p1_commit = [&](const Vec &le, const Vec &li, const Vec &zz) {
        ce = le;
        ci = li;
        cz = zz;
        c_on = true;
        ++p1_upd;
    };
    const auto p1_refuse = [&] {
        ++p1_ref;
        if (!p1.keep && c_on) {
            c_on = false;
            ce.setZero();
            ci.setZero();
            cz.setZero();
            ++p1_reset;
        }
    };
''')

rep('''        const EqpResult eqp =
            eliminated_only
                ? eliminated_candidate(qp, ws, x, dual_k, kkt, face, counters, verdict, eff_opts)''','''        if (p1_copy) {
            const std::vector<Index> &aw_c = ws.active_ineq();
            std::size_t kk = 0;
            for (Index r = 0; r < mi; ++r) {
                while (kk < aw_c.size() && aw_c[kk] < r) {
                    ++kk;
                }
                if (!(kk < aw_c.size() && aw_c[kk] == r)) {
                    ci(r) = 0.0;
                }
            }
            for (Index i = 0; i < n; ++i) {
                if (ws.bound_state()[static_cast<std::size_t>(i)] == BoundState::kFree) {
                    cz(i) = 0.0;
                }
            }
            dual_k = c_on ? EqpDualCentre{&ce, &ci, &cz} : EqpDualCentre{};
        }
        const EqpResult eqp =
            eliminated_only
                ? eliminated_candidate(qp, ws, x, dual_k, kkt, face, counters, verdict, eff_opts)''')

rep('''        price(qp, ws, eqp, lambda_e, lambda_i, z);
        dual_k = dual_centring && eqp_answer_meets_working_rows(qp, ws, eqp, ai_row_norm1,
                                                                ae_row_norm1, eff_opts)
                     ? EqpDualCentre{&lambda_e, &lambda_i, &z}
                     : EqpDualCentre{};
''','''        double p1_dlam = 0.0;
        if (p1.trace > 0) {
            const auto &aw_t = ws.active_ineq();
            for (Index j = 0; j < me; ++j) {
                const double c = dual_k.lambda_e != nullptr ? (*dual_k.lambda_e)(j) : 0.0;
                p1_dlam = std::max(p1_dlam, std::abs(eqp.lambda_e(j) - c));
            }
            for (std::size_t k = 0; k < aw_t.size(); ++k) {
                const double c = dual_k.lambda_i != nullptr ? (*dual_k.lambda_i)(aw_t[k]) : 0.0;
                p1_dlam = std::max(p1_dlam, std::abs(eqp.lambda_w(static_cast<Index>(k)) - c));
            }
            p1_dlam_max = std::max(p1_dlam_max, p1_dlam);
        }
        price(qp, ws, eqp, lambda_e, lambda_i, z);
        bool p1_ok = true;
        if (p1.mode != p1s::kZero) {
            if (!p1.nosafe) {
                if (p1.strictsafe) {
                    p1_ok = eqp.x.allFinite();
                    const double xmag = eqp.x.lpNorm<Eigen::Infinity>();
                    const auto cval = [&](const Vec *v, Index j) { return v != nullptr ? (*v)(j) : 0.0; };
                    if (me > 0 && p1_ok) {
                        const Vec r = qp.Ae * eqp.x - qp.be;
                        for (Index j = 0; j < me && p1_ok; ++j) {
                            const double rs = std::max({1.0, std::abs(qp.be(j)), ae_row_norm1(j) * xmag});
                            p1_ok = std::abs(r(j)) <= row_tolerance(rs, eqp.lambda_e(j) - cval(dual_k.lambda_e, j), eff_opts);
                        }
                    }
                    const auto &aw_s = ws.active_ineq();
                    for (std::size_t k = 0; k < aw_s.size() && p1_ok; ++k) {
                        const Index j = aw_s[k];
                        const double rs = std::max({1.0, std::abs(qp.bi(j)), ai_row_norm1(j) * xmag});
                        p1_ok = std::abs(qp.Ai.row(j).dot(eqp.x) - qp.bi(j)) <=
                                row_tolerance(rs, eqp.lambda_w(static_cast<Index>(k)) - cval(dual_k.lambda_i, j), eff_opts);
                    }
                } else {
                    p1_ok = eqp_answer_meets_working_rows(qp, ws, eqp, ai_row_norm1, ae_row_norm1,
                                                          eff_opts);
                }
            }
            const double lam_inf = std::max({me > 0 ? lambda_e.lpNorm<Eigen::Infinity>() : 0.0,
                                             mi > 0 ? lambda_i.lpNorm<Eigen::Infinity>() : 0.0,
                                             z.lpNorm<Eigen::Infinity>()});
            p1_lam_max = std::max(p1_lam_max, lam_inf);
            if (p1.ymax > 0.0 && !(lam_inf <= p1.ymax)) {
                p1_ok = false;
            }
        }
        if (p1.mode == p1s::kLive || p1.mode == p1s::kTrig) {
            dual_k = dual_centring && p1_ok ? EqpDualCentre{&lambda_e, &lambda_i, &z}
                                            : EqpDualCentre{};
            if (dual_centring) {
                p1_ok ? (void)++p1_upd : (void)++p1_ref;
            }
        } else if (p1.mode == p1s::kEvery) {
            if (p1_ok) {
                p1_commit(lambda_e, lambda_i, z);
            } else {
                p1_refuse();
            }
        }
        c_pending_ok = p1_ok;
        if (p1.trace > 1) {
            std::fprintf(stderr, "[p1p] it=%ld nw=%zu nf=%ld ok=%d con=%d dlam=%.3e lam=%.3e mu=%.1e\\n",
                         static_cast<long>(iter), ws.active_ineq().size(),
                         static_cast<long>(ws.num_free()), p1_ok ? 1 : 0,
                         (dual_k.lambda_e != nullptr || dual_k.lambda_i != nullptr) ? 1 : 0, p1_dlam,
                         std::max(me > 0 ? lambda_e.lpNorm<Eigen::Infinity>() : 0.0,
                                  mi > 0 ? lambda_i.lpNorm<Eigen::Infinity>() : 0.0),
                         eff_opts.dual_regularization);
        }
''')

rep('''        if (zero_motion || stationary_on_ws) {
            // A KKT point of the current working set: snap onto it (zero motion
            // only -- see above), then let the multipliers decide.
            if (zero_motion) {''','''        if (zero_motion || stationary_on_ws) {
            // A KKT point of the current working set: snap onto it (zero motion
            // only -- see above), then let the multipliers decide.
            if (p1.mode == p1s::kFace) {
                if (c_pending_ok) {
                    p1_commit(lambda_e, lambda_i, z);
                } else {
                    p1_refuse();
                }
            }
            if (p1.trace > 1) {
                std::fprintf(stderr, "[p1p]   stationary zm=%d\\n", zero_motion ? 1 : 0);
            }
            if (zero_motion) {''')

rep('''                } else if (eff_opts.dual_regularization > 0.0 &&
                           verdict != detail::InertiaVerdict::kSuspect) {''','''                } else if (!p1.norung && eff_opts.dual_regularization > 0.0 &&
                           verdict != detail::InertiaVerdict::kSuspect) {''')

rep('''        const bool readmitted = released_row_out && in_working(ws, dropped.idx);
        if (readmitted && !dual_centring) {
            dual_centring = true;
            ++counters.dual_centre_arms;
        } else if (readmitted && std::getenv("HVEN_P1_NORUNG") == nullptr && // P1SCRATCH
                   counters.dual_regularization_decreases < detail::kMaxDualRegDecreases &&
                   eff_opts.dual_regularization > 0.0) {
            ++counters.dual_regularization_decreases;''','''        const bool readmitted = released_row_out && in_working(ws, dropped.idx);
        if (readmitted) {
            ++p1_readm;
            if (p1.trace > 1) {
                std::fprintf(stderr, "[p1p]   readmitted row %ld\\n", static_cast<long>(dropped.idx));
            }
        }
        if (p1.mode == p1s::kFace && p1.facefull && kind == BlockKind::kNone && !ride_blocked &&
            alpha >= 1.0) {
            if (c_pending_ok) {
                p1_commit(lambda_e, lambda_i, z);
            } else {
                p1_refuse();
            }
        }
        if (readmitted && p1.mode == p1s::kTrig && !dual_centring) {
            dual_centring = true;
            ++counters.dual_centre_arms;
        } else if (readmitted && !p1.norung &&
                   counters.dual_regularization_decreases < detail::kMaxDualRegDecreases &&
                   eff_opts.dual_regularization > 0.0) {
            ++p1_relrung;
            ++counters.dual_regularization_decreases;''')

rep('''    QpSolution out;
    out.status = status;
    out.x = std::move(x);''','''    if (p1.trace > 0) {
        std::fprintf(stderr,
                     "[p1q] n=%ld me=%ld mi=%ld status=%d minors=%ld facts=%ld upd=%ld ref=%ld reset=%ld "
                     "readm=%ld relrung=%ld fprung=%ld fpride=%ld fpdecl=%ld dlam_max=%.3e lam_max=%.3e "
                     "mu_end=%.1e\\n",
                     static_cast<long>(n), static_cast<long>(me), static_cast<long>(mi),
                     static_cast<int>(status), static_cast<long>(counters.minor_iters),
                     static_cast<long>(counters.factorizations), p1_upd, p1_ref, p1_reset, p1_readm,
                     p1_relrung, static_cast<long>(counters.ascent_footprint_rungs),
                     static_cast<long>(counters.ascent_footprint_rides),
                     static_cast<long>(counters.ascent_footprint_declines), p1_dlam_max, p1_lam_max,
                     eff_opts.dual_regularization);
    }
    QpSolution out;
    out.status = status;
    out.x = std::move(x);''')
s=s.replace('#include <cstdlib> // P1SCRATCH\n','#include <cstdio> // P1SCRATCH\n#include <cstdlib> // P1SCRATCH\n#include <string> // P1SCRATCH\n')
open(p,'w').write(s)
print("ok")
