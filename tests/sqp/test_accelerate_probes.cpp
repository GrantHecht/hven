// Accelerate-backend audit probes (macOS only) — the measurement instruments
// for docs/notes/2026-07-28-accelerate-audit-checklist.md sections (c) and
// (e), reported in docs/notes/2026-07-29-accelerate-audit-results.md.
//
// M3 PHASE B DISPOSITIONS (docs/retarget-design-sqp.md §10 item 6): the
// §(a) inertia probes and the §(b) partial-solve composition probes pinned
// the dissolved Accelerate KktSystem twin's OWN mappings (its zero-pivot
// reading of num_perturbed_pivots(), its bare-subfactor solves) and retired
// with it; the composition coverage now lives in test_kkt_partial_solve.cpp's
// Accelerate arms, retargeted onto SymmetricFactor::solve_partial /
// supports_partial_solve(). The §(c) ordering probe (raw Sparse API, no
// dissolved seam involved) survives unchanged, and the §(e) shim probes
// retarget onto hven::linear::detail's namespaced LAPACKE shim (the SQP copy
// of the shim was deleted with the twin).
//
// FIRST-RUN POSTURE: every numeric expectation below that is marked
// "OBSERVED" was pinned from a run on this machine (Apple Silicon, macOS 26,
// AppleClang 21) AFTER first being printed via RecordProperty — nothing here
// is a prediction. Where behavior was still unmeasured the assertions are
// deliberately loose (finiteness / no-crash) and the RecordProperty output is
// the deliverable.
//
// These tests compile only when USE_ACCELERATE_SPARSE is defined; on MKL
// builds this TU is empty.

#ifdef USE_ACCELERATE_SPARSE

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <Accelerate/Accelerate.h>

#include <hven/detail/linear/lapacke_shim.h>
#include <hven/detail/sqp/types.h>

using namespace hven::solvers;
using hven::linear::detail::lapack_int;
using hven::linear::detail::LAPACKE_dsytrf;
using hven::linear::detail::LAPACKE_dsytrs;

namespace {

// --- Checklist §(c): ordering behavior (raw Sparse API) -------------------

// Default ordering for a symmetric LDLT^TPP factorization: does Accelerate
// return a non-identity fill-reducing permutation through fopts.order, and
// does SparseOrderUser accept a caller-supplied one?
TEST(AccelerateProbe, OrderingDefaultAndUser) {
    // Arrow matrix again — AMD should push the dense tip last/first rather
    // than keep the natural order.
    const int n = 6;
    // Build CSC-lower directly for the raw API: column j holds (j,j) and,
    // for j < n-1, the (n-1, j) arrow entry.
    std::vector<long> col_starts;
    std::vector<int> row_idx;
    std::vector<double> vals;
    for (int j = 0; j < n; ++j) {
        col_starts.push_back(static_cast<long>(row_idx.size()));
        row_idx.push_back(j);
        vals.push_back(2.0 + j);
        if (j < n - 1) {
            row_idx.push_back(n - 1);
            vals.push_back(1.0);
        }
    }
    col_starts.push_back(static_cast<long>(row_idx.size()));

    SparseMatrixStructure s{};
    s.rowCount = n;
    s.columnCount = n;
    s.columnStarts = col_starts.data();
    s.rowIndices = row_idx.data();
    s.attributes.kind = SparseSymmetric;
    s.attributes.triangle = SparseLowerTriangle;
    s.blockSize = 1;

    std::vector<int> perm(static_cast<std::size_t>(n), 0);
    SparseSymbolicFactorOptions fopts{};
    fopts.control = SparseDefaultControl;
    fopts.orderMethod = SparseOrderDefault;
    fopts.order = perm.data();
    fopts.ignoreRowsAndColumns = nullptr;
    fopts.malloc = std::malloc;
    fopts.free = std::free;
    fopts.reportError = [](const char *) {};

    SparseOpaqueSymbolicFactorization sym = SparseFactor(SparseFactorizationLDLTTPP, s, fopts);
    RecordProperty("default_status", std::to_string(static_cast<int>(sym.status)));
    std::string p;
    for (int i = 0; i < n; ++i)
        p += fmt::format("{}{}", perm[static_cast<std::size_t>(i)], i + 1 < n ? "," : "");
    RecordProperty("computed_perm", p.c_str());
    EXPECT_EQ(sym.status, SparseStatusOK);
    SparseCleanup(sym);

    // User ordering: identity, explicitly supplied.
    std::vector<int> ident(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        ident[static_cast<std::size_t>(i)] = i;
    fopts.orderMethod = SparseOrderUser;
    fopts.order = ident.data();
    SparseOpaqueSymbolicFactorization sym_user = SparseFactor(SparseFactorizationLDLTTPP, s, fopts);
    RecordProperty("user_status", std::to_string(static_cast<int>(sym_user.status)));
    EXPECT_EQ(sym_user.status, SparseStatusOK);
    SparseCleanup(sym_user);
}

// --- Checklist §(e): the LAPACKE shim over Accelerate's f77 LAPACK --------

// ipiv convention on the canonical indefinite 2x2 that Bunch-Kaufman must
// handle with a 2x2 block: C = [[0,1],[1,0]]. LAPACK-standard convention
// (uplo='L'): ipiv[k] == ipiv[k+1] < 0 marks the 2x2 block.
TEST(AccelerateProbe, ShimDsytrfIpivConventionIndefinite) {
    Eigen::MatrixXd C(2, 2);
    C << 0, 1, 1, 0;
    std::vector<lapack_int> ipiv(2, 0);
    const lapack_int info = LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'L', 2, C.data(), 2, ipiv.data());
    RecordProperty("info", std::to_string(info));
    RecordProperty("ipiv", fmt::format("{},{}", ipiv[0], ipiv[1]).c_str());
    EXPECT_EQ(info, 0);
    EXPECT_LT(ipiv[0], 0);
    EXPECT_EQ(ipiv[0], ipiv[1]);

    // And the solve against it must reproduce the known inverse action:
    // C * x = b with C = [[0,1],[1,0]] swaps entries.
    Eigen::VectorXd b(2);
    b << 3.0, 7.0;
    const lapack_int sinfo =
        LAPACKE_dsytrs(LAPACK_COL_MAJOR, 'L', 2, 1, C.data(), 2, ipiv.data(), b.data(), 2);
    EXPECT_EQ(sinfo, 0);
    EXPECT_NEAR(b(0), 7.0, 1e-14);
    EXPECT_NEAR(b(1), 3.0, 1e-14);
}

// info > 0 semantics on an exactly singular matrix: LAPACK convention is
// info = i (1-based) when D(i,i) is exactly zero.
TEST(AccelerateProbe, ShimDsytrfInfoOnExactSingular) {
    Eigen::MatrixXd C = Eigen::MatrixXd::Zero(2, 2);
    C(0, 0) = 1.0; // second pivot exactly zero
    std::vector<lapack_int> ipiv(2, 0);
    const lapack_int info = LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'L', 2, C.data(), 2, ipiv.data());
    RecordProperty("info", std::to_string(info));
    EXPECT_GT(info, 0);
}

