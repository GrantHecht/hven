# DECLARATION-RF-1b -- the curvature-switch run's switch exit and the YORKNET 055-057 cells (successor to the RF-1 gates; -AB2-r4 kept unedited)

Written and committed, together with every leg script it names (`legs-rf1b/`), before any leg; never edited afterwards (a failed
or void leg chain is answered by a `-r2` file, this one kept). Criteria are never changed after data. Design:
`docs/notes/2026-11-m8-rf1-design.md` section 4.2 with amendments C-1 and B2-1; the diagnosis is `RESULTS-RF-1-AB2-r4.md`, section
"055-057: why the resume gates refuse". Precedent: F-1 (`22d6cccb6`), the curvature-switch run keeps the pre-RF-1 rule.

## 1. The defect, the ruling and the change

Pilot cells 055-057 (YORKNET, `default` + `advanced.elastic_rung_engine=kTier`, two with a `trust_region_growth` variant) enter
restoration at major 0 (h_in 51001); the phase's first run (the curvature-switch run under `kModelThenRegularized`) fails its walk
subproblem at phase major 23. Pre-RF-1 the phase restarted on the regularized model FROM THE SWITCH POINT and converged to h 1.6e-7.
Under design 4.2 the switch run hands back its best captured point (B2-1: h 23136, an earlier iterate), the restart from there costs
~690 factorizations a major, the stall window (5000) fires at h 5851, both resume gates refuse, and the solve ends `kStalled` /
`kRestorationWorkLimit` at major 1 (f 128617; the pre-RF-1 path ends at f ~ 1.43-1.45e4).

Settler ruling C481: on the curvature-switch run `return_best` is `kNever`, so the restart starts from the switch point as before RF-1.

**Implemented narrower than the ruling's letter, and said here.** `ecda15017` makes `best_applies` (the exit rule, `run_major`) false for
a run told to switch (`stop_phase_on_qp_failure_`) that took the switch (`restoration_curvature_switches > 0`). A literal
`return_best = kNever` on the run would also switch design 4.2 off for every phase whose FIRST run ends at a limit or a stall without
switching -- under the shipped `kModelThenRegularized` that is every exact-Hessian phase -- and the existing
`Rf1RestorationSeam.ThePhaseHandsBackItsBestPointNotItsLast` (a phase that ends at the iteration cap on its first run) pins exactly
that hand-back. The defect is the switch exit alone, so only it is excluded; on 055-057 the exit is the switch (phase major 23), so the
two readings coincide there. If the settler wants the literal form, it is a one-line change and a successor gate.

Must-kill `Rf1RestorationSeam.TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint`, with a new seam knob
`pass_restoration_walk_qps` (the companion of `fail_restoration_walk_qps`; `HVEN_TESTING`-gated, `docs/testing.md` updated): the phase
takes two steps (h 19, 17), its third walk subproblem fails, the capture limit makes the order hold h = 19; the restart's rows must be
the same with and without the limit. Measured by the author before this file, on a build with the guard dropped: restart rows
{20,19,17,19,18,16,12,4,0,0} against {20,19,17,17,16,14,10,2,0,0} -- killed. Dev Release `hven_sqp_seam_tests` `Rf1RestorationSeam.*`
and `SqpLadderSeam.*`: 36/36 with the change.

Development reading, not a bar (a throwaway env-gated build on F-2, from the r4 results): 055 `stalled` at 72 majors, f 14535.19; 056
and 057 `max_iter` at 153 majors, f 14527.97 and 14531.80. This gate reports what the real build does against those.

## 2. Arms, binaries, legs and bars

Arms: **R** = m8 `641dedeab12a4aa5dfab7576650d7d0a1eee4647` (reference); **H** = `ecda150173efff5f59ef1a561725c8c5d74d16dc` (the change on
R; `git diff` of `src include tests` between H and this file's commit is empty). Both lane binaries (NEVER=1, the TRAIN lanes: the
`never-<base>` label = `<base>` with `linear.dense_factorization = kNever`, `hb_patch_never.py`) are built from the private hven-bench
clone at `e03cbad` by `legs-rf1b/lane_build.sh`; both pilot binaries from the pilot harness copy by `legs-rf1b/pilot_build.sh`. All four
are frozen in `BINARIES.txt` (an RF1B block, committed BEFORE the first leg, sha-checked by `rf1b_corun.sh`).

