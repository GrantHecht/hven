# M7 WX -- the gates run

> Range **`1ff37db2..65c3a24b`** on branch `m7` (base `1ff37db2` is the A3 gates record, the previous gated head, code-identical to `bb512d69`).
> Contents: `aa19f364` (GCC fix, behaviour-identical); W3 (bench-only: `bench_corpus.cpp --hs` applies the `--ssn-*` levers, ten appended `--hs` CSV columns; the
> replay CSV schema is unchanged); WX B0-B4 (tests, `tests/common_support/route_reference.h`, the `RouteReference` sentinel test, CI/docs, and one Apple-only library
> line, `accelerate_threads.h` setenv overwrite 1 -> 0, not compiled on Linux); WX-R1 (`7beb4801` + riders `99eeb8f3`: the SQP convergence test gates NLP
> complementarity on every major, `duals_ingested` / `duals_carried` removed, the full-step watchdog ranks by max(stationarity, feasibility, complementarity)).
> The claim under test is IDENTITY on U0 and E1; WX-R1's one moved TRAIN row (ACOPR118) is outside what these gates read.

`wrappers-gates/` is the **declaration** (committed before the first leg as `083cf826`, never edited after); `gates/` is the **record**.
Derivations: `wrappers-gates/psym-derivation-wx.txt`; pre-flight: `wrappers-gates/preflight-wx.txt`; rehearsal: `wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 3 headers (modified) + 1 added test-support header (`tests/common_support/route_reference.h`); 27 modified `.cpp`: 14 inside the P-SYM universe, 13 outside it (12 `tests/qp` sources of `hven_qp_walk_seam_tests`, `tests/sqp/test_ipqp_seams.cpp` of `hven_ipqp_seam_tests`); 1 added `.cpp` OUTSIDE it (`tests/qp/test_route_reference.cpp`, with its `tests/CMakeLists.txt` line); no `tests/sqp` or `src` CMake change |
| G1 / G3 | Debug **3231** / Release **3232** = 3229 / 3230 + 2 added by name (`RouteReference.TheReferenceBuildRunsAtMklsAutoDispatch`, `HsSweepRepair.WarmChainOnAQuarterGridCertifiesNoPointOffTheRow`) - 0 removed; skipped sets equal the baseline's (Debug 7, Release 1). The brief's 3230 / 3231 prediction was one test short (the A3 baseline is 3229 / 3230) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; the exact arm vs the A3 record's replay **asserted** 0 / 0 / 0; E1 OFF vs the A3 record's `e1/e1-off.csv`: 29 cells x 69 common columns, 0 differences, the column set UNCHANGED (head adds none, lacks none; the comparator's declared added-column set is empty) |
| G5 P-SYM | Release **6** = A 6 + B 0; Debug **7** = A 7 + B 0; UNEXPLAINED 0; class A noise-only by name: 7 modified in-universe test sources in both configurations; class A byte-identical (new slot): `src/qp/qp_engine.cpp.o` in Release (DIFFERS in Debug); class B none; non-debug movers 1 / 2; DEBUG_ONLY 0 / 5 |
| G6 | install smoke, export contract, clang-format (32 files), `git diff --check`; fold proofs reported as observed |

Three **structural additions** to the runner body, declared in its header: S1 the added-non-.cpp-test-file check admits the one declared test header; S2 the
`EXPECT_CLASS_A_IDENTICAL_{RELEASE,DEBUG}` slots (a modified source whose object is byte-identical), proven offline by `rehearsal/check-class-a-wx.sh` (preflight G);
S3 the E1 comparator accepts an empty declared added-column set. The G4 exact arm's previous record and the E1 OFF baseline are derived from BEFORE as in A3 and resolve
to the A3 record.

## Gates (chain launched 2026-10-29 15:46, worktree `.claude/worktrees/wx-gates-wt`, declaration `083cf826`, `gates/chain-wx.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN**, first run (no `gates-run1/`).

| gate | result |
|---|---|
| G1 Debug suite | 3231 / 3231 (3233 started names = baseline 3231 + 2; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3232 / 3232 (3234 started names, 2 Disabled; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the A3 record: **0 / 0 / 0** differences over 27 cells x 75 columns, ASSERTED (`gates/replay/exact-vs-previous-record.txt`). E1 OFF (`gates/e1/`): arm rc 0, 29 cells x 69 columns, 0 differences, 0 added columns |
| G5 P-SYM | Release 6 = 6 + 0, Debug 7 = 7 + 0; UNEXPLAINED 0 both; negative control 83 / 34; NOISE-ONLY 9 / 58 (0 / 5 in the library); manifests 163 before / 163 after (the one added `.cpp` lies outside the universe); non-debug movers = declared 1 / 2, DEBUG_ONLY 0 / 5 |
| G6 | install smoke PASSED, export contract OK (align 32), clang-format (32 files), `git diff --check`, `DIRTY_FILES=0` |

Observations, not deviations:
- The worktree's `dep/eigen` and `dep/fmt` were empty; populated by copying the main checkout's working trees (untracked content; the copied `.git` pointer files removed).
- The rehearsal's offline script ran with the A3 E1 added-column argument before S3 existed (its E1 line reports rc 1 with 0 differences); re-run by hand with the empty set, rc 0; the committed script carries the corrected argument.
- The class B lists are EMPTY in both configurations (the SolveState layout change moves only `sqp_solver.cpp.o`'s own offsets); the Debug-only class B admission (A2/A3) is not exercised and its proof script is replaced by `check-class-a-wx.sh`.
- Preflight E4 refuses through the Debug = Release - long consistency check before the by-name count, as A3's did; preflight E10 refuses through the class A arithmetic.
- `bench/corpus_cells.h` (modified, comment only) lies outside the runner's clang-format pathspec, a carried property of the derivation.
- Leg wall (informational): G1 8.8 min, G2 2.8, G3 5.5, G4 6.8, G5 21.2, G6 1.4. Every leg printed `PGREP_EMPTY` under the lock hold. `MKL_CBWR` unset (the sentinel test passed).
- The chain ran to completion with no leg re-run.
