// I18 corpus instrument: re-solves one D4QP dump on a fresh QpEngine (tests/qp/support/qp_dump.h, the
// loader of the worktree this evidence directory lives in, so both arms read hkc= alike) and prints one row.
// usage: corpus_replay DUMP [--algebra border|refactorize] [--feas TOL]
#include <cstdio>
#include <cstdlib>
#include <string>

#include "qp/support/qp_dump.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: corpus_replay DUMP [--algebra border|refactorize] [--feas TOL]\n");
        return 2;
    }
    hven::test_support::QpDump d = hven::test_support::load_qp_dump(argv[1]);
    hven::QpOptions o = d.opts;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string k = argv[i], v = argv[i + 1];
        if (k == "--algebra") {
            o.working_set_algebra = v == "border" ? hven::QpWorkingSetAlgebra::kSchurBorder
                                                  : hven::QpWorkingSetAlgebra::kRefactorize;
        } else if (k == "--feas") {
            o.feasibility_tol = std::atof(v.c_str());
        } else {
            std::fprintf(stderr, "unknown argument %s\n", k.c_str());
            return 2;
        }
    }
    d.threads = 1;
    const auto s = hven::test_support::solve_qp_dump(d, o);
    const auto k = hven::test_support::qp_dump_kkt(d, s);
    std::printf("status=%d minors=%ld facts=%ld eqp_refine=%ld border_refine=%ld f=%.12e eq=%.3e "
                "ineq=%.3e box=%.3e stat=%.3e sign=%.3e compl=%.3e hkc=%d\n",
                static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                static_cast<long>(s.counters.eqp_refine_steps),
                static_cast<long>(s.counters.border_refine_steps),
                hven::test_support::qp_dump_objective(d, s.x), k.eq, k.ineq, k.box,
                k.stationarity, k.sign, k.complementarity, o.hessian_known_convex ? 1 : 0);
    return 0;
}
