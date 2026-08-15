#pragma once

// tests/sqp/support/parametric_families.h — test-support only, NOT part of the
// public library surface. SIX SYNTHETIC PARAMETRIC FAMILIES with fully
// ANALYTIC solution paths: the fixed test bed the rest of Phase 4's
// parametric half is written against (the tangential predictor's O(||dp||^2)
// accuracy claim, the continuation driver's sweep, and Phase 5/6's scaling
// and benchmark work all need a family whose x*(p), f*(p), multipliers and
// ACTIVE SET are known in closed form, not merely converged to).
//
// F6 WAS ADDED IN PHASE-4 TASK 11 and is DIFFERENT IN KIND from the five
// below it -- read its own banner before using it. F1-F5 are ordinary
// finite-dimensional NLPs whose multipliers are numbers; F6 is a QUADRATURE
// DISCRETIZATION of an infinite-dimensional problem, so its multipliers carry
// a mesh's quadrature weight (lambda_i = w_i * nu(t_i)) and are NOT samples of
// a mesh-free function. It is the fixture mesh_transfer.h's costate unscaling
// is measured on, and it is the only family here whose "analytic path" depends
// on the mesh it was constructed with.
//
// F1-F3 ARE TASK 8's; F4-F5 WERE ADDED IN TASK 9's FIX ROUND 1, and the reason
// is worth stating at the top because it is a property of F1-F3 a reader will
// otherwise assume away. IN ALL THREE OF F1, F2 AND F3, cE, cI AND THE BOUNDS
// ARE INDEPENDENT OF p -- only the OBJECTIVE moves (F1's c(p), F2's a(p), F3's
// rest length r(p)). That is fine for what Task 8 built them for, but it means
// three of the four parameter-derivative terms in the tangential predictor's
// right-hand side (d/dp cE, d/dp cI, d/dp bounds -- predictor.h) are
// IDENTICALLY ZERO on all three, and the Task-9 review demonstrated by
// mutation that sign errors in any of them left the whole predictor suite
// green. F4 and F5 exist to close exactly that: their constraints and bounds
// are where p enters, and their OBJECTIVE is p-independent, so the terms are
// isolated rather than merely present. Both were verified independently by
// that review (its §1.3 probes A and B) before being adopted here.
//
// Each family is a ParametricNlpModel (nlp_model.h): an ordinary NlpModel
// carrying a parameter vector p that set_parameters() re-poses it at. Each
// additionally exposes, as member functions, the analytic path
//
//     x_star(p), f_star(p), lambda_e_star(p)/lambda_i_star(p), z_star(p),
//     active_set(p)
//
// and its exact ACTIVATION THRESHOLDS -- the p-values at which the active set
// changes. Every one of those is DERIVED IN THE COMMENT ABOVE ITS FAMILY, not
// asserted: the derivations below verify a candidate primal-dual point
// against this project's own KKT conditions (nlp_model.h's stationarity/sign
// convention, reproduced per family), and each family is convex with a
// nonempty feasible set, which is what upgrades "satisfies KKT" to "is THE
// unique global minimizer".
//
// SIGN CONVENTION -- READ THIS BEFORE COMPARING AGAINST THE PLAN TEXT. This
// project states general inequalities as
//
//     cI(x) <= 0
//
// (nlp_model.h; hs_problems.h converts the Hock-Schittkowski sources' g(x) >= 0
// into it). F1 and F2 are written below in <= form ALREADY, so their
// transcription into cI is the IDENTITY MAP -- x1 + x2 <= 1 becomes
// cI = x1 + x2 - 1, and x1^2 + x2^2 <= 1 becomes cI = x1^2 + x2^2 - 1. NO SIGN
// IS FLIPPED ANYWHERE IN THIS HEADER, and a reader who arrives expecting a
// cI >= 0 convention (the plan's Task-8 prose says so; nlp_model.h, which is
// the authority and which the driver, the KKT self-check and every existing
// model already implement, says cI <= 0) must not apply one. The multiplier
// consequence that goes with it: lambda_i >= 0 with stationarity
//
//     grad f + Je^T lambda_e + Ji^T lambda_i - z = 0,
//
// z >= 0 at an active LOWER bound, z <= 0 at an active UPPER bound, z = 0 on a
// free variable. Simple boxes (F1's 0 <= x <= 0.8, F3's x_i <= u) are BOUNDS,
// i.e. lower()/upper(), never rows of cI.
//
// DEGENERACY IS LABELLED, NOT HIDDEN. F1 turns out to be weakly active (zero
// multiplier on a geometrically active row) along its whole middle branch --
// a property of the family as specified, derived below rather than designed
// in. It is left in place and documented because a warm-start subsystem that
// assumes strict complementarity everywhere is exactly the kind of thing this
// test bed should catch; what the tests assert for it is GEOMETRIC activity
// (which is determinate) rather than working-set membership (which, at a zero
// multiplier, is not). F2 and F3 are strictly complementary on both branches
// away from their thresholds.
//
// CROSS-CHECK. Every path below was cross-checked during development against
// an INDEPENDENT numerical optimizer (scipy.optimize.minimize, SLSQP) at
// 8-12 parameter values per family, agreeing to 1e-8 in x and ~1e-16 in f;
// the tests in tests/test_parametric_families.cpp then re-check the same
// claims against THIS project's driver, which is the check that ships.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::test_support {

// The analytic active set at a parameter value, in exactly warm_start.h's
// encoding so a test can compare it against a solve's WarmStart field for
// field. ACTIVE HERE MEANS GEOMETRICALLY ACTIVE -- the constraint holds with
// equality at x*(p) -- which is well defined even where the multiplier
// vanishes; see F1's DEGENERACY note for the one place in this header where
// the distinction bites.
struct AnalyticActiveSet {
    std::vector<std::uint8_t> ineq_active; // size mi(): 1 iff cI_j(x*) == 0
    std::vector<std::int8_t> bound_active; // size n(): -1 lower, 0 free, +1 upper
};

namespace parametric_detail {

// "Effectively infinite" bound, matching qp_problem.h/hs_problems.h. Named
// distinctly from hs_problems.h's detail::kInf on purpose: both headers live
// in namespace hven::solvers::test_support and a constexpr at namespace scope
// would be a redefinition in any TU that included both.
constexpr double kParamInf = 1e20;

// Builds an n x n SpMatU (upper triangle only) from (row, col, value) triples
// with row <= col. Explicit zeros are preserved (setFromTriplets), which is
// what keeps a family's pattern constant across x and p as nlp_model.h's
// STRUCTURAL PATTERN INVARIANCE precondition requires.
inline SpMatU make_upper(Index n, const std::vector<Eigen::Triplet<double>> &triplets) {
    SpMatU H(n, n);
    H.setFromTriplets(triplets.begin(), triplets.end());
    return H;
}

// Builds an m x n row-major Jacobian from (row, col, value) triples.
inline Eigen::SparseMatrix<double, Eigen::RowMajor>
make_jac(Index m, Index n, const std::vector<Eigen::Triplet<double>> &triplets) {
    Eigen::SparseMatrix<double, Eigen::RowMajor> J(m, n);
    J.setFromTriplets(triplets.begin(), triplets.end());
    return J;
}

// ParametricNlpModel precondition 2 (nlp_model.h): a wrong-sized parameter
// vector is a caller error and throws, with the family named in the message
// so the throw identifies its own source (project rule T6).
inline void check_parameter_size(const Vec &p, Index expected, const char *family) {
    if (p.size() != expected) {
        throw std::invalid_argument(
            fmt::format("{}::set_parameters: p has size {}, expected parameter_dim() = {}", family,
                        p.size(), expected));
    }
}

} // namespace parametric_detail

