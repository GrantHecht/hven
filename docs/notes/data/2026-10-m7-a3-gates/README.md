# M7 W2 A3 -- the gates run

> Range **`96f3158e..bb512d69`** on branch `m7` (base `96f3158e` is the previous gated head, code-identical to the toolchain re-baseline record's tree).
> Contents: A3, the IPQP levers: `IpqpOptions::ipqp_ladder_skip_after` (default 3, formerly `detail::kIpqpLadderSkipAfter`) and the fire counter
> `IpqpCounters::ipqp_ladder_zero_trial_skips` (`9bab04cc`, with the E1 arm's levers and `--ladder` instrument), its tests (`3a26a5dd`) and comment-only riders
> (`bb512d69`). The rest of the range is docs and data, not touched here. A3 is claimed identity at the default.

`wrappers-gates/` is the **declaration** (committed before the first leg as `45c253e5`, never edited after); `gates/` is the **record**.
Derivations: `wrappers-gates/psym-derivation-a3.txt`; pre-flight: `wrappers-gates/preflight-a3.txt`; rehearsal: `wrappers-gates/rehearsal/`.

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 3 headers (modified) + the test-support header `tests/sqp/support/indefinite_fixtures.h`; 11 modified `.cpp`, all inside the P-SYM universe (library `sqp_options`, `sqp_solver`, `ipqp_engine`; bench `ipqp_e1_arm`; `hven_drivers_tests` `test_callback`, `test_options_dump`, `test_threads`; `hven_sqp_tests` `test_ipqp_certification`, `test_ipqp_counters`, `test_ipqp_dispatch`, `test_trace_writer`); 1 added `.cpp` inside it (`tests/sqp/test_ipqp_ladder_skip.cpp`, with its `tests/sqp/CMakeLists.txt` line); no outside-universe source, no fixture |
| G1 / G3 | Debug **3229** / Release **3230** = 3221 / 3222 + 8 added by name (all `TEST`) - 0 removed; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; the exact arm vs the toolchain re-baseline record **asserted** 0 / 0 / 0; E1 OFF vs the toolchain record's `e1/e1-off.csv`: 29 cells x 68 common columns, 0 differences, the one added column 0 on every row (asserted) |
| G5 P-SYM | Release **66** = A 11 + B 55; Debug **67** = A 11 + B 56; same 11 class A objects; class B: Debug = Release PLUS `hven_drivers_tests/test_solve_status.cpp.o` (byte-identical in Release; the carried `EXPECT_PSYM_B_DEBUG_ONLY` admission, the same single object as A2); mirror slot empty; UNEXPLAINED 0; non-debug movers 8 / 8; DEBUG_ONLY 0 / 3 |
| G6 | install smoke, export contract, clang-format (16 files), `git diff --check`; fold proofs reported as observed |
| declared moves | `ipqp_ladder_skip_after` (default 3 = the former constexpr); `ipqp_ladder_zero_trial_skips`; one options-dump line |

One **structural addition** to the runner body: the G4 exact arm's previous record and the E1 OFF baseline are derived from BEFORE (the newest record added at or
before it carrying `replay/head-walk.csv`, required to equal the declared toolchain record, its csv-header binary commit required code-identical to BEFORE), and
the g4 segment gains the E1 OFF arm compared by `e1_off_compare_a3.py`.

## Gates (chain launched 2026-10-28 23:13, worktree `.claude/worktrees/a3-gates-wt`, declaration `45c253e5`, `gates/chain-a3.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN**, first run (no `gates-run1/`).

| gate | result |
|---|---|
| G1 Debug suite | 3229 / 3229 (3231 started names = baseline 3223 + 8; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3230 / 3230 (3232 started names, 2 Disabled; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the toolchain record: **0 / 0 / 0** differences over 27 cells x 75 columns, ASSERTED (`gates/replay/exact-vs-previous-record.txt`). E1 OFF (`gates/e1/`): arm rc 0, 29 cells x 68 columns, 0 differences, `ipqp_ladder_zero_trial_skips` 0 on 29/29 rows |
| G5 P-SYM | Release 66 = 11 + 55, Debug 67 = 11 + 56; UNEXPLAINED 0 both; negative control 106 / 99; manifests 162 before / 163 after, ONLY-AFTER = `test_ipqp_ladder_skip.cpp.o`; non-debug movers = declared 8 both, DEBUG_ONLY 0 / 3 |
| G6 | install smoke PASSED, export contract OK (align 32), clang-format (16 files), `git diff --check`, `DIRTY_FILES=0` |

How the new counter was handled. The **replay csvs** carry no column for it (`bench/bench_corpus.cpp` prints an explicit column list and is not in the range; schema 76
unchanged), so G4 stays 27 x 75 and nothing needed handling. The **E1 arm csv** gains it (`ipqp_ladder_zero_trial_skips`); the base has no such column, so the
repro rule (columns common to both files, `wall_s` excluded, byte-equal) cannot see it; the comparator therefore asserts the head-only column set EQUALS the declared
single name and that the column is `0` on every row, which is what the identity claim implies at the shipped skip count on convex E1 cells (measured: 29 / 29 zero).

Observations, not deviations:
- The worktree's `dep/eigen` and `dep/fmt` were empty; populated by copying the main checkout's working trees (untracked content; the copied `.git` pointer files removed).
- The worktree was first created under `.scratch/`, where the runner's segment guard refuses any root path containing `.scratch/`; it was moved (`git worktree move`) to `.claude/worktrees/a3-gates-wt` before the pre-flight. The rehearsal logs carry the old path.
- Leg wall (informational): G1 8.7 min, G2 2.8, G3 5.5, G4 6.8, G5 34.7, G6 1.4. Every leg printed `PGREP_EMPTY` under the lock hold.
- The chain ran to completion with no leg re-run.
