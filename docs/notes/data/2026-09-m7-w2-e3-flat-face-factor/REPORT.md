# M7 W2 E3 — the flat-face factor: the regularization's centre on the EQP side

Settler preamble. The implementer delivered this text as its hand-back; it is reproduced verbatim
below except for this preamble. Base `c864b8a6` (sources `0fa59cdc`, NC2 + fix1); brief
`.superpowers/m7-w2-e3-brief.md`; settler adjudication E3-8–E3-11 in that brief §2. Decision: the
proximal arm measured here does NOT ship — no production change; arm on branch `e3` (worktree
`agent-a69f46e11796560b9`, commit `cce88386`), NOT mergeable.

---

# M7 W2 E3 — the flat-face factor: the regularization's centre on the EQP side

Implementer hand-back. Worktree `agent-a69f46e11796560b9`, branch `e3` on `c864b8a6` (sources
`0fa59cdc` = NC2 + fix1). Builds `.scratch/nc2-build-{debug,release}` (clang, `HVEN_FP_MODE=SAFER_FAST`,
incremental). Evidence `.scratch/e3/` (this round; `.scratch/nc2/` and `.scratch/nc2-fix1/` read-only,
read only to COPY tools and to compare). Every solver leg under `flock /tmp/box-build.lock` with the
`PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` markers, `set -o pipefail`,
`MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2`, one solver at a time.

SIGNOFF: M7-W2-E3-REPORT

## 0. THE ANSWER, IN ONE PARAGRAPH

E3-1's three measurements all came out, and two of them CORRECT the record. (a) The ordinary step on
a flat face ascends `f`, always, by a positive fraction of the absolute centring's own term: on
`Cs_STEENBRB` qp1/qp3/qp5 **every** ordinary pass of the exact cycle ascends, and over one period the
ordinary ascent EXACTLY cancels the rides' descent (qp1: `+6.694355e-05` against `-6.694355e-05`, net
`8.0e-19`). (b) The one refinement step contracts the EQP's own residual by 33x / 2.3x / 3.2x on those
three cells — fix1's inferred "at best 2x-3.5x" is right for qp3/qp5 and wrong for qp1, because
`stat_free_inf` (measured at the ITERATE) and the EQP answer's own residual are different objects.
(c) **The equality drift is NOT the regularized model, it is the FACTOR'S SOLVE**: the same system,
solved densely, lands `8.0e-08` off the rows — exactly `mu*|lambda_e|` with `|lambda_e| = 8.0`, not
`7e10` — and `3.6e-14` after one refinement step, while the engine's own answer at that pass leaves a
residual of `2.57e+02` against its OWN regularized system, concentrated entirely in the equality rows.
The PROXIMAL arm (E3-2), built and measured, then does something remarkable and something
disqualifying at once: it converts `Cs_STEENBRB` qp1/qp3/qp5/qp6 from `kMaxIter` to **kOptimal at the
Clarabel optimum**, kills the equality drift on every cell that had it (`eq_drift_violations`
7502 -> 0, 4798 -> 0, 5775 -> 0, 2875 -> 0, 2351 -> 0) and drops the terminal stationarity residual by
eight decades on the STEENBRB family — and it LOSES four certified `kOptimal` cells
(`ACOPP57_def` qp0-qp3, `kOptimal` in 2-8 minors -> `kMaxIter` at 1795) and produces a **NaN objective**
on two (`STEENBRB_def` qp2, qp3). By E3-4 it therefore **DOES NOT SHIP**. No production commit; the
arm is committed on branch `e3` labelled `arm(m7): W2 E3 ...` so the settler can read it.

## 1. E3-1 — THE THREE MEASUREMENTS (all OBSERVED, before any design)

Instrument: two SCRATCH patches, both revertible and both reverted before the branch's final state
was fixed — `.scratch/e3/tools/patch_e3_measure.py` (HVEN_TESTING-only walk-seam columns; no
production path touched) and `.scratch/e3/tools/patch_e3_arm.py` (the arm itself). The seam columns
are the ordinary step's `g'p` and `p'Hp` on EVERY pass, the centring term
`(delta/2)(||x_F||^2 - ||eqp.x_F||^2)`, the same three for the PRE-refinement answer
(`unrefined_out`, plumbed through the seam), and the EQP solve's own residuals against BOTH its own
regularized system and the unregularized one, split into the free block (`rho`) and the constraint
block. Harness: the scratch gtest `tests/qp/test_qp_e3_measure.cpp` (`E3.MeasureOrdinaryStep`), run
by `.scratch/e3/ord.sh`; per-pass CSVs under `.scratch/e3/probes/`.

### 1a. THE ORDINARY STEP ASCENDS BY THE CENTRING TERM — every pass of the cycle

`Cs_STEENBRB` qp1/qp3/qp5, Release, `border` and `refactorize` summaries IDENTICAL, tail = the last
400 records. `delta = 1e-7` on 2127 of qp1's 2247 ordinary passes (the SUSPECT LADDER escalated once;
the centring column reproduces `0.5*delta*(x_free_sq - eqp_free_sq)` to the last digit, which is how
that was read off rather than assumed).

| cell | ordinary passes | ASCENDING | tail ordinary | tail ASCENDING | tail median `g'p` | tail median centre | `g'p`/centre |
|---|---|---|---|---|---|---|---|
| qp1 | 2247 | **2156** | 133 | **133** | +2.407076e-05 | +7.487349e-05 | 0.321 |
| qp3 | 1361 | **1299** | 80 | **80** | +1.405018e-06 | +1.094359e-05 | 0.406 |
| qp5 | 2230 | **2151** | 133 | **133** | +2.445891e-07 | +2.113773e-06 | 0.116 |

So the ordinary step's slope is POSITIVE and is a positive fraction (0.12 … 0.41) of the absolute
centring's own term — the mechanism the brief's §0 named, confirmed with numbers, on every pass in the
cycle and on three cells. `alpha == 1` on those passes (the FULL step to the EQP point), so this is
the EQP point itself being uphill, not a ratio-test artifact.

**THE CYCLE'S ENERGY LEDGER** (`.scratch/e3/logs/e31a-cycle-ledger.log`), qp1's last period-6 block,
`df` computed per action from the recorded slope, sign and `alpha` (`H == 0`, so `f(x+alpha d) - f(x)`
is exactly `alpha*g'd` — a proof, not an estimate):

| minor | action | alpha | `g'p` | centre | df |
|---|---|---|---|---|---|
| 6594 | moveOnly | 1 | +4.28728e-05 | +2.16026e-04 | **+4.28728e-05** |
| 6595 | dropDantzig | — | 0 (p = 0) | 0 | 0 |
| 6596 | rideStepped | 1.58253 | −3.84691e-05 | — | **−6.08785e-05** |
| 6597 | moveOnly | 1 | +2.40708e-05 | +7.48735e-05 | **+2.40708e-05** |
| 6598 | dropDantzig | — | 0 (p = 0) | 0 | 0 |
| 6599 | rideStepped | 3.65067 | −1.66135e-06 | — | **−6.06504e-06** |

moveOnly total **+6.694355e-05**, rideStepped total **−6.694355e-05**, NET **+8.0e-19**. The cycle is
EXACTLY closed in `f`, and the two halves are the ordinary steps' ascent and the rides' descent. qp5
(period 6): +4.136997e-07 against −4.136997e-07, net 1.6e-22. qp3 (period 10, 2 rides + 2 moveOnly +
4 drops + 2 rideUndone): +2.628749e-06 against −2.628749e-06, net 0 exactly.

### 1b. THE TRUE CONTRACTION OF THE ONE REFINEMENT STEP — and a correction to fix1

Measured as `||rho_ref||inf / ||rho_unref||inf` where `rho` is the FREE BLOCK of the EQP's own
unregularized residual at the pre- and post-refinement answer (the same `residual_of` the code uses),
median over the tail's ordinary passes:

