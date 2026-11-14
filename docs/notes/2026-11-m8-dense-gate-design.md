# M8 dense gate -- the detector judges the equilibrated matrix; the solve gates judge backward error

Status: implemented on branch `m8-dense-gate` (from `origin/m8` `030bae675`). Evidence and the declared event:
`docs/notes/data/2026-11-m8-dense-gate/`.

## 1. The defect

The DN-3 flip made `DensePolicy::kAuto` the default. `SqpDriverElastic.ElasticSurvivesALargeConstraintScale` (row
scale 1e8) then ended `kNumericalError` on the dense arm with no elastic activation, and was pinned to `kNever`
(`2d6d89cd0`). The diagnosis (local branch `diag-dn3-flip-findings`): at x0 the working rows are a dependent opposite
pair at scale 1e8; the detector flags the factorization; the shift response (Ruiz D, sigma = 2^-30) produces answers
the shifted-solve gate refuses every time (64 dense factorizations, 32 flagged, 32 shifted refactors, 32 refusals);
the walk escalates to `kNumericalError`, and the driver routes that to nothing. The sparse arm (Pardiso, scaling and
matching, pivot perturbation 1e-8) solves the same systems and reaches the Farkas `kInfeasible` the elastic tier needs.

Two measurements were wrong for the same reason -- they are not invariant under the scaling a KKT system carries:

- **The detector** read rcond of the RAW matrix. A diagonal scaling S makes rcond(S A S) as small as one likes while
  the system stays well posed; row scale alone could flag.
- **The gates** read rho = ||r|| / ||b||. For an answer y, ||r|| / ||b|| <= eta (1 + ||K|| ||y|| / ||b||): rho carries
  the condition number, so a backward-stable answer of an ill-conditioned system -- including every answer of a
  regularized singular system -- reads as a failure. The elastic fixture's shifted answers read rho = 1.0 and
  eta = 4.2e-11.

## 2. The detector (fix 1)

`DenseSession::factorize` (kReport, unshifted): the screen (rook D-ratio < 1e-15) and the raw dsycon_rook estimate
are unchanged. When the raw estimate is below 2^-53 the matrix is judged again equilibrated: D =
`ruiz_symmetric_scale(A)` (the very function `hven::detail::equilibration_scale` now delegates to, so the shift
response factors the matrix the detector judged), ||D A D||_1 from the CSR, and ||(D A D)^-1||_1 =
||D^-1 A^-1 D^-1||_1 by Higham's 1-norm estimator (xLACN2's algorithm, `estimate_symmetric_one_norm`) through
dsytrs_rook on the factors already in hand. Flagged iff that rcond is below 2^-53 too (or NaN). On its no-growth
exit the estimator keeps the larger of its last two estimates (C341 F7; each is ||B x||_1 at ||x||_1 = 1, a lower
bound -- xLACN2 keeps the later one, which for a symmetric B can only be smaller through rounding).

- The numerics are not touched: an unflagged factorization keeps the factors it always had (the raw rook factors),
  so a matrix flagged before and cleared now is solved exactly as `DIAG_NOFLAG` solved it, and is then judged by the
  solve gates below.
- Cost: only on screened, raw-flagged factorizations -- one O(nnz) Ruiz pass and about five O(n^2) solves. Counted
  in `SymmetricFactor::Counters::dense_equilibrated_estimate_count`.
- A shifted factorization is already equilibrated; its single estimate decides, as before.
- The IPM factors under kReport but does not consume the flag (its step guard reads the residual), so its numerics
  and statuses do not move; only its `dense_flagged_count` falls.

**The raw factors of a cleared matrix (C341 F3, investigated and decided).** A matrix the re-estimate clears keeps
dsytrf_rook's factors of the RAW matrix. Rook LDL^T is backward stable relative to ||A||, so the concern was that a
unit-scale block of an s-scaled KKT carries an error of u s. Measured on K = S K0 S (K0 well conditioned, S = diag(1,
1, s | s, 1): one variable at curvature s^2, one constraint row at scale s beside a unit row, the scaled variable
uncoupled and coupled to the unit block), s = 1e8, 1e10, 1e12, every case screened, raw-flagged and cleared, against
S^-1 K0^-1 S b in long double:

| solve | worst componentwise relative error, any block (6 cases) |
|---|---|
| raw rook factors, one solve | <= 8.9e-16 |
| raw rook factors + one refinement step | <= 2.9e-16 |
| D K D factored (sigma = 0) | <= 1.1e-15 |
| walk EQP (`solve_eqp`), dense | <= 2.9e-16 |
| whole walk, dense | <= 2.9e-16 (kOptimal) |
| platform session (Pardiso, kNever), coupled fixture | 1.1e-1 at s = 1e8, above 1e18 at s >= 1e10; the walk ends non-optimal |

