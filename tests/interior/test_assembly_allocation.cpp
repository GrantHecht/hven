// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <memory>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/non_linear_program.h"

// UNITY-BUILD NOTE: this suite is compiled with UNITY_BUILD ON, so an anonymous
// namespace does not isolate these helpers from the other test TUs merged into
// the same batch. Every name below carries an alloc_/Alloc prefix for that
// reason.

// The counting hook.
//
// Eigen takes its dynamic storage from the C allocator -- Memory.h's
// aligned_malloc and handmade_aligned_malloc call std::malloc, and
// aligned_realloc calls std::realloc -- and never from ::operator new, so a
// replaced global operator new cannot see a matrix or a vector allocation at
// all. The counter therefore interposes the allocator entry points themselves
// and forwards to the C library's, which are available wherever the __libc_
// aliases are. Every entry point Eigen and operator new can reach is covered,
// not just malloc, so an allocation through any of them is counted rather than
// silently missed. glibc's remaining allocators (memalign, valloc, pvalloc,
// reallocarray) are left alone; nothing on the measured path calls them. The
// counter is thread-local and does nothing but return while disarmed, so no
// code outside the measured window is affected by its presence.
#if defined(__GLIBC__)
#define HVEN_TEST_HAS_ALLOC_COUNTER 1
extern "C" void *__libc_malloc(std::size_t);
extern "C" void *__libc_calloc(std::size_t, std::size_t);
extern "C" void *__libc_realloc(void *, std::size_t);
extern "C" void *__libc_memalign(std::size_t, std::size_t);
namespace {
thread_local bool alloc_counter_armed = false;
thread_local unsigned long alloc_counter_hits = 0;
inline void alloc_counter_note() {
    if (alloc_counter_armed) {
        alloc_counter_hits++;
    }
}
} // namespace
extern "C" void *malloc(std::size_t size) {
    void *block = __libc_malloc(size);
    alloc_counter_note();
    return block;
}
extern "C" void *calloc(std::size_t count, std::size_t size) {
    void *block = __libc_calloc(count, size);
    alloc_counter_note();
    return block;
}
extern "C" void *realloc(void *block, std::size_t size) {
    void *moved = __libc_realloc(block, size);
    alloc_counter_note();
    return moved;
}
extern "C" void *aligned_alloc(std::size_t alignment, std::size_t size) {
    void *block = __libc_memalign(alignment, size);
    alloc_counter_note();
    return block;
}
extern "C" int posix_memalign(void **out, std::size_t alignment, std::size_t size) {
    if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0) {
        return EINVAL;
    }
    void *block = __libc_memalign(alignment, size);
    alloc_counter_note();
    if (block == nullptr) {
        return ENOMEM;
    }
    *out = block;
    return 0;
}
#else
#define HVEN_TEST_HAS_ALLOC_COUNTER 0
namespace {
thread_local bool alloc_counter_armed = false;
thread_local unsigned long alloc_counter_hits = 0;
} // namespace
#endif

namespace {

constexpr double kAllocInf = std::numeric_limits<double>::infinity();

/// n = 2, one equality row, one upper-bounded row and one lower-bounded row,
/// so an assembly runs the objective piece and both constraint pieces and the
/// inequality piece owns the Hessian.
struct AllocTestProblem : hven::solvers::NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 3; }
    int num_jac_nonzeros() const override { return 6; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kAllocInf, -kAllocInf;
        xu << kAllocInf, kAllocInf;
        gl << 4.0, -kAllocInf, 1.0;
        gu << 4.0, 9.0, kAllocInf;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[0] + x[1] * x[1];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
        g[1] = 2.0 * x[1];
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + 2.0 * x[1];
        g[1] = x[0] * x[0] + x[1];
        g[2] = x[0] * x[1];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 1, 1, 2, 2;
        c << 0, 1, 0, 1, 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1;
        c << 0, 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 2.0;
        v[2] = 2.0 * x[0];
        v[3] = 1.0;
        v[4] = x[1];
        v[5] = x[0];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor + 2.0 * lambda[1];
        v[1] = lambda[2];
        v[2] = 2.0 * obj_factor;
    }
    std::string name() const override { return "AllocTestProblem"; }
};

} // namespace

// The counter must be able to see an allocation through every entry point it
// interposes, or the pin below would pass for the wrong reason on whichever
// one it was blind to.
TEST(NlpAdapterAllocationTest, TheCounterSeesEachInterposedAllocator) {
#if HVEN_TEST_HAS_ALLOC_COUNTER
    auto count_of = [](auto &&action) {
        alloc_counter_hits = 0;
        alloc_counter_armed = true;
        action();
        alloc_counter_armed = false;
        return alloc_counter_hits;
    };

    // Eigen's own path first: a dynamic vector, and a conservativeResize,
    // which is what reaches realloc.
    Eigen::VectorXd probe;
    EXPECT_GT(count_of([&] { probe = Eigen::VectorXd::Zero(1024); }), 0u) << "Eigen allocation";
    EXPECT_DOUBLE_EQ(probe.sum(), 0.0);
    EXPECT_GT(count_of([&] { probe.conservativeResize(2048); }), 0u) << "conservativeResize";
    // conservativeResize preserves the original entries but does not
    // zero-initialize the newly grown tail, so only the original head is
    // checked here.
    EXPECT_DOUBLE_EQ(probe.head(1024).sum(), 0.0);

    void *block = nullptr;
    EXPECT_GT(count_of([&] { block = std::malloc(64); }), 0u) << "malloc";
    EXPECT_GT(count_of([&] { block = std::realloc(block, 128); }), 0u) << "realloc";
    std::free(block);
    EXPECT_GT(count_of([&] { block = std::calloc(8, 8); }), 0u) << "calloc";
    std::free(block);
    EXPECT_GT(count_of([&] { block = std::aligned_alloc(64, 128); }), 0u) << "aligned_alloc";
    std::free(block);
    int rc = 0;
    EXPECT_GT(count_of([&] { rc = posix_memalign(&block, 64, 128); }), 0u) << "posix_memalign";
    EXPECT_EQ(rc, 0);
    std::free(block);
#else
    GTEST_SKIP() << "no allocator interposition on this platform";
#endif
}

