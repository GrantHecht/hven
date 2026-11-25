# DECLARATION-RF-1c -- the restoration stall window's pass floor and per-run window (successor to RF-1b; `DECLARATION-RF-1b.md` kept unedited)

Written and committed, together with every leg script it names (`legs-rf1c/`), before any leg; never edited afterwards (a failed or void
leg chain is answered by a `-r2` file, this one kept). Criteria are never changed after data. Precedent: `DECLARATION-RF-1b.md`.

## 1. The defect and the change

Since RF-1 (merge `641dedeab`) BRITGAS/default and NET3/default, optimal before it, end `stalled` / `restoration_work_limit` at major 1.
Diagnosis (verified by bisect and a knob test): `RestorationStallWindow::observe` (`include/hven/detail/sqp/globalization/restoration.h`) is
called once per restoration pass with the phase-cumulative factorization count. When one pass costs more than the window W
(`advanced.restoration_stall_factorizations`, 5000) the window spans a single major and fires; and a regularized restart inherits the failed
run's factorizations (one window object serves the phase's runs), so on NET3 it fires before the restart has taken a step.

Change (`ff8a8f2c3`, code head; the prototype was `.scratch/regr-diag/fix2.patch`, measured on `fd1e318b5`):

- (a) the reference sample must also lie at least `kRestorationStallMinPasses` = 5 passes back (`samples_.size() > K` to test, `> K + 1` to
  slide), so a stall verdict rests on at least six observed passes;
- (b) `RestorationStallWindow::new_run()` clears the samples and keeps the running best; `enter_restoration` calls it before every phase run
  after the first.

**K is a constant, not an option** (`kRestorationStallMinPasses`, restoration.h). No option is added: the options registry, the generated
options reference, the dump fixtures and the sizeof pins are untouched (the one header comment on `restoration_stall_factorizations` in
`sqp_options.h` states the new rule; the registry's one-line description, a generated artifact, is left as is: it still states the window's
10%-over-W rule, to which the floor adds). A tunable K would be a second knob on a window whose only role is to cut a flat grind; the
measured failure is one constant's absence, not a regime that needs tuning.

Prototype results (fd1e318b5, as a development reading, not a bar): BRITGAS/default optimal 50 majors / 74805 factorizations; NET3/default
optimal 13 / 30607; SAWPATH/sqp-qn optimal 44 / 47406; YORKNET/default numerical_error at 12 majors (previously 6); ACOPR118/sqp-qn
unchanged.

## 2. Arms, binaries, legs and bars