The raw factors lose nothing: with a fixed pivot order LDL^T's rounding is invariant under diagonal scaling (the
factors of S K0 S are S times K0's to rounding), and rook pivoting picked a stable order on every case. Refactoring
as D K D when the re-estimate clears would buy no accuracy and cost one more n^3/3 factorization per cleared matrix
(33 IPM + 8 SQP TRAIN cells carry such matrices), so it is NOT taken; no TRAIN cost measurement is needed for a
change not made. Pinned by `QpDenseArm.TheRawFactorsOfAScaleClearedKktAreAccurateInEveryBlock` (1e-14 per component,
raw solve, EQP and walk). The comparison also shows the sparse arm is the one that loses the scaled coupled
fixture (registered below, not this event's).

On the elastic fixture the equilibrated rcond is 5e-17 (the dependent pair is singular whatever its scaling): it stays
flagged and shifted. The fixtures that do clear are the scale-only ones (section 4).

## 3. The gates (fix 2)

Every dense solve gate reads the normwise backward error (Rigal-Gaches; Higham, *Accuracy and Stability of
Numerical Algorithms*, 2nd ed., Thm 7.1), inf-norms:

    eta(y) = ||b - K y|| / (||K|| ||y|| + ||b||)

the smallest relative perturbation of K and b that y solves exactly.

| gate | system | was | now |
|---|---|---|---|
| shifted (walk EQP, SSN step, eliminated twin; `shifted_solve_refused`) | (D K D)(D^-1 y) = D b | ||D r|| / ||D b|| <= 1e-8 | eta in D's metric <= 1e-8 |
| shifted K0, bordered (`bordered_shifted_backward_error`) | K_b in D' = diag(D, 1) | max of two block ratios, each against its own rhs | one eta of the whole bordered system in D' <= 1e-8 |
| unshifted (SSN step; `unshifted_dense_solve_refused`) | K y = b, measured as (D K D)(D^-1 y) = D b, D = Ruiz(K) | ||r|| / ||b||: refine above 1e-8, refuse above 1e-8 | eta in D's metric: refine above 1e-8, refuse above 1e-8 |
| unshifted, IPQP step solves | the Ruiz-scaled K the tier factors (its own D; the check's D when `ruiz_equilibration` is off) | rho: refine above 1e-8, refuse above 1e-5 | eta: refine and refuse above 1e-8 |

**The threshold, 1e-8, argued.** A backward-stable rook LDL^T solve of order n has eta <= p(n) u (1 + rho_n) (Higham
Thm 11.3; growth rho_n bounded for rook pivoting): at the dense orders (n <= 167) and modest growth, ~1e-14. Reaching
1e-8 with u = 2^-53 needs a growth factor above ~5e5 on a correct factorization: the signature of a broken factor,
not of rounding. 1e-8 is also the relative size of the platform session's own static pivot perturbation (Pardiso
iparm[9] = 8, which the SQP's factors leave at Pardiso's default), so a dense answer the gate passes is as close to K
as a perturbed-pivot platform answer the engine already steps on. For the shifted gate: an unrefined first-attempt
answer has eta <= sigma / ||D K D|| ~ 9e-10 (Ruiz makes ||D K D||inf >= ~1), inside the threshold; the retry's shift
(256 sigma ~ 2.4e-7) must be refined away first. That asymmetry is deliberate: the retry exists for a
matrix the first shift did not make decidable, so its answer is trusted only once refinement against K has
removed its larger shift, while the first attempt's answer is already inside the threshold. The IPQP's looser 1e-5 refusal existed because rho has an n eps kappa
floor on late interior systems; eta has none, so the tier takes the common threshold.

**What this changes in kind.** DN-1d's gate refused the answer of an inconsistent singular system (residual O(rhs),
multipliers ~1/sigma). Its backward error is ~sigma: it is the exact answer of a system within 1e-8 of K, the answer
Pardiso's perturbed pivots give, and the gate now passes it. The walk's own row-residual check (E4-4) still refuses an
answer whose ROWS miss (the O(1) inconsistent duplicated rows stay `residual_refused`); what is no longer refused is an
answer that meets its rows and carries large multipliers -- which is what the elastic tier's Farkas certificate is
built from. The near-inconsistent fixtures DN-1d wrote to exercise refusal now read accepted (section 4).

