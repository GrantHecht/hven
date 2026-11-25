# M8 I18 -- RESULTS (gate of DECLARATION-I18.md)

Code commit `c9a716eac` on `origin/m8` `331b31837`; declaration commit `7c443c973`. Chain `legs/orchestrate.sh gate ...` (stream
`gate/orchestrate.out`), 2026-11-20 15:53-16:51, every leg its own box-lane step. **ALL_LEGS_PASSED.**

| leg | result |
|---|---|
| P0 | P0_OK: code paths == `c9a716eac`, clean; base `331b31837` clean; no pending edge in `build`, `build-debug`, base `build`; lane sha256 A `207c2954...`, B `9267a8ef...` |
| P1 | Release and Debug: "100% tests passed, 0 tests failed out of 3958" (`gate/ctest-summary-*.txt`) |
| P2 | replay 27 cells x walk/ssn/ipm, A vs B differences 0 / 0 / 0, B vs dev B 0 -- `I18_REPLAY_PASS` |
| P3 | corpus 150 rows each: 14 movers = `movers-corpus.txt`; the issue's 4 rows kOptimal (8 minors; 1 factorization on the border algebra, 8 refactorizing); 0 kOptimal lost; both-optimal minors +0.04%, factorizations +0.06% -- `I18_CORPUS_PASS` |
| P4 | TRAIN 476 rows: 28 movers = `movers-train.txt`, all NEUTRAL; 0 missing/capped; every B row equals its dev row; both-optimal QP minors 64220 -> 63663 (-0.87%) -- `I18_TRAIN_PASS` |
| P5 | nlp-35 42 rows: 10 movers = `movers-nlp35.txt`, all NEUTRAL, 0 missing/capped -- `I18_NLP35_PASS` (the STEENBR cost, DECLARATION section 2, unchanged from dev) |
| P6 | control clean; A1-A5, A7, A8, L1 killed; A6 survived (recorded) -- `MUTATIONS_AS_DECLARED` |
| P7 | install smoke and export contract rc 0, default and clang++ front ends |

Gate cell rows: `gate/cells-*.tar.xz`. The orchestrator's stream was written to `.scratch/gate-orchestrate.out` and copied to
`gate/orchestrate.out` (the declaration's command line named the latter as the redirect target).

## CI

Run 44815447963 (`workflow_dispatch`, head `7c443c973`): linux-clang-release, linux-clang-coverage, macos-clang-release green on
attempt 1; **windows-clang-release red on attempt 1** with one test, `SqpElasticWitnessStart.SawpathMajor0RungSolvesFromItsWitness`
(kOptimal, the recomputed stationarity 8.38e-6 against the portable 1e-6), and **green on attempt 2** (rerun of the failed job, same
head). Read as a Windows run-to-run flip (the WX-R11 amendment: Windows is off-reference and flips run to run), on this evidence:
locally the base and the I18 code give byte-identical rows for the test under every MKL code path (`MKL_CBWR` unset, COMPATIBLE,
SSE4_2, AVX, AVX2, AVX512), and under every forced path the BASE already ends at stationarity 1.00e-6 and fails the same bound --
the bound is knife-edge off the reference build before this change. The final-head CI run is recorded below.

**Final head:** run 44830464873 (`workflow_dispatch`, head `a253e265b`, code paths == `c9a716eac`): all four lanes green on
attempt 1 (linux-clang-release, linux-clang-coverage, macos-clang-release, windows-clang-release).
