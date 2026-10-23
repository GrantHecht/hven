// a1_probe -- M7 W2 A1 design-round diagnostic. READ-ONLY instrument: it builds
// public/bench fixtures and calls the shipped engines with public options only.
// hven is unmodified. Modes:
//   e1 <cell-id> <converge_slack> <hard_cap>   one E1 cell, tier-direct, per-iteration trace
//   gate8 <converge_slack>                     the gate-8 family at Sigma targets, tier-direct
//   f7 <walk|ipm>                               F7 corpus neutral-cold + physics-informed cells:
//                                               SQP weak/near census (M7-20 precondition read)
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "ipqp_e1_arm.h"
#include "corpus_cells.h"

#include <hven/drivers/trace.h>

using namespace hven;
using namespace hven::solvers;

namespace {

struct IterSink final : TraceSink {
    void on_ipqp_iter(const IpqpTraceIterEvent &e) override {
        std::printf("iter it=%lld mu=%.3e res_p=%.3e res_d=%.3e res_c=%.3e sigma=%.3e "
                    "alpha_p=%.3e alpha_d=%.3e rho=%.3e\n",
                    static_cast<long long>(e.it), e.mu, e.res_p, e.res_d, e.res_c, e.sigma,
                    e.alpha_p, e.alpha_d, e.rho);
    }
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
};

IpqpOptions iopts_for(double slack, Index cap) {
    IpqpOptions o;
    o.ipqp_converge_slack = slack;
    if (cap > 0) {
        o.ipqp_hard_iter_cap = cap;
        o.ipqp_max_iter = cap;
        o.ipqp_max_factorizations = 4 * cap;
    }
    return o;
}

int run_e1(const std::string &id, double slack, Index cap) {
    const auto specs = e1arm::taxonomy();
    const auto it = std::find_if(specs.begin(), specs.end(),
                                 [&](const e1arm::CellSpec &s) { return s.id == id; });
    if (it == specs.end()) {
        std::fprintf(stderr, "unknown cell %s\n", id.c_str());
        return 2;
    }
    const e1arm::Cell cell = e1arm::build(*it);
    const IpqpOptions io = iopts_for(slack, cap);
    QpOptions qo;
    qo.tr_radius = std::numeric_limits<double>::infinity();
    IpqpEngine tier(qo);
    IterSink sink;
    tier.attach_trace(&sink);
    const IpqpResult r = tier.solve(cell.qp, nullptr, io, SolveOverrides{});
    tier.attach_trace(nullptr);

    const Index mi = cell.qp.bi.size();
    std::vector<char> truth(static_cast<std::size_t>(mi), 0);
    for (const Index j : cell.active) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    const Vec sstar = cell.qp.bi - e1arm::detail::a_times(cell.qp.Ai, cell.x_star);
    Index over = 0, under = 0, unc = 0, unc_true_active = 0;
    std::vector<double> over_noise; // z_j * s*_j / mu over over-committed rows
    std::vector<double> over_rel;   // s*_j / row_scale_j over over-committed rows
    double min_inactive_sstar = std::numeric_limits<double>::infinity();
    for (Index j = 0; j < mi; ++j) {
        const auto u = static_cast<std::size_t>(j);
        const IpqpFace f = r.ineq_face[u];
        if (truth[u] == 0) {
            min_inactive_sstar = std::min(min_inactive_sstar, sstar(j));
        }
        if (f == IpqpFace::kUncertain) {
            ++unc;
            unc_true_active += truth[u];
        } else if (f == IpqpFace::kActive && truth[u] == 0) {
            ++over;
            over_noise.push_back(r.lambda_i(j) * sstar(j) / r.mu);
            over_rel.push_back(sstar(j) / cell.row_scale(j));
        } else if (f == IpqpFace::kInactive && truth[u] == 1) {
            ++under;
        }
    }
    auto med = [](std::vector<double> v) {
        if (v.empty()) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    auto mx = [](const std::vector<double> &v) {
        return v.empty() ? std::numeric_limits<double>::quiet_NaN()
                         : *std::max_element(v.begin(), v.end());
    };
    std::printf("result id=%s slack=%.0e cap=%lld status=%d escape=%d iters=%lld facts=%lld "
                "mu=%.3e sqrt_mu=%.3e res_p=%.3e res_d=%.3e res_c=%.3e\n",
                id.c_str(), slack, static_cast<long long>(cap), static_cast<int>(r.status),
                static_cast<int>(r.escape_reason), static_cast<long long>(r.counters.ipqp_iters),
                static_cast<long long>(r.counters.ipqp_factorizations), r.mu,
                std::sqrt(std::max(r.mu, 0.0)), r.residuals.primal_iq, r.residuals.stationarity,
                r.residuals.complementarity);
    std::printf("face active_true=%zu over_committed=%lld under_committed=%lld uncertain=%lld "
                "uncertain_truly_active=%lld min_inactive_slack_abs=%.3e\n",
                cell.active.size(), static_cast<long long>(over), static_cast<long long>(under),
                static_cast<long long>(unc), static_cast<long long>(unc_true_active),
                min_inactive_sstar);
    std::printf("overcommit z*s_true/mu median=%.3e max=%.3e ; s_true/row_scale median=%.3e "
                "max=%.3e\n",
                med(over_noise), mx(over_noise), med(over_rel), mx(over_rel));
    const e1arm::SolveRow row = e1arm::solve(cell, io);
    std::printf("e2e usable=%d polished=%d rule_a=%lld rule_a_missed=%lld rule_a_fp=%lld "
                "e2e_rule_a_missed=%lld e2e_rule_a_fp=%lld e2e_factorizations=%lld\n",
                row.e2e_usable ? 1 : 0, row.e2e_polished ? 1 : 0,
                static_cast<long long>(row.rule_a), static_cast<long long>(row.rule_a_missed),
                static_cast<long long>(row.rule_a_false_positive),
                static_cast<long long>(row.e2e_rule_a_missed),
                static_cast<long long>(row.e2e_rule_a_false_positive),
                static_cast<long long>(row.e2e_factorizations));
    return 0;
}

// DEMOTION EXPERIMENT: the tier's face handed to refine_on_face as the driver hands it, with
// committed-active rows demoted under three variants: none (today), band (history-free proxy:
// committed-active rows whose multiplier z <= kIpqpTightBandFactor * kIpqpWeakActiveFactor *
// sqrt(mu)), oracle (exactly the truly inactive committed rows -- an UPPER BOUND, unrealizable).
int run_e1demote(const std::string &id, double slack, Index cap) {
    const auto specs = e1arm::taxonomy();
    const auto it = std::find_if(specs.begin(), specs.end(),
                                 [&](const e1arm::CellSpec &s) { return s.id == id; });
    if (it == specs.end()) {
        return 2;
    }
    const e1arm::Cell cell = e1arm::build(*it);
    const IpqpOptions io = iopts_for(slack, cap);
    QpOptions qo;
    qo.tr_radius = std::numeric_limits<double>::infinity();
    IpqpEngine tier(qo);
    const IpqpResult r = tier.solve(cell.qp, nullptr, io, SolveOverrides{});
    const Index mi = cell.qp.bi.size();
    std::vector<char> truth(static_cast<std::size_t>(mi), 0);
    for (const Index j : cell.active) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    const double band = hven::solvers::detail::kIpqpTightBandFactor *
                        hven::solvers::detail::kIpqpWeakActiveFactor *
                        std::sqrt(std::max(r.mu, 0.0));
    const bool usable = ipqp_exit_is_a_usable_step(r, qo, io);
    for (const char *variant : {"none", "band", "oracle"}) {
        IpqpResult d = r;
        Index demoted = 0, demoted_true_active = 0, face_rows = 0;
        for (Index j = 0; j < mi; ++j) {
            const auto u = static_cast<std::size_t>(j);
            if (!d.ineq_active[u]) {
                continue;
            }
            bool demote = false;
            if (std::string(variant) == "band") {
                demote = d.lambda_i(j) <= band;
            } else if (std::string(variant) == "oracle") {
                demote = truth[u] == 0;
            }
            if (demote) {
                d.ineq_active[u] = false;
                d.ineq_face[u] = IpqpFace::kUncertain;
                d.lambda_i(j) = 0.0;
                ++demoted;
                demoted_true_active += truth[u];
            }
        }
        for (Index j = 0; j < mi; ++j) {
            face_rows += d.ineq_active[static_cast<std::size_t>(j)] ? 1 : 0;
        }
        bool polished = false;
        Index pf = 0;
        Vec x_end = d.x;
        if (usable) {
            QpEngine polish(qo);
            QpSolution refined;
            polished = polish.refine_on_face(cell.qp, ipqp_result_to_qp_solution(d),
                                             SolveOverrides{}, refined);
            pf = refined.counters.factorizations;
            if (polished) {
                x_end = refined.x;
            }
        }
        const Vec ax = e1arm::detail::a_times(cell.qp.Ai, x_end);
        Index missed = 0, fp = 0;
        for (Index j = 0; j < mi; ++j) {
            const bool a = (cell.qp.bi(j) - ax(j)) <= 1e-8 * cell.row_scale(j);
            const bool t = truth[static_cast<std::size_t>(j)] == 1;
            missed += (!a && t) ? 1 : 0;
            fp += (a && !t) ? 1 : 0;
        }
        std::printf("demote id=%s slack=%.0e cap=%lld iters=%lld mu=%.3e band_z=%.3e variant=%s "
                    "usable=%d demoted=%lld demoted_true_active=%lld face_rows=%lld "
                    "active_true=%zu polished=%d polish_facts=%lld e2e_rule_a_missed=%lld "
                    "e2e_rule_a_fp=%lld\n",
                    id.c_str(), slack, static_cast<long long>(cap),
                    static_cast<long long>(r.counters.ipqp_iters), r.mu, band, variant,
                    usable ? 1 : 0, static_cast<long long>(demoted),
                    static_cast<long long>(demoted_true_active), static_cast<long long>(face_rows),
                    cell.active.size(), polished ? 1 : 0, static_cast<long long>(pf),
                    static_cast<long long>(missed), static_cast<long long>(fp));
        std::fflush(stdout);
    }
    return 0;
}

// The gate-8 family (tests/sqp/test_ipqp_certification.cpp:529-559), rebuilt verbatim.
QpProblem gate8(double s, double h) {
    QpProblem qp;
    qp.H = Eigen::SparseMatrix<double>(2, 2);
    std::vector<Eigen::Triplet<double>> t = {{0, 0, 2.0}, {1, 1, h}};
    qp.H.setFromTriplets(t.begin(), t.end());
    qp.g = Vec::Zero(2);
    qp.Ae = Eigen::SparseMatrix<double>(0, 2);
    qp.be = Vec(0);
    qp.Ai = Eigen::SparseMatrix<double>(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << -10.0, -s;
    qp.upper = Vec(2);
    qp.upper << 10.0, s;
    return qp;
}

int run_gate8(double slack) {
    QpOptions qo;
    qo.tr_radius = std::numeric_limits<double>::infinity();
    // mu_stop measured exactly as gate8_mu_stop() does, at the SHIPPED slack.
    IpqpEngine probe(qo);
    const double mu_stop = probe.solve(gate8(1.0e-4, -1.0), nullptr, IpqpOptions{}, {}).mu;
    std::printf("gate8 mu_stop(shipped slack)=%.6e\n", mu_stop);
    for (const double target : {0.77, 1.28, 10.0, 250.0, 1000.0, 25000.0, 2.5e5}) {
        const double s = std::sqrt(2.0 * mu_stop / target);
        for (const double h : {-1.0, 1.0}) {
            IpqpEngine tier(qo);
            const IpqpResult r = tier.solve(gate8(s, h), nullptr, iopts_for(slack, 0), {});
            const double sigma = r.zl(1) / (r.x(1) + s) + r.zu(1) / (s - r.x(1));
            std::printf("gate8 slack=%.0e target=%.3g h=%+.0f s=%.3e iters=%lld mu=%.3e "
                        "sigma=%.3e x1=%.2e status=%d escape=%d read=%lld downgraded=%d "
                        "kept_tight=%d band=%lld noise=%lld\n",
                        slack, target, h, s, static_cast<long long>(r.counters.ipqp_iters), r.mu,
                        sigma, r.x(1), static_cast<int>(r.status),
                        static_cast<int>(r.escape_reason),
                        static_cast<long long>(r.counters.ipqp_final_inertia_read),
                        r.certificate_downgraded ? 1 : 0, r.read_kept_tight ? 1 : 0,
                        static_cast<long long>(r.counters.ipqp_read_kept_tight_sides),
                        static_cast<long long>(r.counters.ipqp_read_barrier_noise_sides));
        }
    }
    return 0;
}

int run_f7(const std::string &engine) {
    using namespace hven::solvers::corpus;
    for (const CorpusCell &cell : all_cells()) {
        // The U0 replay's own window (bound arc) only: the path-interface neutral cells are
        // outside the replay set and the walk runs them for minutes (registered, not a defect).
        if ((cell.start != StartTaxonomy::kNeutralCold &&
             cell.start != StartTaxonomy::kPhysicsInformed) ||
            cell.ctag != (std::getenv("A1_F7_PATH") != nullptr ? ConstraintFamily::kPathInterface
                                                               : ConstraintFamily::kBoundArc)) {
            continue;
        }
        // A1_F7_MAXN bounds the node count (the path window's walk runs past 14 minutes at
        // N = 2000, outside a light diagnostic).
        if (const char *mx = std::getenv("A1_F7_MAXN");
            mx != nullptr && cell.n_nodes > std::stoll(mx)) {
            continue;
        }
        corpus::detail::EngineConfig cfg;
        cfg.qp_mode = engine == "ipm" ? QpMode::kIpm : QpMode::kWalk;
        F7CollocationChain model = corpus::detail::make_model(cell);
        const SqpOptions opts = corpus::detail::options_for_cell(cell, cfg);
        SqpSolver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = cell.start == StartTaxonomy::kNeutralCold
                           ? model.start_point()
                           : corpus::detail::physics_informed_start(model, cell.p);
        const SqpResult res = corpus::detail::budgeted_solve(driver, model, x0);
        Index rows_nz_near = 0, rows_nz_weak = 0, majors_with_rows = 0;
        long long near_sum = 0, active_sum = 0;
        for (const SqpIterate &h : res.history) {
            if (!h.qp_solved) {
                continue;
            }
            ++majors_with_rows;
            rows_nz_near += h.near_active_rows > 0 ? 1 : 0;
            rows_nz_weak += h.weak_active_rows > 0 ? 1 : 0;
            near_sum += h.near_active_rows;
            active_sum += h.active_rows;
        }
        std::printf("f7 engine=%s cell=%s mi_nodes=%lld status=%d majors=%lld weak_peak=%lld "
                    "near_peak=%lld majors_qp=%lld majors_near_gt0=%lld majors_weak_gt0=%lld "
                    "near_sum=%lld active_rows_sum=%lld ipqp_band=%lld ipqp_noise=%lld "
                    "ipqp_uncertain=%lld\n",
                    engine.c_str(), cell.id, static_cast<long long>(cell.n_nodes),
                    static_cast<int>(res.status), static_cast<long long>(res.history.size()),
                    static_cast<long long>(res.counters.weak_active_peak),
                    static_cast<long long>(res.counters.near_active_peak),
                    static_cast<long long>(majors_with_rows),
                    static_cast<long long>(rows_nz_near), static_cast<long long>(rows_nz_weak),
                    near_sum, active_sum,
                    static_cast<long long>(res.counters.ipqp.ipqp_read_kept_tight_sides),
                    static_cast<long long>(res.counters.ipqp.ipqp_read_barrier_noise_sides),
                    static_cast<long long>(res.counters.ipqp.ipqp_face_uncertain));
        std::fflush(stdout);
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a1_probe e1|gate8|f7 ...\n");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode == "e1" && argc == 5) {
        return run_e1(argv[2], std::stod(argv[3]), std::stoll(argv[4]));
    }
    if (mode == "e1demote" && argc == 5) {
        return run_e1demote(argv[2], std::stod(argv[3]), std::stoll(argv[4]));
    }
    if (mode == "gate8" && argc == 3) {
        return run_gate8(std::stod(argv[2]));
    }
    if (mode == "f7" && argc == 3) {
        return run_f7(argv[2]);
    }
    std::fprintf(stderr, "bad arguments\n");
    return 2;
}
