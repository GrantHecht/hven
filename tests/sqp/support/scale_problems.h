#pragma once

// tests/support/scale_problems.h — test-support only, NOT part of the public
// library surface. THE PHASE-5 SCALE GENERATOR: one parametric family, F7,
// shaped like the workload tycho actually runs (a direct-collocation optimal
// control transcription) and carrying a MANUFACTURED SOLUTION that is exact in
// closed form at every problem size. Every scale study, policy ruling and the
// PSIOPT head-to-head in Phase 5 is measured against this family, so its
// analytic surface -- x*(p), f*(p), the multipliers, the active set and the
// activity junctions -- is DERIVED here in full rather than asserted, in the
// same style as tests/support/parametric_families.h's F1-F6. This header
// INCLUDES that one, for its AnalyticActiveSet encoding and its
// parametric_detail helpers: there is no second copy of either.
//
// WHY A SEVENTH FAMILY. F3 (spring chain) is banded and scales, but it is a
// pure QP with one equality row, no inequality rows and a diagonal Hessian; F6
// is a quadrature discretization but SEPARABLE, with an identity Jacobian and
// no dynamics. Neither has what makes a collocation NLP expensive: a
// BLOCK-BANDED equality Jacobian coupling consecutive nodes, one inequality
// row per node, a box on part of each node's variables, and a Lagrangian
// Hessian with state-control coupling inside every node block. F7 has all
// four, at any size, with the optimum known exactly.
//
// SIGN CONVENTIONS -- nlp_model.h's, unchanged and never flipped here:
//
//     min f(x)  s.t.  cE(x) = 0,  cI(x) <= 0,  l <= x <= u,
//     grad f + Je^T lambda_e + Ji^T lambda_i - z = 0,   lambda_i >= 0,
//     z >= 0 at an active LOWER bound, z <= 0 at an active UPPER bound,
//     z == 0 on a free variable,
//     hess L = obj_scale*hess f + sum_i lambda_e(i) hess(cE_i)
//                               + sum_j lambda_i(j) hess(cI_j),  upper triangle.
//
// =====================================================================
// F7 -- TRAPEZOIDAL COLLOCATION CHAIN WITH A STATE PATH CONSTRAINT.
//
// THE TRANSCRIPTION. N nodes on t in [0, 1], t_k = k*h with h = 1/(N-1).
// Each node carries ns STATES y_k in R^ns and nc CONTROLS u_k in R^nc, laid
// out NODE-MAJOR:
//
//     x = ( y_0, u_0, y_1, u_1, ..., y_{N-1}, u_{N-1} ),   n = N*(ns + nc),
//     y_{k,i} = x[k*(ns+nc) + i],  u_{k,j} = x[k*(ns+nc) + ns + j].
//
// Node-major is what makes every matrix below BANDED: a defect row touches
// only nodes k and k+1, so its columns lie in a window of 2*(ns+nc)
// consecutive indices, and the Hessian is block DIAGONAL by node.
//
// THE DYNAMICS ARE LINEAR:  y' = F(y, u) = A y + C u, with
//
//     A = -theta*I + sigma*(first subdiagonal)   (a damped cascade:
//         A_{ii} = -theta, A_{i,i-1} = sigma, everything else 0)
//     C_{ij} = 1 iff j == i mod nc, else 0       (each control drives the
//         states congruent to it mod nc -- ns x nc, exactly one 1 per row)
//
// and the EQUALITY ROWS are the ns initial-condition rows followed by the
// ns*(N-1) TRAPEZOIDAL DEFECT rows,
//
//     cE^ic    = y_0 - m(0, p) * e                                      (ns rows)
//     cE^def_k = y_{k+1} - y_k - (h/2)[F(y_k,u_k) + F(y_{k+1},u_{k+1})] - d_k(p)
//                                                    k = 0..N-2, ns rows each
//
// so me = ns*N. e is a fixed unit vector (below), m(0,p) is the manufactured
// state profile at t = 0 -- THE PARAMETRIC BOUNDARY CONDITION -- and d_k(p) in
// R^ns is the MANUFACTURED FORCING derived below. Both cE blocks are AFFINE in
// x: hess cE is identically zero and Je is a CONSTANT matrix.
//
// THE PATH INEQUALITY, one row per node (mi = N), is a ball on the state:
//
//     cI_k(x) = (1/2)( ||y_k||^2 - R^2 ) <= 0,     k = 0..N-1,
//
// already in this project's <= form -- nothing is negated. It is CONVEX (its
// Hessian is the identity on node k's state block, PSD), which is what keeps
// the whole problem convex, and it is where the Lagrangian Hessian's
// dependence on lambda_i lives.
//
// THE BOX is on the CONTROLS ONLY: -U <= u_{k,j} <= U with U = kControlBound;
// the states are unbounded (+/- kParamInf). That is the standard OCP shape --
// bounded actuators, free states, path limits as rows.
//
// THE OBJECTIVE is a quadrature-weighted quadratic with STATE-CONTROL COUPLING
// plus a linear term whose coefficients are problem DATA:
//
//     f(x) = sum_k h*[ (1/2)||y_k||^2 + (rho/2)||u_k||^2 + gamma*y_k.(C u_k) ]
//            - sum_k [ q_k.y_k + r_k.u_k ]
//          = (1/2) x^T W x - g^T x,
//
// with W block-diagonal by node, W_k = h*[[I, gamma*C], [gamma*C^T, rho*I]],
// and g the assembled (q_k, r_k). The quadratic part is FIXED; (q_k, r_k) are
// the MANUFACTURED SOURCE TERMS derived below, the objective-side analogue of
// d_k. gamma is kCouplingScale/sqrt(m_max) with m_max = ceil(ns/nc) -- see
// CONDITIONING for why it depends on the shape.
//
// ---------------------------------------------------------------------
// THE MANUFACTURED SOLUTION. Everything below is a CHOICE (the manufactured
// profile) followed by a DERIVATION (the data that makes the choice optimal).
//
// CHOICE 1 -- THE STATE PROFILE, CLAMPED BY THE PATH CONSTRAINT.
//
//     phi(t)    = 1 + sin(pi t)          in [1, 2] on [0,1], peak at t = 1/2
//     psi(t, p) = p * phi(t)             the UNCONSTRAINED profile magnitude
//     m(t, p)   = min( psi(t, p), R )    the CLAMPED magnitude
//     y*(t)     = m(t, p) * e,           e a fixed unit vector with e_i > 0
//
// e_i proportional to 1 + i/ns, normalized to ||e|| = 1. Then
// ||y*(t)|| = m(t, p) (because ||e|| = 1 and m >= 0 for p > 0), so
//
//     cI at the profile = (1/2)( m(t,p)^2 - R^2 ) <= 0 always, and = 0
//     exactly where psi(t,p) >= R.
//
// THE ACTIVE SUB-INTERVAL AND ITS JUNCTIONS. psi(t,p) > R reads
// p(1 + sin(pi t)) > R, i.e. sin(pi t) > R/p - 1 =: xi(p). On [0,1],
// sin(pi t) is symmetric about t = 1/2 and increases on [0, 1/2], so for
// xi in (0,1) the solution set is the OPEN SUB-INTERVAL
//
//     ( T(p), 1 - T(p) ),        T(p) = arcsin( xi(p) ) / pi,
//                                xi(p) = R/p - 1.                    (F7-JCT)
//
// xi is decreasing in p, so the window WIDENS with p -- the parameter controls
// the active set, which is the property Phase 5's warm-start and activation
// studies need. The two ends of the sub-interval regime are exactly where xi
// leaves (0,1):
//
//     xi(p) = 1  <=>  p = R/2  -- at or below it psi <= R everywhere and NO
//                                 row is active (empty window);
//     xi(p) = 0  <=>  p = R    -- at or above it psi >= R everywhere and EVERY
//                                 row is active, INCLUDING node 0. That case is
//                                 EXCLUDED; see THE ONE DEGENERACY below.
//
// THE FAMILY'S DESIGN RANGE IS THEREFORE 0 < p < R, with the proper
// sub-interval geometry on (R/2, R). (F7-JCT) describes the active set only on
// (R/2, R): below R/2 the set is empty (junction_left/junction_right clamp xi
// to [0,1], so both collapse to t = 1/2 there), at or above R it is the whole
// horizon and the problem is degenerate.
//
// THE RANGE IS ENFORCED, ON THE ANALYTIC-PATH ACCESSORS ONLY (review round 1,
// Q-1): x_star, f_star, lambda_e_star, lambda_i_star, z_star, active_set and
// the three junction functions all throw std::invalid_argument outside
// 0 < p < R, naming p and the range (project rule T6). set_parameters and the
// eval_* surface are deliberately NOT guarded, because the MODEL is total in p
// -- it is the CLAIM "x_star(p) is the optimum" that is not -- and because the
// pattern-invariance test legitimately drives the model to p = 1.4 to check
// that the sparsity patterns hold still there.
//
// THE ONE DEGENERACY, NAMED RATHER THAN HIDDEN. At p >= R the path row of
// NODE 0 is active, and node 0's state is FIXED OUTRIGHT by the ns
// initial-condition rows; the active path row's gradient (y_0^T = R e^T) is
// then a linear combination of those rows' gradients, so LICQ FAILS and the
// assembled K0 is exactly singular before regularization. That is precisely
// the geometry the Accelerate standing rule forbids a fixture to present, so
// p >= R is outside the family's range: p_saturation() returns R and names it,
// and every test samples p < R. Inside the range node 0 is STRICTLY inactive
// (psi(0,p) = p < R), which is what the LICQ proof in CONDITIONING (b) needs.
//
// CHOICE 2 -- THE CONTROL PROFILE. Free (see WHY THE FORCING IS FREE below):
//
//     u*_j(t) = kControlAmp * cos( pi t + 2 pi j / nc ),   j = 0..nc-1,
//
// so |u*_j| <= kControlAmp = 0.5 < U = 1 STRICTLY at every node -- the box is
// inactive at the optimum by construction, hence z*(p) = 0 (see (V3)). The box
// is a real box the QP engine must carry (2*N*nc finite bounds, and iterates
// do reach it from the cold start); it is simply not part of the OPTIMAL
// active set, which keeps this family's activity story entirely in the path
// rows, where the parameter controls it.
//
// DERIVATION 1 -- THE FORCING d_k THAT MAKES x* SATISFY THE DEFECTS EXACTLY.
// Define, for k = 0..N-2,
//
//     d_k(p) := y*(t_{k+1}) - y*(t_k)
//               - (h/2)[ F(y*(t_k),u*(t_k)) + F(y*(t_{k+1}),u*(t_{k+1})) ]. (F7-D)
//
// Then cE^def_k(x*) = 0 IDENTICALLY, for every N, every p and every choice of
// the two profiles -- this is the manufactured-solutions method (add the source
// term that the chosen solution's residual leaves behind), and d_k is ordinary
// problem DATA, closed-form in t_k, t_{k+1} and p. And cE^ic(x*) = 0 because
// t_0 = 0 and the boundary datum IS m(0,p) e. So x* IS FEASIBLE for every
// equality row, EXACTLY, at every size -- not to truncation error.
//
// WHY THE FORCING IS FREE -- and why (F7-D) is not a cheat. A collocation NLP
// is defined by (dynamics, forcing, cost, constraints); manufacturing a
// solution means choosing the FORCING and the COST SOURCES to fit a chosen
// profile, exactly as a manufactured-solutions convergence study for a PDE
// chooses a source term to fit a chosen field. The DISCRETIZATION is untouched
// -- the defect rows are the real trapezoidal rule, the Jacobian is the real
// block-banded one, and nothing about the problem's structure, conditioning or
// difficulty depends on d_k being nonzero. What it buys is that the profile
// does not have to satisfy the DISCRETE dynamics, which would force either a
// linear-in-t profile (no clamping, no junctions) or an O(h^2) mismatch
// between the continuous and the discrete optimum -- and hence an f* correct
// only to truncation error rather than exactly.
//
// DERIVATION 2 -- THE MULTIPLIERS. All three are CHOSEN in closed form and then
// VERIFIED against the KKT conditions; the objective's linear data absorbs
// whatever they are (Derivation 3).
//
//     lambda_i*_k := h * nu(t_k, p),  nu(t,p) := max( 0, psi(t,p) - R )  (F7-LI)
//     lambda_e*^ic  := h * s,          s fixed, s_i = 1 - i/(2 ns)
//     lambda_e*^(k) := h * mu(t_k) * s,  mu(t) := cos(pi t),  k = 0..N-2 (F7-LE)
//     z*            := 0.
//
// (F7-LI) is the F6 pattern -- a multiplier DENSITY nu(t,p) sampled at the
// nodes and scaled by the local quadrature weight, here the uniform h. It is
// nonnegative everywhere and STRICTLY positive exactly on the open window
// (F7-JCT), which is what makes complementarity and STRICT complementarity
// hold together. The h in (F7-LE) is the same scaling argument: the cost is a
// quadrature so grad f is O(h), while the defect rows have O(1) entries, so a
// costate balancing them is O(h). s is deliberately NOT parallel to e, so the
// manufactured solution does not live in a one-dimensional invariant subspace
// in which a state-index transposition would be invisible.
//
// DERIVATION 3 -- THE OBJECTIVE'S SOURCE TERMS (q_k, r_k), FROM STATIONARITY.
// grad f(x) = W x - g, so at x*, with z* = 0, stationarity
//
//     grad f(x*) + Je^T lambda_e* + Ji(x*)^T lambda_i* = 0
//
// is one linear equation for g, solved BY INSPECTION -- no matrix is inverted,
// because g enters grad f additively:
//
//     g = W x* + Je^T lambda_e* + Ji(x*)^T lambda_i*,
//
// i.e. block by block, with (C u)_i = u_{i mod nc} and (C^T w)_j = sum over
// { i : i mod nc == j } of w_i,
//
//     q_k = h[ y*_k + gamma C u*_k ] + (Je^T le)_{y_k} + lambda_i*_k y*_k
//     r_k = h[ rho u*_k + gamma C^T y*_k ] + (Je^T le)_{u_k}             (F7-G)
//
// The two adjoint blocks are read off Je (write L^ic, L^(k) for the costates
// and take L^(-1) = L^(N-1) = 0, so the end cases need no special text):
//
//     (Je^T le)_{y_k} = [k==0] L^ic + (I - (h/2)A^T) L^(k-1)
//                                   - (I + (h/2)A^T) L^(k)
//     (Je^T le)_{u_k} = -(h/2) C^T ( L^(k-1) + L^(k) )                 (F7-ADJ)
//
// because cE^def_k has d/dy_k = -I - (h/2)A, d/dy_{k+1} = I - (h/2)A and
// d/du_k = d/du_{k+1} = -(h/2)C, while cE^ic has d/dy_0 = I. (A^T w)_i =
// -theta w_i + sigma w_{i+1}. Ji's row k is y_k^T on node k's state block and
// zero elsewhere, so (Ji^T li)_{y_k} = lambda_i*_k y*_k and (Ji^T li)_{u_k} = 0
// -- which is why lambda_i appears in q_k only.
//
// ---------------------------------------------------------------------
// THE KKT VERIFICATION -- the point of the whole construction. x* is THE unique
// global minimizer, and (x*, lambda_e*, lambda_i*, z*) is its KKT quadruple:
//
// (V1) CONVEXITY. f is a strictly convex quadratic: W is block diagonal with
//      blocks h*[[I, gamma C],[gamma C^T, rho I]] whose eigenvalues are
//      h*(1 +/- gamma sigma_i(C)) on the coupled pairs and h*rho elsewhere
//      (rho = 1); since gamma*||C||_2 = kCouplingScale = 1/2 by the choice of
//      gamma (CONDITIONING (a)), every eigenvalue lies in h*[1/2, 3/2] > 0.
//      cE is affine and cI is convex, so the feasible set is closed and convex;
//      it is nonempty because x* is in it ((V2)). A strictly convex objective
//      over a nonempty closed convex set has a UNIQUE minimizer, and for a
//      convex program the KKT conditions are SUFFICIENT -- so exhibiting the
//      quadruple settles it, with no local-vs-global caveat anywhere.
//
// (V2) PRIMAL FEASIBILITY. Equalities: exactly zero, by (F7-D) and t_0 = 0.
//      Path rows: (1/2)(m(t_k,p)^2 - R^2) <= 0 since m <= R by definition.
//      Box: |u*_{k,j}| <= kControlAmp < kControlBound, strictly.
//
// (V3) DUAL FEASIBILITY. lambda_i*_k = h*max(0, psi - R) >= 0. z* = 0 is the
//      value this project requires on a FREE variable, and every variable is
//      free at x* (states unbounded; controls strictly inside the box by (V2)).
//      lambda_e* is unrestricted in sign, as an equality multiplier is.
//
// (V4) COMPLEMENTARITY. lambda_i*_k * cI_k(x*) = 0 for every k: where
//      psi(t_k,p) <= R the multiplier is 0, and where psi(t_k,p) > R the
//      profile is clamped so m = R and cI_k = 0. Bound complementarity is
//      trivial (z* = 0). STRICT complementarity holds at every node except one
//      sitting exactly on a junction, where cI_k and lambda_i*_k vanish
//      together; junction_margin(p) reports the worst node-to-junction distance
//      in units of h and the tests sample p so that it stays well above zero --
//      the policy F6 states, made measurable here.
//
// (V5) STATIONARITY. By construction (F7-G): g is DEFINED as the residual that
//      makes it hold. The check that (F7-G) is the correct transcription of
//      (F7-ADJ) and of Ji is numerical -- the derivative checker at small N and
//      the KKT self-check at every tested N, in tests/test_scale_problems.cpp.
//
// f*(p) IN CLOSED FORM, DERIVED RATHER THAN EVALUATED. Writing f as
// (1/2)x^T W x - g^T x and substituting the stationarity identity
// g = W x* + Je^T lambda_e* + Ji(x*)^T lambda_i*:
//
//     f* = (1/2)x*^T W x* - g^T x*
//        = -(1/2) x*^T W x* - lambda_e*^T (Je x*) - lambda_i*^T (Ji(x*) x*).
//
// Both remaining products are known WITHOUT forming a matrix:
//   * Je x* = the equality rows' constant term, because cE(x) = Je x - b is
//     affine and cE(x*) = 0; b = ( m(0,p) e ; d_0 ; ... ; d_{N-2} );
//   * (Ji(x*) x*)_k = y*_k . y*_k = m(t_k,p)^2, row k of Ji being y_k^T.
// Hence
//
//     f*(p) = -(1/2) sum_k h[ ||y*_k||^2 + rho||u*_k||^2 + 2 gamma y*_k.(C u*_k) ]
//             - sum_k lambda_i*_k * m(t_k,p)^2
//             - lambda_e*^ic . ( m(0,p) e )
//             - sum_{k=0}^{N-2} lambda_e*^(k) . d_k(p).                  (F7-F)
//
// (F7-F) is a DIFFERENT expression from eval_f's -- it never touches g -- so
// comparing eval_f(x_star(p)) against f_star(p) is a genuine transcription
// check on the source terms rather than a tautology. That comparison is pinned
// in tests/test_scale_problems.cpp.
//
// ---------------------------------------------------------------------
// STRUCTURE, AND WHY NOTHING DENSIFIES. Per node the Hessian's upper triangle
// carries ns state diagonal entries (obj_scale*h + lambda_i_k -- the objective
// and the path row's contribution, ALWAYS emitted, including when lambda_i is
// zero or obj_scale is zero), ns state-control cross entries (row y_i, column
// u_{i mod nc}, value obj_scale*h*gamma -- the state-control coupling that
// makes hess L non-diagonal) and nc control diagonal entries, i.e.
//
//     nnz(hess L) = N*(2 ns + nc),
//     nnz(Je)     = ns + (N-1)*(6 ns - 2),
//     nnz(Ji)     = N*ns,
//
// all O(n), and every one of the three patterns is FIXED: it does not move
// with x, with p, with obj_scale or with the lambda_e/lambda_i VALUES, as
// nlp_model.h's STRUCTURAL PATTERN INVARIANCE (extended in Phase-5 Task 0 to
// cover eval_hess's arguments) requires of every implementer and as
// ParametricNlpModel's precondition 1 additionally requires in p. Ji's entries
// ARE the state values and are emitted even at y_k = 0, where every one of
// them is numerically zero.
//
// The p-dependent DATA (q_k, r_k, d_k, the boundary constant) is rebuilt once
// per set_parameters into O(n) storage and read back with no transcendental
// call in any eval_*, which is what keeps the n = 10^4 finite-difference gate
// -- 2n objective evaluations of O(n) each -- inside its runtime budget.
//
// CONDITIONING, AND THE K0 GEOMETRY (Accelerate standing rule,
// docs/notes/2026-07-28-accelerate-audit-checklist.md: a fixture must not
// present exactly-singular K0 geometry, and no counter pinned on it may depend
// on a backend-specific factorization path).
//
//   (a) hess L IS POSITIVE DEFINITE for obj_scale > 0 and lambda_i >= 0. Each
//       node block is obj_scale*h*[[I, gamma C],[gamma C^T, rho I]] plus a
//       nonnegative diagonal (the path term). The first matrix's eigenvalues
//       are obj_scale*h*(1 +/- gamma sigma_i(C)) and obj_scale*h*rho; C's
//       columns are the indicator vectors of the residue classes of the state
//       indices mod nc, which are ORTHOGONAL, so C^T C is diagonal with entries
//       m_j = #{i : i mod nc == j} and ||C||_2 = sqrt(max_j m_j) =
//       sqrt(ceil(ns/nc)) = sqrt(m_max). Setting gamma := kCouplingScale /
//       sqrt(m_max) therefore gives gamma*||C||_2 = 1/2 EXACTLY, INDEPENDENT OF
//       (ns, nc): every node block's spectrum lies in obj_scale*h*[1/2, 3/2], a
//       condition number of at most 3 per block at every shape and size. That
//       is the whole reason gamma is shape-dependent rather than a literal.
//   (b) THE ACTIVE CONSTRAINT GRADIENTS ARE LINEARLY INDEPENDENT (LICQ) for
//       0 < p < R, so K0 = [[hess L, A^T],[A, 0]] with A = [Je; Ji_active] is
//       nonsingular before any regularization is added.
//         * Je alone has full row rank ns*N: order its rows (ic, def_0, ...,
//           def_{N-2}) and take the STATE columns only; the result is block
//           lower bidiagonal with diagonal blocks I (the ic rows) and
//           I - (h/2)A (defect k, in the y_{k+1} columns), and I - (h/2)A is
//           nonsingular because ||(h/2)A|| <= (h/2)(theta+sigma) < 1 for every
//           N >= 3.
//         * For the combined matrix, suppose alpha^T Je + beta^T Ji_active = 0.
//           Ji has no control columns, so the control columns give
//           alpha^T Je_u = 0; the state columns give alpha = -Je_y^{-T} Ji_y^T
//           beta, whence beta^T ( Ji_y Je_y^{-1} Je_u ) = 0. Row k of that
//           product is y*_k^T (dy/du), and its block in NODE k's OWN control
//           columns is (h/2) y*_k^T (I - (h/2)A)^{-1} C for k >= 1 (from the
//           defect row k-1, which is what determines y_k). Now
//           (I - (h/2)A)^{-1} = (1 + (h/2)theta)^{-1} sum_{m>=0}
//           ((h sigma/2)/(1 + (h/2)theta))^m (subdiagonal)^m is a NONNEGATIVE
//           matrix (a Neumann series with positive coefficients), and y*_k > 0
//           componentwise (e > 0, m > 0), so that block is a positive multiple
//           of the residue-class sums of a strictly positive vector -- strictly
//           positive in every one of its nc entries. Each active node
//           contributes its own control columns, so the matrix is
//           triangular-with-nonzero-diagonal in the causal ordering and has
//           full row rank; hence beta = 0, then alpha = 0.
//         * THE k = 0 EXCEPTION IS EXACTLY THE DEGENERACY NAMED ABOVE:
//           dy_0/du = 0 (node 0's state is pinned by the ic rows), so the
//           argument needs node 0's path row to be INACTIVE -- which holds iff
//           psi(0,p) = p < R, i.e. exactly on the design range.
//       This was checked numerically as well as derived: rank[Je; Ji_active] is
//       full at every sampled (N, p) in the design range.
//   (c) No counter pinned against this family depends on a backend-specific
//       factorization path, and the small-N fixtures present the SAME geometry
//       as the large ones, so nothing here rests on a size-dependent accident.
// =====================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(__linux__)
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <tycho_sqp/nlp_model.h>
#include <tycho_sqp/types.h>

