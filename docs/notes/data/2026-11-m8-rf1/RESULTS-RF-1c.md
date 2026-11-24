# RESULTS-RF-1c -- the restoration stall window's pass floor and per-run window (DECLARATION-RF-1c.md, -r2)

Arms: R = origin/m8 `976739bc6`, H = `ff8a8f2c3` (the change on R). Declaration `a867f62d3` and the binaries block of `BINARIES.txt`
(`e42bad9ff`) were committed before the first leg; `DECLARATION-RF-1c-r2.md` (`73ba2070c`) before the r2 mutation leg. Evidence in
`gate-RF-1c/`. Counters, statuses and objectives only; no wall-clock claim. Toolchain: clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64
(AMD Ryzen 7 5800X3D), MKL, `MKL_NUM_THREADS=1`; legs under `box-lane` (never `--exclusive`). **No option was added** (K is the constant
`kRestorationStallMinPasses`). Every bar of the declaration is met; the CI row is at the end.

## The change

`RestorationStallWindow` tests only when at least six passes are on record (the reference sample is at least five passes back) and has
`new_run()` (clears the samples, keeps the running best), called by `enter_restoration` before every phase run after the first. Tests
re-pinned as declared (section 3 of the declaration): `Rf1RestorationStall.*` (two re-pinned, two new), and two `Rf1RestorationSeam` tests whose
five-pass circle/line phase can no longer be cut by a window of 0/1 (one now cuts the bouncing spread phase; one became
`APhaseShorterThanThePassFloorIsNotCutAtAnyWindow`); new seam test `ARestartNeedsTheFloorsPassesOfItsOwn`.

## Legs

| leg | result |
|---|---|
| C | ctest Release **4109/4109**, Debug **4109/4109** (`suites/`), at head `a867f62d3` (code `ff8a8f2c3`). All `Rf1RestorationSeam.*` and RF-1b's `TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint` pass |
| N | counter replay vs origin/m8 (walk, ssn, ipm; base once, head x3; byte-exact `compare_replay.py`): **9 comparisons, 0 differences** (27 cells x 75 columns each). No replay cell reaches the changed path |
| M | `VERDICT MUTATIONS_AS_DECLARED` (`mutation-r2/`): control clean; **M1 killed** by 6 tests (`OnePassCostingMoreThanTheWindowIsNotAStall` among them); **M2 killed** by `Rf1RestorationSeam.ARestartNeedsTheFloorsPassesOfItsOwn`; R1 (new_run also forgets the best) survives, as declared (reported). The first mutation attempt (`mutation/`, `mutations.out`) stopped at `PREFLIGHT_DOES_NOT_APPLY M2`: the declared anchor had two spaces before a comment that `clang-format` made one; corrected in `-r2` (anchor text only), no mutation having been applied |
| T | TRAIN, the 119 hven-sqp roster cells x {D, K} (238 rows per arm): **0 status movers, 0 lost solves, 0 missing; factorizations 200759 -> 200759 (+0), iterations 4598 -> 4598**; 210/238 solved in both. `VERDICT TRAIN_BARS_MET`. **HS99EXP.D and HS99EXP.K are unchanged** (stalled at 50 majors, 6543 and 7013 factorizations in both arms: their window cut is a genuine flat phase, 131/120 passes, far past the floor). 10 rows `infeasible` / 8 certified in both arms, the same rows: no false certificate |
| P | the 62-cell pilot: **solved R 19 -> H 20, 0 lost, 0 threw, 0 false certificates** (no cell certifies infeasible in either arm). `VERDICT PILOT_BARS_MET`. Movers below |
| U | TU-2 cutest cells, 48 cells per arm: **solved R 12 -> H 26, 0 lost, no false certificate**. Movers below. `VERDICT TU2_NO_LOSS_NO_FALSE_CERT` |
| S | install smoke and export contract, default and clang front ends, rc 0 |

### Pilot movers (R -> H: status / majors / factorizations)

BRITGAS (cells 019-027): 019 unchanged (max_iter / 1 / 10055, both).

| cell | R | H |
|---|---|---|
| 020, 021 | stalled / 1 / 16212 | max_iter / 1 / 18227 |
| 022, 023 | stalled / 1 / 18159 | numerical_error / 50 / 178441 |
| 024 | stalled / 1 / 25937 | numerical_error / 16 / 68400 |
| 025, 026 | stalled / 1 / 19967 | numerical_error / 16 / 75120 |
| 027 (`default+qp.working_set_algebra=kRefactorize`) | stalled / 1 / 25373 | **optimal / 87 / 204501** |

