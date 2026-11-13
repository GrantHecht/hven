# M8 DN-3 -- measured dense auto-selection (DECLARATION-r2)

Successor of `DECLARATION-r1.md` (commit `d186a1332`), which stays frozen beside this file with every r1 output.
Committed before r2's first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-r3.md`.

## 0. What happened under r1 (the record, kept)

- **P5 ran first and alone and PASSED** (`runtime/run_all.log`, 21:56-22:42): counter identity in all four modes
  (interior 3, ipm 81, ssn 81, walk 81 (round, cell) pairs, 0 differences), filtered B/A interior 1.000000, ipm 0.999998,
  ssn 1.000000, walk 1.000000, VERDICT PASS.
- **The chain (`legs/chain.sh`) FAILED P1**: `suites/ctest-rel.log` "99% tests passed, 29 tests failed out of 3632"
  (22:42-22:43). The 29 are the tests that write temporary files (IpmConsole.*, SqpConsole.*, IpmStopReason.*Door*,
  Verbosity.OneLineTurnsTheConsoleOn, CorpusRunnerProcess.*, CorpusBaseline.*, IpqpAcceptanceA2.*DumpSeam*): the console
  captures read back empty, the corpus runner's CSVs are not written. Cause: `/tmp` (tmpfs, `usrquota`) was at this
  user's quota (about 13 GB of earlier rounds' scratch under `/tmp/claude-1000`); the harness itself reported EDQUOT on
  `/tmp` at 22:44. The same suites passed at the same code head at 21:09-21:17 (r1 §8). Not a code failure.
- The chain was then stopped by hand during `ctest-dbg` (`suites/ctest-dbg.log`, partial, 30 of 3634); replay,
  interior, falsifier and install smoke did not run under r1. No adoption-leg data exist.
- The scratch orchestration that ran r1 is kept as `legs/orchestrate-r1.sh`.

## 1. What r2 changes (nothing else)

- **TMPDIR** is exported to `build-dn3/tmp` (the home volume) for every leg: `legs/chain-r2.sh` (= `chain.sh` with that
  export and the r2 output directories) and `legs/orchestrate-r2.sh` (the declared order, each leg under its own flock,
  stopping at the first failure). The pre-existing earlier-round files under `/tmp/claude-1000` are not this round's and
  are not deleted.
- r2's outputs: `chain-r2/`, `suites-r2/`, `replay-r2/`, `interior-r2/`, `failproof-r2/`, `install-smoke-r2/`,
  `legs-logs-r2/`; the E, S, P, T, G legs write where r1 §10 says (none ran under r1).
- **P5 is carried, not re-run**: r1's P5 result stands for r2 (same code head `96ac1a27d`, same arm A
  `e53c29d2...` and arm B `3cb0aafb...` binaries; the gate writes nothing under `/tmp`).

Everything else is r1's, unchanged: the code head, the binaries and their sha256s, THE CHANGE, the declared differences,
P1-P4 and P6 with their criteria (P1: "100% tests passed, 0 tests failed out of 3632" in both trees; P4: head 56, the ten
WANT sets in `legs/failproof.sh`), criterion T, criterion E with its expectation (ENUM_PASS, 227 candidates as listed in
r1 §6), criterion A and the R-items, the flip clause, the NOT RUN list, the disclosures (r1 §8) and the STOP table (an
S-N on any r2 P-criterion is a STOP).

## 2. Order

```
bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8/docs/notes/data/2026-11-m8-dn3/legs/orchestrate-r2.sh all
```
(each leg inside it: `flock /tmp/box-build.lock bash <leg script> ...`, `pgrep -a hven_bench_cell` empty first.)

ON FAIL: stop, commit the evidence, report; no re-run, no criterion change after data.
