# M8 WA-1 -- REBASE onto origin/m8 bc8ed0e12 (RS-4: include spelling, include sorting, repo-wide clang-format)

After run 1 passed on base 66cc44ccc, origin/m8 moved to bc8ed0e12 (RS-4). The three WA-1 commits (code, DECLARATION, run-1 evidence)
were rebased with no textual conflict: code `677de9540` (was `0d674bdcb`), DECLARATION `8cc5cae8f` (was `ee9847ad9`), evidence
`8df83a18d` (was `ff90e834a`; kept locally as branch `wa-1-pre-rs4`). The changed files are clang-format clean under the new
`.clang-format`. The DECLARATION and the leg files are untouched; this file and `legs/rebase-bc8ed0e12/` are new. This is a check of
the rebased head, not a successor declaration.

| check | result |
|---|---|
| Release full suite | 100% tests passed, 0 tests failed out of 3891 (`legs/rebase-bc8ed0e12/ctest-rel.log`) |
| Debug full suite | 100% tests passed, 0 tests failed out of 3891 (`legs/rebase-bc8ed0e12/ctest-dbg.log`) |
| replay 27 cells x walk / ssn / ipm, once, vs run 1's B leg rows (`replay/B-k1-<mode>.csv`), byte-exact | 0 / 0 / 0 differences |

Rebased Release corpus binary sha256 `68e6ed4ceb781f9584ec63584ab95acb03f0401bfca5eb246e72b0c725eb97c1`. TRAIN, the runtime gate and the
mutation leg were not re-run on the rebased head (RS-4 is formatter output and include spelling; the replay identity above covers the
walk's counters). Linux x86-64, clang 22.1.8, MKL 2026.2, SAFER_FAST, MKL_NUM_THREADS=1. CI on the rebased head: `CI.md`.
