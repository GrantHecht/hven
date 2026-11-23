# RESULTS-RF-1b -- the curvature-switch run's switch exit and YORKNET 055-057 (DECLARATION-RF-1b.md)

Arms: R = m8 `641dedeab`, H = `ecda15017` (the RF-1b change on R). Declaration `6dea31140` and the binaries block of `BINARIES.txt`
(`5a977014b`) were committed before the first leg; evidence in `gate-RF-1b/`. Counters only; no wall-clock claim. Toolchain: clang 22.1.8,
`-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64 (AMD Ryzen 7 5800X3D), `MKL_NUM_THREADS=1`; legs under `box-lane` (never `--exclusive`). Every bar
of the declaration is met. The pilot harness (swept) was recreated by `legs-rf1b/harness_rebuild.sh` (see "Pilot harness" below).

## The change (`ecda15017`), and one deviation from the ruling's letter

`best_applies` is false for a run told to switch (`stop_phase_on_qp_failure_`) that took the switch (`restoration_curvature_switches > 0`), so the
regularized restart starts from the switch point as before RF-1. This is narrower than "`return_best = kNever` on the switch run" (ruling C481): under
the shipped `kModelThenRegularized` every exact-Hessian phase's first run is a "switch run", and a blanket `kNever` would turn design 4.2 off for phases
whose first run ends at a limit or stall without switching (pinned by `Rf1RestorationSeam.ThePhaseHandsBackItsBestPointNotItsLast`). The two readings
coincide on 055-057 (the exit there is the switch at phase major 23) -- and the throwaway build's numbers are reproduced exactly (below). The literal
form is a one-line change if the settler wants it, with its own gate. Declared in `DECLARATION-RF-1b.md` section 1.

## Must-kill and mutations (leg M, `gate-RF-1b/mutations.out`)

Must-kill `Rf1RestorationSeam.TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint` (new seam knob `pass_restoration_walk_qps`, the companion of
`fail_restoration_walk_qps`; the fixture is `LinearRowModel`: the phase takes two steps, h 19 then 17, its third subproblem fails, the capture limit makes
the restoration order hold the h = 19 point). Measured failure with the guard dropped (the author's pre-declaration build and the leg's M22 build):
the restart's first depth-1 rows stand at the best point, not the switch point -- `{20,19,17,19,18,16,12,4,0,0}` against `{20,19,17,17,16,14,10,2,0,0}`.
Control clean; **M22 killed** by exactly that test; **M20 (F-1) killed** by `Rf1RestorationSeam.ARowRefusalOnTheSwitchRunTakesTheSwitch`; **M21 (F-2)
killed** by `SqpEqualityBasis.F6bAPhaseThatFeasesOnlyTheRetainedRowsCertifiesNothing`. `VERDICT MUTATIONS_AS_DECLARED`.

## Leg P -- the 62 pilot cells (`gate-RF-1b/pilot/`; 0 cells threw)

`VERDICT PILOT_BARS_MET`: 0 lost, no false certificate, 055-057 off major 1. Solved (optimal + acceptable): R 19, H 19. Statuses R: optimal 8, acceptable 11,
max_iter 8, numerical_error 19, stalled 16; H: optimal 8, acceptable 11, max_iter 10, numerical_error 19, stalled 14. **Exactly three cells differ from R in
status, iterations or factorizations -- 055, 056 and 057, the targeted ones:**

| cell | R | H |
|---|---|---|
| 055 `kTier` | stalled, 1 major, 29241 fact, f 128617.18 | stalled, **72 majors**, 48964 fact, f **14535.19** |
| 056 `kTier`+`kAnyAccepted` | stalled, 1 major, 29241 fact | max_iter, **153 majors**, 43181 fact, f **14527.97** |
| 057 `kTier`+`kRatioOrFeasibilityStep` | stalled, 1 major, 29241 fact | max_iter, **153 majors**, 43692 fact, f **14531.80** |

These are the throwaway build's readings to the digit (055 stalled at 72 majors, f 14535.19; 056/057 max_iter at 153, f 14527.97 / 14531.80); the 055-057
solves now end where the pre-RF-1 path did (f ~ 1.45e4), at the iteration budget or the restoration stall window, not at major 1. Pilot factorizations
sum 710126 -> 758240 (the three cells do the work they did not do). None of the three is a solve (nor was any at R or C'): the bar is "off major 1".

## Leg T -- TRAIN (`gate-RF-1b/train/`)

476 rows per arm (238 roster cells x {D, K}; 238 hven-sqp rows), 0 missing; `VERDICT TRAIN_BARS_MET`: **0 lost solves, 0 false certificates** (10 certified
rows in each arm, the same ten: BURKEHAN, FLETCHER, HS88, HS92, LOOTSMA, D and K). hven-sqp: optimal 210 -> 210, factorizations 200407 -> 200759 (+352),
iterations 4599 -> 4598; statuses identical on all 238 rows. hven-ipm identical (224 optimal / 225 optimal+acceptable, 13477 factorizations, 9415
iterations). **One mover versus R: `MESH.K`** (`never-default`, `numerical_error` in both arms): R 131 majors, 4095 factorizations, f -14143131.84;
H 130 majors, 4447 factorizations, f -14143155.62 (lower objective, h_inf 1.4e-7 vs 4.9e-7). Reason: the solve restores 7 (R) vs 8 (H) times and takes the
curvature switch 4 times in both; the restart after a switch now starts at the switch point, which moves the path (elastic activations 12 -> 14, restoration
iterations 62 -> 71). Re-run ALONE (`gate-RF-1b/solo/`, one core, both lane binaries, twice each): the two rows of each arm are bit-identical to the co-run's
-- the mover reproduces, it is the change and not contention. (`MESH.D` does not move.)

## Leg C, Y, S -- ctest, symbol check, install smoke

ctest at the committed head: Release 100%, 4091/4091 (+2 not run: skipped/disabled, as before); Debug 100%, 4091/4091. Production `libhven.a`: 0 symbols
of `pass_restoration_walk_qps`. Install smoke PASSED on both front ends, export contract OK (rc 0).

## CI (`gate-RF-1b/ci.txt`)

Run 45090941397 at `5a977014b`: macOS, Linux release, Linux coverage success; **windows-clang-release failed one test**, `Continuation.ProbeBudgetBoundsAFailingProposal`
arm A, which read 560 on a Windows runner (clang-cl, MKL 2026.1, off-reference; r4 pinned Windows at 559, and WX-R11 records that Windows flips run to run).
The reference build reads 559 with this change (the box Release ctest and the reference Linux CI runner), so this is an off-reference reading, not a mover.
Tests-only commit `6f3f9a3fe` (the `04d06081f` pattern): off the reference build 558, 559 or 560 is accepted and recorded; the reference build keeps 559 exactly
(`src`/`include` unchanged by it). **Run 45096673801 at `6f3f9a3fe`: windows-clang-release, linux-clang-release, macos-clang-release, linux-clang-coverage all
success** (the Windows and Linux runners of that run recorded 559). Control: `ci.yml` dispatched on m8 `641dedeab` (run 45096622738, nothing pushed): all four
success. Apple values: none newly observed, none fabricated.

## Pilot harness (recreated)

`/home/ghecht/Projects/hven/.scratch/tu1-diag/hb` was swept. Rebuilt exactly as `legs/pilot_build.sh` describes: `legs-rf1b/harness_rebuild.sh` = `git archive 146e6ec`
of `hven-bench-tu1` (tree untouched) + `legs-rf1b/diagqp_patch.py`, which re-writes the DIAGQP stderr line (inserted after the solve in `run_sqp`; per non-optimal
ledger QP record, `DIAGQP #<1-based index> label=<label> status=<int>` and every nonzero `QpCounters` Index field in struct order; format read off the r3/r4 pilot
`.err` files, which this run's `.err` files match line for line in shape). The recreated patch is not byte-identical to the swept one (that is unrecoverable); it is
observation-only and no bar reads it. The pilot binaries were then built by `legs-rf1b/pilot_build.sh` (R and H pins), shas in `BINARIES.txt`.
Both lane builds in `legs-rf1b/lane_build.sh` share one private clone and must run serially (a first parallel attempt of the R lane failed at the clone and was re-run).

## Open items for the settler

- The literal ruling form (`return_best = kNever` on the whole first run) versus the shipped narrower guard: identical on 055-057; differ on phases whose first run
  ends at a limit/stall without a switch. Say if the literal form is wanted.
- 055-057 now end `stalled` (055, 72 majors) / `max_iter` (056, 057, 153 majors) at f ~ 1.45e4 -- the pre-RF-1 outcome class, not solves.
