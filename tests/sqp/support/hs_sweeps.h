#pragma once

// tests/support/hs_sweeps.h — test-support only, NOT part of the public
// library surface. PHASE-5 TASK 7: a NONCONVEX PARAMETRIC CORPUS, built by
// re-posing six of hs_problems.h's Hock-Schittkowski models along a
// one-dimensional parameter.
//
// =====================================================================
// WHY THIS EXISTS, AND WHY IT IS NOT parametric_families.h
//
// Phase 4 shipped seven synthetic parametric families (F1-F7,
// tests/support/parametric_families.h) with analytic solution paths, and the
// Task-13 warm-start battery measured the whole warm-start subsystem on them.
// That corpus answered the economics questions it was built for and left TWO
// POLICY LEVERS UNADJUDICATED, for the same underlying reason in both cases:
// F1-F7 are CONVEX, their linearizations are never badly misleading, and the
// globalization therefore never has to fight.
//
//   (a) SqpOptions::warm_full_step -- the Kungurtsev-Diehl full-step-first
//       rule (sqp_driver.h's FULL-STEP-FIRST WARM note). The battery returned
//       a NULL result: the mode ENGAGED on 67-88 % of warm/hot majors and its
//       watchdog NEVER fired, yet not one observed count moved when the lever
//       was flipped. Default-true was neither supported nor undermined, and
//       the human ruling was to keep it and re-adjudicate on a corpus that can
//       discriminate.
//   (b) kappa_soc -- the never-adopted magnitude gate on the SECOND-ORDER
//       CORRECTION attempt (sqp_driver.h's A NAMED CANDIDATE FOR TASK 11).
//       Phase 3's HS battery saw SOC attempted TWICE in 27 problems; the
//       Phase-4 parametric corpus never reaches SOC AT ALL, so the outcome
//       counters Phase-4 Task 1 added (SqpCounters::soc_applied /
//       soc_qp_infeasible / soc_rejected) have had no data to report.
//
// Both levers only speak on problems where a trial step can be REJECTED and
// where a rejected trial can INCREASE the constraint violation. That is what
// this header supplies: nonconvex objectives, reverse-convex and product-form
// constraints, published start points that are already infeasible, and long
// parameter steps that land a warm start far enough from x*(p) for the funnel
// to have something to decide.
//
// =====================================================================
// THE CONSTRUCTION -- AN AFFINE RE-POSING, DELIBERATELY THE SIMPLEST ONE THAT
// MOVES THE SOLUTION
//
// For a base model (f, cE, cI, l, u, x0) from hs_problems.h, a tilt vector t,
// an equality shift sE and an inequality shift sI, HsSweep is the NLP
//
//     min   f(x) + p * t^T x
//     s.t.  cE(x) - p * sE = 0
//           cI(x) - p * sI <= 0
//           l <= x <= u
//
// with the base model's own start_point(). p is a scalar (parameter_dim() ==
// 1). At p = 0 it IS the published Hock-Schittkowski problem, so the whole
// corpus is anchored on transcriptions that Phase 3 already source-checked
// and derivative-checked; nothing here re-derives a formula.
//
// WHY AFFINE IN p AND NOT SOMETHING RICHER. The three requirements on a
// parametric family in this project (nlp_model.h's ParametricNlpModel
// preconditions, plus what the tangential predictor assumes) are that
// set_parameters change VALUES ONLY, that the sparsity pattern be independent
// of x AND p, and that the family be differentiable in p. An affine re-posing
// gets all three for free and, critically, gets them WITHOUT TOUCHING ANY
// SECOND DERIVATIVE:
//
//   - the tilt is LINEAR in x, so it moves eval_grad by exactly p*t and moves
//     eval_hess NOT AT ALL;
//   - the shifts are CONSTANT in x, so they move eval_ce/eval_ci by a constant
//     and move eval_jac_e/eval_jac_i/eval_hess NOT AT ALL.
//
// So every derivative this header forwards is the base model's own, already
// verified, and the only new arithmetic is a vector add. The NONCONVEXITY --
// the property the whole corpus exists for -- is therefore inherited exactly:
// hess(f) and every hess(cI_j) are the published problem's, unchanged at every
// p. A richer p-dependence (scaling a constraint, curving the objective) would
// have to re-derive Hessians and would put a fresh transcription between the
// adjudication and the published problem, which is the one thing this corpus
// cannot afford: if the numbers are disputed, the dispute must be about the
// SOLVER, not about whether the model is HS40.
//
// STRUCTURAL PATTERN INVARIANCE (nlp_model.h's binding precondition, in both
// its x form and its p form) HOLDS BY CONSTRUCTION AND IS NOT MERELY HOPED
// FOR: eval_jac_e, eval_jac_i and eval_hess are FORWARDED VERBATIM to the base
// model, which hs_problems.h's detail::make_jac note already establishes emits
// its full structural pattern (explicit zeros included) at every x. p enters
// none of the three. The eval_hess clause added in Phase-5 Task 0 --
// independence of obj_scale and of the lambda VALUES -- likewise transfers
// unchanged, because the forward passes all four arguments straight through.
// tests/test_hs_sweeps.cpp pins the pattern across two x AND two p per problem
// rather than resting on this paragraph.
//
// =====================================================================
// THE SIX PROBLEMS, AND WHY THESE SIX
//
// Chosen from docs/notes/2026-07-29-hs-battery-results.md's per-problem table
// to span the two behaviours the adjudications need -- SOC traffic (which
// needs violation-INCREASING rejected trials) and elastic/restoration traffic
// (which needs INCONSISTENT linearizations):
//
//   HS10  n=2  mi=1   linear objective, so the subproblem Hessian is
//                     lambda_i*hess(cI1) and is indefinite whenever a negative
//                     multiplier is priced. The Phase-3 battery's MOST
//                     EXPENSIVE problem: 8 elastic activations, 48 rho
//                     escalations, 218 minors from a start point whose
//                     violation is 599.
//   HS15  n=2  mi=2   Rosenbrock under a DISCONNECTED feasible set, start
//                     point infeasible. 1 elastic activation cold.
//   HS26  n=3  me=1   the degenerate quartic: hess(f) singular at the
//                     solution, linear convergence tail, and the problem whose
//                     seed K0 is exactly singular (null(H) meets null(Ae) --
//                     Task 11b).
//   HS33  n=3  mi=2   a REVERSE-CONVEX row (feasible side is the outside of a
//                     ball), i.e. a negative-definite Hessian contribution at
//                     a positive multiplier, plus two active lower bounds at
//                     the start point.
//   HS40  n=4  me=3   three product-form equalities in four variables -- the
//                     tightest equality fixture in the battery, where a
//                     linearization is misleading in the exact way the
//                     Maratos effect needs.
//   HS77  n=5  me=2   THE ONLY PROBLEM IN THE WHOLE PHASE-3 BATTERY THAT EVER
//                     REACHED SOC (2 attempts, 0 applied). If any base problem
//                     produces SOC traffic under a sweep, this one must.
//
// THE SWEEP RANGES AND SHIFT DIRECTIONS ARE CHOSEN, NOT DERIVED, and the
// choosing rule is stated so it is reproducible: for each problem the shift
// direction is the one that moves the ACTIVE geometry (the constraint whose
// activity defines the solution) rather than a slack row, and the range is the
// widest one on which every arm of tests/test_hs_sweeps.cpp still converges at
// every grid point. There is no analytic solution path here -- unlike
// parametric_families.h, these problems have none in closed form -- so a
// sweep's correctness evidence is the per-solve KKT self-check
// (support/nlp_kkt_check.h), exactly as the Phase-3 battery's was.

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <tycho_sqp/nlp_model.h>
#include <tycho_sqp/types.h>