#include "parametric_families.h"

namespace tycho::sqp::test_support {

#if defined(__linux__)
// Peak resident set size in MiB, from /proc/self/status's VmHWM. INFORMATIONAL
// ONLY -- printed/recorded, never asserted: it is a property of the allocator
// and the whole process, not of any one fixture or solve alone. Originally
// test_scale_problems.cpp's own helper (ScaleF7ScaleReadinessAtHundredThousand
// Variables' peak-RSS print); PHASE-5 TASK 2 moved it here, unchanged, so the
// bench harness (bench/bench_scale.cpp) can reuse the SAME /proc parser
// rather than growing a second copy -- see that file's own note.
// strtol rather than sscanf("%ld"): sscanf cannot report a conversion failure
// distinctly from a range error, which clang-tidy flags
// (bugprone-unchecked-string-to-number-conversion) and which would here turn a
// malformed line into a confident wrong number rather than the -1 sentinel.
inline double peak_rss_mib() {
    std::FILE *f = std::fopen("/proc/self/status", "r");
    if (f == nullptr) {
        return -1.0;
    }
    static constexpr char kKey[] = "VmHWM:";
    char line[256];
    double kib = -1.0;
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        if (std::strncmp(line, kKey, sizeof(kKey) - 1) != 0) {
            continue;
        }
        char *end = nullptr;
        errno = 0;
        const long value = std::strtol(line + sizeof(kKey) - 1, &end, 10);
        if (end != line + sizeof(kKey) - 1 && errno == 0 && value >= 0) {
            kib = static_cast<double>(value);
        }
        break;
    }
    std::fclose(f);
    return kib < 0.0 ? -1.0 : kib / 1024.0;
}
#else
inline double peak_rss_mib() { return -1.0; } // not available off Linux
#endif

