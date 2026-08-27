// piqp_e1_driver.cpp — the E1 acquisition experiment's SOLVE ARM.
//
// Derived, in this disposable scratch workspace, from the archived
// tycho_sqp prototypes/piqp_bridge/piqp_f7_driver.cpp (commit 91c4ec1). The
// solve path, the bound-sentinel mapping, the external-residual derivation and
// the referee `gate` are that file's, carried over unchanged; what is new here
// is the E1 row: the taxonomy columns, the ACQUIRED-vs-TRUE active-set
// comparison, and a derived factorization count. Read that file's banner for
// the parts this one inherits — in particular why the residual is recomputed
// in OUR unscaled data rather than read off PIQP's own preconditioner-scaled
// info.primal_res/info.dual_res, and why PIQP is an ORACLE that is never
// vendored (its sparse backend links Timothy Davis's LDL, LGPL-2.1+).
//
// ---------------------------------------------------------------------------
// THE ACQUIRED ACTIVE SET — the stated thresholding rule
// ---------------------------------------------------------------------------
//
// An interior-point solution never puts a slack exactly at zero, so "which
// rows did PIQP report active" is a thresholding question and the threshold is
// declared here rather than left to a reader:
//
//   RULE A (PRIMARY, slack rule; the CSV's `active_set_size_found`):
//       row j is ACTIVE iff  bi(j) - (Ai x)(j)  <=  1e-8 * row_scale(j)
//   where row_scale is the generator's OWN per-row scale, carried in the dump
//   (E1_ROWSCALE_VEC) so the rule reads activity in the same units the cell's
//   near-activity margins were drawn in.
//
//   RULE B (CORROBORATING, dual rule; `active_set_size_found_dual`):
//       row j is ACTIVE iff  z_u(j)  >=  1e-6 * max(1, ||z_u||_inf).
//
// Both are reported on every row. The separation the rules have to resolve is
// known by construction: a truly active row's slack is O(mu) ~ 1e-10 or below
// at convergence, while the TIGHTEST inactive row in the hardest margin class
// sits at ~0.5e-6 * row_scale — two orders above Rule A's threshold — so the
// rule is not sitting on top of the taxonomy's own tie pressure. Set agreement
// (`active_true_recovered`, `active_false_positive`) is reported against the
// dump's E1_ACTIVE_TRUE, so the CSV records WHICH rows were acquired, not just
// how many.
//
// ---------------------------------------------------------------------------
// `factorizations` — a DERIVED counter, and how it is derived
// ---------------------------------------------------------------------------
//
// PIQP's public Info (piqp/results.hpp) exposes no cumulative factorization
// count: `factor_retires` is a RETRY counter that is reset to 0 after every
// successful factorization (solver.tpp:462 and :705), so its returned value is
// 0 on any run that did not exit through PIQP_NUMERICS. What the pinned
// source does expose is its control flow, and it is unambiguous: there is
// exactly ONE `m_kkt_system.update_scalings_and_factor` site in the
// initialization (solver.tpp:442) and exactly ONE inside the main iteration
// loop (solver.tpp:684), each wrapped in a retry `while`. So
//
//     factorizations = 1 + info.iter        (successful numeric factorizations)
//
// with any regularization RETRIES excluded because they are not observable
// through the public API. The CSV carries `factor_retires_final` beside it so
// a reader can see the one case where retries would be visible. This is a
// derivation from the pinned v0.6.3 source, stated as such — not a counter
// PIQP reports.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <piqp/piqp.hpp>

#include <tycho_sqp/nlp_model.h>
#include <tycho_sqp/qp_problem.h>

#include "../support_tests/scale_problems.h"

#include "e1_dump.h"

