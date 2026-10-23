# M7 W2 RD8 -- READING8 on the exact-Hessian SQP label (the last SNOPT-gap round)

> Branch `rd8`, base `6a2a58c7` (the SCW-gated m7 head; RETAKE18 pin). Commits: `ba3fbf24`
> (wip, NOT FOR MERGE: the pattern-stable eliminated assembly, WLF-PSA) and this record.
> Provenance: clang 22.1.8, AMD Ryzen 7 5800X3D, 2026-10-23. Every leg ran under
> `flock /tmp/box-build.lock` with `pgrep -a hven_bench_cell` empty inside the lock, one solver
> at a time, `MKL_NUM_THREADS=1`, `taskset -c 2`. Walls are informational. Scratch:
> `.scratch/rd8` of worktree `agent-af791fbc982c20d3f` (STATE.md there; the 15 MB QP dumps stay there).

## Result

**Negative for READING8 on both labels.** No change set found in this round converts it: the
exact label's first QP cannot be solved by the walk in the cell's budget, and the linear algebra
is only one of three blockers. The one change built for production, the pattern-stable
eliminated assembly (WLF-PSA, `ba3fbf24`), halves the per-factorization cost on the cap-latched
path (READING8 QP0: 0.21 -> 0.11 s) and brings BRITGAS-default under the 300 s ceiling (298.1 s,
R18 376.3 s), but it changes the rounding of every QP latched past the cap. On the 46 cells it is
PATH-CHAOTIC: 14 identical, 20 moved, 1 converted (SAWPATH qn), 2 LOST (NET3 default,
DALLASM qn), two worse local minima (STEENBRC, STEENBRG default), net 16 / 8 unchanged. With 17
(Release) / 14 (Debug) recorded-fixture pins moving as well, it is a **measured negative as a
default**; not for merge.

## 1. Diagnosis at 6a2a58c7 (ranked)

In-cell (scratch lane `d0` = HEAD + the env-gated QP log): **no QP finishes in 300 s**; the
first QP alone outlasts the cell (`cells-diag/diag0`). QP0 (entry dump, `C10_DUMPENTRY=0`):
n = 2002, me = 1000, mi = 0; Ae is the dense lower-triangular 1000 x 1001 Volterra block in P plus
two bilinear entries per row in A (502 500 nonzeros); H is 2x2 blocks per (A_j, P_j); the trust
region (radius 1) bounds every P. On null(Ae) the reduced Hessian has dimension 1002 and **998
negative eigenvalues**. be is 1e-5 (x = 0 nearly meets the rows); 385 of the 1001 A's sit on a
bound at x = 0. SNOPT's row: 19 majors, 1188 minors in total, f = -2290.45.

Replay of QP0 at HEAD (`qp0-replays/base-2000`, 2000 minors, 586 s):

1. **The walk ascends from a start that is 1e-5 off its equality rows.** Minor 0's start repair
   pins by most-negative-diagonal order, one probe factorization per pin: 877 probes (~180 s),
   then the n/2 budget stops it and it undoes itself. Minors 1-968 add one bound per minor at
   alpha 1e-8..1e-7 while q RISES from 0 to +322: each EQP step heads for the stationary point
   of an indefinite face, and the ascent guard (section 4c) never fires because it requires the
   iterate to meet its working rows, which it never does at those step lengths. Counterfactual
   (`psa-be0-3000`, be := 0 so x = 0 meets the rows): the guard fires (215 rides) and q
   DESCENDS to -191 by minor 600.