class F7CollocationChain : public ParametricNlpModel {
  public:
    // ---- the family's fixed constants (each derived from, or explained in,
    // the banner above) ----
    static constexpr double kRho = 1.0;           // control weight in the cost
    static constexpr double kCouplingScale = 0.5; // gamma*||C||_2, shape-independent
    static constexpr double kTheta = 1.0;         // A's diagonal damping
    static constexpr double kSigma = 0.5;         // A's subdiagonal cascade
    static constexpr double kControlAmp = 0.5;    // ||u*||inf
    static constexpr double kControlBound = 1.0;  // the box: |u| <= this
    // <numbers> is not pulled in anywhere else in this test bed; the literal
    // is exact to the last bit of binary64 (F6's own reasoning, verbatim).
    static constexpr double kPi = 3.14159265358979323846;

    // nodes N >= 3, ns >= 1 states and nc >= 1 controls per node. p0 is the
    // initial parameter; the default sits inside the design range (R/2, R) and
    // was chosen for its junction margin (0.40 h at N = 10, 0.44 h at N = 100),
    // so no node sits near a junction at either size the path gate solves at.
    // radius is the path bound R.
    explicit F7CollocationChain(Index nodes = 10, Index states = 3, Index controls = 2,
                                double p0 = 0.68, double radius = 1.0)
        : nodes_(checked_nodes(nodes)), ns_(checked_dim(states, "states")),
          nc_(checked_dim(controls, "controls")), radius_(checked_radius(radius)),
          h_(1.0 / static_cast<double>(nodes_ - 1)), gamma_(kCouplingScale / std::sqrt(m_max())),
          p_(Vec::Constant(1, p0)) {
        build_vectors();
        rebuild_data();
    }

