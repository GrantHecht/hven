# M7 W2 P1E + C13 -- the gates run

> Range **`d495e15a..feca4400`** on branch `m7` (base `d495e15a` is the W1 + CERT-1 record's commit, the previous GREEN gate
> record). Contents: `16f02c0e` (ledger, docs only); P1E `d3d4c922` (the C11 YORKNET / SAWPATH major-0 fixtures) and `63e31aad`
> (three SqpOptions levers, default OFF: `elastic_witness_start`, `elastic_last_optimal_rung`, `restoration_resume_reduction`;
> SqpCounters 61 -> 63, SqpOptions 33 -> 36, dump 65 -> 68 lines); C13 `71fed56c` (`QpEngine::PassOverScan` + `detail::RowSubset`,
> behaviour-identical) and `feca4400` (four comment riders).

This directory holds the **declaration** (`wrappers-gates/`) and, once the chain has run, the **record** (`gates/`). The run
executes in its own git worktree, never in the settler's tree; the runner derives its root from its own location.

## Declared values (every one derived from the range or measured by this declaration's rehearsal)

Derivations and their evidence: `wrappers-gates/psym-derivation-p1e-c13.txt`. Pre-flight transcripts and falsifiers:
`wrappers-gates/preflight-p1e-c13.txt`. Rehearsal evidence: `wrappers-gates/rehearsal/`.

| item | declared |
|---|---|
| changed surface | 5 headers (`solver_counters.h`, `elastic.h`, `qp_engine.h`, `ws_independence.h`, `sqp_solver_types.h`); 11 modified `.cpp` (10 in the P-SYM universe, 1 outside: `test_sqp_ladder_seams.cpp`); 1 added `.cpp` INSIDE the universe (`tests/sqp/test_elastic_witness_start.cpp`); 4 added fixtures; `tests/sqp/CMakeLists.txt` +3 non-comment lines |
| counters | SqpCounters 61 -> 63; SqpOptions 33 -> 36 (asserted by the suite's own tests, not by the runner) |
| G1 / G3 suites | Debug **2937** / Release **2938** = W1 + CERT-1's 2930 / 2931 + 7 added by name, 0 removed; skipped sets equal the baseline's by name (Debug 7, Release 1); the by-name baseline is bounded to W1 + CERT-1's `gates/` |
| G2 | the long test alone, 1 / 1 |
| G4 | the declared 43-difference set (walk 17 / ssn 13 / ipm 13), as a SET under the 1e-5 relative / 1e-13 absolute gate; re-measured exact-identical to W1 + CERT-1's replay |
| G5 P-SYM | Release DIFFERING **62** = class A 10 + class B 52; Debug **63** = class A 10 + class B 53 (one declared Debug-only class B: `test_solve_status.cpp.o`, byte-identical in Release); UNEXPLAINED 0; non-debug section movers 10 both configs; DEBUG_ONLY 0 / 3; manifests 151 -> 152 (one ADDED in-universe object) |
| G6 | install smoke, export contract, clang-format (17 files), `git diff --check`; fold proofs 46 / 0 |

## Runner changes against W1 + CERT-1's runner (nothing else)

Re-pointed names / scratch / label / BEFORE; the by-name baseline pinned to W1 + CERT-1's `gates/`; the uncapped classifier is a
byte-identical copy; outside-universe declaration is one modified source and no added one; ONE added in-universe source
(EXPECT_PSYM_ADDED 1); a NEW pre-flight check on `tests/sqp/CMakeLists.txt`'s added lines (EXPECT_SQP_CMAKE_ADDS); the
fixture-directory check changed from "named once" to "named twice, once in each of the two reader targets' compile definitions";
the offline rehearsal script's EXACT arm names the previous GREEN record (W1 + CERT-1's `gates/replay`). Details: runner header and
sections 0 / 2 of the derivation.
