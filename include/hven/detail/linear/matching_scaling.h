// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// MAXIMUM-PRODUCT WEIGHTED MATCHING AND THE SYMMETRIC SCALING BUILT ON IT.
//
// Written from the papers only: I. S. Duff and J. Koster, "On algorithms for
// permuting large entries to the diagonal of a sparse matrix", SIAM J. Matrix
// Anal. Appl. 22(4), 2001 (the maximum-product transversal as a minimum-cost
// bipartite assignment, solved by shortest augmenting paths with Dijkstra on
// reduced costs, and the row/column scaling read off its optimal duals); and
// I. S. Duff and S. Pralet, "Strategies for scaling and pivoting for sparse
// symmetric indefinite problems", SIAM J. Matrix Anal. Appl. 27(2), 2005 (the
// symmetric scaling d_i = sqrt(r_i c_i) of a symmetric matrix from the same
// duals, and the rule for indices the matching leaves unmatched).
//
// THE PROBLEM. For a square A, find a permutation sigma maximizing
// prod_i |a_{i,sigma(i)}|. With c_ij = -log|a_ij| on the structural nonzeros it
// is the assignment problem min sum_i c_{i,sigma(i)}, whose LP dual is
// max sum u_i + sum v_j subject to u_i + v_j <= c_ij. At an optimum the
// scaling r_i = exp(u_i), s_j = exp(v_j) makes every |r_i a_ij s_j| <= 1 with
// equality on the matching.
//
// THE SYMMETRIC CASE. When A is symmetric, (u, v) and (v, u) are both dual
// feasible and optimal, so their average w = (u + v)/2 is too; complementary
// slackness then gives |a_ij| exp(w_i) exp(w_j) = 1 on every matched (i, j)
// and <= 1 everywhere. d_i = exp(w_i) = sqrt(r_i s_i) is therefore a
// SYMMETRIC scaling D A D with all entries bounded by 1 in magnitude and a
// unit entry in every matched row -- what a factorization that pivots on the
// unscaled entries' magnitudes wants, and invariant to the ill scaling of A.
//
// Used by the KKT factor's platform-solve response (detail/kkt/kkt_factor.h):
// the scaling is generic sparse linear algebra, independent of KKT structure,
// so it lives in linear/ beside the factor surface it prepares matrices for.

#include <vector>

#include "hven/core/types.h"

namespace hven::linear::detail {

/// The outcome of max_product_matching(): the matching and its optimal duals.
struct MaxProductMatching {
    /// col_of_row[i] = the column matched to row i, or -1 when row i is
    /// unmatched (A structurally singular).
    std::vector<Index> col_of_row;
    /// Dual variables in the cost c_ij = -log|a_ij|: u_i + v_j <= c_ij on every
    /// structural nonzero, with equality on matched pairs.
    Vec u;
    Vec v;
    /// The number of matched rows (== A.rows() iff A is structurally nonsingular).
    Index matched = 0;
};

/// @brief The maximum-product matching of the square matrix `a` (FULL storage:
/// every structural nonzero present; explicit zeros are not edges), by
/// Dijkstra shortest augmenting paths on reduced costs (Duff & Koster 2001).
/// Throws std::invalid_argument when `a` is not square or holds a non-finite
/// entry. O(n (nnz + n) log n) worst case.
MaxProductMatching max_product_matching(const SpMatRM &a);

/// @brief The symmetric maximum-product-matching scaling d (Duff & Pralet
/// 2005) of the symmetric matrix whose UPPER triangle (diagonal included) is
/// `upper`: |d_i a_ij d_j| <= 1 for every entry of a matched row and column,
/// = 1 on the matching. An index the matching leaves unmatched takes
/// d_i = 1 / max_j |a_ij d_j| over its matched neighbours j (1 when it has
/// none). Every d_i is finite and positive. Throws std::invalid_argument when
/// `upper` is not square, holds a non-finite entry, or holds an entry below
/// the diagonal.
Vec symmetric_matching_scaling(const SpMatRM &upper);

} // namespace hven::linear::detail
