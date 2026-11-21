# DECLARATION-PF-2e-r1 -- the probe restricted to the eliminated path (E2); every leg re-taken on E2

`DECLARATION-PF-2e.md` is kept unedited. Written and committed before any leg on E2 (only builds, dev unit-test runs and the
scratch diagnostic below preceded it).

## 1. Finding that changes the plan (leg P2 on E, `gate/train/`)

P2 ran on E (rule as declared, both paths): its asserted criterion held (0 status movers, 0 missing/capped; `gate/train/path-E-vs-L.txt`),
but NET1 D and K moved their path (qp minors 853 -> 787, 784 -> 841; the same optimum, objective and stationarity byte-equal) and
read `dep_grey_reread_dependent` 4 -> 0: four certified flips were lost, which the task's bar ("never weaken a certified decision
without evidence") forbids. Diagnosed with the scratch event log run on TRAIN (a scratch lane binary, pf2d L + the diag patch, pin
`adf11f94c`, never pushed; `diag/sim-train-and-r1.txt`, `diag/net1-border-collapse.txt`): all 8 lost flips on TRAIN are NET1's, all on
the BORDER path, all the same shape -- rho 0.50 -> 3.35e-6 (test, converged) -> 3.40e-6 (re-read step 1: q = 1.02, STAGNANT, and
the mu-free residual stagnant with it, 5.0e-9 -> 4.5e-9) -> 4.6e-11 (step 2: collapse). The refinement on the Schur complement
solve is not a steady contraction: a stagnant step can precede a collapse, so the probe's premise fails there. On the ELIMINATED path,
over TRAIN (1 332 flips), the qp suites, the replay, the f7 cells and the fixtures, 0 flips are lost and 0 test verdicts change; the
outcome-changing refinements contract steadily (largest q at a probe point 0.335, LAKES: 0.335, 0.34, 0.34 -- below the threshold,
and a steady q >= 0.5 could not have reached kDepTol in the budget, which is what the reach encodes). The sim's 16 "test verdict
changes" on NET1 under the first rule were false positives of the simulation (R-A structural dependencies, which overrule the
"independent" either way); corrected in the r1 simulation.

## 2. Changes to the plan (everything else of the declaration stands)

- **E2** = E + the probe on the eliminated path only (`test_probe`, `reread_probe` require `!sys.bordered`); a bordered test and re-read
  refine to the budget as before (L1b's resume, bit-identical, applies on both paths). Code and test commits are recorded in
  `BINARIES.txt` under "E2" before the first E2 leg, with the lane binary `lane E2` and the measurement binary `measE2`.
- Declared pin move updated: `AStalledRefinementAboveTheUndecidableBandIsAdmittedAndFlagged` reads solves 2 (probe_cut) on the
  eliminated path and 1 + kDepRefineSteps (no cut) on the border path; the `WsIndependenceProbe` tests assert the border path is never
  cut.
- Legs P1 (ctest rel and dbg), P2 (TRAIN: L and E2 co-run; the E result above is kept as evidence of the finding), P3 (measure: newL
  control and newE2) and P4 (mutations P1-P8 + **P9: the border exclusion removed** (killers: the border cases of
  `WsIndependenceProbe.*` and `AStalledRefinement...`)) run on E2 exactly as declared for E. CI at the pushed `pf2e` head.
- P2 criterion added for E2 (asserted): `dep_grey_reread_dependent` equal to L's on every TRAIN row (no lost flip), besides 0 status
  movers and 0 missing/capped. Path movers and their objective-quality reads are reported as declared.
