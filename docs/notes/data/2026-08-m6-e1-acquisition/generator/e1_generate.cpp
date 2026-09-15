// e1_generate.cpp — the E1 acquisition experiment's CELL GENERATOR.
//
// E1 asks whether an interior-point QP solve ACQUIRES a nontrivial active set
// at scale on F7-pattern cells (protocol:
// hven/docs/notes/2026-08-26-e1-acquisition-experiment.md). Answering that
// needs cells whose true active set is KNOWN, which the oracle's own cells
// were not: every one of them had an effectively equality-constrained first
// QP. This file manufactures such cells.
//
// ---------------------------------------------------------------------------
// WHAT IS TAKEN FROM F7, AND WHAT IS CONSTRUCTED
// ---------------------------------------------------------------------------
//
// The QP's STRUCTURE is F7's own, not an imitation of it: the generator builds
// tests/support/scale_problems.h's F7CollocationChain at the requested node
// count and takes H, Ae, Ai, lower, upper VERBATIM from the same first-QP
// linearization the oracle measured — the neutral cold start
// (x0 = model.start_point(), lambda_e = lambda_i = 0), through the same local
// `build_subproblem` reimplementation piqp_f7_driver.cpp already carries
// (verified equivalent to tycho_sqp's own build_subproblem in that file's
// banner). n = 5N, me = 3N, mi = N; H is block-diagonal by node, Ae is the
// collocation staircase, Ai row k reads node k's state block only, and the box
// is the control box [-1, 1] (states carry the +/-1e20 "no bound" sentinel).
//
// Those blocks do NOT depend on the family's parameter p: start_point() is
// p-independent, and eval_hess/eval_jac_e/eval_jac_i at a fixed x are too (p
// enters only the constant data the VALUES g/be/bi are built from). So a
// constructed cell is window-independent by construction, and the generator
// records window=constructed rather than pretending to a bound/path identity
// that would carry no information.
//
// The cell's VALUES g, be, bi are then constructed by KKT INVERSION:
//
//   1. pick x* (states U[-0.6, 0.6], controls U[-0.4, 0.4] — the control draw
//      keeps x* strictly interior to the [-1, 1] control box, so the box is
//      inactive at the solution by construction and the ONLY activity in the
//      cell is the inequality activity the taxonomy is about);
//   2. pick the active set A, |A| = round(active_fraction * mi), uniformly
//      without replacement;
//   3. pick multipliers: lambda_e ~ U[-1, 1] (free sign), lambda_i(j) ~
//      U[0.5, 1.5] STRICTLY POSITIVE for j in A and exactly 0 otherwise —
//      strict complementarity, so "acquired" is a sharp question;
//   4. be := Ae x*;
//   5. bi := Ai x* + s, with s(j) = 0 for j in A and s(j) > 0 otherwise (the
//      margin rule below);
//   6. g := -(H x* + Ae^T lambda_e + Ai^T lambda_i).
//
// Step 6 is exactly qp_problem.h's stationarity convention
// (grad f + Ae^T lambda_e + Ai^T lambda_i - z = 0 with grad f = Hx + g) at
// z = 0. H is positive definite (checked, see `--verify`), so the KKT point
// (x*, lambda_e, lambda_i, z = 0) is not merely stationary: x* is the UNIQUE
// global minimizer, and A is exactly its active set.
//
// ---------------------------------------------------------------------------
// THE NEAR-ACTIVITY MARGIN AXIS
// ---------------------------------------------------------------------------
//
// Row scale, per inactive row j: rs(j) = sum_k |Ai(j,k)| * |x*(k)| — the size
// of the terms the row's own value is summed from, so a margin expressed as a
// multiple of it is scale-free. Written into the dump (E1_ROWSCALE_VEC) so the
// driver's activity rule uses the SAME scale the margins were drawn in rather
// than a second, independent guess at one.
//
// The protocol asks for "the nearest decile at margin m". With M inactive
// rows, quantile position q_t = (t + 0.5)/M for t = 0..M-1, the relative
// margin assigned at that position is
//
//     rel(t) = m * 10^(3 * (q_t - 0.1))
//
// — a log-uniform spread three decades wide whose 10th percentile is EXACTLY
// m (q = 0.1 gives rel = m), with the tightest row at m*10^-0.3 ~ 0.5 m and
// the loosest at m*10^2.7. The quantile positions are then randomly permuted
// across the inactive rows, so the tight rows are not spatially clustered in
// the collocation index — a clustered layout would be a structural artifact
// the taxonomy never asked for. s(j) = rel(j) * rs(j).
//
// ---------------------------------------------------------------------------
// ANCHORS
// ---------------------------------------------------------------------------
//
// `anchor` emits the oracle's OWN cell instead: the real F7 first QP at the
// neutral cold start, values and all (no construction, no sidecar). This is
// corpus_cells.h::first_qp_for_cell's kNeutralCold branch reproduced exactly —
// F7CollocationChain(N, 3, 2, p, 1.0), p = 0.45 (bound) or 0.85 (path),
// x0 = start_point(), zero multipliers — which is what
// `tycho_sqp_corpus --dump-qp f7_nN_<window>_neutral` writes. Its true active
// set is NOT constructed and is recorded as such: the committed oracle
// artifact measured 0/mi rows active at PIQP's solution on both of these cells
// (prototypes/piqp_bridge/results/activity_diagnostics.txt), and this
// experiment re-measures it rather than assuming it.
//
// NOTHING HERE IS RANDOM ACROSS RUNS: every draw comes from a seeded
// std::mt19937_64 and every cell's seed is written into its own dump.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <Eigen/SparseCholesky>
#include <Eigen/SparseCore>
#include <Eigen/SparseQR>

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