YORKNET (cells 054-061 and neighbours): 053 and 054 unchanged.

| cell | R | H |
|---|---|---|
| 050 (default) | numerical_error / 6 / 14137 | numerical_error / 12 / 32503 |
| 051 | numerical_error / 11 / 19913 | numerical_error / 36 / 46433 |
| 052 | stalled / 27 / 15262 | numerical_error / 31 / 18878 |
| 055 (kTier) | stalled / 72 / 48964 | numerical_error / 80 / 51053 |
| 056, 057 (kTier + growth) | max_iter / 153 / 43181, 43692 | numerical_error / 16 / 46808, 46779 |
| 058 | numerical_error / 12 / 19904 | numerical_error / 9 / 27593 |
| 059, 060, 061 | numerical_error / 53 / 20415-20433 | numerical_error / 55 / 22747-22765 |
| 062 | stalled / 34 / 20241 | numerical_error / 53 / 40260 |

The BRITGAS grind cells that were stalled at major 1 at R do not become solved (other than 027): the window no longer cuts at major 1, and they
now run on to `numerical_error` or `max_iter`. None of them was solved at R, so no bar moves; the honest reading is that the window's early cut at
R was masking a different failure on those labels (a `numerical_error` after 16-50 majors), not that the fix lost anything. YORKNET's cells
likewise run on past the old cut (YORKNET default: 6 -> 12 majors, as the prototype); none solves in either arm. Cells 056 and 057 end earlier
in majors (153 -> 16) but at `numerical_error` instead of `max_iter`; neither arm solves them.

### TU-2 cells (BRITGAS, NET3, SAWPATH, YORKNET x {default, sqp-qn} x A0-A5)

18 of 48 cells identical. Movers (all arms A0-A5 listed where they share the reading):

| problem / label | R | H |
|---|---|---|
| BRITGAS default A0, A1 | stalled / 1 / 19967, 20376 | **optimal** / 50 / 74805; 45 / 65995 |
| BRITGAS default A2, A3 | stalled / 1 / 19967 | numerical_error / 16 / 75120 |
| BRITGAS default A4, A5 | stalled / 1 / 25937 | numerical_error / 16 / 68400 |
| NET3 default A0-A5 | stalled / 1 / 13749 | **optimal** / 13 / 30607 (A0, A2, A3); 36 / 36799 (A1, A4, A5) |
| SAWPATH sqp-qn A0-A5 | stalled / 16 / 19847 | **optimal** / 44 / 47406 (A0, A2, A3); 52-53 / 82381 (A1, A4, A5) |
| YORKNET default A0 | numerical_error / 6 / 14137 | numerical_error / 12 / 32503 |
| YORKNET default A1 | numerical_error / 12 / 19904 | numerical_error / 9 / 27593 |
| YORKNET default A2-A5 | numerical_error / 53 / 20415-20433 | numerical_error / 55 / 22747-22765 |
| YORKNET sqp-qn A0, A2, A3 | stalled / 1 / 15801 | numerical_error / 1 / 18809 |
| YORKNET sqp-qn A1 | numerical_error / 2 / 31417 | max_iter / 7 / 64466 |
| YORKNET sqp-qn A4 | numerical_error / 2 / 31417 | max_iter / 32 / 74975 |
| YORKNET sqp-qn A5 | numerical_error / 2 / 31417 | numerical_error / 24 / 74384 |

BRITGAS/NET3/SAWPATH/YORKNET default-A0 read as the prototype's (BRITGAS 50 / 74805, NET3 13 / 30607, SAWPATH sqp-qn 44 / 47406, YORKNET
numerical_error at 12 majors); ACOPR118 is in the pilot and unchanged. The 18 identical cells are the sqp-qn BRITGAS and NET3 cells and the default SAWPATH cells (none reaches the changed window
behaviour).

## Cost growth on the cells the window exists to cut (leg G)

- **TRAIN**: none. No TRAIN row changes by one factorization or iteration (200759 and 4598 in both arms); the floor lengthens no TRAIN
  phase, because every window cut on TRAIN (HS99EXP among them) is already past six passes.
