// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_e1_arm.h -- the E1 taxonomy's 29 cells, rebuilt in tree, and the IPQP
// tier's solve arm over them (spec section 8.1 A4).
// Recipe source: docs/notes/data/2026-08-m6-e1-acquisition/generator/e1_generate.cpp.

// THE REPRODUCTION IS DRAW-FOR-DRAW, not merely recipe-for-recipe: the same
// mt19937_64 seeds, the same distribution objects in the same order, the same
// vector orderings. `certify()` below is what proves it against the artifact.

// The generator itself cannot be rebuilt here -- it compiles against the
// origin project's headers and its dumps were deleted by the sweep script --
// so the identity evidence is its recorded KKT-VERIFICATION log, not a diff.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <Eigen/SparseCholesky>
#include <Eigen/SparseCore>

#include <fmt/format.h>

#include <hven/core/solver_counters.h>
#include <hven/core/types.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_driver.h>

#include "support/e1_cells.h"
#include "support/scale_problems.h"

namespace hven::solvers::e1arm {

using test_support::F7CollocationChain;

/// E1's own family constants, at the two values `run_sweep.sh` ran the anchors
/// at (`e1_generate.cpp`'s `kBoundArcP` / `kPathInterfaceP`).
inline constexpr double kBoundArcP = 0.45;
inline constexpr double kPathInterfaceP = 0.85;
inline constexpr Index kStates = 3;
inline constexpr Index kControls = 2;
inline constexpr double kRadius = 1.0;

/// E1's pre-registered gate: fewer than 40 tier iterations per cell.
inline constexpr Index kIterGate = 40;

enum class Layout { kScattered, kContiguous, kAnchor };

/// One taxonomy entry. `seed` and `nodes` are the sweep scripts' own values;
/// an anchor carries no construction and reads `anchor_p` instead.
struct CellSpec {
    std::string id;
    Index nodes = 20000;
    double active_fraction = 0.0;
    double margin = -1.0;
    std::uint64_t seed = 0;
    Layout layout = Layout::kScattered;
    double anchor_p = kBoundArcP;
};

/// A built cell plus the ground truth the solver is never told.
struct Cell {
    CellSpec spec;
    QpProblem qp;
    Vec x_star, lambda_e_star, lambda_i_star;
    std::vector<Index> active;
    Vec row_scale;
    Index active_offset = -1;
};

namespace detail {

using test_support::e1_detail::a_times;
using test_support::e1_detail::add_at_times;
using test_support::e1_detail::add_h_times;

inline F7CollocationChain make_model(Index nodes, double p) {
    F7CollocationChain model(nodes, kStates, kControls, p, kRadius);
    model.set_parameters(Vec::Constant(1, p));
    return model;
}

/// `[Ae; Ai_A]` with the active rows appended in the order given, which is the
/// order the generator's `active_jacobian` builds them in.
inline Eigen::SparseMatrix<double> active_jacobian(const QpProblem &qp,
                                                   const std::vector<Index> &active) {
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(qp.Ae.nonZeros() + qp.Ai.nonZeros()));
    for (Index r = 0; r < qp.Ae.rows(); ++r) {
        for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
            t.emplace_back(static_cast<int>(r), static_cast<int>(it.col()), it.value());
        }
    }
    const Index me = qp.Ae.rows();
    std::vector<Index> slot(static_cast<std::size_t>(qp.Ai.rows()), -1);
    Index extra = 0;
    for (const Index j : active) {
        slot[static_cast<std::size_t>(j)] = extra++;
    }
    for (Index r = 0; r < qp.Ai.rows(); ++r) {
        const Index s = slot[static_cast<std::size_t>(r)];
        if (s < 0) {
            continue;
        }
        for (SpMatRM::InnerIterator it(qp.Ai, r); it; ++it) {
            t.emplace_back(static_cast<int>(me + s), static_cast<int>(it.col()), it.value());
        }
    }
    Eigen::SparseMatrix<double> j(me + extra, qp.g.size());
    j.setFromTriplets(t.begin(), t.end());
    return j;
}