// piqp_f7_driver.cpp's own local build_subproblem, carried verbatim: NOT
// `#include <tycho_sqp/sqp_driver.h>`, because that header drags in
// qp_engine.h -> kkt_system.h -> MKL Pardiso with no Linux opt-out, which this
// out-of-tree generator has no use for. See that file's banner for the
// equivalence evidence.
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

constexpr double kBoundArcP = 0.45;      // bench/corpus_cells.h::detail::kBoundArcP
constexpr double kPathInterfaceP = 0.85; // bench/corpus_cells.h::detail::kPathInterfaceP
constexpr Index kStates = 3;
constexpr Index kControls = 2;
constexpr double kRadius = 1.0;

F7CollocationChain make_model(long nodes, double p) {
    F7CollocationChain model(static_cast<Index>(nodes), kStates, kControls, p, kRadius);
    model.set_parameters(Vec::Constant(1, p));
    return model;
}

// ---------------------------------------------------------------------------
// THE VERIFIER. Reads back the dump (and, for a constructed cell, its sidecar)
// from DISK and checks the KKT conditions on what was actually written — so a
// serialization bug is caught by the same check that certifies the algebra.
// ---------------------------------------------------------------------------

// std::to_string is %f-with-6-decimals, which prints every residual in this
// verifier as "0.000000" -- useless for a log whose whole job is to record how
// small they actually were. This prints the number instead.
std::string fnum(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.6e", v);
    return std::string(buf);
}

std::string inum(long v) { return std::to_string(v); }

// An exact LICQ rank test is a sparse QR of [Ae; Ai_A]^T, which is affordable
// at small sizes and may or may not be at N = 20000. The ceiling is a knob
// rather than a literal so the answer can be MEASURED instead of assumed.
long licq_max_rows_setting() {
    if (const char *env = std::getenv("E1_LICQ_MAX_ROWS")) {
        return std::strtol(env, nullptr, 10);
    }
    return 5000;
}

// ---------------------------------------------------------------------------
// LICQ. Two tests, because the exact one does not scale.
//
// [Ae; Ai_A] must have full row rank at x*, or the multipliers stop being
// unique and the cell measures a constraint-qualification degeneracy instead
// of active-set acquisition.
//
//   EXACT (`licq_exact_rank`): a sparse QR of [Ae; Ai_A]^T. Truthful, and
//   superlinear. Measured end-to-end (generate + verify) on this staircase:
//   instant at 660 rows (N = 200), 11-16 s at 6020-6600 rows (N = 2000),
//   171 s at 16500 rows (N = 5000), past a 300 s budget at 33000 rows
//   (N = 10000). At N = 20000's 60200-66000 rows it cannot be paid, and this
//   file does not pretend otherwise.
//
//   CHEAP (`licq_normal_equations`): an LDL^T of J J^T, which is banded here
//   and costs almost nothing at any size in this experiment. A nonsingular
//   factor with a strictly positive |D| certifies full row rank. THE CAVEAT,
//   stated rather than buried: forming J J^T squares the condition number, so
//   this test is weaker than the QR on a nearly-deficient J -- it is a
//   numerical certificate, not an exact rank. It is the test the generator
//   gates on at full size, and both tests are run wherever both are
//   affordable so the cheap one is never the only evidence at a size where
//   the exact one could speak.
struct LicqNormal {
    bool ok = false;
    double d_min = 0.0, d_max = 0.0;
};