    // ---- shape accessors ----
    Index node_count() const { return nodes_; }
    Index state_dim() const { return ns_; }
    Index control_dim() const { return nc_; }
    Index vars_per_node() const { return ns_ + nc_; }
    double step() const { return h_; }
    double radius() const { return radius_; }
    double gamma() const { return gamma_; }
    double node_time(Index k) const { return static_cast<double>(k) * h_; }
    // 0-based index of node k's first variable (its state block; the control
    // block follows at + state_dim()).
    Index node_offset(Index k) const { return k * (ns_ + nc_); }
    // e and s are exposed for the same reason F1-F6 expose their analytic
    // surface: a downstream task that wants to build its own reference profile
    // (a mesh transfer, a predictor's finite difference, the PSIOPT bridge's
    // problem dump) should read the vectors this family actually uses rather
    // than re-transcribe two formulas from the banner. Neither has a caller
    // TODAY -- flagged in review round 1 (Q-8) so a later reader does not
    // assume they are exercised.
    const Vec &state_direction() const { return e_; }   // the unit vector e
    const Vec &costate_direction() const { return s_; } // the vector s

    // ---- the profile functions (banner, CHOICE 1 / CHOICE 2) ----
    static double phi(double t) { return 1.0 + std::sin(kPi * t); }
    double psi(double t, double p) const { return p * phi(t); }
    double profile(double t, double p) const { return std::min(psi(t, p), radius_); }
    // (F7-LI)'s density: > 0 exactly on the active window.
    double nu(double t, double p) const { return std::max(0.0, psi(t, p) - radius_); }
    // (F7-LE)'s density.
    static double mu(double t) { return std::cos(kPi * t); }
    double control_star(Index j, double t) const {
        return kControlAmp *
               std::cos(kPi * t + 2.0 * kPi * static_cast<double>(j) / static_cast<double>(nc_));
    }

