# M7 W2 SCW -- column scaling on by default (SCL-W1)

> Branch `scw-scaling-default`, base `8965d973` (m7). Commits: `03f14cba` (YKN-S1), `59939b29` (SCW:
> the default flip, the coupling fix, the trace rider, SCL-T2), and this record.
> Provenance: clang 22.1.8, Release (LTO off for the lane; the library's own flags for the suites),
> AMD Ryzen 7 5800X3D, 2026-10-22, measured tree == `59939b29` (byte-identical to the WIP object
> `1809ef84` every leg below ran on). Every leg ran under `flock /tmp/box-build.lock`, one solver at a
> time, `MKL_NUM_THREADS=1`, `taskset -c 2`, with `pgrep -a hven_bench_cell` empty before it and inside
> the lock. Walls are informational.

## Result

`SqpOptions::column_scaling` default `kOff` -> `kAuto`, `column_scaling_threshold` default
`1e3` -> `2^26` (= 1/sqrt(machine epsilon)). A declared options event: two dump values move
(`column_scaling = kAuto`, `column_scaling_threshold = 67108864`).

| measurement | result |
|---|---|
| Release suite | 3199 / 3199 (1 skipped, 4 disabled, as before) |
| Debug suite | 3199 / 3199 (7 skipped, 4 disabled), long test included |
| U0 replay, exact, vs `2026-10-m7-rst-gates/gates/replay` | walk 0, ssn 0, ipm 0 differences over 27 cells x 75 columns (`u0-candidate/`) |
| 46 TRAIN cells, harness label `kAuto` (RETAKE17 options) vs RETAKE17 measured rows | 37 IDENTICAL (every key) + 9 capped in both; 0 moved, 0 lost (`cells-label/vs-retake17.md`) |
| 46 TRAIN cells, label override REMOVED (library default) vs RETAKE17 | 37 IDENTICAL + 9 capped in both (`cells-nolabel/vs-retake17.md`) |

The two cell arms run byte-identical options (every dump in both arms is one file per label;
`cells-label/{default,sqp-qn}.dump`), because the new library default IS the label. Against RETAKE17's
declared dumps they differ in exactly one line, `column_scaling_threshold = 1000` -> `67108864`.
Only SAWPATH (both labels) fires the gate (spread 6.87e10); every other finished cell's spread is
<= 512 (`cells-nolabel/column-rule.txt`). READING8 (spread 32768, fired at 1e3 in RETAKE17) no longer
fires; it is capped at 300 s in both, so its change is unobserved.

**No declared value movers** in U0 or the 46 cells. The movers are the options dump (above), the
`sqp.solve.end` line (two keys, below) and the suite tests listed under Tests.

## Phase 1: the diagnosis (scratch copies of the base with environment hooks; not committed code)

kAuto @ 1e3 as the default reproduced the predecessor: U0 walk 98 / ssn 137 / ipm 293 differences,
one status loss, `f7_n1000_path_warm` Optimal -> NumericalError (`u0-kauto-1e3/`); the Release suite
30 failures (`phase1/kauto-1e3-default-suite-failures.txt`, 3193 tests at the base).

`f7_n1000_path_warm`, walk engine (`phase1/pathwarm-*.csv`; status, factorizations, minors, QPs):

| arm | n1000 | n800 |
|---|---|---|
| kOff | Optimal 2 / 12 / 2 | Optimal 2 / 6 / 2 |
| kAlways (columns only) | Optimal 2 / 12 / 2 | Optimal 2 / 6 / 2 |
| kAuto @1e3 (columns + row layer) | NumericalError 8 / 28 / 7 | Optimal 3 / 8 / 3 |
| kAlways + enable_scaling | NumericalError 8 / 28 / 7 | Optimal 3 / 8 / 3 |
| kOff + enable_scaling (rows only) | NumericalError 114 / 634 / 6 | NumericalError 6 / 14 / 6 |
| kAuto @1e3 with a one-sided objective factor | Optimal 2 / 12 / 2 | Optimal 2 / 6 / 2 |

The cause is the ROW layer's TWO-SIDED objective factor. F7's start gradient inf-norm is 0.05..1.95,
so `scaling_max_gradient / ||g||` amplified the objective 51..1940x. The scaled
stationarity test is that much stricter than the caller's `kkt_tol`. The predecessor's inference, that
the row layer's refusal to take warm-start state is the cause, is REFUTED. The column layer drops the
same state under kAlways, and kAlways keeps the cell at 2 factorizations.

SAWPATH (`phase1/sawpath-*.txt`, lane, label `default`): the row layer's objective factor there is a
SHRINK (gradient 1.9e6 -> obj 5.1e-5), so the one-sided rule keeps it Optimal in 9 majors, 1 s. A
relative Jacobian-entry floor (sqrt(eps) x row max) was tried against the warm-point defect below. It
cut SAWPATH's spread from 6.9e10 to 1.3e8 and the cell then hit its 120 s cap. Rejected.

The 30 suite failures, classified by reading each assertion:
* 18 contract changes. 8 are exact model-call counts that the rule's one Jacobian probe moves by +1.
  4 are default-value asserts. 6 are factor or "unscaled" premises.
* 8 genuine. 6 have one cause: the rule re-read the Jacobian at a CONVERGED warm point. There a
  variable at about 0 nearly zeroes its own column (HS7), the gate fired, and the warm re-solve lost its
  0-major exit or its kOptimal (3 tests). One false 0-major kOptimal was certified at caller-unit
  stationarity 3.0. The other 2: the SSN prox-carry lever was dead on fired F7 hops, and one shipped
  kSsn F7 row cost 87x the factorizations.
* 4 premises that need re-pinning.

At 1e3, a one-sided objective plus the floor (`u0-onesided-floor-1e3/`) had no U0 status loss.
Every F7 cell still fired, though, and the kIpm arm paid for it: `f7_n1000_path_warm` went from 44 to
67 factorizations, and the bound cells from 8 to 12. Firing on a mesh bought nothing.

## Phase 2: the design (published sources only)

1. **The engaged row layer never amplifies the objective.** kAuto's engagement uses IPOPT's
   one-sided gradient rule `min(1, g_max / ||grad f(x0)||_inf)` (Waechter & Biegler, Math. Prog.
   106 (2006), s3.8). `enable_scaling` set by the caller keeps the two-sided rule, unchanged.
   `ScalingRule::amplify_objective`.
2. **The gate at 2^26 = 1/sqrt(eps).** Past this spread, the smaller columns' contributions keep
   fewer than half of a double's digits. The column rule is Ruiz's inf-norm equilibration (Ruiz,
   RAL-TR-2001-034), with power-of-two factors as in LAPACK's xGEEQUB. On a transcription, a control
   column enters every defect row as O(h), so its Ruiz factor grows like N (F7: 2^11..2^15 for
   N = 1e3..2e4). This is genuine Ruiz imbalance but not a defect the SQP needs repaired. 2^26 leaves
   such meshes unscaled up to about 4e7 nodes. This is how SCL-G1 is answered: the gate is not
   size-invariant in principle, but it sits beyond any realistic mesh.
3. **The decision travels with the warm start** (`SqpWarmStart::column_factors`). Every
   kAlways/kAuto solve records d, or all ones when it left the variables unscaled. A warm start that
   the ingest gate admits is solved under that decision, with no probe. A chain therefore keeps its
   cold root's variables and never re-reads the rule at a converged point. kOff and a kCold ceiling
   ignore the record. A record of the wrong size, or that is not all positive powers of two, is
   ignored.
4. **A caller-unit stationarity ceiling on column-scaled solves.** kOptimal also needs the
   caller-unit stationarity `max_j s_j / (obj d_j) <= 1`. This is the analogue of IPOPT's unscaled
   `dual_inf_tol` (default 1, IPOPT options reference). It is loose on purpose: SAWPATH's caller-unit
   stationarity at its optimum is 5.6e-4, against `kkt_tol` 1e-6. Refusals count in
   `scaled_convergence_refusals`.

Final certification stays on the caller's problem: the existing feasibility guard (caller-unit
rows and box <= `feas_tol`), the ceiling above, and the caller-unit re-measurement of every returned
residual.