- **Pilot and TU-2 cells that R cut as `stalled`**: H/R factorizations 2.2-4.2x on the recovered cells (BRITGAS default 3.2-3.7x to optimal,
  NET3 2.2-2.7x, SAWPATH 2.4-4.2x), 1.1-2.4x on YORKNET, up to 9.8x on BRITGAS pilot 022/023 (the run continues to numerical_error at 50
  majors). This is the cost of letting a phase the old window truncated go on, not the cost of the floor on a genuine grind: on the
  recovered cells the R figure is a truncated run (it ended at major 1 or 16, at 13k-26k factorizations), so the ratio compares a cut run to a
  finished one. The one cell family where the floor itself is the only difference is the flat grind, and none of those exists in the
  recorded sets (HS99EXP, the one flat TRAIN grind, is unchanged).

## Deviations from the declaration

1. The leg M anchor for M2 (above); answered by `DECLARATION-RF-1c-r2.md` before the corrected leg.
2. The pilot/TU-2/TRAIN legs ran co-run on one lane (pinned cores, declared); no pilot or TU-2 cell ended at the 900 s kill (all 220 rows rc=0) and no TRAIN row moved, so no
   solo re-run was needed.
3. The Windows CI lane failed in its `Install LLVM` step on the first dispatch (runner setup; no hven code ran); see the CI row.

## CI

`gh workflow run ci.yml --ref rf1c`, run 45329417735 at head `73ba2070c` (code `ff8a8f2c3`): linux-clang-release, linux-clang-coverage and
macos-clang-release green on the first pass; windows-clang-release failed in its `Install LLVM` step (runner setup, before any hven code ran) and
was re-run (`gh run rerun --failed`): green. **All four lanes green.** The macOS lane passes the re-pinned seam tests unchanged
(no Accelerate-specific value is claimed or pinned by RF-1c); nothing UNOBSERVED.

---

# Addendum (review riders; DECLARATION-RF-1c-r3.md, -r4.md)

Appended after the independent review (SHIP AFTER FIXES) and the riders it asked for. Everything above is unchanged. The riders'
commits are those of `git log 22c9b6879..`; `git diff 22c9b6879 -- include src` is comments and the registry description
string only (no non-comment production line moved, so the counter replay, TRAIN, pilot and TU-2 R/H legs are not re-run; their evidence stands).
Mutation, ctest and probe legs under `box-lane`, counters and statuses only.

## Tests

- `Rf1RestorationStall.ANewRunKeepsTheBestAndForgetsTheSamples` gains the keeps-best case (best 1.0, `new_run()`, six passes at cost 100 against a
  window of 10 falling 5.0 -> 2.0: cut on the sixth with the best kept, not cut by a window that never saw the 1.0).
