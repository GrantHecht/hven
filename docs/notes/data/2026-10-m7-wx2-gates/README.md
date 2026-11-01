# M7 WX2 -- the gates run

> Range **`fcd2574f..aaa95642`** on branch `m7` (base `fcd2574f` is the WX gates record, the previous gated head, code-identical to `65c3a24b`).
> Contents: WX B5 / T7 / B6 / B7 (tests only: exact route pins made reference-only with portable arms, `RecordProperty`); WX-R10 (CI); WX-R13 (`tests/CMakeLists.txt`: a
> configure-time MKL dispatch probe, `tests/common_support/mkl_dispatch_probe.cpp` run by `try_run`, deciding `HVEN_TEST_REFERENCE_DISPATCH` for every test target; the added header
> `mkl_dispatch_signature.h`); CF-1 (a format-only clang-format sweep of 22 files: 14 headers, `get_core_count.cpp`, `non_linear_program.cpp`, six `tests/interior` sources); CF-1-R1
> (`EIGEN_MAX_ALIGN_BYTES` on the vendored Eigen target as well as on hven). The claim under test is IDENTITY on U0 and E1, NAME/COUNT identity of the suites, and that nothing moves
> beyond the declared class A / noise shape.

`wrappers-gates/` is the **declaration** (committed before the first leg as `17408463`, never edited after); `gates/` is the **record**.
Derivation: `wrappers-gates/psym-derivation-wx2.txt`; pre-flight: `wrappers-gates/preflight-wx2.txt`; rehearsal: `wrappers-gates/rehearsal/` (two P-SYM passes, the offline G4/E1 half, the
re-classification, the census on the rehearsal's own data, the offline class A / census proof `check-class-a-wx2.sh`).

## Declared values (derived from the range or measured by the declaration's rehearsal)

| item | declared |
|---|---|
| changed surface | 14 headers (CF-1, all `include/hven/detail/{globalization,interior}`); 18 modified `.cpp`: 14 inside the P-SYM universe (2 `src/`, 6 `tests/interior` in a unity build, 6 `hven_sqp_tests`), 4 outside it (3 `tests/qp` sources of `hven_qp_walk_seam_tests`, `tests/sqp/test_sqp_equality_basis_seams.cpp` of `hven_sqp_seam_tests`); 1 added `.cpp` WITH NO OBJECT (the `try_run` probe); 1 added test-support header; `tests/CMakeLists.txt` = exactly the 45-line WX-R13 block; top-level `CMakeLists.txt` +1 `target_compile_definitions`; no `src/` or `tests/sqp` CMake change |
| G1 / G3 | Debug **3231** / Release **3232**, **0 added, 0 removed** (checked against git: the range adds and removes no `TEST`/`TEST_F`/`TEST_P` line); started names 3233 / 3234; skipped sets equal the baseline's (Debug 7, Release 1) |
| G2 | the long test alone, 1 / 1 |
| G4 | the 43-difference set (walk 17 / ssn 13 / ipm 13) vs the M6 pin, as a SET; the exact arm vs the WX record's replay **asserted** 0 / 0 / 0; E1 OFF vs the WX record's `e1/e1-off.csv`: 29 cells x 69 common columns, 0 differences, column set unchanged |
| G5 P-SYM | Release **2** = A 2 + B 0; Debug **2** = A 2 + B 0; UNEXPLAINED 0; the 2 are `test_qp_lift_oracle` and `test_sqp_solver` (modified sources whose edits change instructions); 12 further modified in-universe sources are accounted for by name (4 NOISE-ONLY, 2 library sources IDENTICAL in Release / NOISE-ONLY in Debug, 6 unity-batch members); DEBUG_ONLY 0 / 6; non-debug movers 0 / 0 |
| G5 census | EIGEN_DELTA 50 / 50; C_PURE_DIFFERING 0 / 36; UNEXPLAINED 0 |
| G6 | install smoke, export contract (align 32), clang-format (35 files), `git diff --check`, `DIRTY_FILES=0`; fold proofs reported as observed |

Seven **structural additions** to the runner body, each declared in its header and each with a refused falsifier (`preflight-wx2.txt` W*, E5b; `check-class-a-wx2.sh` H*, C*):
W2-S1 an added `.cpp` with no object (the `try_run` probe); W2-S2 an empty ADDED name list, checked against git; W2-S3 the WX-R13 block in `tests/CMakeLists.txt` checked as a sorted
multiset; W2-S4 every HEAD-tree configure confirms `-> REFERENCE (route pins asserted)` exactly once; W2-S5 no build log names the probe; W2-S6 modified sources whose only object is a
unity batch object, plus the library-mover derivation subtracting declared NOISE-ONLY class A objects; W2-S7 the **object census** (`object_census_wx2.py`).

## Gates (chain launched 2026-10-30 01:18, worktree `.claude/worktrees/wx2-gates-wt`, declaration `17408463`, `gates/chain-wx2.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**. **GREEN**, first run (no `gates-run1/`).

| gate | result |
|---|---|
| G1 Debug suite | 3231 / 3231 (3233 started names = baseline 3233 + 0; skipped = baseline's 7) |
| G2 long test | 1 / 1 |
| G3 Release suite | 3232 / 3232 (3234 started names, 2 Disabled; skipped = baseline's 1) |
| G4 U0 replay vs the M6 pin | the declared 43-difference set (17 / 13 / 13), measured set EQUALS the declared list. Exact arm vs the WX record: **0 / 0 / 0** differences over 27 cells x 75 columns, ASSERTED (`gates/replay/exact-vs-previous-record.txt`). E1 OFF (`gates/e1/`): arm rc 0, 29 cells x 69 columns, 0 differences, 0 added columns |
| G5 P-SYM | Release 2 = 2 + 0, Debug 2 = 2 + 0; UNEXPLAINED 0 both; negative control 41 / 31; NOISE-ONLY 7 / 18 (0 / 6 in the library); manifests 163 before / 163 after; non-debug movers = declared 0 / 0, DEBUG_ONLY 0 / 6; the 6 unity members each in an object that is in both manifests and not a mover |
| G5 census | 277 objects per configuration, 216 with a compile-command delta, 50 gaining `-DEIGEN_MAX_ALIGN_BYTES=32`; Release 14 differ (9 in the P-SYM universe, 5 outside) and **none for the define alone** (C_PURE 0: all 50 define-delta objects byte-identical except 4 that a modified source or the commit stamp explains); Debug 71 differ (20 in the universe, 51 outside), **36 for the define alone** (the allocator variant), UNEXPLAINED 0 |
| G6 | install smoke PASSED, export contract OK (align 32), clang-format (35 files), `git diff --check`, `DIRTY_FILES=0` |

Observations, not deviations:
- The reference-dispatch line was printed and confirmed in every HEAD-tree configure (G1, G3, both G5 after arms): `vendor=AuthenticAMD avx2=1 fma=1 avx512f=0 mkl_auto_branch=2 (MKL_CBWR_AUTO=2) -> REFERENCE (route pins asserted)`. `MKL_CBWR` unset (the sentinel test passed).
- The worktree's `dep/eigen` and `dep/fmt` were populated by copying the main checkout's working trees (untracked content; the copied `.git` pointer files removed).
- P-SYM cannot see CF-1-R1's effect: the seam executables and the golden-rig support are outside `PSYM_GROUPS` (the runner asserts their objects are not in the manifest). The census is the instrument that classifies them; the class C ("define alone") count is MEASURED and DECLARED per configuration, not assumed.
- The census treats `-DNAME="<12 hex>"` commit stamps (`HVEN_*_GIT_DESCRIBE`, `HVEN_RIG_COMMIT`) as a class S token whose value must be each arm's own commit; a precompiled-header output (`.pch`) is an intermediate and is not censused (every object that uses it is).
- Leg wall (informational): G1 8.7 min, G2 2.8, G3 5.5, G4 6.8, G5 20.3, G6 1.4. The chain started at 01:18:10; G1 took the lock at 01:24:05. Every leg printed `PGREP_EMPTY` under the lock hold.
- The chain ran to completion with no leg re-run.