struct LicqNormal {
    bool ok = false;
    double d_min = 0.0, d_max = 0.0;
};

/// The generator's CHEAP LICQ certificate: an LDL^T of `J J'`. Squaring the
/// condition number makes it weaker than a rank-revealing QR, which is why the
/// generator ran both wherever the exact test was affordable.
inline LicqNormal licq_normal_equations(const QpProblem &qp, const std::vector<Index> &active) {
    const Eigen::SparseMatrix<double> j = active_jacobian(qp, active);
    const Eigen::SparseMatrix<double> g = j * j.transpose();
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> ldlt;
    ldlt.compute(g);
    LicqNormal out;
    if (ldlt.info() != Eigen::Success) {
        return out;
    }
    const auto d = ldlt.vectorD();
    out.d_min = d.cwiseAbs().minCoeff();
    out.d_max = d.cwiseAbs().maxCoeff();
    out.ok = out.d_min > 0.0 && std::isfinite(out.d_min);
    return out;
}

} // namespace detail

/// The 29 cells: 18 scattered + 2 anchors (`run_sweep.sh`) then 9 contiguous
/// (`run_variant_contiguous.sh`), each with that script's own seed arithmetic.
inline std::vector<CellSpec> taxonomy() {
    const double afs[3] = {0.01, 0.10, 0.30};
    const char *mtags[3] = {"1e-2", "1e-4", "1e-6"};
    const double margins[3] = {1e-2, 1e-4, 1e-6};
    const int aftags[3] = {1, 10, 30};
    std::vector<CellSpec> out;
    int idx = 0;
    for (const Index nodes : {Index(4000), Index(20000)}) {
        for (int a = 0; a < 3; ++a) {
            for (int m = 0; m < 3; ++m) {
                ++idx;
                CellSpec s;
                s.id = fmt::format("e1_f7_n{}_af{:02d}_m{}", nodes, aftags[a], mtags[m]);
                s.nodes = nodes;
                s.active_fraction = afs[a];
                s.margin = margins[m];
                s.seed = static_cast<std::uint64_t>(20260826 + idx);
                s.layout = Layout::kScattered;
                out.push_back(s);
            }
        }
    }
    for (const char *win : {"bound", "path"}) {
        CellSpec s;
        s.id = fmt::format("e1_anchor_f7_n20000_{}_neutral", win);
        s.layout = Layout::kAnchor;
        s.anchor_p = std::string(win) == "bound" ? kBoundArcP : kPathInterfaceP;
        out.push_back(s);
    }
    int sidx = 0;
    for (int a = 0; a < 3; ++a) {
        for (int m = 0; m < 3; ++m) {
            ++sidx;
            CellSpec s;
            s.id = fmt::format("e1blk_f7_n20000_af{:02d}_m{}", aftags[a], mtags[m]);
            s.active_fraction = afs[a];
            s.margin = margins[m];
            s.seed = static_cast<std::uint64_t>(20260827 + sidx);
            s.layout = Layout::kContiguous;
            out.push_back(s);
        }
    }
    return out;
}

