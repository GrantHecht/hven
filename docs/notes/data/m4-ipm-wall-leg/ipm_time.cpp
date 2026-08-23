// IPM wall probe. The standing SQP bench does not reach InteriorPointSolver, so
// the consumption switch is measured here: repeated whole solves of a mid-size
// NLP through NLPSolver, which is the driver the retargeted evaluation path
// sits under. Not committed.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

#include "hven/model/nlp_solver.h"

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
namespace { constexpr double kInf = std::numeric_limits<double>::infinity(); }

// A dense-Jacobian quadratic-constraint problem, sized by N. Dense rows make
// the KKT assembly (and so the evaluation path) a real share of each iteration.
template <int N>
struct Wide : NLPProblem {
    int num_vars() const override { return N; }
    int num_cons() const override { return N; }
    int num_jac_nonzeros() const override { return N * N; }
    int num_hess_nonzeros() const override { return N; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kInf); xu.setConstant(kInf); gl.setZero(); gu.setZero(); }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm(); }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x; }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        for (int r = 0; r < N; r++) g[r] = x[r] + 0.5 * x[(r + 1) % N] + x[r] * x[r] - 0.25; }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        int k = 0; for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) { r[k] = i; c[k] = j; k++; } }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < N; i++) { r[i] = i; c[i] = i; } }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setZero();
        for (int r = 0; r < N; r++) { v[r * N + r] = 1.0 + 2.0 * x[r]; v[r * N + ((r + 1) % N)] += 0.5; } }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double o, ConstEigenRef<Eigen::VectorXd> l,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        for (int i = 0; i < N; i++) v[i] = o + 2.0 * l[i]; }
    std::string name() const override { return "Wide"; }
};

template <int N> double solve_once(int &iters, int &flag, double &obj) {
    hven::solvers::NLPSolver solver(std::make_shared<Wide<N>>());
    solver.optimizer_->set_print_level(3);
    Eigen::VectorXd x0 = Eigen::VectorXd::LinSpaced(N, -0.5, 0.5);
    auto t0 = std::chrono::steady_clock::now();
    auto exit_flag = solver.optimize(x0);
    auto t1 = std::chrono::steady_clock::now();
    // The ITERATION COUNT and the flag are two different answers and this used
    // to print the flag under both names -- `iters` was the convergence flag
    // cast to int, so every row read `iters=0` and the count that a code-path
    // change would actually move was never in the record.
    flag = static_cast<int>(exit_flag);
    iters = solver.optimizer_->result().iter_num_;
    obj = solver.return_x().squaredNorm();
    return std::chrono::duration<double>(t1 - t0).count();
}

template <int N> void arm(const char *tag, int reps) {
    std::vector<double> s;
    int iters = 0, flag = 0;
    double obj = 0.0;
    for (int r = 0; r < reps; r++)
        s.push_back(solve_once<N>(iters, flag, obj));
    std::sort(s.begin(), s.end());
    std::printf("%s n=%d reps=%d median_s=%.6f min_s=%.6f iters=%d flag=%d xnorm2=%.17g\n", tag, N,
                reps, s[s.size() / 2], s.front(), iters, flag, obj);
}

int main(int argc, char **argv) {
    const int reps = argc > 1 ? std::atoi(argv[1]) : 9;
    arm<60>("wide", reps);
    arm<120>("wide", reps);
    arm<240>("wide", reps);
    return 0;
}