| cell | `||rho_unref||inf` | `||rho_ref||inf` | contraction | full-residual contraction |
|---|---|---|---|---|
| qp1 (delta = 1e-7) | 1.280000e-05 | 3.871790e-07 | **3.02e-02** | 3.02e-02 |
| qp3 | 5.492737e-07 | 2.361017e-07 | **4.31e-01** | 4.31e-01 |
| qp5 | 2.449720e-07 | 7.540540e-08 | **3.08e-01** | 3.08e-01 |

**A CORRECTION, stated plainly.** fix1 §B.1 inferred the contraction as "at best 2x-3.5x" from
`||rho||inf / (delta*||x||inf)` being 0.28 … 0.47. That ratio is right for qp3 and qp5 and WRONG for
qp1, where the step contracts 33x. The two quantities are different objects: fix1's `rho` is
`stat_free_inf` — the stationarity residual at the ITERATE `x` with `price`'s multipliers, i.e. one
`alpha`-step away from the EQP point — while the contraction above is the EQP ANSWER's own residual.
`||rho_unref||inf = delta*||y_free||inf` exactly (qp1: 1e-7 * 128), so the starting point of the
contraction is not `delta*||x||inf` either. fix1's conclusion ("rho IS the primal regularization's own
term") is unaffected; only the contraction arithmetic is.

### 1c. THE DRIFT IS THE FACTOR'S SOLVE, NOT THE REGULARIZED MODEL

Two independent measurements, both OBSERVED, and they agree.

**OFFLINE** (`.scratch/e3/tools/eqp3.py` — eqp.py extended with the row-residual, contraction and
proximal columns — on `.scratch/nc2/yk3_it177.state`, log `logs/e31c-yk3-it177.log`). The dense
re-solve of the SAME bound-eliminated regularized system (nf = 273, me = 256, nw = 0, delta = mu = 1e-8):

| answer | `|Ae x - be|inf` | `|x - x_k|inf` | `|lambda_e|inf` | `||rho||inf` |
|---|---|---|---|---|---|
| regularized (exact) | **8.000000e-08** | 7.633950e-07 | **8.0** | 5.000001e-09 |
| + one refinement step | **3.577485e-14** | 1.676068e-08 | 8.0 | 7.549517e-15 |

and `|Ae x - be|inf / (mu*|lambda_e|inf) = 1.0000` — the row residual of the EXACT regularized answer
IS `mu*|lambda_e|`, to four digits, with `|lambda_e| = 8.0`. The brief's alternative hypothesis needed
`|lambda_e| ~ 7e10`; it is 8. The ENGINE's own `eqp.x` at that pass is `7.108589e+02` off the rows and
`2.351447e+01` from `x_k`, i.e. **23.5 away from an answer that is 7.6e-07 away**. Conditioning:
`cond(K) = 1.086e+11`, `cond(K0) = 3.938e+17`, `min|eig(K)| = 1.0e-08`; the face has nullity 17 with
`||Z'HZ||inf = 7.5e-28` — the free block along the face IS `delta*I`.

