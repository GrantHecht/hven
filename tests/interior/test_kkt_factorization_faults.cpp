// Fault-injected coverage for the interior-point engine's KKT factorization on
// the paths a real backend cannot be made to take from any fixture: a numeric
// factorization that fails, and a symbolic analysis that fails. Those are the
// two paths where the engine's evidence projection is BACKEND-SPECIFIC, so
// each half below is written against the backend it applies to and only one is
// ever compiled.
//
// Compiled ONLY into the standalone hven_fault_injection_tests executable
// (tests/CMakeLists.txt), alongside tests/linear/test_fault_injection.cpp and
// for the same reason: that is the one target where
// hven/detail/linear/fault_injection.h's injectors exist at all.
//
// What is being pinned is FIDELITY, not preference. The engine's previous
// backend interfaces categorized their own backends' error codes into
// Eigen::ComputationInfo differently, and left different values behind on a
// failed factorization. Those categorizations and those values are what the
// engine's inertia ladder and its diagnostics were written against, so a code
// landing in a different category, or an invalid count reaching a place that
// used to see a defined zero, is a behavior change however honest the new
// answer is.

#include <vector>

#include <gtest/gtest.h>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/detail/linear/fault_injection.h"

#if defined(USE_ACCELERATE_SPARSE)
#include <Accelerate/Accelerate.h>
#endif

namespace {

using hven::SpMatRM;
using hven::linear::detail::testing::AnalyzeFaultInjector;
using hven::linear::detail::testing::FactorizeFaultInjector;
using hven::solvers::KktFactorization;

// An ordinary, healthy 2x2 system: diag(2, -3), one positive and one negative
// eigenvalue. Every test below makes the INJECTED fault the reason a
// factorization does not happen; the matrix itself is unremarkable.
SpMatRM fault_probe_matrix() {
    std::vector<Eigen::Triplet<double>> entries{{0, 0, 2.0}, {1, 1, -3.0}};
    SpMatRM m(2, 2);
    m.setFromTriplets(entries.begin(), entries.end());
    m.makeCompressed();
    return m;
}

KktFactorization::Options fault_probe_options() {
    KktFactorization::Options opts;
    opts.kind = hven::linear::FactorKind::kLDLT;
    opts.num_threads = 1;
    return opts;
}

// Both injectors are process-global statics, so every test that arms one
// disarms it through this guard rather than at the end of the test body --
// an assertion failure must not leave the next test running under an injected
// fault.
struct ArmedFactorizeFault {
    explicit ArmedFactorizeFault(int backend_code) {
        FactorizeFaultInjector::injected_backend_code = backend_code;
        FactorizeFaultInjector::active = true;
    }
    ~ArmedFactorizeFault() { FactorizeFaultInjector::active = false; }
};

struct ArmedAnalyzeFault {
    ArmedAnalyzeFault() { AnalyzeFaultInjector::active = true; }
    ~ArmedAnalyzeFault() { AnalyzeFaultInjector::active = false; }
};

// --- the symbolic-failure path, identical on both backends -----------------
//
// The engine's previous interfaces recorded the backend's status and returned,
// leaving the caller to carry on against a factorization that does not exist.
// The exception the linear surface raises instead must not reach the engine.

TEST(KktFactorizationFaultTest, SymbolicFailureIsRecordedRatherThanPropagated) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    {
        ArmedAnalyzeFault armed;
        EXPECT_NO_THROW(kkt.compute());
    }

    EXPECT_EQ(kkt.info(), Eigen::InvalidInput);
    // Nothing was factorized, so nothing has a size or a cost.
    EXPECT_EQ(kkt.factor_mem(), 0);
    EXPECT_EQ(kkt.factor_flops(), 0);
}

TEST(KktFactorizationFaultTest, SymbolicFailureLeavesTheObjectAnalyzableAgain) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    {
        ArmedAnalyzeFault armed;
        kkt.compute();
    }

    // The failed analysis committed nothing, so the next attempt is an
    // ordinary one.
    EXPECT_NO_THROW(kkt.compute());
    EXPECT_EQ(kkt.info(), Eigen::Success);
    EXPECT_EQ(kkt.peigs(), 1);
    EXPECT_EQ(kkt.neigs(), 1);
}

