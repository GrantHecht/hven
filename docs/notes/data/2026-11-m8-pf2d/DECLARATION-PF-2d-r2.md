# DECLARATION-PF-2d-r2 -- L3 also withheld: the shippable head is L1; L3 and L2 run as evidence arms F and G

`DECLARATION-PF-2d.md` and `-r1.md` are kept unedited. Written and committed before any gate or measurement leg (builds, unit-test runs and one
Release ctest run of the F tree preceded it).

## 1. Finding

The Release ctest of the L1+L3 tree (code `f3d69e8e9`; `gate/suites/ctest-rel.log`, 4046 tests, 2 failed) fails two existing tests that L1 passes:

- `QpE2WorkingSet.TheUndecidableBorderPingPongRetreatsOnceAndEndsOptimal` (`dep_undecidable` 1 -> 0, `dep_border_retreats` 1 -> 0): a constructed,
  exactly dependent border candidate reads first-solve rho1 = 0.064 and the stalled refinement collapses it to 1.2e-6 (UNDECIDABLE). L3 admits it as
  independent at the first solve. A diagnostic build (L1 + a log of rho1 vs the refined verdict for every test that would refine; scratch, not committed)
  over the whole qp and qp-seam suites (15 296 refining tests) finds this one the only verdict change with rho1 >= 1e-2: the stated "0 verdict changes at 1e-2"
  of SIZING-PF-2c holds on the eight f7 cells and fails on this fixture.
- `QpLatchReason.BritgasMainSubproblemHoldsItsConditioningLatch`: BRITGAS qp0 walks differently (objective 0.24429 vs 0.24292, factorizations 167 vs 166).
  Of its 115 refining tests 102 would be decided at rho1 >= 1e-2 (none changes verdict; 35 of them would have converged under full refinement, so
  `stall_admit` flips from false to true on those). The mechanism by which the path moves is UNOBSERVED.

## 2. Changes to the plan

- The shippable head **L** (branch `pf2d`) = L1 only (commit `21ae792ce` + the test commit + legs/docs): P1 (ctest rel and dbg at the final head and at the L1
  commit), P2 L vs A, P3 L vs A (0 movers off the tally keys), P4 R-mutations, P5, CI, and the P6 headline `newL`/U0 are asserted/reported for L as declared.
- **F** = L1 + L3 (`f3d69e8e9`, branch `pf2d-l3`) and **G** = F + the literal L2 band (branch `pf2d-l2`) are EVIDENCE ARMS: replay (F, G vs A: statuses and
  counters reported), TRAIN (D/K, 476 rows each; status movers and counter movers reported per row with cause where observed), P6 arms `newF`, `newG`, and
  mutations T1-T6 (in F's tree) and B1-B7 (in G's tree) against their own tests. Their ctest results are recorded, not asserted (F: 2 failures above;
  G: the ACOPR57 failure of -r1).
- P6 arms: `u0`, `prev`, `newL` (the shippable head), `newF`, `newG`; the `off` arm is dropped (the detector-off tally is the same program in every head and was
  taken in PF-2b). Instruction medians over three rounds as declared.
