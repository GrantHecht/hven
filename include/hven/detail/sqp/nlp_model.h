#pragma once

// nlp_model.h — the driver-facing NLP the Phase-3 SQP driver consumes:
//
//     min   f(x)
//     s.t.  cE(x)  = 0
//           cI(x) <= 0
//           l <= x <= u
//
// NlpModel is a pure interface. Every Hock-Schittkowski problem in
// tests/support/hs_problems.h implements it; sqp_driver.h (Task 4) will
// consume it the way qp_engine.h consumes QpProblem -- linearizing cE/cI at
// the current iterate into a QpProblem each major iteration.
//
// MULTIPLIER SIGN CONVENTION -- IDENTICAL to qp_problem.h's stationarity
// condition, with cE/cI/Je/Ji standing in for that header's be/bi/Ae/Ai:
//
//     grad(f)(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,  lambda_i >= 0
//
// where Je = eval_jac_e(x), Ji = eval_jac_i(x), grad(f) = eval_grad(x). z is
// the bound multiplier: >= 0 when a variable sits at its active lower bound,
// <= 0 at an active upper bound, 0 when free -- again exactly qp_problem.h's
// convention. This identity is not a coincidence to be re-derived per task:
// the Phase-3 driver's subproblem (Task 4) linearizes with Ae = Je, Ai = Ji,
// be = -cE(x), bi = -cI(x), so a QpSolution's lambda_e/lambda_i/z already
// ARE the NLP's multipliers at that iterate, with no sign flip anywhere in
// the driver.
//
// EXACT LAGRANGIAN HESSIAN. eval_hess(x, obj_scale, lambda_e, lambda_i)
// returns the upper triangle only (SpMatU -- same storage convention as
// qp_problem.h::H, enforced the same way by that header's validate()) of
//
//     obj_scale * hess(f)(x)
//       + sum_i lambda_e(i) * hess(cE_i)(x) + sum_j lambda_i(j) * hess(cI_j)(x)
//
// i.e. the Hessian, with respect to x, of
// L(x, lambda) = obj_scale*f(x) + lambda_e^T cE(x) + lambda_i^T cI(x) -- the
// SAME Lagrangian sign convention as the stationarity condition above
// (+lambda_e^T cE, +lambda_i^T cI). obj_scale exists for the driver's later
// elastic-mode/merit-function scaling (Tasks 8-9); every Task-3 caller passes
// obj_scale = 1.0. tests/support/derivative_check.h's assert_hessian is the
// transcription guard for this composite: it finite-differences the
// GRADIENT OF THE LAGRANGIAN (eval_grad + Je^T lambda_e + Ji^T lambda_i), not
// just hess(f) alone, so a model that gets a constraint Hessian wrong (or
// omits it) fails the check even when eval_hess's objective-only term is
// correct.
//
// STRUCTURAL PATTERN INVARIANCE -- A BINDING PRECONDITION ON EVERY
// IMPLEMENTER, ADDED IN PHASE 4 (Task 8) because the warm-start machinery
// already assumed it while this header never said it. eval_hess, eval_jac_e
// and eval_jac_i must each return the SAME SPARSITY PATTERN at every x: the
// set of (row, col) entries a model emits is a property of the MODEL, not of
// the point it is evaluated at. **AND, FOR eval_hess, NOT OF THE ARGUMENTS IT
// IS CALLED WITH** (clause added in Phase-5 Task 0 on review: the rule was
// stated for x alone while two callers already relied on the wider form): the
// pattern must be independent of `obj_scale` and of the `lambda_e`/`lambda_i`
// VALUES as well. Concretely, an implementation may NOT skip constraint j's
// Hessian block because `lambda_i(j) == 0.0`, nor drop the objective block at
// `obj_scale == 0.0` -- both must be emitted as structural zeros. Two
// separate places depend on exactly this: sqp_driver.h's warm-start INGEST
// probe and its zero-major EMISSION probe both build at ZERO multipliers and
// compare the resulting hash against one taken from a subproblem built with
// the solve's REAL multipliers, and the restoration phase's own model calls
// eval_hess with `obj_scale = 0`. An entry whose value happens to be 0.0 at
// some particular x must still be emitted, as a structural zero -- Eigen's
// setFromTriplets preserves explicit zeros, and hs_problems.h's
// detail::make_jac note documents the same rule for the HS transcriptions
// (HS40's d(cE2)/dx1 = 2*x1*x4 at x1 = 0 is the worked example).
//
// WHY IT IS BINDING RATHER THAN ADVISORY. qp_engine.h's
// detail::structural_hash is an FNV-1a fingerprint over H/Ae/Ai's index
// arrays only, and warm_start.h's WarmStart::structure_hash carries it
// between solves as the warm-vs-hot discriminator: a cached K0 factorization
// is reused whenever a later subproblem's structural_hash matches the one
// the factorization was built from. A model whose pattern shifts with x
// makes that hash a function of the ITERATE, so two subproblems of the same
// model at different points hash differently (a silently missed hot start --
// merely slow), and, worse, a pattern that COLLAPSES at a special x (an
// entry dropped because it evaluated to zero) and later reappears can hash
// back to a previously-seen value with a different symbolic factorization
// behind it. Neither failure is detectable from inside the engine, which is
// why it is stated here as a requirement on the model instead.

#include <Eigen/SparseCore>

#include <tycho_sqp/types.h>

namespace tycho::sqp {

// The driver-facing NLP. IMPLEMENTER PRECONDITION: eval_hess/eval_jac_e/
// eval_jac_i return an x-INDEPENDENT sparsity pattern, and eval_hess's is
// additionally independent of `obj_scale` and of the lambda_e/lambda_i
// VALUES it is called with -- see this header's STRUCTURAL PATTERN
// INVARIANCE note for the rule and for the warm-start hash (warm_start.h's
// WarmStart::structure_hash, qp_engine.h's detail::structural_hash) that
// depends on it.
class NlpModel {
  public:
    virtual ~NlpModel() = default;

