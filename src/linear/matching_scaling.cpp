// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>

#include <fmt/format.h>

#include <hven/detail/linear/matching_scaling.h>

namespace hven::linear::detail {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

void require_square_finite(const SpMatRM &a, const char *what) {
    if (a.rows() != a.cols()) {
        throw std::invalid_argument(fmt::format("{}: the matrix must be square, got {} x {}", what,
                                                static_cast<long long>(a.rows()),
                                                static_cast<long long>(a.cols())));
    }
    for (Index i = 0; i < a.outerSize(); ++i) {
        for (SpMatRM::InnerIterator e(a, i); e; ++e) {
            if (!std::isfinite(e.value())) {
                throw std::invalid_argument(fmt::format("{}: entry ({}, {}) is not finite", what,
                                                        static_cast<long long>(i),
                                                        static_cast<long long>(e.col())));
            }
        }
    }
}

} // namespace

// THE ALGORITHM (Duff & Koster 2001, sections 3-4). Rows are matched to
// columns. The cost of edge (i, j) is c_ij = log(rowmax_i) - log|a_ij| >= 0,
// the row-normalized form of -log|a_ij| (the same assignment; it differs by a
// constant per row, folded back into u at the end), so that zero duals start
// feasible and every reduced cost rc_ij = c_ij - u_i - v_j is non-negative.
//
// Initial duals and matching: v_j = min_i c_ij, then u_i = min_j (c_ij - v_j),
// and every row takes a free column on a zero-reduced-cost edge if it has one.
//
// Each still-unmatched row i0 then grows a shortest-path tree by Dijkstra over
// columns: from a row r reached at distance delta, column j is reached at
// delta + rc_rj; a popped column that is free ends the search at length L; a
// matched one hands its row on at the column's distance (matched edges have
// zero reduced cost). The duals are then updated over the popped columns S --
// v_j += d_j - L, and u_r += L - d_j for the row r matched to j -- with
// u_i0 += L. Every reduced cost stays non-negative and every edge of the new
// matching, the augmenting path's included, has reduced cost zero, which is the
// optimality certificate. A row whose search exhausts its component without a
// free column stays unmatched (A structurally singular) and changes no dual.
//
// PRUNING (structurally singular A only). Every column a failed search popped is
// DEAD: it is matched, and every alternating path out of it stays inside the
// failed tree (its matched row's edges were all relaxed), which holds no free
// column. A later augmenting path never passes through it, so its matched pair
// never changes again, and later searches skip it -- each dead column is
// expanded by at most one search, and the structurally singular case costs what
// the nonsingular one does. A skipped column's dual v_j is not shifted by the
// later searches' updates, so at the end every dead column's v_j is lowered to
// min_i (c_ij - u_i) over its edges: every reduced cost is again non-negative
// (the dual bound |r_i a_ij s_j| <= 1 holds on every entry), and a dead pair's
// matched edge keeps |r a s| <= 1 but may lose its equality. A structurally
// nonsingular A has no failed search and its outcome is untouched.
//
// NO DEADLINE CHECK, by the cost bound: one search is O((nnz + n) log n) and
// there are at most n, so a matching is O(n (nnz + n) log n) in the worst case
// and, with pruning, ~ (nnz + n) log n per augmentation in practice (Duff &
// Koster 2001, section 6). It runs only inside a platform retake, immediately
// before a sparse factorization of the same matrix whose cost dominates it, and
// at most twice per failed factorization; the engine's own budgets are checked
// between factorizations, which is the granularity this call already sits at.
MaxProductMatching max_product_matching(const SpMatRM &a) {
    require_square_finite(a, "max_product_matching");
    const Index n = a.rows();

    // Row i's edges are the storage range [row_begin(i), row_end(i)) of `a`
    // (compressed or not). An edge with |a_ij| == 0 is not an edge: its cost
    // is +inf and it is never relaxed.
    const auto *outer = a.outerIndexPtr();
    const auto *inner = a.innerIndexPtr();
    const auto *vals = a.valuePtr();
    const auto *nzs = a.innerNonZeroPtr(); // null when compressed
    const auto row_begin = [&](Index i) { return static_cast<Index>(outer[i]); };
    const auto row_end = [&](Index i) {
        return nzs == nullptr ? static_cast<Index>(outer[i + 1])
                              : static_cast<Index>(outer[i] + nzs[i]);
    };
    std::vector<double> cost(static_cast<std::size_t>(n > 0 ? outer[n] : 0), kInf);
    Vec log_rowmax = Vec::Zero(n);
    for (Index i = 0; i < n; ++i) {
        double rowmax = 0.0;
        for (Index k = row_begin(i); k < row_end(i); ++k) {
            rowmax = std::max(rowmax, std::abs(vals[k]));
        }
        log_rowmax(i) = rowmax > 0.0 ? std::log(rowmax) : 0.0;
        for (Index k = row_begin(i); k < row_end(i); ++k) {
            const double m = std::abs(vals[k]);
            if (m > 0.0) {
                cost[static_cast<std::size_t>(k)] = log_rowmax(i) - std::log(m);
            }
        }
    }

    MaxProductMatching out;
    out.col_of_row.assign(static_cast<std::size_t>(n), -1);
    std::vector<Index> row_of_col(static_cast<std::size_t>(n), -1);
    Vec u = Vec::Zero(n);
    Vec v = Vec::Constant(n, kInf);
    for (Index i = 0; i < n; ++i) {
        for (Index k = row_begin(i); k < row_end(i); ++k) {
            const auto j = static_cast<Index>(inner[k]);
            v(j) = std::min(v(j), cost[static_cast<std::size_t>(k)]);
        }
    }
    for (Index j = 0; j < n; ++j) {
        if (v(j) == kInf) {
            v(j) = 0.0; // an empty column constrains nothing
        }
    }
    // Reduced cost, always evaluated in this one order so a zero computed here
    // is the zero computed everywhere.
    const auto rc = [&](Index i, Index k) {
        return (cost[static_cast<std::size_t>(k)] - v(static_cast<Index>(inner[k]))) - u(i);
    };
    for (Index i = 0; i < n; ++i) {
        double best = kInf;
        for (Index k = row_begin(i); k < row_end(i); ++k) {
            best =
                std::min(best, cost[static_cast<std::size_t>(k)] - v(static_cast<Index>(inner[k])));
        }
        u(i) = best == kInf ? 0.0 : best;
        for (Index k = row_begin(i); k < row_end(i); ++k) {
            const auto j = static_cast<Index>(inner[k]);
            if (cost[static_cast<std::size_t>(k)] < kInf && rc(i, k) <= 0.0 &&
                row_of_col[static_cast<std::size_t>(j)] < 0) {
                out.col_of_row[static_cast<std::size_t>(i)] = j;
                row_of_col[static_cast<std::size_t>(j)] = i;
                break;
            }
        }
    }

    // Dijkstra scratch, reset over the touched columns only; the heap's storage
    // is kept across rows (a binary heap over one vector, cleared, not freed).
    std::vector<double> dist(static_cast<std::size_t>(n), kInf);
    std::vector<Index> pred(static_cast<std::size_t>(n), -1);
    std::vector<char> popped(static_cast<std::size_t>(n), 0);
    std::vector<char> dead(static_cast<std::size_t>(n), 0);
    bool any_dead = false;
    std::vector<Index> touched;
    std::vector<Index> scanned;
    using Item = std::pair<double, Index>;
    std::vector<Item> heap;
    const auto heap_push = [&](double d, Index j) {
        heap.emplace_back(d, j);
        std::push_heap(heap.begin(), heap.end(), std::greater<>());
    };
    const auto heap_pop = [&] {
        std::pop_heap(heap.begin(), heap.end(), std::greater<>());
        const Item top = heap.back();
        heap.pop_back();
        return top;
    };

    for (Index i0 = 0; i0 < n; ++i0) {
        if (out.col_of_row[static_cast<std::size_t>(i0)] >= 0) {
            continue;
        }
        const auto relax = [&](Index r, double base) {
            for (Index k = row_begin(r); k < row_end(r); ++k) {
                if (!(cost[static_cast<std::size_t>(k)] < kInf)) {
                    continue;
                }
                const auto j = static_cast<Index>(inner[k]);
                const auto js = static_cast<std::size_t>(j);
                if (popped[js] != 0 || dead[js] != 0) {
                    continue;
                }
                const double nd = base + std::max(0.0, rc(r, k));
                if (nd < dist[js]) {
                    if (dist[js] == kInf) {
                        touched.push_back(j);
                    }
                    dist[js] = nd;
                    pred[js] = r;
                    heap_push(nd, j);
                }
            }
        };
        relax(i0, 0.0);
        Index free_col = -1;
        double length = kInf;
        while (!heap.empty()) {
            const auto [d, j] = heap_pop();
            const auto js = static_cast<std::size_t>(j);
            if (popped[js] != 0 || d > dist[js]) {
                continue; // stale entry
            }
            popped[js] = 1;
            scanned.push_back(j);
            const Index r = row_of_col[js];
            if (r < 0) {
                free_col = j;
                length = d;
                break;
            }
            relax(r, d);
        }
        if (free_col >= 0) {
            for (const Index j : scanned) {
                const auto js = static_cast<std::size_t>(j);
                const double shift = length - dist[js]; // >= 0
                v(j) -= shift;
                const Index r = row_of_col[js];
                if (r >= 0) {
                    u(r) += shift;
                }
            }
            u(i0) += length;
            // Augment along the predecessor chain back to i0.
            Index j = free_col;
            for (;;) {
                const Index r = pred[static_cast<std::size_t>(j)];
                const Index prev = out.col_of_row[static_cast<std::size_t>(r)];
                out.col_of_row[static_cast<std::size_t>(r)] = j;
                row_of_col[static_cast<std::size_t>(j)] = r;
                if (r == i0) {
                    break;
                }
                j = prev;
            }
        } else {
            // A failed search: its popped columns are dead (PRUNING above).
            for (const Index j : scanned) {
                dead[static_cast<std::size_t>(j)] = 1;
            }
            any_dead = any_dead || !scanned.empty();
        }
        for (const Index j : touched) {
            const auto js = static_cast<std::size_t>(j);
            dist[js] = kInf;
            pred[js] = -1;
            popped[js] = 0;
        }
        touched.clear();
        scanned.clear();
        heap.clear();
    }
    if (any_dead) {
        // Dual feasibility on the dead columns' edges (PRUNING above).
        Vec vmin = Vec::Constant(n, kInf);
        for (Index i = 0; i < n; ++i) {
            for (Index k = row_begin(i); k < row_end(i); ++k) {
                const auto j = static_cast<Index>(inner[k]);
                if (dead[static_cast<std::size_t>(j)] != 0 &&
                    cost[static_cast<std::size_t>(k)] < kInf) {
                    vmin(j) = std::min(vmin(j), cost[static_cast<std::size_t>(k)] - u(i));
                }
            }
        }
        for (Index j = 0; j < n; ++j) {
            if (dead[static_cast<std::size_t>(j)] != 0 && vmin(j) < v(j)) {
                v(j) = vmin(j);
            }
        }
    }

    // Back to the cost -log|a_ij|: c_ij = log_rowmax_i + (-log|a_ij|).
    out.u = u - log_rowmax;
    out.v = std::move(v);
    out.matched = static_cast<Index>(std::count_if(out.col_of_row.begin(), out.col_of_row.end(),
                                                   [](Index c) { return c >= 0; }));
    return out;
}