Eigen::SparseMatrix<double> active_jacobian(const e1::GenericQp &q,
                                            const std::vector<long> &active) {
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(q.ae.size() + q.ai.size());
    for (const e1::Triplet &tr : q.ae) {
        t.emplace_back(int(tr.row), int(tr.col), tr.value);
    }
    std::vector<long> slot(static_cast<std::size_t>(q.mi), -1);
    long extra = 0;
    for (const long j : active) {
        slot[static_cast<std::size_t>(j)] = extra++;
    }
    for (const e1::Triplet &tr : q.ai) {
        const long s = slot[static_cast<std::size_t>(tr.row)];
        if (s >= 0) {
            t.emplace_back(int(q.me + s), int(tr.col), tr.value);
        }
    }
    Eigen::SparseMatrix<double> J(q.me + extra, q.n);
    J.setFromTriplets(t.begin(), t.end());
    return J;
}

LicqNormal licq_normal_equations(const e1::GenericQp &q, const std::vector<long> &active) {
    const Eigen::SparseMatrix<double> J = active_jacobian(q, active);
    Eigen::SparseMatrix<double> G = J * J.transpose();
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    ldlt.compute(G);
    LicqNormal out;
    if (ldlt.info() != Eigen::Success) {
        return out;
    }
    const auto D = ldlt.vectorD();
    out.d_min = D.cwiseAbs().minCoeff();
    out.d_max = D.cwiseAbs().maxCoeff();
    out.ok = out.d_min > 0.0 && std::isfinite(out.d_min);
    return out;
}

long licq_exact_rank(const e1::GenericQp &q, const std::vector<long> &active) {
    Eigen::SparseMatrix<double> Jt = active_jacobian(q, active).transpose();
    Jt.makeCompressed();
    Eigen::SparseQR<Eigen::SparseMatrix<double>, Eigen::COLAMDOrdering<int>> qr;
    qr.compute(Jt);
    return qr.rank();
}

struct Check {
    bool ok = true;
    void expect(bool cond, const std::string &what, const std::string &detail) {
        std::printf("  [%s] %s  %s\n", cond ? "PASS" : "FAIL", what.c_str(), detail.c_str());
        if (!cond) {
            ok = false;
        }
    }
};

