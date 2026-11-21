# DECLARATION-PF-2b-r1 -- the macOS lane's reading of the new fixture

Committed before the r1 legs; DECLARATION-PF-2b.md is kept as written. The first gate chain (`gate/orchestrate.out`) passed
every Linux leg at code commit `ae2e6729f` (P1 rel and dbg 4042/4042, P2 replay x3 0 differences, P3 TRAIN 476 rows 0 movers and
sqp B/A 1.000106, P4 26 of 26 mutations killed, P5 install smoke) and the measurement leg ran. CI run 44880880598 (head
`84461cc32`, code `ae2e6729f`): linux-clang-release, linux-clang-coverage and windows-clang-release green; **macos-clang-release
red on one test**, `SqpBackendSolveTally.PartitionsTheSolvesAndEveryComponentIsReachable`: its refinement-step expectation
(`> 0` in every reached bucket) is Pardiso's; Accelerate reports no refinement count (the adapters record 0, as the declaration
said), so those reads are 0 -- the first OBSERVED Accelerate reading of these counters. Nothing else on the lane failed (3937 of
3938).

## The change

`tests/sqp/test_backend_analysis_tally.cpp` only: the refinement expectation is `> 0` off Apple and `== 0` on Apple. No file under
`src include bench examples bindings cmake scripts` moves, so P2, P3 and the measurement stand as taken (they are properties of the
library and bench, which are byte-identical to `ae2e6729f`).

## Re-run (criteria as DECLARATION-PF-2b.md)

P1 (ctest rel and dbg, 4042 each) and P4 restricted to the control and M3 (the refinement-step mutation, the only mutation whose
killer the edit touches; M1, M2 and the rest are killed by assertions the edit leaves alone), then ONE CI dispatch of the four lanes at the
pushed head, green on all four.
