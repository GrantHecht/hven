// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// E1's constructed-cell recipe, in tree: F7's own sparsity, a chosen `x*` and
// active set, then `g`/`be`/`bi` BY KKT INVERSION.
// See docs/notes/data/2026-08-m6-e1-acquisition/generator/e1_generate.cpp:453.

// T9 adds two things E1 did not have: placement at F7's OWN junction indices
// (A1) and simultaneous BOUND activity (A3), which E1 excluded by
// construction. Reproduced rather than linked -- the generator writes files.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_driver.h>

#include "scale_problems.h"

namespace hven::solvers::test_support {

/// E1's own bound-arc parameter: the pattern every constructed cell is cut
/// from (`e1_generate.cpp`'s `kBoundArcP`, and `bench/corpus_cells.h`'s
/// `kBoundArcP`).
inline constexpr double kE1BoundArcP = 0.45;

/// What a constructed cell knows about itself. `qp` is the problem; every
/// other member is ground truth the solver is not told.
struct E1Cell {
    QpProblem qp;
    Vec x_star, lambda_e_star, lambda_i_star, zl_star, zu_star;
    std::vector<Index> active_rows, active_lower, active_upper;
    Vec row_scale;
    std::string id;
};

/// The construction's free choices. `active_rows` is given explicitly, because
/// A1's whole point is WHERE the block sits; row 0 is rejected for the LICQ
/// reason E1 records (node 0's state block is pinned by the IC rows).
struct E1Spec {
    std::string id = "e1_cell";
    Index nodes = 40;
    double margin = 1e-2; ///< E1's margin class: {1e-2, 1e-4, 1e-6}.
    std::uint64_t seed = 1;
    std::vector<Index> active_rows;
    std::vector<Index> active_lower; ///< Variable indices sitting AT their lower bound.
    std::vector<Index> active_upper;
};

namespace e1_detail {

inline void add_at_times(const SpMatRM &a, const Vec &v, Vec &out) {
    for (Index r = 0; r < a.rows(); ++r) {
        for (SpMatRM::InnerIterator it(a, r); it; ++it) {
            out(it.col()) += it.value() * v(r);
        }
    }
}

/// `H` is stored UPPER-TRIANGULAR (QpProblem::validate's convention), so the
/// product has to symmetrize as it goes.
inline void add_h_times(const SpMatRM &h, const Vec &v, Vec &out) {
    for (Index r = 0; r < h.rows(); ++r) {
        for (SpMatRM::InnerIterator it(h, r); it; ++it) {
            out(r) += it.value() * v(it.col());
            if (it.col() != r) {
                out(it.col()) += it.value() * v(r);
            }
        }
    }
}

inline Vec a_times(const SpMatRM &a, const Vec &v) {
    Vec out = Vec::Zero(a.rows());
    for (Index r = 0; r < a.rows(); ++r) {
        for (SpMatRM::InnerIterator it(a, r); it; ++it) {
            out(r) += it.value() * v(it.col());
        }
    }
    return out;
}

} // namespace e1_detail

/// Builds one constructed cell. Throws `std::invalid_argument` on a spec that
/// would break the construction (row 0 active, an out-of-range index, a
/// bound-active index whose bound is absent, a zero row scale).
inline E1Cell make_e1_cell(const E1Spec &spec) {
    F7CollocationChain model(spec.nodes, /*states=*/3, /*controls=*/2, kE1BoundArcP,
                             /*radius=*/1.0);
    model.set_parameters(Vec::Constant(1, kE1BoundArcP));
    const Vec x0 = model.start_point();
    E1Cell cell;
    cell.id = spec.id;
    cell.qp = build_subproblem(model, x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));

    const Index n = cell.qp.g.size();
    const Index me = cell.qp.be.size();
    const Index mi = cell.qp.bi.size();

    for (const Index j : spec.active_rows) {
        if (j <= 0 || j >= mi) {
            throw std::invalid_argument(
                fmt::format("make_e1_cell('{}'): active row {} is outside [1, {}) -- row 0 is "
                            "structurally excluded (LICQ: node 0's state block is pinned by the "
                            "initial-condition rows)",
                            spec.id, j, mi));
        }
    }

    std::mt19937_64 rng(spec.seed);
    std::uniform_real_distribution<double> u_state(-0.6, 0.6);
    std::uniform_real_distribution<double> u_ctrl(-0.4, 0.4);
    std::uniform_real_distribution<double> u_le(-1.0, 1.0);
    std::uniform_real_distribution<double> u_li(0.5, 1.5);

    Vec x_star(n);
    for (Index k = 0; k < spec.nodes; ++k) {
        const Index b = k * 5;
        for (Index i = 0; i < 3; ++i) {
            x_star(b + i) = u_state(rng);
        }
        for (Index j = 0; j < 2; ++j) {
            x_star(b + 3 + j) = u_ctrl(rng);
        }
    }

    // THE BOUND-ACTIVE COMPONENTS (A3). E1 kept the box inactive by
    // construction; a cell with bound activity puts `x*` ON the bound and
    // prices it, which is what the stationarity inversion below then carries.
    Vec zl = Vec::Zero(n);
    Vec zu = Vec::Zero(n);
    for (const Index i : spec.active_lower) {
        if (i < 0 || i >= n || !std::isfinite(cell.qp.lower(i))) {
            throw std::invalid_argument(fmt::format(
                "make_e1_cell('{}'): index {} has no finite lower bound to sit at", spec.id, i));
        }
        x_star(i) = cell.qp.lower(i);
        zl(i) = u_li(rng);
    }
    for (const Index i : spec.active_upper) {
        if (i < 0 || i >= n || !std::isfinite(cell.qp.upper(i))) {
            throw std::invalid_argument(fmt::format(
                "make_e1_cell('{}'): index {} has no finite upper bound to sit at", spec.id, i));
        }
        x_star(i) = cell.qp.upper(i);
        zu(i) = u_li(rng);
    }

