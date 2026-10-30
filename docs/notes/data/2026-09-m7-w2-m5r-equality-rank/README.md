# M7 W2 M5r — is the equality Jacobian itself rank-deficient?

**No solver ran to produce anything here.** The probe loads each cell through the
harness's CUTEst bridge, evaluates the constraint Jacobian at the start point,
and does dense linear algebra on it. Nothing under `src/`, `include/` or `tests/`
was touched. The harness tree was only read.

## The answer, in plain language

M5 worked out from its counters, without ever looking at a Jacobian, that the
subproblem's rank trouble on six cells came from the **mandatory rows**: the
equality constraints plus the fixed bounds. This task looked at those rows
directly.

* **On five of the six cells the inference is CONFIRMED (OBSERVED).**
  `SMBANK`, `DALLASM`, `STEENBRB`, `STEENBRC` and `STEENBRG` have equality
  Jacobians that are exactly rank-deficient, because they contain redundant
  rows. All five are network-flow problems. Their node-balance rows add up to
  zero with ±1 coefficients, so one balance row in each connected network says
  nothing new. `SMBANK` and `DALLASM` are single networks, each with **1**
  redundant row. `STEENBRB` is 12 copies of a 9-node network, one copy per
  period, so it has **12** redundant rows. `STEENBRC` and `STEENBRG` have 14
  copies and **14** redundant rows. CUTEst reports every one of these rows as
  linear, and the Jacobian does not move at a perturbed point. So the
  deficiency is present in **every** linearization the solver builds, not only
  at x0. None of these cells has a fixed bound, so the mandatory skeleton is
  exactly `Ae` and its deficiency is `Ae`'s. All three thresholds give the same
  count, and so does an independent SVD.
* **On `SAWPATH` the inference is only PARTIALLY right (OBSERVED).** Its 578
  equality rows are **not** linearly dependent. At machine precision they have
  full rank, and QR and SVD agree on that. But they are very badly scaled: row
  norms run from 1.41 to 6.0e10. They are also ill-conditioned, with singular
  values falling smoothly to 3.5e-12 of the largest and no gap. M5's arm judged
  dependence with a **relative** cut of 1e-8, and at that cut 575 of the 578
  rows read as dependent. So the arm's refusal is real, but "genuine linear
  dependence" is the wrong cause. The cause is scaling and conditioning, which
  is a different engine question. Equilibrating the rows does not remove it:
  234 rows (QR) or 331 rows (SVD) still read dependent at 1e-8.

What this means for the engine proposal: for the network cells the question
is exactly **"handle a rank-deficient equality Jacobian"**. The deficiency is
structural, known in advance (one redundant balance per connected component)
and permanent. `SAWPATH` belongs to a **separate** question: a relative
dependence test run on badly scaled, ill-conditioned rows.

Beyond M5's six cells, the same check finds more dependent equality rows.
`STEENBRD` and `STEENBRE` carry the same 12 and 14 redundant rows as
`STEENBRB` and `STEENBRC`. `BIGBANK` has 1 redundant row in `Ae`, and its
skeleton adds 149 more, because its 308 fixed arc flows cut the network into
pieces. `MSS2` and `MSS3` are nonlinear, with deficiencies of **325** and
**946** at x0 and again at the perturbed point. Their structure is not
identified here. The arm never ran on any of these cells in M5.

### Cell by cell against Q1

| cell | M5 Q1 (counters, INFERRED) | this task (Jacobian, OBSERVED) | verdict |
|---|---|---|---|
| `SMBANK` | refusal 29/29, max deficit 1 | `Ae` 64 rows, rank 63, deficiency **1** at every threshold, raw and equilibrated; one redundant network balance (`c61:N62`); 64/64 rows linear | **CONFIRMED** — and the size (1) matches |
| `DALLASM` | refusal 5/8, max deficit 2 | `Ae` deficiency **1**, one redundant balance (`c3:N4`), 151/151 linear | **CONFIRMED** that the rows are dependent. The 5/8 is NOT explained by the rows, which are deficient on every linearization (see "Open") |
| `STEENBRB` | refusal 5/23, max deficit 45 | `Ae` deficiency **12**, one per period network, 108/108 linear | **CONFIRMED** on the rows. The 5/23 frequency is not explained by them (see "Open") |
| `STEENBRC` | refusal 6/22, max deficit 37 | `Ae` deficiency **14**, 126/126 linear | **CONFIRMED** on the rows; frequency open |
| `STEENBRG` | refusal 6/23, max deficit 37 | `Ae` deficiency **14** (the same 14 rows as C), 126/126 linear | **CONFIRMED** on the rows; frequency open |
| `SAWPATH` | refusal 11/11, max deficit 878 | full rank at eps (578/578, QR = SVD). Deficiency 574 at 1e-10 and **575 at the arm's 1e-8**. Row norms 1.41..6.0e10; cond(R) 6.0e10 | **PARTIAL**: the refusal is reproduced at the arm's cut, but exact linear dependence is **REFUTED**. The cause is scaling and ill-conditioning |

