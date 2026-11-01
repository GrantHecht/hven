# M7 W2 YKN -- YORKNET (SNOPT-only TRAIN cell): diagnosis, design, and a measured negative

Branch `ykn-yorknet` at m7 `91dd3f64`. One commit lands from this round, and it is not a YORKNET lever:
`03903d55`, the RST-I1 rider (below). No YORKNET capability is landed. Phases 1-2 were run, the
candidate levers were measured in the cell behind environment-gated scratch hooks, and none of the
combinations converts the cell. The diagnosis shows a chain of four separate obstacles. Removing the
first two takes the main loop from a best KKT residual of 50.4 to 6.7e-5, but the cell still does not
converge. The rest of this README makes that argument with counters.

## Provenance

| | |
|---|---|
| toolchain | clang 22.1.8 (Fedora 22.1.8-4.fc44), Release, LTO off (RETAKE16's lane build flags) |
| host | AMD Ryzen 7 5800X3D, 8 physical cores |
| date | 2026-10-22 |
| bench lane | a working-tree copy of hven-bench `c214ba1` (pin file at `91dd3f64`), built against an install of this tree; the lane carries scratch patches that do nothing unless their environment variables are set (trace sink, option overrides, a CUTEst Hessian print) |
| baseline | RETAKE17 (pin `91dd3f64`) was still running throughout this round. The baseline is therefore RETAKE16's rows plus a HEAD control on this lane. The source at `91dd3f64` equals the RST rider head `a5c29790` (only docs and data differ), and the HEAD control's YORKNET `default` row equals the RST record's rider-head row in every field that is not a time |
| cell conditions | CLAUDE.md section 7: one solver at a time, each cell its own hold of `/tmp/box-build.lock`, interleaved with RETAKE17's cells, `MKL_NUM_THREADS=1`, `taskset -c 2`, and a watchdog that signals only its own child. Wall-clock is informational |

## Files

* `target-traces.txt`: per-depth trace summaries for YORKNET under the HEAD control and the main lever
  combinations. It includes the closest-to-KKT rows, the HEAD control's post-restoration rows, and the QN
  label at HEAD.
* `nonfinite-hessian.txt`: every non-finite CUTEst Hessian slot along the HEAD trajectory, every QP step
  that lands a component exactly on 0, and every non-finite-step rollback.
* `qp-evidence.txt`: oracle readings (HiGHS phase-1 LP, Clarabel, dense spectra; scratch venv, never
  vendored) of the failing subproblems; the exact solve of the walk's final face; the terminal mu-free
  refine's jump; offline walk replays under dual_mu, uniform-shift and anti-cycling arms.
* `experiments.txt`: every in-cell experiment row (status, majors, f, wall, closest-to-KKT row), with the
  lever each row changed.
* `rst-i1.txt`: the rider's test, mutant and suites.

## Diagnosis, exact label (`default`)

HEAD: `numerical_error`/40, f = 14706.40, stationarity 50.4, feasibility 6.1e-6, 35 s. SNOPT's committed
row is `Optimal`/110 majors at f = 14617.54. IPOPT reaches f = 14239.97.

**1. The Hessian is infinite at exactly-zero pipe flows, and the network forces exact zeros.** Pipes 1-10
carry sgn(q)|q|^1.852. Their CUTEst second derivative is 1.577904·|q|^-0.148, which is +inf (or NaN)
at q = 0. Pump flows have a bound q(13,t) >= 0. When a pump sits on that bound (pump off), the linear
mass balances at nodes 5 and 13 (zero demand) force the free pipe flows q(1,t) and q(9,t) to exactly 0.
The QP's step lands p_j = -x_j exactly on a free variable that is not TR-pinned (`nonfinite-hessian.txt`,
`YKN_TR`). The gradient and the Jacobian stay finite there: 1.852|q|^0.852 equals 0. Only the
Hessian is lost. On the HEAD trajectory this is 11 non-finite-step rollbacks: 10 at depth 0 and 1 in
restoration, whose exact phase also reads non-finite curvature at obj_factor = 0. Before restoration,
5 rollbacks halve the radius from 64 to 2 (m12-m21). After restoration, 4 rollbacks halve it from 0.1
to 0.00625 (m30-m38). Then the m39 subproblem ends `max_iter` and the solve ends. The rollback
(R2-1) can never step onto a point with a pump off, and that may be where the solution is.