    Vec lambda_e(me);
    for (Index i = 0; i < me; ++i) {
        lambda_e(i) = u_le(rng);
    }
    Vec lambda_i = Vec::Zero(mi);
    std::vector<char> is_active(static_cast<std::size_t>(mi), 0);
    for (const Index j : spec.active_rows) {
        lambda_i(j) = u_li(rng);
        is_active[static_cast<std::size_t>(j)] = 1;
    }

    // E1's row scales, read in the units the margins are drawn in.
    cell.row_scale = Vec::Zero(mi);
    for (Index r = 0; r < cell.qp.Ai.rows(); ++r) {
        for (SpMatRM::InnerIterator it(cell.qp.Ai, r); it; ++it) {
            cell.row_scale(r) += std::abs(it.value()) * std::abs(x_star(it.col()));
        }
    }
    for (Index j = 0; j < mi; ++j) {
        if (!(cell.row_scale(j) > 0.0)) {
            throw std::invalid_argument(fmt::format(
                "make_e1_cell('{}'): row {} has a zero row scale -- a relative margin on it "
                "would be meaningless",
                spec.id, j));
        }
    }

    // E1's margin ladder over the inactive rows: the nearest decile sits at
    // `margin` of row scale, the rest spread three decades above it, permuted
    // so the tight rows do not cluster in the collocation index.
    const Vec r_star = e1_detail::a_times(cell.qp.Ai, x_star);
    Vec bi = r_star;
    std::vector<Index> inactive;
    for (Index j = 0; j < mi; ++j) {
        if (is_active[static_cast<std::size_t>(j)] == 0) {
            inactive.push_back(j);
        }
    }
    const std::size_t M = inactive.size();
    std::vector<std::size_t> pos(M);
    std::iota(pos.begin(), pos.end(), std::size_t(0));
    std::shuffle(pos.begin(), pos.end(), rng);
    for (std::size_t t = 0; t < M; ++t) {
        const double qpos = (static_cast<double>(pos[t]) + 0.5) / static_cast<double>(M);
        const double rel = spec.margin * std::pow(10.0, 3.0 * (qpos - 0.1));
        const Index j = inactive[t];
        bi(j) = r_star(j) + rel * cell.row_scale(j);
    }
    cell.qp.bi = bi;
    cell.qp.be = e1_detail::a_times(cell.qp.Ae, x_star);

    // THE INVERSION: stationarity `H x* + g + Ae' le + Ai' li - zL + zU = 0`.
    Vec g = Vec::Zero(n);
    e1_detail::add_h_times(cell.qp.H, x_star, g);
    e1_detail::add_at_times(cell.qp.Ae, lambda_e, g);
    e1_detail::add_at_times(cell.qp.Ai, lambda_i, g);
    g -= zl;
    g += zu;
    cell.qp.g = -g;

    cell.x_star = x_star;
    cell.lambda_e_star = lambda_e;
    cell.lambda_i_star = lambda_i;
    cell.zl_star = zl;
    cell.zu_star = zu;
    cell.active_rows = spec.active_rows;
    cell.active_lower = spec.active_lower;
    cell.active_upper = spec.active_upper;
    std::sort(cell.active_rows.begin(), cell.active_rows.end());
    std::sort(cell.active_lower.begin(), cell.active_lower.end());
    std::sort(cell.active_upper.begin(), cell.active_upper.end());
    return cell;
}

/// E1's two acquired-active-set rules, reported side by side with the ratio
/// rule so A1's counts are comparable with E1's CSV
/// (`docs/notes/data/2026-08-m6-e1-acquisition/README.md:120`).
struct E1RuleCounts {
    Index rule_a = 0;    ///< slack <= 1e-8 * row_scale
    Index rule_b = 0;    ///< lambda_i >= 1e-6 * max(1, ||lambda_i||inf)
    Index recovered = 0; ///< Rule A rows that are really active
    Index false_positive = 0;
    Index missed = 0;
};

inline E1RuleCounts e1_rule_counts(const E1Cell &cell, const Vec &x, const Vec &lambda_i) {
    const Index mi = cell.qp.bi.size();
    const Vec r = e1_detail::a_times(cell.qp.Ai, x);
    std::vector<char> truth(static_cast<std::size_t>(mi), 0);
    for (const Index j : cell.active_rows) {
        truth[static_cast<std::size_t>(j)] = 1;
    }
    double dual_scale = 1.0;
    if (lambda_i.size() > 0) {
        dual_scale = std::max(1.0, lambda_i.lpNorm<Eigen::Infinity>());
    }
    E1RuleCounts c;
    for (Index j = 0; j < mi; ++j) {
        const bool a = (cell.qp.bi(j) - r(j)) <= 1e-8 * cell.row_scale(j);
        const bool b = lambda_i(j) >= 1e-6 * dual_scale;
        c.rule_a += a ? 1 : 0;
        c.rule_b += b ? 1 : 0;
        const bool t = truth[static_cast<std::size_t>(j)] == 1;
        c.recovered += (a && t) ? 1 : 0;
        c.false_positive += (a && !t) ? 1 : 0;
        c.missed += (!a && t) ? 1 : 0;
    }
    return c;
}

} // namespace hven::solvers::test_support