/// Builds one cell. Throws `std::invalid_argument` when no offset admits LICQ
/// (the generator's "infeasible by construction" outcome, which never fired).
inline Cell build(const CellSpec &spec) {
    Cell cell;
    cell.spec = spec;
    const double p = spec.layout == Layout::kAnchor ? spec.anchor_p : kBoundArcP;
    F7CollocationChain model = detail::make_model(spec.nodes, p);
    const Vec x0 = model.start_point();
    cell.qp = build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    if (spec.layout == Layout::kAnchor) {
        return cell;
    }

    const Index n = cell.qp.g.size();
    const Index me = cell.qp.be.size();
    const Index mi = cell.qp.bi.size();
    std::mt19937_64 rng(spec.seed);
    std::uniform_real_distribution<double> u_state(-0.6, 0.6);
    std::uniform_real_distribution<double> u_ctrl(-0.4, 0.4);
    std::uniform_real_distribution<double> u_le(-1.0, 1.0);
    std::uniform_real_distribution<double> u_li(0.5, 1.5);

    Vec x_star(n);
    for (Index k = 0; k < spec.nodes; ++k) {
        const Index b = k * (kStates + kControls);
        for (Index i = 0; i < kStates; ++i) {
            x_star(b + i) = u_state(rng);
        }
        for (Index j = 0; j < kControls; ++j) {
            x_star(b + kStates + j) = u_ctrl(rng);
        }
    }

    // ROW 0 IS STRUCTURALLY EXCLUDED: node 0's state block is pinned by the
    // initial-condition equality rows, so activating its path row breaks LICQ.
    const auto n_active = static_cast<Index>(std::llround(spec.active_fraction * double(mi)));
    std::vector<Index> active, inactive;
    if (spec.layout == Layout::kScattered) {
        std::vector<Index> rows(static_cast<std::size_t>(mi - 1));
        std::iota(rows.begin(), rows.end(), Index(1));
        std::shuffle(rows.begin(), rows.end(), rng);
        active.assign(rows.begin(), rows.begin() + n_active);
        inactive.assign(rows.begin() + n_active, rows.end());
        inactive.push_back(0);
        std::sort(active.begin(), active.end());
    } else {
        std::uniform_int_distribution<Index> u_off(1, mi - n_active);
        const Index o0 = u_off(rng);
        const Index span = mi - n_active;
        Index chosen = -1;
        for (Index attempt = 0; attempt < span; ++attempt) {
            const Index o = 1 + ((o0 - 1 + attempt) % span);
            std::vector<Index> trial(static_cast<std::size_t>(n_active));
            std::iota(trial.begin(), trial.end(), o);
            if (detail::licq_normal_equations(cell.qp, trial).ok) {
                chosen = o;
                active = trial;
                break;
            }
        }
        if (chosen < 0) {
            throw std::invalid_argument(
                fmt::format("e1arm::build('{}'): no offset in [1, {}] admits LICQ for a "
                            "contiguous block of {} rows",
                            spec.id, span, n_active));
        }
        cell.active_offset = chosen;
        std::vector<char> is_active(static_cast<std::size_t>(mi), 0);
        for (const Index j : active) {
            is_active[static_cast<std::size_t>(j)] = 1;
        }
        for (Index j = 0; j < mi; ++j) {
            if (is_active[static_cast<std::size_t>(j)] == 0) {
                inactive.push_back(j);
            }
        }
    }

    Vec lambda_e(me);
    for (Index i = 0; i < me; ++i) {
        lambda_e(i) = u_le(rng);
    }
    Vec lambda_i = Vec::Zero(mi);
    for (const Index j : active) {
        lambda_i(j) = u_li(rng);
    }

    cell.row_scale = Vec::Zero(mi);
    for (Index r = 0; r < cell.qp.Ai.rows(); ++r) {
        for (SpMatRM::InnerIterator it(cell.qp.Ai, r); it; ++it) {
            cell.row_scale(r) += std::abs(it.value()) * std::abs(x_star(it.col()));
        }
    }

    // The margin ladder: a three-decade log-uniform spread whose 10th
    // percentile is exactly `margin`, permuted so the tight rows do not
    // cluster in the collocation index.
    const Vec r_star = detail::a_times(cell.qp.Ai, x_star);
    Vec bi = r_star;
    const std::size_t count = inactive.size();
    std::vector<std::size_t> pos(count);
    std::iota(pos.begin(), pos.end(), std::size_t(0));
    std::shuffle(pos.begin(), pos.end(), rng);
    for (std::size_t t = 0; t < count; ++t) {
        const double qpos = (static_cast<double>(pos[t]) + 0.5) / static_cast<double>(count);
        const double rel = spec.margin * std::pow(10.0, 3.0 * (qpos - 0.1));
        const Index j = inactive[t];
        bi(j) = r_star(j) + rel * cell.row_scale(j);
    }
    cell.qp.bi = bi;
    cell.qp.be = detail::a_times(cell.qp.Ae, x_star);

    Vec g = Vec::Zero(n);
    detail::add_h_times(cell.qp.H, x_star, g);
    detail::add_at_times(cell.qp.Ae, lambda_e, g);
    detail::add_at_times(cell.qp.Ai, lambda_i, g);
    cell.qp.g = -g;

    cell.x_star = x_star;
    cell.lambda_e_star = lambda_e;
    cell.lambda_i_star = lambda_i;
    cell.active = active;
    return cell;
}

