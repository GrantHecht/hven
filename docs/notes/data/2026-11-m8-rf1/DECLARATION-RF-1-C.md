# DECLARATION-RF-1-C -- gate C: the SQP failure-exit labels are reporting only

Written and committed before any gate leg; never edited afterwards (a failed or void leg is answered by a `-r1` file, this one
kept). Criteria are never changed after data. Design: `docs/notes/2026-11-m8-rf1-design.md` sections 1-2 and amendment C-1.

## 1. What is built (commit `c721dc32d`, on pf2d `84a6e3373`)

- `SqpOptions::termination` (`SqpTerminationOptions`): the IPM's four `acceptable_*_tol` names and defaults (1e-2, 1e-3,
  1e-3, 1e-3), validated finite, > 0 and >= the convergence tolerance.
- At the end of `finish`, top level only: an exit `kNumericalError` beside `kSubproblemFailure` / `kRestorationRefused` /
  `kRestorationFailed` / `kRestorationNoClaim`, or `kStalled` beside `kSubproblemIterationLimit`, whose returned point passes
  the tier (engine stationarity and complementarity of the `kkt` passed to `finish`; the result's caller-unit `feasibility_e`,
  `feasibility_i`) is reported `kAcceptable`, stop reason `kNone`, detail `exit <reason>`; `best_iterate` dropped.
- At the `kFinishQpFailure` arm, top level only: a last QP `QpStatus::kIterationLimit` (no non-finite exit reason) reports
  `kStalled` beside the new `StopReason::kSubproblemIterationLimit` (15). `return_best_applies` treats kStalled,
  kNumericalError and kAcceptable alike, so the returned point is unchanged by construction.

## 2. Pinned expectations re-derived (the declared event; all in `c721dc32d`)

Found by the development ctest run of `c721dc32d`'s parent tree plus these tests (Release, 11 failures, all status pins);
each edit is the relabel section 1 predicts:

| test | old pin | new pin | why |
|---|---|---|---|
| `Continuation.ProbeBudgetBoundsAFailingProposal` (step 1) | kNumericalError | kStalled | minor-cap exit |
| `QpCapCounters.TheCappedExitIsCountedApartFromANumericalOne` | kNumericalError | kStalled | minor-cap exit |
| `QpCapBudget.TheDefaultIsThreeAndOneIsTheOldGate` | kNumericalError | kStalled | minor-cap exit |
| `QpCapBudget.ABudgetOfThreeRoutesThreeFailuresAndThenExits` | kNumericalError | kStalled | minor-cap exit |
| `QpCapRetry.ADoubledCapThatStillDoesNotSufficeFailsAsTodayAndSaysSo` | kNumericalError | kStalled | minor-cap exit |
| `QpFailureStep.TheCappedFixtureConvergesOnItsOwnSteps` (routed arm) | kNumericalError | kStalled | minor-cap exit |
| `SqpSolverQpFailure.RepeatedQpMaxIterStillPropagates` | kNumericalError | kStalled | minor-cap exit |
| `SqpSolverQpFailure.RoutedFailuresCountTowardTheRestorationGate` | kNumericalError | kStalled | minor-cap exit |
| `SqpLadderSeam.CertOneAPointTheConvergenceTestCallsFeasibleIsNoCertificate` | kNumericalError | kAcceptable, stop none, message names `exit restoration_` | no-claim exit at a feasible, acceptable point |
| `TraceMessagesSqp.SubproblemFailureLinesMatchTheExitCounters` | warning iff `kSubproblemFailure` | warning iff the solve ended through a subproblem exit (either reason, or kAcceptable naming it) | the new reason |
| `OptionsReference.HeaderDefaultsAgreeWithTheRenderedDefaults` | -- | `SqpTerminationOptions` mapped to its header | new struct |

Layout and dump pins moved with the new field (`sizeof(SqpOptions)` 728 -> 760, `SqpTerminationOptions` 32; the
`sqp_default` / `sqp_perturbed` dump fixtures gain the four lines; `sqp_all_perturbed` perturbs them).
`docs/options-reference.md` regenerated. New tests: `Rf1AcceptableLabel.*` (4), `Rf1MinorCapExit.*` (2),
`SqpLadderSeam.Rf1ARestorationFailureIsLabelledAtTheTopLevelOnly`.

## 3. Arms and binaries (sha256 appended to `BINARIES.txt` before the first leg; frozen)

- **REF** = pf2d's evidence arm `L` (code `21ae792ce`; `git diff 21ae792ce 84a6e3373 -- src include` is empty, so it is this
  branch's base library): its committed TRAIN rows `../2026-11-m8-pf2d/gate/train/L/` and replay CSVs
  `../2026-11-m8-pf2d/gate/replay/L-k1-*.csv`. Nothing of REF is re-run.
- **C** = `c721dc32d`: corpus binary `.scratch/build-rf1/rel/bench/hven_sqp_corpus` (copied to `.scratch/bin/C/`); TRAIN lane
  binary from `legs/lane_build.sh C c721dc32d...` with `NEVER=1` (the private hven-bench clone at `e03cbad`, as pf2d).
- clang 22.1.8, `-DHVEN_FP_MODE=SAFER_FAST -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON`, Linux x86-64, AMD Ryzen 7
  5800X3D, MKL 2026.2, `MKL_NUM_THREADS=1`. Accelerate and Windows: CI only.

## 4. Gate (legs in `legs/`, evidence in `gate-C/`)

| leg | instrument | criterion |
|---|---|---|
| C1 | `ctest.sh` rel and dbg at `c721dc32d` (box-lane, `ctest -j4`) | each log "100% tests passed, 0 tests failed" |
| C2 | `train_enum.sh` (238 roster cells x {D, K}, stat mode, CAP 300 s, serial in one box lane, `MKL_NUM_THREADS=1`; counters only) + `c_relabel_check.py` vs REF | **0 path movers** (every key but `status`/`outcome` equal, residual keys inside the calibrated gate, `.optdump` equal once the four new SQP `termination` lines are dropped); missing/capped 0; **0 UNEXPECTED status movers**; every status mover listed with its class and residuals (`RELABEL_ACCEPTABLE?` rows explained per row, not a failure) |
| C3 | `replay.sh` x1 (27 cells x walk/ssn/ipm) of C, compared with REF's `L-k1-*.csv` by `scripts/compare_replay.py` | every difference is in the `status` column of a cell whose REF status is `numerical_error`; listed |
| C4 | `mutations.sh` C1..C12 (+R1 reported) | control clean; C1..C12 each killed (`MUTATIONS_AS_DECLARED`); tree restored. C1 label in the sub-driver, C2 `<=` -> `<` on stationarity, C3/C4/C5 complementarity / equality / inequality comparison dropped, C6 finiteness dropped, C7 `kIterationLimit` labelable, C8 minor-cap split disabled, C9 stop reason left at the old reason, C10 detail not set, C11 best iterate kept, C12 tier order not validated. R1 (the minor-cap split's top-level guard removed) is REPORTED: no fixture drives a capped QP through a restoration sub-solve's `kFinishQpFailure` arm |
| C5 | `install_smoke.sh` (default and clang front ends) | smoke and export contract rc 0 |
| CI | one dispatch of the four-lane workflow at the pushed head of `rf1` (`gh run watch --interval 120`) | green on all four lanes |

## 5. Not run

HOLDOUT (never). The census (A+B2's gate). Wall-clock. Accelerate and Windows readings (CI only).
