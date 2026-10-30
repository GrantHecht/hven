# M7 toolchain re-baseline, 2026-10-27 system update

Declared event (settler ruling C133): every pinned replay value was measured before the 2026-10-27 `dnf upgrade --refresh` (dnf history id 98, 2026-10-28 01:01 local).
This record re-derives them on the new toolchain at the unchanged source pin, `m7` `a869e7f3` (code identical to the A2 gates' range head; the A2 gates record is the "old" side).
No source change was needed: **no build fixes, no commits other than this record.**

## Provenance (old -> new)

| component | old | new | note |
|---|---|---|---|
| Intel oneAPI MKL (`intel-oneapi-mkl-devel`) | 2026.1.0-236 | **2026.2.0-881** | the only numerically relevant mover; hven links MKL statically (`libmkl_intel_lp64.a`, `libmkl_intel_thread.a`, `libmkl_core.a`) via `/opt/intel/oneapi/mkl/latest`, so each binary embeds the new kernels |
| glibc | 2.43-8.fc44 | **2.43-9.fc44** | libm is dynamically linked; Debug-class transcendental changes would show here |
| clang / llvm | 22.1.8-4.fc44 (installed 2026-07-24) | 22.1.8-4.fc44 | unchanged |
| gcc / libstdc++ | 16.2.1-2.fc44 (2026-08-22) | 16.2.1-2.fc44 | unchanged |
| cmake | 4.3.0-1.fc44 | 4.3.0-1.fc44 | unchanged |
| clang-format | 22.1.8 | 22.1.8 | unchanged |
| kernel | 7.1.8-200 (removed by the transaction; 7.1.9/7.2.4 also installed) | running **7.2.12-200.fc44** | |
| oneAPI compiler runtime (libiomp5) | -- | `intel-oneapi-compiler-dpcpp-cpp-runtime` 2026.2.0-863 installed in the same transaction | `libiomp5.so` resolved from `/opt/intel/oneapi/compiler/latest` |

Also in the transaction: nvidia 615.71.09, KDE 26.08.1, firmware 20261019 (irrelevant). MKL path layout: `/opt/intel/oneapi/mkl/{2025.3,2026.0,2026.1,2026.2,latest}`; `latest` now resolves to 2026.2 and the repo's Find module picked it up with no path change (`MKL_*_LIBRARY` lines in `logs/configure-release.log`). No environment variables (`MKLROOT`, setvars) are set in the shell.

## Legs (all run under `flock /tmp/box-build.lock`, `MKL_NUM_THREADS=1`, ctest `-j4`, PGREP_EMPTY before each; `wrappers/run.sh` + `wrappers/legs.sh`, transcript `logs/chain.log`)

Fresh build dirs, configure lines copied from the A2 gates runner (`-G Ninja`, `/usr/bin/clang++`, tests ON; Release adds `HVEN_BUILD_BENCH=ON`).

| leg | result |
|---|---|
| Release configure + build | clean, rc 0; 19 compiler warnings, same count as the A2 gates' build log |
| Debug configure + build | clean, rc 0; 19 warnings, same as A2 |
| G3 Release suite | **3222 / 3222 passed**, skipped set 1 (as A2) |
| G1 Debug suite (long test excluded) | **3221 / 3221 passed**, skipped 7 (as A2) |
| G2 long test alone (`Continuation.ProbeBudgetBoundsAFailingProposal`, Debug) | 1 / 1 passed (170 s) |
| install smoke (`scripts/check_install_smoke.sh`) | PASSED (both solvers converge through the generic consumer; 19 standalone-include TUs) |
| export contract (`check_export_contract.sh`, align 32) | OK |
| `git diff --check` | clean |
| clang-format | see below |

**clang-format.** The formatter version did NOT change (22.1.8), so this is not a toolchain effect. The A2 gates only ran `--dry-run -Werror` over the range's changed files; this re-baseline ran it over every tracked C++ file in `include/ src/ tests/ bench/` (439 files) as a wider probe and it reports 167 diagnostics in 18 files, all in legacy-migrated interior/globalization headers and tests (`super_scalar_traits.h` 52, `tests/interior/test_watchdog.cpp` 20, `restoration.h` 11, ...; full list in `logs/clang-format.log`). These pre-date this event; nothing was reformatted. Registered as an observation for the settler, not a gate failure.

## U0 replay (G4 shape: `hven_sqp_corpus --engine {walk,ssn,ipm}` over `u0-replay-cells.txt`, `taskset -c 2`, 27 cells x 75 columns per engine)

* **Exact arm vs the A2 gates record (`2026-10-m7-a2-gates/gates/replay/head-*.csv`)**: 0 differences on walk, ssn and ipm (CMP rc 0 each; `replay/exact-vs-a2-gates-record.txt`). By a separate column-by-column read, the only column that differs anywhere in the three CSVs is `wall_s` (informational). So residual-class columns are also bit-identical here: the new MKL + glibc moved **no** counter, status or residual on any U0 cell.
* **Gated arm vs the M6 close pin (`2026-09-m6-close-gate/replay`, gate 1e-5)**: differences walk 17 / ssn 13 / ipm 13 = **43**, the declared 43-set. The comparison transcript is **text-identical** to the A2 gates' `replay/comparison.txt` (every cell, column and A/B value, modulo the header line), so the 43-set is reproduced exactly, values included.
  * exact-class (counter/status): 5, all walk `qp_minors` (2 -> 3 on `f7_n{1000,2000,5000,10000,20000}_bound_neutral`); no status, factorization or other counter differs on any engine.
  * residual-class: 38 `kkt_residual` (15) / `kkt_stationarity` (17) / `kkt_primal` (6) entries (walk 12, ssn 13, ipm 13), all far below the gate (1e-5; values ~1e-13 vs ~1e-16).
  * **new differences against the declared set: 0.**

## E1 OFF arm

`hven_sqp_ipqp_e1_arm --gate` (OFF) vs `docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv` (via `2026-10-m7-a1-impl/scripts/i2_compare.py repro`, `wall_s` excluded): **29 cells x 65 columns, 0 differences** (`e1/repro-vs-a4-gate-mu1e-2.txt`).

## Conclusion and the proposed new baseline

The toolchain event moved nothing the pins assert: counters, statuses and residuals are all bit-identical to the pre-update gates record (U0 exact arm 0/0/0, E1 OFF 0). The proposed new baseline for the next gates to assert against is this record's own `replay/head-{walk,ssn,ipm}.csv` and `e1/e1-off.csv`; the CSVs are byte-equal to the A2 gates' except `wall_s` and the provenance header (binary `a869e7f371e7`, new worktree path/host date). Suite counts are unchanged (Debug 3221, Release 3222, long 1; skipped 7 / 1), so the A2 gates' suite-names baselines remain valid.

Caveats: the exact-arm result is for the 27 U0 cells and 29 E1 cells only; the Pardiso-heavy suites passing says no more than that. The harness RETAKE19 (next step in C133) is not run here. Wall-clock figures in the CSVs are informational (single-threaded, serialized).