**The pilot harness was swept and is recreated** (the first sentence of `pilot_build.sh` names `/home/ghecht/Projects/hven/.scratch/tu1-diag/hb`):
`legs-rf1b/harness_rebuild.sh` takes `git archive 146e6ec` of `/home/ghecht/Projects/hven-bench-tu1` (tracked files only) into that path and
applies `legs-rf1b/diagqp_patch.py`, which re-writes the DIAGQP stderr line (the original patch is gone; the line's format is read off the
r3/r4 pilots' `.err` files: after the solve, per non-optimal ledger QP record, `DIAGQP #<1-based record index> label=<label> status=<int>`
then every nonzero `Index` field of `QpCounters`, struct order, `name=value`; observation only, nothing it reads is written back). The
`.err` is a diagnostic: no bar reads it, and the solver is not touched by it. The tree `hven-bench-tu1` is never written.

Toolchain and machine as the earlier gates (clang, `-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64, AMD Ryzen 7 5800X3D, MKL,
`MKL_NUM_THREADS=1`); the toolchain versions are stamped in the results file. Every step under `~/.local/bin/box-lane --` (never
`--exclusive`); counters only, no wall-clock claim. Evidence in `gate-RF-1b/`.

| leg | instrument (in `legs-rf1b/`) | bar |
|---|---|---|
| P | `rf1b_corun.sh`: `pilot_run_r1.sh` R and H (the 62 cells of `pilot_cells.tsv`; budget 300, 900 s kill reported, one pinned core each); `pilot_report.py`; `rf1b_check.py pilot` | **0 cells lost versus R** (R optimal/acceptable -> anything else), **cells 055, 056 and 057 do not end at `iterations == 1`**, no cell throws, no false certificate (H certified infeasible where R is not): `VERDICT PILOT_BARS_MET`. Solved counts, and every cell whose status, iterations or factorizations differ from R (a MOVER), are reported with a reason |
| T | `rf1b_corun.sh`: `train_enum.sh` (238 roster cells x {D, K}, CAP 300 s, one pinned core) on lane R and lane H; `ab2_train.py`; `rf1b_check.py train` | **hven-sqp: 0 lost solves versus R and 0 false infeasibility certificates** (a row of H with status `infeasible` or `sqp_infeasibility_certified=1` where R has neither): `VERDICT TRAIN_BARS_MET`. 476 rows per arm, 0 missing; hven-ipm rows run too and are reported. HOLDOUT never |
| M | `rf1b_chain.sh` -> `mutations_rf1b.sh` with `mutate_rf1b.py`; control first; the sqp filters of `mutations_ab2_r4.sh` unchanged | control clean; **M22 (new: the guard dropped) killed by the must-kill of section 1; M20 (F-1) and M21 (F-2) still killed**: `VERDICT MUTATIONS_AS_DECLARED` |
| C | `rf1b_chain.sh`: `ctest.sh` rel and dbg at the committed head | "100% tests passed" in both logs; counts reported |
| Y, S | `rf1b_chain.sh` | the production `libhven.a` carries no symbol of `pass_restoration_walk_qps` (count 0); the install smoke and export-contract checks rc 0 on both front ends |
| CI | `gh workflow run ci.yml --ref rf1b` at the pushed head, `gh run watch <id> --interval 120` (never shorter; a 403 waits for the reset) | green on all four lanes; an off-reference (Accelerate/Windows/AMD or Intel runner) pin that differs is recorded as observed and pinned in a separate tests-only commit after the `04d06081f` pattern, then CI re-run; Apple values not observed are UNOBSERVED |

Co-run protocol (CLAUDE.md section 7): legs T and P co-run as one process per pinned PHYSICAL core of one box lane (SMT siblings idle),
`MKL_NUM_THREADS=1` in every process, two TRAIN arms and two pilot arms at once; the columns asserted are counters, statuses and
objectives, wall-clock stays informational and is never quoted. A cell that ends CAPPED (the 300 s watchdog) or at a 900 s kill, or any
deviation that decides a bar, is a candidate only: it is re-run ALONE under the serial rule before it is called a regression.

## 3. Mutations

M20 (F-1 exclusion dropped) and M21 (F-2 reverted) are re-run unchanged from `DECLARATION-RF-1-AB2-r4.md` section 3; **M22** is new:
anchor `if (stop_phase_on_qp_failure_ && st.out.counters.restoration_curvature_switches > 0) {` (the `best_applies` guard), rewritten
`if (false && ...`; must-kill `Rf1RestorationSeam.TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint`. M1-M19 are not re-run: the change
touches no code any of them rewrites except through `best_applies`, whose other pinning tests (`ThePhaseHandsBackItsBestPointNotItsLast`,
`TheStallExitTakesTheReductionResume`) are inside the control's filters.

## 4. Not run

HOLDOUT (never). Wall-clock. The QP walk census. Accelerate and Windows readings (CI only).