// =====================================================================
// F1 -- PARAMETRIC BOX QP.
//
//     min_x  1/2 x^T x - c(p)^T x
//     s.t.   x1 + x2 <= 1
//            0 <= x1 <= 0.8,  0 <= x2 <= 0.8
//     c(p) = (p, 1 - p),  p in [0, 1].
//
// TRANSCRIPTION. f(x) = 1/2 ||x||^2 - c(p)^T x (so grad f = x - c(p) and
// hess f = I, constant in x AND p); cI(x) = x1 + x2 - 1 <= 0, mi = 1,
// Ji = [1 1], hess cI = 0; me = 0; the box is lower() = (0,0),
// upper() = (0.8, 0.8). Already in <= form -- see this header's SIGN
// CONVENTION note; nothing is negated.
//
// ---------------------------------------------------------------------
// DERIVATION OF THE PATH AND OF BOTH ACTIVATION THRESHOLDS.
//
// f(x) = 1/2 ||x - c(p)||^2 - 1/2 ||c(p)||^2, so f is strictly convex with
// unconstrained minimizer x = c(p), and the feasible set is a nonempty
// compact convex polytope (it contains 0). KKT is therefore necessary and
// sufficient and the minimizer is unique: exhibiting ONE feasible point with
// multipliers satisfying stationarity, the sign conditions and
// complementarity settles each branch. Stationarity here reads
//
//     x - c(p) + lambda * (1, 1) - z = 0,   lambda >= 0.           (F1-KKT)
//
// STEP 1 -- THE LINEAR ROW IS ALWAYS EXACTLY ON ITS BOUNDARY AT THE
// UNCONSTRAINED MINIMIZER. c1(p) + c2(p) = p + (1 - p) = 1 identically, so
// the unconstrained minimizer x = c(p) satisfies x1 + x2 = 1 -- feasible for
// every p, and never strictly interior to that row. This is why lambda = 0
// on the middle branch below and why F1 is weakly active there (DEGENERACY,
// further down).
//
// STEP 2 -- WHICH BOUNDS THE UNCONSTRAINED MINIMIZER VIOLATES. x1 = p and
// x2 = 1 - p both lie in [0, 1] for p in [0, 1], so no LOWER bound is ever
// violated. The UPPER bound 0.8 is violated by
//     x1 = p        iff  p > 0.8,
//     x2 = 1 - p    iff  1 - p > 0.8  iff  p < 1 - 0.8 = 0.2.
// THOSE ARE THE TWO ACTIVATION THRESHOLDS, and they are the only ones:
//
//     p_low  = 1 - 0.8 = 0.2   (x2 leaves / reaches its upper bound)
//     p_high =     0.8 = 0.8   (x1 reaches / leaves its upper bound)
//
// BRANCH M (0.2 <= p <= 0.8) -- no bound active.
//   Take x* = (p, 1 - p), lambda = 0, z = (0, 0).
//   Feasible: both coordinates lie in [0.2, 0.8] subset [0, 0.8]; the row is
//   satisfied with equality. (F1-KKT): x* - c = 0, lambda = 0, z = 0. Signs:
//   lambda = 0 >= 0; z = 0 on two free variables. Complementarity: cI = 0 and
//   lambda = 0 (weak). Hence
//       f*(p) = 1/2||c||^2 - ||c||^2 = -1/2 (p^2 + (1 - p)^2).
//
// BRANCH L (0 <= p < 0.2) -- x2 at its UPPER bound.
//   Take x* = (p, 0.8), lambda = 0, z = (0, p - 0.2).
//   Feasible: x1 = p in [0, 0.2) subset [0, 0.8]; x2 = 0.8; and
//   x1 + x2 = p + 0.8 < 1, so the row is STRICTLY inactive.
//   (F1-KKT) row 1: p - p + 0 - 0 = 0.
//   (F1-KKT) row 2: 0.8 - (1 - p) + 0 - (p - 0.2) = 0.8 - 1 + p - p + 0.2 = 0.
//   Signs: lambda = 0 >= 0 with cI = p - 0.2 < 0 (strict complementarity on
//   the row); z2 = p - 0.2 < 0 at an ACTIVE UPPER bound, which is the sign
//   this project requires there, and z2 -> 0 exactly as p -> 0.2 -- the
//   threshold is precisely where that bound multiplier changes sign, with
//   slope dz2/dp = 1, i.e. TRANSVERSALLY. z1 = 0 with x1 = p strictly inside
//   (0, 0.8) for p > 0.
//       f*(p) = 1/2(p^2 + 0.64) - p*p - (1 - p)*0.8 = -1/2 p^2 + 0.8 p - 0.48.
//   Continuity at p = 0.2: -0.02 + 0.16 - 0.48 = -0.34, and branch M gives
//   -1/2(0.04 + 0.64) = -0.34. Agree.
//
// BRANCH U (0.8 < p <= 1) -- x1 at its UPPER bound. BY SYMMETRY, not by a
//   second computation: the family is invariant under the simultaneous
//   involution p -> 1 - p, (x1, x2) -> (x2, x1). Indeed c(1 - p) = (1 - p, p)
//   is c(p) swapped, ||x||^2 and the box 0 <= x <= 0.8 are symmetric in the
//   two coordinates, and x1 + x2 <= 1 is symmetric. Applying it to branch L:
//       x*(p) = (0.8, 1 - p),  lambda = 0,  z = (0.8 - p, 0),
//       f*(p) = f*_L(1 - p) = -1/2 (1-p)^2 + 0.8 (1-p) - 0.48
//                           = 0.32 - 1/2 (1 - p)^2 - 0.8 p.
//   Check at p = 0.8: 0.32 - 0.02 - 0.64 = -0.34, matching branch M. z1 =
//   0.8 - p < 0 at an active upper bound, again vanishing exactly at the
//   threshold.
//
// SUMMARY: lambda(p) = 0 for ALL p in [0, 1]; the two real active-set events
// are bound activations at p = 0.2 and p = 0.8.
//
// ---------------------------------------------------------------------
// DEGENERACY -- READ BEFORE USING F1 AS A PREDICTOR/CONTINUATION FIXTURE.
// Because c(p) sums to 1 identically (STEP 1), the row x1 + x2 <= 1 is
// GEOMETRICALLY ACTIVE WITH MULTIPLIER EXACTLY ZERO on all of branch M, and
// strictly inactive on L and U. Strict complementarity therefore FAILS on the
// entire middle branch. Consequences:
//   (i)  the row's WORKING-SET membership at a solve's exit is not determined
//        by the KKT conditions -- a solver may report it active or inactive
//        and be right either way. The tests assert GEOMETRIC activity
//        (|cI(x*)| <= tol), which is determinate, and treat working-set
//        membership as an observation rather than a contract.
//   (ii) the two ENDPOINTS are degenerate too: at p = 0, x* = (0, 0.8) puts
//        x1 exactly on its LOWER bound with z1 = 0; symmetrically at p = 1.
//        active_set() below reports those as active (they are, geometrically),
//        and the tests sample p away from 0, 0.2, 0.8 and 1.
//   (iii) the bound activations themselves ARE strictly complementary on both
//        sides (z crosses zero with slope 1), so F1 remains a clean fixture
//        for an activation EVENT; it is the row, not the box, that is weak.
// =====================================================================
class F1BoxQp : public ParametricNlpModel {
  public:
    // The box's upper bound, and the right-hand side of the linear row. Both
    // appear in the thresholds below, which are derived FROM them rather than
    // hard-coded alongside them.
    static constexpr double kBoxUpper = 0.8;
    static constexpr double kSumLimit = 1.0;

    // The two activation thresholds derived in STEP 2 above:
    //   p_low  = kSumLimit - kBoxUpper = 0.2 -- x2 = 1 - p reaches kBoxUpper,
    //   p_high = kBoxUpper             = 0.8 -- x1 = p     reaches kBoxUpper.
    // kPLow is written as the literal 0.2 rather than as the subtraction
    // because kSumLimit - kBoxUpper evaluates to 0.19999999999999996 in
    // binary64 (0.8 is not representable); the two agree to 1 ulp and the
    // literal is the value the derivation names. ThresholdsMatchTheirDefining
    // Geometry in tests/test_parametric_families.cpp pins that agreement so
    // the two constants cannot drift apart.
    static constexpr double kPLow = 0.2;
    static constexpr double kPHigh = kBoxUpper;

