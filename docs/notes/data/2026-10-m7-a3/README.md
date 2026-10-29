# M7 W2 A3 — the IPQP levers: the measurements

The A3 levers — `IpqpOptions::ipqp_converge_slack` (existing; arms S1 = 1, S3 = 1e3) and the new
`IpqpOptions::ipqp_ladder_skip_after` (formerly `detail::kIpqpLadderSkipAfter`; arms K1 = 1, KN =
1000000 "never") with its fire counter `ipqp_ladder_zero_trial_skips` — measured per the design
page `docs/notes/2026-10-m7-w2-a3-ipqp-levers-design.md` and its PRE-REGISTERED section 5. The page
landed at `2909cb21` before any measured row existed; no criterion was edited after.

## Provenance

* hven: branch `w2-a3` off `m7` `534bc87f`. Pre-registration `2909cb21`; implementation
  `ac4b07e6`; test-only fix `70f8d393` (Debug-stable pins; `git diff ac4b07e6 70f8d393 -- include
  src bench` is empty). **Measured head for every measurement leg and the TRAIN pin: `ac4b07e6`.
  Final suites, mutants and P-SYM: `70f8d393`.**
* **Toolchain (C133): NEW.** clang 22.1.8 (Fedora 22.1.8-4.fc44 — the same version A2/A7 record);
  Intel oneAPI MKL resolved at `/opt/intel/oneapi/mkl/latest` → **2026.2** (intel-oneapi-mkl
  2026.2.0-881, the 2026-10-27 update; `MKL_INCLUDE_DIR` in every build's `CMakeCache.txt`).
  Hardware: AMD Ryzen 7 5800X3D (8 physical cores). Date: 2026-10-28. Every OFF-identity claim is
  head vs **this round's own clean build of `534bc87f` on the same toolchain, same session**.
* Box terms (CLAUDE.md §7/§8): every leg its own `flock /tmp/box-build.lock` hold, `pgrep -a
  hven_bench_cell` as a separate command first, one solver at a time, `MKL_NUM_THREADS=1`,
  `OMP_NUM_THREADS=1`, no `timeout`, ctest at `-j4` under `setsid`. The lock was shared with the
  C133 re-baseline round; the `PGREP_NONEMPTY` lines in the chain logs are at BUILD legs only and
  show that round's RETAKE19 cells, which the lock then waited out. No measurement leg reported a
  foreign cell. Counters only; `wall_s` columns are informational. Accelerate: UNOBSERVED.
* HOLDOUT was never run.
* **Declared, not hidden:** two DEVELOPMENT probes (`out/dev/`, `scripts/dev_probe*.sh`) ran the
  new `--ladder` instrument at skip 0/1/2/3/never (Release, then Debug), solely to write the
  unit-test pins of `tests/sqp/test_ipqp_ladder_skip.cpp`. `out/dev/lad-k3.csv` (git describe
  `2909cb2145f1-dirty`, 15:19Z) ran on the uncommitted tree before `ac4b07e6` (15:20Z);
  `out/dev/dbg-lad-k3.csv` (git describe `ac4b07e66c92`, clean, 15:42Z) ran after `ac4b07e6` and
  before `70f8d393` (15:43Z), via `scripts/dev_probe_dbg.sh`, to choose the Debug-stable pin
  fixture. They are not protocol rows; the protocol ran at `ac4b07e6`. No criterion was changed.

## The gates (page §5.1)

| gate | predicted | measured |
|---|---|---|
| **G1** U0 three-arm 27-cell replay, head vs base (new toolchain) | 0/0/0 | **27 × 75, 0 / 0 / 0** (`out/meas/u0-exact-*.txt`) |
| **G2** E1 `--gate` OFF, head vs base, base columns less `wall_s`; new counter 0 on every row | 0 | **29 × 68, 0 differences; counter 0 on 29/29** |
| **G2b** "the same for `--families`" | 0 | identity half: **8 × 46, 0 differences**. Counter half: **ILL-POSED** — see below |
| **G3** suites at `70f8d393` | green | Release **3230/3230**, Debug **3229/3229** (long test excluded), long test **1/1** (`out/suites/`) |
| **G3** P-SYM `534bc87f` vs `70f8d393`, same build path, both configs | exit 1, 0 unexplained | exit 1 (expected); **Release 66 = 11 source-changed + 55 includers, Debug 67 = 11 + 56, UNEXPLAINED 0**; negative control 107 / 106 includers identical (`out/psym/`) |
| **G4** fire counter: 0 on every E1 row under every arm; 0 everywhere under KN; > 0 on a `--ladder` fixture at OFF | PASS | **PASS** (E1 0 ×5 arms; KN 0/0/0; OFF ladder 63) |
| **G5** mutants | all killed | **9 / 9 KILLED** (`out/mutants/mutants.txt`): K1 literal 3, K2 `>` for `>=`, K3 memory condition dropped, K4 counter dropped, K5 counter moved to A2's estimate site, K6 counter every iteration, K7 validation admits negatives, K8 dump line dropped, K9 fold overwrites |
| **T-0** (descriptive) base-new vs committed old-toolchain records | — | U0 vs `2026-10-m7-a2-gates/gates/replay/`: **0 / 0 / 0**; E1 vs `a4-gate-mu1e-2.csv`: **29 × 65, 0 differences**. The MKL 2026.2 update moved nothing these instruments read |

**The first Debug suite (at `ac4b07e6`) failed one test** — the new exact pins on gate 7's
`equality_null_space` fixture, whose trajectory is 26 iterations in Release and 30 in Debug. Fixed
test-only at `70f8d393` (pins moved to `two_negative_eigenvalue_row`, identical in both configs);
the summary line is in `out/suites/chain-a.txt`.

**G2b is ill-posed in its counter half, and the round says so rather than amend it.** G2 reads
"…0 differences; the head's one added column (the new counter) is 0 on every row" and G2b reads
"the same for `--families`". The families instrument carries three nonconvex rows (`hs24` 13,
`hs33` 14, `two_negative_eigenvalue_row` 23), on which the counter fires at the default BY DESIGN —
page §3.2 clause 4 ("Non-zero at the shipped 3 on a nonconvex subproblem") and G4 (which requires
it to fire at OFF). The base binary has no such column, so the counter half compares nothing
against the base; it contradicts the page's own definition. The identity half — the clause's
purpose — holds exactly. **Consequence, as pre-registered: the §3.2 landing rule ("the field lands
at 3 iff §5.1's gates pass") is NOT APPLIED by this round; it is the settler's.** §5.4's
recommendation rule references no §5.1 gate and is evaluated below.

The design page's §5 opening clause ("A criterion found ill-posed is reported as ill-posed and the
round STOPS; it is not amended") was not applied when G2b's counter half was found ill-posed. The
round continued with §5.2-5.4, which do not depend on G2b; no criterion was edited and no default
moved. The settler ratified the continuation (ruling C140, 2026-10-28).

## The fixture clauses (page §5.2) — VETO-CLASS, predicted → measured

OFF (same build): E1 RED `{e1_f7_n20000_af30_m1e-6}`; end-to-end exact 12/27; tier-contract 13/27;
Σ `e2e_factorizations` 746. Every row: `out/meas/scored.txt`.

| clause | S1 (slack 1) | S3 (slack 1e3) | K1 (skip 1) | KN (never) |
|---|---|---|---|---|
| **F-a** no new RED | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** |
| **F-b** recovery (e2e, tier-contract ≥ OFF) | PASS → **PASS** (20, 17) | FAIL → **FAIL** (9, 11) | PASS → **PASS** (identical rows) | PASS → **PASS** (identical rows) |
| **F-c** families, no cell lost | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** |
| **F-d** ladder, gate-9 conditions | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** | PASS → **PASS** |
| **veto-class overall** | **PASS** | **FAIL (F-b)** | **PASS** | **PASS** |

Descriptive (no veto), predicted → measured:

| | OFF | S1 | S3 | K1 | KN |
|---|---|---|---|---|---|
| E1 Σ e2e factorizations | 746 | dearer → **793** | cheaper → **716** | identical → **746** | identical → **746** |
| families Σ HS ipqp factorizations | 183 | dearer → **209** | cheaper → **151** | — → **270** | — → **207** |
| ladder Σ ipqp factorizations | 177 | dearer → **183** | cheaper → **170** | ≤ OFF → **193 (WRONG)** | ≥ OFF → **251** |

**K1's prediction was wrong, and why.** Skipping the zero trial earlier commits the attempt to a
positive shift for the rest of the attempt (the latch, page §2): fixtures that at OFF needed a
modification for 2–3 iterations and then ran unmodified (`hs_indefinite_equality` 2 of 14,
`indefinite_box_family_c0` 2 of 7) instead run shifted to the end and take more iterations (20,
15). On `hs33` the cost is 112 → 201 factorizations. The latch, not the zero trial, dominates.
**KN** pays the zero trial every iteration: `two_negative_eigenvalue_row` 38 → 89 factorizations
(27 → 49 iterations), `equality_null_space` 45 → 67.

**S1** re-measures the T10 record on the new toolchain: tier Rule-A recovery 9 → 23/27 (the
record: 9 → 23), +47 iterations (690 → 737; the record: +47), end-to-end 12 → 20.

## Probe P — the sticky trial (page §3.3), DESCRIPTIVE

Patched scratch copies of `ac4b07e6` (`out/meas/probe-{pu,pc}.patch`), every option at OFF:

* **P-U** (T9's unconditional form) — predicted ≥ 1 fixture to a budget exit → **2 lost**:
  `two_negative_eigenvalue_row` 27 → 60 (`max_iter`, kBudget) and `indefinite_box_family_c1`
  22 → 60; `hs33` 112 → 228 factorizations. T9's freeze class reproduces at `init_mu = 1e-2`.
* **P-C** (T4b's registered conditional form) — predicted no fixture lost → **WRONG: 1 lost**:
  `two_negative_eigenvalue_row` `optimal` → `numerical_error` (escape kInfeasibleSuspect, 32
  iterations). Otherwise within a few factorizations of OFF (ladder 177 → 186, HS 183 → 185).

Reading (pre-registered): P-C loses a fixture cell, so **the record's NOT ADOPTED extends to the
conditional form**. A3-R1 is NOT registered as a re-offer.

## TRAIN (page §5.3) — the A4 rule verbatim, `D = sqp-ipm`

**How it ran.** Two scratch clones of `hven-bench` at `40bb726`, local commits only, never pushed
(`scripts/hven-bench-*.patch`): `bench` — pin → `ac4b07e6` plus the four labels, each `sqp-ipm` plus
ONE field, proved from the lane binary's own dumps (`out/train/dumps/*-vs-sqp-ipm.diff`: exactly one
line each; head-vs-base `sqp-ipm` differs by exactly the new `ipqp.ipqp_ladder_skip_after = 3`
line); `bench-base` — pin → `534bc87f`, nothing else. The CUTEst `.SIF` files and the harness's
decoded instances (`.scratch/h3/decoded`) are symlinked read-only from the harness. Runner:
`scripts/w2_a4_sweep.sh` → `runners/run_serial.py`, TRAIN split, the sets' own ceilings, one cell
per leg, inside the clone's `scripts/leg.sh` gates: **484 legs, every one `PGREP_NO_UNCOVERED_CELL`
and `IN_LOCK_PGREP_EMPTY`** (one pre-lock `PGREP_NONEMPTY`, at the first `hs003` leg: a covered
RETAKE19 cell of the re-baseline round, which the lock waited out). 473 legs exit 0; the 11 exit-1
legs are `hs025` under every label and run — the documented reference failure A2 also recorded.
**A first attempt was aborted at 12:40** after `sqp-ipm`'s `hs` run A: every cutest cell errored on a
missing decoded instance (the clone lacked the `decoded` link). It was stopped, archived
(`out/train/aborted-attempt-1/`) and restarted from empty; nothing from it is used. HOLDOUT was
never run.

**Determinism (A4 rule §4): every label, both families, run A vs run B — `N / N / 0`** (21/21/0 on
`hs`, 23/23/0 on `cutest`; ten compares, `out/train/determinism-*.txt`).

**Controls.** **C-1 (gate) PASS:** the base pin's `sqp-ipm` run A reproduces the head's cell for
cell — **0 of 21 `hs`, 0 of 23 `cutest`** differ on outcome, iterations, factorizations, objective
or `major_iters`. **C-2 (descriptive): 0 / 0** — the base pin on the new toolchain reproduces A2's
old-toolchain `sqp-ipm` run A cell for cell. No toolchain effect on any TRAIN row.

**The decision** — the harness's `scripts/w2_a4_decide.py`, unmodified, run A scored, `D = sqp-ipm`
in the tree's `default` slot (`out/train/decisions.txt`):

| family | label | optimal | dnf_wall | error | \|B\| | median cost | verdict | predicted |
|---|---|---:|---:|---:|---:|---:|---|---|
| `hs` | `sqp-ipm` (D) | 21 | 0 | 0 | 21 | 10.6667 | — | — |
| `hs` | `slack1` | 21 | 0 | 0 | 21 | 11.625 | **lose: cost** | lose |
| `hs` | `slack1e3` | 21 | 0 | 0 | 21 | **9.88889** | **WIN** | WIN |
| `hs` | `skip1` | 21 | 0 | 0 | 21 | 10.6667 | **TIE** | TIE |
| `hs` | `skipnever` | 21 | 0 | 0 | 21 | 10.6667 | **TIE** | TIE or lose |
| `cutest` | `sqp-ipm` (D) | 16 | 5 | 2 | 16 | 207.696 | — | — |
| `cutest` | `slack1` | 16 | 6 | 1 | 16 | 207.696 | **TIE** | lose (**WRONG**) |
| `cutest` | `slack1e3` | 16 | 6 | 1 | 16 | 207.696 | **TIE** | WIN or TIE |
| `cutest` | `skip1` | 16 | 5 | 2 | 16 | **207.676** | **WIN** | TIE (**WRONG**) |
| `cutest` | `skipnever` | 16 | 5 | 2 | 16 | 207.812 | **lose: cost** | TIE or lose |

Per cell (`out/train/per-cell-run-A.txt`):

* **The tier barely touches `cutest`.** Under either slack only 4 of 23 cells move; `TAX53322`'s
  `error` becomes `dnf_wall` under both (clause 2 scores that in their favour, errors 2 → 1, exactly
  as A2's `mufloor` did) and the median is unmoved. Under either skip value 8 cells move, each by at
  most 0.9 % of its factorizations.
* **`skip1`'s `cutest` WIN is 0.02 factorizations per major** (207.676 vs 207.696): seven cells a
  few factorizations cheaper (`STEENBRB` 40541 → 40539, `ACOPR118` 15985 → 15983), `ACOPP118`
  dearer (3148 → 3175). Its `hs` TIE hides one large per-cell loss: **`hs033` 118 → 470
  factorizations** (19.7 → 36.2 per major) — the in-tree families' `hs33` 112 → 201 again (the
  latch, above).
* **`slack1e3` wins `hs` broadly**: of the 19 cells that move, 17 are cheaper per major (`hs033`
  19.7 → 12.3); `hs010` is dearer (3.1 → 4.4), and `hs027` takes more factorizations (110 → 174)
  over more majors.
* `skipnever` on `hs`: one cell moves (`hs033` 118 → 119).

## What the pre-registered reading decides (page §5.4)

The pre-registered rule: an arm is RECOMMENDED iff (1) it WINS on a family and loses on none, (2) it
passes every VETO-CLASS fixture clause (C129), and (3) its runs are deterministic `N / N / 0`.

| arm | (1) TRAIN | (2) veto-class | (3) determinism | reading | predicted |
|---|---|---|---|---|---|
| S1 slack 1 | `hs` lose | PASS | PASS | **not recommended** | not recommended |
| S3 slack 1e3 | `hs` WIN, `cutest` TIE | **FAIL (F-b: recovery 12 → 9, 13 → 11)** | PASS | **not recommended — vetoed (C129)** | vetoed |
| K1 skip 1 | `hs` TIE, `cutest` WIN | PASS | PASS | **RECOMMENDED by the rule** | not recommended (**WRONG**) |
| KN never | `cutest` lose | PASS | PASS | **not recommended** | not recommended |

* **`ipqp_converge_slack` stays 1e2.** Slack 1 buys face recovery and costs factorizations on `hs`.
  Slack 1e3 is the only arm that wins `hs` broadly, and it is the arm whose face recovery the veto
  protects.
* **For `ipqp_ladder_skip_after`, the pre-registered rule recommends 1.** These facts are stated
  beside that result, not folded into it. The `cutest` WIN is a 0.01 % median margin, over cells
  the tier barely touches. The `hs` TIE carries `hs033` at 4× the factorizations. Every in-tree
  descriptive moved the other way (families HS 183 → 270, ladder 177 → 193). The page did not
  pre-register a margin, a per-cell loss bound or a descriptive bar, so this README reports these
  facts and does not resolve them. **No default moves in this round**: under page §5.4 a move is
  the settler's declared event. The field's landing is also the settler's call (G2b, above).
* **The sticky trial stays NOT ADOPTED**, now for both forms (Probe P).

## For the settler

* **G2b's counter half is ill-posed** (above). The §3.2 landing rule was not applied.
* **K1's TRAIN recommendation conflicts with every descriptive measurement.** The rule's
  arithmetic is not changed here.
* Registered: **A3-R2**, the skip rule's one-way latch. Once the rule fires within an attempt, the
  zero trial is never offered again. That latch, not the zero trial, drives K1's costs; a rule
  that re-offers IC-1 once the shift reaches `ipqp_reg_floor` is a separate design. **A3-R3**: a
  per-major slack schedule. **M7-39** stays CARRIED (page §3.4). **A3-R1 is not registered**,
  because P-C lost a fixture cell.

## What is here

* `scripts/` — every driver as it ran, the two scratch-clone patches
  (`hven-bench-scratch-clone.patch`: pin → `ac4b07e6` + the four labels;
  `hven-bench-base-clone.patch`: pin → `534bc87f`).
* `out/meas/` — U0 CSVs and compares, E1 / families / ladder CSVs per arm, the probe CSVs and
  patches, T-0 compares, `scored.txt` (re-derived by `scripts/score_meas.py`).
* `out/suites/`, `out/mutants/`, `out/psym/`, `out/dev/`, `logs/`.
* `out/train/` — the TRAIN tree, determinism compares, decisions, controls, option dumps, chain
  summary.