#include "hs_problems.h"

namespace tycho::sqp::test_support {

// One Hock-Schittkowski problem re-posed along a scalar parameter -- see this
// header's THE CONSTRUCTION note for the three-line formula and for why it is
// affine.
//
// OWNERSHIP. The base model is held by unique_ptr and is never handed out:
// every NlpModel method below either forwards to it verbatim or forwards and
// adds this class's own p-dependent term. There is no way for a caller to
// reach the base and re-pose it behind this object's back.
class HsSweep : public ParametricNlpModel {
  public:
    // `tilt` has size n(), `ce_shift` size me(), `ci_shift` size mi() -- all
    // three taken from the base problem, so a mis-sized vector is a
    // construction-time error and cannot survive to a solve (project rule T6:
    // the sizes are in the message).
    HsSweep(int hs_number, Vec tilt, Vec ce_shift, Vec ci_shift, double p_init)
        : number_(hs_number), base_(make_hs(hs_number).model), tilt_(std::move(tilt)),
          ce_shift_(std::move(ce_shift)), ci_shift_(std::move(ci_shift)), p_(p_init) {
        if (tilt_.size() != base_->n() || ce_shift_.size() != base_->me() ||
            ci_shift_.size() != base_->mi()) {
            throw std::invalid_argument(fmt::format(
                "HsSweep(HS{}): tilt/ce_shift/ci_shift have sizes {}/{}/{}, expected {}/{}/{}",
                hs_number, tilt_.size(), ce_shift_.size(), ci_shift_.size(), base_->n(),
                base_->me(), base_->mi()));
        }
        if (!std::isfinite(p_)) {
            throw std::invalid_argument(
                fmt::format("HsSweep(HS{}): p_init = {} is not finite", hs_number, p_));
        }
    }

