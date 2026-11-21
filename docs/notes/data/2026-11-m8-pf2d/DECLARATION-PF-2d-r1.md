# DECLARATION-PF-2d-r1 -- L2 withheld from the shippable head; the literal L2 band runs as evidence arm G

`DECLARATION-PF-2d.md` is kept unedited. This successor is written and committed before any gate or measurement leg (only builds and
unit-test runs of the development tree preceded it).

## 1. Finding that changes the plan

Implementing L2 literally (re-read only for converged rho in (1e-6, 1e-4], commit `e6981723e` first form, now `336723ed1` on branch
`pf2d-l2`) makes an existing test fail in Release: `QpE2WorkingSet.AGreyReReadSolvesTheAcopr57RestorationQp`
(`tests/qp/seam/test_qp_anti_cycling_arm.cpp`) goes from kOptimal / f = -18.3950535 to status 1 (kNumericalError class) on both algebras (objective
-16.362, KKT stationarity 0.12). Measured on that dump with the old rule (log: 72 re-reads, 68 certify dependent): re-reads that flip a
candidate to dependent occur at test rho up to **4.5e-4** (rho 1.05e-4, 1.31e-4, 1.51e-4, 2.0e-4, 2.3e-4, 2.6e-4, 2.7e-4, 3.1e-4, 4.0e-4, 4.5e-4
all flip). SIZING-PF-2c's "largest flipping rho 1.55e-5" is the f7 cells' figure; the restoration QP of ACOPR57 is a counterexample to
using it as a corpus-wide bound. The re-reads on the f7 cells sit at rho 3e-4..1e-3 (89 %), so any band that keeps every ACOPR57 flip (top >= 5e-4)
also keeps most of the f7 re-reads: L2's saving and its safety are in tension on this evidence.

## 2. Changes to the plan (everything else of the declaration stands)

- The shippable head **F** (branch `pf2d`) = L1 + L3 (+ the test and legs commits): bit-identical L1, L3 as declared. P1 (ctest rel and dbg at L and
  at F), P4 for L1/L3, P5 and CI apply to F.
- **G** = F + the literal L2 commit (branch `pf2d-l2`, local; built from `.scratch/armG`) is an EVIDENCE ARM, not a candidate for merge: P2 (replay x3)
  and P3 (TRAIN 238 x 2, status-mover bar) run on G exactly as the declaration states for F (so the lead's L2 question -- 0 status movers on TRAIN? --
  is answered with data), P6 gains the arm `newG` (= measD built from G + the lever patch), and the L2 mutations B1-B7 run in G's tree against G's tests.
  ctest on G is NOT part of the gate: its one known failure is the ACOPR57 test above (observed), and the other tests of G are not asserted.
- Arms in P2/P3: A, L, F, G co-run (four pinned physical cores of one lane). P3 criteria: L vs A as declared (0 movers off the tally keys); F vs A:
  0 status movers is the bar and F's counter movers are reported (L3's `dep_test_solves`, `dep_stall_admits`); G vs A reported the same way, its status
  movers listed (a nonzero count is a finding, not a gate failure).
- P6 arms: `u0`, `prev`, `new` (= F), `off`, plus `newG`. The headline ratio is `new`/U0 (the shippable head); `newG`/U0 shows what L2 would add.