namespace {

using tycho::sqp::Index;
using tycho::sqp::NlpModel;
using tycho::sqp::QpProblem;
using tycho::sqp::Vec;
using tycho::sqp::test_support::F7CollocationChain;

// Carried verbatim from piqp_f7_driver.cpp -- see that file for why
// sqp_driver.h is reimplemented here rather than included.
QpProblem build_subproblem(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                           const Vec &lambda_i, double obj_scale = 1.0) {
    QpProblem qp;
    qp.H = model.eval_hess(x, obj_scale, lambda_e, lambda_i);
    qp.H.makeCompressed();
    qp.g = obj_scale * model.eval_grad(x);
    qp.Ae = model.eval_jac_e(x);
    qp.Ae.makeCompressed();
    qp.be = -model.eval_ce(x);
    qp.Ai = model.eval_jac_i(x);
    qp.Ai.makeCompressed();
    qp.bi = -model.eval_ci(x);
    qp.lower = model.lower() - x;
    qp.upper = model.upper() - x;
    return qp;
}

e1::GenericQp from_qp_problem(const QpProblem &qp) {
    e1::GenericQp g;
    g.n = qp.n();
    g.me = qp.me();
    g.mi = qp.mi();
    for (int k = 0; k < qp.H.outerSize(); ++k) {
        for (tycho::sqp::SpMatU::InnerIterator it(qp.H, k); it; ++it) {
            g.h.push_back({it.row(), it.col(), it.value()});
        }
    }
    g.g = qp.g;
    for (int k = 0; k < qp.Ae.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ae, k); it; ++it) {
            g.ae.push_back({it.row(), it.col(), it.value()});
        }
    }
    g.be = qp.be;
    for (int k = 0; k < qp.Ai.outerSize(); ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ai, k); it; ++it) {
            g.ai.push_back({it.row(), it.col(), it.value()});
        }
    }
    g.bi = qp.bi;
    g.lower = qp.lower;
    g.upper = qp.upper;
    return g;
}

struct PiqpResult {
    piqp::Status status = piqp::Status::PIQP_UNSOLVED;
    long iter = 0;
    long factor_retires = 0;
    double wall_s = 0.0;
    double setup_s = 0.0;
    Vec x, y, z_u, z_bl, z_bu;
};

// piqp_f7_driver.cpp's own sentinel mapping and its reasoning -- tycho_sqp's
// "practically infinite" bound is a large FINITE double (1e20), which a
// barrier method reads as a genuine extreme constraint; PIQP only recognises
// an actual infinity as "no bound". Feasibility below is still scored against
// the UNMAPPED bounds.
constexpr double kEffectivelyInfinite = 1e19;

double to_piqp_bound(double v) {
    if (v >= kEffectivelyInfinite) {
        return std::numeric_limits<double>::infinity();
    }
    if (v <= -kEffectivelyInfinite) {
        return -std::numeric_limits<double>::infinity();
    }
    return v;
}

Vec to_piqp_bounds(const Vec &v) {
    Vec out(v.size());
    for (long i = 0; i < v.size(); ++i) {
        out(i) = to_piqp_bound(v(i));
    }
    return out;
}

PiqpResult solve_with_piqp(const e1::GenericQp &qp, double eps_abs = 1e-10, int max_iter = 200) {
    const long n = qp.n, me = qp.me, mi = qp.mi;
    const Vec lower = to_piqp_bounds(qp.lower);
    const Vec upper = to_piqp_bounds(qp.upper);

    Eigen::SparseMatrix<double> P(n, n);
    {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(qp.h.size() * 2);
        for (const e1::Triplet &tr : qp.h) {
            t.emplace_back(int(tr.row), int(tr.col), tr.value);
            if (tr.row != tr.col) {
                t.emplace_back(int(tr.col), int(tr.row), tr.value);
            }
        }
        P.setFromTriplets(t.begin(), t.end());
    }
    Eigen::SparseMatrix<double> A(me, n);
    {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(qp.ae.size());
        for (const e1::Triplet &tr : qp.ae) {
            t.emplace_back(int(tr.row), int(tr.col), tr.value);
        }
        A.setFromTriplets(t.begin(), t.end());
    }
    Eigen::SparseMatrix<double> G(mi, n);
    {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(qp.ai.size());
        for (const e1::Triplet &tr : qp.ai) {
            t.emplace_back(int(tr.row), int(tr.col), tr.value);
        }
        G.setFromTriplets(t.begin(), t.end());
    }

    piqp::SparseSolver<double> solver;
    solver.settings().eps_abs = eps_abs;
    solver.settings().eps_rel = 0.0;
    solver.settings().check_duality_gap = false;
    solver.settings().max_iter = max_iter;
    solver.settings().verbose = false;
    solver.settings().compute_timings = true;

    if (mi > 0) {
        solver.setup(P, qp.g, A, qp.be, G, piqp::nullopt, qp.bi, lower, upper);
    } else {
        solver.setup(P, qp.g, A, qp.be, piqp::nullopt, piqp::nullopt, piqp::nullopt, lower, upper);
    }

    const auto t0 = std::chrono::steady_clock::now();
    const piqp::Status status = solver.solve();
    const auto t1 = std::chrono::steady_clock::now();

    PiqpResult r;
    r.status = status;
    r.iter = static_cast<long>(solver.result().info.iter);
    r.factor_retires = static_cast<long>(solver.result().info.factor_retires);
    r.wall_s = std::chrono::duration<double>(t1 - t0).count();
    r.setup_s = solver.result().info.setup_time;
    r.x = solver.result().x;
    r.y = solver.result().y;
    r.z_u = solver.result().z_u;
    r.z_bl = solver.result().z_bl;
    r.z_bu = solver.result().z_bu;
    return r;
}