    virtual Index n() const = 0;  // number of variables
    virtual Index me() const = 0; // number of equality constraints
    virtual Index mi() const = 0; // number of inequality constraints

    virtual double eval_f(const Vec &x) const = 0;
    virtual Vec eval_grad(const Vec &x) const = 0; // size n
    virtual Vec eval_ce(const Vec &x) const = 0;   // size me()
    virtual Vec eval_ci(const Vec &x) const = 0;   // size mi()

    // Exact Lagrangian Hessian, upper triangle only. See header comment.
    virtual SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                             const Vec &lambda_i) const = 0;

    virtual Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const = 0;
    virtual Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const = 0;

    virtual const Vec &lower() const = 0; // size n (+/-inf allowed, e.g. +/-1e20)
    virtual const Vec &upper() const = 0; // size n

    virtual Vec start_point() const = 0; // size n

    // VALUES ONLY -- f(x), cE(x), cI(x) -- no gradient, no Jacobians (Phase-5
    // Task 8, the eval-economics carry: docs/notes/2026-07-29-phase-3-close-
    // carries.md and its Phase-4 follow-on). Several driver call sites
    // (sqp_driver.h's rejected-trial funnel evaluation, the warm-resolution
    // probe) judge or hash a point using only f/cE/cI and never touch
    // eval_grad/eval_jac_e/eval_jac_i/eval_hess for it; paying for the
    // derivatives there was pure waste every time the answer turned out not
    // to need them (a rejected trial, a probe whose result the caller only
    // hashes).
    //
    // THE DEFAULT IMPLEMENTATION IS THE OPT-IN CONTRACT: it calls this same
    // model's own eval_f/eval_ce/eval_ci (respecting the same me()==0/
    // mi()==0 skip eval_nlp already applies) and nothing else, so it returns
    // EXACTLY the values eval_nlp's f/ce/ci fields would have held, at
    // EXACTLY the cost those three calls already were -- every existing
    // model therefore keeps working, unmodified, with no behavior change
    // whatsoever from adding this method. A model override exists ONLY to
    // go faster than that baseline -- e.g. by sharing sub-expressions across
    // f/cE/cI that eval_f/eval_ce/eval_ci would otherwise recompute
    // independently, or by a cheaper closed form entirely -- never to return
    // a DIFFERENT number: whatever this returns must be bit-identical to
    // calling eval_f(x)/eval_ce(x)/eval_ci(x) directly, the same invariant
    // eval_nlp already rests its f/ce/ci fields on. See
    // tests/support/scale_problems.h's F7CollocationChain override for the
    // one model in this tree that exercises the override rather than the
    // default.
    virtual void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const {
        f = eval_f(x);
        cE = me() > 0 ? eval_ce(x) : Vec(0);
        cI = mi() > 0 ? eval_ci(x) : Vec(0);
    }
};

// A model carrying a PARAMETER VECTOR p alongside its variables x: the same
// NLP, re-posed at a new p by set_parameters(). This is the surface the
// Phase-4 warm-start subsystem's parametric half consumes -- the tangential
// predictor (predictor.h) differences the model across p to build its
// sensitivity right-hand side, and the continuation driver (continuation.h)
// sweeps p from p0 to p1 re-solving at each step -- and the surface
// tests/support/parametric_families.h's three synthetic families implement.
//
// The parameters are MODEL STATE, not an argument threaded through every
// eval_*: every NlpModel method above reports the model AT THE CURRENT p, so
// a solve sees one fixed p from start to finish and needs no parametric
// awareness at all. set_parameters is therefore non-const and is the ONLY
// way p changes; a caller that must not disturb an in-flight solve holds the
// model by const reference, which makes that statically true.
//
// PRECONDITIONS ON EVERY IMPLEMENTER, both binding:
//
//   1. STRUCTURAL PATTERN INVARIANCE IN p AS WELL AS IN x. NlpModel already
//      requires eval_hess/eval_jac_e/eval_jac_i to emit an x-independent
//      pattern (see this header's note of that name); for a
//      ParametricNlpModel the pattern must ALSO be independent of p.
//      set_parameters may change the VALUES those three methods report and
//      must never change their structure -- nor n()/me()/mi(), which are
//      pattern by another name. This is what lets a warm start taken at p
//      be fed into a solve at p + dp at all: warm_start.h's
//      WarmStart::structure_hash (qp_engine.h's detail::structural_hash)
//      keys the cached K0 factorization on pattern alone, so a p-dependent
//      pattern would either silently forfeit every hot start along a
//      continuation sweep or, if a pattern that collapsed at one p
//      reappeared at another, match a hash whose symbolic factorization no
//      longer describes the matrix. The whole point of a continuation sweep
//      is that consecutive solves ARE structurally identical; a model that
//      breaks this is not a parametric family, it is a family of families.
//
//   2. set_parameters(p) with p.size() != parameter_dim() is a caller error
//      and must throw std::invalid_argument (project rule T6: never a
//      silent resize, never a diagnostic that is not also the thrown
//      message).
//
// parameters() returns the current p, so a caller can save/restore it around
// a probe (the predictor's one-extra-evaluation finite difference does
// exactly that) without tracking it separately.
class ParametricNlpModel : public NlpModel {
  public:
    virtual Index parameter_dim() const = 0;
    virtual void set_parameters(const Vec &p) = 0; // size parameter_dim()
    virtual Vec parameters() const = 0;            // size parameter_dim()
};

} // namespace tycho::sqp