Vec symmetric_matching_scaling(const SpMatRM &upper) {
    require_square_finite(upper, "symmetric_matching_scaling");
    const Index n = upper.rows();
    std::vector<Eigen::Triplet<double>> trip;
    trip.reserve(static_cast<std::size_t>(2 * upper.nonZeros()));
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator e(upper, i); e; ++e) {
            const auto j = static_cast<Index>(e.col());
            if (j < i) {
                throw std::invalid_argument(fmt::format(
                    "symmetric_matching_scaling: entry ({}, {}) lies below the diagonal; pass the "
                    "upper triangle",
                    static_cast<long long>(i), static_cast<long long>(j)));
            }
            trip.emplace_back(i, j, e.value());
            if (j != i) {
                trip.emplace_back(j, i, e.value());
            }
        }
    }
    SpMatRM full(n, n);
    full.setFromTriplets(trip.begin(), trip.end());

    const MaxProductMatching m = max_product_matching(full);
    std::vector<char> matched_col(static_cast<std::size_t>(n), 0);
    for (Index i = 0; i < n; ++i) {
        const Index c = m.col_of_row[static_cast<std::size_t>(i)];
        if (c >= 0) {
            matched_col[static_cast<std::size_t>(c)] = 1;
        }
    }
    // An index is scaled from the duals when its row AND its column are matched
    // (every index, when `upper` is structurally nonsingular).
    std::vector<char> good(static_cast<std::size_t>(n), 0);
    Vec d = Vec::Ones(n);
    for (Index i = 0; i < n; ++i) {
        const auto is = static_cast<std::size_t>(i);
        if (m.col_of_row[is] >= 0 && matched_col[is] != 0) {
            const double di = std::exp(0.5 * (m.u(i) + m.v(i)));
            if (std::isfinite(di) && di > 0.0) {
                d(i) = di;
                good[is] = 1;
            }
        }
    }
    // Duff & Pralet's rule for the rest: scale the row so its largest entry
    // against the matched indices is 1.
    for (Index i = 0; i < n; ++i) {
        if (good[static_cast<std::size_t>(i)] != 0) {
            continue;
        }
        double big = 0.0;
        for (SpMatRM::InnerIterator e(full, i); e; ++e) {
            const auto j = static_cast<Index>(e.col());
            if (good[static_cast<std::size_t>(j)] != 0) {
                big = std::max(big, std::abs(e.value()) * d(j));
            }
        }
        const double di = big > 0.0 ? 1.0 / big : 1.0;
        d(i) = (std::isfinite(di) && di > 0.0) ? di : 1.0;
    }
    return d;
}

} // namespace hven::linear::detail
