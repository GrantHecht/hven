// Accelerate-backend audit probes (macOS only) — the measurement instruments
// for docs/notes/2026-07-28-accelerate-audit-checklist.md sections (a)-(c)
// and (e), reported in docs/notes/2026-07-29-accelerate-audit-results.md.
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

#include <hven/detail/sqp/kkt_system.h>
#include <hven/detail/sqp/lapacke_shim.h>

using namespace hven::solvers;

namespace {

// The checklist's canonical 3x3 probe matrices (upper triangle, CSR).
SpMatU dense_to_upper(const Eigen::MatrixXd &D) {
    SpMatU K = D.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K.makeCompressed();
    return K;
}

SpMatU near_singular_kkt(double eps) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, eps, 0, 1, 0, 0;
    return dense_to_upper(D);
}

SpMatU saddle_kkt3() {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    return dense_to_upper(D);
}

Eigen::MatrixXd full_from_upper(const SpMatU &K) {
    return Eigen::MatrixXd(K).selfadjointView<Eigen::Upper>();
}

// --- Checklist §(a): inertia reporting across the eps sweep ---------------

TEST(AccelerateProbe, InertiaEpsSweep) {
    for (double eps : {1e-6, 1e-10, 1e-12, 1e-14}) {
        KktSystem kkt{QpOptions{}};
        kkt.factorize(near_singular_kkt(eps));
        const std::string tag = fmt::format("{:.0e}", eps);
        RecordProperty(fmt::format("pos_{}", tag).c_str(), std::to_string(kkt.num_pos_eigs()));
        RecordProperty(fmt::format("neg_{}", tag).c_str(), std::to_string(kkt.num_neg_eigs()));
        RecordProperty(fmt::format("zero_{}", tag).c_str(),
                       std::to_string(kkt.num_perturbed_pivots()));
        // Analytic inertia is (2, 1, 0) for every eps > 0.
        EXPECT_EQ(kkt.num_pos_eigs() + kkt.num_neg_eigs() + kkt.num_perturbed_pivots(), 3)
            << "eps=" << eps;

        // Solve accuracy alongside the inertia claim.
        Vec b(3);
        b << 1, 2, 3;
        const Vec x = kkt.solve(b);
        const double resid = (full_from_upper(near_singular_kkt(eps)) * x - b).norm();
        RecordProperty(fmt::format("resid_{}", tag).c_str(), fmt::format("{:.3e}", resid));
        EXPECT_TRUE(std::isfinite(resid)) << "eps=" << eps;
    }
}

// §(a): the exactly-singular case. Pardiso's pinned behavior: no throw,
// silently perturbs one pivot, reports (2, 1, perturbed=1). What does
// Accelerate LDLTTPP do — complete with a zero pivot, or fail the
// factorization with SparseMatrixIsSingular (a throw from factorize())?
TEST(AccelerateProbe, ExactlySingularBehavior) {
    KktSystem kkt{QpOptions{}};
    bool threw = false;
    std::string what;
    try {
        kkt.factorize(near_singular_kkt(0.0));
    } catch (const std::runtime_error &e) {
        threw = true;
        what = e.what();
    }
    RecordProperty("threw", threw ? "yes" : "no");
    RecordProperty("what", what.c_str());
    if (!threw) {
        RecordProperty("pos", std::to_string(kkt.num_pos_eigs()));
        RecordProperty("neg", std::to_string(kkt.num_neg_eigs()));
        RecordProperty("zero", std::to_string(kkt.num_perturbed_pivots()));
    }
    SUCCEED(); // the recorded properties are the deliverable
}

// --- Checklist §(b): partial-solve composition ----------------------------

// Clean (unperturbed on MKL) saddle matrix: does the bare L/D/L^T composition
// reproduce the full solve on Accelerate, where the internal AMD permutation
// P and inf-norm equilibration S are NOT folded into the subfactor stages?
TEST(AccelerateProbe, ComposedPartialsVsFullSolve) {
    KktSystem kkt{QpOptions{}};
    kkt.factorize(saddle_kkt3());
    Vec b(3);
    b << 1, 2, 3;
    const Vec x_full = kkt.solve(b);
    const Vec x_comp = kkt.solve_backward(kkt.solve_diagonal(kkt.solve_forward(b)));
    const double diff = (x_full - x_comp).norm();
    const double full_resid = (full_from_upper(saddle_kkt3()) * x_full - b).norm();
    RecordProperty(
        "x_full", fmt::format("[{:.6g}, {:.6g}, {:.6g}]", x_full(0), x_full(1), x_full(2)).c_str());
    RecordProperty(
        "x_comp", fmt::format("[{:.6g}, {:.6g}, {:.6g}]", x_comp(0), x_comp(1), x_comp(2)).c_str());
    RecordProperty("diff", fmt::format("{:.3e}", diff).c_str());
    RecordProperty("full_resid", fmt::format("{:.3e}", full_resid).c_str());
    EXPECT_LT(full_resid, 1e-10); // the full solve itself must be trustworthy
}

// Same question on a matrix big enough for AMD to actually reorder (arrow
// matrix: dense last row/column), so a "composition happens to match because
// P == I" false pass is ruled out.
TEST(AccelerateProbe, ComposedPartialsVsFullSolveArrow) {
    const int n = 8;
    Eigen::MatrixXd D = Eigen::MatrixXd::Zero(n, n);
    for (int i = 0; i < n - 1; ++i) {
        D(i, i) = 2.0 + i;
        D(i, n - 1) = 1.0;
    }
    D(n - 1, n - 1) = -1.0; // indefinite tip
    SpMatU K = dense_to_upper(D);
    KktSystem kkt{QpOptions{}};
    kkt.factorize(K);
    Vec b = Vec::LinSpaced(n, 1.0, static_cast<double>(n));
    const Vec x_full = kkt.solve(b);
    const Vec x_comp = kkt.solve_backward(kkt.solve_diagonal(kkt.solve_forward(b)));
    const double diff = (x_full - x_comp).norm();
    const double full_resid = (full_from_upper(K) * x_full - b).norm();
    RecordProperty("diff", fmt::format("{:.3e}", diff).c_str());
    RecordProperty("full_resid", fmt::format("{:.3e}", full_resid).c_str());
    RecordProperty("pos", std::to_string(kkt.num_pos_eigs()));
    RecordProperty("neg", std::to_string(kkt.num_neg_eigs()));
    EXPECT_LT(full_resid, 1e-9);
}

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
// eigenvalues with the SAME 1x1/2x2 decode convention schur_complement.h's
// rebuild_schur() uses. Returns -1 on factorization failure.
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
