# TU-0 r3 results (DECLARATION-TU-0-r3.md; code commit 1333eb50845746a2ef905c9f91d60c220e11f31b)

`orchestrate-r3.out`: ALL_LEGS_PASSED, GATE_RC=0. Outputs under `gate-r3/` (the TRAIN cell files are in `gate-r3/train/cells.tar.xz`).

| leg | result |
|---|---|
| P0 | code paths == the code commit and clean; base worktree `331b31837`; corpus A, lane A, lane B sha256 match; corpus B recorded; trees current |
| P1 | Release and Debug ctest: "100% tests passed, 0 tests failed out of 3981" |
| P2 | replay A vs B, 27 cells x walk, ssn, ipm: differences 0 in each |
| P3 | TRAIN 238 x {D,K}: 476 rows, 0 movers, 0 missing/capped, `TU0_TRAIN_PASS`; optdump = arm A + exactly the nine new lines |
| P4 | carried by P2 + P3 (counters, statuses and objectives at defaults are byte-identical) |
| P5 | control clean; all 35 mutations killed (`gate-r3/mutation/<id>.killers.txt`), tree restored, `MUTATIONS_AS_DECLARED` |
| P6 | install smoke and export contract rc 0 on the default and clang++ front ends |

History: r1 stopped at P1's criterion (declared count 3982 was `ctest -N`'s total; ctest reads 3980), r2 passed P0-P3 and stopped at P5 (M2 survived; fixed by a test), r3 passed.
Disclosures: `build-debug` got one `ninja` outside the lock before r1 (about a minute, three bench links); r1/r2 ran under the legacy lock, r3 under `box-lane`; the first r1 orchestrate stop (`BUILD_TREE_STALE`) preceded any leg.