/// The seven quantities the generator's own verifier printed per cell, plus
/// the contiguous offset. Each is compared against the recorded log, so a
/// drifted draw sequence shows up as a differing digit rather than silently.
struct RegenCertificate {
    double stat_inf = 0.0, stat_scale = 0.0;
    double min_inactive_slack_rel = 0.0, min_active_multiplier = 0.0;
    double decile = -1.0, min_box_slack = 0.0;
    double licq_d_min = 0.0, licq_d_max = 0.0;
    Index active_offset = -1;
};

inline RegenCertificate certify(const Cell &cell) {
    RegenCertificate out;
    out.active_offset = cell.active_offset;
    const Index n = cell.qp.g.size();
    const Index mi = cell.qp.bi.size();

    Vec stat = cell.qp.g;
    detail::add_h_times(cell.qp.H, cell.x_star, stat);
    detail::add_at_times(cell.qp.Ae, cell.lambda_e_star, stat);
    detail::add_at_times(cell.qp.Ai, cell.lambda_i_star, stat);
    out.stat_inf = stat.lpNorm<Eigen::Infinity>();
    Vec grad_only = cell.qp.g;
    detail::add_h_times(cell.qp.H, cell.x_star, grad_only);
    out.stat_scale =
        std::max({1.0, cell.qp.g.lpNorm<Eigen::Infinity>(), grad_only.lpNorm<Eigen::Infinity>()});

    const Vec r = detail::a_times(cell.qp.Ai, cell.x_star);
    const Vec slack = cell.qp.bi - r;
    std::vector<char> truth(static_cast<std::size_t>(mi), 0);
    for (const Index j : cell.active) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    out.min_inactive_slack_rel = std::numeric_limits<double>::infinity();
    out.min_active_multiplier = std::numeric_limits<double>::infinity();
    std::vector<double> inactive_rel;
    inactive_rel.reserve(static_cast<std::size_t>(mi));
    for (Index j = 0; j < mi; ++j) {
        if (truth[static_cast<std::size_t>(j)] != 0) {
            out.min_active_multiplier = std::min(out.min_active_multiplier, cell.lambda_i_star(j));
            continue;
        }
        const double rel = slack(j) / cell.row_scale(j);
        inactive_rel.push_back(rel);
        out.min_inactive_slack_rel = std::min(out.min_inactive_slack_rel, rel);
    }
    std::sort(inactive_rel.begin(), inactive_rel.end());
    const auto decile_idx = static_cast<std::size_t>(0.1 * double(inactive_rel.size()));
    out.decile = inactive_rel.empty() ? -1.0 : inactive_rel[decile_idx];

    out.min_box_slack = std::numeric_limits<double>::infinity();
    for (Index i = 0; i < n; ++i) {
        out.min_box_slack = std::min(out.min_box_slack, cell.x_star(i) - cell.qp.lower(i));
        out.min_box_slack = std::min(out.min_box_slack, cell.qp.upper(i) - cell.x_star(i));
    }

    const detail::LicqNormal ne = detail::licq_normal_equations(cell.qp, cell.active);
    out.licq_d_min = ne.d_min;
    out.licq_d_max = ne.d_max;
    return out;
}

