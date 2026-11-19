// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The maximum-product matching and its symmetric scaling
// (hven/detail/linear/matching_scaling.h), against two independent references:
// brute force over every permutation on small matrices, and a dense O(n^3)
// Hungarian assignment (written here, not shared with the library) on larger
// random ones. Plus the LP optimality certificate (dual feasibility and
// complementary slackness) and the symmetric scaling's bound on every case.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/detail/linear/matching_scaling.h"

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::detail::max_product_matching;
using hven::linear::detail::MaxProductMatching;
using hven::linear::detail::symmetric_matching_scaling;

constexpr double kInf = std::numeric_limits<double>::infinity();

// A dense matrix with zeros for "no entry", converted to a sparse one with
// only the nonzeros stored.
using Dense = std::vector<std::vector<double>>;

SpMatRM to_sparse(const Dense &a) {
    const auto n = static_cast<Index>(a.size());
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j < n; ++j) {
            if (a[i][j] != 0.0) {
                t.emplace_back(i, j, a[i][j]);
            }
        }
    }
    SpMatRM s(n, n);
    s.setFromTriplets(t.begin(), t.end());
    return s;
}

SpMatRM upper_of(const Dense &a) {
    const auto n = static_cast<Index>(a.size());
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (a[i][j] != 0.0) {
                t.emplace_back(i, j, a[i][j]);
            }
        }
    }
    SpMatRM s(n, n);
    s.setFromTriplets(t.begin(), t.end());
    return s;
}

// sum_i -log|a_{i, sigma(i)}| over a full matching (kInf if it uses a zero).
double matching_cost(const Dense &a, const std::vector<Index> &col_of_row) {
    double c = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double m = std::abs(a[i][static_cast<std::size_t>(col_of_row[i])]);
        if (m == 0.0) {
            return kInf;
        }
        c -= std::log(m);
    }
    return c;
}

// Brute force: the minimum of matching_cost over all permutations, and the
// maximum cardinality over all permutations (count of nonzeros used).
struct Brute {
    double best_cost = kInf;
    Index max_card = 0;
};
Brute brute(const Dense &a) {
    const auto n = a.size();
    std::vector<Index> p(n);
    std::iota(p.begin(), p.end(), 0);
    Brute b;
    do {
        b.best_cost = std::min(b.best_cost, matching_cost(a, p));
        Index card = 0;
        for (std::size_t i = 0; i < n; ++i) {
            card += a[i][static_cast<std::size_t>(p[i])] != 0.0 ? 1 : 0;
        }
        b.max_card = std::max(b.max_card, card);
    } while (std::next_permutation(p.begin(), p.end()));
    return b;
}

// Dense Hungarian (min-cost perfect assignment, potentials + shortest
// augmenting path, O(n^3)) on c_ij = -log|a_ij| with zeros forbidden.
double hungarian_cost(const Dense &a) {
    const auto n = static_cast<Index>(a.size());
    const double big = 1e100;
    auto cost = [&](Index i, Index j) {
        const double m = std::abs(a[i - 1][j - 1]);
        return m == 0.0 ? big : -std::log(m);
    };
    std::vector<double> u(n + 1, 0.0), v(n + 1, 0.0), minv(n + 1);
    std::vector<Index> p(n + 1, 0), way(n + 1, 0);
    std::vector<char> used(n + 1);
    for (Index i = 1; i <= n; ++i) {
        p[0] = i;
        Index j0 = 0;
        std::fill(minv.begin(), minv.end(), kInf);
        std::fill(used.begin(), used.end(), 0);
        do {
            used[j0] = 1;
            const Index i0 = p[j0];
            double delta = kInf;
            Index j1 = 0;
            for (Index j = 1; j <= n; ++j) {
                if (used[j] == 0) {
                    const double cur = cost(i0, j) - u[i0] - v[j];
                    if (cur < minv[j]) {
                        minv[j] = cur;
                        way[j] = j0;
                    }
                    if (minv[j] < delta) {
                        delta = minv[j];
                        j1 = j;
                    }
                }
            }
            for (Index j = 0; j <= n; ++j) {
                if (used[j] != 0) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const Index j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }
    std::vector<Index> col_of_row(n);
    for (Index j = 1; j <= n; ++j) {
        col_of_row[p[j] - 1] = j - 1;
    }
    return matching_cost(a, col_of_row);
}

// Every structural nonzero: u_i + v_j <= -log|a_ij| (+ tol); every matched
// pair: equality (to tol). The LP optimality certificate.
void expect_certificate(const Dense &a, const MaxProductMatching &m, double tol) {
    const auto n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (a[i][j] == 0.0) {
                continue;
            }
            const double c = -std::log(std::abs(a[i][j]));
            const double scale = 1.0 + std::abs(c);
            EXPECT_LE(m.u(static_cast<Index>(i)) + m.v(static_cast<Index>(j)), c + tol * scale)
                << "dual infeasible at (" << i << ", " << j << ")";
            if (m.col_of_row[i] == static_cast<Index>(j)) {
                EXPECT_NEAR(m.u(static_cast<Index>(i)) + m.v(static_cast<Index>(j)), c, tol * scale)
                    << "slack on matched (" << i << ", " << j << ")";
            }
        }
    }
}