    int hs_number() const { return number_; }

    // ---- NlpModel: sizes, bounds and start point are the base problem's ----
    Index n() const override { return base_->n(); }
    Index me() const override { return base_->me(); }
    Index mi() const override { return base_->mi(); }
    const Vec &lower() const override { return base_->lower(); }
    const Vec &upper() const override { return base_->upper(); }
    Vec start_point() const override { return base_->start_point(); }

    // ---- NlpModel: the three p-dependent VALUES ----
    double eval_f(const Vec &x) const override { return base_->eval_f(x) + p_ * tilt_.dot(x); }
    Vec eval_grad(const Vec &x) const override { return base_->eval_grad(x) + p_ * tilt_; }
    Vec eval_ce(const Vec &x) const override { return base_->eval_ce(x) - p_ * ce_shift_; }
    Vec eval_ci(const Vec &x) const override { return base_->eval_ci(x) - p_ * ci_shift_; }

    // ---- NlpModel: the three STRUCTURES, forwarded verbatim ----
    //
    // These three carry the STRUCTURAL PATTERN INVARIANCE obligation and the
    // forward is what discharges it: the tilt is linear in x and the shifts
    // are constant in x, so neither contributes a Jacobian entry or a second
    // derivative, and p appears in none of the three. All four eval_hess
    // arguments pass through unchanged, which is what carries the Phase-5
    // Task 0 clause (pattern independent of obj_scale and of the lambda
    // VALUES) as well as the original x clause.
    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &lambda_i) const override {
        return base_->eval_hess(x, obj_scale, lambda_e, lambda_i);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return base_->eval_jac_e(x);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return base_->eval_jac_i(x);
    }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    Vec parameters() const override { return Vec::Constant(1, p_); }
    void set_parameters(const Vec &p) override {
        if (p.size() != 1) {
            throw std::invalid_argument(
                fmt::format("HsSweep(HS{})::set_parameters: p has size {}, expected "
                            "parameter_dim() = 1",
                            number_, p.size()));
        }
        if (!std::isfinite(p(0))) {
            throw std::invalid_argument(
                fmt::format("HsSweep(HS{})::set_parameters: p = {} is not finite", number_, p(0)));
        }
        p_ = p(0);
    }

  private:
    int number_;
    std::unique_ptr<NlpModel> base_;
    Vec tilt_;
    Vec ce_shift_;
    Vec ci_shift_;
    double p_;
};

// Everything one corpus member needs to be swept, in one place, so the
// adjudication tests and the note's tables read the SAME grid definition
// rather than two hand-synced copies.
//
// `dp` IS PINNED RATHER THAN ADAPTIVE, and that is a requirement of the KD
// adjudication rather than a convenience. ContinuationOptions' step controller
// grows dp after a step that converged within target_majors -- so a lever that
// changes how many majors a step costs changes WHICH PARAMETER VALUES ARE
// VISITED, and a cross-arm ratio computed over two different grids is not a
// comparison at all (the Phase-4 battery's fix round 1 learned this the
// expensive way with the predictor arm). Every arm here therefore runs with
// dp_init == dp_min == dp_max == dp and target_majors = 0, which makes the
// grid a CONSTANT of the problem: p0, p0+dp, ..., p1. A step that fails cannot
// be retried shorter (dp is already at its floor), so the sweep FAILS instead
// of silently re-gridding -- which is the behaviour that makes the matched
// comparison checkable rather than assumed.
struct HsSweepSpec {
    int number;
    const char *name;
    Vec tilt;
    Vec ce_shift;
    Vec ci_shift;
    double p0;
    double p1;
    double dp;
    // Majors budget per solve. Sized per problem exactly as the Phase-3
    // battery sized its own: large enough that a healthy solve never touches
    // it, small enough that a trapped one cannot eat the runtime budget.
    Index max_iter;
};

