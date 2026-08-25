// A/B probe: drives the interior-point engine through NonLinearProgram and
// dumps every deterministic thing a solve produces. Not committed.
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
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

// Two LINEAR inequalities that cannot both hold: x0 >= 1 and x0 <= -1. The
// least-infeasible point is x0 = 0, the constraint gradients are constant and
// nonzero there, and no reduction in infeasibility is available from it -- so
// feasibility restoration converges at a still-infeasible point, which is the
// locally-infeasible classification and the arm that reaches the
// restoration-exit objective.
struct Infeas : NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 1; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -10.0; xu << 10.0; gl << 1.0, -kInf; gu << kInf, -1.0; }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0]*x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0*x[0]; }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0]; g[1] = x[0]; }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,1; c << 0,0; }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0; c << 0; }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v << 1.0, 1.0; }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double o, ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override { v[0] = o*2.0; }
    std::string name() const override { return "Infeas"; }
};

// The Maratos example: min 2(x0^2 + x1^2 - 1) - x0 subject to x0^2 + x1^2 = 1.
// The full Newton step from a point on the circle INCREASES the constraint
// violation, so the line search rejects it and the second-order correction is
// what recovers the step. The textbook trigger for SOC, and the only way to
// reach SocRecovery's own trial evaluation.
struct Maratos : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kInf, -kInf; xu << kInf, kInf; gl << 0.0; gu << 0.0; }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 2.0*(x[0]*x[0] + x[1]*x[1] - 1.0) - x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 4.0*x[0] - 1.0; g[1] = 4.0*x[1]; }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0]*x[0] + x[1]*x[1] - 1.0; }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,0; c << 0,1; }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r, Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0,1; c << 0,1; }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0*x[0]; v[1] = 2.0*x[1]; }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double o, ConstEigenRef<Eigen::VectorXd> l,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = o*4.0 + l[0]*2.0; v[1] = o*4.0 + l[0]*2.0; }
    std::string name() const override { return "Maratos"; }
};

using hven::solvers::InteriorPointSolver;

// One cell: configure the solver from PUBLIC settings, drive it through the
// chosen entry, print everything deterministic the solve produced.
// argv[1] = print level; argv[2] = optional cell-name filter (exact match), so
// one cell can be inspected or re-run on its own.
static const char *g_only = nullptr;
static bool selected(const char *tag) { return g_only == nullptr || std::string(tag) == g_only; }

template <class P, class Configure, class Drive>
void cell(const char *tag, Eigen::VectorXd x0, int level, Configure configure, Drive drive) {
    if (!selected(tag)) return;
    hven::solvers::NLPSolver solver(std::make_shared<P>());
    solver.optimizer_->set_print_level(level);
    configure(*solver.optimizer_);
    std::printf("=== %s ===\n", tag);
    auto flag = drive(solver, x0);
    Eigen::VectorXd x = solver.return_x();
    std::printf("flag=%d\n", static_cast<int>(flag));
    for (int i = 0; i < x.size(); i++) std::printf("x[%d]=%.17g\n", i, x[i]);
}

int main(int argc, char **argv) {
    const int level = argc > 1 ? std::atoi(argv[1]) : 0;
    g_only = argc > 2 ? argv[2] : nullptr;
    Eigen::VectorXd a(4); a << 1.0, 5.0, 5.0, 1.0;
    Eigen::VectorXd c(1); c << 0.2;
    Eigen::VectorXd m(2); m << 0.98, 0.19899748483246252;

    auto optimize = [](hven::solvers::NLPSolver &s, const Eigen::VectorXd &x0) {
        return s.optimize(x0);
    };
    auto solve = [](hven::solvers::NLPSolver &s, const Eigen::VectorXd &x0) {
        return s.solve(x0);
    };

    // LANG line search. Reaches ClassicMeritAcceptance::eval_rhs, which nothing
    // else in this instrument selects (the default is AUGLANG).
    cell<Hs071>("hs071_lang", a, level,
                [](InteriorPointSolver &s) {
                    s.set_opt_ls_mode(InteriorPointSolver::LineSearchModes::LANG);
                },
                optimize);

    // Filter acceptance. Reaches the generic trial-point evaluator, which the
    // classic-merit default never calls.
    cell<Hs071>("hs071_filter", a, level,
                [](InteriorPointSolver &s) {
                    s.settings().acceptance_strategy_ =
                        hven::solvers::AcceptanceStrategies::filter;
                    s.settings().barrier_governor_ =
                        hven::solvers::BarrierGovernors::monitored;
                },
                optimize);

    // Second-order correction. Reaches SocRecovery::eval_trial_constraints.
    cell<Maratos>("maratos_soc", m, level,
                  [](InteriorPointSolver &s) { s.settings().max_soc_ = 1; }, optimize);

    // Objective-free phase. Reaches the OPTNO arm of the evaluation switch.
    cell<Hs071>("hs071_optno", a, level,
                [](InteriorPointSolver &s) {
                    s.settings().soe_mode_ = InteriorPointSolver::AlgorithmModes::OPTNO;
                },
                solve);

    // Feasibility restoration, entered because the problem has no feasible
    // point. Reaches the restoration evaluation branch and the restoration-exit
    // objective sites.
    cell<Infeas>("infeasible_restoration", c, level,
                 [](InteriorPointSolver &s) {
                     s.settings().restoration_mode_ =
                         hven::solvers::RestorationModes::l1_nested;
                     s.settings().acceptance_strategy_ =
                         hven::solvers::AcceptanceStrategies::filter;
                     s.settings().barrier_governor_ =
                         hven::solvers::BarrierGovernors::monitored;
                 },
                 optimize);
    return 0;
}
