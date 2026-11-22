# RESULTS-RF-1-AB2-r4 -- the narrow re-check after F-2 (DECLARATION-RF-1-AB2-r4.md)

Arms: C' = `b1fe115d0` (`rf1-C-rebased`, r3's binaries, sha-checked), ABr4 = `994619746` (F-2 on `ce210b804`). Evidence in
`gate-AB2-r4/` (committed `eae5a33e3`). Counters only; no wall-clock claim. Binaries frozen in `BINARIES.txt` (r4 block,
`690c2dfab`) before the first leg. Every bar of the declaration is met.

## The 054-YORKNET verdict (section 1 of the declaration, restated with the numbers)

**False certificate.** AB2' (r3) certified `infeasible` at major 101 through the pre-existing certified arm: the 4th
restoration's regularized restart ended kOptimal (stationarity 5.3e-10) at `kkt_r.feasibility` 8.68e-6 > 2 feas_tol. The
equality basis had reduced the rows (247 of 256 retained); the restored point has the retained rows at inf 1.8e-10 and the whole
violation (l1 1.19e-5) on two dropped rows (114: -8.68e-6, 221: -3.22e-6) that no phase multiplier priced, with gradients out of
the retained rows' span (residuals 9.7e-4, 3.7e-4). The linearized l1 violation over all rows, in the box, falls 15.5% within an
inf-norm step of 1e-3 and 100% within 1e-2: not a stationary point of h. The hole (QC-1's margin read over rows the phase does
not carry) is unchanged since C'; RF-1's routes (retry rungs, 49 box projections, the after-rungs elastic route into four
restorations) steered the iterate there after C' had stopped at major 22 on the minor-cap budget. F-2 reads the claim on the
retained rows only.

## Leg T -- TRAIN (`gate-AB2-r4/train/`)

476 rows per arm, 0 missing. **hven-sqp: 0 lost solves, 0 false certificates** (`r4_check.txt`: `VERDICT TRAIN_BARS_MET`).
Totals ABr4 vs C': hven-sqp optimal 210 -> 210, factorizations 200718 -> 200407, iterations 4480 -> 4599; hven-ipm identical
(224/225, 13477 factorizations). Status movers vs C' (4): HS99EXP.D/K `max_iter` -> `stalled` (r3's two, unchanged) and
**LEWISPOL.D/K `infeasible` (certified) -> `numerical_error`** at the same point (f 3.00038 / 3.00014, h_inf 3.9991). Against
r3's AB2' the only movers are LEWISPOL.D/K (`vs_ABr3.txt`; every other row identical in status, iterations and
factorizations): F-2 withdrew a certificate C' and AB2' both issued. Read on a throwaway instrumented build (pre-F-2, not
evidence of this gate): LEWISPOL (n 6, 9 equality rows) restores its 6 retained rows to 3.6e-8 and leaves the 3 dropped rows at
1.6e-4, -0.9996, -3.999; the linearized l1 violation over all rows falls 1.3% within an inf-norm step of 1e-3 and 97.4% within
1e-1 -- that certificate was not a stationary point of h either. 10 certified rows remain, the same in both arms (BURKEHAN, HS88,
HS92, LOOTSMA, D and K: certified; FLETCHER D/K: `infeasible`, uncertified).

## Leg P -- the 62 pilot cells (`gate-AB2-r4/pilot/`; 0 cells threw)

`r4_check.txt`: `VERDICT PILOT_BARS_MET`. **054-YORKNET: `numerical_error`, not certified** (106 majors, 22907 factorizations,
f 14704.18; the 4th phase handed back through the reduction resume, h 2.36e-4 -> 1.19e-5; three rejected majors there, then a 5th restoration request refused by the entry
rule -- no step accepted since the resume). Solved: C' 12, r3's AB2' 19, ABr4 **19**; no cell solved by C' or by r3's AB2' is unsolved. Against r3's AB2', 59
cells are identical (status, iterations, factorizations, f); the 3 movers are 054 and **DALLASM 028/029** (`sqp-qn` +
`adaptive_dual_regularization_max=1e-6` [+ `kRatioOrFeasibilityStep`]): `infeasible` (certified, in C' too) -> `numerical_error`
at the same point (h_inf 9.23e-6, restoration h_out 1.87e-5, sub-status optimal): F-2 withdrew those claims as well (by
construction only a violation carried by dropped rows changes the verdict; the stationarity of those two points was not
re-measured). C' re-run identical to its r3 rows. (The declaration's development reading named the entry cap for 054's last refusal; the
trace says the entry rule. Not a bar; the declaration is not edited.)