int verify_constructed(const std::string &qp_path, const std::string &sol_path) {
    const e1::GenericQp q = e1::read_dump(qp_path);
    const e1::GroundTruth gt = e1::read_solution(sol_path);
    Check c;

    std::printf("VERIFY %s  (n=%ld me=%ld mi=%ld kind=%s layout=%s offset=%ld "
                "active_fraction=%g margin_class=%g seed=%llu)\n",
                q.cell_id.c_str(), q.n, q.me, q.mi, q.kind.c_str(), q.layout.c_str(),
                q.active_offset, q.active_fraction, q.margin_class, q.seed);

    c.expect(gt.x_star.size() == q.n && gt.lambda_e.size() == q.me && gt.lambda_i.size() == q.mi,
             "sidecar shapes", "");

    // (1) Stationarity: H x* + g + Ae^T le + Ai^T li - z = 0 with z = 0.
    Vec stat = q.g;
    e1::add_h_times(q.h, gt.x_star, stat);
    e1::add_at_times(q.ae, gt.lambda_e, stat);
    e1::add_at_times(q.ai, gt.lambda_i, stat);
    const double stat_inf = stat.lpNorm<Eigen::Infinity>();
    // Scale the tolerance by the magnitude of the terms that cancel, so the
    // check is a real cancellation test rather than an absolute-zero test on
    // an O(1)-magnitude expression.
    Vec grad_only = q.g;
    e1::add_h_times(q.h, gt.x_star, grad_only);
    const double stat_scale =
        std::max({1.0, q.g.lpNorm<Eigen::Infinity>(), grad_only.lpNorm<Eigen::Infinity>()});
    c.expect(stat_inf <= 1e-12 * stat_scale, "stationarity ||Hx*+g+Ae'le+Ai'li||_inf",
             "= " + fnum(stat_inf) + " (scale " + fnum(stat_scale) + ")");

    // (2) Equality feasibility.
    Vec ce = e1::a_times(q.ae, gt.x_star, q.me) - q.be;
    const double feas_e = q.me > 0 ? ce.lpNorm<Eigen::Infinity>() : 0.0;
    c.expect(feas_e <= 1e-12 * std::max(1.0, q.be.lpNorm<Eigen::Infinity>()),
             "equality feasibility ||Ae x* - be||_inf", "= " + fnum(feas_e));

    // (3) Inequality feasibility + the exact active set + strict
    //     complementarity.
    const Vec r = e1::a_times(q.ai, gt.x_star, q.mi);
    Vec slack = q.bi - r; // >= 0 required; == 0 exactly on the active set
    std::vector<char> is_true_active(static_cast<std::size_t>(q.mi), 0);
    for (const long j : q.active_true) {
        is_true_active[static_cast<std::size_t>(j)] = 1;
    }
    double max_active_slack_abs = 0.0;
    double min_inactive_slack_rel = std::numeric_limits<double>::infinity();
    double min_active_multiplier = std::numeric_limits<double>::infinity();
    double max_inactive_multiplier = 0.0;
    double worst_violation = 0.0;
    long counted_active = 0;
    std::vector<double> inactive_rel;
    inactive_rel.reserve(static_cast<std::size_t>(q.mi));
    for (long j = 0; j < q.mi; ++j) {
        worst_violation = std::max(worst_violation, -slack(j));
        if (is_true_active[static_cast<std::size_t>(j)] != 0) {
            ++counted_active;
            max_active_slack_abs = std::max(max_active_slack_abs, std::abs(slack(j)));
            min_active_multiplier = std::min(min_active_multiplier, gt.lambda_i(j));
        } else {
            const double rel = slack(j) / q.row_scale(j);
            inactive_rel.push_back(rel);
            min_inactive_slack_rel = std::min(min_inactive_slack_rel, rel);
            max_inactive_multiplier = std::max(max_inactive_multiplier, std::abs(gt.lambda_i(j)));
        }
    }
    const long want_active = static_cast<long>(std::llround(q.active_fraction * double(q.mi)));
    c.expect(counted_active == want_active && counted_active == long(q.active_true.size()),
             "|A| matches round(active_fraction * mi)",
             "= " + inum(counted_active) + " (want " + inum(want_active) + ")");
    c.expect(worst_violation <= 1e-12 * std::max(1.0, q.bi.lpNorm<Eigen::Infinity>()),
             "inequality feasibility max(Ai x* - bi)", "= " + fnum(-worst_violation));
    c.expect(max_active_slack_abs <= 1e-14 * std::max(1.0, q.bi.lpNorm<Eigen::Infinity>()),
             "active rows hold with EQUALITY", "max|slack| = " + fnum(max_active_slack_abs));
    c.expect(min_inactive_slack_rel > 0.0, "inactive rows have STRICTLY positive slack",
             "min rel slack = " + fnum(min_inactive_slack_rel));
    c.expect(min_active_multiplier > 0.0, "active multipliers STRICTLY positive",
             "min = " + fnum(min_active_multiplier));
    c.expect(max_inactive_multiplier == 0.0, "inactive multipliers exactly zero",
             "max = " + fnum(max_inactive_multiplier));
    double comp = 0.0;
    for (long j = 0; j < q.mi; ++j) {
        comp = std::max(comp, std::abs(gt.lambda_i(j) * slack(j)));
    }
    c.expect(comp <= 1e-14 * std::max(1.0, q.bi.lpNorm<Eigen::Infinity>()),
             "complementarity max|li_j * slack_j|", "= " + fnum(comp));

    // (4) The margin decile actually sits where the taxonomy says.
    std::sort(inactive_rel.begin(), inactive_rel.end());
    const std::size_t decile_idx = static_cast<std::size_t>(0.1 * double(inactive_rel.size()));
    const double decile = inactive_rel.empty() ? -1.0 : inactive_rel[decile_idx];
    c.expect(inactive_rel.empty() ||
                 (decile >= 0.5 * q.margin_class && decile <= 2.0 * q.margin_class),
             "10th-percentile relative margin == margin_class (within 2x)",
             "decile = " + fnum(decile) + " vs " + fnum(q.margin_class));

    // (5) The box is inactive at x*, which is what makes z = 0 admissible.
    double min_box_slack = std::numeric_limits<double>::infinity();
    for (long i = 0; i < q.n; ++i) {
        min_box_slack = std::min(min_box_slack, gt.x_star(i) - q.lower(i));
        min_box_slack = std::min(min_box_slack, q.upper(i) - gt.x_star(i));
    }
    c.expect(min_box_slack > 1e-3, "box strictly inactive at x* (so z = 0 is admissible)",
             "min box slack = " + fnum(min_box_slack));

    // (6) H positive definite => the KKT point is the UNIQUE global minimizer.
    {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(q.h.size() * 2);
        for (const e1::Triplet &tr : q.h) {
            t.emplace_back(int(tr.row), int(tr.col), tr.value);
            if (tr.row != tr.col) {
                t.emplace_back(int(tr.col), int(tr.row), tr.value);
            }
        }
        Eigen::SparseMatrix<double> H(q.n, q.n);
        H.setFromTriplets(t.begin(), t.end());
        Eigen::SimplicialLLT<Eigen::SparseMatrix<double>> llt;
        llt.compute(H);
        c.expect(llt.info() == Eigen::Success, "H is positive definite (sparse Cholesky succeeds)",
                 "");
    }

    // (7) LICQ at x*: [Ae; Ai_A] full row rank. See the two tests' banner above
    //     for why there are two and what each is worth.
    {
        const long licq_rows = q.me + long(q.active_true.size());
        const LicqNormal ne = licq_normal_equations(q, q.active_true);
        c.expect(ne.ok,
                 "LICQ (cheap, every size): LDL^T of [Ae;Ai_A][Ae;Ai_A]^T is nonsingular",
                 "min|D| = " + fnum(ne.d_min) + ", max|D| = " + fnum(ne.d_max) + " over " +
                     inum(licq_rows) + " rows");
        const long licq_max_rows = licq_max_rows_setting();
        if (licq_rows <= licq_max_rows) {
            const long rank = licq_exact_rank(q, q.active_true);
            c.expect(rank == licq_rows, "LICQ (exact): [Ae; Ai_A] has full row rank",
                     "rank " + inum(rank) + " of " + inum(licq_rows));
        } else {
            std::printf("  [SKIP] LICQ exact rank check (%ld rows > E1_LICQ_MAX_ROWS = %ld; a "
                        "sparse QR of this staircase is superlinear -- 11-16 s at 6600 rows, "
                        "171 s at 16500, past 300 s at 33000 -- so the cheap certificate above "
                        "is what stands at this size, cross-validated against the exact test "
                        "wherever the exact test was affordable)\n",
                        licq_rows, licq_max_rows);
        }
    }

    std::printf("VERIFY %s: %s\n\n", q.cell_id.c_str(), c.ok ? "ALL CHECKS PASS" : "*** FAILED ***");
    return c.ok ? 0 : 1;
}