2. **Near-cycling on the pinned face.** After the add phase the walk enters a period-2
   drop / ride-undone sweep (one productive ride per ~75 minors; CYC's class-A mechanism).
   With CYC-C1 (kappa = 1) on the counterfactual QP: q -742 by minor 750, then the walk ascends
   again and stops on stagnation at minor 905, kMaxIter (`psa-be0-leak1-3000`). So even from a
   feasible start, with C1, QP0 does not certify.
3. **Per-minor linear algebra.** Past pin 128 the walk is cap-latched on the eliminated path,
   and every pass pays a fresh Pardiso symbolic analysis: 0.21 s per factorization. QP0 needs
   ~1000 bound additions by construction (the reduced Hessian has 998 negative eigenvalues), so
   ~1000 minors is a floor for any one-constraint-per-minor active-set walk.

The exact label therefore needs (1) and (2) solved before (3) matters; (3) alone cannot convert.

The QN label (`cells-diag/qncell`, with PSA): QP0 526 minors, 89 s; warm QPs 14-112 minors at
0.25-0.4 s per factorization -- the L-BFGS compact form adds 20 dense rows/columns, so the padded
factorization is no cheaper than the reduced one was -- ~10 s per major; capped at 20 majors.

## 2. Experiments (scratch arms, env-gated; `tools/patch_arms_rd8.py`)

| arm | QP0 outcome |
|---|---|
| base (HEAD) | 2000 minors kMaxIter, q +181.9, 586 s; repair refused (877 probes) |
| PSA only | 1093 minors, stagnation stop (kMaxIter), q +190.6, 207 s; 0.11 s/factorization |
| batch repair (doubling, budget 2002) | 2000 pins, all bounded variables: dependent set, 500 minors of removals |
| batch + bisection (`RD8_RBATCH=2`) | minimal prefix 1522 pins in 11 probes (was 877 probes for a failed repair); pins move x -> rows violated; 2000 minors kMaxIter, q -591 |
| be := 0 (counterfactual) | guard fires; q -204.6 at 3000 minors (drop/undo grind) |
| be := 0 + C1 (kappa 1) | q -742 at minor 750, then stagnation stop at 905, kMaxIter |
| min-norm face step once (`RD8_MEET`) | blocked at alpha = 0 by at-bound variables: no effect |
| in-cell C1 + PSA | QP0 1093 minors, 205 s, stagnation stop; cell capped |

## 3. Design (sources)

Built: **WLF-PSA**, the registered pattern-stable eliminated assembly. Past `schur_cap` in border
mode the eliminated K is factorized as its padded twin: K0's layout and stored pattern
(`assemble_kkt_full`), entries touching a pinned variable set to explicit zeros and its diagonal
to 1. Up to a symmetric permutation K_pad = diag(K_elim, I), so its solves (rhs zero on the padded
rows) are the eliminated system's and its inertia is the eliminated one plus one positive per pin
(`kkt_inertia`). The analysis is keyed on the pattern, which no longer moves with the pin set.
This is the standard symbolic/numeric split (George & Liu, Computer Solution of Large Sparse
Positive Definite Systems, 1981; Duff, Erisman & Reid, Direct Methods for Sparse Matrices, 2nd ed.
2017, ch. 9-10 on reusing an analysis across value changes) applied to the bound-eliminated KKT of
an active-set QP (Gill, Murray, Saunders & Wright, "A Schur-complement method for sparse quadratic
programming", 1990, motivates keeping the factorized pattern fixed across working-set changes).
No iparm change. Accelerate: the same `SymmetricFactor` calls; UNOBSERVED.

Not built (what conversion would need, from the measurements above):
* a feasibility-first rule for a walk whose start face has the wrong inertia: reach the working
  rows (minimum-norm correction with the at-bound variables held, Nocedal & Wright 2006 s16.5;
  Gill, Murray & Wright, Practical Optimization 1981 s5.2) before the inertia-controlling rules
  (Gill, Murray, Saunders & Wright, "Inertia-controlling methods for general quadratic
  programming", SIAM Rev. 33, 1991) can act;
* CYC-C1 (registered; kappa <= 1, scoped to the mu-regularized ordinary p);
* a stop for the post-descent re-ascent on QP0 (not diagnosed in this round);
* for QN, bordering past the cap for stacks grown incrementally (the adaptive cap) or a K0 taken
  at the seed working set, since each warm QP refactorizes a K with 20 dense rows per minor.

## 4. PSA: what it costs (`ba3fbf24`)

Tests: `tests/sqp/test_kkt_padded.cpp`, 10 tests (pattern identity across pin sets; reduced rows
equal the eliminated K; solve and inertia equal the eliminated factor's; one analysis across three
pin sets; a plain factorization clears the padding; map refusal; the applicability rule; solve_eqp
past the cap equals below it; a start repair past cap 0 equals the refactorize oracle; a walk past
the cap equals the oracle with zero inertia re-reads). Mutants 10/10 killed (`tools/mutants.py`;
M9, the walk's eliminated inertia read, needed the start-repair test).

Suites (`suites/`; 3212 run of 3214 listed): Release 17 failed, Debug 14 failed, long test 1/1
in both. Every failure is a recorded-fixture pin on a QP latched past the cap (STEENBRB / BRITGAS
/ YORKNET restoration and elastic dumps) plus one precondition in `SqpElasticWitnessStart`.
Outcomes move both ways: `QpE4ProximalEqp.TheSteenbrbRestorationQp2ExitsFiniteAndSaysWhy`
(declared kNumericalError) now certifies kOptimal (both builds);
`QpNc2FlatFace.TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeBorder` loses kOptimal for kMaxIter
(both builds); Debug `QpStagnationStop.BritgasRestorationFootprintFace` ends kNumericalError;
`QpW1AscentGuard.BritgasElasticQp1Border` counts 2 equality-drift violations where it pinned 0.
Mechanism pins (rank repair, refused exchange, saddle shift, convex certificate, structural-first,
minor counts of the declined-ride fixtures) read 0 or move by 1-8 minors: those dead ends no longer
occur on the new rounding, so the fixtures lose their trigger. Full list: `suites/*failures.txt`.

U0 exact vs `2026-10-m7-scw-gates` (`u0/`): ssn 0, ipm 0; walk 7 differences, all residual-class
columns of `f7_n1000_path_warm` and `f7_n800_path_warm` (kkt_residual, stationarity, primal,
complementarity in the 8th-9th digit) -- the two cells whose walk QPs pass the cap; counters and
statuses identical. Declared, not re-pinned.

46 TRAIN cells vs RETAKE18: section 5.

## 5. 46 cells (`cells/`, lane = `ba3fbf24` built as RETAKE18's lane, bench 40bb726 + an env-gated trace overlay, options dumps byte-identical to RETAKE18's declared dumps)

Caps as RETAKE18 measured them (300 s for its capped cells, 590 s otherwise). Every key compared
against RETAKE18's measured rows (`cells/vs-retake18.md`):

| class | cells |
|---|---|
| IDENTICAL (every key) | 14: ACOPP118 x2, ACOPP57 x2, ACOPR118 x2, ACOPR57 x2, DALLASM-default, READING6 x2, SAWPATH-default, SMBANK x2 |
| CAPPED both | 9: MSS2-default, MSS3 x2, READING8 x2, TAX213322 x2, TAX53322 x2 |
| CONVERTED | SAWPATH-sqp-qn: numerical_error 16 maj -> optimal 48 maj, f 750.4589 (the default label's optimum), 234 s |
| LOST | NET3-default: optimal 23 maj f 5859393.70 -> numerical_error 71 maj f 6066927.56; DALLASM-sqp-qn: optimal 142 maj -> max_iter 297 maj (same f to 15 digits) |
| MOVED, same status | 20; of the optimal ones: BRITGAS-default 62 -> 60 maj, f 0, **298.1 s** (R18 376.3 s, past the ceiling; the 4-cell probe 297.6 s); STEENBRB/D/E-default same f to 8+ digits, 1.2-1.9x the majors; **STEENBRC-default f 27504.94 -> 27593.99 and STEENBRG-default 27420.93 -> 27520.37: worse local minima**; BIGBANK-default, BRITGAS-qn, ZAMB2-8 x2: f equal to 14+ digits. Non-optimal ones moved inside their status (BIGBANK-qn, MSS2-qn, NET3-qn, STEENBR*-qn, YORKNET x2) |

Label tallies against RETAKE18: exact 16 -> 16 (BRITGAS in under the ceiling, NET3 out), QN 8 -> 8
(SAWPATH in, DALLASM out). READING8 capped in both labels.

## 6. Recommendation

* **READING8: negative, recorded.** Ranked blockers (section 1): the ascent from an off-face start
  on a 998-negative-eigenvalue reduced Hessian (the ascent guard is gated on meeting the rows), the
  drop / ride-undone near-cycling on the pinned face (CYC-C1 shortens it but QP0 still re-ascends
  and stops uncertified), and ~1000 minors at 0.1-0.2 s each. Each is a walk-quality feature, not a
  lever.
* **PSA (`ba3fbf24`): do not land as the default.** The cost win is real (one analysis per latch
  instead of one per pass) but it re-rounds every past-cap QP, and on this corpus that is a coin
  toss per cell (2 lost, 1 converted, 2 worse local minima, BRITGAS in under the ceiling by 2 s).
  If kept at all, as an opt-in for a future round that also makes the past-cap walk robust to
  rounding.
* **CYC-C1** stays registered; this round measured it on READING8 QP0 only (above), not on the 46.

## 7. Registered

* RD8-F: a feasibility-first (face-meeting) rule for walks whose start face has the wrong inertia:
  the minimum-norm correction with the at-bound variables held, before the ascent guard can act.
* RD8-R: the start repair's probe count. Doubling + bisection on the curvature-order prefix finds
  the same minimal prefix in O(log n) probes (11 vs 877 on READING8 QP0); but pins at a bound move
  x off the rows, so it needs RD8-F first.
* RD8-Q: QN warm QPs refactorize a K with the L-BFGS block's dense rows every minor (0.3 s at n =
  2022); bordering past the cap for stacks grown in-solve, or a K0 at the seed working set.
* WLF-PSA's measured outcome (this record) for the register entry.
