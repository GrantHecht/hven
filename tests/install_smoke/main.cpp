// Standalone install-tree smoke consumer for hven's install/export rules.
// Not part of hven's own build (see this directory's CMakeLists.txt) --
// built against an *installed* hven package via find_package(hven), the
// same way an external consumer would. Solves the canonical Ipopt HS071
// example (borrowed from tests/interior/test_nlp_solver.cpp) through the
// public NLPProblem/NLPSolver surface and checks the known optimum, so a
// green run proves both link-time (libhven.a's symbols actually resolve)
// and run-time (the sparse backend the installed package re-found is the
// real one) correctness -- not just "it configured".

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>

#include "hven/model/nlp_solver.h"

namespace {
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Hs071Problem : hven::solvers::NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kInf, 40.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                      Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                         Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071Problem"; }
};
} // namespace

int main() {
    hven::solvers::NLPSolver solver(std::make_shared<Hs071Problem>());

    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    const auto flag = solver.optimize(x0);

    if (flag != hven::ConvergenceFlags::CONVERGED) {
        std::fprintf(stderr, "install smoke: solve did not converge (flag=%d)\n",
                     static_cast<int>(flag));
        return 1;
    }

    const Eigen::VectorXd x = solver.return_x();
    const double f_expected = 17.0140172;
    double f_actual = 0.0;
    Hs071Problem{}.eval_f(x, f_actual);

    if (std::abs(f_actual - f_expected) > 1e-5) {
        std::fprintf(stderr,
                     "install smoke: objective %.7f does not match the known optimum %.7f\n",
                     f_actual, f_expected);
        return 1;
    }

    std::printf("install smoke: hven::solvers::NLPSolver converged, objective = %.7f (OK)\n",
                f_actual);
    return 0;
}
