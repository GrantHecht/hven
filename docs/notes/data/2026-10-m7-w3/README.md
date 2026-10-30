# M7 W3 — the globalization ratification ladder: the measurements

R1 (`ssn_sigma_rule = kResidualArmed`), R4 (`ssn_infeasibility_rule = kFarkasGated`, with the
ordered 1e-4 re-derivation of `kSsnFarkasResidualTol`), R5 (`ssn_certify_from_face`) and the τ item
(`kFunnelTauBar`), measured per the design page `docs/notes/2026-10-m7-w3-ratification-design.md`
and its PRE-REGISTERED §5. The page landed at `de8dab89` before any measured row existed; no
criterion was edited after.

## Provenance

* hven: branch `w3` off `m7` `bb512d69`. Pre-registration `de8dab89`; the bench switch `7187a255`
  (`hven_sqp_corpus --hs` applies the SSN measurement levers and appends ten SSN columns; bench only).
  **Measured head for every leg and the TRAIN pin: `7187a255`.** `git diff bb512d69 7187a255 --
  include src` is EMPTY (G0).
* Scratch commits (local branches, never merged, never pushed; one line each, `patches/`):
  `w3-scratch-farkas1e-4` `29f45772` (`kSsnFarkasResidualTol = 1e-4`), `w3-scratch-tau10` `bc7635a6`
  (`kFunnelTauBar = 10.0`), `w3-scratch-tau1e3` `156e47c3` (`kFunnelTauBar = 1000.0`). Each built as
  a whole tree with the head's configure line.
* hven-bench: four scratch clones at `b5423f6` with one local commit each (`patches/hven-bench-scratch-*.patch`):
  `bench` (pin `7187a255` + labels `sqp-ssn-sigma-armed`, `sqp-ssn-farkas`, `sqp-ssn-certify-face`),
  `bench-base` (pin `bb512d69`), `bench-tau10` (pin `bc7635a6`), `bench-tau1e3` (pin `156e47c3`).
  The installed headers of the two τ prefixes were checked to carry 10.0 and 1000.0.
* **Toolchain (C133): NEW.** clang 22.1.8 (Fedora 22.1.8-4.fc44); MKL `/opt/intel/oneapi/mkl/latest`
  → **2026.2** (`MKL_INCLUDE_DIR` in every `CMakeCache.txt`). AMD Ryzen 7 5800X3D. 2026-10-28/29.
  No old-toolchain number is used as a same-run comparison.
* Box terms: every leg its own `flock /tmp/box-build.lock` hold with PGREP / IN_LOCK_PGREP /
  FOREGROUND_START / FOREGROUND_END markers (`logs/`, `out/train/logs/`), `MKL_NUM_THREADS=1`,
  `OMP_NUM_THREADS=1`, no `timeout`, ctest `-j4` under `setsid`. The lock was shared with the A3
  gates round; my legs queued behind it. **668/668 TRAIN legs (660 cell legs = 7x88 + 44, plus 8 pin/harness build legs; per `out/train/logs/chain-summary.txt`) `PGREP_NO_UNCOVERED_CELL` and
  `IN_LOCK_PGREP_EMPTY`**; every in-tree leg `IN_LOCK_PGREP_EMPTY`. Counters only; wall informational.
  Accelerate: UNOBSERVED. **HOLDOUT was never run.**