M5's "max rank deficit" column counts rows in the working set, not in the
mandatory skeleton, and working sets are larger. So it is **not** comparable
to the numbers here, except on `SMBANK`, where both are 1.

### Open, and registered rather than explained away

On `DALLASM` and `STEENBRB/C/G` the equality rows are linear and deficient.
Every QP these cells build therefore carries the same dependent rows.
Even so, M5's mandatory-dependent refusal fired on only 5/8, 5/23, 6/22 and
6/23 of their solves. The arm checks the skeleton only on its entry-repair
path: after the seed set fails the invariant, and only when the rank refresh
counts as testable (`qp_degeneracy_arm.h`, `b1_entry_repair`). The QP's own
`Ae` could also differ from the model's on some solves, for example under an
elastic reformulation. **This artifact cannot tell which of these applies,
because no solver ran.** The rows do not explain the frequency, and that
question is left open.

## What was computed

For each of the **23 TRAIN** cells, taken from `sets/split-v1.json` and never
typed in (the probe refused the HOLDOUT name `ACOPP300`, see
`logs/chain-m5r.txt`):

1. `n`, `me` (rows with `g_lower == g_upper`, checked against CUTEst's
   `equatn`), `mi`, fixed bounds, finite bounds, bounds active within an
   **absolute** 1e-8, and components outside the box.
2. `rank(Ae)`, `rank([Ae; e_i fixed])` (the skeleton) and
   `rank([Ae; e_i active])` (the entry set). Each is computed with Eigen 3.5.0
   `ColPivHouseholderQR` on the transpose. The same R is read at three
   thresholds relative to |R_11|: Eigen's default (`diagSize·eps`), **1e-10**,
   and **1e-8**. The 1e-8 read is **added, and declared as added**: it is M5's
   arm `dependency_tol` (`qp_degeneracy_arm.h:453`), which makes the comparison
   with M5 direct. Each rank is also taken on the **row-equilibrated** rows
   (every row scaled to unit norm), which was **added after the second run**
   when `SAWPATH` showed that a relative cut depends on row scaling.
3. `|R_11|/|R_rr|` at the rank, and `|R_{r+1}|/|R_11|`, the gap the cut sits
   in.
4. The dependent rows. For each one this lists its CUTEst index and name
   (`cutest_cnames`), plus the combination of pivot-basis rows that reproduces
   it: support, coefficients, and residual. The residual is ≤ 1.3e-13 on every
   cell except `SAWPATH`.
5. **An identity check**, from a second QR on a different matrix:
   `rank([Ae; E_S]) = |S| + rank(Ae restricted to the columns outside S)`.
   It holds on every cell and point at both thresholds except `SAWPATH`. There
   it holds at the default cut and fails at 1e-10, which is what
   threshold-dependent rows look like.
6. **Point dependence.** Where x0 is out of the box or f(x0) is non-finite,
   the same reads are taken at the **projected start** `min(max(x0,l),u)`.
   This is hven's own `project_start_point` rule, computed here from the
   declared box without a solver. Every cell also gets the equality-Jacobian
   deficiency at a **perturbed** point (x0 ± 1e-3·(1+|x0_i|), projected), plus
   CUTEst's count of linear rows. The perturbed point is not a solve iterate.
7. **The OFF solution's final iterate: SKIPPED.** No committed M3c, M4 or M5
   row or trace carries x; they carry counters. x0 is the decisive point, and
   for the linear cells the answer does not depend on the point.

Where a value is missing, the reason is stated:

* **`NET3`**: f(x0) and the Jacobian at x0 are both non-finite, so the x0
  ranks are recorded as **UNEVALUABLE**. At the projected start (41 clamps)
  everything is finite: `Ae` and the skeleton have full rank, and the
  entry-set deficiency of 166 equals its row-count excess of 166.
* **`STEENBRD` and `STEENBRE`**: f(x0) is non-finite but the Jacobian is
  finite. Both points were evaluated (2 clamps) and agree.
* **`TAX213322` and `TAX53322`**: `me = 0`, so there is nothing to rank
  beyond the active bounds.

The entry-set deficiencies are mostly **row-count excess**: there are more
active rows than variables. That is the case on `SMBANK` (9 = 9),
`DALLASM` (5 = 5), `NET3` (166 = 166) and `BIGBANK` (223 = 223). `YORKNET` is
the exception, at 40 against an excess of 16 at x0. The entry set at x0 is
not what the arm's seed was on any solve M5 ran, so these are context, not
verdicts.