- `Rf1RestorationSeam.ARestartNeedsTheFloorsPassesOfItsOwn` counts the restart's own rows (after the first run's failed-subproblem row) and
  asserts exactly 6 (measured 6 with the fix, 3 without `new_run()`; the first run's 13 rows play no part).
- New `Rf1RestorationSeam.AFallingPhaseSurvivesItsWindowByTheFallTest` (LinearRowModel, trust radius 0.25, window 1: eight passes, h 20 -> 0, not
  cut, solve equal to the one under an unbounded window). The first version used radius 1.0, whose phase ends on its sixth pass, and survived the
  fall-test mutant (see the mutation table); radius 0.25 fixes that (radius 0.02 gives eleven passes that fall under 10% per five, rightly cut).

## Mutation legs (r3 and r4)

| leg | result |
|---|---|
| r3 (`mutation-r3/leg.out`, 2026-11-24 01:46:59 -> 01:59:18) | `VERDICT MUTATIONS_AS_DECLARED`. control clean. **M1** killed (7 tests). **M2** killed by exactly the tightened `ARestartNeedsTheFloorsPassesOfItsOwn`. **R1** (new_run also forgets the best; r2: killers empty) now killed by exactly `Rf1RestorationStall.ANewRunKeepsTheBestAndForgetsTheSamples`. **M3** (fall test dropped, added in r3) killed by four tests, but **not by its declared killer** (`AFallingPhase...`: its phase ended on the sixth pass) |
| r4 (`mutation-r4/leg.out`, 02:00:40 -> 02:06:24) | M3-only leg after the fixture fix: control clean, **M3 killed**, now including by `AFallingPhaseSurvivesItsWindowByTheFallTest` (5 killers). `VERDICT MUTATIONS_AS_DECLARED` |

The r2 leg's stdout was found at `.scratch/rf1c-build-legs/mut2.out` and is added as `mutation-r2/leg.out`.

## ctest

Release **4110/4110**, Debug **4110/4110** (`suites-r3/`; 4109 + the one new test), at head `fd59231d0`.

## (a) Aggregate cost (recomputed from the cell files)

| set | cells | factorizations R | factorizations H | ratio | solved R -> H |
|---|---|---|---|---|---|
| pilot | 62 | 758240 | 1506740 | **1.99x** | 19 -> 20 (+1) |
| TU-2 | 48 | 722494 | 1572167 | **2.18x** | 12 -> 26 (+14) |

The fix buys its solves with roughly a doubling of the factorizations on these sets (the ratio is not per solved cell: most of the added work is
runs that R truncated going on to a non-solved end, e.g. pilot 022/023).

## (b) The label shift

Pilot status counts R -> H: `stalled` **14 -> 3**, `numerical_error` **19 -> 29**, `optimal` 8 -> 9, `acceptable` 11 -> 11, `max_iter` 10 -> 10. Eleven fewer
cells end `stalled`: cells that R reported `stalled` end as `numerical_error` (or `max_iter` / `optimal`): the change turns a window cut into the next failure the run
reaches. A consumer that keys on `stalled` sees it vanish on these cells. (TU-2: `stalled` 21 -> 0, `numerical_error` 12 -> 17, `optimal` 12 -> 26,
`max_iter` 3 -> 5.)

## (c) Pilot 056/057 end earlier at a worse objective

YORKNET kTier + growth: R `max_iter` / 153 majors / 43181 and 43692 factorizations, f = 14527.97 and 14531.80; H `numerical_error` / 16 majors /
46808 and 46779 factorizations, f = 14863.17 on both. Fewer majors and a worse (+2.3%) objective; neither arm solves them. The earlier summary
("end earlier in majors") undersold that H's endpoint is also worse.

## (d) The floor's own cost on an expensive-pass flat grind is unmeasured

No recorded cell is an expensive-pass genuine flat grind that the window should cut: TRAIN reaches a window verdict only on HS99EXP (D and K),
whose passes are cheap and which is far past six passes, so **0 movers there shows no collateral damage, not the floor's cost**. The floor lengthens
a flat phase to at least six passes, so its cost on such a phase scales with the pass cost times up to five extra passes; nothing here measures it.

## (e) The NET3 restart's dependence on keeping the best, measured (leg V, `r1probe/`)

The declaration asserted it. Arm P = the committed code with mutation R1 (`new_run()` also resets the best) built as a local-only commit
(`legs-rf1c/rf1c_r1probe.sh`), the 48 TU-2 cells run on the four pinned physical cores of one lane (co-run protocol, counters only):
**P versus H: 48 of 48 cells identical (status, majors, factorizations, objective), 0 movers; P versus R: the same 30 movers as H.** On
these cells, **forgetting the best changes nothing**, NET3/default included (optimal / 13 / 30607 and 36 / 36799, as H). The claim that the NET3
restart "depends on" keeping the best is **not borne out** by the TU-2 cells: the property is argued (a restart whose own incumbent starts higher
should not undo progress), pinned at the window-object level, and unmeasured as a solver-level effect. The per-run fresh window is what NET3
needed.

## (f) Pilot 022/023 (9.8x)

R: `stalled` / 1 major / 18159 factorizations; H: `numerical_error` / 50 majors / 178441. H's row: 7 restoration entries (3 reduction resumes,
143 restoration iterations), the last phase ending at h_out = 1.3e-9 (`restoration_sub_status` optimal) with `kkt_stationarity` = 1.25 at the
exit: a repeated restoration-success / return cycle that never converges the main problem. The fix exposes that cycle (R's early cut ended the
run before it began); it does not cause it. It is a separate defect on this label (kTier + kAnyAccepted).

## Nits

`legs-rf1c/rf1c_check_r3.py` and `tu2_report_r3.py` (copies; originals unedited) iterate the union of the R and H cells and fail on a cell
missing from either; `tu2_report_r3.py` fails its verdict on `THREW_OR_RC`. Re-run on the existing evidence: pilot `PILOT_BARS_MET` (SOLVED R 19
NEW 20) and TU-2 `TU2_NO_LOSS_NO_FALSE_CERT`, output byte-identical to the originals (`diff` empty).

## (g) CI

CI_PLACEHOLDER
