# M8 GL-0 -- DECLARATION-r3 (committed before any r3 leg; DECLARATION.md and DECLARATION-r2.md stay as written)

Run 2 (DECLARATION-r2.md, code head `fcd93dd44`): P4 promoted re-stated and PASSED (arm A shows the same 4 stale-baseline rows; B vs A on the baseline invocation: 0 differences); P5 install smoke PASSED; M mutations: control clean; c1-c6, f1, r2-r5, h1, h2 KILLED; r1 APPLY_FAIL (an instrument fault: the mutation's anchor text predated clang-format); h3 SURVIVES (no fixture held two consecutive routed QP failures at one iterate), x1 survives (reported, not must-kill). The verdict is FAIL on r1 and h3.

## r3 changes
- Instrument: r1's anchor text is corrected; mutations.sh points at the re-laid-out test tree (`tests/solver/`).
- Test: `QpCapBudget.ConsecutiveRoutedFailuresAreOneRejectionRunInTheGl0Counters` (tests/sqp/test_qp_cap_levers.cpp): three consecutive routed failures give `reject_qp_failed == 3` and the histogram `{1,1,1,0,0}`; h3 must now be killed. Test-only; no product code moves.
- Rebase: the branch is rebased onto origin/m8 `aa1330cad` (RS-2: tests re-laid out under tests/{core,linear,model,kkt,qp,warmstart,solver,trace,ipm,sqp,bench}). The rebase moves GL-0's test edits to their mapped homes (tests/solver/test_callback.cpp, test_threads.cpp, tests/trace/test_trace_writer.cpp, test_trace_v1_fixtures.cpp, golden/trace_v1/sqp_complete.jsonl) and changes no product code (src/include are byte-identical to run 1's head; bench/ differs from it only by RS-2's include-path edits).

## r3 legs, on the rebased head (all must pass)
1. Release and Debug build, Release and Debug ctest, every target: 100% passed.
2. Install smoke (default and clang) + export contract, rc 0.
3. Mutations: the full list of DECLARATION.md, control fails nothing, every must-kill killed; x1 reported.
4. The Release `hven_sqp_corpus` sha256 of the rebased head is recorded next to run 1's. RT, P3 and P4 stand on run 1's evidence because the product code is byte-identical; if the two shas are equal that statement is a proof, if they differ the difference is reported (RS-2's include-path edits in bench/ may move a build-path-dependent byte) and the replay is NOT re-derived silently -- it is reported as a rebase effect on a measured leg, with the one-line replay A-vs-B re-run as the arbiter only if the coordinator asks.
