# M8 TU-0 r3 -- successor round (DECLARATION)

Committed before the first r3 leg. `DECLARATION-TU-0.md` (with its r1 addendum) and `DECLARATION-TU-0-r2.md` stand except where this file replaces them; the r1 round (`gate/`,
`orchestrate.out`) and the r2 round (`gate-r2/`, `orchestrate-r2.out`) are kept as records.

## 0. Why r3

r2 passed P0-P3 (Release and Debug ctest 3980/3980; replay 27 cells x walk, ssn, ipm: 0 differences; TRAIN 238 x {D,K}: 476 rows, 0 movers, `TU0_TRAIN_PASS`) and stopped at P5:
**mutation M2 survived** (`advanced.adaptive_dual_regularization_max` read as the old constant in the schedule's clamp, iterations >= 1: every pinning test left it alone, because the
existing tests read the ceiling only at iteration 0 and as an upper bound). The tree was restored by the leg (clean). Nothing else failed.

## 1. What changes

- **New code commit `1333eb50845746a2ef905c9f91d60c220e11f31b`**: one new test, `Tu0AdaptiveDualRegularizationMax.ALaterMajorsMuCanExceedTheOldCeilingOnlyWhenTheOptionRaisesIt`
  (a later major's `regularization_dual` exceeds 1e-8 at ceiling 1; reference route; never above 1e-8 at the default). `src`, `include` are unchanged against `0947374fe`; the library is the same.
- **ctest count: 3981** (ctest's "out of N"; r2's 3980 plus the new test), Release and Debug.
- Binaries: corpus A `92e3f2939970b372b3400683ddc2b2b66f06cc1346e8ecbc56e892fcd39a563f`, lane A `9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481` (unchanged);
  lane B `eb5b9403c3b558a5e25a3cdcc90b837ab5def31afd2e81a51a3f79439afb1ae0` (pin = the new code commit; branch `tu0-r3b-B` in the private clone); corpus B recorded by P0.
- Instruments: the box rule changed during the round (owner ruling: two 4-core lanes). The legs now run through `box-lane` once per step (`orchestrate_tu0.sh` wraps each leg and the P0 relink),
  the no-concurrent-cell gate (`pgrep hven_bench_cell`) is removed (counter legs may co-run on the other lane), cell pinning uses the lane's first physical CPU (`BOX_LANE_PHYS`), and
  `-j16` became `${BOX_LANE_JOBS:-4}`. No criterion changed; wall-clock is not asserted.
- Disclosure: r1/r2 legs ran under the legacy flock with 16 jobs; r3 under the lanes. The ctest and TRAIN criteria are counters and statuses, scheduling-invariant.

## 2. Order

```
export PATH=$HOME/.local/bin:$PATH
bash docs/notes/data/2026-11-m8-tu/legs/orchestrate_tu0.sh 1333eb50845746a2ef905c9f91d60c220e11f31b 331b3183747b5c1849651706f648de2c337ddf75 92e3f2939970b372b3400683ddc2b2b66f06cc1346e8ecbc56e892fcd39a563f 617eab89dad81c99409bb01217fa8561c37fcb4c7476628640f327e06556df75 9d766ea786fdbffb938120547b629177c5ff8d4309efa144194a5044246c9481 eb5b9403c3b558a5e25a3cdcc90b837ab5def31afd2e81a51a3f79439afb1ae0 3981 gate-r3 \
  > docs/notes/data/2026-11-m8-tu/orchestrate-r3.out 2>&1
```
A failed leg stops the chain and is answered by `DECLARATION-TU-0-r4.md`. After the chain passes: CI on the pushed head, four lanes green.