TEST(KktFactorizationFaultTest, AMalformedAssemblyIsACallerErrorAndStillThrows) {
    KktFactorization kkt(fault_probe_options());
    // No structural diagonal in row 1: an assembly bug, not a numeric outcome,
    // and the record-and-continue path above must not absorb it.
    std::vector<Eigen::Triplet<double>> entries{{0, 0, 2.0}, {0, 1, 1.0}};
    SpMatRM m(2, 2);
    m.setFromTriplets(entries.begin(), entries.end());
    m.makeCompressed();
    kkt.matrix() = m;

    EXPECT_THROW(kkt.compute(), std::invalid_argument);
}

#if defined(USE_ACCELERATE_SPARSE)

// --- Accelerate: the numeric-failure path ----------------------------------

TEST(KktFactorizationFaultTest, AccelerateNumericRefusalIsRecordedAsANumericalIssue) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    {
        ArmedFactorizeFault armed(static_cast<int>(SparseFactorizationFailed));
        kkt.compute();
    }
    EXPECT_EQ(kkt.info(), Eigen::NumericalIssue);

    {
        ArmedFactorizeFault armed(static_cast<int>(SparseMatrixIsSingular));
        kkt.compute();
    }
    EXPECT_EQ(kkt.info(), Eigen::NumericalIssue);
}

TEST(KktFactorizationFaultTest, AccelerateHardErrorsAreRecordedAsInvalidInput) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    for (const int code :
         {static_cast<int>(SparseParameterError), static_cast<int>(SparseInternalError),
          static_cast<int>(SparseStatusReleased)}) {
        ArmedFactorizeFault armed(code);
        kkt.compute();
        EXPECT_EQ(kkt.info(), Eigen::InvalidInput) << "backend status " << code;
    }
}

TEST(KktFactorizationFaultTest, AccelerateFailedFactorizationZeroFillsItsEvidence) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    // A successful factorization first, so the values below are demonstrably
    // reset rather than merely never written.
    kkt.compute();
    ASSERT_EQ(kkt.peigs(), 1);
    ASSERT_GT(kkt.factor_mem(), 0);

    {
        ArmedFactorizeFault armed(static_cast<int>(SparseFactorizationFailed));
        kkt.compute();
    }

    // The deterministic zero-fill the engine's previous Accelerate interface
    // produced on every abandoned factorization -- reproduced deliberately,
    // not inherited: the linear layer's own answer here is an invalid count,
    // which the engine has no way to read as one.
    EXPECT_EQ(kkt.peigs(), 0);
    EXPECT_EQ(kkt.neigs(), 0);
    EXPECT_EQ(kkt.ppivs(), 0);
    EXPECT_EQ(kkt.factor_mem(), 0);
    EXPECT_EQ(kkt.factor_flops(), 0);
}

#else

// --- MKL: the numeric-failure path -----------------------------------------

TEST(KktFactorizationFaultTest, PardisoZeroPivotCodesAreRecordedAsANumericalIssue) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    // -4 and -7 are the zero / near-zero pivot codes, the ordinary outcome of
    // probing a perturbation during inertia correction: recorded, never
    // printed as a hard error.
    for (const int code : {-4, -7}) {
        ArmedFactorizeFault armed(code);
        kkt.compute();
        EXPECT_EQ(kkt.info(), Eigen::NumericalIssue) << "backend error " << code;
    }
}

TEST(KktFactorizationFaultTest, PardisoOtherErrorsAreRecordedAsInvalidInput) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    // -1 input inconsistent, -2 out of memory, -3 reordering problem: hard
    // errors, which the ladder does surface.
    for (const int code : {-1, -2, -3}) {
        ArmedFactorizeFault armed(code);
        kkt.compute();
        EXPECT_EQ(kkt.info(), Eigen::InvalidInput) << "backend error " << code;
    }
}

TEST(KktFactorizationFaultTest, PardisoFailedFactorizationReportsNoInertia) {
    KktFactorization kkt(fault_probe_options());
    kkt.matrix() = fault_probe_matrix();

    {
        ArmedFactorizeFault armed(-4);
        kkt.compute();
    }

    // This backend's previous interface read its parameter array
    // unconditionally after a failed call, so it had no defined values to
    // preserve. The linear layer's invalid counts pass through, and the
    // ladder's own singularity test reads them as a system to perturb.
    EXPECT_LT(kkt.peigs(), 0);
    EXPECT_LT(kkt.neigs(), 0);
    EXPECT_EQ(kkt.ppivs(), 0);
}

#endif

} // namespace