// Layout guard: the shim supports only LAPACK_COL_MAJOR and mirrors
// LAPACKE's "parameter 1 is illegal" convention for anything else.
TEST(AccelerateProbe, ShimRejectsRowMajor) {
    Eigen::MatrixXd C(2, 2);
    C << 2, 0, 0, 3;
    std::vector<lapack_int> ipiv(2, 0);
    EXPECT_EQ(LAPACKE_dsytrf(LAPACK_ROW_MAJOR, 'L', 2, C.data(), 2, ipiv.data()), -1);
}

// Block-walk cross-check: factor a matrix through the shim and count negative
// eigenvalues with the SAME 1x1/2x2 decode convention
// DenseSymmetricFactor::block_evidence() uses (the walk migrated there from
// schur_complement.h's rebuild_schur()). Returns -1 on factorization failure.
Index shim_block_walk_neg_count(Eigen::MatrixXd C, std::vector<lapack_int> &ipiv) {
    const auto n = static_cast<lapack_int>(C.rows());
    ipiv.assign(static_cast<std::size_t>(n), 0);
    if (LAPACKE_dsytrf(LAPACK_COL_MAJOR, 'L', n, C.data(), n, ipiv.data()) != 0)
        return -1;
    Index neg = 0;
    Index k = 0;
    while (k < n) {
        const auto uk = static_cast<std::size_t>(k);
        if (ipiv[uk] > 0) {
            if (C(k, k) < 0.0)
                ++neg;
            k += 1;
        } else {
            const double a11 = C(k, k);
            const double a21 = C(k + 1, k);
            const double a22 = C(k + 1, k + 1);
            const double trace = a11 + a22;
            const double det = a11 * a22 - a21 * a21;
            const double disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
            if ((trace + disc) / 2.0 < 0.0)
                ++neg;
            if ((trace - disc) / 2.0 < 0.0)
                ++neg;
            k += 2;
        }
    }
    return neg;
}

TEST(AccelerateProbe, ShimBlockWalkMatchesKnownInertia) {
    // 3x3 indefinite with known inertia (2 pos, 1 neg; eigenvalues ~ {4.3,
    // 2.05, -3.35}). OBSERVED: all-1x1 pivots (ipiv = 1,2,3), so this case
    // exercises only the walk's 1x1 arm.
    Eigen::MatrixXd C3(3, 3);
    C3 << 4, 1, 0, 1, -3, 1, 0, 1, 2;
    std::vector<lapack_int> ipiv;
    EXPECT_EQ(shim_block_walk_neg_count(C3, ipiv), 1);
    RecordProperty("ipiv_3x3", fmt::format("{},{},{}", ipiv[0], ipiv[1], ipiv[2]).c_str());

    // The canonical forced-2x2 case: C = [[0,1],[1,0]], eigenvalues {+1, -1}.
    // This drives the walk's ELSE branch (the trace/discriminant 2x2 decode),
    // which the 3x3 case above never reaches (review finding: without this,
    // the 2x2 decode was dead code in this probe). OBSERVED: ipiv = (-2, -2).
    Eigen::MatrixXd C2(2, 2);
    C2 << 0, 1, 1, 0;
    EXPECT_EQ(shim_block_walk_neg_count(C2, ipiv), 1);
    ASSERT_EQ(ipiv.size(), 2u);
    RecordProperty("ipiv_2x2", fmt::format("{},{}", ipiv[0], ipiv[1]).c_str());
    EXPECT_LT(ipiv[0], 0); // proof the 2x2 arm actually ran
    EXPECT_EQ(ipiv[0], ipiv[1]);
}

} // namespace

#endif // USE_ACCELERATE_SPARSE