struct ExternalResidual {
    double stationarity = 0.0;
    double feasibility = 0.0;
    double complementarity = 0.0;
    double residual() const { return std::max(stationarity, feasibility); }
};

ExternalResidual external_residual(const e1::GenericQp &qp, const PiqpResult &r) {
    Vec grad = qp.g;
    e1::add_h_times(qp.h, r.x, grad);
    e1::add_at_times(qp.ae, r.y, grad);
    e1::add_at_times(qp.ai, r.z_u, grad);

    ExternalResidual out;
    const double bound_tol = 1e-8;
    for (long i = 0; i < qp.n; ++i) {
        const bool at_lower = (r.x(i) - qp.lower(i)) <= bound_tol;
        const bool at_upper = (qp.upper(i) - r.x(i)) <= bound_tol;
        const double z = r.z_bl(i) - r.z_bu(i);
        double s;
        if (at_lower && at_upper) {
            s = 0.0;
        } else if (at_lower) {
            s = std::max(0.0, -(grad(i) - z));
        } else if (at_upper) {
            s = std::max(0.0, (grad(i) - z));
        } else {
            s = std::abs(grad(i) - z);
        }
        out.stationarity = std::max(out.stationarity, s);
    }

    Vec ce = e1::a_times(qp.ae, r.x, qp.me) - qp.be;
    if (qp.me > 0) {
        out.feasibility = std::max(out.feasibility, ce.lpNorm<Eigen::Infinity>());
    }
    Vec ci = e1::a_times(qp.ai, r.x, qp.mi) - qp.bi;
    for (long j = 0; j < qp.mi; ++j) {
        out.feasibility = std::max(out.feasibility, std::max(0.0, ci(j)));
        out.complementarity = std::max(out.complementarity, std::abs(r.z_u(j) * ci(j)));
    }
    for (long i = 0; i < qp.n; ++i) {
        out.feasibility = std::max(out.feasibility, std::max(0.0, qp.lower(i) - r.x(i)));
        out.feasibility = std::max(out.feasibility, std::max(0.0, r.x(i) - qp.upper(i)));
    }
    return out;
}

// The acquired active set, under both declared rules (see this file's banner).
// Up to this many disagreeing rows are described individually on stdout, so a
// cell whose acquired set differs from its constructed one is never left as a
// bare count -- the sweep log then carries WHICH row and HOW FAR off it was.
constexpr int kMaxDisagreementsPrinted = 10;

struct Acquired {
    long found_slack = 0;
    long found_dual = 0;
    long recovered = 0;      // |found_slack INTERSECT true|
    long false_positive = 0; // |found_slack \ true|
    long missed = 0;         // |true \ found_slack|
    double max_ci = -std::numeric_limits<double>::infinity();
    double min_box_slack = std::numeric_limits<double>::infinity();
    double max_abs_z_u = 0.0;
};

constexpr double kSlackTol = 1e-8; // Rule A
constexpr double kDualTol = 1e-6;  // Rule B

