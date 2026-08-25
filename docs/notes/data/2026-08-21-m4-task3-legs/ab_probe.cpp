// A/B probe: drives the interior-point engine through NonLinearProgram and
// dumps every deterministic thing a solve produces. Not committed.
#include <cstdio>
#include <limits>
#include <memory>
#include "hven/model/nlp_solver.h"

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
namespace { constexpr double kInf = std::numeric_limits<double>::infinity(); }

struct Hs071 : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1,1,1,1; xu << 5,5,5,5; gl << 25.0, 40.0; gu << kInf, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0]*x[3]*(x[0]+x[1]+x[2]) + x[2]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0]=x[3]*(2*x[0]+x[1]+x[2]); g[1]=x[0]*x[3]; g[2]=x[0]*x[3]+1.0; g[3]=x[0]*(x[0]+x[1]+x[2]); }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0]=x[0]*x[1]*x[2]*x[3]; g[1]=x[0]*x[0]+x[1]*x[1]+x[2]*x[2]+x[3]*x[3]; }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,0,0,0,1,1,1,1; c << 0,1,2,3,0,1,2,3; }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,1,1,2,2,2,3,3,3,3; c << 0,0,1,0,1,2,0,1,2,3; }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0]=x[1]*x[2]*x[3]; v[1]=x[0]*x[2]*x[3]; v[2]=x[0]*x[1]*x[3]; v[3]=x[0]*x[1]*x[2];
        v[4]=2*x[0]; v[5]=2*x[1]; v[6]=2*x[2]; v[7]=2*x[3]; }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double o, ConstEigenRef<Eigen::VectorXd> l,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0=x[0],x1=x[1],x2=x[2],x3=x[3];
        v[0]=o*2*x3+l[1]*2; v[1]=o*x3+l[0]*x2*x3; v[2]=l[1]*2; v[3]=o*x3+l[0]*x1*x3;
        v[4]=l[0]*x0*x3; v[5]=l[1]*2; v[6]=o*(2*x0+x1+x2)+l[0]*x1*x2; v[7]=o*x0+l[0]*x0*x2;
        v[8]=o*x0+l[0]*x0*x1; v[9]=l[1]*2; }
    std::string name() const override { return "Hs071"; }
};

struct Rosen : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 3; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -kInf,-kInf; xu << kInf,kInf; }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double a=1.0-x[0], b=x[1]-x[0]*x[0]; f=a*a+100.0*b*b; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        const double b=x[1]-x[0]*x[0]; g[0]=-2.0*(1.0-x[0])-400.0*x[0]*b; g[1]=200.0*b; }
    void eval_g(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,1,1; c << 0,0,1; }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double o, ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double b=x[1]-x[0]*x[0];
        v[0]=o*(2.0-400.0*b+800.0*x[0]*x[0]); v[1]=o*(-400.0*x[0]); v[2]=o*200.0; }
    std::string name() const override { return "Rosen"; }
};

template <class P> void run(const char *tag, Eigen::VectorXd x0, int level) {
    hven::solvers::NLPSolver solver(std::make_shared<P>());
    solver.optimizer_->set_print_level(level);
    std::printf("=== %s ===\n", tag);
    auto flag = solver.optimize(x0);
    Eigen::VectorXd x = solver.return_x();
    std::printf("flag=%d\n", static_cast<int>(flag));
    for (int i = 0; i < x.size(); i++) std::printf("x[%d]=%.17g\n", i, x[i]);
}

int main(int argc, char **argv) {
    const int level = argc > 1 ? std::atoi(argv[1]) : 0;
    Eigen::VectorXd a(4); a << 1.0, 5.0, 5.0, 1.0;
    run<Hs071>("hs071", a, level);
    Eigen::VectorXd b(2); b << -1.2, 1.0;
    run<Rosen>("rosenbrock", b, level);
    return 0;
}
