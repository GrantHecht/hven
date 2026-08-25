// Gated-vs-ungated identity probe for the Task 9 epoch gate.
//
// Three problems, solved through NLPSolver at the default objective scale, with
// every reported number printed in %a. Two arms differing only in whether the
// per-factorization pattern guard runs must print byte-identical output: the
// guard reads the matrix and decides whether to throw, and feeds nothing into
// the factorization.
#include <cstdio>
#include <limits>
#include <memory>
#include <string>

#include "hven/model/nlp_solver.h"

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
namespace { constexpr double kInf = std::numeric_limits<double>::infinity(); }

// Problem 1: dense-Jacobian quadratic constraints, the wall leg's family.
template <int N> struct Wide : NLPProblem {
    int num_vars() const override { return N; }
    int num_cons() const override { return N; }
    int num_jac_nonzeros() const override { return N * N; }
    int num_hess_nonzeros() const override { return N; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kInf); xu.setConstant(kInf); gl.setZero(); gu.setZero(); }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = 0.5 * x.squaredNorm(); }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override { g = x; }
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

// Problem 2: HS071 -- bounds on every variable, one range row, one equality row.
struct Hs071 : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0; xu << 5.0, 5.0, 5.0, 5.0; gl << 25.0, 40.0; gu << kInf, 40.0; }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]); g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0; g[3] = x[0] * (x[0] + x[1] + x[2]); }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0]*x[0] + x[1]*x[1] + x[2]*x[2] + x[3]*x[3]; }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,0,0,0,1,1,1,1; c << 0,1,2,3,0,1,2,3; }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,1,1,2,2,2,3,3,3,3; c << 0,0,1,0,1,2,0,1,2,3; }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0]=x[1]*x[2]*x[3]; v[1]=x[0]*x[2]*x[3]; v[2]=x[0]*x[1]*x[3]; v[3]=x[0]*x[1]*x[2];
        v[4]=2.0*x[0]; v[5]=2.0*x[1]; v[6]=2.0*x[2]; v[7]=2.0*x[3]; }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double o, ConstEigenRef<Eigen::VectorXd> l,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0]=o*2.0*x[3];                          v[1]=o*x[3];
        v[2]=0.0;                                  v[3]=o*x[3];
        v[4]=0.0;                                  v[5]=0.0;
        v[6]=o*(2.0*x[0]+x[1]+x[2]);               v[7]=o*x[0];
        v[8]=o*x[0];                               v[9]=0.0;
        v[0]+=l[0]*0.0 + l[1]*2.0; v[2]+=l[1]*2.0; v[5]+=l[1]*2.0; v[9]+=l[1]*2.0;
        v[1]+=l[0]*x[2]*x[3]; v[3]+=l[0]*x[1]*x[3]; v[4]+=l[0]*x[0]*x[3];
        v[6]+=l[0]*x[1]*x[2]; v[7]+=l[0]*x[0]*x[2]; v[8]+=l[0]*x[0]*x[1]; }
    std::string name() const override { return "HS071"; }
};

// Problem 3: an equality-constrained box, so a bound-bearing classification and
// an active bound multiplier are both in the picture.
struct Boxed : NLPProblem {
    static constexpr int N = 6;
    int num_vars() const override { return N; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return N; }
    int num_hess_nonzeros() const override { return N; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(0.2); xu.setConstant(2.0); gl.setConstant(3.0); gu.setConstant(3.0); }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = 0.5 * x.squaredNorm(); }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override { g = x; }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override { g[0] = x.sum(); }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < N; i++) { r[i] = 0; c[i] = i; } }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < N; i++) { r[i] = i; c[i] = i; } }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override { v.setConstant(1.0); }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double o, ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override { v.setConstant(o); }
    std::string name() const override { return "Boxed"; }
};

void dump(const char *tag, hven::solvers::NLPSolver &solver, hven::ConvergenceFlags flag) {
    const auto &r = solver.optimizer_->result();
    std::printf("%s flag=%d iters=%d obj=%a\n", tag, static_cast<int>(flag), r.iter_num_, r.obj_val_);
    const Eigen::VectorXd x = solver.return_x();
    for (int i = 0; i < x.size(); i++) std::printf("%s x[%d]=%a\n", tag, i, x[i]);
    for (int i = 0; i < r.eq_lmults_.size(); i++) std::printf("%s eq[%d]=%a\n", tag, i, r.eq_lmults_[i]);
    for (int i = 0; i < r.iq_lmults_.size(); i++) std::printf("%s iq[%d]=%a\n", tag, i, r.iq_lmults_[i]);
    for (int i = 0; i < r.bound_lmults_.size(); i++) std::printf("%s z[%d]=%a\n", tag, i, r.bound_lmults_[i]);
    for (int i = 0; i < r.eq_cons_.size(); i++) std::printf("%s ce[%d]=%a\n", tag, i, r.eq_cons_[i]);
    for (int i = 0; i < r.iq_cons_.size(); i++) std::printf("%s ci[%d]=%a\n", tag, i, r.iq_cons_[i]);
}

template <class P> void run(const char *tag, const Eigen::VectorXd &x0) {
    hven::solvers::NLPSolver solver(std::make_shared<P>());
    solver.optimizer_->set_print_level(3);
    dump(tag, solver, solver.optimize(x0));

    // A SECOND solve on the same instance, with a partition renegotiation in
    // between: the sequence the epoch gate exists for. The base arm re-lays and
    // does not re-analyze; whether it survives at all is the finding, and where
    // it does the numbers must still agree.
    solver.nlp_->negotiate_partition_count(1);
    try {
        const std::string second = std::string(tag) + "/re";
        dump(second.c_str(), solver, solver.optimize(x0));
    } catch (const std::exception &error) {
        std::printf("%s/re THREW %s\n", tag, error.what());
    }
}

int main() {
    run<Wide<40>>("wide40", Eigen::VectorXd::LinSpaced(40, -0.5, 0.5));
    run<Hs071>("hs071", (Eigen::VectorXd(4) << 1.0, 5.0, 5.0, 1.0).finished());
    run<Boxed>("boxed", Eigen::VectorXd::Constant(Boxed::N, 0.6));
    return 0;
}