namespace hs_sweep_detail {

inline Vec vec1(double a) { return Vec::Constant(1, a); }
inline Vec vec0(Index m) { return Vec::Zero(m); }

} // namespace hs_sweep_detail

// THE CORPUS. Six specs; the per-problem shift direction and range rationale
// is in this header's THE SIX PROBLEMS note, and the observed behaviour each
// one produces is tabulated in
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md.
inline const std::vector<HsSweepSpec> &hs_sweep_specs() {
    using hs_sweep_detail::vec0;
    using hs_sweep_detail::vec1;
    static const std::vector<HsSweepSpec> kSpecs = [] {
        std::vector<HsSweepSpec> s;
        // HS10 -- widen the single ellipse row (ci1 - p <= 0). p > 0 enlarges
        // the feasible set around the published solution, so the ACTIVE row
        // moves and the linear objective's solution slides along it. The
        // published start point (-10, 10) has violation 599 at every p, which
        // is what keeps the elastic tier in play on the cold arm.
        s.push_back({10, "HS10", vec0(2), vec0(0), vec1(1.0), 0.0, 3.0, 0.5, 60});
        // HS15 -- relax the PRODUCT row (ci1 = 1 - x1 x2, shifted to
        // 1 - x1 x2 - p). This is the row whose activity defines the solution
        // AND the row that disconnects the feasible set, so the sweep walks
        // the branch geometry rather than a slack constraint.
        s.push_back(
            {15, "HS15", vec0(2), vec0(0), (Vec(2) << 1.0, 0.0).finished(), 0.0, 1.5, 0.25, 60});
        // HS26 -- move the single equality's level set. The quartic degeneracy
        // that makes hess(f) singular at the solution is untouched, so every
        // point of the sweep keeps the long linear tail.
        s.push_back({26, "HS26", vec0(3), vec1(1.0), vec0(0), 0.0, 2.0, 0.5, 80});
        // HS33 -- move the REVERSE-CONVEX row (ci2, the ball whose outside is
        // feasible). Shrinking/growing that ball is what changes which bounds
        // stay active, i.e. it moves the active set rather than the objective.
        s.push_back(
            {33, "HS33", vec0(3), vec0(0), (Vec(2) << 0.0, 1.0).finished(), 0.0, 2.0, 0.5, 60});
        // HS40 -- move cE1's level (x1^3 + x2^2 - 1 - p = 0), the equality
        // that fixes x1 and hence the whole eliminated chain. A tilt is added
        // on x1 as well: with three equalities in four variables the feasible
        // manifold is a curve, and moving the objective ALONG it is what makes
        // consecutive solves disagree about where on the curve to sit.
        s.push_back({40, "HS40", (Vec(4) << 1.0, 0.0, 0.0, 0.0).finished(),
                     (Vec(3) << 1.0, 0.0, 0.0).finished(), vec0(0), 0.0, 1.0, 0.25, 60});
        // HS77 -- move BOTH equalities. This is the corpus's designated SOC
        // source (the only Phase-3 problem that ever reached a second-order
        // correction), so its shift is the least conservative: both rows move
        // together, which keeps the linearization inconsistent for longer.
        s.push_back(
            {77, "HS77", vec0(5), (Vec(2) << 1.0, 1.0).finished(), vec0(0), 0.0, 2.0, 0.5, 80});
        return s;
    }();
    return kSpecs;
}

inline const HsSweepSpec &hs_sweep_spec(int number) {
    for (const HsSweepSpec &s : hs_sweep_specs()) {
        if (s.number == number) {
            return s;
        }
    }
    throw std::invalid_argument(
        fmt::format("hs_sweep_spec: HS{} is not in the nonconvex sweep corpus", number));
}

// Builds the corpus member for `number`, posed at its own p0. The caller owns
// it; run_continuation re-poses it as the sweep proceeds.
inline std::unique_ptr<HsSweep> make_hs_sweep(int number) {
    const HsSweepSpec &s = hs_sweep_spec(number);
    return std::make_unique<HsSweep>(s.number, s.tilt, s.ce_shift, s.ci_shift, s.p0);
}

} // namespace tycho::sqp::test_support