## Riders

* **YKN-S1** (`03f14cba`): `SsnEngine` treats a seeded bound price within
  `1024 eps * max(1, ||z||_inf)` on an absent side as zero, where before it threw. YORKNET's
  -1.8e-26 is accepted.
* **SCL-R2's trace item**: `sqp.solve.end` gains `columns_active` and `col_spread`. This is a
  declared trace-schema event (`docs/trace-schema-v0.md` s4.2), and both golden lines were
  re-derived. The column report is written before the emission, so the trace and the result agree.
  A test pins this.
* **SCL-T2**: `sqp_solver.h` now says that a kSeeded ingest which the seeded dual clamp degrades to
  kCold stands on x0, while its factors were read at warm.x.

## Tests

The 8 exact model-call-count contracts either pin `column_scaling = kOff`, because their subject is
not the probe (the ladder seams, eval economics, the restoration seed count), or restate the probe
term (`CallCountPerMajorIsBounded`, `SocRescuePaysNoExtraHessian`). The column-layer driver tests set
a 1e3 fixture gate. `TheShippedDefaultIsOff` became `TheShippedDefaultIsAutoAtTheHalfPrecisionGate`.

New tests:
* `ColumnScalingDefault.BelowTheGateTheDefaultIsTheOffSolvePlusTheProbe`: bit-identical answer,
  `evals_values` + 1, two Jacobian calls.
* `TheEngagedRowLayerNeverAmplifiesTheObjective`.
* `ColumnScalingWarmChain.AFiredDecisionIsCarriedWithoutAProbe` and
  `AnUnscaledDecisionIsNotReReadAtTheWarmPoint`, which also cover bad records, the kCold ceiling
  and kOff.
* `ScaledConvergenceGuard.ACallerUnitGradientAColumnFactorHidIsRefused`.
* `SsnEngineLocal.ARoundOffPriceOnAnAbsentBoundIsZero`.

Mutants, all KILLED:
* M1 no ceiling.
* M2 the engaged layer amplifies.
* M3 carried decision ignored.
* M4 unscaled decision not exported.
* M5 carried decision still probes.
* M6 report written after the emission.
* M7 no power-of-two check.
* M8 SSN band exact zero.
* M9 SSN band unbounded.
* M10 trace drops `col_spread`.

## Open

* A warm start WITHOUT a record (hand-built, a payload, an interior-point crossover) at a point where
  a column nearly vanishes still re-reads the rule there. At 2^26 that needs a column ~2^-26 of its
  row's scale. The ceiling bounds the worst case, a false kOptimal, to caller-unit stationarity <= 1;
  tighter than that is not guaranteed.
* A fired solve still drops its scaled-space warm state (radius, funnel, regularization, hot
  factorization, prox) at both ends. A carried non-identity decision could keep it, since the scaled
  space is the same. Not done here. Only SAWPATH-class problems fire.
* The default probe costs one Jacobian evaluation per cold solve (none on a carried warm hop).
* A carried decision outranks a change of mode or threshold mid-chain, short of kOff. This is
  documented.