bool is_permutation_partial(const MaxProductMatching &m, Index n) {
    std::vector<char> seen(static_cast<std::size_t>(n), 0);
    for (const Index c : m.col_of_row) {
        if (c < 0) {
            continue;
        }
        if (c >= n || seen[static_cast<std::size_t>(c)] != 0) {
            return false;
        }
        seen[static_cast<std::size_t>(c)] = 1;
    }
    return true;
}

// A random n x n matrix with density `p`, magnitudes spanning 10^[-lo, lo].
Dense random_dense(std::mt19937_64 &rng, Index n, double p, double lo, bool symmetric) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> expo(-lo, lo);
    Dense a(n, std::vector<double>(n, 0.0));
    for (Index i = 0; i < n; ++i) {
        for (Index j = symmetric ? i : 0; j < n; ++j) {
            if (unit(rng) < p) {
                const double sign = unit(rng) < 0.5 ? -1.0 : 1.0;
                a[i][j] = sign * std::pow(10.0, expo(rng));
                if (symmetric) {
                    a[j][i] = a[i][j];
                }
            }
        }
    }
    return a;
}

TEST(MaxProductMatching, MatchesBruteForceOnSmallMatrices) {
    std::mt19937_64 rng(20261118);
    int nonsingular = 0;
    int singular = 0;
    for (int trial = 0; trial < 600; ++trial) {
        const Index n = 1 + trial % 7;
        const double p = trial % 3 == 0 ? 0.3 : (trial % 3 == 1 ? 0.55 : 0.9);
        const Dense a = random_dense(rng, n, p, 6.0, false);
        const Brute b = brute(a);
        const MaxProductMatching m = max_product_matching(to_sparse(a));
        ASSERT_TRUE(is_permutation_partial(m, n));
        // Maximum cardinality (Berge: one augmenting-path search per row suffices).
        EXPECT_EQ(m.matched, b.max_card) << "trial " << trial;
        if (b.max_card == n) {
            ++nonsingular;
            EXPECT_NEAR(matching_cost(a, m.col_of_row), b.best_cost,
                        1e-9 * (1.0 + std::abs(b.best_cost)))
                << "trial " << trial;
            expect_certificate(a, m, 1e-10);
        } else {
            ++singular;
        }
    }
    // Both regimes were exercised.
    EXPECT_GT(nonsingular, 100);
    EXPECT_GT(singular, 50);
}

TEST(MaxProductMatching, MatchesADenseHungarianOnLargerRandomMatrices) {
    std::mt19937_64 rng(7);
    for (int trial = 0; trial < 40; ++trial) {
        const Index n = 20 + 7 * trial % 60;
        Dense a = random_dense(rng, n, 0.08, 10.0, false);
        // A small diagonal keeps it structurally nonsingular without making the
        // diagonal the answer.
        for (Index i = 0; i < n; ++i) {
            if (a[i][i] == 0.0) {
                a[i][i] = 1e-11;
            }
        }
        const MaxProductMatching m = max_product_matching(to_sparse(a));
        ASSERT_EQ(m.matched, n);
        const double ref = hungarian_cost(a);
        EXPECT_NEAR(matching_cost(a, m.col_of_row), ref, 1e-8 * (1.0 + std::abs(ref)))
            << "trial " << trial << " n " << n;
        expect_certificate(a, m, 1e-9);
    }
}

TEST(MaxProductMatching, IgnoresExplicitZerosAndAcceptsUncompressedStorage) {
    // [[0(stored), 2], [3, 0]] -- the only edge set is the anti-diagonal.
    SpMatRM a(2, 2);
    a.reserve(Eigen::VectorXi::Constant(2, 3)); // uncompressed
    a.insert(0, 0) = 0.0;
    a.insert(0, 1) = 2.0;
    a.insert(1, 0) = 3.0;
    ASSERT_FALSE(a.isCompressed());
    const MaxProductMatching m = max_product_matching(a);
    EXPECT_EQ(m.matched, 2);
    EXPECT_EQ(m.col_of_row[0], 1);
    EXPECT_EQ(m.col_of_row[1], 0);
}

TEST(MaxProductMatching, RefusesNonSquareAndNonFinite) {
    SpMatRM r(2, 3);
    EXPECT_THROW(max_product_matching(r), std::invalid_argument);
    SpMatRM a(2, 2);
    a.insert(0, 0) = std::numeric_limits<double>::quiet_NaN();
    a.insert(1, 1) = 1.0;
    EXPECT_THROW(max_product_matching(a), std::invalid_argument);
    a.coeffRef(0, 0) = kInf;
    EXPECT_THROW(max_product_matching(a), std::invalid_argument);
}