    explicit F1BoxQp(double p0 = 0.5) : p_(Vec::Constant(1, p0)) {}

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F1BoxQp");
        p_ = p;
    }
    Vec parameters() const override { return p_; }

    double p() const { return p_(0); } // scalar convenience

    // ---- NlpModel ----
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        const Vec c = c_of(p_(0));
        return 0.5 * x.squaredNorm() - c.dot(x);
    }
    Vec eval_grad(const Vec &x) const override { return x - c_of(p_(0)); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) + x(1) - kSumLimit); }

    // hess f = I; cI is linear so it contributes nothing. Pattern (two
    // diagonal entries) is constant in x and in p.
    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return parametric_detail::make_upper(2, {{0, 0, obj_scale}, {1, 1, obj_scale}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return parametric_detail::make_jac(1, 2, {{0, 0, 1.0}, {0, 1, 1.0}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kBoxUpper);
        return u;
    }

    // Strictly interior to the box and to the row (0.4 + 0.4 < 1), and equal
    // to x*(p) for no p at all, so a cold solve from here is never a no-op.
    Vec start_point() const override { return Vec::Constant(2, 0.4); }

    // ---- the analytic path (derivation in the banner above) ----
    static Vec c_of(double p) { return (Vec(2) << p, 1.0 - p).finished(); }

    static Vec x_star(double p) {
        if (p < kPLow) {
            return (Vec(2) << p, kBoxUpper).finished(); // branch L
        }
        if (p > kPHigh) {
            return (Vec(2) << kBoxUpper, 1.0 - p).finished(); // branch U
        }
        return c_of(p); // branch M
    }

    static double f_star(double p) {
        if (p < kPLow) {
            return -0.5 * p * p + 0.8 * p - 0.48;
        }
        if (p > kPHigh) {
            const double q = 1.0 - p;
            return -0.5 * q * q + 0.8 * q - 0.48; // branch L's formula at 1 - p
        }
        return -0.5 * (p * p + (1.0 - p) * (1.0 - p));
    }

    // lambda_i == 0 for every p -- see STEP 1 / DEGENERACY.
    static Vec lambda_i_star(double) { return Vec::Zero(1); }
    static Vec lambda_e_star(double) { return Vec(0); }

    static Vec z_star(double p) {
        Vec z = Vec::Zero(2);
        if (p < kPLow) {
            z(1) = p - kPLow; // < 0, active upper bound on x2
        } else if (p > kPHigh) {
            z(0) = kPHigh - p; // < 0, active upper bound on x1
        }
        return z;
    }

    static AnalyticActiveSet active_set(double p) {
        AnalyticActiveSet a;
        a.bound_active.assign(2, 0);
        const Vec x = x_star(p);
        // GEOMETRIC activity, exact-arithmetic: on branch L, x2 == kBoxUpper
        // and x1 == p (which equals the LOWER bound 0 only at p == 0); on
        // branch U, mirrored. Written as comparisons against x_star so the
        // reported set can never drift from the reported point.
        for (Index i = 0; i < 2; ++i) {
            if (x(i) <= 0.0) {
                a.bound_active[static_cast<std::size_t>(i)] = -1;
            } else if (x(i) >= kBoxUpper) {
                a.bound_active[static_cast<std::size_t>(i)] = +1;
            }
        }
        // The row is active exactly on branch M (x1 + x2 == 1 there and
        // < 1 strictly on L and U) -- with a ZERO multiplier; see DEGENERACY.
        const bool row_active = (p >= kPLow && p <= kPHigh);
        a.ineq_active.assign(1, row_active ? 1 : 0);
        return a;
    }

  private:
    Vec p_;
};

// =====================================================================
// F2 -- PARAMETRIC PROJECTION ONTO THE UNIT DISK (NLP, curvature in both the
// objective and the constraint).
//
//     min_x  (x1 - p)^2 + (x2 - p^2)^2
//     s.t.   x1^2 + x2^2 <= 1
//
// TRANSCRIPTION. Write a(p) = (p, p^2). Then f(x) = ||x - a(p)||^2, so
// grad f = 2(x - a(p)) and hess f = 2I (f is a sum of squares of AFFINE
// functions of x -- p enters only the constant terms, so the Hessian is
// constant in x and p). cI(x) = x1^2 + x2^2 - 1 <= 0, mi = 1, Ji = [2x1 2x2],
// hess cI = 2I. me = 0, no finite bounds. Already in <= form; nothing is
// negated (see this header's SIGN CONVENTION note).
//
// ---------------------------------------------------------------------
// DERIVATION.
//
// f is the SQUARED EUCLIDEAN DISTANCE from x to a(p) and the feasible set is
// the closed unit disk D -- a nonempty closed convex set. The minimizer is
// therefore the (unique) metric projection x*(p) = proj_D(a(p)):
//
//     ||a|| <= 1  =>  x* = a,        f* = 0;
//     ||a|| >  1  =>  x* = a/||a||,  f* = (||a|| - 1)^2.
//
// (Both facts are the standard projection-onto-a-ball formula; both are
// re-derived from KKT below, so nothing here rests on citing it.)
//
// THE ACTIVATION THRESHOLD. The unconstrained minimizer a(p) is feasible iff
//
//     ||a(p)||^2 = p^2 + p^4 <= 1.
//
// Substitute t = p^2 >= 0:  t^2 + t - 1 <= 0. The quadratic t^2 + t - 1 has
// roots t = (-1 +/- sqrt(5))/2; the positive root is
//
//     t* = (sqrt(5) - 1)/2 = 0.61803398874989484820...
//
// (the reciprocal golden ratio 1/phi, satisfying t*^2 = 1 - t*), and since the
// parabola opens upward, t^2 + t - 1 <= 0 exactly on [(-1-sqrt5)/2, t*], whose
// intersection with t >= 0 is [0, t*]. Hence p^2 <= t*, i.e.
//
//     p* = sqrt((sqrt(5) - 1)/2) = 0.78615137775742328606...
//
// Sanity: p*^2 = 0.6180339887498949, p*^4 = 0.3819660112501051, and the two
// sum to 1 exactly, which is the defining equation p^2 + p^4 = 1.
//
// BRANCH I (inactive, 0 <= p <= p*):
//   x*(p) = a(p) = (p, p^2), lambda = 0, f*(p) = 0, cI(x*) = p^2 + p^4 - 1 <= 0.
//   Stationarity 2(x* - a) + lambda*2x* = 0 holds trivially. Feasible; signs
//   and complementarity hold (strictly, for p < p*).
//
// BRANCH A (active, p > p*): the constraint holds with equality and
//   stationarity reads
//
//       2(x - a) + lambda * 2x = 0   =>   (1 + lambda) x = a.          (F2-KKT)
//
//   With ||x|| = 1 and 1 + lambda > 0, taking norms gives 1 + lambda = ||a||,
//   so the MULTIPLIER IS DERIVED, not assumed:
//
//       lambda(p) = ||a(p)|| - 1,      x*(p) = a(p)/||a(p)||.
//
//   For p >= 0, ||a|| = sqrt(p^2 + p^4) = p sqrt(1 + p^2), so
//
//       x*(p) = (1, p)/sqrt(1 + p^2)      [check: x1^2 + x2^2 = 1 exactly]
//       lambda(p) = p sqrt(1 + p^2) - 1
//       f*(p) = ||x* - a||^2 = (||a|| - 1)^2 = (p sqrt(1 + p^2) - 1)^2,
//
//   the last line because x* and a are colinear with ||x*|| = 1 <= ||a||.
//   lambda >= 0 iff ||a|| >= 1 iff p >= p*, so the sign condition holds
//   exactly on branch A, lambda(p*) = 0, and the crossing is TRANSVERSAL:
//       d/dp [p sqrt(1+p^2)] = sqrt(1+p^2) + p^2/sqrt(1+p^2) > 0.
//   Strict complementarity therefore holds on BOTH branches except at the
//   single point p = p* itself -- the one parameter value a test must not
//   sample. z = 0 identically (no finite bounds), on both branches.
//
// NEGATIVE p. The model stays well defined for p < 0, and a predictor's
// finite differences may probe there, so x_star/f_star/lambda_i_star below
// implement the general-p forms: ||a|| = |p| sqrt(1 + p^2), the threshold is
// |p| = p*, and on the active branch x*(p) = (sign(p), |p|)/sqrt(1 + p^2)
// (which is a/||a|| with the sign of p carried by the first component only,
// since a2 = p^2 >= 0). The derivations above are stated for p >= 0, the
// range Tasks 9/10 sweep.
// =====================================================================
class F2CircleNlp : public ParametricNlpModel {
  public:
    explicit F2CircleNlp(double p0 = 0.5) : p_(Vec::Constant(1, p0)) {}

    // t* = (sqrt(5) - 1)/2, the positive root of t^2 + t - 1 = 0, and
    // p* = sqrt(t*) = 0.78615137775742328606... -- see the banner. Functions
    // rather than constexpr constants because std::sqrt is not constexpr in
    // standard C++.
    static double t_activation() { return 0.5 * (std::sqrt(5.0) - 1.0); }
    static double p_activation() { return std::sqrt(t_activation()); }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F2CircleNlp");
        p_ = p;
    }
    Vec parameters() const override { return p_; }

    double p() const { return p_(0); }

    // ---- NlpModel ----
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return (x - a_of(p_(0))).squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return 2.0 * (x - a_of(p_(0))); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x.squaredNorm() - 1.0); }

    // hess L = obj_scale * 2I + lambda_i(0) * 2I -- both terms diagonal, so
    // the pattern is the two diagonal entries at every x and every p.
    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        const double d = 2.0 * obj_scale + 2.0 * lambda_i(0);
        return parametric_detail::make_upper(2, {{0, 0, d}, {1, 1, d}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    // [2x1, 2x2]: both entries emitted ALWAYS, including at x = 0 where they
    // are numerically zero -- nlp_model.h's STRUCTURAL PATTERN INVARIANCE.
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return parametric_detail::make_jac(1, 2, {{0, 0, 2.0 * x(0)}, {0, 1, 2.0 * x(1)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -parametric_detail::kParamInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, parametric_detail::kParamInf);
        return u;
    }

    // Strictly feasible (0.5 < 1 in norm) and not on the path for any p.
    Vec start_point() const override { return Vec::Constant(2, 0.5); }

    // ---- the analytic path (derivation in the banner above) ----
    static Vec a_of(double p) { return (Vec(2) << p, p * p).finished(); }

    // ||a(p)|| = |p| sqrt(1 + p^2).
    static double a_norm(double p) { return std::abs(p) * std::sqrt(1.0 + p * p); }

    static bool constraint_active(double p) { return a_norm(p) > 1.0; }

    static Vec x_star(double p) {
        if (!constraint_active(p)) {
            return a_of(p);
        }
        const double s = std::sqrt(1.0 + p * p);
        const double sign = (p >= 0.0) ? 1.0 : -1.0;
        return (Vec(2) << sign / s, std::abs(p) / s).finished();
    }

    static double f_star(double p) {
        if (!constraint_active(p)) {
            return 0.0;
        }
        const double d = a_norm(p) - 1.0;
        return d * d;
    }

    static Vec lambda_i_star(double p) {
        return Vec::Constant(1, constraint_active(p) ? a_norm(p) - 1.0 : 0.0);
    }
    static Vec lambda_e_star(double) { return Vec(0); }
    static Vec z_star(double) { return Vec::Zero(2); } // no finite bounds

    static AnalyticActiveSet active_set(double p) {
        AnalyticActiveSet a;
        a.bound_active.assign(2, 0);
        a.ineq_active.assign(1, constraint_active(p) ? 1 : 0);
        return a;
    }

  private:
    Vec p_;
};

// =====================================================================
// F3 -- PARAMETRIC SPRING CHAIN (banded, scalable to n = 10^4).
//
//     min_x   sum_{i=1}^{n-1} 1/2 (x_{i+1} - x_i - r(p))^2
//     s.t.    x_1 = 0                       (equality pin, me = 1)
//             x_i <= u,  i = 1..n           (UPPER BOUNDS -- mi = 0)
//
// indices 1-based in this comment, 0-based in the code (x(0) is the pin).
//
// TWO MODELLING CHOICES THE PLAN LEAVES OPEN, made here and stated so a
// reviewer can check them against the derivation rather than guess:
//   (1) r(p) = p, the identity map, so the parameter IS the rest length and
//       every threshold below reads directly in p.
//   (2) THE CEILING IS DERIVED FROM A DESIRED THRESHOLD, not fixed: the
//       constructor takes p_act (default 0.5) and sets u = p_act * (n - 1),
//       because the activation threshold derived below is exactly u/(n - 1).
//       Fixing u instead would put the threshold at u/(n-1), i.e. shrink it
//       like 1/n, and the SAME p-sweep could not exercise both branches at
//       n = 50 and at n = 10^4. This is the only place n enters the path in a
//       way a caller must think about.
//
// TRANSCRIPTION. With d_i := x_{i+1} - x_i - r for i = 1..n-1 and the end
// conventions d_0 = d_n = 0:
//     f = 1/2 sum_i d_i^2,     df/dx_j = d_{j-1} - d_j,
//     hess f = L, the PATH-GRAPH LAPLACIAN: L_jj = (number of springs
//         touching j) = 1 for j in {1, n} and 2 otherwise; L_{j,j+1} = -1.
// L is constant in x and in p (r enters only through the d_i in the
// gradient), so eval_hess's pattern -- the tridiagonal band -- is trivially
// x- and p-independent. cE(x) = x_1 is LINEAR, so it adds nothing to the
// Lagrangian Hessian: hess L = obj_scale * L. Je = e_1^T. mi = 0, so there is
// no cI to sign-convert at all; the ceiling is upper(), the pin is cE = 0
// which is already this project's form.
//
// ---------------------------------------------------------------------
// DERIVATION.
//
// L is PSD with null space span(1) (only a uniform translation of the whole
// chain is free), and the pin x_1 = 0 removes exactly that direction, so the
// problem is a STRICTLY convex QP on the pin's null space over a nonempty
// feasible set (x = 0 is feasible whenever u >= 0). KKT is necessary and
// sufficient; the minimizer is unique. Stationarity, in this project's
// convention (z_j <= 0 at an active UPPER bound, z_j = 0 free, lambda_e
// unrestricted):
//
//     (d_{j-1} - d_j) + [j == 1] * lambda - z_j = 0.               (F3-KKT)
//
// BRANCH F (free, (n - 1) r <= u). Take x_i = (i - 1) r. Every d_i = 0, so
//   f = 0, which is the unconstrained minimum of a nonnegative objective;
//   feasibility needs max_i x_i = (n - 1) r <= u, which is the branch
//   condition. (F3-KKT) gives z = 0 (all d vanish) and lambda = 0. Done.
//
// BRANCH C (clamped, (n - 1) r > u). CLAIM: x_i = (i - 1) s with
//   s := u/(n - 1) -- STILL UNIFORM SPACING, at the shallower slope the
//   ceiling permits, with ONLY THE LAST NODE on the bound. Verify:
//     - feasibility: x_i = (i-1)s <= (n-1)s = u, with equality iff i = n;
//     - every d_i = s - r, a NEGATIVE constant (s < r on this branch);
//     - (F3-KKT) for 2 <= j <= n-1: z_j = d_{j-1} - d_j = 0, and those nodes
//       are strictly below the ceiling, which is exactly what a free variable
//       requires;
//     - (F3-KKT) for j = n: z_n = d_{n-1} - d_n = (s - r) - 0 = s - r < 0 --
//       the sign an ACTIVE UPPER bound requires, and STRICTLY, so strict
//       complementarity holds here;
//     - (F3-KKT) for j = 1: x_1 = 0 < u (assuming u > 0) is free, so z_1 = 0
//       and lambda = -(d_0 - d_1) = d_1 = s - r.
//   Hence f*(p) = (n - 1) * 1/2 (s - r)^2 = 1/2 (n - 1) (u/(n-1) - r)^2.
//
// IN ONE LINE: x*_i(p) = (i - 1) * min(p, u/(n - 1)), and the single
// activation threshold is
//
//     p_act(n) = u/(n - 1)      [= the constructor's p_act, by choice (2)].
//
// ---------------------------------------------------------------------
// THE ACTIVE SET IS {n} OR EMPTY -- NEVER A GROWING FRONT. This corrects the
// plan's Task-8 expectation that "the bound-activation front index is an
// analytic function of p and n"; for THIS objective it is not, and the
// natural candidate that would make it so is not merely suboptimal but not
// even KKT-consistent. Take the naive path x_i = min((i-1) r, u) -- follow
// the rest-length ramp, clamp everything above the ceiling -- and let k be
// its first clamped index, so x_{k-1} = (k-2) r < u = x_k. Then
// d_{k-2} = 0 while d_{k-1} = u - (k-1) r < 0, so
//
//     z_{k-1} = d_{k-2} - d_{k-1} = (k-1) r - u > 0
//
// at a node STRICTLY BELOW its bound, which (F3-KKT) requires to carry
// z_{k-1} = 0. It is worse numerically too: at n = 6, u = 1, r = 0.35 it
// gives f = 0.12375 against the optimum 0.05625 (and 0.375 vs 0.225 at
// r = 0.5) -- checked against an independent SLSQP solve, which lands on the
// uniform-spacing path above.
// The mechanism: the chain energy penalizes only DIFFERENCES, so the interior
// of a clamped run against a FLAT ceiling feels no net force; rather than
// bending at a front, the whole chain relaxes to a single shallower slope.
// Both indices are exposed below -- active_front_index(p), which is what the
// KKT conditions give, and rest_front_index(p), the first node the rest-length
// ramp WOULD put above u (smallest i with (i-1) p > u, i.e. floor(u/p) + 2 in
// 1-based terms) -- so that a downstream task can measure the naive quantity
// if it wants it, without any test asserting it as this family's active set.
//
// K0 GEOMETRY (Accelerate standing rule, docs/notes/2026-07-28-accelerate-
// audit-checklist.md). L alone IS singular (the constant null vector), which
// is worth stating because fixtures are required not to present exactly-
// singular K0 geometry. The assembled K0 here is not singular: the pin's row
// closes the null direction, and the engine's own primal_delta/dual_mu
// regularization is applied on top of it, so
// K0 = [[L + delta I, e_1], [e_1^T, -mu]] is nonsingular for any
// delta, mu > 0. No counter pinned in tests/test_parametric_families.cpp
// depends on a backend-specific factorization path.
// =====================================================================
class F3SpringChain : public ParametricNlpModel {
  public:
    // n: chain length (>= 2). p_act: the DESIRED activation threshold, which
    // fixes the ceiling u = p_act * (n - 1) -- see modelling choice (2).
    // p0: the initial rest length; the default sits on the free branch.
    explicit F3SpringChain(Index n = 50, double p_act = 0.5, double p0 = 0.25)
        : n_(checked_n(n)), u_(checked_p_act(p_act) * static_cast<double>(n_ - 1)),
          p_(Vec::Constant(1, p0)) {}

    double u() const { return u_; }
    // p_act(n) = u/(n-1), the single threshold derived in the banner.
    double p_activation() const { return u_ / static_cast<double>(n_ - 1); }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F3SpringChain");
        p_ = p;
    }
    Vec parameters() const override { return p_; }

    double p() const { return p_(0); }

    // ---- NlpModel ----
    Index n() const override { return n_; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double r = p_(0);
        double acc = 0.0;
        for (Index i = 0; i + 1 < n_; ++i) {
            const double d = x(i + 1) - x(i) - r;
            acc += 0.5 * d * d;
        }
        return acc;
    }

    // df/dx_j = d_{j-1} - d_j: spring i contributes -d_i to node i and +d_i
    // to node i+1 (0-based).
    Vec eval_grad(const Vec &x) const override {
        const double r = p_(0);
        Vec g = Vec::Zero(n_);
        for (Index i = 0; i + 1 < n_; ++i) {
            const double d = x(i + 1) - x(i) - r;
            g(i) -= d;
            g(i + 1) += d;
        }
        return g;
    }

    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0)); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    // obj_scale * L, upper triangle: diagonal 1 at the two ends and 2 in
    // between, -1 on the first superdiagonal. cE is linear and contributes
    // nothing.
    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(2 * n_));
        for (Index j = 0; j < n_; ++j) {
            const double diag = (j == 0 || j == n_ - 1) ? 1.0 : 2.0;
            t.emplace_back(static_cast<int>(j), static_cast<int>(j), obj_scale * diag);
            if (j + 1 < n_) {
                t.emplace_back(static_cast<int>(j), static_cast<int>(j + 1), -obj_scale);
            }
        }
        return parametric_detail::make_upper(n_, t);
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return parametric_detail::make_jac(1, n_, {{0, 0, 1.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, n_);
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }

    // The flat chain: feasible (it satisfies the pin and sits below the
    // ceiling) and far from x*(p) on both branches.
    Vec start_point() const override { return Vec::Zero(n_); }

    // ---- the analytic path (derivation in the banner above) ----

    // The realized spacing s = min(r, u/(n-1)).
    double spacing(double p) const { return std::min(p, p_activation()); }
    bool bound_active(double p) const { return p > p_activation(); }

    Vec x_star(double p) const {
        const double s = spacing(p);
        Vec x(n_);
        for (Index i = 0; i < n_; ++i) {
            x(i) = static_cast<double>(i) * s;
        }
        return x;
    }

    double f_star(double p) const {
        const double d = spacing(p) - p;
        return 0.5 * static_cast<double>(n_ - 1) * d * d;
    }

    // lambda = d_1 = s - r (zero on the free branch, negative on the clamped
    // one -- an equality multiplier carries no sign condition).
    Vec lambda_e_star(double p) const { return Vec::Constant(1, spacing(p) - p); }
    Vec lambda_i_star(double) const { return Vec(0); }

    Vec z_star(double p) const {
        Vec z = Vec::Zero(n_);
        if (bound_active(p)) {
            z(n_ - 1) = spacing(p) - p; // < 0 at an active upper bound
        }
        return z;
    }

    AnalyticActiveSet active_set(double p) const {
        AnalyticActiveSet a;
        a.ineq_active.clear(); // mi == 0
        a.bound_active.assign(static_cast<std::size_t>(n_), 0);
        if (bound_active(p)) {
            a.bound_active.back() = +1;
        }
        return a;
    }

    // 0-BASED index of the first node ON the ceiling at the optimum, or n()
    // (the usual one-past-the-end sentinel) when no node is. By the
    // derivation this is n()-1 or nothing -- never an interior front.
    Index active_front_index(double p) const { return bound_active(p) ? n_ - 1 : n_; }

    // 0-BASED index of the first node the REST-LENGTH RAMP x_i = i*p would put
    // strictly above the ceiling (smallest i with i*p > u), or n() when the
    // ramp clears the whole chain. This is the "front" a naive reading of the
    // family measures; it is NOT the active set -- see the banner. Provided so
    // a downstream task can compute it without re-deriving it, and so the
    // distinction stays visible in code rather than only in prose.
    Index rest_front_index(double p) const {
        if (!(p > 0.0)) {
            return n_; // a zero/negative rest length never reaches the ceiling
        }
        const double ratio = u_ / p;
        if (ratio >= static_cast<double>(n_)) {
            return n_; // guards the cast below against an out-of-range ratio
        }
        return std::min(n_, static_cast<Index>(std::floor(ratio)) + 1);
    }

  private:
    // Both checks run in the MEMBER-INITIALIZER LIST, before n_/u_ and the
    // two bound vectors sized from them exist: Vec::Constant(n, ...) with a
    // negative n trips an Eigen assertion (Debug) or allocates nonsense
    // (Release) long before a constructor body could report anything, so a
    // body check would be a diagnostic that never fires (project rule T6:
    // the throw must be the thing that reports).
    static Index checked_n(Index n) {
        if (n < 2) {
            throw std::invalid_argument(
                fmt::format("F3SpringChain: n ({}) must be >= 2 (a chain needs a spring)", n));
        }
        return n;
    }
    static double checked_p_act(double p_act) {
        if (!(p_act > 0.0)) { // catches NaN and negatives
            throw std::invalid_argument(fmt::format(
                "F3SpringChain: p_act ({}) must be > 0; it is the activation threshold the "
                "ceiling u = p_act*(n-1) is derived from",
                p_act));
        }
        return p_act;
    }

    Index n_;
    double u_;
    Vec p_;
    // Built once at construction: lower() / upper() return references, and n
    // is a per-instance size, so these cannot be the function-local statics
    // the two-variable families above use.
    Vec lower_ = Vec::Constant(n_, -parametric_detail::kParamInf);
    Vec upper_ = Vec::Constant(n_, u_);
};

