# W5 T8.9r-attrib4 — the redraw experiment: the four-arm per-row wall, and what E1 and E2 recover

`mechanism.md` (attrib3) charged a third of `9cebbbe`'s step to per-call
allocate/free churn, measured through an allocator intervention, and left two
thirds unexplained. **This leg asks the direct question: if the churn is the
carrier, does making the per-call storage PERSISTENT recover the step?** Two
patched arms answer it, each a scratch build of a `git archive` extraction of
`e51a7e0` with one patch, built by the T8.9r recipe, measured against `102f729`
and `e51a7e0` in the same five rounds.

**THE ANSWER IS NO, AND THE MARGIN IS LARGE.** The better of the two recovers
**+4.5 %** of the corpus step, inside the arms' own round-to-round spread; the
other is a **regression**. Both leave every counter and diagnostic identical.
And the fault instrument says why the question was worth asking anyway: **E2
removes 57.3 % of the head's excess minor page faults and buys 4.5 % of the
wall step**, which is this leg's one quantitative result — see `mechanism.md`
§C.

---

## A. The arms, and what they are

| arm | dir | what | `libhven.a` sha256 |
|---|---|---|---|
| a1 | `base___` | `102f729`, the interior leg's base | `735eea1d9d51ef93…` |
| a2 | `head___` | `e51a7e0`, the group-1 head | `60bfe03f79078eb8…` |
| a3 | `e1_____` | `e51a7e0` + `experiment-E1.patch` | `385625c8675cde7b…` |
| a4 | `e2_____` | `e51a7e0` + `experiment-E2.patch` | `04c6b058dbb9450f…` |

a1 and a2 are `../../attribution/arms.txt`'s own binaries, re-verified by sha256
before use (2/2 executables, 2/2 libraries) and copied into fixed-width
directory names so the measured child's argv is the same length at every arm.
a3 and a4 were built for this leg.

**THE RECIPE CALIBRATION PASSES.** A fourth build — `cal____`, an UNPATCHED
`e51a7e0` extraction put through this leg's exact recipe — produced a
`libhven.a` whose sha256 is
`60bfe03f79078eb8ab51fd099a7253f91e232714a008a52be0a40e49403fa30a`, **byte for
byte `attribution/arms.txt`'s**. The build path is not in that file, so the
quantity that can be calibrated is, and it is. (`logs/B1-build.log`.)

**THE ARGV LOCK HOLDS: `ARGV_LEN` reads 263 at every arm in every round**, printed
in each batch log rather than asserted.

**THE MEASUREMENT PIN**, identical at every arm:

```
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    <bin> --engine interior \
          --cells f7_n1000_bound_physics,f7_n5000_bound_physics,\
f7_n10000_bound_neutral,f7_n20000_bound_neutral \
          --csv <.../w-aN-rR.csv>
```

FIVE rounds, one batch per round, arm order ROTATED by (round−1). The first row
every process writes is `f7_n1000_bound_physics/MakeParameter` at **all four
arms** and is excluded by the pre-declared positional rule R3; eleven rows are
scored. `predeclaration.txt` is hashed in `logs/B0-predeclaration.log` at
**20:11:57 UTC**, before the first `FOREGROUND_START` of a timed batch at
**20:26:50 UTC**.

## B. The correctness gate — PASSED, on a larger population than the brief asked for

Every patched arm was run over the WHOLE `--engine interior --cells all` leg and
compared against `e51a7e0`'s, column by column, before any wall figure was read.

| arm | rows | columns compared | cells compared | mismatches |
|---|---|---|---|---|
| E1 | 85 | 30 (every column but `wall_s`) | 2 550 | **0** |
| E2 | 85 | 30 | 2 550 | **0** |

`--cells all` writes **85 rows**, a superset of the 43 the T8.9r interior leg
captures, so the gate is taken on more rows than the brief specified, not fewer
(`logs/B2-gate.log`, `logs/B3-gate-check.log`, `scripts/gate_check.py`).
**`hven_tests` was NOT built at any arm** (`-DHVEN_BUILD_TESTS=OFF`): no unit
suite is claimed here, and a fix commit adopting either patch would run them.