Acquired acquired_set(const e1::GenericQp &qp, const PiqpResult &r) {
    Acquired out;
    const Vec ci = e1::a_times(qp.ai, r.x, qp.mi) - qp.bi; // = (Ai x) - bi = -slack
    std::vector<char> is_true(static_cast<std::size_t>(qp.mi), 0);
    for (const long j : qp.active_true) {
        is_true[static_cast<std::size_t>(j)] = 1;
    }
    double z_inf = 0.0;
    for (long j = 0; j < qp.mi; ++j) {
        z_inf = std::max(z_inf, std::abs(r.z_u(j)));
    }
    const double z_thresh = kDualTol * std::max(1.0, z_inf);
    for (long j = 0; j < qp.mi; ++j) {
        const double slack = -ci(j);
        const double scale = (qp.row_scale.size() == qp.mi) ? qp.row_scale(j) : 1.0;
        const bool active_a = slack <= kSlackTol * std::max(scale, 1e-300);
        const bool active_b = r.z_u(j) >= z_thresh;
        if (active_a) {
            ++out.found_slack;
            if (is_true[static_cast<std::size_t>(j)] != 0) {
                ++out.recovered;
            } else {
                ++out.false_positive;
                if (out.false_positive <= kMaxDisagreementsPrinted) {
                    std::printf("  disagreement: row %ld FALSE POSITIVE -- slack %.6e, row_scale "
                                "%.6e, rel %.6e, z_u %.6e\n",
                                j, slack, scale, slack / scale, r.z_u(j));
                }
            }
        } else if (is_true[static_cast<std::size_t>(j)] != 0) {
            ++out.missed;
            if (out.missed <= kMaxDisagreementsPrinted) {
                std::printf("  disagreement: row %ld MISSED (true-active, not acquired under the "
                            "slack rule) -- slack %.6e, row_scale %.6e, rel %.6e, threshold %.6e, "
                            "z_u %.6e\n",
                            j, slack, scale, slack / scale, kSlackTol * scale, r.z_u(j));
            }
        }
        if (active_b) {
            ++out.found_dual;
        }
        out.max_ci = std::max(out.max_ci, ci(j));
        out.max_abs_z_u = std::max(out.max_abs_z_u, std::abs(r.z_u(j)));
    }
    for (long i = 0; i < qp.n; ++i) {
        out.min_box_slack = std::min(out.min_box_slack, r.x(i) - qp.lower(i));
        out.min_box_slack = std::min(out.min_box_slack, qp.upper(i) - r.x(i));
    }
    return out;
}

// ---------------------------------------------------------------------------
// THE REFEREE GATE -- carried over from piqp_f7_driver.cpp unchanged. See that
// file for why eps_abs defaults to 1e-12 here and 1e-10 on the measurement arm.
// ---------------------------------------------------------------------------
int run_gate(double eps_abs = 1e-12) {
    constexpr double kBoundArcP = 0.45;
    constexpr Index kGateNodes = 30;

    F7CollocationChain model(kGateNodes, /*states=*/3, /*controls=*/2, kBoundArcP, /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kBoundArcP));
    const Vec x0 = model.start_point();
    const QpProblem qp = build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    const e1::GenericQp gq = from_qp_problem(qp);

    const PiqpResult r = solve_with_piqp(gq, eps_abs);
    if (r.status != piqp::Status::PIQP_SOLVED) {
        std::cerr << "piqp_e1_driver: gate: PIQP did not report PIQP_SOLVED (status="
                  << static_cast<int>(r.status) << ") -- HARD STOP\n";
        return 1;
    }
    const Vec x = x0 + r.x;
    const Vec x_star = model.x_star(kBoundArcP);
    const double x_err = (x - x_star).lpNorm<Eigen::Infinity>();
    const ExternalResidual res = external_residual(gq, r);
    std::cout << "piqp_e1_driver gate: N=" << kGateNodes << " p=" << kBoundArcP
              << " eps_abs=" << eps_abs << " piqp_iter=" << r.iter << " wall_s=" << r.wall_s
              << " x_err_inf=" << x_err << " (threshold 1e-8)"
              << " ext_stationarity=" << res.stationarity << " ext_feasibility=" << res.feasibility
              << "\n";
    if (!(x_err <= 1e-8)) {
        std::cerr << "piqp_e1_driver: gate: FAIL -- x_err_inf = " << x_err
                  << " exceeds 1e-8. HARD STOP: no oracle row may be quoted.\n";
        return 1;
    }
    std::cout << "piqp_e1_driver: gate: PASS\n";
    return 0;
}

// ---------------------------------------------------------------------------
// THE E1 MEASUREMENT ARM.
// ---------------------------------------------------------------------------

const char *kCsvHeader =
    "id,n,me,mi,kind,active_fraction,margin_class,status,iters,factorizations,"
    "factor_retires_final,active_set_size_true,active_set_size_found,active_set_size_found_dual,"
    "active_true_recovered,active_false_positive,active_missed,res_primal,res_dual,"
    "res_complementarity,x_err_inf,min_box_slack,max_ci,max_abs_z_u,wall_info_s,setup_info_s,"
    "threads\n";

