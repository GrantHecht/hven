#pragma once

// soc.h -- the second-order-correction subproblem construction of the SQP
// driver, carved verbatim out of drivers/sqp_driver.h (phase-C S3, restructure
// only). The comments below still speak from that header's point of view: the
// SECOND-ORDER CORRECTION and MODEL EVALUATION notes they cite remain at the
// top of drivers/sqp_driver.h ("the header note's WARM START paragraph" is
// qp_engine.h's own, exactly as before the move).
//
// NOT SELF-CONTAINED BY DESIGN: `NlpEval` is defined in drivers/sqp_driver.h,
// which includes this header at the exact point the carved code stood -- after
// NlpEval's definition, before the driver class -- so the original definition
// order is preserved without a header cycle (this header never includes
// sqp_driver.h). Any other includer must have NlpEval complete first.

#include <stdexcept>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

namespace hven::solvers {

// SECOND-ORDER CORRECTION note for the derivation; this is the whole of the
// rhs-shift computation, factored out so it can be tested away from the
// driver's loop and away from any engine (mirrors qp_failure_is_retryable's
// own reason for being a free function). Returns a COPY of `qp` with only
// `be`/`bi` touched -- H, g, Ae, Ai, lower, upper are untouched, which is
// what lets the re-solve reuse qp_engine.h's hot-start K0 (see the header
// note's WARM START paragraph).
//
// `ev`/`ev_trial` are the model evaluations at the CURRENT iterate x and at
// the REJECTED trial point x + p (both already in hand -- see MODEL
// EVALUATION; this function makes no model call of its own). `p` is the
// rejected step (qs.x) and `ineq_active` is the rejected solve's own
// QpSolution::ineq_active (size qp.mi()).
//
//     be_soc = -cE(x) - (cE(x+p) - cE(x) - Je p)          (equalities)
//     bi_soc(j) = -cI(x)(j) - (cI(x+p)(j) - cI(x)(j) - Ji p(j)),
//         for j ACTIVE in `ineq_active` ONLY -- every other row's bi is
//         COPIED FROM qp.bi UNCHANGED. An inactive row's own linearization
//         was never the thing that failed, and shifting it risks
//         manufacturing a NEW active row the correction was never meant to
//         touch. (inequalities)
inline QpProblem build_soc_subproblem(const QpProblem &qp, const NlpEval &ev,
                                      const NlpEval &ev_trial, const Vec &p,
                                      const std::vector<bool> &ineq_active) {
    QpProblem soc_qp = qp;
    if (qp.me() > 0) {
        const Vec residual_e = ev_trial.ce - ev.ce - qp.Ae * p;
        soc_qp.be = -ev.ce - residual_e;
    }
    if (qp.mi() > 0) {
        if (static_cast<Index>(ineq_active.size()) != qp.mi()) {
            throw std::invalid_argument(fmt::format(
                "build_soc_subproblem: ineq_active has size {}, expected {} (= qp.mi())",
                ineq_active.size(), qp.mi()));
        }
        const Vec Ai_p = qp.Ai * p;
        Vec bi_soc = qp.bi; // UNCHANGED on inactive rows -- see the note above
        for (Index j = 0; j < qp.mi(); ++j) {
            if (ineq_active[j]) {
                const double residual_i = ev_trial.ci(j) - ev.ci(j) - Ai_p(j);
                bi_soc(j) = -ev.ci(j) - residual_i;
            }
        }
        soc_qp.bi = std::move(bi_soc);
    }
    return soc_qp;
}

} // namespace hven::solvers