/// One A4 row: the tier's own verdict on one cell.
struct SolveRow {
    std::string id;
    Index n = 0, me = 0, mi = 0;
    QpStatus status = QpStatus::kOptimal;
    IpqpCounters counters;
    Index active_true = 0, active_found = 0;
    Index misclassified = 0, uncertain = 0;
    Index rule_a = 0, rule_b = 0, rule_a_missed = 0, rule_a_false_positive = 0;
    double res_primal = 0.0, res_dual = 0.0, res_comp = 0.0, x_err_inf = -1.0;
    double wall_s = 0.0;
};

/// Solves one cell with the shipped tier and reads its face back against the
/// ground truth. `tr_radius = inf` is the test bed's own unbounded-step
/// configuration (`test_ipqp_acceptance.cpp`'s `tight_opts`).
inline SolveRow solve(const Cell &cell, const IpqpOptions &iopts) {
    SolveRow row;
    row.id = cell.spec.id;
    row.n = cell.qp.g.size();
    row.me = cell.qp.be.size();
    row.mi = cell.qp.bi.size();
    row.active_true = static_cast<Index>(cell.active.size());

    QpOptions qopts;
    qopts.tr_radius = std::numeric_limits<double>::infinity();
    IpqpEngine tier(qopts);
    const auto t0 = std::chrono::steady_clock::now();
    const IpqpResult r = tier.solve(cell.qp, nullptr, iopts, SolveOverrides{});
    row.wall_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    row.status = r.status;
    row.counters = r.counters;
    std::vector<char> truth(static_cast<std::size_t>(row.mi), 0);
    for (const Index j : cell.active) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    for (Index j = 0; j < row.mi; ++j) {
        const IpqpFace got = r.ineq_face[static_cast<std::size_t>(j)];
        if (got == IpqpFace::kActive) {
            ++row.active_found;
        }
        if (got == IpqpFace::kUncertain) {
            ++row.uncertain;
            continue;
        }
        const IpqpFace want =
            truth[static_cast<std::size_t>(j)] == 1 ? IpqpFace::kActive : IpqpFace::kInactive;
        if (got != want) {
            ++row.misclassified;
        }
    }

    const Vec ax = detail::a_times(cell.qp.Ai, r.x);
    const Vec aex = detail::a_times(cell.qp.Ae, r.x);
    row.res_primal = std::max(row.me > 0 ? (aex - cell.qp.be).lpNorm<Eigen::Infinity>() : 0.0,
                              (ax - cell.qp.bi).cwiseMax(0.0).lpNorm<Eigen::Infinity>());
    Vec stat = cell.qp.g;
    detail::add_h_times(cell.qp.H, r.x, stat);
    detail::add_at_times(cell.qp.Ae, r.lambda_e, stat);
    detail::add_at_times(cell.qp.Ai, r.lambda_i, stat);
    stat -= r.zl;
    stat += r.zu;
    row.res_dual = stat.lpNorm<Eigen::Infinity>();
    row.res_comp = r.mu;
    if (cell.x_star.size() == r.x.size()) {
        row.x_err_inf = (r.x - cell.x_star).lpNorm<Eigen::Infinity>();
    }

    // E1's own two acquired-set rules, REPORTED beside the ratio rule so the
    // A4 table is comparable with the artifact's CSV columns. Rule A's
    // absolute 1e-8 threshold is the artifact the ratio rule exists to retire.
    if (cell.row_scale.size() == row.mi) {
        double dual_scale = 1.0;
        if (r.lambda_i.size() > 0) {
            dual_scale = std::max(1.0, r.lambda_i.lpNorm<Eigen::Infinity>());
        }
        for (Index j = 0; j < row.mi; ++j) {
            const bool a = (cell.qp.bi(j) - ax(j)) <= 1e-8 * cell.row_scale(j);
            const bool b = r.lambda_i(j) >= 1e-6 * dual_scale;
            row.rule_a += a ? 1 : 0;
            row.rule_b += b ? 1 : 0;
            const bool t = truth[static_cast<std::size_t>(j)] == 1;
            row.rule_a_false_positive += (a && !t) ? 1 : 0;
            row.rule_a_missed += (!a && t) ? 1 : 0;
        }
    }
    return row;
}

} // namespace hven::solvers::e1arm