Arms: **R** = origin/m8 `976739bc6f10a314a219fb35ec7c656c5aff8f76` (reference); **H** = `ff8a8f2c3` (the change on R; `git diff` of
`src include tests` between H and this file's commit is empty). Binaries are built by `legs-rf1c/lane_build.sh` (TRAIN lane, NEVER=1,
hven-bench private clone at `e03cbad`), `pilot_build.sh` (the TU-1 DIAGQP harness copy, rebuilt by `harness_rebuild.sh` only if swept) and
`tu2_build.sh` (the TU-2 harness, hven-bench-tu2 `c3d176e`, as TU-2 ran it), all linking one hven install per arm (the TRAIN lane's, built
by `build_hven_pin.sh`: Release, sequential MKL, no LTO). Frozen in `BINARIES.txt` (an RF1C block, committed BEFORE the first leg,
sha-checked by `rf1c_corun.sh` and `rf1c_tu2.sh`). Toolchain and machine as RF-1b (clang, `-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64, AMD Ryzen
7 5800X3D, MKL, `MKL_NUM_THREADS=1`). Every step under `~/.local/bin/box-lane --` (never `--exclusive`; no leg asserts wall-clock; counters,
statuses and objectives only). Evidence in `gate-RF-1c/`.

| leg | instrument (in `legs-rf1c/`) | bar |
|---|---|---|
| C | `rf1c_chain.sh`: `ctest.sh` rel and dbg at the committed head | "100% tests passed" in both logs; counts reported. All `Rf1RestorationSeam.*` (M11/M12/M13/M19/M20 among them, as re-pinned below) and RF-1b's `TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint` stay green |
| N | `rf1c_chain.sh` -> `replay.sh`: corpus binaries from R and H, base once per mode, head three times per mode, `scripts/compare_replay.py` byte-exact, modes walk/ssn/ipm | **0 differences expected unless restoration is reached**; every mover is reported with its cell, the mode and the restoration counters. A difference in a cell that never enters restoration is a failure |
| M | `rf1c_chain.sh` -> `mutations_rf1c.sh` with `mutate_rf1c.py`; control first | control clean; **M1 and M2 killed**; R1 reported: `VERDICT MUTATIONS_AS_DECLARED` |
| T | `rf1c_corun.sh`: `train_enum.sh` (the 119 hven-sqp roster cells x {D, K}: 238 rows per arm, CAP 300 s, one pinned core) on lane R and lane H; `ab2_train.py`; `rf1c_check.py train` | **0 lost solves versus R and 0 false infeasibility certificates**: `VERDICT TRAIN_BARS_MET`. The hven-ipm rows are not run: the stall window is `detail/sqp` only, no ipm TU includes it. HS99EXP.D and HS99EXP.K stall through the window today and may move; reported as a mover whichever way. HOLDOUT never |
| P | `rf1c_corun.sh`: `pilot_run_r1.sh` R and H (the 62 cells of `pilot_cells.tsv`; budget 300 iterations, 900 s kill reported, one pinned core each); `pilot_report.py`; `rf1c_check.py pilot` | **0 cells lost versus R, no cell throws, no false certificate on ANY cell** (H certified infeasible where R is not): `VERDICT PILOT_BARS_MET`. Reported: solved counts, statuses, factorizations of every MOVER, BRITGAS cells 019-027 and YORKNET 054-061 by row |
| U | `rf1c_tu2.sh`: BRITGAS, NET3, SAWPATH, YORKNET x labels {default, sqp-qn} x TU-2's arms A0-A5 (48 cells per arm, `--budget-facts 250000`, `--budget-iters 300`), R and H co-run, `tu2_report.py` | **0 lost solves, no false certificate**; every mover reported. The four cells named in section 1 (BRITGAS/default, NET3/default, SAWPATH/sqp-qn, YORKNET/default at A0) are expected to read as the prototype's, as a reading, not a bar |
| G | cost growth: H/R factorizations on every pilot and TU-2 cell where R ended `stalled`/`restoration_work_limit` (the cells the window exists to cut, and R's bug cut early), and on every TRAIN row where H's restoration work exceeds R's | reported only; the floor lengthens a flat phase to at least six passes, so the growth on a genuinely flat grind is expected and is what is measured here |
| S | `rf1c_chain.sh` -> `install_smoke.sh`: install smoke and export contract on both front ends | rc 0 on both |
| CI | `gh workflow run ci.yml --ref rf1c` at the pushed head, `gh run watch <id> --interval 120` (never shorter; a 403 waits for the reset) | green on all four lanes; an off-reference (Accelerate/Windows) pin that differs is recorded as observed, never filled in; Apple values not observed are UNOBSERVED |

Co-run protocol (CLAUDE.md section 7): legs T, P and U co-run as one process per pinned PHYSICAL core of one box lane (SMT siblings idle),
`MKL_NUM_THREADS=1` in every process, two arms at once; the columns asserted are counters, statuses and objectives, wall-clock stays
informational and is never quoted. A cell that ends CAPPED (the 300 s watchdog) or at a 900 s kill, or any deviation that decides a bar, is a
candidate only: it is re-run ALONE under the serial rule before it is called a regression.

## 3. Tests, must-kills and the declared break

- **M1** (the pass floor dropped: `kRestorationStallMinPasses = 0`, which restores the pre-RF-1c window exactly) must be killed by
  `Rf1RestorationStall.OnePassCostingMoreThanTheWindowIsNotAStall` (one pass costs 6000 factorizations against a window of 5000: the second
  observation of a flat best must not fire) and by the re-pinned `Rf1RestorationSeam.APhaseShorterThanThePassFloorIsNotCutAtAnyWindow`.
- **M2** (the `phase_stall.new_run()` call dropped) must be killed by `Rf1RestorationSeam.ARestartNeedsTheFloorsPassesOfItsOwn`, a seam
  fixture: the spread phase's first run takes twelve walk subproblems and fails the next (`pass_restoration_walk_qps = 12`,
  `fail_restoration_walk_qps = 1`: the existing RF-1b knobs, no new knob), the curvature switch starts the regularized restart, and with a
  window of one factorization only the pass floor holds the phase open. Measured by the author before this file: 19 phase rows with the fresh
  window, 16 without; the test asserts at least `12 + 6`. The fixture's restart is flat once it reaches the bounce, not progressing: it pins
  that the restart gets five passes of its own, which is the property the NET3 restart needs; `Rf1RestorationStall.ANewRunKeepsTheBestAndForgetsTheSamples`
  pins the window object's side, including a restart that falls.
- **R1** (reported, not asserted): `new_run` also forgets the running best. The unit tests are not expected to kill it; it is listed so the
  "keeps the running best" half of (b) is visibly an argued property, the one the NET3 restart (whose own incumbent starts higher) depends on.
- **The declared intentional break.** The `Rf1RestorationStall` unit tests pinned a flat best cut at the window and a zero window cutting at the
  first observation: a flat best is now cut at the window AND after five passes behind the reference. They are re-pinned
  (`AFlatBestIsCutAtTheWindowAndTheFloorAndAFallingOneIsNot`, `TheBestIsARunningMinimumAndTheWindowSlides`, with the two new tests). Two
  `Rf1RestorationSeam` tests used the five-pass circle/line phase with a window of 0/1 and could no longer cut it: `AStalledPhaseIsTheRestorationWorkLimit`
  now cuts the bouncing spread phase (the fixture of `TheStallExitTakesTheReductionResume`, window 2, reduction resume off), and
  `APhaseFallingTenPercentPerWindowIsNotCutAndAFlatterOneIs` becomes `APhaseShorterThanThePassFloorIsNotCutAtAnyWindow` (the window's fall test
  itself being pinned on the window object). The old Accelerate-only branch of that test (observed on a Mac, CI run 45036848180) is gone with the
  test; the new one makes no Accelerate-specific claim, and the Mac lane is what shows whether a five-pass phase stays five passes there.

## 4. Not run

HOLDOUT (never). Wall-clock. The QP walk census. The hven-ipm TRAIN rows (no code path). Accelerate and Windows readings (CI only).