    // ---- the activity geometry (F7-JCT) ----
    // At or below p_activation() no path row is active. At or above
    // p_saturation() every row is, INCLUDING node 0 -- which is the LICQ
    // degeneracy named in the banner, and the upper end of the valid range.
    // In between, the active set is the open interval (junction_left(p),
    // junction_right(p)).
    double p_activation() const { return 0.5 * radius_; }
    double p_saturation() const { return radius_; }
    // THE EMPTY-WINDOW BOUNDARY, which the clamp inside junction_left() encodes
    // and which a caller reading a junction back must know about: for
    // 0 < p <= p_activation() = R/2 the defining quantity xi(p) = R/p - 1 is
    // >= 1, arcsin's argument is clamped to 1, and BOTH junctions collapse to
    // t = 1/2 -- an empty window, no row active, which is a legitimate branch of
    // the family (path_samples() in tests/test_scale_problems.cpp samples it at
    // p = 0.45) and NOT an error. At p = R/2 EXACTLY the collapse is to the
    // single point t = 1/2 where psi = R exactly: row_active()'s strict > calls
    // that node INACTIVE while it is geometrically ON the bound with a zero
    // multiplier, so a sweep that samples p = p_activation() at ODD N (which
    // puts a node at t = 1/2) would see active_set() and a geometric active set
    // disagree there. No test samples that pair.
    double junction_left(double p) const {
        checked_path_parameter(p, "junction_left");
        const double xi = std::min(1.0, std::max(0.0, radius_ / p - 1.0));
        return std::asin(xi) / kPi;
    }
    double junction_right(double p) const { return 1.0 - junction_left(p); }
    // Node k's path row is active at the optimum iff psi(t_k, p) > R -- the
    // STRICT form, matching F6's policy: a node exactly on a junction is
    // geometrically on the boundary with a zero multiplier and is reported
    // INACTIVE. No tested (N, p) pair puts a node there.
    //
    // THIS ONE IS DELIBERATELY *NOT* RANGE-GUARDED, unlike active_set() which
    // is built from it. It is the predicate that DEMONSTRATES the p >= R
    // degeneracy -- ScaleF7.JunctionGeometryIsAnalytic evaluates it at
    // p = 1.05 and at p_saturation() +/- 1e-9 to pin that node 0 enters the
    // active set exactly at R -- and a guard here would make that
    // demonstration unwritable. It answers a question about the model's
    // geometry ("would this row hold with equality?"), not about the
    // manufactured solution, which is what the guarded accessors claim.
    bool row_active(Index k, double p) const { return psi(node_time(k), p) > radius_; }
    // The smallest node-to-junction distance, in units of h -- the strict
    // complementarity margin of (V4), made measurable. Meaningful only on the
    // sub-interval regime (p_activation(), p_saturation()), where both
    // junctions are interior; below p_activation() both collapse to t = 1/2 and
    // the number it returns is a distance to a point no activity happens at.
    double junction_margin(double p) const {
        const double left = junction_left(p); // range-guards p
        const double right = junction_right(p);
        double best = std::numeric_limits<double>::infinity();
        for (Index k = 0; k < nodes_; ++k) {
            const double t = node_time(k);
            best = std::min(best, std::min(std::abs(t - left), std::abs(t - right)));
        }
        return best / h_;
    }

    // ---- ParametricNlpModel ----
    Index parameter_dim() const override { return 1; }
    void set_parameters(const Vec &p) override {
        parametric_detail::check_parameter_size(p, 1, "F7CollocationChain");
        p_ = p;
        rebuild_data(); // VALUES move with p; not one sparsity pattern does
    }
    Vec parameters() const override { return p_; }
    double p() const { return p_(0); }

    // ---- NlpModel ----
    Index n() const override { return total_vars(); }
    Index me() const override { return ns_ * nodes_; }
    Index mi() const override { return nodes_; }

