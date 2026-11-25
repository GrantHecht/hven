# DECLARATION-PF-7 -- small-problem fixed overhead and the instrumentation cost

Committed before the first gate leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is answered by `DECLARATION-PF-7-r2.md`.
HOLDOUT is never run. hven branch `pf7` at m8 `331b31837`; code commit `dc0aacc30c94ef292cda35840ac977a7d8126438` (this file and the instruments come after it and add only evidence).

## 1. What was attributed (dev, callgrind; `dev/`, `tools/cg_solve.py`, `tools/cg_classes.py`, disclosed; none of it is a gate leg)

Instrument: `valgrind --tool=callgrind` on the lane binary built from base `331b31837` (arm A), cells hs033/hs012 (both lanes), BT11 (both), ALSOTAME (sqp), default options; `perf stat -e instructions:u` for all totals.
Per-cell instruction counts from `perf stat` (dev, 44 HS cells, A): median 6.71M (ipm), 8.42M (sqp), matching PF-3's 6.8M / 8.5M.

Net solver instructions = inclusive cost of `Ipm/SqpSolver::solve` minus the once-per-process OpenMP/MKL bring-up (`__kmp_do_serial_initialize`, reached from `ensure_solver_initialized` on the
ipm lane and from the first LAPACK call on the sqp lane; PF-6's subject, not changed here). Findings:

- **IPM tiny cells (hs012, hs033, BT11): net 0.57-0.65M; `hven::detail::get_core_count()` is 0.285M of it (~45 %)**, called by `IpmSolverImpl`'s constructor: it parsed `/proc/cpuinfo` line by line through `std::ifstream`, copying every line (the long `flags` lines included). That is the one fixable per-solve setup cost found.
- **SQP tiny cells (hs012, hs033): net 1.1-2.3M, of which the dense arm's LAPACK is ~1.0M (333 + 143 `dsytrs` calls on 3x3-ish matrices)**; hven's own frames in those solves total under 1 % each (largest self cost `SchurComplement::solve` 0.76 %, owned by the pf4 branch). The wrapper layers PF-3 flagged cost, per call: `LAPACKE_dsytrs_rook_work` self 52 + `DSYTRS_ROOK` 88 instructions of ~1 800; `MKL_Set_Num_Threads_Local` x2 = 141; `LAPACKE_dsytrs` nan-check pair 316 (the check is behaviour: a non-finite factor or right-hand side is refused with info < 0; not removed). The remaining ~1 400 per call are inside MKL's `mkl_lapack_dsytrs(_rook)` -> gemv/ger/swap/scal kernels. Column-major `_work` calls make no copies and no transposes, so there is no transposition cost to remove; replacing MKL's `dsytrs` by hand-written loops would change the rounding (gemv/ger FMA order is MKL's) and is not neutral.
- Allocation: `malloc`+`free` inclusive ~0.4M of a 1.1M sqp-hs012 net, but most callers are the harness (problem construction, `small_dense_sources`); solver-side callers are `SchurComplement::solve` and Eigen resizes (~0.1M), in the files pf4 owns. Not touched.
- Options validation, workspace construction, `prepare_solve`, `SolveState`/`MajorState` constructors, ledger/trace plumbing: each below 0.1 % of the cell (13 to 70k instructions per solve in total).
- `EIGEN_INITIALIZE_MATRICES_BY_ZERO` (memset 1.3-1.9 % of sqp cells) is a project-wide build define; not touched.

## 2. The change (code commit)

`src/util/get_core_count.cpp` (Linux branch only; Windows/Apple branches unchanged):
(a) `/proc/cpuinfo` read with `fopen`/`fread` into one buffer and scanned in place for the `physical id` / `core id` keys (same trimmed-key rule, same `atoi` of the value, same
"non-empty line with no colon -> hardware_concurrency()" and "no core id -> hardware_concurrency()" fallbacks, same unique (physical id, core id) pair count);
(b) the platform probe is taken once per process (`static const`); `std::thread::hardware_concurrency()` is read on every call, so the result is still `min(tcount, probed)`.
New unit test `tests/core/test_core_count.cpp` (3 tests): range, repeated/concurrent calls agree, and equality with the old stream parse kept as the reference.
Behaviour: none; the value is bit-identical on this machine and every solver counter/status/objective is unchanged (section 4, L3). The only difference: a `/proc/cpuinfo` that changes mid-process (CPU hotplug) is no longer re-read.

Not changed, with the evidence in section 1: the LAPACKE calls (no neutral gain), the thread-scope pair, the Schur/KKT files (pf4's), allocation churn.

## 3. The instrumentation-cost experiment (not a gate; evidence `dev/icount-{A,NI}-*.csv`, to be re-taken at the gate)

Variant NI = base `331b31837` sources with every guard of an emission site replaced by its constant (`trace_ != nullptr`, `ipqp_trace_ != nullptr`, `user_trace_ != nullptr`, `ledger_ != nullptr` -> `false`;
`ledger_ == nullptr` -> `true`; the `TraceSink *const sink = this->trace_` initialiser -> `nullptr`) in `ipm_solver.cpp`, `sqp_solver.cpp`, `qp/ipqp/ipqp_engine.cpp`, `qp/walk/walk_engine.cpp` (55 lines; the compiler then drops every emission body).
`tools/ni_patch.py` reproduces it from the base. Compared with A over HS TRAIN (44), nlp-35 full cells (34), small-dense TRAIN (194), default options, co-run width 4 on pinned physical cores, instructions:u.
On the ipm lane no ledger is attached by the harness and no trace sink: A-vs-NI = the cost of the OFF guards. On the sqp lane the harness attaches a Ledger always (and no trace sink): A-vs-NI = the guards + the ledger-ON cost. Status, iterations, factorizations, objective and every non-ledger counter must be equal.

## 4. Gate legs and criteria (fixed here)

All under `box-lane --` (one step per leg), `MKL_NUM_THREADS=1`; wall-clock is not asserted. Builds: arm A = `git archive 331b31837` + `dep/` (`.scratch/base-src`, `build-A-rel`), arm B = this worktree (`build-rel`, `build-debug`), flags as `legs/build.sh`.
Lane binaries (private bench clone `/home/ghecht/Projects/hven-bench-pf7` at hven-bench `e03cbad`, unmodified, pin prefix per `legs/../.scratch/build_variant.sh`): A `b42f816bced119b5b58eb761cf483b6256c038a8f9f5eacf183877094507910b`, B `7fd02391a62c69314653f8e8e60908fc348d949687a7b4f029633388343ab9fa` (built after the last source edit; `src/` of the working tree equals the code commit).

| leg | instrument | criterion |
|---|---|---|
| L1 | `legs/ctest.sh` Release, Debug on arm B | "100% tests passed, 0 tests failed"; the three `CoreCount.*` tests present; the total test count is recorded (m8's last gate count + 3) |
| L2 | `legs/replay.sh` arm A and arm B | 27 cells x walk, ssn, ipm; `scripts/compare_replay.py` A vs B reads "differences: 0" in each of the three |
| L3 | `legs/train.sh` A and B | TRAIN roster 238 cells, serial, one pinned CPU; every cell row (all keys except wall/seconds/time) equal A vs B; no DNF; no missing row |
| L4 | `tools/icmp.py` on L3 | B/A instruction table for HS and small-dense TRAIN reported per lane (sum, median, min, max); asserted: no cell above 1.005 (0.5 %) |
| L5 | `legs/mutations.sh` | control passes (3 tests run); K1-K4 (section 5) each fail `CoreCount.*` |
| L6 | `legs/install_smoke.sh` | smoke + export contract rc 0 on both front ends |
| L7 | instrumentation experiment, re-taken on the gate binaries (variant NI rebuilt from the base) | reported; asserted only identity (no non-ledger difference) |
| CI | push `pf7`, `gh run` on its head | all four lanes green |

Instruction counts are deterministic at one MKL thread (PF-3: <= 0.03 % run to run above 1e9 instructions; 0.2-1.2 % on 8-17 MI cells, so HS-scale effects under 1 % are reported but the L4 bound is 0.5 % per cell upward). A cell outside it is re-run alone before it is called a deviation.

## 5. Mutations

| id | what | expected |
|---|---|---|
| K1 | `core id` key length check broken (8 for 7) | killed by `CoreCount.MatchesTheStreamParseOfProcCpuinfoOnThisMachine` |
| K2 | pair dedupe lost (every core id line counted) | killed (same test) |
| K3 | count + 1 | killed (same test) |
| K4 | cached probe replaced by 1 | killed (same test) |
Equivalent on this single-socket machine and therefore not listed: dropping the `physical id` update (it is 0 everywhere here), and dropping the `min(tcount, ...)`.