* **Declared, not hidden:**
  * Two in-tree legs exceeded the 600 s leg bound: `u0-tau10-walk` (16 min — the two `path_warm`
    cells' SETUP hops (a cold solve at p0, not a scored row) ran long at τ̄ = 10; both scored rows
    `Optimal` in 0.11/0.23 s) and each corpus-44 leg (~8 min; the ~2 min figure was my own estimate from the
    committed walls, not the page's -- the page gives only the 300 s per-cell cut, §5.2). Nothing was killed.
  * The τ-probe recipe's first attempt (`logs/tauprobe-recipe-attempt1.log`) failed to BUILD
    (rc 93: the compile line starts with the `ccache` launcher, so the include flag was injected in
    the wrong place). Script fixed, re-run as leg `tauprobe-recipe-r2`; no measurement was consumed.

## The gates (page §5.1)

| gate | predicted | measured |
|---|---|---|
| **G0** library identity | empty | **empty** (`git diff bb512d69 7187a255 -- include src`: 0 lines) |
| **G1** U0, head vs the toolchain record, exact | 0/0/0 | **27 × 75, 0 / 0 / 0** |
| **G2** E1 OFF vs the toolchain record (record's columns less `wall_s`) | 0 | **29 × 68, 0 differences** |
| **G3** Release suite at the head; Debug bench targets compile | green | **3230/3230**; Debug build rc 0 (`logs/chain-build.txt`) |
| **G-H1** `--hs` OFF identity, head vs base, base's 28 columns less the 5 wall columns | 0 | **walk 27 × 23, 0; ssn 27 × 23, 0** |
| **G-H2** non-vacuity under `--ssn-certify-from-face` | > 20 rows, each by OFF's `ssn_refinements` | **24 rows, all 24 exactly OFF's `ssn_refinements`** |
| **G-S** `farkas1e-4` OFF rows = head OFF rows (corpus-44, HS-27); τ recipe unpatched self-test | 0; byte-identical | **0 / 0; SELFTEST byte-identical**; the recipe's 10 and 1000 points also equal the scratch builds' in-tree probes byte for byte |

## The R fixture clauses (page §5.2) — VETO-CLASS, predicted → measured

Every row: `out/meas/scored.txt` (re-derived by `scripts/score_meas.py`).

| clause | R1 | R4 | R5 |
|---|---|---|---|
| **F-a** corpus-44 (`Optimal` kept) | PASS → **PASS** (44/44) | PASS → **PASS** (44/44) | PASS → **PASS** (44/44) |
| **F-b** HS-27 (`optimal` kept) | PASS → **PASS** (27/27) | PASS → **PASS** (27/27) | PASS → **PASS** (27/27) |
| **F-c** R5's preconditions | — | — | FAIL → **FAIL** (not built) |

Descriptive, predicted → measured:

| | OFF | R1 | R4 | R5 |
|---|---|---|---|---|
| corpus-44 Σ factorizations | 729 | → **729** (one row moves: `f7_n2000_path_corrupted`, residual digits and `ssn_prox_updates` 2 → 5) | → **729** (0 rows move) | → **666** (−63; 43 rows, each by exactly OFF's `ssn_refinements`) |
| HS-27 Σ factorizations | 1490 | → **1452** (14 rows move; all have OFF `ssn_prox_updates > 0`) | → **1514** (2 rows: HS10 166 → 184, HS15 37 → 43; both have OFF suspect exits) | → **1326** (−164; 24 rows, each exactly OFF's `ssn_refinements`) |
| HS-27 Σ `esc_infeasible_suspect` | 7 | 6 | **4** (3 refused) | 7 |

Every descriptive prediction held: R1 identical wherever OFF's `ssn_prox_updates` is 0; R4 identical
wherever OFF's `esc_infeasible_suspect` is 0; R5 moves only `factorizations`, by exactly OFF's
`ssn_refinements`, on both instruments. The corpus-44 carries no suspect exit under any arm (P7's two
corpus suspects are excluded cells, as the page said).

**Probe, R1 (descriptive, `lever-sigma 10000 20260807`):** the P7 record reproduces in kind — on
the ill-conditioned family `kResidualArmed` cuts factorizations 539 414 → 498 874 (−7.5 %) and
escapes 4 190 → 2 514; ≈ null on generic, degenerate and ill-cond + obj×1e6; `kResidualAlways`
dearer everywhere.

## The R4 1e-4 re-derivation (page §5.3)

| clause | predicted | measured |
|---|---|---|
| **E-a** probe false `kInfeasible` at 1e-4, three FP populations | 0 / 0 / 0 | **0 / 0 / 0** (refusals 61 / 165 / 1 548, identical at 1e-6 and 1e-4) |
| **E-b** probe recall 1e-4 ≥ 1e-6 | PASS | **PASS**: 55.7 % (5 567, 233 refusals) → **56.8 % (5 684, 0 refusals) = the symptoms arm** |
| **E-c** corpus-44 + HS-27 status kept, R4@1e-6 → R4@1e-4 | PASS | **PASS** |
| **verdict** | re-derived safe | **1e-4 RE-DERIVED SAFE on the false-positive axis** |

What the measurement adds, stated beside the verdict: at 1e-4 the gate **refuses nothing** on any
population this round has except the probe's three FP populations — the recall population, HS-27
and corpus-44 are byte-identical to the OFF (symptoms) arm (HS: the two rows 1e-6 moved, HS10 and
HS15, go back to OFF exactly; the three suspect exits 1e-6 refused are all certified at 1e-4). So
1e-4 keeps R4's false-positive elimination and gives back its whole recall/cost concession; whether
the three HS suspects it certifies are TRUE infeasible subproblems is not observable from an NLP
cell. The M11 A/B (`lever-farkas-m11`) is byte-identical at both tolerances (the `<b, y> < 0`
conjunct still refuses nothing the residual conjunct does not). The probe's recall counts reproduce
P7-B to the unit (5 684 / 5 567 / 233); its FP populations draw more cells and more symptom false
positives than P7-B's (47 / 118 / 203 vs 47 / 16 / 24) — the hven probe has evolved since; the
FP-elimination reading is unchanged.

## The τ fixture clauses (page §5.4) — VETO-CLASS

| clause | T10 | T1e3 |
|---|---|---|
| **T-a** U0 walk | PASS → **PASS** (2 rows move, residual digits only) | PASS → **PASS** (0 rows move) |
| **T-b** HS-27 walk | PASS → **PASS** (HS6, HS7, HS12, HS43 move; all optimal) | PASS → **PASS** (0 rows move) |
| **T-c** probe `all_optimal` | PASS → **PASS** | PASS → **PASS** |

**Probe reproduction (descriptive):** the TAU §4.4 table reproduces EXACTLY on the new toolchain —
majors identical over τ̄ ∈ {10, 100, 1e3, 1e5}; τ̄ = 1: HS26 88 → 97 majors, 190 → 220 minors,
4 rejects; HS40 minors 59 → 62, 2 rejects; every problem `all_optimal = 1` at every point
(`out/meas/tauprobe-*.txt`).

## TRAIN (page §5.5) — the A4 rule verbatim

**Determinism: every label, both families, `N / N / 0`** (21/21/0 `hs`, 23/23/0 `cutest`; 14
compares, `out/train/determinism-*.txt`). **Dumps:** each R label differs from `sqp-ssn` by exactly
its one line; base-vs-head `sqp-ssn` 0 lines; τ arms vs `default` 0 lines (as the page said; the arm
is its pin). **C-1 (gate) PASS:** base `sqp-ssn` run A reproduces head's cell for cell, **0 of 21
`hs`, 0 of 23 `cutest`**. **C-2 (descriptive):** head `default` cutest run A vs RETAKE19: **0 of 23**
differ. All rc=1 legs are reference failures or `error` outcomes (hs025, hs033 local minimum;
BRITGAS/STEENBRB/STEENBRC/YORKNET under `sqp-ssn`); 0 refusals.

The decision (`scripts/w2_a4_decide.py`, unmodified, run A; `out/train/decisions-{r,tau}.txt`):

| family | label vs D | optimal | dnf_wall | error | \|B\| | m(X) vs m(D) | verdict | predicted |
|---|---|---:|---:|---:|---:|---|---|---|
| `hs` | `sqp-ssn-sigma-armed` vs `sqp-ssn` | 21 | 0 | 0 | 21 | 6.6667 = 6.6667 | **TIE** | TIE |
| `cutest` | `sqp-ssn-sigma-armed` vs `sqp-ssn` | 14 | 5 | 4 | 14 | 80.3043 = 80.3043 | **TIE** | TIE |
| `hs` | `sqp-ssn-farkas` vs `sqp-ssn` | 21 | 0 | 0 | 21 | 6.6667 = 6.6667 | **TIE** | TIE |
| `cutest` | `sqp-ssn-farkas` vs `sqp-ssn` | 14 | 5 | 4 | 14 | 80.4918 > 80.3043 | **lose: cost** | TIE or lose |
| `hs` | `sqp-ssn-certify-face` vs `sqp-ssn` | 21 | 0 | 0 | 21 | **6 < 6.6667** | **WIN** | WIN |
| `cutest` | `sqp-ssn-certify-face` vs `sqp-ssn` | 14 | 5 | 4 | 14 | **79.7065 < 80.3043** | **WIN** | WIN |
| `hs` | T10 vs `default` | 21 | 0 | 0 | 21 | 1 = 1 | **TIE** | TIE or lose |
| `cutest` | T10 vs `default` | 14 | 7 | 2 | 14 | — | **lose: errors 2 > 1** | TIE or lose |
| `hs` | T1e3 vs `default` | 21 | 0 | 0 | 21 | 1 = 1 | **TIE** | TIE |
| `cutest` | T1e3 vs `default` | 16 | 6 | 1 | 15 | 148.7 > 139.957 | **lose: cost** | TIE (**WRONG**) |

(`sqp-ssn` D: `hs` 21 optimal; `cutest` 14 optimal, 5 dnf_wall, 4 error. `default` D: `cutest` 16
optimal, 6 dnf_wall, 1 error.)

Per cell (run A; `out/train/tree-*`):

* **R1** moves 9 `hs` cells (8 cheaper; HS43 54 → 38) and 10 `cutest` cells both ways: ACOPP118
  1 753 → 6 425, DALLASM majors 18 → 47, STEENBRG 35 873 → 32 371. The median does not move on
  either family. The ladder arms on TRAIN, so W3-R4's precondition ("a population that exhibits the
  geometry") is met in the weak sense; nothing here says which (c, exponent) would win.
* **R4** moves one cell per family (HS10 166 → 184; READING6 600 → 603): the refusals continue to
  the budget, which is the cost the record named.
* **R5** moves 18 `hs` and 10 `cutest` cells, every one cheaper and every outcome unchanged; one
  objective moves in its 9th digit (ACOPP118 129660.68619 → 129660.68591), the only non-factorization
  movement seen anywhere for R5.
* **T10** on `cutest`: ACOPR118 `optimal` → `dnf_wall`, NET3 `optimal` → `error` (185 majors);
  ACOPR57 and SAWPATH cheaper.
* **T1e3** on `cutest`: **BRITGAS `dnf_wall` → `optimal`** (53 majors, 54 574 factorizations, inside
  the 300 s ceiling — the cell `default` misses only on the wall), NET3 `optimal` → `dnf_wall`,
  ACOPR118/ACOPR57 dearer (14 802 → 36 871; 1 618 → 2 974), YORKNET still `error`.

## What the pre-registered reading decides (page §5.6)

| arm | (1) TRAIN | (2) veto-class | (3) determinism | (4) C-1 / pin | reading | predicted |
|---|---|---|---|---|---|---|
| R1 `kResidualArmed` | TIE / TIE | PASS | PASS | PASS | **not recommended** — `kLadder` stays | not recommended |
| R4 `kFarkasGated` | TIE / lose | PASS | PASS | PASS | **not recommended** — `kSymptoms` stays; 1e-4 re-derived safe | not recommended |
| R5 `ssn_certify_from_face` | **WIN / WIN** | **FAIL (F-c)** | PASS | PASS | **not recommended this round** — precondition unmet; `false` stays | WIN, blocked by F-c |
| τ T10 | TIE / lose | PASS | PASS | PASS | not qualifying | not qualifying |
| τ T1e3 | TIE / lose | PASS | PASS | PASS | not qualifying | not qualifying |

* **R1, R4: the shipped OFF defaults stand**, in the rule's words: no WIN.
* **R5 is the one lever with a TRAIN win over its OFF state on both families**, with every
  certificate outcome unchanged on TRAIN, corpus-44 and HS-27. Its default flip remains blocked by
  the two preconditions (W3-R1). Building them is the only thing between R5 and a flip proposal.
* **τ: the published value is NOT wrong for this population.** Neither arm qualifies; no setting
  lands; `kFunnelTauBar = 100` stays a `constexpr`. One fact for the record beside it: τ̄ = 1000
  converts BRITGAS inside the ceiling (W3-R5).
* **No default moves in this round.** No fixture veto fired (F-a/F-b/T-a/T-b/T-c all PASS); F-c is a
  declared precondition FAIL.

## Prediction misses

* T1e3 `cutest`: predicted TIE, measured **lose: cost** (and a BRITGAS conversion).
* Wall budget per corpus-44 leg: ~2 min was an estimate from committed walls (the page gives only the 300 s per-cell cut, §5.2), measured ~8 min (no clause depends on it).
* Descriptive miss (nothing is decided on it): page §5.3 predicted `esc_infeasible_suspect` = 0 on every HS-27 row; at OFF the measured sum is 7. The corpus-44 half of that prediction held.
* The probe's FP population counts differ from P7-B's (the evolved probe), not the reading.

## What is here

* `corpus44-cells.txt` (part of the pre-registration).
* `scripts/` — every driver as it ran (`scripts/tau_recipe.sh` is committed in its fixed (r2) form; attempt 1's script version is not committed, only its log; `chain_build.sh`, `chain_meas.sh`, `tau_recipe.sh`,
  `train_chain.sh`, `master.sh`, `leg.sh`, `build.sh`, `train_close.py` adapted from A3's,
  `score_meas.py`, `compare_cols.py`, `set_pin.py`, `edit_lane.py`, `preview_a.py` — the last only
  PREVIEWED run A during the chain; the close is `train_close.py`).
* `patches/` — the three hven scratch commits and the four hven-bench clone commits.
* `out/meas/` — every in-tree CSV and probe output; `scored.txt`. Note: `scored.txt`'s recall line contains an FP tuple (5567, 7); that is a parser artefact of `scripts/score_meas.py:527` misreading `fact(diag)` 7.58. The quoted 233 refusals come from the raw probe text (`probe-head-lever-farkas.txt`). Neither file is edited.
* `out/train/` — `tree-r/` (D = `sqp-ssn` in the `default` slot), `tree-tau/` (D = `default`; slots
  `tau10`, `tau1e3`), `controls/`, determinism, decisions, C-1/C-2, option dumps, chain summary and
  every leg log.
* `logs/` — build and in-tree chain transcripts, the suite log, the recipe's two attempts.