## C. The four-arm per-row table

Median of five rounds, seconds. RECOVERY = 1 − (E/base − 1) / (head/base − 1),
per the pre-declared rule, reported with its sign and never clipped.

| row | base `102f729` | head `e51a7e0` | E1 | E2 | head/base | E1/base | E2/base | E1 recovery | E2 recovery |
|---|---|---|---|---|---|---|---|---|---|
| `f7_n1000_bound_physics/MakeConstraint` | 0.031989 | 0.032536 | 0.032676 | 0.032593 | 1.0171 | 1.0215 | 1.0189 | −25.6 % | −10.5 % |
| `f7_n1000_bound_physics/RelaxBounds` | 0.031971 | 0.032436 | 0.032631 | 0.032569 | 1.0146 | 1.0206 | 1.0187 | −41.7 % | −28.5 % |
| `f7_n5000_bound_physics/MakeParameter` | 0.179004 | 0.185896 | 0.186005 | 0.186340 | 1.0385 | 1.0391 | 1.0410 | −1.6 % | −6.4 % |
| `f7_n5000_bound_physics/MakeConstraint` | 0.174289 | 0.180803 | 0.180941 | 0.179688 | 1.0374 | 1.0382 | 1.0310 | −2.1 % | +17.1 % |
| `f7_n5000_bound_physics/RelaxBounds` | 0.173521 | 0.180013 | 0.177631 | 0.177711 | 1.0374 | 1.0237 | 1.0241 | +36.7 % | +35.5 % |
| `f7_n10000_bound_neutral/MakeParameter` | 0.539287 | 0.554721 | 0.554696 | 0.554164 | 1.0286 | 1.0286 | 1.0276 | +0.2 % | +3.6 % |
| `f7_n10000_bound_neutral/MakeConstraint` | 0.523104 | 0.538628 | 0.538507 | 0.537214 | 1.0297 | 1.0294 | 1.0270 | +0.8 % | +9.1 % |
| `f7_n10000_bound_neutral/RelaxBounds` | 0.523523 | 0.533464 | 0.541195 | 0.538622 | 1.0190 | 1.0338 | 1.0288 | −77.8 % | −51.9 % |
| `f7_n20000_bound_neutral/MakeParameter` | 1.127292 | 1.157333 | 1.160374 | 1.157245 | 1.0266 | 1.0293 | 1.0266 | −10.1 % | +0.3 % |
| `f7_n20000_bound_neutral/MakeConstraint` | 1.093735 | 1.125526 | 1.126744 | 1.113727 | 1.0291 | 1.0302 | 1.0183 | −3.8 % | +37.1 % |
| `f7_n20000_bound_neutral/RelaxBounds` | 1.092237 | 1.112104 | 1.125686 | 1.117176 | 1.0182 | 1.0306 | 1.0228 | −68.4 % | −25.5 % |
| **scored corpus (11 rows)** | **5.489952** | **5.633460** | **5.657087** | **5.627050** | **1.0261** | **1.0304** | **1.0250** | **−16.5 %** | **+4.5 %** |
| **median per-row recovery** | | | | | | | | **−3.8 %** | **+0.3 %** |

**THE STEP REPRODUCES, FROM A FOURTH ROUND SET.** head/base on the eleven scored
rows is **1.0261 (+2.614 %)**, against §5's 1.0249 over 29 rows, §11's 1.0250
over these eleven and attrib3's cumulative 1.0283 over the T8.4 arms. The thing
being chased is there and is the same size.

**AND THE SPREAD IS THE LIMIT ON READING THE TWO EXPERIMENT COLUMNS.** The five
rounds' scored-corpus sums span:

| arm | min (s) | max (s) | spread |
|---|---|---|---|
| a1 base | 5.480564 | 5.497692 | 0.3125 % |
| a2 head | 5.626756 | 5.649129 | 0.3976 % |
| a3 E1 | 5.650304 | 5.665712 | 0.2727 % |
| a4 E2 | 5.617700 | 5.641660 | 0.4265 % |