    // f(x) = sum_k h[ (1/2)|y|^2 + (rho/2)|u|^2 + gamma y.(Cu) ] - g^T x.
    //
    // THE RAW POINTER IS DELIBERATE AND IS CONFINED TO THIS ONE FUNCTION.
    // derivative_check.h's assert_gradient calls eval_f 2n times, so the
    // n = 10^4 gate calls it 20000 times; reading x through Eigen's checked
    // operator() (or through .segment() Blocks) costs 32.7 s of the Debug
    // build's budget there against 4.1 s reading x.data() -- measured, both
    // figures, in tests/test_scale_problems.cpp's gate. Every OTHER eval_*
    // below keeps Eigen's Debug bounds checks, and this function's index
    // arithmetic is the same arithmetic they use, pinned by the structure
    // tests and by the derivative checker at small N.
    double eval_f(const Vec &x) const override {
        // Q-4, review round 1: the raw pointer below removes Eigen's per-access
        // Debug bounds check, so the size precondition it silently relied on is
        // restored explicitly. Free in Release, and it costs nothing of the
        // 8.7x win because it is one comparison per CALL, not per element.
        eigen_assert(x.size() == total_vars());
        const double *xd = x.data();
        double acc = 0.0;
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            for (Index i = 0; i < ns_; ++i) {
                const double y = xd[b + i];
                acc += 0.5 * y * y + gamma_ * y * xd[b + ns_ + cmap_[i]];
            }
            for (Index j = 0; j < nc_; ++j) {
                const double u = xd[b + ns_ + j];
                acc += 0.5 * kRho * u * u;
            }
        }
        return h_ * acc - data_g_.dot(x);
    }

    // grad f = W x - g, block by block.
    Vec eval_grad(const Vec &x) const override {
        Vec out(total_vars());
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            const auto y = x.segment(b, ns_);
            const auto u = x.segment(b + ns_, nc_);
            for (Index j = 0; j < nc_; ++j) {
                out(b + ns_ + j) = h_ * kRho * u(j);
            }
            for (Index i = 0; i < ns_; ++i) {
                const Index j = i % nc_;
                out(b + i) = h_ * (y(i) + gamma_ * u(j));
                out(b + ns_ + j) += h_ * gamma_ * y(i);
            }
        }
        return out - data_g_;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec out(me());
        for (Index i = 0; i < ns_; ++i) {
            out(i) = x(i) - data_ic_(i);
        }
        for (Index k = 0; k + 1 < nodes_; ++k) {
            const Index b = node_offset(k);
            const Index b1 = node_offset(k + 1);
            const Index row = ns_ + k * ns_;
            for (Index i = 0; i < ns_; ++i) {
                const double f0 = dynamics(x, b, i);
                const double f1 = dynamics(x, b1, i);
                out(row + i) = x(b1 + i) - x(b + i) - 0.5 * h_ * (f0 + f1) - data_d_(k * ns_ + i);
            }
        }
        return out;
    }

    Vec eval_ci(const Vec &x) const override {
        Vec out(mi());
        for (Index k = 0; k < nodes_; ++k) {
            out(k) = 0.5 * (x.segment(node_offset(k), ns_).squaredNorm() - radius_ * radius_);
        }
        return out;
    }

    // TASK 8 (the eval-economics carry): THE VALUES-ONLY DEMONSTRATION
    // OVERRIDE. nlp_model.h's eval_values default implementation (calling
    // eval_f/eval_ce/eval_ci separately) is already correct for every model
    // in this tree, F7 included -- this override exists to demonstrate that
    // a model CAN do better than three independent calls, not because the
    // default would have been wrong.
    //
    // THE ARITHMETIC BELOW IS TRANSCRIBED VERBATIM from eval_f's node loop,
    // eval_ci's node loop and eval_ce's defect-row loop above -- same
    // operations, same order, so the result is bit-identical to calling the
    // three separately, not merely close (the whole point of the override
    // is to reuse the SAME numbers more cheaply, never to compute DIFFERENT
    // ones; see nlp_model.h's own note on that invariant). What changes is
    // which loops run: f's per-node accumulation and cI's per-node norm
    // share the same node loop here (one pass over x instead of two), and
    // NONE of eval_grad/eval_jac_e/eval_jac_i/eval_hess is called at all --
    // the entire saving this override buys over the default is exactly
    // those four calls, on every values-only query the driver makes (this
    // family's sparsity is large and banded at scale, so a Jacobian/Hessian
    // call is the expensive part eval_values exists to let a caller skip).
    // cE's defect rows stay their OWN pass over node PAIRS, deliberately:
    // folding them into the per-node loop above would read x(b1 + i) before
    // node k+1's own iteration of that loop runs, changing which values are
    // in hand when -- not merely a style choice, since it changes what is
    // being computed rather than just how.
    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override {
        eigen_assert(x.size() == total_vars());
        cE = Vec(me());
        cI = Vec(mi());
        double acc = 0.0;
        for (Index i = 0; i < ns_; ++i) {
            cE(i) = x(i) - data_ic_(i);
        }
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            // -- eval_f's inner loop, verbatim --
            for (Index i = 0; i < ns_; ++i) {
                const double y = x(b + i);
                acc += 0.5 * y * y + gamma_ * y * x(b + ns_ + cmap_[i]);
            }
            for (Index j = 0; j < nc_; ++j) {
                const double u = x(b + ns_ + j);
                acc += 0.5 * kRho * u * u;
            }
            // -- eval_ci's inner loop, verbatim --
            cI(k) = 0.5 * (x.segment(b, ns_).squaredNorm() - radius_ * radius_);
        }
        // -- eval_ce's defect rows, verbatim -- see the note above for why
        // this stays its own pass rather than folding into the loop above.
        for (Index k = 0; k + 1 < nodes_; ++k) {
            const Index b = node_offset(k);
            const Index b1 = node_offset(k + 1);
            const Index row = ns_ + k * ns_;
            for (Index i = 0; i < ns_; ++i) {
                const double f0 = dynamics(x, b, i);
                const double f1 = dynamics(x, b1, i);
                cE(row + i) = x(b1 + i) - x(b + i) - 0.5 * h_ * (f0 + f1) - data_d_(k * ns_ + i);
            }
        }
        f = h_ * acc - data_g_.dot(x);
    }

    // hess L = obj_scale*W + sum_k lambda_i(k) * (I on node k's state block);
    // cE is affine, so lambda_e contributes nothing. EVERY entry below is
    // emitted on every call, whatever obj_scale and the multipliers are.
    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(nodes_ * (2 * ns_ + nc_)));
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            for (Index i = 0; i < ns_; ++i) {
                t.emplace_back(static_cast<int>(b + i), static_cast<int>(b + i),
                               obj_scale * h_ + lambda_i(k));
                t.emplace_back(static_cast<int>(b + i), static_cast<int>(b + ns_ + (i % nc_)),
                               obj_scale * h_ * gamma_);
            }
            for (Index j = 0; j < nc_; ++j) {
                t.emplace_back(static_cast<int>(b + ns_ + j), static_cast<int>(b + ns_ + j),
                               obj_scale * h_ * kRho);
            }
        }
        return parametric_detail::make_upper(total_vars(), t);
    }

    // Je -- CONSTANT in x (cE is affine); see the transcription block above.
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(ns_ + (nodes_ - 1) * (6 * ns_ - 2)));
        for (Index i = 0; i < ns_; ++i) {
            t.emplace_back(static_cast<int>(i), static_cast<int>(i), 1.0);
        }
        const double half = 0.5 * h_;
        for (Index k = 0; k + 1 < nodes_; ++k) {
            const Index b = node_offset(k);
            const Index b1 = node_offset(k + 1);
            const Index row = ns_ + k * ns_;
            for (Index i = 0; i < ns_; ++i) {
                const int r = static_cast<int>(row + i);
                // d/dy_k = -I - (h/2)A and d/dy_{k+1} = I - (h/2)A, with
                // A_{ii} = -theta and A_{i,i-1} = sigma.
                t.emplace_back(r, static_cast<int>(b + i), -1.0 + half * kTheta);
                t.emplace_back(r, static_cast<int>(b1 + i), 1.0 + half * kTheta);
                if (i > 0) {
                    t.emplace_back(r, static_cast<int>(b + i - 1), -half * kSigma);
                    t.emplace_back(r, static_cast<int>(b1 + i - 1), -half * kSigma);
                }
                // d/du_k = d/du_{k+1} = -(h/2)C.
                t.emplace_back(r, static_cast<int>(b + ns_ + (i % nc_)), -half);
                t.emplace_back(r, static_cast<int>(b1 + ns_ + (i % nc_)), -half);
            }
        }
        return parametric_detail::make_jac(me(), total_vars(), t);
    }

    // Ji -- row k is y_k^T on node k's state block. Every entry is emitted at
    // every x, INCLUDING x = 0 where all of them are numerically zero.
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(nodes_ * ns_));
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            for (Index i = 0; i < ns_; ++i) {
                t.emplace_back(static_cast<int>(k), static_cast<int>(b + i), x(b + i));
            }
        }
        return parametric_detail::make_jac(mi(), total_vars(), t);
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }

    // A flat state profile at 30% of the path radius with zero controls:
    // strictly inside every path row and the box, violating only the boundary
    // and defect rows, and equal to x*(p) for no p at all.
    Vec start_point() const override {
        Vec x = Vec::Zero(total_vars());
        for (Index k = 0; k < nodes_; ++k) {
            x.segment(node_offset(k), ns_) = 0.3 * radius_ * e_;
        }
        return x;
    }

    // ---- the analytic path (derivations in the banner) ----
    //
    // EVERY ACCESSOR BELOW RANGE-GUARDS p AGAINST THE DESIGN RANGE (0, R), and
    // that is where the guard belongs rather than on set_parameters(): the
    // MODEL is total in p (every eval_* is well defined at any p, and
    // ScaleF7Contract.PatternsAreIndependentOfXPMultipliersAndObjScale
    // deliberately drives it to p = 1.4), while the CLAIM "this is the
    // optimum" is not. Outside the range the claim fails in both directions
    // and used to fail SILENTLY: at p >= R node 0's path row goes active while
    // node 0's state is pinned by the initial-condition rows, so LICQ fails and
    // K0 is exactly singular (rank 39 of 40 at N = 10, ns = 3, nc = 2,
    // p = 1.05, measured in review round 1) -- the geometry the Accelerate
    // standing rule forbids a fixture to present; at p < 0 the profile is not
    // even FEASIBLE (max cI(x*) = +0.209 at p = -0.6, measured), so f_star
    // would be a number about nothing. The two ENDS of the range are excluded
    // for different reasons and both are excluded strictly: p = R is where the
    // rank deficiency starts (it bites AT R, not only above it), and p = 0 is
    // where the profile degenerates to zero. p in (0, R/2] is INSIDE the range
    // and merely has an empty window -- see junction_left()'s note.

    Vec x_star(double p) const {
        checked_path_parameter(p, "x_star");
        Vec x(total_vars());
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k);
            const double t = node_time(k);
            x.segment(b, ns_) = profile(t, p) * e_;
            for (Index j = 0; j < nc_; ++j) {
                x(b + ns_ + j) = control_star(j, t);
            }
        }
        return x;
    }

    // (F7-F): derived from stationarity, NOT eval_f evaluated at x_star -- so
    // the two agreeing is a transcription check on the source terms g.
    double f_star(double p) const {
        checked_path_parameter(p, "f_star");
        double acc = 0.0;
        Vec y_prev(ns_), u_prev(nc_), y(ns_), u(nc_);
        star_at(0, p, y, u);
        for (Index k = 0; k < nodes_; ++k) {
            if (k > 0) {
                y_prev = y;
                u_prev = u;
                star_at(k, p, y, u);
                // -lambda_e*^(k-1) . d_{k-1}, with d from (F7-D).
                acc -= costate_at(k - 1).dot(defect_constant(y_prev, u_prev, y, u));
            }
            double cross = 0.0;
            for (Index i = 0; i < ns_; ++i) {
                cross += y(i) * u(i % nc_);
            }
            acc -= 0.5 * h_ * (y.squaredNorm() + kRho * u.squaredNorm() + 2.0 * gamma_ * cross);
            acc -= lambda_i_at(k, p) * y.squaredNorm();
        }
        acc -= (h_ * s_).dot(profile(0.0, p) * e_); // -lambda_e*^ic . (m(0,p) e)
        return acc;
    }

    // (F7-LE). The costates are p-INDEPENDENT by choice -- the parameter enters
    // the primal profile and the data, not the multipliers of the affine rows.
    Vec lambda_e_star(double p) const {
        checked_path_parameter(p, "lambda_e_star");
        Vec le(me());
        le.head(ns_) = h_ * s_;
        for (Index k = 0; k + 1 < nodes_; ++k) {
            le.segment(ns_ + k * ns_, ns_) = costate_at(k);
        }
        return le;
    }

    // (F7-LI).
    Vec lambda_i_star(double p) const {
        checked_path_parameter(p, "lambda_i_star");
        Vec li(mi());
        for (Index k = 0; k < nodes_; ++k) {
            li(k) = lambda_i_at(k, p);
        }
        return li;
    }

    // Every variable is free at x* ((V3)), so the bound multipliers vanish.
    Vec z_star(double p) const {
        checked_path_parameter(p, "z_star");
        return Vec::Zero(total_vars());
    }

    AnalyticActiveSet active_set(double p) const {
        checked_path_parameter(p, "active_set");
        AnalyticActiveSet a;
        a.bound_active.assign(static_cast<std::size_t>(total_vars()), 0);
        a.ineq_active.assign(static_cast<std::size_t>(nodes_), 0);
        for (Index k = 0; k < nodes_; ++k) {
            a.ineq_active[static_cast<std::size_t>(k)] =
                static_cast<std::uint8_t>(row_active(k, p) ? 1 : 0);
        }
        return a;
    }

  private:
    Index total_vars() const { return nodes_ * (ns_ + nc_); }

    // m_max = max_j #{i : i mod nc == j} = ceil(ns/nc) = ||C||_2^2.
    //
    // THE INTEGER DIVISION IS THE POINT, hence the trailing NOLINT below:
    // (ns + nc - 1) / nc is ceil(ns/nc) evaluated in integers, and it is
    // load-bearing -- gamma = kCouplingScale/sqrt(m_max) makes gamma*||C||_2
    // exactly 1/2 only if m_max is the integer ceiling (CONDITIONING (a)).
    // Verified at ns/nc = 1/1 -> 1, 3/2 -> 2, 4/3 -> 2, 7/3 -> 3, 1/5 -> 1.
    // The suppression is TRAILING rather than NOLINTNEXTLINE because
    // NOLINTNEXTLINE applies to the next LINE, not the next statement: with
    // explanatory prose between it and the code it is inert and clang-tidy
    // fires anyway (review round 1, Q-7 residue -- reproduced, then fixed here).
    double m_max() const {
        return static_cast<double>((ns_ + nc_ - 1) / nc_); // NOLINT(bugprone-integer-division)
    }

    // F(y, u)_i = -theta y_i + sigma y_{i-1} + u_{i mod nc}, read straight out
    // of x at node offset b (no temporaries -- this is eval_ce's inner loop).
    double dynamics(const Vec &x, Index b, Index i) const {
        double v = -kTheta * x(b + i) + x(b + ns_ + (i % nc_));
        if (i > 0) {
            v += kSigma * x(b + i - 1);
        }
        return v;
    }

    // (A^T w)_i = -theta w_i + sigma w_{i+1}.
    Vec apply_a_transpose(const Vec &w) const {
        Vec out(ns_);
        for (Index i = 0; i < ns_; ++i) {
            out(i) = -kTheta * w(i);
            if (i + 1 < ns_) {
                out(i) += kSigma * w(i + 1);
            }
        }
        return out;
    }

    // (C u)_i = u_{i mod nc}.
    Vec apply_c(const Vec &u) const {
        Vec out(ns_);
        for (Index i = 0; i < ns_; ++i) {
            out(i) = u(i % nc_);
        }
        return out;
    }

    // (C^T w)_j = sum over { i : i mod nc == j } of w_i.
    Vec apply_c_transpose(const Vec &w) const {
        Vec out = Vec::Zero(nc_);
        for (Index i = 0; i < ns_; ++i) {
            out(i % nc_) += w(i);
        }
        return out;
    }

    void star_at(Index k, double p, Vec &y, Vec &u) const {
        const double t = node_time(k);
        y = profile(t, p) * e_;
        for (Index j = 0; j < nc_; ++j) {
            u(j) = control_star(j, t);
        }
    }

    double lambda_i_at(Index k, double p) const { return h_ * nu(node_time(k), p); }
    Vec costate_at(Index k) const { return (h_ * mu(node_time(k))) * s_; }

    // (F7-D) for one interval, from its two endpoints' profiles.
    Vec defect_constant(const Vec &y0, const Vec &u0, const Vec &y1, const Vec &u1) const {
        Vec d(ns_);
        for (Index i = 0; i < ns_; ++i) {
            double f0 = -kTheta * y0(i) + u0(i % nc_);
            double f1 = -kTheta * y1(i) + u1(i % nc_);
            if (i > 0) {
                f0 += kSigma * y0(i - 1);
                f1 += kSigma * y1(i - 1);
            }
            d(i) = y1(i) - y0(i) - 0.5 * h_ * (f0 + f1);
        }
        return d;
    }

    // The shape-only vectors (e, s, the box), built once at construction.
    void build_vectors() {
        cmap_.resize(static_cast<std::size_t>(ns_));
        for (Index i = 0; i < ns_; ++i) {
            cmap_[static_cast<std::size_t>(i)] = i % nc_;
        }
        e_ = Vec(ns_);
        s_ = Vec(ns_);
        for (Index i = 0; i < ns_; ++i) {
            e_(i) = 1.0 + static_cast<double>(i) / static_cast<double>(ns_);
            s_(i) = 1.0 - 0.5 * static_cast<double>(i) / static_cast<double>(ns_);
        }
        e_ /= e_.norm();
        lower_ = Vec::Constant(total_vars(), -parametric_detail::kParamInf);
        upper_ = Vec::Constant(total_vars(), parametric_detail::kParamInf);
        for (Index k = 0; k < nodes_; ++k) {
            const Index b = node_offset(k) + ns_;
            lower_.segment(b, nc_).setConstant(-kControlBound);
            upper_.segment(b, nc_).setConstant(kControlBound);
        }
    }

    // Rebuilds the p-dependent DATA -- the boundary constant, the defect
    // forcing (F7-D) and the objective's source terms (F7-G) -- once per
    // set_parameters, so that no eval_* has to evaluate a transcendental.
    void rebuild_data() {
        const double p = p_(0);
        data_ic_ = profile(0.0, p) * e_;
        data_d_ = Vec(ns_ * (nodes_ - 1));
        data_g_ = Vec(total_vars());

        Vec y_prev(ns_), u_prev(nc_), y(ns_), u(nc_);
        star_at(0, p, y_prev, u_prev);
        for (Index k = 0; k + 1 < nodes_; ++k) {
            star_at(k + 1, p, y, u);
            data_d_.segment(k * ns_, ns_) = defect_constant(y_prev, u_prev, y, u);
            y_prev = y;
            u_prev = u;
        }

        const Vec lam_ic = h_ * s_;
        for (Index k = 0; k < nodes_; ++k) {
            star_at(k, p, y, u);
            // (F7-ADJ)
            Vec adj_y = Vec::Zero(ns_);
            Vec costate_sum = Vec::Zero(ns_);
            if (k == 0) {
                adj_y += lam_ic;
            }
            if (k >= 1) {
                const Vec lm = costate_at(k - 1);
                adj_y += lm - 0.5 * h_ * apply_a_transpose(lm);
                costate_sum += lm;
            }
            if (k + 1 < nodes_) {
                const Vec lk = costate_at(k);
                adj_y -= lk + 0.5 * h_ * apply_a_transpose(lk);
                costate_sum += lk;
            }
            const Vec adj_u = -0.5 * h_ * apply_c_transpose(costate_sum);
            // (F7-G)
            const Index b = node_offset(k);
            const double li = lambda_i_at(k, p);
            const Vec cu = apply_c(u);
            const Vec cty = apply_c_transpose(y);
            for (Index i = 0; i < ns_; ++i) {
                data_g_(b + i) = h_ * (y(i) + gamma_ * cu(i)) + adj_y(i) + li * y(i);
            }
            for (Index j = 0; j < nc_; ++j) {
                data_g_(b + ns_ + j) = h_ * (kRho * u(j) + gamma_ * cty(j)) + adj_u(j);
            }
        }
    }

    // THE DESIGN-RANGE GUARD for every analytic-path accessor (review round 1,
    // Q-1). Both ends are excluded STRICTLY and for different reasons, both
    // named in the message so the throw is the thing that reports (T6):
    // p >= R makes node 0's path row active while node 0's state is pinned by
    // the initial-condition rows, which costs LICQ and leaves K0 exactly
    // singular; p <= 0 collapses the profile and leaves x_star infeasible. The
    // !(...) form catches NaN as well.
    void checked_path_parameter(double p, const char *what) const {
        if (!(p > 0.0 && p < radius_)) {
            throw std::invalid_argument(fmt::format(
                "F7CollocationChain::{}: p ({}) is outside the design range (0, R) with R = {}. "
                "At p >= R node 0's path row is active while the initial-condition rows pin "
                "node 0's state, so LICQ fails and K0 is singular; at p <= 0 the manufactured "
                "profile is not feasible. The active window is empty on (0, R/2] and is the "
                "sub-interval (T(p), 1-T(p)) on (R/2, R).",
                what, p, radius_));
        }
    }

    // ---- construction-time checks. Each runs in the MEMBER-INITIALIZER LIST,
    // before any vector is sized from the value it guards: a body check would
    // fire after Vec::Constant(n, ...) had already tripped an Eigen assertion
    // (Debug) or allocated nonsense (Release), i.e. it would be a diagnostic
    // that never reports (project rule T6). F3SpringChain's reasoning, applied
    // to three arguments instead of two.
    static Index checked_nodes(Index nodes) {
        if (nodes < 3) {
            throw std::invalid_argument(fmt::format(
                "F7CollocationChain: nodes ({}) must be >= 3 (a collocation chain needs at least "
                "two intervals for the path window to be interior)",
                nodes));
        }
        return nodes;
    }
    static Index checked_dim(Index d, const char *what) {
        if (d < 1) {
            throw std::invalid_argument(
                fmt::format("F7CollocationChain: {} ({}) must be >= 1", what, d));
        }
        return d;
    }
    static double checked_radius(double r) {
        if (!(r > 0.0)) { // also catches NaN
            throw std::invalid_argument(fmt::format(
                "F7CollocationChain: radius ({}) must be > 0; it is the path bound R, and the "
                "family's design range for p is (0, R)",
                r));
        }
        return r;
    }

    Index nodes_;
    Index ns_;
    Index nc_;
    double radius_;
    double h_;
    double gamma_;
    Vec p_;

    // cmap_[i] = i mod nc, C's row-to-column map, MATERIALIZED FOR eval_f
    // ALONE. Every other site recomputes i % nc_ inline, and that asymmetry is
    // deliberate rather than an oversight (Q-9, review round 1): eval_f is the
    // one function the n = 10^4 finite-difference gate calls 20000 times, so it
    // is the one place a per-element integer division shows up in a measured
    // budget -- the same measurement that justifies its x.data() read. Two
    // encodings of one map is a real (small) cost; it is paid here and nowhere
    // else.
    std::vector<Index> cmap_;
    Vec e_;       // unit state direction
    Vec s_;       // costate direction
    Vec lower_;   // -inf on states, -kControlBound on controls
    Vec upper_;   // +inf on states, +kControlBound on controls
    Vec data_ic_; // m(0,p) * e
    Vec data_d_;  // (F7-D), ns*(N-1) entries
    Vec data_g_;  // (F7-G), n entries
};

} // namespace tycho::sqp::test_support