// ---------------------------------------------------------------------------
// GENERATION
// ---------------------------------------------------------------------------

enum class Layout { kScattered, kContiguous };

int generate_cell(const std::string &id, long nodes, double active_fraction, double margin,
                  unsigned long long seed, const std::string &out_prefix,
                  Layout layout = Layout::kScattered) {
    F7CollocationChain model = make_model(nodes, kBoundArcP);
    const Vec x0 = model.start_point();
    const QpProblem qp0 =
        build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    e1::GenericQp q = from_qp_problem(qp0);
    q.cell_id = id;
    q.taxonomy = "neutral";
    q.window = "constructed";
    q.n_nodes = nodes;
    q.kind = "constructed";
    q.layout = layout == Layout::kContiguous ? "contiguous" : "scattered";
    q.active_fraction = active_fraction;
    q.margin_class = margin;
    q.seed = seed;

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> u_state(-0.6, 0.6);
    std::uniform_real_distribution<double> u_ctrl(-0.4, 0.4);
    std::uniform_real_distribution<double> u_le(-1.0, 1.0);
    std::uniform_real_distribution<double> u_li(0.5, 1.5);

    // x*: state coordinates first, then control coordinates, node by node --
    // the control draw stays strictly inside the [-1, 1] control box.
    Vec x_star(q.n);
    for (long k = 0; k < nodes; ++k) {
        const long b = k * (kStates + kControls);
        for (long i = 0; i < kStates; ++i) {
            x_star(b + i) = u_state(rng);
        }
        for (long j = 0; j < kControls; ++j) {
            x_star(b + kStates + j) = u_ctrl(rng);
        }
    }

    // The active set: a uniform sample without replacement from rows 1..mi-1.
    //
    // ROW 0 IS EXCLUDED, and the exclusion is structural, not cosmetic. F7's
    // path row k reads node k's STATE block only, and node 0's state block is
    // pinned outright by the family's three initial-condition equality rows
    // (Ae's first block is the identity on y_0). So row 0, if activated, lies
    // in the row space of Ae and [Ae; Ai_A] loses rank: LICQ fails, the
    // multipliers at x* stop being unique, and the cell would be measuring a
    // constraint-qualification degeneracy rather than active-set ACQUISITION.
    // scale_problems.h's own banner names this same degeneracy at p >= R
    // ("node 0's path row goes active while node 0's state is pinned by the
    // initial-condition rows, so LICQ fails and K0 is exactly singular"). It
    // was caught here by the LICQ check below, which reported rank 619 of 620
    // on the first draft of this generator -- exactly one row short, exactly
    // row 0. Excluding it costs the taxonomy nothing: |A| is unchanged, and
    // row 0 simply joins the inactive rows and draws a margin like any other.
    const long n_active = static_cast<long>(std::llround(active_fraction * double(q.mi)));
    if (n_active > q.mi - 1) {
        std::fprintf(stderr, "e1_generate: active_fraction %g would need %ld of %ld rows, but "
                             "row 0 is structurally excluded (LICQ)\n",
                     active_fraction, n_active, q.mi);
        return 1;
    }
    std::vector<long> active, inactive;
    if (layout == Layout::kScattered) {
        std::vector<long> rows(static_cast<std::size_t>(q.mi - 1));
        std::iota(rows.begin(), rows.end(), 1L);
        std::shuffle(rows.begin(), rows.end(), rng);
        active.assign(rows.begin(), rows.begin() + n_active);
        inactive.assign(rows.begin() + n_active, rows.end());
        inactive.push_back(0); // the structurally excluded row is simply inactive
        std::sort(active.begin(), active.end());
    } else {
        // THE CONTIGUOUS (JUNCTION-WINDOW) LAYOUT. The active set is one solid
        // block of consecutive inequality rows [o, o + k), the shape F7's own
        // geometry produces between its two junctions -- as against the
        // scattered layout's uniform random subset. Row 0 stays excluded for
        // the same structural reason (its state block is pinned by the
        // initial-condition equality rows), so the block is drawn inside
        // [1, mi - 1]: the offset o is uniform on [1, mi - k].
        //
        // THE OFFSET SHIFT RULE, applied even though it is not expected to
        // fire: a contiguous block interacts with the collocation staircase
        // differently from a scattered one, so LICQ is RE-TESTED at the drawn
        // offset rather than assumed from the scattered case. If the cheap
        // full-size LICQ certificate fails at o, the offset is shifted by +1
        // (wrapping within [1, mi - k]) and re-tested, up to every admissible
        // offset. If NO offset admits LICQ, the cell is recorded as
        // INFEASIBLE-BY-CONSTRUCTION and no row is fabricated for it.
        std::uniform_int_distribution<long> u_off(1, q.mi - n_active);
        const long o0 = u_off(rng);
        long chosen = -1;
        const long span = q.mi - n_active; // admissible offsets are 1..span
        for (long attempt = 0; attempt < span; ++attempt) {
            const long o = 1 + ((o0 - 1 + attempt) % span);
            std::vector<long> trial(static_cast<std::size_t>(n_active));
            std::iota(trial.begin(), trial.end(), o);
            const LicqNormal ne = licq_normal_equations(q, trial);
            if (ne.ok) {
                if (attempt > 0) {
                    std::printf("  offset shift: drawn offset %ld failed the LICQ certificate; "
                                "shifted %ld to offset %ld (min|D| = %.6e)\n",
                                o0, attempt, o, ne.d_min);
                }
                chosen = o;
                active = trial;
                break;
            }
            std::printf("  offset %ld REJECTED by the LICQ certificate (min|D| = %.6e, "
                        "factorization %s)\n",
                        o, ne.d_min, ne.ok ? "succeeded" : "failed or singular");
        }
        if (chosen < 0) {
            std::fprintf(stderr,
                         "e1_generate: %s is INFEASIBLE BY CONSTRUCTION: no offset in [1, %ld] "
                         "admits LICQ for a contiguous block of %ld rows. No cell written.\n",
                         id.c_str(), span, n_active);
            return 2;
        }
        q.active_offset = chosen;
        std::vector<char> is_active(static_cast<std::size_t>(q.mi), 0);
        for (const long j : active) {
            is_active[static_cast<std::size_t>(j)] = 1;
        }
        for (long j = 0; j < q.mi; ++j) {
            if (is_active[static_cast<std::size_t>(j)] == 0) {
                inactive.push_back(j);
            }
        }
    }
    q.active_true = active;

    Vec lambda_e(q.me);
    for (long i = 0; i < q.me; ++i) {
        lambda_e(i) = u_le(rng);
    }
    Vec lambda_i = Vec::Zero(q.mi);
    for (const long j : active) {
        lambda_i(j) = u_li(rng);
    }

    // Row scales and the margin ladder.
    q.row_scale = Vec::Zero(q.mi);
    for (const e1::Triplet &tr : q.ai) {
        q.row_scale(tr.row) += std::abs(tr.value) * std::abs(x_star(tr.col));
    }
    for (long j = 0; j < q.mi; ++j) {
        if (!(q.row_scale(j) > 0.0)) {
            std::fprintf(stderr, "e1_generate: row %ld has a zero row scale -- a relative margin "
                                 "on it would be meaningless. Aborting rather than fabricating "
                                 "one.\n",
                         j);
            return 1;
        }
    }

    const Vec r = e1::a_times(q.ai, x_star, q.mi);
    Vec bi = r;
    const std::size_t M = inactive.size();
    // Quantile positions, permuted across the inactive rows so the tight rows
    // are not clustered in the collocation index.
    std::vector<std::size_t> pos(M);
    std::iota(pos.begin(), pos.end(), std::size_t(0));
    std::shuffle(pos.begin(), pos.end(), rng);
    for (std::size_t t = 0; t < M; ++t) {
        const double qpos = (double(pos[t]) + 0.5) / double(M);
        const double rel = margin * std::pow(10.0, 3.0 * (qpos - 0.1));
        const long j = inactive[t];
        bi(j) = r(j) + rel * q.row_scale(j);
    }
    q.bi = bi;
    q.be = e1::a_times(q.ae, x_star, q.me);

    Vec g = Vec::Zero(q.n);
    e1::add_h_times(q.h, x_star, g);
    e1::add_at_times(q.ae, lambda_e, g);
    e1::add_at_times(q.ai, lambda_i, g);
    q.g = -g;

    const std::string qp_path = out_prefix + ".qp";
    const std::string sol_path = out_prefix + ".sol";
    e1::write_dump(qp_path, q,
                   std::string("e1_generate ") +
                       (layout == Layout::kContiguous ? "cellblk" : "cell") +
                       ": F7-pattern structure (H/Ae/Ai/box from F7CollocationChain's "
                       "neutral-cold first QP), g/be/bi by KKT inversion, active set laid out " +
                       q.layout);
    e1::GroundTruth gt;
    gt.x_star = x_star;
    gt.lambda_e = lambda_e;
    gt.lambda_i = lambda_i;
    e1::write_solution(sol_path, gt);
    std::printf("generated %s: n=%ld me=%ld mi=%ld |A|=%ld (%.4g) margin=%g layout=%s "
                "offset=%ld seed=%llu -> %s\n",
                id.c_str(), q.n, q.me, q.mi, n_active, active_fraction, margin, q.layout.c_str(),
                q.active_offset, seed, qp_path.c_str());
    return verify_constructed(qp_path, sol_path);
}