E2's corpus sits **0.114 %** below the head's and E1's sits **0.419 %** above it,
against a per-arm round-to-round spread of 0.27–0.43 %. **E2's +4.5 % is inside
that spread and is reported as a number, not as a recovery anyone should act
on; E1's −16.5 % is a slowdown of about one spread and is reported the same
way.** Neither is 80 %, neither is 30 %, and that is the finding.

## D. The solo evidence

Everything in §C asserts wall clock and was taken under the T8.9r fix1 R2
discipline — one solve at a time, `taskset -c 2`, `MKL_NUM_THREADS=1`, the box
lock held for the whole batch, the driving shell pinned off `cpu2` and `cpu10`.

**THE IDLE ACCOUNTING IS THE CORRECTED ONE** (`scripts/idle_proof.py`, astra's
fix1-review item 7): foreign task time on a core is `user + nice + steal +
guest`, not `user` alone, with only the run's own `user` seconds subtracted and
only on `cpu2`; pids present in one snapshot of a batch but not the other are
counted as TRANSIENTS rather than dropped; every foreign state seen is listed.
The superseded `user`-only figure is printed beside the repaired one in
`logs/IDLE-PROOF-WALL.md`.

**FIVE TIMED WALL BATCHES, ALL PINNED-CLEAN, `idle_proof.py` exits 0.**

| batch | timed wall (s) | cpu2 foreign TASK (s) | fraction | cpu10 TASK (s) | fraction | verdict |
|---|---|---|---|---|---|---|
| wall-r1 | 26.36 | 0.010 | 0.0379 % | 0.110 | 0.4173 % | PINNED-CLEAN |
| wall-r2 | 26.45 | 0.020 | 0.0756 % | 0.090 | 0.3403 % | PINNED-CLEAN |
| wall-r3 | 26.73 | 0.020 | 0.0748 % | 0.070 | 0.2619 % | PINNED-CLEAN |
| wall-r4 | 26.71 | 0.010 | 0.0374 % | 0.130 | 0.4867 % | PINNED-CLEAN |
| wall-r5 | 26.55 | 0.010 | 0.0377 % | 0.090 | 0.3390 % | PINNED-CLEAN |

**ROUND 2 WAS RE-RUN TWICE, and the correction is what caught the second
failure.** On the first pass `wall-r2` read **0.260 s / 0.9142 %** of foreign
task time on `cpu2`. On the second pass it read **0.140 s / 0.5317 %** on the
SMT sibling — and **0.110 s of that is `nice`**, which the superseded
`user`-only accounting would have reported as **0.030 s / 0.114 %** and passed.
That is astra's item-7 defect caught in the wild, on this box, on this leg's own
data. The third pass is the retained data; each re-run overwrote its batch's
log, CSVs and stdout, which is what "the batch is re-run" means. **Nothing was
ever signalled.** The `BOX_PAUSE`/`BOX_PAUSE_GIVEUP` record is in each batch log.

**The fault pass and E3 are declared CO-RUN TOLERANT in `predeclaration.txt`**
— they assert per-process counters, not wall clock, and no wall figure from
either is quoted anywhere.

**THE FOLD PROOF** ran both ways against stubbed failures and is retained
(`logs/F1-fold-proof-fail.log`, `…-pass.log`): worst-fold reaches 5 from
(2, 5, 3) with a later rc 0 and a later smaller rc 3 both failing to clear it,
and 0 when every stub succeeds.

TOOLCHAIN AND HARDWARE: as `../../PROVENANCE.txt` — clang 22.1.8, CMake 4.3.0,
Ninja 1.13.2, MKL 2026.1, AMD Ryzen 7 5800X3D (Zen 3), Fedora 44, kernel
7.1.9-200.fc44, `perf_event_paranoid = 2`, `cpu2` pinned with `cpu10` idle.
Submodules `dep/eigen bc3b3987…`, `dep/fmt 407c905e…`, the two commits
`arms.txt` names.
APPLE / ACCELERATE: UNOBSERVED.  WINDOWS: UNOBSERVED.