**2. With the Hessian substituted, the main loop's QPs fail on the dual regularization's footprint.**
Lever A (non-finite entries zeroed, or read at a perturbed point; the two give identical trajectories)
removes every rollback. The main loop then reaches h ~ 1e-3 by m21 and ends `numerical_error`/26 on
its QPs: m20-m21 are saddle-exit misses and m22-m25 are stagnation stops, after which the failure
budget runs out. All six failing QPs are feasible (phase-1 LP). Their H has 291 of 312 eigenvalues
zero, a largest eigenvalue of 1.5e5, and two at -6.3e-4. On the stagnation QPs the walk takes full
steps along a flat face (Z'HZ singular, 12-dimensional), with |p| falling from 0.09 to 0.015 at a ratio
of about 0.965 per minor, the objective constant, and the working-row residual constant at 2.41e-5.
That residual is the dual_mu·|lambda| footprint. The adaptive schedule
dual_mu = clamp(KKT_residual^1.5, 1e-12, 1e-8) uses the absolute residual (about 50 here), so dual_mu
stays at its ceiling of 1e-8. At that dual_mu the rows cannot be met: the residual holds at 2.41e-5
for 150 passes, and it is gone at dual_mu <= 1e-10. So the dual regularization dominates these rows'
coupling. The terminal mu-free refine moves the iterate by 3e5 to 1.6e8 on these singular faces and is
refused. **Re-solved at dual_mu <= 1e-10, all six solve `kOptimal` in 5-50 minors** (`qp-evidence.txt`).
Neither a uniform shift H + rel·hs·I, nor the interior-point tier as a fallback, nor any anti-cycling arm
(EXPAND, lexicographic) solves the stagnation QPs. Solving the walk's final face of qp13 exactly gives a KKT
point: rows met to 2.6e-10, stationarity 4.9e-10 against a scale of 306, wrong-signed pin prices no
larger than 7.2e-10, objective 7.1775. The walk re-solved at dual_mu <= 1e-10 certifies that same
objective (7.177484607). The stalled walk sits at 5.6316 because it is off the rows by the footprint.

**3. With both levers, the main loop gets close but does not converge.** Lever B re-solves a failed walk
QP once at a smaller dual_mu (ladder 1e-10, 1e-12, 0) from the same seed. It works for 141 of 222
retries in the cell. Combined with A, and with failed steps projected into the box so the ordinary
failure routing applies, the run lasts 190 majors and reaches **stationarity 6.7e-5 with feasibility
4.0e-6 at f = 14680.869** (m119). HEAD's best is 50.4. The run still ends `numerical_error`. Two things
remain:
(i) The stationarity read with each QP's multipliers jumps by up to 6 decades within a few majors
at feasibility ~1e-6: 75 at m111, 6.7e-5 at m119, 32.9 at m127. The solution is degenerate: where q = 0 the head-loss rows lose their q column,
so LICQ fails, and qp39 (m36 of the A + B run) has Ae of rank 255/256. A small dual_mu removes exactly the
stabilization that the stabilized-SQP literature prescribes for non-unique multipliers.
(ii) Convex, feasible QPs with a rank-deficient Ae defeat the walk at every dual_mu. Clarabel solves
qp39. The walk ends `kNumericalError` at dual_mu = 1e-8, 1e-10 and 1e-12, and `max_iter` at 0.
The point being approached (f = 14680.87) is also a worse local point than SNOPT's (14617.54).

**QN label (`sqp-qn`).** HEAD: `numerical_error`/1, 375 s. The m0 subproblem fails, the elastic ladder
runs seven rungs, and restoration starts from major 0 at h = 51,001. The phase takes 65 majors and 46.8k
minors, with 10 walk QPs ending `max_iter`, several at the 7,680-minor cap. It stalls at the phase's own
feasibility of 5.1e-4. Lever A cannot act there, because no Hessian is evaluated. Lever B does not help:
all 8 retries at dual_mu = 1e-12 end `max_iter` again, and the cell hits the 450 s cap. This label's
obstacle is the walk on the restoration phase's LP-like subproblems. Two of them were read by the oracle
(`qp-evidence.txt`, last section). Both are convex (515 of 824 eigenvalues zero, none negative) and
feasible, and Clarabel solves them. The walk churns: 1,742 adds against 1,716 drops on 524 pins, and
it reaches the 7,680-minor cap 2.4e-4 above f*. dual_mu = 1e-10 or 1e-12 changes nothing. This is
degenerate-LP pivoting, the class the CYC round left open (WLF-CYC / CYC-C1). It is not the
exact label's footprint mechanism.

## Design: what a conversion needs (published literature only)

* **A. A bounded model Hessian where the exact one is not finite.** TR-SQP global convergence requires
  only uniformly bounded model Hessians (Conn, Gould & Toint, *Trust-Region Methods*, SIAM 2000,
  assumption AM.4 and chapter 15; Fletcher, Leyffer & Toint, SIAM J. Optim. 13 (2002), filter-SQP;
  Nocedal & Wright 2006, ch. 18). When H is non-finite while g, J and c are finite, substitute bounded
  entries for the subproblem: zero, or the entries read at x + eps·e_j. Nothing else changes and no
  certificate is touched. It acts only when H is non-finite. It is cheap and sound, and it converts
  nothing alone: `numerical_error`/26 in place of /40.
* **B. Dual regularization sized to the face.** Stabilized SQP ties dual_mu to the optimality residual
  (Wright, Comput. Optim. Appl. 11 (1998); Hager, Comput. Optim. Appl. 12 (1999); Gill & Robinson,
  SIAM J. Optim. 23 (2013); for regularized KKT systems Friedlander & Orban, Math. Prog. Comp. 4 (2012)).
  hven's schedule follows that rule on the absolute residual. On YORKNET's scale (f ~ 1.5e4,
  |grad f| ~ 300) it keeps dual_mu at 1e-8, where the faces need <= 1e-10. A failure-scoped retry
  at the schedule's floor solves the QPs. Item (i) above shows why that is not enough: at a degenerate
  solution a small dual_mu gives erratic multipliers. B has to come with D.
* **C. The walk on faces with rank-deficient rows.** For linear dependence in the working set and
  degeneracy, see Gill, Murray, Saunders & Wright, "Inertia-controlling methods for general quadratic
  programming", SIAM Review 33 (1991), and Fletcher, "Resolving degeneracy in quadratic programming",
  Annals of OR 47 (1993). The cheapest driver-level step is to refresh the equality basis, which drops
  dependent rows, on a failure the routing today ends without one, before giving up.
* **D. A dual_mu schedule on a scaled residual.** This is the stabilized-SQP rule read in the problem's
  own units, for example residual / max(1, |grad f|_inf, |J'lambda|_inf) (Izmailov & Solodov,
  Math. Program. 2012, "Stabilized SQP revisited"; Gill, Kungurtsev & Robinson, Math. Program. 2017).
  It moves every cell's subproblems, so it needs its own round measured on the 46 cells.

Recommendation: one round for A + B + D together, measured on all 46 cells with YORKNET as the target,
and C as the next walk item if qp39's class remains. A and B were not landed here: neither converts
anything alone, and B alone is shown above to work against stabilization.

## RST-I1 (commit `03903d55`)

The regularized restart now also requires `!st.interrupted`. A consumer stop taken inside the first
restoration sub-solve, its switch row included, ends the solve `kInterrupted` with no regularized phase
after it. Test: `SqpLadderSeam.AStopOnTheSwitchRowEndsTheSolveWithoutTheRegularizedRestart`. Mutant
(conjunct removed): killed, with 3 depth-1 rows instead of 1. The bench lane attaches no callback, so
no cell can move. See `rst-i1.txt` for the suites.

## RST-N4 (recorded here; the RST record is frozen data)

`docs/notes/data/2026-10-m7-rst/README.md`, riders section: "M1 moves no cell: none of the 46
reaches an exhausted elastic tier inside a phase" should read "none of the **measured 38**". The 8
cells capped in both arms were not observed, so the claim covers only the cells that produced a row.
That file is not edited.

## Side finding

Under `qp_mode = kSsn` on YORKNET with lever A, `SsnEngine::solve` throws on a warm start whose
z(13) = -1.8e-26 prices an absent upper bound. A value at round-off level on an absent bound is
refused as a contract violation. kSsn is not a shipped label. This is registered for the SSN tier and
not acted on.