YORKNET 055-057 (`elastic_rung_engine=kTier`): unchanged, `stalled` at major 1, 29241 factorizations, f 128617 (C' 14318-14528).
Diagnosed, not fixed (remedy proposed below).

## Leg M -- mutations (`gate-AB2-r4/mutations.out`)

Control clean; `VERDICT MUTATIONS_AS_DECLARED`. M19 killed by `Rf1RestorationSeam.TheStallExitTakesTheReductionResume`; M20 by
`Rf1RestorationSeam.ARowRefusalOnTheSwitchRunTakesTheSwitch`; **M21 (F-2 reverted) by
`SqpEqualityBasis.F6bAPhaseThatFeasesOnlyTheRetainedRowsCertifiesNothing`**.

## Leg C -- ctest (`gate-AB2-r4/suites/`)

Release 100%, 4087/4087; Debug 100%, 4087/4087 (r3: 4086 + the F-2 fixture).

## CI

Run 45064036804 at `eae5a33e3`: windows-clang-release, macos-clang-release, linux-clang-coverage success; **linux-clang-release
FAILED** (3 tests) on an Intel Xeon 6975P-C runner whose dispatch probe read OFF-REFERENCE (avx512f=1, mkl_auto_branch=14):
`HsSweepCorpus.Corpus` HS15 warm 21/39 and `HsSweepCorpus.Hs15WarmKdOffWithTheFailedSubproblemStep` 39 (r3 pinned Linux Release
to 22/40 and 40 after an AMD off-reference runner read those), and `Continuation.ProbeBudgetBoundsAFailingProposal` arm A 558
(559 elsewhere). On the reference build F-2 moves none of the three (the gate's ctest at `994619746` reads 22/40, 40 and
559, as r3 did at `b4f72e5a7`); these are the readings of an off-reference CPU not observed before. Tests-only commit `04d06081f`: off the reference build either observed value is accepted and recorded
(`record_off_reference_route`); the reference build keeps 22/40, 40 and 559 exactly (`src`/`include` unchanged).

Run 45068201995 at `04d06081f`: **windows-clang-release, linux-clang-release (an AMD EPYC 9V45 runner, OFF-REFERENCE),
macos-clang-release, linux-clang-coverage all success.** (`gate-AB2-r4/ci.txt`.) The results commit that follows adds
documents only.

## 055-057: why the resume gates refuse, and the proposed remedy (diagnosis only; no code in this gate)

All three enter restoration at major 0 (h_in 51001; the elastic tier exhausted on a verified-infeasible QP). The phase's first run
(the curvature-switch run under `kModelThenRegularized`) fails its walk subproblem at phase major 23 -- identically in both arms.
Then:

- **C'** restarts the phase on the regularized model FROM THE SWITCH POINT (run 1's last iterate, wrapper objective 876.6); the
  restart converges in 68 cheap majors to h 1.6e-7 and the phase resumes feasibly; the solve goes on to f ~ 1.43-1.45e4.
- **AB2'** runs the switch run with `return_best = kOnAnyNonOptimal` (design 4.2), so run 1 hands back its best captured point
  (B2-1: the lowest parent h among phase-feasible iterates; the only phase-feasible improvement is run-1 major 4, h 23136 -- every
  later iterate is phase-infeasible at walk tolerance). The restart therefore begins at h 23136 instead of the switch point,
  costs ~600-700 factorizations a major there, and the phase-cumulative stall window (W 5000) fires at restart major 7 with
  h 5851. The resume gates then refuse: not feasible (inf 1e-6 test), and the reduction resume needs h_out <= 0.1 h_in = 5100.1
  against 5851.1 (ratio 0.115). The exit is `kStalled` / `kRestorationWorkLimit` at major 1.

**Proposed remedy**, in the spirit of F-1 (the curvature-switch run keeps the pre-RF-1 rule): the switch run does not hand back its
best point -- its `return_best` stays `kNever`, so the regularized restart starts from the switch point as before RF-1; the best-h
hand-back applies to the run whose result the parent's resume gates read. Measured on a throwaway env-gated build (on F-2, not
evidence of this gate): 055 `stalled` at 72 majors, f 14535.19; 056 and 057 `max_iter` at 153 majors, f 14527.97 and 14531.80;
each first restoration's restart converges (68 majors) and resumes at h 1.6e-7, exactly as at C'. A must-kill would pin the
restart's start (the switch point, not the run's best) on a seam fixture. Settler ruling required; not implemented here.

## Open items for the settler

- The 055-057 remedy above (switch run `return_best = kNever`), with its own gate if adopted.
- F-2 also withdrew pre-existing certificates (LEWISPOL D/K on TRAIN, DALLASM 028/029 on the pilot); they now end
  `numerical_error`. LEWISPOL's was measured non-stationary; DALLASM's was not re-measured.
