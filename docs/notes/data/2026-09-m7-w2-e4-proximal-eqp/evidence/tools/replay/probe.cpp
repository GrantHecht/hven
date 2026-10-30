// M7 W2 E4 fix2 scratch probe: the small analytic QPs behind the status regressions,
// with the full counter set printed. Not part of the tree.
#include <cstdio>
#include <string>
#include <hven/detail/qp/qp_engine.h>
#include "support/indefinite_fixtures.h"
using namespace hven;
using namespace hven::solvers;

static void show(const char *tag, const QpSolution &s) {
    const QpCounters &c = s.counters;
    std::printf("%-42s status=%d minors=%ld facts=%ld refuse=%ld residiters=%ld polish=%ld "
                "tpsolves=%ld tprej=%ld tpad=%ld tref=%ld tresc=%ld cert=%ld tol=%ld rej=%ld "
                "blk=%ld retreats=%ld zmexh=%ld zmcap=%ld x=",
                tag, (int)s.status, (long)c.minor_iters, (long)c.factorizations,
                (long)c.eqp_residual_refusals, (long)c.eqp_refine_iters, (long)c.eqp_polish_steps,
                (long)c.terminal_polish_solves, (long)c.terminal_polish_rejected,
                (long)c.terminal_polish_adopted, (long)c.terminal_mu_refines,
                (long)c.terminal_mu_refine_rescues, (long)c.dep_infeasible_certificates,
                (long)c.dep_certificate_toleranced, (long)c.dep_certificate_rejections,
                (long)c.dep_blocker_terminations, (long)c.dep_border_retreats,
                (long)c.zero_motion_exhausted, (long)c.zero_motion_cap_hits);
    for (Index i = 0; i < s.x.size() && i < 6; ++i) std::printf("%s%.10g", i ? "," : "", s.x(i));
    std::printf("\n");
}

static QpProblem rank_deficient_inconsistent() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = -1.0;
    Hd(1, 1) = -1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(2, 2);
    Aed << 1.0, 0.0, 1.0, 0.0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec(2);
    qp.be << 0.0, 1.0;
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << -1.0, -1e20;
    qp.upper = Vec(2);
    qp.upper << 1.0, 1e20;
    return qp;
}

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "all";
    for (const auto alg : {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        const char *an = alg == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refact";
        if (which == "all" || which == "indef") {
            const QpProblem qp = test_support::indefinite_equality_qp();
            QpOptions o;
            o.ws_algebra = alg;
            show((std::string("indefinite_equality_qp/") + an).c_str(), QpEngine{o}.solve(qp));
        }
        if (which == "indefsweep") {
            const QpProblem qp = test_support::indefinite_equality_qp();
            for (const int e2 : {1, 0}) {
                for (const double pd : {1e-8, 1e-10, 1e-6, 0.0}) {
                    for (const double dm : {1e-8, 0.0}) {
                        QpOptions o;
                        o.ws_algebra = alg;
                        o.independent_working_set = e2 != 0;
                        o.primal_delta = pd;
                        o.dual_mu = dm;
                        char tag[128];
                        std::snprintf(tag, sizeof(tag), "indef/%s e2=%d pd=%g dm=%g", an, e2, pd, dm);
                        show(tag, QpEngine{o}.solve(qp));
                    }
                }
            }
        }
        if (which == "all" || which == "t7fix2") {
            const QpProblem qp = rank_deficient_inconsistent();
            QpOptions o;
            o.ws_algebra = alg;
            o.dual_mu = 0.0;
            QpEngine eng{o};
            const QpSolution cold = eng.solve(qp);
            show((std::string("t7fix2-cold/") + an).c_str(), cold);
            show((std::string("t7fix2-warm/") + an).c_str(), eng.solve(qp, cold));
        }
    }
    return 0;
}