## Cross-check (independent computation)

The probe wrote the three matrices it factored for `SMBANK`, `STEENBRB` and
`SAWPATH` as `%.17g` triplets (`crosscheck/`). `wrappers/crosscheck_m5r.py`
recomputed their ranks by **numpy 2.5.1 SVD** in a separate interpreter, with
the thresholds paired as declared in its header. The brief asked for two
cells; `SAWPATH` was added because it is the cell in dispute.

* `SMBANK` and `STEENBRB`: **0 disagreements** in 36 comparisons. The gaps
  are clean: s₆₄/s₁ = 1.3e-16 on `SMBANK` and s₉₇/s₁ = 1.3e-15 on `STEENBRB`,
  against a smallest kept value of 0.2–0.4.
* `SAWPATH`: **12 disagreements, all reported and none reconciled.** On the
  raw rows, QR and SVD agree at every threshold: 578, 4 and 3 at the default,
  1e-10 and 1e-8 cuts. On the **row-equilibrated** rows they agree at the
  default cut (578) and **disagree** at the other two: QR gives 509 and SVD 430
  at 1e-10, and QR gives 344 and SVD 247 at 1e-8. The singular values sit in a
  dense continuum around both cuts (`logs/crosscheck-m5r.txt` prints them).
  Where the spectrum has no gap, a threshold count is not a well-defined rank,
  and the two methods are entitled to differ. This supports the reading that
  `SAWPATH` is ill-conditioned rather than rank-deficient.

## Protocol and provenance

* **Build.** `wrappers/replay_build_m5r.py` replays M3c pass 1's probe-build
  commands (4 compile edges and 1 link edge) from
  `.scratch/m7w2m3c/probebuild/build.ninja`. It uses the Ninja reader from a
  **byte copy** of M4's `replay_build_m4.py` (sha256 `7403158b…`, checked at
  build time). The declared differences are the probe TU's source, object and
  dep-file paths, and the link's target name. Every other token matched, and
  the transcript says so for each command. `-DEIGEN_MAX_ALIGN_BYTES=32` is on
  every TU. The link names the **installed** `libhven.a` (sha256
  `18a71b72…`, checked against M3c's pin). No `HVEN_TESTING` is set, because
  no seam is needed. The harness CMake was never entered.
* **Binary** `.scratch/m7w2m5r/build/m5r_rank_probe`, sha256
  `fc66e01bb84dbb5d0644839ca0d9117ef44f549315e2203c9cc7b18b5098f0b2`. Every row
  carries it, along with the hven and harness commits, the SIF path and
  sha256, the decoded `OUTSDIF.d` sha256, the date and the host.
* **Box lock per cell.** Each cell ran under `flock /tmp/box-build.lock`, with
  a `pgrep` as its own command before the lock and again inside it, and with
  `PGREP_EMPTY` / `FOREGROUND_START` / `FOREGROUND_END` markers. It ran with
  `MKL_NUM_THREADS=1` and `taskset -c 2`. All 23 legs returned rc=0. Wall
  times are informational: seconds per cell, 18 s at most (`MSS3`).
* **Runs, declared.** The chain ran four times, and only the fourth run's
  rows are committed:
  1. Every leg crashed in the name lookup (a `cutest_cnames` buffer-stride
     bug). No row survived.
  2. The first complete run. It showed that `SAWPATH`'s rank depends on the
     cut.
  3. Added the row-equilibrated read, the 1e-8 arm read, the linear-row count
     and the perturbed point.
  4. Added the fixed-only and active-only row counts.

  Every re-run rebuilt the probe and re-ran all 23 cells. Nothing was
  carried over.

## Files

| path | what |
|---|---|
| `tables/rank.md` | the table (§1), structural readings (§2, INTERPRETATION), point dependence (§3), identity and threshold agreement (§4); every number cites its row file |
| `rows/<CELL>.txt` | one per TRAIN cell: provenance, then `key=value` for every quantity above, and every dependent row as a `*_DEP` line |
| `crosscheck/<CELL>/*.csv` | the dumped matrices the SVD cross-check read |
| `logs/chain-m5r.txt`, `logs/build-m5r.txt`, `logs/crosscheck-m5r.txt` | the chain transcript (lock markers, refusals exercised), the replay-build transcript, and the cross-check output |
| `wrappers/` | `m5r_rank_probe.cpp`, `replay_build_m5r.py`, `replay_build_m4.py` (byte copy), `chain_m5r.sh`, `crosscheck_m5r.py`, `tables_m5r.py` |
| `MANIFEST.sha256` | sha256 of every file here |
