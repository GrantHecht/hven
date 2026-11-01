# M7 W2 SNX + SNR -- the gates run

> Range **`5ecd0dea..fcde1bf9`** on branch `m7` (base `5ecd0dea` is the SDL-4 + SDL-6 record's commit, the previous GREEN gate record).
> Contents: SNX -- the QP stagnation reading (`a5e40888`; `QpCounters::stagnation_drops / stagnation_stops`, new `tests/qp/test_qp_stagnation_stop.cpp`,
> fixtures `SNX_BRITGAS_resto_qp{4,27}`), its riders (`b2b92bac`, `fcde1bf9`); SNR -- the SQP restoration resume reads the convergence test's inf-norm
> feasibility (`70ccae57`; `SqpCounters::restoration_feas_out`), its riders (`8aad8446`, `b2b92bac`); `13d2ea04` (ledger catch-up, docs only).

`wrappers-gates/` is the **declaration** (committed before the first leg as `97669894`, never edited after); `gates/` is the **record**.
Derivations: `wrappers-gates/psym-derivation-snx-snr.txt`; pre-flight: `wrappers-gates/preflight-snx-snr.txt`; rehearsal:
`wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 6 headers; 10 modified `.cpp` (7 inside the P-SYM universe: 2 library, 2 `hven_drivers_tests`, 3 `hven_sqp_tests`; 3 outside: `tests/qp/test_qp_{rank_repair,walk_seam}.cpp` in `hven_qp_walk_seam_tests`, `tests/sqp/test_sqp_ladder_seams.cpp` in `hven_sqp_seam_tests`); 1 added `.cpp` (`tests/qp/test_qp_stagnation_stop.cpp`, outside, in `hven_qp_walk_seam_tests`); 2 added fixtures; one CMake line (`tests/CMakeLists.txt` +1) |
| G1 / G3 | Debug **3117** / Release **3118** = 3105 / 3106 + 12 added by name, 0 removed; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; exact arm vs the SDL-4 + SDL-6 record measured 0 / 0 / 0 (recorded) |
| G5 P-SYM | Release **72** = A 7 + B 65; Debug **73** = A 7 + B 66; class A the same 7 objects in both; class B Debug = Release + `tests/drivers/.../test_solve_status.cpp.o` (byte-identical in Release); UNEXPLAINED 0; non-debug movers 10; DEBUG_ONLY 0 / 4 |
| G6 | install smoke, export contract, clang-format (17 files), `git diff --check`; fold proofs 46 / 0 |
| declared moves | Debug-only re-pins of `a5e40888` (`SqpDriverRestorationSeed.AWorseElasticCandidateIsRefusedOnTheSameModel` 1 -> 0, `SqpDriverRestorationExcursion` cramped budget 43 -> 5, `QpRdvRankRepair.SawpathQp1*` Debug certifies, stationarity 1e-5 in Debug) and their `#ifndef NDEBUG` assertions (`fcde1bf9`); the new counter keys `stagnation_drops`, `stagnation_stops`, `restoration_feas_out`; no U0 column added |

## Gates (chain pid 1850096, worktree `agent-a576df553a1d7307d`, declaration `97669894`, 2026-10-19, `gates/chain-snx-snr.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN.**

| gate | result |
|---|---|
| G1 Debug suite | 3117 / 3117 (3119 started names = baseline 3107 + 12; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3118 / 3118 (3120 started names; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the SDL-4 + SDL-6 record: **0 / 0 / 0** differences over 27 cells x 75 columns (`gates/replay/exact-vs-sdl4-sdl6-record.txt`) |
| G5 P-SYM | Release 72 = 7 + 65, Debug 73 = 7 + 66; UNEXPLAINED 0 both; class A sets identical (`drivers/sqp_solver`, `qp/qp_engine`, `tests/drivers/test_{callback,threads}`, `tests/sqp/test_{qp_cap_levers,sqp_restoration,trace_writer}`); class B sets equal by name per configuration, the one declared Debug-only object asserted byte-identical in Release against the complete negative-control listing; non-debug movers = declared 10 both, DEBUG_ONLY 0 / 4; manifests 160 / 160; the four outside sources (three modified, one added) built in exactly one listed outside target each, no outside object in either manifest |
| G6 | install smoke, export contract, clang-format (17 files), `git diff --check`, `DIRTY_FILES=0` |

Every move the range makes was declared and PASSED in both configurations inside the green suites: the 12 added tests (`QpStagnationStop` 11, `QpCapRetry.AStagnationStopIsNotRetried`),
the Debug-only re-pins and the `#ifndef NDEBUG` assertions of `fcde1bf9`, the re-premised SNR fixtures. The new counter keys are folded (fold proofs 46 / 0). None of the 27 U0 cells moves
a single column against the previous record (exact arm 0 / 0 / 0), so the stagnation reading and the resume-test change do not reach the replay; nothing in G4 or G5 moved beyond the declaration.

Observations, not deviations:
- Negative control was 94 (Release) and 83 (Debug) in the chain. The rehearsal's offline Release classification read 95: its full-tree build also listed the outside-universe object `tests/CMakeFiles/hven_qp_walk_seam_tests.dir/qp/test_qp_stagnation_stop.cpp.o` (an object the BEFORE arm does not build); class A / class B sets are identical.
- Leg wall (informational): G1 8.9 min, G2 2.8, G3 5.4, G4 5.6, G5 37.4, G6 1.4. Every leg printed `PGREP_EMPTY` under the lock hold.
- `gates/psym-sections-{Release,Debug}.txt` keep the section checker's summary and mover lines only.
- The worktree was created without its submodules; `dep/eigen` and `dep/fmt` were populated from the main checkout's working trees (same gitlinks), untracked-clean. A first rehearsal attempt stopped at cmake configure (rc 91) before any build; the rehearsal that measured the declaration is the second (log in `wrappers-gates/rehearsal/`).
- The rehearsal ran at a temporary docs-only commit `f7b8dad8` (the wrappers, above `fcde1bf9`), reset away before the declaration was committed; its log records that hash.