int generate_anchor(const std::string &id, long nodes, const std::string &window,
                    const std::string &out_prefix) {
    double p = 0.0;
    if (window == "bound") {
        p = kBoundArcP;
    } else if (window == "path") {
        p = kPathInterfaceP;
    } else {
        std::fprintf(stderr, "e1_generate: anchor window must be 'bound' or 'path'\n");
        return 1;
    }
    F7CollocationChain model = make_model(nodes, p);
    const Vec x0 = model.start_point();
    const QpProblem qp0 =
        build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    e1::GenericQp q = from_qp_problem(qp0);
    q.cell_id = id;
    q.taxonomy = "neutral";
    q.window = window;
    q.n_nodes = nodes;
    q.kind = "anchor";
    q.active_fraction = 0.0;
    q.margin_class = -1.0;
    q.seed = 0;
    q.active_true.clear(); // measured 0/mi at the oracle; re-measured here
    q.row_scale = Vec::Zero(q.mi);
    for (const e1::Triplet &tr : q.ai) {
        q.row_scale(tr.row) += std::abs(tr.value) * std::abs(x0(tr.col));
    }
    for (long j = 0; j < q.mi; ++j) {
        if (!(q.row_scale(j) > 0.0)) {
            q.row_scale(j) = 1.0; // an anchor's rule is reported, never gated
        }
    }
    const std::string qp_path = out_prefix + ".qp";
    e1::write_dump(qp_path, q,
                   "e1_generate anchor: the ORACLE's own cell -- F7CollocationChain neutral-cold "
                   "first QP, values and all (corpus_cells.h first_qp_for_cell/kNeutralCold)");
    std::printf("generated %s (anchor, window=%s, p=%g): n=%ld me=%ld mi=%ld -> %s\n", id.c_str(),
                window.c_str(), p, q.n, q.me, q.mi, qp_path.c_str());
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc < 2) {
            std::fprintf(stderr,
                         "usage: e1_generate cell <id> <nodes> <active_fraction> <margin> <seed> "
                         "<out-prefix>\n"
                         "       e1_generate cellblk <id> <nodes> <active_fraction> <margin> "
                         "<seed> <out-prefix>   (contiguous block layout)\n"
                         "       e1_generate anchor <id> <nodes> <bound|path> <out-prefix>\n"
                         "       e1_generate verify <qp-path> <sol-path>\n");
            return 1;
        }
        const std::string mode = argv[1];
        if (mode == "cell") {
            if (argc != 8) {
                std::fprintf(stderr, "e1_generate cell: wrong argument count\n");
                return 1;
            }
            return generate_cell(argv[2], std::stol(argv[3]), std::stod(argv[4]),
                                 std::stod(argv[5]), std::stoull(argv[6]), argv[7]);
        }
        if (mode == "cellblk") {
            if (argc != 8) {
                std::fprintf(stderr, "e1_generate cellblk: wrong argument count\n");
                return 1;
            }
            return generate_cell(argv[2], std::stol(argv[3]), std::stod(argv[4]),
                                 std::stod(argv[5]), std::stoull(argv[6]), argv[7],
                                 Layout::kContiguous);
        }
        if (mode == "anchor") {
            if (argc != 6) {
                std::fprintf(stderr, "e1_generate anchor: wrong argument count\n");
                return 1;
            }
            return generate_anchor(argv[2], std::stol(argv[3]), argv[4], argv[5]);
        }
        if (mode == "verify") {
            if (argc != 4) {
                std::fprintf(stderr, "e1_generate verify: wrong argument count\n");
                return 1;
            }
            return verify_constructed(argv[2], argv[3]);
        }
        std::fprintf(stderr, "e1_generate: unknown mode '%s'\n", mode.c_str());
        return 1;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "e1_generate: error: %s\n", e.what());
        return 1;
    }
}
