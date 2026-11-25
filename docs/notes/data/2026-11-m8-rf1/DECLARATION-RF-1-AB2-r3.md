# DECLARATION-RF-1-AB2-r3 -- gate A+B2 on the rebased head, with the three settler rulings (successor; -AB2, -r1, -r2 kept unedited)

Written and committed, together with every leg script it names, before any leg; never edited afterwards (a failed or void leg
chain is answered by a `-r4` file, this one kept). Criteria are never changed after data. Design:
`docs/notes/2026-11-m8-rf1-design.md`; the A+B2 interpretations of `DECLARATION-RF-1-AB2.md` section 2 stand, as amended below.
The pre-rebase gate's read-out (`RESULTS-RF-1-AB2.md`) is superseded by this gate's: nothing of it is carried as evidence.

## 1. Why a successor gate, and what changed since `-AB2`

`rf1` was rebased onto `origin/m8` `692727dc8` (adds the symbolic-analysis reuse; the counters append after it), and three
rulings were implemented on top:

- **S-1 `cfa600be0`** the restoration phase's work limit is a STALL WINDOW: `advanced.restoration_stall_factorizations`
  (Index, default 5000, renamed from `restoration_max_factorizations`). It fires only when the phase's best parent h has not
  fallen by at least 10% over the last W factorizations (counted across the phase's runs); the caller's
  `common.max_factorizations` stays the hard cap and wins the label. It replaces the fixed 5000 + F_entry budget, which cut
  CRESC100 D/K at major 1 and YORKNET pilot cells 055-057 while h was falling.
- **F-1 `22d6cccb6`** the any-h row-refusal elastic route (design 3.3) does not apply on the phase's curvature-switch run
  (`stop_phase_on_qp_failure_`): there a row refusal keeps the pre-RF-1 rule (the CRESC50.D false-certificate fix).
  Must-kill `Rf1RestorationSeam.ARowRefusalOnTheSwitchRunTakesTheSwitch` (seam knob `restoration_as_row_refusal`).
- **M `f70f5c850`** fixtures for mutations M13 (HVEN_TESTING knob `restoration_capture_limit`) and M19 (a stall `kWorkLimit`
  into the reduction resume). Both measured killed by their author; re-measured here.
- `b4f72e5a7` re-derives every pin the rebase moved, mechanically (counter field table, `sizeof` pins, trace golden,
  options reference).

## 2. Declared interpretation (S-1, stated here, not silently)

**The stall window reads every iterate's parent h, phase-feasible or not**, while the B2-1 comparator
(`restoration_best_improves`: lowest parent h among phase-feasible iterates, strictly below the phase start's) governs the
hand-back only. Reason (from the implementation): walk-tolerance iterates of the circle phase are never capturable, so the
comparator's incumbent alone would cut a falling phase. The window keeps the running minimum of every observed parent h, so a
regularized restart does not undo progress; the incumbent is tracked whether or not the hand-back is armed, so the stall does
not depend on `return_best` (M13 remains a hand-back-only mutation). The stall's `kWorkLimit` is labelled `kStalled` /
`kRestorationWorkLimit` by the parent, and the reduction resume reads it as it read the budget (design 4.4).

## 3. Arms, binaries, legs and bars

Arms: **C'** = local branch `rf1-C-rebased` = `b1fe115d0` (C = `c721dc32d` rebased onto the same m8: code `5ba2828ea` plus
the test-arm fix; not pushed) and **AB2'** = `b4f72e5a7` (the pushed head of `rf1`; `git diff` of `src include tests` against
the committed tree is empty at this file's commit). Both arms are rebuilt for this gate (lane, pilot and corpus binaries) and
frozen in `BINARIES.txt`, appended with this file's results commit but BEFORE the first leg; the gate-C TRAIN rows are
pre-rebase and are NOT used: C' TRAIN is re-run. Tags in the scripts: `Cr3` = C', `ABr3` = AB2'. clang 22.1.8,
`-DHVEN_FP_MODE=SAFER_FAST`, Linux x86-64, AMD Ryzen 7 5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Every step under
`~/.local/bin/box-lane --` (one 4-core lane per step, never `--exclusive`); counters only, no wall-clock claim. The corpus
binaries' embedded `# binary:` stamp is stale (configure-time); their authority is the sha256 and the commit in `BINARIES.txt`.

| leg | instrument (in `legs/`) | bar / what is reported |
|---|---|---|
| T | `r3_corun.sh`: `train_enum.sh` (238 roster cells x {D, K}, CAP 300 s, one pinned core) on lane Cr3 and lane ABr3, then `ab2_train.py` ABr3 vs Cr3 | **Zero false infeasibility certificates on TRAIN, and specifically CRESC50.D must not certify infeasible** (a `kInfeasible` row of AB2' on a cell C' did not certify, or on a cell whose reference solve is feasible, is a false certificate; each listed per cell). Every status mover listed with its objective-quality read. **Any solve lost versus C'** (optimal/acceptable -> other) is listed per cell with its reason; a lost solve is an open item for the settler, not a pass. Reported: optimal and optimal+acceptable counts per arm and mode, factorization and iteration totals per lane. Missing rows 0. HOLDOUT never |
| P | `r3_corun.sh`: `pilot_run_r1.sh` Cr3 and ABr3 (the 62 cells of `pilot_cells.tsv`; iteration budget 300, a 900 s kill reported, one pinned core each; the decoded-set link of -r1) + `pilot_report.py` | each cell old vs new; reported against the pre-rebase counts (baseline 12 solved, prior AB2 19); BRITGAS factorizations, CRESC100.D/K (leg T) and YORKNET cells 055-057 called out. No bar. Void if any cell throws for an infrastructure reason (count printed) |
| W (the QP walk census) | `r3_census.sh Cr3` and `r3_census.sh ABr3`, one box lane each, through `run_walk_census_lane.sh` (4-wide T1/T3, T2 solo within its lane), tiered from `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv`; then `census_compare.py` Cr3 vs ABr3 | every mismatch listed. **DEVIATION from the instrument's declared terms** (as in `-AB2`): T2 is solo in its lane, not on the machine, so a wall-dependent status difference is a CANDIDATE only, never a finding, and is not re-run alone here |
| M | `r3_chain.sh` -> `mutations_ab2_r3.sh` with `mutate_ab2_r3.py`; control first; each mutation rebuilt and run on the pinning filters of `-AB2` (sqp: `Rf1*:RestorationBestOrder.*:QpCap*:QpFailureStep.*:SqpSolverQpFailure.*:SqpSolverRestoration*:RestorationResumeReduction*:Continuation.ProbeBudget*:HsSweepCorpus.*`; seam: `SqpLadderSeam.*:Rf1*`; trace: `TraceMessagesSqp.*`) | control clean; **M1-M20 each must be killed**; a survivor is REPORTED by name (`MUTATION_SURVIVED_<id>`), never hidden. R1 reported only |
| C | `r3_chain.sh`: `ctest.sh` rel and dbg at the committed head | "100% tests passed" in both logs; counts reported |
| S | `r3_chain.sh`: `install_smoke.sh` (`scripts/check_install_smoke.sh`, default and clang front ends, export contract) | smoke and export contract rc 0 |
| Y | `r3_chain.sh`: `nm -C` and `strings` over the production `libhven.a` of the Release tree | no symbol and no string for `restoration_capture_limit` (the knob exists only under `HVEN_TESTING`): count 0 |
| CI | the four-lane workflow at the pushed head of `rf1` (`gh run watch <id> --interval 120`) | green on all four lanes; an Accelerate/Windows route pin that differs is recorded as observed and pinned in a separate commit exactly as `7d66b5e60`/`82c37da99`, then CI re-run; never guessed (Apple values not observed are UNOBSERVED) |

## 4. Mutations (re-derived from `b4f72e5a7`'s range; every one must-kill; anchors counted by `mutate_ab2_r3.py`, preflighted)

M1 R1's skip rule inverted; M2 R2's inverted; M3 R4's inverted; M4 the replay guard disabled; M5 the projection on every
termination kind; M6 the projection never applied; **M7 the any-h row-refusal route reverted to the bound rule (re-anchored on
`any_h_row_refusal`)**; M8 the floor exit's `h <= feas_tol` conjunct dropped; M9 its noise conjunct dropped; M10 the after-rungs
elastic route reverted; **M11 the stall window never fires (re-anchored on `RestorationStallWindow::observe`'s return)**; **M12 the
stall labelled the caller's `kWorkLimit` (re-anchored on `phase_stalled`)**; **M13 the sub-driver's `return_best` reverted to
`kNever` (new fixture: `restoration_capture_limit`)**; M14 the B2-1 comparator reverted to the shared order; M15 its
phase-feasibility conjunct dropped; **M16 its start incumbent dropped (re-anchored: the incumbent block is now outside
`best_armed`)**; M17 the budget-best exit without a capture; M18 the elastic-exhaustion route inside the phase reverted;
**M19 the reduction-resume extension reverted (re-anchored on `phase_stalled`; new fixture)**; **M20 (new) the F-1 exclusion
dropped: row refusals route to elastic on the curvature-switch run too** (must-kill:
`Rf1RestorationSeam.ARowRefusalOnTheSwitchRunTakesTheSwitch`). R1 the comparator made non-strict (reported). Known likely
survivors, stated before the data: M15 and M16 (no fixture is known to separate them, as at `-AB2`, where `RestorationBestOrder.*`
and the B2-1 fixture killed them), M9 (the noise floor's calibration point is SMBANK, not a unit fixture). The reading is the
measurement, not this prediction.

## 5. Not run

HOLDOUT (never). Wall-clock. Accelerate and Windows readings (CI only). A machine-solo re-run of census candidates.