// =====================================================================
// F4 -- MOVING CONSTRAINTS (TWO PARAMETERS). The family in which p enters
// EVERY constraint kind at once and the objective NOT AT ALL.
//
//     min_x  1/2 ||x||^2
//     s.t.   x1 - a(p)  = 0            (equality)
//           -x2 + a(p) <= 0            (inequality, i.e. x2 >= a(p))
//            a(p) <= x3                (a LOWER BOUND that moves with p)
//     a(p) = p1 + p2,   p in R^2
//
// (1-based in this comment, 0-based in the code.)
//
// WHY IT EXISTS -- see this header's banner. grad f = x and hess f = I are
// p-INDEPENDENT, so the predictor's d/dp grad_x L term vanishes identically
// and the ONLY things driving the step are d/dp cE, d/dp cI and d/dp lower --
// one per constraint kind, isolated. A sign error in any of the three is a
// wrong x here and cannot hide behind the objective term. Adopted from the
// Task-9 review's probe A, generalized from one parameter to TWO so the
// predictor's DIRECTIONAL finite difference (p + h*dp/||dp||, scaled back by
// ||dp||/h) is exercised with a genuine direction rather than with +/-1.
//
// TRANSCRIPTION. f = 1/2||x||^2 (grad f = x, hess f = I, both constant in x
// and p); cE(x) = x1 - a(p), Je = [1 0 0]; cI(x) = -x2 + a(p) <= 0 -- ALREADY
// in this project's <= form, nothing negated -- Ji = [0 -1 0]; both
// constraints are affine in x so neither contributes to hess L, which is
// therefore obj_scale * I at every x and p. lower() = (-inf, -inf, a(p)),
// upper() = +inf. All three patterns (I, Je, Ji) are constant in x AND p, as
// nlp_model.h's STRUCTURAL PATTERN INVARIANCE requires of a
// ParametricNlpModel.
//
// ---------------------------------------------------------------------
// DERIVATION.
//
// f is strictly convex and the feasible set is a nonempty closed convex
// polyhedron (x = (a, max(a,0), max(a,0)) is in it for every p), so KKT is
// necessary AND sufficient and the minimizer is unique: exhibiting one
// feasible point with multipliers settles each branch. Stationarity, in this
// project's convention (grad f + Je^T le + Ji^T li - z = 0, li >= 0, z >= 0 at
// an active LOWER bound):
//
//     row 1:  x1 + le      = 0
//     row 2:  x2 - li      = 0                                  (F4-KKT)
//     row 3:  x3 - z3      = 0
//
// x1 = a is forced by the equality, so le = -a on BOTH branches.
//
// BRANCH A (a(p) > 0) -- both the row and the bound active.
//   x* = (a, a, a), li = a, z3 = a, le = -a.
//   Feasible: x1 = a; -x2 + a = 0; x3 = a is ON its lower bound.
//   (F4-KKT): row 2 gives li = x2 = a, row 3 gives z3 = x3 = a.
//   Signs: li = a > 0 (STRICTLY -- strict complementarity holds on this whole
//   branch, unlike F1's row), z3 = a > 0 at an active LOWER bound, which is
//   the sign this project requires there.
//       f*(p) = 1/2 (a^2 + a^2 + a^2) = 3/2 a^2.
//
// BRANCH B (a(p) <= 0) -- neither active.
//   x* = (a, 0, 0), li = 0, z3 = 0, le = -a.
//   Feasible: -0 + a = a <= 0 satisfies the row; 0 >= a satisfies the bound.
//   (F4-KKT): row 2 gives li = x2 = 0, row 3 gives z3 = x3 = 0.
//   Complementarity: both constraints are STRICTLY inactive for a < 0.
//       f*(p) = 1/2 a^2.
//
// Continuity at a = 0: both give x* = 0 and f* = 0. THE SINGLE THRESHOLD IS
// a(p) = 0, i.e. the LINE p1 + p2 = 0, and it is degenerate in the same way
// every threshold is (li and z3 both vanish there); tests sample a > 0.
//
// x*(p) IS AFFINE IN p ON EACH BRANCH, which is the point: a first-order
// predictor has ZERO linearization error on it, so what a prediction's
// residual measures is the KKT regularization alone (predictor.h's THE THREE
// ERROR TERMS note; the review measured err = 1.000*delta*||dp|| here too).
//
// K0 GEOMETRY (Accelerate standing rule): H = I is positive definite and the
// two constraint rows are linearly independent, so the assembled K0 is
// nonsingular before any regularization is added.
// =====================================================================
class F4MovingConstraints : public ParametricNlpModel {
  public:
    explicit F4MovingConstraints(double p0 = 0.15, double p1 = 0.15)
        : p_((Vec(2) << p0, p1).finished()), lower_(make_lower(p0 + p1)) {}