// A steady-state assembly must not touch the allocator. The window is one
// complete engine-facing assembly -- NonLinearProgram::eval_kkt, the call the
// interior-point solver drives every iteration -- so it covers the pieces'
// residual writes, adjoint-gradient and Jacobian scatters, the consume-once
// multiplier record and the Hessian scatter, not only the host's evaluation
// entry points. The host holds every destination it writes and the model fills
// them in place, so after the first assembly has warmed both there is nothing
// left to allocate.
TEST(NlpAdapterAllocationTest, AWarmAssemblyAllocatesNothing) {
    auto problem = std::make_shared<AllocTestProblem>();
    auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);
    auto core = std::make_shared<hven::solvers::NLPAdapterCore>(model, problem->name());
    auto nlp = hven::solvers::make_nlp_program(core);

    Eigen::SparseMatrix<double, Eigen::RowMajor> kkt(nlp->kkt_dim_, nlp->kkt_dim_);
    nlp->analyze_sparsity(kkt);

    Eigen::VectorXd LE(1), LI(2);
    LE << 0.4;
    LI << 0.9, 0.2;
    double val = 0.0;
    Eigen::VectorXd PGX = Eigen::VectorXd::Zero(nlp->primal_vars_);
    Eigen::VectorXd AGX = Eigen::VectorXd::Zero(nlp->primal_vars_);
    Eigen::VectorXd FXE = Eigen::VectorXd::Zero(nlp->equal_cons_);
    Eigen::VectorXd FXI = Eigen::VectorXd::Zero(nlp->inequal_cons_);

    auto assemble_at = [&](const Eigen::VectorXd &x) {
        Eigen::Map<Eigen::VectorXd>(kkt.valuePtr(), kkt.nonZeros()).setZero();
        // Every destination is accumulated into, not assigned, so a fresh
        // assembly starts from an explicit zero.
        PGX.setZero();
        AGX.setZero();
        FXE.setZero();
        FXI.setZero();
        val = 0.0;
        nlp->eval_kkt(2.0, x, LE, LI, val, PGX, AGX, FXE, FXI, kkt);
    };

    // Warm-up: two assemblies at two iterates, so every buffer has reached its
    // final size and the iterate cache has been through an invalidation.
    Eigen::VectorXd x1(2), x2(2), x3(2);
    x1 << 1.3, 0.7;
    x2 << 2.1, -0.4;
    x3 << 1.9, 0.25;
    assemble_at(x1);
    assemble_at(x2);
    assemble_at(x1);

    // The storage every consumer reads through, before and after.
    const double *grad_before = core->gradient().data();
    const double *ce_before = core->equality_residuals().data();
    const double *ci_before = core->inequality_residuals().data();
    const double *je_before = core->equality_jacobian().valuePtr();
    const double *ji_before = core->inequality_jacobian().valuePtr();
    const double *hess_before = core->hessian().valuePtr();

    alloc_counter_hits = 0;
    alloc_counter_armed = true;
    assemble_at(x3);
    alloc_counter_armed = false;
    const unsigned long hits = alloc_counter_hits;

#if HVEN_TEST_HAS_ALLOC_COUNTER
    EXPECT_EQ(hits, 0u) << "a warm assembly reached the allocator " << hits << " times";
#else
    (void)hits; // no interposition on this platform; the identity check below stands alone
#endif

    // The portable half, and the reason the count is zero: nothing moved. A
    // by-value return assigned into a member would have swapped its buffer,
    // so a stable address here is the same fact the counter reports.
    EXPECT_EQ(core->gradient().data(), grad_before);
    EXPECT_EQ(core->equality_residuals().data(), ce_before);
    EXPECT_EQ(core->inequality_residuals().data(), ci_before);
    EXPECT_EQ(core->equality_jacobian().valuePtr(), je_before);
    EXPECT_EQ(core->inequality_jacobian().valuePtr(), ji_before);
    EXPECT_EQ(core->hessian().valuePtr(), hess_before);

    // And the assembly produced what it should, so the in-place refill is not
    // merely cheap but correct.
    EXPECT_DOUBLE_EQ(core->equality_residuals()[0], x3[0] + 2.0 * x3[1] - 4.0);
    EXPECT_DOUBLE_EQ(core->inequality_residuals()[0], x3[0] * x3[0] + x3[1] - 9.0);
    EXPECT_DOUBLE_EQ(core->inequality_residuals()[1], 1.0 - x3[0] * x3[1]);
    EXPECT_DOUBLE_EQ(core->gradient()[0], 2.0 * x3[0]);
    EXPECT_NEAR(FXE[0], x3[0] + 2.0 * x3[1] - 4.0, 1e-12);
    EXPECT_NEAR(PGX[0], 2.0 * 2.0 * x3[0], 1e-12);
    EXPECT_NEAR(kkt.coeff(1, 1), 2.0 * 2.0, 1e-12);
}