**The walk's terminal rule, as it now reads** (qp_engine.h section 5, E4-4 as amended by E4-27). An inconsistent
row set reaches the walk the way it does on the sparse arm: the shifted answer passes the backward-error gate, the
row-residual check refuses it, the suspect ladder escalates, and after the ladder is exhausted the walk takes a step
from the shifted factor's answer and the terminal site classifies -- sparse-arm parity. Only an answer the
backward-error gate refuses is still terminal (no verified step exists), so `dense_shifted_terminations` is now a
broken-factor counter: a correct shifted factorization does not reach it.

**The unshifted gate's metric** (C341 F1). The shifted gate already measures in D's metric; the unshifted check
(`dense_solve_check_refused` through `unshifted_dense_solve_refused`, and the IPQP when it does not equilibrate)
measured in the raw one, where a row at scale s carries ||K|| ||y|| (and ||b||) to s-size and a unit-scale row's
residual up to 1e-8 s passes: vacuous on exactly the rows an SQP constraint scale leaves at unit size. It now
measures eta of (D K D)(D^-1 y) = D b with D = `ruiz_symmetric_scale(K)`, always. A trigger (the cheap raw eta
passing with ||K|| ||y|| >> ||b||) was considered and not taken: the disparity can sit in either term of the
denominator (a scaled row's rhs is as large as its matrix entries), so no raw reading separates a vacuous pass from
a real one without D, and D costs at most 8 O(nnz) sweeps (one, on an already balanced K) against the O(n^3/3)
factorization each check follows -- at n = 167 and a dense KKT's nnz <= n(n+1)/2 ~ 1.4e4, ~1e5 flops against
~1.6e6, and the SSN tier (the only consumer on a default-equilibrated path) is not the SQP's default QP mode.
Pinned by `DenseSolveCheck.ARowScaleCannotHideAUnitRowMiss` (raw eta 1e-13 passes; D's metric reads ~1e-5).

**The SSN step's merit exemption** (C341 F4). An SSN step taken unguarded (`SsnSafeguards::kBare`, or the hinted
first step / the watchdog's relaxed steps) is not taken when it came from a shifted factor: the gate passes the
answer of a system within 1e-8 of K, and on a singular K that answer can carry multipliers of order 1/sigma, so such
a step always faces the Armijo test on the FB merit.

## 4. Movers (the declared event)

See `docs/notes/data/2026-11-m8-dense-gate/DECLARATION.md` for the full list and the criteria. In short: the unit
fixtures DN-1d / DN-3a / DN-3b wrote around the two old measurements are re-derived (each test states its old
reading): 15 in `tests/qp/test_qp_dense_arm.cpp`, 4 in `tests/drivers/test_dense_arm_engines.cpp`, 6 in
`tests/linear/test_symmetric_factor_dense_arm.cpp`, plus new tests for the equilibrated re-estimate, the estimator,
a wrong-numerics refusal, the scale-only HS90 row and an intrinsically singular SQP shift fixture.
`ElasticSurvivesALargeConstraintScale` returns to the default arm. Every other test (HS sweep, warm-start battery,
certified fallback including the T7 escape ternary, restoration, seed, excursion) reads as before, Release and
Debug.

## 5. Registered (not done here)

- (3) a platform retake when a shifted factorization's solves are refused: not needed by the corpus evidence
  (section 4 of the declaration); kept registered.
- (Done in C341.) The integration coverage of the refusal branches -- no natural fixture produces eta > 1e-8
  through a correct factorization -- is restored through `DenseSolveFaultInjector`, a dense-solve wound in the two
  Apache-2.0 adapters (docs/testing.md): walk EQP refusal -> suspect ladder -> terminal, border shifted-K0 ->
  eliminated fallback, SSN unshifted and shifted refusal -> proximal ladder, and the border verdict gate's placement
  (a contracting wound, which refinement heals, separates a gate on the first Schur solve from one after
  refinement). It replaced the IPQP's private wound.
- The IPM's own dense step guard (`kDenseResidualRatioSingular`, ||r||_2 / ||b||_2 > 1e-5) still reads rho, by
  ruling (C341): it is a singularity detector feeding the IPM's inertia ladder, not a factor-sanity gate.
- The platform session's answer on the coupled F3 fixture (section 2) at s >= 1e8: relative error 1e-1 .. 1e25 and a
  non-optimal walk where the dense arm is exact (Pardiso's scaling/matching with pivot perturbation 1e-8 on a
  1e24-spread matrix). The sparse arm's problem, not the dense gate's; registered.
- Apple/Accelerate: UNOBSERVED (the estimator runs through the shim's dsytrs_rook; no Mac run).