    // a(p) = p1 + p2 -- the one scalar the whole family moves with.
    static double a_of(const Vec &p) { return p(0) + p(1); }
    double a() const { return a_of(p_); }

    // The threshold is a(p) == 0; ACTIVE means a > 0 (see the derivation).
    static bool constraints_active(const Vec &p) { return a_of(p) > 0.0; }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 2; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 2, "F4MovingConstraints");
        p_ = p;
        lower_(2) = a_of(p_);
    }
    Vec parameters() const override { return p_; }

    // ---- NlpModel ----
    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; } // p-INDEPENDENT
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0) - a()); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, -x(1) + a()); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return parametric_detail::make_upper(
            3, {{0, 0, obj_scale}, {1, 1, obj_scale}, {2, 2, obj_scale}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        // [1 0 0], with the two structural zeros emitted so the pattern is
        // constant (nlp_model.h's STRUCTURAL PATTERN INVARIANCE).
        return parametric_detail::make_jac(1, 3, {{0, 0, 1.0}, {0, 1, 0.0}, {0, 2, 0.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return parametric_detail::make_jac(1, 3, {{0, 0, 0.0}, {0, 1, -1.0}, {0, 2, 0.0}});
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }

    // Not on the path for any p in the sampled range, and above the moving
    // lower bound there.
    Vec start_point() const override { return Vec::Constant(3, 1.0); }

    // ---- the analytic path (derivation in the banner above) ----
    static Vec x_star(const Vec &p) {
        const double a = a_of(p);
        const double t = std::max(a, 0.0);
        return (Vec(3) << a, t, t).finished();
    }
    static double f_star(const Vec &p) {
        const double a = a_of(p);
        return constraints_active(p) ? 1.5 * a * a : 0.5 * a * a;
    }
    static Vec lambda_e_star(const Vec &p) { return Vec::Constant(1, -a_of(p)); }
    static Vec lambda_i_star(const Vec &p) {
        return Vec::Constant(1, constraints_active(p) ? a_of(p) : 0.0);
    }
    static Vec z_star(const Vec &p) {
        Vec z = Vec::Zero(3);
        if (constraints_active(p)) {
            z(2) = a_of(p); // > 0 at an active LOWER bound
        }
        return z;
    }
    static AnalyticActiveSet active_set(const Vec &p) {
        AnalyticActiveSet s;
        const bool on = constraints_active(p);
        s.ineq_active.assign(1, on ? 1 : 0);
        s.bound_active.assign(3, 0);
        if (on) {
            s.bound_active[2] = -1; // lower
        }
        return s;
    }

  private:
    static Vec make_lower(double a) {
        return (Vec(3) << -parametric_detail::kParamInf, -parametric_detail::kParamInf, a)
            .finished();
    }

    Vec p_;
    Vec lower_;
    Vec upper_ = Vec::Constant(3, parametric_detail::kParamInf);
};

// =====================================================================
// F5 -- MOVING THRESHOLD. The family whose ACTIVATION itself is driven by p
// moving the CONSTRAINT, not by p moving the objective's target.
//
//     min_x  1/2 ||x||^2
//     s.t.   -x1 + p <= 0              (inequality, i.e. x1 >= p)
//            p <= x2                   (a LOWER BOUND that moves with p)
//
// (1-based in this comment, 0-based in the code.)
//
// WHY IT EXISTS. F1's bound activation is driven by the OBJECTIVE's target
// c(p) walking into a FIXED box, and F2's by the objective's target leaving a
// FIXED disk. Neither exercises what happens when the CONSTRAINT ITSELF moves
// past a stationary point -- which is the case in which the predictor's ADD
// and FIX repairs must be triggered by d/dp cI and d/dp lower rather than by
// the step in x. Stepping this family from p < 0 to p > 0 fires BOTH in one
// call: the row must be ADDed and the bound must be FIXed, purely because the
// constraints moved. Adopted from the Task-9 review's probe B, with the
// moving BOUND added alongside the moving ROW so one prediction covers both.
//
// TRANSCRIPTION. f = 1/2||x||^2 (grad f = x, hess f = I -- p-INDEPENDENT, the
// same isolation F4 has); cI(x) = -x1 + p <= 0, already in <= form, Ji =
// [-1 0]; me = 0; lower() = (-inf, p), upper() = +inf. Patterns constant in x
// and p.
//
// ---------------------------------------------------------------------
// DERIVATION. Strictly convex objective, nonempty polyhedral feasible set
// (x = (max(p,0), max(p,0)) is always in it), so KKT is necessary and
// sufficient and the minimizer is unique. Stationarity:
//
//     row 1:  x1 - li = 0,      row 2:  x2 - z2 = 0.            (F5-KKT)
//
// The two coordinates are completely decoupled -- separate objective terms,
// one constraint each -- so each is the scalar problem "minimize 1/2 t^2
// subject to t >= p", whose solution is t = max(p, 0):
//
//   p > 0:  t = p, and the constraint is active with multiplier t = p > 0
//           (STRICTLY complementary; li = p for the row, z2 = p >= 0 for the
//           lower bound, both the signs this project requires).
//   p <= 0: t = 0, strictly interior for p < 0, multiplier 0.
//
// Hence x*(p) = (max(p,0), max(p,0)), li = z2 = max(p,0), f*(p) = max(p,0)^2,
// and THE SINGLE THRESHOLD IS p = 0 for BOTH constraints at once. Each branch
// is affine in p, so a first-order predictor is again exact to the
// regularization floor away from p = 0 -- and a prediction that STEPS ACROSS
// p = 0 lands on the far side exactly, since both branches are affine and the
// far-side branch is what the ADD/FIX repairs put it on.
//
// K0 GEOMETRY: H = I, one constraint row; nonsingular before regularization.
// =====================================================================
class F5MovingThreshold : public ParametricNlpModel {
  public:
    explicit F5MovingThreshold(double p0 = -0.1)
        : p_(Vec::Constant(1, p0)), lower_(make_lower(p0)) {}

    double p() const { return p_(0); }
    static bool constraints_active(double p) { return p > 0.0; }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F5MovingThreshold");
        p_ = p;
        lower_(1) = p_(0);
    }
    Vec parameters() const override { return p_; }

    // ---- NlpModel ----
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; } // p-INDEPENDENT
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, -x(0) + p_(0)); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return parametric_detail::make_upper(2, {{0, 0, obj_scale}, {1, 1, obj_scale}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return parametric_detail::make_jac(1, 2, {{0, 0, -1.0}, {0, 1, 0.0}});
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }

    Vec start_point() const override { return Vec::Constant(2, 1.0); }

    // ---- the analytic path ----
    static Vec x_star(double p) { return Vec::Constant(2, std::max(p, 0.0)); }
    static double f_star(double p) {
        const double t = std::max(p, 0.0);
        return t * t;
    }
    static Vec lambda_e_star(double) { return Vec(0); }
    static Vec lambda_i_star(double p) { return Vec::Constant(1, std::max(p, 0.0)); }
    static Vec z_star(double p) {
        Vec z = Vec::Zero(2);
        z(1) = std::max(p, 0.0); // >= 0 at an active LOWER bound
        return z;
    }
    static AnalyticActiveSet active_set(double p) {
        AnalyticActiveSet s;
        const bool on = constraints_active(p);
        s.ineq_active.assign(1, on ? 1 : 0);
        s.bound_active.assign(2, 0);
        if (on) {
            s.bound_active[1] = -1;
        }
        return s;
    }

  private:
    static Vec make_lower(double p) {
        return (Vec(2) << -parametric_detail::kParamInf, p).finished();
    }

    Vec p_;
    Vec lower_;
    Vec upper_ = Vec::Constant(2, parametric_detail::kParamInf);
};

// =====================================================================
// F6 -- DISCRETIZED INTEGRAL PENALTY WITH A PATH BOUND. The family whose
// MULTIPLIERS CARRY A QUADRATURE WEIGHT, added in Phase-4 Task 11 as the
// fixture mesh_transfer.h's costate unscaling is measured on.
//
//     min_x   sum_{i=0}^{N-1} w_i * ( cosh(x_i - a(t_i)) - 1 )
//     s.t.    x_i - p <= 0,   i = 0..N-1                  (mi = N)
//     a(t) = sin(pi t),   t_i the mesh NODES, w_i the mesh QUADRATURE WEIGHTS,
//     p in R the PATH BOUND LEVEL (this family's parameter).
//
// (0-based throughout, in the comment and in the code.)
//
// WHY IT EXISTS, AND WHY IT IS THE ONLY FAMILY IN THIS HEADER WHOSE
// MULTIPLIERS ARE NOT MESH-FREE. F1-F5 are finite-dimensional problems whose
// multipliers are numbers; this one is a QUADRATURE DISCRETIZATION of an
// infinite-dimensional problem, and that changes what a multiplier IS. The
// continuous problem behind it is
//
//     min_y  INTEGRAL_0^1 g(y(t), t) dt   s.t.  y(t) <= p  for all t,
//     g(y, t) = cosh(y - a(t)) - 1,
//
// whose stationarity condition (in this project's sign convention, with nu(t)
// the path multiplier DENSITY -- a multiplier PER UNIT t, since the constraint
// is imposed at every t of a continuum) reads
//
//     dg/dy (y(t), t) + nu(t) = 0,   nu(t) >= 0,   nu(t) * (y(t) - p) = 0.
//
// Replacing the integral by a quadrature rule sum_i w_i g(x_i, t_i) and the
// continuum of constraints by one row per node gives the DISCRETE stationarity
// condition (nlp_model.h's convention, Ji = I here):
//
//     w_i * dg/dy (x_i, t_i) + lambda_i = 0,
//
// so, comparing the two,
//
//     lambda_i = w_i * nu(t_i).                                     (F6-COSTATE)
//
// THAT IS THE WHOLE POINT OF THIS FAMILY. The discrete multiplier is NOT a
// sample of the continuous one: it is a sample TIMES THE LOCAL QUADRATURE
// WEIGHT, i.e. an INTEGRATED quantity attached to a node. Two meshes
// discretizing the SAME continuous problem therefore carry multipliers that
// differ by the ratio of their weights -- an O(1) discrepancy that does NOT
// vanish as either mesh refines -- which is exactly why mesh_transfer.h
// unscales by the source weights before interpolating and rescales by the
// destination's (that header's own derivation, of which (F6-COSTATE) is the
// concrete instance this fixture makes measurable).
//
// ---------------------------------------------------------------------
// TRANSCRIPTION. f(x) = sum_i w_i (cosh(x_i - a_i) - 1) with a_i := a(t_i),
// so grad f_i = w_i sinh(x_i - a_i) and hess f = diag(w_i cosh(x_i - a_i)) --
// DIAGONAL at every x and every p, so the pattern (the N diagonal entries) is
// constant as nlp_model.h's STRUCTURAL PATTERN INVARIANCE requires. cI(x) =
// x - p*1 is AFFINE in x, already in this header's <= form (nothing negated),
// with Ji = I -- again a constant pattern -- and contributing nothing to the
// Lagrangian Hessian, so hess L = obj_scale * diag(w_i cosh(x_i - a_i)).
// me = 0; no finite bounds (the path bound is a ROW, not a box -- see WHY A
// ROW below).
//
// WHY A ROW AND NOT A BOX. Both encodings are legitimate and the costate
// scaling is identical for either (with a box, stationarity reads
// w_i dg/dy - z_i = 0, so z_i = w_i * nu(t_i) with the opposite sign
// convention and the same weight factor). The row is chosen because it puts
// the whole story in ONE vector, lambda_i, which is the vector Task 11's
// transfer tests compare -- a box would split it across z and leave lambda_i
// empty. mesh_transfer.h transfers lambda_e, lambda_i AND z by the same rule
// for exactly this reason: which vector a transcription parks its path
// multipliers in is a transcription choice, not a mathematical one.
//
// ---------------------------------------------------------------------
// DERIVATION OF THE PATH.
//
// The problem is SEPARABLE: coordinate i appears in exactly one objective term
// and one constraint row. So it is N independent scalar problems
//
//     min_{x_i}  w_i (cosh(x_i - a_i) - 1)   s.t.  x_i <= p,
//
// each with w_i > 0 (a mesh precondition, checked in the constructor) and
// cosh strictly convex with its unique minimizer at x_i = a_i. A scalar
// strictly convex function restricted to a half-line attains its minimum at
// the unconstrained minimizer when that point is feasible and AT THE BOUND
// otherwise, so
//
//     x*_i = min(a_i, p),
//
// and the whole problem is convex with a nonempty feasible set (x = p*1 is
// feasible), so this KKT point is THE unique global minimizer. The
// multipliers, from the discrete stationarity condition above with
// dg/dy = sinh(x_i - a_i):
//
//     a_i <= p (inactive): x*_i = a_i, sinh(0) = 0, so lambda_i = 0, and
//         cI_i = a_i - p <= 0 -- strictly complementary wherever a_i < p.
//     a_i >  p (active):   x*_i = p, so
//             lambda_i = -w_i sinh(p - a_i) = w_i sinh(a_i - p) > 0,
//         STRICTLY positive, and cI_i = 0. Strictly complementary.
//
// In the (F6-COSTATE) form, therefore, THE ANALYTIC MULTIPLIER DENSITY IS
//
//     nu(t) = sinh( max(0, a(t) - p) ),                              (F6-NU)
//
// a function of t ALONE -- no mesh in it anywhere, which is the property the
// transfer's interpolation step relies on. z*(p) = 0 identically (no finite
// bounds), and lambda_e is empty (me = 0).
//
//     f*(p) = sum_i w_i ( cosh(max(0, a_i - p)) - 1 ).
//
// THE ACTIVATION THRESHOLD is p = max_t a(t) = 1 (attained at t = 1/2): for
// p >= 1 no row is active anywhere and nu == 0; for p < 1 the ACTIVE SET IS A
// SUB-INTERVAL of [0, 1],
//
//     { t : sin(pi t) > p } = ( T(p), 1 - T(p) ),   T(p) = arcsin(p)/pi,
//
// (using sin(pi t)'s symmetry about t = 1/2 on [0, 1]), whose two endpoints
// are the ACTIVITY JUNCTIONS mesh_transfer.h's inheritance rule is written
// against. At the default p = 0.6, T = 0.20483276469913345... and the active
// sub-interval is (0.2048..., 0.7951...).
//
// nu IS SMOOTH INSIDE THE ACTIVE SUB-INTERVAL AND HAS A KINK AT EACH JUNCTION
// -- it is max(0, .) of a smooth function -- and that is a PROPERTY OF
// ACTIVATED PATH CONSTRAINTS, not an artifact of this choice of a(t). It is
// stated here because it sets the interpolation accuracy a mesh transfer can
// possibly achieve: linear interpolation of nu is second-order accurate away
// from the junctions and only FIRST-order in the one cell containing each
// junction (the standard h/4 * (slope jump) bound for a linear interpolant
// straddling a kink), so a transfer's error, measured over the whole vector,
// is dominated by the two junction cells. tests/test_mesh_transfer.cpp
// measures and pins both numbers separately for exactly this reason.
//
// ---------------------------------------------------------------------
// WHY cosh AND NOT A QUADRATIC. A quadratic g would make the NLP a QP whose
// first subproblem IS the answer, and a cold solve would converge in one
// major -- leaving no majors for a warm start to save, so Task 11's
// cold-vs-warm comparison would have nothing to measure. cosh keeps the
// separable, analytically-solvable structure (every step of the derivation
// above uses only strict convexity and separability) while making the solve a
// genuine Newton iteration: from the cold start x = 0 the scalar Newton map
// is x <- x - tanh(x - a_i), which needs several steps to reach 1e-6 from
// |x - a_i| ~ 1. It is also the reason the objective is written
// cosh(.) - 1 rather than cosh(.): the -1 makes f*(p) -> 0 as the problem
// becomes unconstrained, so f is a penalty in the ordinary sense.
//
// K0 GEOMETRY (Accelerate standing rule, docs/notes/2026-07-28-accelerate-
// audit-checklist.md): hess L is DIAGONAL with entries obj_scale * w_i *
// cosh(x_i - a_i) >= obj_scale * w_i > 0 for obj_scale > 0, i.e. positive
// definite, and Ji = I has full row rank, so the assembled K0 is nonsingular
// before any regularization is added. No counter pinned against this fixture
// depends on a backend-specific factorization path.
// =====================================================================

// Uniformly spaced nodes on [t0, t1]: count >= 2 points with t_0 = t0 and
// t_{count-1} = t1 exactly (Eigen's LinSpaced pins both endpoints). Written
// here rather than in mesh_transfer.h because building a mesh is a fixture
// concern -- the library's Mesh is a plain (nodes, weights) pair with no
// opinion about where either came from.
inline Vec uniform_nodes(Index count, double t0 = 0.0, double t1 = 1.0) {
    if (count < 2) {
        throw std::invalid_argument(fmt::format(
            "uniform_nodes: count ({}) must be >= 2 (a mesh needs an interval)", count));
    }
    return Vec::LinSpaced(count, t0, t1);
}

// COMPOSITE TRAPEZOID weights for `nodes`: w_0 = (t_1 - t_0)/2,
// w_i = (t_{i+1} - t_{i-1})/2 for interior i, w_{N-1} = (t_{N-1} - t_{N-2})/2.
// They sum to t_{N-1} - t_0 and are strictly positive whenever the nodes are
// strictly increasing, which is precisely mesh_transfer.h's Mesh
// precondition. NOTE THAT THE END WEIGHTS ARE HALF THE INTERIOR ONES even on
// a uniform mesh: a transfer that copied multipliers raw would therefore be
// wrong by a factor of 2 at the ends of a uniform-to-uniform map, before any
// refinement ratio is considered at all.
inline Vec trapezoid_weights(const Vec &nodes) {
    const Index count = nodes.size();
    if (count < 2) {
        throw std::invalid_argument(fmt::format(
            "trapezoid_weights: nodes has size {}, needs >= 2 (a mesh needs an interval)", count));
    }
    Vec w(count);
    w(0) = 0.5 * (nodes(1) - nodes(0));
    for (Index i = 1; i + 1 < count; ++i) {
        w(i) = 0.5 * (nodes(i + 1) - nodes(i - 1));
    }
    w(count - 1) = 0.5 * (nodes(count - 1) - nodes(count - 2));
    return w;
}

class F6PathBoundQuadrature : public ParametricNlpModel {
  public:
    // nodes/weights are the quadrature mesh (any strictly increasing nodes and
    // strictly positive weights -- the SAME preconditions mesh_transfer.h's
    // Mesh carries, checked here so a fixture cannot be built outside them).
    // p0 is the initial path bound level; the default sits below
    // max_t a(t) = 1, so the constraint is active on a sub-interval.
    explicit F6PathBoundQuadrature(Vec nodes, Vec weights, double p0 = 0.6)
        : nodes_(checked_nodes(std::move(nodes))),
          weights_(checked_weights(std::move(weights), nodes_.size())), p_(Vec::Constant(1, p0)) {}

    // a(t) = sin(pi t) -- the target the penalty tracks. Static because the
    // transfer tests re-derive (F6-NU) from it without an instance.
    static double a_of(double t) { return std::sin(kPi * t); }

    // (F6-NU): the analytic multiplier DENSITY, the mesh-free function whose
    // samples the discrete multipliers carry a weight factor on top of.
    static double nu_of(double t, double p) { return std::sinh(std::max(0.0, a_of(t) - p)); }

    // The activation threshold: for p >= 1 = max_t a(t) nothing is ever
    // active. Below it the active set is the open sub-interval
    // (T(p), 1 - T(p)) with T(p) = arcsin(p)/pi -- see the banner.
    static constexpr double kPActivation = 1.0;
    static double junction_left(double p) { return std::asin(p) / kPi; }
    static double junction_right(double p) { return 1.0 - junction_left(p); }

    const Vec &nodes() const { return nodes_; }
    const Vec &weights() const { return weights_; }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F6PathBoundQuadrature");
        p_ = p;
    }
    Vec parameters() const override { return p_; }

    double p() const { return p_(0); }

    // ---- NlpModel ----
    Index n() const override { return nodes_.size(); }
    Index me() const override { return 0; }
    Index mi() const override { return nodes_.size(); }

    double eval_f(const Vec &x) const override {
        double acc = 0.0;
        for (Index i = 0; i < n(); ++i) {
            acc += weights_(i) * (std::cosh(x(i) - a_of(nodes_(i))) - 1.0);
        }
        return acc;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g(n());
        for (Index i = 0; i < n(); ++i) {
            g(i) = weights_(i) * std::sinh(x(i) - a_of(nodes_(i)));
        }
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return (x.array() - p_(0)).matrix(); }

    // diag(obj_scale * w_i * cosh(x_i - a_i)); cI is affine and contributes
    // nothing. Every diagonal entry is emitted at every x, so the pattern is
    // constant (STRUCTURAL PATTERN INVARIANCE).
    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(n()));
        for (Index i = 0; i < n(); ++i) {
            t.emplace_back(static_cast<int>(i), static_cast<int>(i),
                           obj_scale * weights_(i) * std::cosh(x(i) - a_of(nodes_(i))));
        }
        return parametric_detail::make_upper(n(), t);
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, n());
    }
    // Ji = I, every diagonal entry emitted at every x.
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(n()));
        for (Index i = 0; i < n(); ++i) {
            t.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0);
        }
        return parametric_detail::make_jac(n(), n(), t);
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }

    // The flat profile x = 0: feasible for every p >= 0, and far enough from
    // x*(p) (whose peak is min(1, p)) that a cold solve costs a real Newton
    // iteration rather than one step -- see WHY cosh in the banner.
    Vec start_point() const override { return Vec::Zero(n()); }

    // ---- the analytic path (derivation in the banner above) ----
    Vec x_star(double p) const {
        Vec x(n());
        for (Index i = 0; i < n(); ++i) {
            x(i) = std::min(a_of(nodes_(i)), p);
        }
        return x;
    }

    double f_star(double p) const {
        double acc = 0.0;
        for (Index i = 0; i < n(); ++i) {
            acc += weights_(i) * (std::cosh(std::max(0.0, a_of(nodes_(i)) - p)) - 1.0);
        }
        return acc;
    }

    // (F6-COSTATE): lambda_i = w_i * nu(t_i). THIS IS THE WEIGHT-SCALED
    // VECTOR the mesh transfer exists to map correctly.
    Vec lambda_i_star(double p) const {
        Vec lam(n());
        for (Index i = 0; i < n(); ++i) {
            lam(i) = weights_(i) * nu_of(nodes_(i), p);
        }
        return lam;
    }
    Vec lambda_e_star(double) const { return Vec(0); }
    Vec z_star(double) const { return Vec::Zero(n()); }

    AnalyticActiveSet active_set(double p) const {
        AnalyticActiveSet s;
        s.bound_active.assign(static_cast<std::size_t>(n()), 0); // no finite bounds
        s.ineq_active.assign(static_cast<std::size_t>(n()), 0);
        for (Index i = 0; i < n(); ++i) {
            // Strictly, matching the derivation: a_i == p is the degenerate
            // boundary case (zero multiplier) and is reported INACTIVE, which
            // is what "geometrically active with a vanishing price" resolves
            // to under this header's DEGENERACY IS LABELLED policy. No mesh
            // used in the tests places a node exactly there.
            if (a_of(nodes_(i)) > p) {
                s.ineq_active[static_cast<std::size_t>(i)] = 1;
            }
        }
        return s;
    }

  private:
    // std::numbers::pi would need <numbers>; this header already pulls in
    // <cmath> and the literal is exact to the last bit of binary64.
    static constexpr double kPi = 3.14159265358979323846;

    static Vec checked_nodes(Vec nodes) {
        if (nodes.size() < 2) {
            throw std::invalid_argument(
                fmt::format("F6PathBoundQuadrature: nodes has size {}, needs >= 2 (a quadrature "
                            "mesh needs an interval)",
                            nodes.size()));
        }
        for (Index i = 0; i + 1 < nodes.size(); ++i) {
            if (!(nodes(i + 1) > nodes(i))) {
                throw std::invalid_argument(
                    fmt::format("F6PathBoundQuadrature: nodes must be strictly increasing, but "
                                "nodes[{}] = {} is not less than nodes[{}] = {}",
                                i, nodes(i), i + 1, nodes(i + 1)));
            }
        }
        return nodes;
    }
    static Vec checked_weights(Vec weights, Index expected) {
        if (weights.size() != expected) {
            throw std::invalid_argument(
                fmt::format("F6PathBoundQuadrature: weights has size {}, expected {} (one per "
                            "node)",
                            weights.size(), expected));
        }
        for (Index i = 0; i < weights.size(); ++i) {
            if (!(weights(i) > 0.0)) { // also catches NaN
                throw std::invalid_argument(
                    fmt::format("F6PathBoundQuadrature: weights[{}] = {} must be > 0 (a "
                                "quadrature weight scales this node's objective term and its "
                                "multiplier)",
                                i, weights(i)));
            }
        }
        return weights;
    }

    Vec nodes_;
    Vec weights_;
    Vec p_;
    // Sized from nodes_, so these cannot be the function-local statics the
    // two-variable families above use (F3's own reasoning, verbatim).
    Vec lower_ = Vec::Constant(nodes_.size(), -parametric_detail::kParamInf);
    Vec upper_ = Vec::Constant(nodes_.size(), parametric_detail::kParamInf);
};

} // namespace hven::solvers::test_support