// |d_i a_ij d_j| <= 1 everywhere; = 1 (to rounding) on the matching, so every
// row of a structurally nonsingular matrix carries a unit entry.
void expect_scaled_bound(const Dense &a, const Vec &d, bool nonsingular) {
    const auto n = static_cast<Index>(a.size());
    ASSERT_EQ(d.size(), n);
    for (Index i = 0; i < n; ++i) {
        ASSERT_TRUE(std::isfinite(d(i)) && d(i) > 0.0) << "d(" << i << ") = " << d(i);
        double rowmax = 0.0;
        for (Index j = 0; j < n; ++j) {
            const double s = std::abs(a[i][j]) * d(i) * d(j);
            EXPECT_LE(s, 1.0 + 1e-10) << "(" << i << ", " << j << ")";
            rowmax = std::max(rowmax, s);
        }
        if (nonsingular) {
            EXPECT_NEAR(rowmax, 1.0, 1e-10) << "row " << i;
        }
    }
}

TEST(SymmetricMatchingScaling, BoundsEveryEntryAndPutsAUnitInEveryRow) {
    std::mt19937_64 rng(11);
    int checked = 0;
    for (int trial = 0; trial < 200; ++trial) {
        const Index n = 2 + trial % 30;
        Dense a = random_dense(rng, n, 0.25, 12.0, true);
        for (Index i = 0; i < n; ++i) {
            if (trial % 2 == 0 && a[i][i] == 0.0) {
                a[i][i] = 1e-9;
            }
        }
        const MaxProductMatching m = max_product_matching(to_sparse(a));
        const Vec d = symmetric_matching_scaling(upper_of(a));
        expect_scaled_bound(a, d, m.matched == n);
        checked += m.matched == n ? 1 : 0;
    }
    EXPECT_GT(checked, 100);
}

TEST(SymmetricMatchingScaling, PairsASaddlePointBlockAcrossItsZeroDiagonal) {
    // K = [[eps*I, A'], [A, 0]] with A badly scaled: the matching pairs each
    // constraint row with a variable (the zero block has nothing to match), and
    // the scaled K has unit entries on those pairs.
    const Index nv = 4;
    const Index nc = 2;
    Dense k(nv + nc, std::vector<double>(nv + nc, 0.0));
    for (Index i = 0; i < nv; ++i) {
        k[i][i] = 1e-10;
    }
    const double a[2][4] = {{1e8, 0.0, 3.0, 0.0}, {0.0, 1e-6, 0.0, 2e-6}};
    for (Index r = 0; r < nc; ++r) {
        for (Index j = 0; j < nv; ++j) {
            k[nv + r][j] = a[r][j];
            k[j][nv + r] = a[r][j];
        }
    }
    const MaxProductMatching m = max_product_matching(to_sparse(k));
    ASSERT_EQ(m.matched, nv + nc);
    for (Index r = nv; r < nv + nc; ++r) {
        EXPECT_LT(m.col_of_row[static_cast<std::size_t>(r)], nv) << "constraint row " << r;
    }
    const Vec d = symmetric_matching_scaling(upper_of(k));
    expect_scaled_bound(k, d, true);
    EXPECT_NEAR(std::abs(k[nv][0]) * d(nv) * d(0), 1.0, 1e-12);
}

TEST(SymmetricMatchingScaling, ScalesAStructurallySingularIndexByItsMatchedNeighbours) {
    // Index 2 couples only to index 0, and index 0 is the only partner of
    // both 1 and 2 -- one of them is left unmatched.
    Dense a = {{0.0, 4.0, 1e-3}, {4.0, 0.0, 0.0}, {1e-3, 0.0, 0.0}};
    const MaxProductMatching m = max_product_matching(to_sparse(a));
    EXPECT_EQ(m.matched, 2);
    const Vec d = symmetric_matching_scaling(upper_of(a));
    ASSERT_EQ(d.size(), 3);
    for (Index i = 0; i < 3; ++i) {
        EXPECT_TRUE(std::isfinite(d(i)) && d(i) > 0.0);
    }
    expect_scaled_bound(a, d, false);
    // An empty row and column: d = 1 there, the rest scaled.
    Dense z = {{2.0, 0.0}, {0.0, 0.0}};
    const Vec dz = symmetric_matching_scaling(upper_of(z));
    EXPECT_NEAR(dz(0) * dz(0) * 2.0, 1.0, 1e-14);
    EXPECT_EQ(dz(1), 1.0);
}

TEST(SymmetricMatchingScaling, RefusesALowerTriangleEntry) {
    SpMatRM a(2, 2);
    a.insert(1, 0) = 1.0;
    EXPECT_THROW(symmetric_matching_scaling(a), std::invalid_argument);
    SpMatRM b(2, 2);
    b.insert(0, 1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(symmetric_matching_scaling(b), std::invalid_argument);
}

} // namespace
