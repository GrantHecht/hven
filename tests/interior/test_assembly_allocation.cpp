#include <gtest/gtest.h>

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
// Eigen takes its dynamic storage from std::malloc -- Memory.h's
// aligned_malloc and handmade_aligned_malloc both call it -- and never from
// ::operator new, so a replaced global operator new cannot see a matrix or a
// vector allocation at all. The counter therefore interposes malloc itself and
// forwards to the C library's, which is available wherever __libc_malloc is.
// It is thread-local and does nothing but return while disarmed, so no code
// outside the measured window is affected by its presence.
#if defined(__GLIBC__)
#define HVEN_TEST_HAS_ALLOC_COUNTER 1
extern "C" void *__libc_malloc(std::size_t);
namespace {
thread_local bool alloc_counter_armed = false;
thread_local unsigned long alloc_counter_hits = 0;
} // namespace
extern "C" void *malloc(std::size_t size) {
    void *block = __libc_malloc(size);
    if (alloc_counter_armed) {
        alloc_counter_hits++;
    }
    return block;
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

/// Every model call one Hessian-bearing assembly makes, in the order the
/// pieces make them.
void alloc_run_one_assembly(hven::solvers::NLPAdapterCore &core, const Eigen::VectorXd &x,
                            const Eigen::VectorXd &le, const Eigen::VectorXd &li, double sigma) {
    double f = core.model_->eval_f(x);
    (void)f;
    core.refresh_gradient(x);
    core.refresh_residuals(x);
    core.refresh_jacobians(x);
    core.eval_hessian_values(x, sigma, le, li);
}

} // namespace

// The counter must be able to see an allocation, or the pin below would pass
// for the wrong reason. One deliberate dynamic vector, armed.
TEST(NlpAdapterAllocationTest, TheCounterSeesAnEigenAllocation) {
#if HVEN_TEST_HAS_ALLOC_COUNTER
    alloc_counter_hits = 0;
    alloc_counter_armed = true;
    Eigen::VectorXd probe(1024);
    probe.setZero();
    alloc_counter_armed = false;
    EXPECT_GT(alloc_counter_hits, 0u)
        << "the malloc hook did not observe an Eigen allocation, so the zero-allocation pin "
           "would be vacuous";
    EXPECT_DOUBLE_EQ(probe.sum(), 0.0);
#else
    GTEST_SKIP() << "no malloc interposition on this platform";
#endif
}

// A steady-state assembly must not touch the allocator. The host holds every
// destination it writes and the model rewrites its own storage in place, so
// after the first assembly has warmed both there is nothing left to allocate.
TEST(NlpAdapterAllocationTest, AWarmAssemblyAllocatesNothing) {
    auto problem = std::make_shared<AllocTestProblem>();
    auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);
    hven::solvers::NLPAdapterCore core(model, problem->name());

    Eigen::VectorXd x(2), le(1), li(2);
    x << 1.3, 0.7;
    le << 0.4;
    li << 0.9, 0.2;

    // Warm-up: two assemblies at two iterates, so every buffer has reached its
    // final size and the iterate cache has been through an invalidation.
    alloc_run_one_assembly(core, x, le, li, 2.0);
    Eigen::VectorXd x2(2);
    x2 << 2.1, -0.4;
    alloc_run_one_assembly(core, x2, le, li, 2.0);
    alloc_run_one_assembly(core, x, le, li, 2.0);

    // The storage every consumer reads through, before and after.
    const double *grad_before = core.gradient().data();
    const double *ce_before = core.equality_residuals().data();
    const double *ci_before = core.inequality_residuals().data();
    const double *je_before = core.equality_jacobian().valuePtr();
    const double *ji_before = core.inequality_jacobian().valuePtr();
    const double *hess_before = core.hessian().valuePtr();

    Eigen::VectorXd x3(2);
    x3 << 1.9, 0.25;
    alloc_counter_hits = 0;
    alloc_counter_armed = true;
    alloc_run_one_assembly(core, x3, le, li, 2.0);
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
    EXPECT_EQ(core.gradient().data(), grad_before);
    EXPECT_EQ(core.equality_residuals().data(), ce_before);
    EXPECT_EQ(core.inequality_residuals().data(), ci_before);
    EXPECT_EQ(core.equality_jacobian().valuePtr(), je_before);
    EXPECT_EQ(core.inequality_jacobian().valuePtr(), ji_before);
    EXPECT_EQ(core.hessian().valuePtr(), hess_before);

    // And the values are the ones the assembly should have produced, so the
    // in-place refill is not merely cheap but correct.
    EXPECT_DOUBLE_EQ(core.equality_residuals()[0], x3[0] + 2.0 * x3[1] - 4.0);
    EXPECT_DOUBLE_EQ(core.inequality_residuals()[0], x3[0] * x3[0] + x3[1] - 9.0);
    EXPECT_DOUBLE_EQ(core.inequality_residuals()[1], 1.0 - x3[0] * x3[1]);
    EXPECT_DOUBLE_EQ(core.gradient()[0], 2.0 * x3[0]);
    EXPECT_DOUBLE_EQ(core.equality_jacobian().coeff(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(core.inequality_jacobian().coeff(0, 0), 2.0 * x3[0]);
    EXPECT_DOUBLE_EQ(core.hessian().coeff(1, 1), 2.0 * 2.0);
}