**IN-ENGINE** (`logs/e31c-inengine-yk.log`, the `res_reg_unref` column = `||rhs - K y0||inf`, the
factor's residual against its OWN regularized system). A0_YORKNET qp3, minors 174-180:

| minor | action | `p_inf` | `||rhs - K y0||inf` | `rho_unref` | `rho_ref` | row resid (ref) |
|---|---|---|---|---|---|---|
| 174 | moveOnly | 4.95e-09 | 1.14e-13 | 5.00e-09 | 1.46e-13 | 4.89e-14 |
| 176 | rideStepped | 1.01e+08 | 4.25e-03 | 5.06e-01 | 5.06e-01 | 7.03e-10 |
| **177** | **add** | 2.35e+01 | **2.565e+02** | 2.31e-07 | 3.20e-07 | **7.109e+02** |
| 178 | add | 1.03e-02 | 1.14e-13 | 5.10e-09 | 1.46e-13 | 4.89e-14 |

5053 of qp3's 7680 passes leave `||rhs - K y0||inf > 1e-4`; on qp4 the worst is 1.34e+14. At minor 177
the error is **entirely in the constraint rows** (free block 2.3e-07 against rows 7.109e+02), which is
why the one refinement step — which reduces the residual by a factor `||K^-1 diag(reg)||` on a system
the factor cannot solve — does not repair it.

**ANSWER: the regularized system's exact answer does NOT miss the equality rows by 1/delta-sized
amounts. It misses them by `mu*|lambda_e|` = 8e-08, and by 3.6e-14 after refinement. The 7.1e+02 the
walk is handed is an inaccurate SOLVE of a system with `cond(K) = 1.1e11` whose free block along a
17-dimensional flat face is `delta*I`.** M12's routing of the repair to "the regularized KKT solve"
is correct; its parenthetical ("the dual regularization on the equality block") is not the mechanism.

## 2. THE ARM AS IMPLEMENTED (E3-2) — the exact system, and the four modes

`include/hven/detail/qp/e3_prox_arm.h` (new, scratch) + the four rhs derivations and the two
refinement targets it is wired into. Env `E3_PROX`, read ONCE per `QpEngine::run` through a
`CentreGuard` that sets the thread-local centre to that walk's own `x` and restores it on scope exit
(so a nested or concurrent walk cannot inherit another's centre).

**The system solved, mode 2 (the candidate).** `K` is BIT-IDENTICAL to the shipped assembly — the
proximal term is a rhs change only, because the stationarity row of
`min (1/2)x'Hx + g'x + (delta/2)||x_F - x_k,F||^2  s.t. Ae x = be, Aw x = bw, x_P pinned` is
`(H_FF + delta I) x_F + Ae_F' le + Aw_F' lw = -g_F - hess_shift + delta*x_k,F`. So, per pass:

    rhs = [ -g_F - hess_shift + delta*x_k,F ; be - eq_shift ; bw - ineq_shift ]     (K unchanged)
    refinement: r = (rhs - K y) + diag(reg) y   with reg = [ 0 on the free block ; -mu on the rows ]

i.e. the primal `delta` is now part of the MODEL (a proximal term) and only the DUAL `mu` is an error
to back out. The classical form of this is the proximal-point / primal-dual regularization of
Friedlander & Orban 2012 (*A primal-dual regularized interior-point method for convex quadratic
programs*) and the regularized-SQP treatment of Gill & Wong 2015 — papers only, no source read.

**The four modes**, so the two halves are separable (the two-line table is the whole switch):

| mode | rhs | refinement target |
|---|---|---|
| 0 (OFF, shipped) | centred at 0 | fully unregularized (delta and mu both backed out) |
| 1 | PROXIMAL | fully unregularized |
| 2 (**the candidate**) | PROXIMAL | proximal (delta kept, mu backed out) |
| 3 (control) | centred at 0 | proximal |

**Sites wired** (all four rhs derivations the walk can reach, and both refinement targets):
`solve_eqp` (`eqp_solve.h`); `build_bordered_system` + `bordered_regularization` (`bordered_eqp.h`,
which the border twin's verdict-site refinement shares); the VERDICT-SITE eliminated re-derivation in
`src/qp/qp_engine.cpp` (whose own note says "the two must stay in step"). On the border path the
proximal term is added on ALL `n` variable rows unmasked, which equals the eliminated path's masked
form because a pinned variable satisfies `x_i == x_k,i` exactly (the pin border holds it there) so
`delta*x_k,i - delta*x_i` cancels; the cross-algebra corpus is the check, and it passes (§3).

**MODE 0 IS INERT — the control.** The E2-ON corpus at `E3_PROX=0`, all four (algebra, list) legs, is
**BYTE-IDENTICAL** to fix1's own `fix1-*` rows (`.scratch/nc2-fix1/corpus/`), so neither scratch patch
perturbs the shipped behaviour. That is the premise every mode-2 number below is a difference against.

**DOES THE CHANGE REACH THE OPTION-OFF PATH? YES, AND IT IS DECLARED AS SUCH.** The primal `delta`
is applied by `assemble_kkt_core` on every EQP solve regardless of `independent_working_set`, so the
centring change is a change to **EVERY QP SOLVE** — E2 on, E2 off, IPM-side and SQP-side alike. There
is no option to hide it behind. The E2-OFF corpus is measured in §3 for exactly that reason.

## 3. THE BATTERY (E3-3) — every mover declared

### 3a. The 51-dump corpus, E2 ON, both algebras, all columns

`rowcmp` base(mode 0) -> m2, `.scratch/e3/logs/` and the tables below. **49 of 51 rows change on each
algebra; 16 (border) / 17 (refactorize) change STATUS.** Cross-algebra agreement is preserved: the set
of rows that differ between `border` and `refactorize` under m2 is of the same character and size as
under base (§3c).

**THE WINS.** The whole `Cs_STEENBRB` restoration family lands ON the Clarabel optimum, four of them
converting `kMaxIter` -> `kOptimal`, and the terminal stationarity residual falls eight decades:

| cell | Clarabel `f*` | base `f` (status, minors) | m2 `f` (status, minors) | base `f-f*` | m2 `f-f*` | base `stat` -> m2 |
|---|---|---|---|---|---|---|
| qp1 | −1378.22319848 | −1378.22272466 (cap, 6600) | **−1378.22320088 (opt, 232)** | +4.74e−04 | −2.40e−06 | 3.93e−07 -> 1.08e−16 |
| qp2 | −192.000444927 | −192.000437989 (opt, 252) | −192.000445485 (opt, 560) | +6.94e−06 | −5.58e−07 | 7.62e−08 -> 1.10e−16 |
| qp3 | −384.000315772 | −384.000271723 (cap, 6600) | **−384.00031719 (opt, 2915)** | +4.40e−05 | −1.42e−06 | 1.53e−07 -> 1.10e−16 |
| qp4 | −192.000158557 | −192.000146263 (opt, 246) | −192.000158958 (opt, 1536) | +1.23e−05 | −4.01e−07 | 5.84e−08 -> 1.10e−16 |
| qp5 | −192.000037036 | −192.000031467 (cap, 6600) | **−192.000037958 (opt, 5147)** | +5.57e−06 | −9.22e−07 | 7.54e−08 -> 1.09e−16 |
| qp6 | −192.000015027 | −192.000007685 (cap, 6600) | **−192.000015819 (opt, 6211)** | +7.34e−06 | −7.92e−07 | 5.00e−08 -> 1.10e−16 |

(The m2 objective sits a few 1e−07 BELOW `f*` on every cell; the walk's own feasibility residual there
is 3e−15 … 3e−14 and Clarabel's is 6.7e−14, so the two points agree inside both tolerances. These are
NOT "better than optimal" claims — they are the same optimum.)

**THE DRIFT IS GONE.** `eq_drift_violations` and the terminal `|Ae x - be|`:

| cell | base `eqviol` -> m2 | base `eq` -> m2 `eq` |
|---|---|---|
| A0_YORKNET/qp3 | 7502 -> **0** | 2.50e−01 -> **3.71e−14** |
| A0_YORKNET/qp4 | 2351 -> **0** | 5.17e+00 -> **2.28e−14** |
| A0_YORKNET/qp6 | 4798 -> **0** | 7.81e−03 -> **1.33e−14** |
| A0_YORKNET/qp2 | 2875 -> **0** | 5.68e−14 -> 1.02e−13 |
| A0_YORKNET/qp1 | 7 -> **0** | 1.56e−13 -> 1.50e−13 |
| A0_YORKNET/qp5 | 2 -> **0** | 2.10e−14 -> 1.95e−14 |
| SAWPATH_def/qp1 | 5775 -> **0** | 1.42e−14 -> 9.09e−13 |
| STEENBRB_def/qp3 | 765 -> **1636** | 8.11e+02 -> **nan** |

Six of the nine drifting cells go to zero violations; `STEENBRB_def/qp3` gets WORSE and is one of the
two NaN cells (below). Two further status wins: `Hj_STEENBRD/qp4` `num -> opt` (same `f`, `stat`
5.76e−01 -> 3.85e−17) and `YORKNET_def/qp1` `num -> opt` with `eq` 3.59e+03 -> 3.68e−13 (base's point
was not feasible at all, so its lower `f` is not a comparison).

**THE DISQUALIFYING MOVERS.** These are the reason the arm does not ship.

| cell | base -> m2 | what happens | reading |
|---|---|---|---|
| ACOPP57_def/qp0..qp3 (×4, BOTH algebras) | **opt -> cap** | `kOptimal` in 2-8 minors becomes `kMaxIter` at 1795, with `f`, `eq` and `stat` unchanged to every printed digit | **FOUR CERTIFIED CELLS LOST.** The point is the same; the walk can no longer RECOGNISE it. A cell that used to terminate in 2 minors now burns its whole cap. |
| STEENBRB_def/qp2 | cap -> num, **`f = -nan`** | 4680 -> 559 minors, `eq = nan` | a NaN objective introduced by the arm |
| STEENBRB_def/qp3 | num -> num, **`f = -nan`** | 1185 -> 2963 minors, `eqviol` 765 -> 1636 | the second NaN, and the only drift cell that gets worse |
| SAWPATH_def/qp1 | cap -> cap | `f` 176.47 -> **1269.19**, `stat` 3.84e−14 -> 3.41e−02 | a far worse incumbent at the cap |
| A0_YORKNET/qp1 | num -> cap | 777 -> 7680 minors, `f` −21156.2 -> −20855.4 | a worse incumbent, and the full cap burned |
| A0_YORKNET/qp2, qp5, qp6 | cap -> cap | `f` −10828 -> −10433, −3013 -> −2894, −1519 -> −1469 | worse incumbents at the cap (the rows are met now, which the base's were too on qp2/qp5) |
| ACOPR57_def/qp6 | cap -> num | 7100 -> 240 minors | an honest early failure replacing a cap; no point either way |
| DALLASM_def/qp1,qp2,qp3; DALLASM_f4/qp2,qp3 | num<->cap both ways | `eq ~ 1.5` on every one of them in BOTH arms | the DALLASM family never reaches feasibility in either arm; these flips are churn on an already-failing cell, not a regression |

### 3b. AN INDEPENDENT DENSE DEMONSTRATION, and why E3-4's FALLBACK is also unavailable

`.scratch/e3/logs/e31-cs1-it216.log` — `tools/eqp3.py` on `.scratch/nc2/cs1_it216.state`
(`Cs_STEENBRB` qp1, nf = 107, me = 96, nw = 0, `delta = 1e-7` — fixed by the state, since the dense
`||rho||inf = 1.28e-05` reproduces the engine's own `rho_unref` exactly). On this face,
`rank(C) = 96`, **nullity 11**, `||Z'HZ||inf = 0` EXACTLY:

| system solved | `g'p` | centring term | `|Ae x - be|inf` |
|---|---|---|---|
| ABSOLUTE centring, EXACT dense answer | **+2.707485e-04** | +2.706688e-03 | 1.24e−12 (= `mu*|lam|`, ratio 1.0027) |
| ABSOLUTE + one refinement step | **+2.556732e-04** | +2.684075e-03 | 3.84e−13 |
| PROXIMAL centring, EXACT dense answer | **−1.507531e-05** | −2.933615e-04 | 1.19e−12 |
| PROXIMAL + one refinement step | **−3.159744e-04** | −9.316122e-03 | 3.84e−13 |

So the ascent is a property of the SYSTEM, not of the solve: an EXACT dense answer to the
absolutely-centred regularized EQP on this flat face ascends by 0.100 of its own centring term, and the
proximally-centred one descends. CAVEAT, stated: the working set this state reconstructs is not the
engine's at that exact pass (the engine's `|eqp.x - x|inf` there is 1.4e−14, the dense answer's is
64.3), so this is a demonstration on a genuine flat face OF THIS QP and the in-engine numbers of §1a
are the observation about the engine's own passes. The two agree in sign and order.

**AND THE FALLBACK DIES HERE.** E3-4's fallback is "the delta-free re-price of the multipliers at the
drop-deciding pass on flat faces only". On this face the unregularized system is **EXACTLY SINGULAR**:
`cond(K0) = inf`, `min|eig(K0)| = 0`, inertia `(101, 101)` against the expected `(107, 96)`. On the
YORKNET face of §1c it is numerically singular the same way (`cond(K0) = 3.94e+17`,
`min|eig| = 3.35e−15`, inertia `(264, 265)` against `(273, 256)`). A delta-free re-price needs a
factorization of exactly that matrix, so **on a flat face there is nothing to factorize** — and fix1
already showed the cheap form of it (the pass's own regularized factor) removes only 3 % … 43 %. The
fallback is refused on evidence, not on cost; its cost was never reached.

### 3c. WHY THE ARM LOSES `ACOPP57` — the mechanism, and what would have to change with it

INFERRED from the arithmetic, then MEASURED (§3d). Under the shipped scheme the refinement targets the
UNREGULARIZED system, whose answer at a KKT point of the face is `p = 0` to rounding, so
`||p||inf <= step_tol = feas_tol*max(1,||x||inf)` fires and the loop enters its KKT branch, prices, finds
nothing to drop and exits `kOptimal`. Under PROXIMAL centring the answer is the proximal minimiser,
which on a face with `x_k` on its rows is exactly

    p = -(1/delta) * P_Z (Hx_k + g)          (P_Z the orthogonal projector onto null(C))

so `||p||` is the reduced gradient AMPLIFIED BY `1/delta = 1e8`. At a point whose reduced gradient sits
at the rounding level of `||g||`, `||p|| ~ 1e-16*||g||/1e-8 = 1e-8*||g||`, which is ABOVE
`step_tol ~ 1e-9`. The KKT-point test's effective threshold becomes `||P_Z g|| <= delta*step_tol ~ 1e-17`
— unreachable in double precision. **The arm's fixed point is right (`rho = -delta*p -> 0`) and its
CONVERGENCE TEST is not**: `||p|| <= step_tol` is the wrong currency once `p` carries a `1/delta`.

That is a repairable defect, and repairing it is a change to the walk's termination test, which this
round's scope excludes. It is the first registered item (§6).

### 3d. The 51-dump corpus, E2 OFF — the option-off path moves too, and the DRIFT GETS WORSE there

`baseoff` (mode 0, `--e2 0`) against `m2off` (mode 2, `--e2 0`), border, all columns
(`.scratch/e3/logs/e3-corpus-e2off.log`). **46 of 51 rows move; 7 change status.** Control: the
`baseoff` rows reproduce fix1's `fix1off-*` rows byte for byte on the 24 rows checked before the file
was complete, and `baseoff` differs from `m2off` on 46 rows — which is the direct measurement that the
centring change is NOT behind the E2 option.

| direction | cells |
|---|---|
| WON `cap -> opt` | `Cs_STEENBRB` qp3, qp4, qp5, qp6 (all four to `f*`: −384.00031719, −192.000158958, −192.000037958, −192.000015819); `Hj_STEENBRD/qp4` (4680 -> 190 minors, `stat` 4.10e−04 -> 4.40e−17) |
| **LOST `opt -> cap`** | **`A0_YORKNET/qp6`: 941 minors `kOptimal` with `eq = 4.55e−15` becomes 7680 minors `kMaxIter` with `eq = 6.25e−02`** |
| LOST `cap -> num` | `Hj_STEENBRD/qp1` (4680 -> 554 minors, `stat` 1.16e−02 -> 5.76e−01) |
| **DRIFT MADE WORSE** | `A0_YORKNET` qp3 `eq` 4.54e−14 -> **5.00e−01**; qp4 2.32e−02 -> **2.50e−01**; qp5 1.74e−14 -> **1.25e−01**; qp6 4.55e−15 -> **6.25e−02** |

That last row is decisive against E3-4's fifth criterion ("the drift dumps do not get worse"): on the
E2-ON path the arm REMOVES the drift on six cells, and on the E2-OFF path it CREATES it on four. The
`stat` column improves almost everywhere in both arms (e.g. `DALLASM_def/qp1` 7.55e−04 -> 5.64e−17,
`READING6_def/qp0` 7.47e−09 -> 7.74e−17), which is the arm's fixed point being genuinely better — the
failures are not about the fixed point.

### 3e. THE MECHANISM OF BOTH REGRESSIONS, MEASURED (not inferred)

`.scratch/e3/logs/e3-nontermination-mechanism.log`. `A0_YORKNET/qp3`, border, last 300 passes:

| | mode 0 (shipped) | mode 2 (the arm) |
|---|---|---|
| action histogram | moveOnly 50, dropDantzig 50, rideStepped 50, add 150 | **moveOnly 300** |
| `p_inf` median (min … max) | 1.03e−02 (0 … 6.11e+06) | 2.76e−09 (1.21e−09 … 3.67e−09) |
| `step_tol` | 1.00e−09 | 1.00e−09 |
| passes with `p_inf <= step_tol` | 50 of 300 | **0 of 300** |
| `p_inf/step_tol` median | 1.03e+07 | **2.76** |
| `g'p` at the last pass | +2.30e−01 | **+5.72e−17** |
| `rho_ref` / row residual | 1.46e−13 / 7.11e+02 | 1.62e−15 / 4.89e−14 |

The arm's tail is 300 consecutive full-step (`alpha = 1`) ordinary passes at a point whose true reduced
gradient is 1e−17, whose rows are met to 5e−14 and whose stationarity residual is 1.6e−15 — **a
converged point the loop cannot recognise**, because `p = -(1/delta) P_Z g` turns a 1e−17 reduced
gradient into a 2.8e−09 step and `step_tol = feas_tol*max(1,||x||inf) = 1e−09`. `ACOPP57_def/qp0` is the
same picture with nothing else happening at all: mode 0 exits `kOptimal` at 2 (refactorize) / 8 (border)
minors with **0 adds and 0 drops**; mode 2 runs 1795 minors, still with **0 adds and 0 drops**, `f`
unchanged in its first 10 digits. The churn also collapses under the arm — `A0_YORKNET/qp3`
`ws_adds` 5165 -> 158, `ws_drops` 5029 -> 21, `ws_exchanges` 3771 -> 18, `eq_drift_violations`
7502 -> 0 — which is the arm working; it simply cannot stop.

**THE NaN, OBSERVED** (`logs/e3-nan-onset.log`). `STEENBRB_def/qp2` under mode 2 blows up at minor 0:
`rho_ref = 5.59e+13` and `res_reg_unref = 9.31e+10` on the first solve, `p_inf = 6.81e+38` and
`g'p = 2.04e+42` by minor 3, `max p_inf = 6.84e+97`. `x_inf` never leaves 1.45e−13 (`alpha` is NaN, so
nothing moves) and the engine exits `kNumericalError` at 559 — an HONEST failure, but with `f = -nan`.
`0.1/delta = 1e+07` is the unbounded-artifact scale, and a `p` of 1e+38 is 31 decades past it: the
runaway guard is written on `|x_i|`, and `x` never moves, so it never fires. This is the `1/delta`
amplification again, on a problem whose `|g|` is O(1e12).

**A PRE-EXISTING FINDING, NOT THE ARM'S.** `QpWalkAction::kUnset` — which `qp_walk_trace.h` calls "a
BUG in the instrumentation" that "the fixtures assert against rather than tolerating" — occurs on
3 of 4680 BASE (mode 0) passes of `STEENBRB_def/qp2` and 1 of 283 base passes of `Hj_STEENBRD/qp4`.
47 per-pass captures were checked; those are the only ones. Registered (§6).

### 3f. The descent invariant the arm actually delivers (and does not)

`.scratch/e3/logs/e3-descent-invariant-by-mode.log`, `e3-descent-invariant-cs-all.log`,
`e3-descent-vs-rowresidual.log`. The theoretical statement only binds where `x_k` is ON the working
rows (then `p` lies in the face's nullspace), so the table is restricted to ordinary passes with
`active_res_inf <= 1e-6`:

| cell (border) | mode 0: rows-met ordinary | mode 0 ASCENDING | mode 0 worst `df` | mode 2: rows-met | mode 2 ASCENDING | mode 2 worst `df` |
|---|---|---|---|---|---|---|
| qp1 | 2156 | **2142** | +2.569e−04 | 20 | **0** | 0 |
| qp3 | 1361 | **1299** | +1.546e−05 | 2784 | 119 | +7.133e−06 |
| qp5 | 2230 | **2151** | +2.251e−06 | 5052 | 29 | +2.645e−06 |
| qp2 | (opt at base, no cycle) | — | — | 441 | 14 | +2.309e−05 |
| qp4 | — | — | — | 1328 | 7 | +3.766e−09 |
| qp6 | — | — | — | 6088 | 15 | +5.542e−07 |

So the arm takes the ascending fraction from **99.3 % / 95.4 % / 96.5 %** down to **0 % / 4.3 % / 0.6 %**
and never produces a cycle — but it does NOT deliver an exact invariant. Tightening the row threshold
to 1e−13 removes the large residual ascents (qp2 2.31e−05 -> 2.05e−06, qp3 7.13e−06 -> 8.29e−10,
qp6 5.54e−07 -> 1.18e−07) and not all of them, because the proximal step's RANGE component is amplified
by `1/delta` as well: a row residual of `r` contributes a step of order `r/delta`, so the guarantee
needs `active_res <~ delta*tol`, not `<~ tol`. A shipped DESCENT-INVARIANT fixture would therefore need
a tolerance rather than an exact sign test — and since the arm does not ship, it is not built (§5).

### 3g. THE ATTRIBUTION CORPUS — and the round's most valuable finding

The four modes on all 51 dumps, both algebras, E2 on (`.scratch/e3/logs/e3-corpus-attribution-all.log`).
The two halves of the arm turn out to do DIFFERENT, COMPLEMENTARY things, and the CONTROL is the best
of the three:

| mode | what it changes | `kOptimal` WON | LOST | NaN `f` | rows moved |
|---|---|---|---|---|---|
| m1 | proximal rhs only | 6 | **5** (`ACOPP57_def` qp0-qp3 border, `Cs_STEENBRB/qp4`) | 0 | 48/51 |
| m2 | proximal rhs + proximal refinement | 6 | **4** (`ACOPP57_def` qp0-qp3) | **2** | 49/51 |
| **m3** | **proximal refinement ONLY** (the shipped centring at 0, `delta` kept in the refinement target, only `mu` backed out — a ONE-LINE change) | **11** (border) / 9 (refactorize) | **2** (`Cs_STEENBRB` qp2, qp4) | 1 (`STEENBRB_def/qp3`) | 49/51 |

**m3's wins, border:** `A0_YORKNET` **qp1, qp2, qp3, qp4, qp5 AND qp6 ALL become `kOptimal`** (778-866
minors, from `kMaxIter` at 7680 or `kNumericalError`), with the drift gone and a BETTER objective on
every one of the six:

| cell | base status/minors | m3 status/minors | base `eq` -> m3 | base `eqviol` -> m3 | base `f` -> m3 `f` |
|---|---|---|---|---|---|
| qp1 | num / 777 | **opt / 819** | 1.56e−13 -> 1.56e−13 | 7 -> 3 | −21156.1781754 -> −21156.1786123 |
| qp2 | cap / 7680 | **opt / 778** | 5.68e−14 -> 8.53e−14 | 2875 -> 8 | −10828.4159537 -> −10857.1018889 |
| qp3 | cap / 7680 | **opt / 838** | **2.50e−01 -> 8.86e−14** | 7502 -> 14 | −10552.2812457 -> −10933.5674026 |
| qp4 | cap / 7680 | **opt / 842** | **5.17e+00 -> 3.83e−14** | 2351 -> 5 | −5838.62938949 -> −5840.56376515 |
| qp5 | cap / 7680 | **opt / 822** | 2.10e−14 -> 2.10e−14 | 2 -> 24 | −3013.37677544 -> −3013.72053087 |
| qp6 | cap / 7680 | **opt / 866** | **7.81e−03 -> 4.52e−15** | 4798 -> 11 | −1518.80924226 -> −1530.21964864 |

plus `Hj_STEENBRD` qp1 and qp4 -> `kOptimal`, `STEENBRB_def/qp1` -> `kOptimal`, `YORKNET_def/qp1` ->
`kOptimal`, and `Cs_STEENBRB/qp1` -> `kOptimal` at 326 minors.

**THE ATTRIBUTION IS CLEAN, and it is the answer the settler needs:**
- The **PROXIMAL RHS** is what fixes the FLAT-FACE ordinary-step ascent — only m1/m2 convert the
  `Cs_STEENBRB` cycle family (qp3/qp5/qp6 `cap -> opt`) — **and it is what breaks termination**:
  `ACOPP57_def` qp0-qp3 go `opt -> cap` under m1 (border) and m2 (both algebras) and are **UNTOUCHED
  under m3** (still 8 / 2 minors, `kOptimal`).
- The **PROXIMAL REFINEMENT TARGET** is what fixes the DRIFT/CHURN family — only m3 (and m2, partially)
  touches `A0_YORKNET`, and m3 takes all six to `kOptimal` — and it costs `Cs_STEENBRB` qp2/qp4
  (`opt -> cap`, because without the proximal rhs the flat-face ascent is still there and the extra
  accuracy just lets the cycle run longer) plus the `STEENBRB_def/qp3` NaN.

**AND m3 FAILS E3-4 TOO, for a reason that justifies the candidate's design** (§3l): two certified
corpus cells lost, one NaN, **75 Release-suite failures against the arm's 52**, and — decisively —
**U0's terminal `kkt_stationarity` gets WORSE by four to five decades** (`f7_n10000_bound_activity`
1.016559106e−15 -> 9.999902125e−12; `_corrupted` 9.189172314e−14 -> 1.207195544e−09; `_neutral`
6.300386629e−13 -> 4.999693639e−09), 56 walk differences all of them residual-class and all of them
worse. That is exactly what keeping `delta` in the model WITHOUT the proximal centring must do: the
answer then carries a permanent `-delta*x_F` stationarity residual that refinement no longer removes,
so every KKT-residual assertion in the suite degrades by `O(delta*||x||)`.

**THE TWO HALVES BELONG TOGETHER, and that is the round's design conclusion.** The proximal CENTRING is
what makes keeping `delta` in the refinement target harmless: at a converged point `p -> 0`, so the
residual `-delta*p -> 0` and the fixed point is exact — which is why the CANDIDATE (m2) IMPROVES U0's
residuals by orders of magnitude (§3j) where the control (m3) degrades them. Neither half alone is a
lever; the pair is, and the pair's single blocker is the termination test (§6 item 1).

### 3h. THE SUITES — Release, mode 0 (control) against mode 2 (the arm)

`.scratch/e3/logs/ctest-rel-m0.log`, `ctest-rel-m2.log`, `ctest -j4` under `setsid`,
`MKL_NUM_THREADS=1`, inside the lock.

| | tests | failed | reading |
|---|---|---|---|
| **mode 0 (control)** | 2863 | **1** | the ONE failure is `E3.MeasureOrdinaryStep`, the round's own scratch harness, which `ASSERT_NE`s on an env var ctest does not set. 2863 = fix1's 2862 + that one test. **The scratch patches are inert: nothing else moves.** |
| **mode 2 (the arm)** | 2863 | **52** | 51 real failures |

The 51, grouped by what they say (full list in the log):

- **THE RUNAWAY / UNBOUNDED GUARD BREAKS — this answers E3-2's explicit question by measurement.**
  `QpEngine.UnboundedProblemIsReportedAsNumericalErrorNotOptimal`,
  `QpEngine.RunawayWithActiveRowClearsItsMultipliers`,
  `QpEngine.EffectivelyInfiniteBoundsStillDetectTheRunaway`. **The absolute centring's bound IS needed
  somewhere, and the somewhere is the unbounded-artifact detection** — exactly the reason
  `qp_types.h` ~:99 gives for it. A per-step bound relative to `x_k` is NOT enough: the iterate can
  walk out, or (the NaN cells) `p` can blow past `0.1/delta` while `x` never moves at all, and the guard
  is written on `|x_i|`.
- **CROSS-ALGEBRA EQUIVALENCE BREAKS.** `QpEngineBorder.BorderModeMatchesRefactorizeMode`,
  `QpEngineBorder.RebuildUnderTinySchurCapMatchesRefactorize`.
- **HS PINS MOVE, and not only as counter pins.** `HsSweepCorpus.Corpus`,
  `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions`. (E3-4's criterion (4) is
  therefore also failed; phase 6 reads them per-mode.)
- **fix1's OWN FIXTURE FAILS.** `QpNc2FlatFace.TheSteenbrbRestorationLpQp3NeverAscendsTheSlope{Border,
  Refactorize}` — the arm changes the walk that fixture pins.
- **THE T7 CENSUS ITSELF PASSES**: `QpEngineStructuralViolation.T7TheFeasibleCensusHalfOverTheFULLDualMuSet`
  is NOT in the failure list at mode 2, i.e. **120/120 holds under the arm**. Its sibling
  `T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras` fails.
- The rest: `QpE2WorkingSet` (6), `B1Gate` (4), `QpEngineIndefinite` (6), `QpDegeneracyArm` (1),
  `QpEngineTr` (1), `SqpDriver*` (15), `WarmStartBattery` (2), `MeshTransfer`, `PredictorRatioTest`,
  `ScaleF7`, `F7ColdScaleSmoke`, `SqpEqualityBasis`, `Continuation.ProbeBudgetBoundsAFailingProposal`.

### 3i. THE T7 CENSUS AND THE HS PINS, per mode (`logs/census-hs-m{0,2,3}.log`)

16 tests: `QpEngineStructuralViolation.T7*` (the 120/120 feasible census over the full `dual_mu` set and
its certified-infeasible sibling), `HsSweepCorpus.Corpus`, `HsBattery.*`. Release, `hven_sqp_tests`.

| mode | result |
|---|---|
| 0 (control) | **16/16 PASSED** — the scratch patches move nothing |
| 2 (the arm) | **13 passed, 3 FAILED**: `HsSweepCorpus.Corpus`, `HsBattery.CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions`, `QpEngineStructuralViolation.T7ACertifiedInfeasibleExitRETURNSTheRefinedPointInBothAlgebras`. **`T7TheFeasibleCensusHalfOverTheFULLDualMuSet` PASSES — the 120/120 census holds under the arm.** The HS pins DO move, and as a corpus-level failure rather than a counter pin with attribution, so E3-4's criterion (4) fails. |
| **3 (the control arm)** | **16/16 PASSED** — the census AND every HS pin are INTACT under the one-line refinement-target change |

### 3j. U0, 27 cells x 75 columns x 3 engines, against the M6-close pin (gate 1e-5)

`logs/phase4.log`, `u0/comparison.txt`.

| mode | walk | ssn | ipm |
|---|---|---|---|
| 0 (control) | **0 differences** (7 within gate) | **0** | **0** |
| 2 (the arm) | **37 differences** (69 within gate) | **0** | **0** |

**Every one of the 37 is explained, and the explanation is the same one:** 25 of them are `qp_minors`
going 2 -> 3 or 2 -> 4 — one or two EXTRA minors per subproblem, which is precisely the non-terminating
tail of §3e seen at its smallest; the remaining 12 are residual-class columns that get strictly BETTER
by orders of magnitude (`kkt_stationarity` 6.300386629e−13 -> 3.754407364e−20 on
`f7_n10000_bound_neutral`, 1.259925399e−12 -> 4.975325948e−20 on `f7_n20000_bound_neutral`;
`kkt_primal` 1.938683814e−13 -> 2.519882995e−16). No status, count-of-majors or verdict column moves.
So E3-4's criterion (3) — "no UNEXPLAINED mover" — is met; the movers are real and accounted for.
`ssn` and `ipm` are untouched, which is the expected signature of an EQP-layer change (the SSN and
interior tiers do not go through `solve_eqp`).

### 3k. The Debug suites

`logs/ctest-dbg-m{0,2}.log`, `ctest -j4` under `setsid`, the > 600 s test
`Continuation.ProbeBudgetBoundsAFailingProposal` EXCLUDED per E3-6 (so 2862, not 2863).

| | tests | failed | wall |
|---|---|---|---|
| mode 2 (the arm) | 2862 | **50** | 629.66 s |
| mode 0 (control) | 2862 | **1** | 682.02 s — the ONE failure is `E3.MeasureOrdinaryStep` again. **The control is clean in BOTH configurations.** |

The Debug and Release failure sets agree on **46 of 50 / 52**. Debug-only: the two
`QpNc2FlatFace.TheSteenbrbRestorationLpQp4NeverAscendsTheSlope{Border,Refactorize}` (Debug-gated
fixtures), `SqpDriverRestoration.AlgebraModeDivergenceIsANonUniqueSubproblemOptimum`,
`SqpDriverRestorationExcursion.TheSubStatusDiscriminatesAtOneParentStatus`. Release-only:
`Continuation.ProbeBudgetBoundsAFailingProposal` (excluded in Debug by the rule), three more
`QpE2WorkingSet.ADeclinedRideIsExchangedOnSteenbrbRestorationQp{4,5,6}`,
`QpE2WorkingSet.AStructuralDependencyDecidesFirstOnTheSteenbrbElasticQp`,
`SqpDriverQpFailure.RepeatedQpMaxIterStillPropagates`. The two configurations therefore tell the same
story and neither is a Debug-assert story: no Eigen assert fires under the arm.

### 3l. The CONTROL mode measured like the candidate (mode 3, Release suite + U0)

`logs/ctest-rel-m3.log`, `u0m3/comparison.txt`.

| | mode 0 | mode 2 (candidate) | mode 3 (control) |
|---|---|---|---|
| Release suite failures / 2863 | **1** (the scratch harness) | 52 | **75** |
| T7 census + HS (16 tests) | 16/16 | 13/16 | **16/16** |
| U0 walk differences / (27x75) | **0** | 37, residuals strictly BETTER | **56, residuals strictly WORSE** |
| corpus `kOptimal` won / lost / NaN | — | 6 / 4 / 2 | 11 / 2 / 1 |

Mode 3's 75 failures include `QpEngine.OracleBattery`, `IpqpDispatch.KIpmAndKWalkAgreeOnTheAnswerAcrossTheHsBattery`,
nine `Continuation.*`, four `Predictor*`, six `QpEngineIndefinite.*` and the four
`QpNc2FlatFace.TheSteenbrbRestorationLpQp{2,4}NeverAscendsTheSlope*` — i.e. the KKT-accuracy tests,
which is the signature of the `O(delta)` bias above.

## 4. THE DECISION (E3-4), criterion by criterion, with the numbers

| E3-4 criterion | verdict | the numbers |
|---|---|---|
| (1) the descent property holds by measurement on the Cs cycle — no ascending ordinary step; Cs qp1 converges or exits honestly | **PASS, with a qualification** | the cycle is GONE on all six Cs cells and qp1 reaches `kOptimal` at the Clarabel optimum in 232 minors; on rows-met ordinary passes the ascending fraction falls from 99.3 % to 0 % on qp1, but 0.6 %-4.3 % of such passes still ascend on qp2-qp6 (worst `df` +2.3e−05), so the INVARIANT is not exact (§3f) |
| (2) the census does not lose a certified cell | **FAIL on the corpus, PASS on the census itself** | `T7TheFeasibleCensusHalfOverTheFULLDualMuSet` PASSES (120/120 holds, §3i); the CORPUS loses FOUR certified cells (`ACOPP57_def` qp0-qp3, `opt -> cap`, both algebras) on the E2-ON path and ONE (`A0_YORKNET/qp6`, `opt -> cap`) on the E2-OFF path (§3a, §3d) |
| (3) U0 has no unexplained mover | **PASS** | 37 walk differences, every one explained: 25 are `qp_minors` +1/+2 (the non-terminating tail at its smallest), 12 are residual columns getting strictly BETTER; `ssn` and `ipm` 0 differences (§3j) |
| (4) HS pins move only as counter pins with attribution | **FAIL** | `HsSweepCorpus.Corpus` and `HsBattery.CrashBasisIsANullResult...` FAIL outright at mode 2 (16/16 pass at mode 0 and at mode 3) — a corpus-level failure, not a counter pin (§3i) |
| (5) the drift dumps do not get worse | **FAIL on the option-off path** | E2 ON: `eq_drift_violations` 7502/4798/5775/2875/2351/7 -> 0 on six cells and the terminal `|Ae x - be|` 2.50e−01 -> 3.71e−14, 5.17e+00 -> 2.28e−14, 7.81e−03 -> 1.33e−14. E2 OFF: `A0_YORKNET` qp3 4.54e−14 -> **5.00e−01**, qp4 2.32e−02 -> **2.50e−01**, qp5 1.74e−14 -> **1.25e−01**, qp6 4.55e−15 -> **6.25e−02** |
| (unlisted, and decisive on its own) | **FAIL** | `STEENBRB_def` qp2 and qp3 return `f = -nan` under the arm, from a `p_inf` reaching 6.84e+97 (§3e) |
| (unlisted, and also decisive) | **FAIL** | the full suites: **52 of 2863 Release** and **50 of 2862 Debug** against the control's 1 (§3h, §3k), including the three runaway/unbounded-detection tests and both cross-algebra equivalence tests |

**THE ARM DOES NOT SHIP.** No production commit. The mechanism it implements is right — the arm's
fixed point is strictly better everywhere it is measured (`stat` falling from 1e−04 … 1e−07 to 1e−16 …
1e−19 across the corpus, the whole `Cs_STEENBRB` family landing on the Clarabel optimum, the equality
drift removed on six cells) — and the IMPLEMENTATION is disqualified by two defects that are both the
same defect: `p` now carries a factor `1/delta`, and two things in the walk are written in the wrong
currency for that. The KKT-point test `||p||inf <= feas_tol*max(1,||x||inf)` becomes unreachable
(§3e: 300 consecutive full ordinary steps at `p_inf/step_tol = 2.8` around a point whose reduced
gradient is 1e−17), and the runaway guard `|x_i| > 0.1/delta` never sees a `p` of 1e+38 because `x`
never moves. Both are changes to the WALK's termination and safeguard tests, which this round's scope
excludes (E3-4: "neither the WIP FACE CHECK nor any change to `ride_sign`/`drop_worst` is in scope";
the termination test is the same class of change).

**AND THE FALLBACK IS NOT AVAILABLE EITHER** (§3b): a `delta`-free re-price needs a factorization of the
unregularized system, which on a flat face is EXACTLY singular (`cond(K0) = inf`, inertia (101, 101)
against the expected (107, 96) on `Cs_STEENBRB` qp1's face; `cond = 3.94e+17`, (264, 265) against
(273, 256) on `A0_YORKNET` qp3's). Its cost was never reached because the object it would factorize does
not exist. What fix1 already measured — the same step with the pass's own REGULARIZED factor — removes
3 % … 43 % (§1b), which is the status quo.

## 5. FIXTURES (E3-5) — NOT BUILT, and why, item by item

E3-5 is conditional on the arm shipping ("Fixtures (if the arm ships)"). It does not, so none are
added; the tree's committed test suite is unchanged. For the record, what each one would have needed:

- **The 3-4-variable flat-face LP where the absolute centring ascends and the proximal descends.** The
  demonstration exists and is stronger than a synthetic would be: `Cs_STEENBRB` qp1's own face, in
  dense arithmetic, `g'p = +2.707e−04` absolute against `−1.507e−05` proximal (§3b). A synthetic LP is
  writable; it would only be a fixture if the proximal side were production.
- **The ordinary-step DESCENT INVARIANT on the walk seam, both algebras, both configurations.** Cannot
  be asserted as an exact sign test even under the arm: 0.6 % … 4.3 % of rows-met ordinary passes still
  ascend, because the range component is `1/delta`-amplified too (§3f). It would need a tolerance of
  the form `g'p <= C*delta*||p||^2` — a real invariant, but one whose constant has to be measured
  first, and it is not this round's to pin.
- **`R2_Cs_STEENBRB_qp1` as `tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp1.txt`.** The E3-5 precondition
  ("if it converges: `kOptimal` at the Clarabel oracle") is met ONLY under the arm. Under the shipped
  code it is `kMaxIter` at 6600 minors, 4.74e−04 above `f*`. Adding the dump as a fixture asserting the
  base behaviour would pin the cycle, which is not what E3-5 asked for. NOT added.
- **`A0_YORKNET` qp3's drift bound.** Under the shipped code the terminal `|Ae x - be|` is 2.50e−01 with
  `eq_drift_violations = 7502`; a fixture asserting "≤ the structural tolerance" would fail, and one
  asserting 2.50e−01 would pin a bug. NOT added. The drift's CAUSE is now identified (§1c), which is
  what a future fixture should be written against.

## 6. REGISTERED ITEMS

1. **THE TERMINATION TEST IS IN THE WRONG CURRENCY FOR A PROXIMALLY-CENTRED EQP** (the blocker for this
   whole direction). `||p||inf <= feas_tol*max(1,||x||inf)` cannot recognise a converged point once
   `p = -(1/delta) P_Z g`. A proximal EQP needs its KKT-point test on `delta*||p||` or directly on the
   free-block stationarity residual (which the seam already computes as `stat_free_inf`), and its
   runaway guard on `||p||` as well as on `|x_i|`. Measured cost of NOT doing it: four certified cells
   on the E2-ON path, one on the E2-OFF path, two NaN objectives. **This is the item to schedule before
   the proximal centring is retried** — and with it, the arm is the strongest single lever measured in
   W2 (the whole `Cs_STEENBRB` family to the Clarabel optimum, the equality drift to zero).
2. **THE EQUALITY DRIFT IS A LINEAR-SOLVE DEFECT, ready to be attacked directly.** `||rhs - K y0||inf`
   reaches 2.57e+02 (YORKNET qp3 minor 177) and 1.34e+14 (qp4 minor 159) against the pass's OWN
   regularized system, on `cond(K) ~ 1e11` faces whose free block along an O(10)-dimensional nullspace
   is `delta*I`. Candidates, all cheap and none in this round's scope: (a) a residual CHECK at the EQP
   site that refuses an answer whose own `||rhs - K y||` exceeds a tolerance and re-solves or retreats
   (the walk-side form of this is the review's "a step whose `Ae*sdir` exceeds tolerance is refusable
   before it is taken"); (b) iterating the refinement on the REGULARIZED system rather than once on the
   unregularized one; (c) scaling. The diagnostic column that finds it is one mat-vec.
3. **`QpWalkAction::kUnset` occurs on the BASE walk.** 3 of 4680 passes on `STEENBRB_def/qp2` and 1 of
   283 on `Hj_STEENBRD/qp4`, mode 0, Release. `qp_walk_trace.h` calls this "a BUG in the
   instrumentation" that "the fixtures assert against rather than tolerating"; no existing fixture
   covers those two cells. Either the claim needs qualifying or the loop has an exit that claims no
   action.
4. **fix1 §B.1's contraction arithmetic** (the "at best 2x-3.5x" inference) should be read as a
   statement about `stat_free_inf` at the iterate, not about the EQP answer's own residual, which
   contracts 33x on `Cs_STEENBRB` qp1 (§1b). No conclusion of fix1 changes.
5. **The `unrefined_out` plumbing is worth keeping** as a seam column if the EQP layer is revisited: the
   pre/post-refinement residual pair, split into free block and rows, is what made §1b and §1c
   decidable, and it costs two mat-vecs under `HVEN_TESTING`.

## 7. WHAT I COULD NOT DETERMINE

- **Whether a proximal centring with a repaired termination test keeps the wins.** The four Cs
  conversions and the drift removal are measured with the CURRENT test, so they survive the cap; the
  `ACOPP57` and NaN cells cannot be re-measured without the out-of-scope change.
- **Whether the two NaN cells are the `1/delta` amplification alone or also a solve failure.** The first
  solve of `STEENBRB_def/qp2` already has `res_reg_unref = 9.31e+10`, i.e. the factor fails on that
  system at pass 0 under BOTH arms; the arm's rhs then multiplies the consequence. Separating them needs
  a dense re-solve of that pass, which needs a state dump the round did not take.
- **The border/eliminated equivalence of the arm at machine precision.** The two algebras agree on the
  corpus to the same extent they do at base (the mode-2 status/`f` columns are identical across
  algebras on 47 of 51 rows, and the four that differ — `DALLASM`, `ACOPP57` — differ at base too), but
  I did not run the dedicated cross-mode equivalence batteries with the arm ON beyond what the suites
  cover, and the unmasked border rhs (§2) is justified by an argument plus the corpus, not by a
  per-variable check.
- **Whether `mode 1` (proximal rhs, unregularized refinement) is a better candidate than `mode 2`.** It
  also converges Cs qp1 to `f*` in 199 minors and produces NO NaN on the corpus, but its refinement step
  blows up on a flat face (`max rho_ref = 3.55e+41`, `max ||rhs - Ky||/||rhs||inf = 6.01e+39`) because it
  targets a system that is INCONSISTENT there, and it loses `Cs_STEENBRB/qp4` on top of the four
  `ACOPP57` cells. I did not run its suites or its U0 — the attribution corpus (§3g) was enough to place
  it, and the box time went to measuring the candidate and the control properly instead. If the
  termination test is repaired, m1 deserves the same battery as m2.

## 8. THE RE-TAKE 9 PREDICTION, PER CELL

**Nothing ships, so the prediction is the trivial one and it is worth stating precisely: EVERY TRAIN
cell's walk is BYTE-IDENTICAL, 0 conversions, and the on-row identity premise is available for ALL 23
cells** — because the tree's production sources at the branch head are `c864b8a6`'s, unchanged. The
brief's expectation ("with the centring change reaching every E2-on solve, the on-row identity premise
is likely available for NO cell") was correct FOR THE ARM and is moot for what ships.

If a future round DOES ship the pair, this round's corpus is the prediction to write it against, and it
says the premise would be available for no cell: 49 of 51 dumps move on the E2-ON path and 46 of 51 on
the E2-OFF path, with `A0_YORKNET` qp1-qp6, `Cs_STEENBRB` qp1-qp6 and `Hj_STEENBRD` qp1/qp4/qp9 all
moving — and those three families are the TRAIN set's own restoration carriers. The five optimal cells
would need a fresh per-cell measurement, not a carried list (the standing rule).

**ONE CAVEAT ON THE BRANCH ITSELF.** The arm commit (§9) adds
`include/hven/detail/qp/e3_prox_arm.h` and wires it into `eqp_solve.h`, `bordered_eqp.h` and
`src/qp/qp_engine.cpp`, and that wiring is compiled into the LIBRARY (it reads `getenv`). With
`E3_PROX` unset the behaviour is byte-identical — the corpus control proves it on 4 x 51 rows and both
suites prove it at 1 expected failure each — but the OBJECT CODE is not, so **the commit is not
mergeable and must not be treated as a base for a P-SYM count.** It exists so the settler can read the
code the numbers came from.

## 9. PROCESS, AND WHAT IS ON THE BRANCH

- Worktree `/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9`, branch `e3` on
  `c864b8a6`. The main tree `/home/ghecht/Projects/hven` was never touched (its only new file is this
  report, under the git-ignored `.superpowers/`).
- Every solver leg ran through `.scratch/e3/leg.sh`: `flock /tmp/box-build.lock` with NO timeout and
  never under `timeout`, an in-lock `pgrep -a -x hven_bench_cell` emitting `PGREP_EMPTY`, then
  `FOREGROUND_START` / `FOREGROUND_END <rc>`; `set -o pipefail` throughout;
  `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2` on every solver cell; ONE solver at a time; `ctest
  -j4` under `setsid`; the > 600 s Debug test `Continuation.ProbeBudgetBoundsAFailingProposal` EXCLUDED
  from both Debug suite runs. Seven legs: `e3-corpus-base`, `e3-phase2`, `e3-phase3`, `e3-phase4`,
  `e3-phase5`, `e3-phase6`, `e3-phase7`, plus the short build/probe legs. No process this round did not
  start was ever signalled. The dense `eqp3.py` runs are offline numpy (OMP/MKL threads 1) and assert no
  wall-clock; they are declared as co-running with a lock-held leg on two occasions.
- IPARM surface untouched. SNOPT firewall: nothing read. References: Friedlander & Orban 2012 and
  Gill & Wong 2015 (papers only, cited for the proximal/primal-dual regularization form).
- No correctness fixture was re-pinned; no fixture was added or removed; `notices/` untouched.
- **THE COMMIT**: one commit on branch `e3`, message labelled `arm(m7): W2 E3 ...`, NO attribution
  trailer of any kind, explicit pathspecs. Files:
  `include/hven/detail/qp/e3_prox_arm.h` (new), `include/hven/detail/qp/eqp_solve.h`,
  `include/hven/detail/kkt/bordered_eqp.h`, `include/hven/detail/qp/qp_walk_trace.h`,
  `src/qp/qp_engine.cpp`, `tests/qp/test_qp_e3_measure.cpp` (new), `tests/CMakeLists.txt`.

## 10. EVIDENCE INDEX (`.scratch/e3/`)

| what | where |
|---|---|
| E3-1(a) per-pass and per-action CSVs | `probes/base/*.{ord,all}.csv`, `probes/m2/`, `probes/arm/`, `probes/diag/` |
| the cycle's energy ledger | `logs/e31a-cycle-ledger.log` |
| E3-1(b)/(c) offline dense | `logs/e31c-yk3-it177.log`, `logs/e31-cs1-it216.log`, `tools/eqp3.py` |
| E3-1(c) in-engine | `logs/e31c-inengine-yk.log` |
| the non-termination mechanism | `logs/e3-nontermination-mechanism.log`, `logs/e3-nan-onset.log` |
| the descent invariant | `logs/e3-descent-invariant-by-mode.log`, `-cs-all.log`, `logs/e3-descent-vs-rowresidual.log` |
| the corpus, 4 modes x 2 algebras x E2 on/off | `corpus/{base,baseoff,m1,m2,m2off,m3}-*.txt`, `logs/e3-corpus-{e2off,attribution-all,m1-attribution}.log` |
| census + HS per mode | `logs/census-hs-m{0,2,3}.log` |
| suites | `logs/ctest-{rel,dbg}-m{0,2,3}.log` |
| U0 | `logs/phase4.log`, `u0/comparison.txt`, `u0m3/comparison.txt`, `logs/u0-m3.log` |
| the runners | `leg.sh`, `ord.sh`, `corpus.sh`, `build.sh`, `phase{2..7}.sh`, `g4.sh`, `g4m3.sh`, `chain{1..5}.sh` |
| the two scratch patches | `tools/patch_e3_measure.py`, `tools/patch_e3_arm.py` |
| running state | `STATE.md` |