void write_csv_header_if_new(const std::string &path) {
    std::ifstream probe(path);
    const bool exists = probe.good() && probe.peek() != std::ifstream::traits_type::eof();
    probe.close();
    if (exists) {
        return;
    }
    std::ofstream out(path);
    out << kCsvHeader;
}

int run_cell(const std::string &dump_path, const std::string &sol_path,
             const std::string &csv_path) {
    const e1::GenericQp qp = e1::read_dump(dump_path);
    const PiqpResult r = solve_with_piqp(qp);
    const ExternalResidual res = external_residual(qp, r);
    const Acquired acq = acquired_set(qp, r);

    double x_err = -1.0;
    if (!sol_path.empty()) {
        const e1::GroundTruth gt = e1::read_solution(sol_path);
        if (gt.x_star.size() != qp.n) {
            throw std::runtime_error("piqp_e1_driver: sidecar x* size disagrees with the dump's n");
        }
        x_err = (r.x - gt.x_star).lpNorm<Eigen::Infinity>();
    }

    const long factorizations = 1 + r.iter;
    const long true_active = static_cast<long>(qp.active_true.size());

    std::cout << "e1 run: id=" << qp.cell_id << " n=" << qp.n << " mi=" << qp.mi
              << " kind=" << qp.kind << " af=" << qp.active_fraction
              << " margin=" << qp.margin_class << " status=" << piqp::status_to_string(r.status)
              << " iters=" << r.iter << " factorizations=" << factorizations
              << " active_true=" << true_active << " active_found=" << acq.found_slack
              << " (dual rule " << acq.found_dual << ")"
              << " recovered=" << acq.recovered << " false_pos=" << acq.false_positive
              << " missed=" << acq.missed << " res_primal=" << res.feasibility
              << " res_dual=" << res.stationarity << " res_comp=" << res.complementarity
              << " x_err_inf=" << x_err << " min_box_slack=" << acq.min_box_slack
              << " max_ci=" << acq.max_ci << " max|z_u|=" << acq.max_abs_z_u
              << " wall_info_s=" << r.wall_s << "\n";

    if (!csv_path.empty()) {
        write_csv_header_if_new(csv_path);
        std::ofstream out(csv_path, std::ios::app);
        out << qp.cell_id << "," << qp.n << "," << qp.me << "," << qp.mi << "," << qp.kind << ","
            << qp.active_fraction << "," << qp.margin_class << ","
            << piqp::status_to_string(r.status) << "," << r.iter << "," << factorizations << ","
            << r.factor_retires << "," << true_active << "," << acq.found_slack << ","
            << acq.found_dual << "," << acq.recovered << "," << acq.false_positive << ","
            << acq.missed << "," << std::setprecision(17) << res.feasibility << ","
            << res.stationarity << "," << res.complementarity << "," << x_err << ","
            << acq.min_box_slack << "," << acq.max_ci << "," << acq.max_abs_z_u << ","
            << std::setprecision(9) << r.wall_s << "," << r.setup_s << ",1\n";
    }
    return r.status == piqp::Status::PIQP_SOLVED ? 0 : 1;
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: piqp_e1_driver gate [eps_abs]\n"
                      << "       piqp_e1_driver run <dump> [--sol <sol>] [--out <csv>]\n"
                      << "       piqp_e1_driver header <csv>   (write the CSV header only)\n";
            return 1;
        }
        const std::string mode = argv[1];
        if (mode == "gate") {
            return argc >= 3 ? run_gate(std::stod(argv[2])) : run_gate();
        }
        if (mode == "header") {
            if (argc < 3) {
                std::cerr << "piqp_e1_driver header: needs a path\n";
                return 1;
            }
            write_csv_header_if_new(argv[2]);
            return 0;
        }
        if (mode == "run") {
            if (argc < 3) {
                std::cerr << "usage: piqp_e1_driver run <dump> [--sol <sol>] [--out <csv>]\n";
                return 1;
            }
            std::string sol_path, csv_path;
            for (int i = 3; i < argc; ++i) {
                const std::string a = argv[i];
                if (a == "--sol" && i + 1 < argc) {
                    sol_path = argv[++i];
                } else if (a == "--out" && i + 1 < argc) {
                    csv_path = argv[++i];
                }
            }
            return run_cell(argv[2], sol_path, csv_path);
        }
        std::cerr << "piqp_e1_driver: unknown mode '" << mode << "'\n";
        return 1;
    } catch (const std::exception &e) {
        std::cerr << "piqp_e1_driver: error: " << e.what() << "\n";
        return 1;
    }
}
