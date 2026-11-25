# M8 dense gate -- DECLARATION-r2 (the successor declaration: C341's follow-ups on the rebased branch)

Committed before its first leg; never edited afterwards. Criteria are never changed after data. A failed or void leg is
answered by `DECLARATION-r3.md`, with this file and every output kept. `DECLARATION.md` (r1, code head `53fc92c93`
on `030bae675`) and its record (`RESULTS.md`, `legs-out/`) stay frozen; r1 PASSED and is not re-cut. r2 exists
because (a) the settler's C341 rulings changed code after r1 (F1, F4, the seam, F7) and (b) the branch is rebased onto
`origin/m8` `286fb5398` (the walk cap, after ap5-g1), so every reference row is re-derived against the new base.

## 0. The change and the base

Branch `m8-dense-gate`, rebased (unpushed, local) onto `origin/m8` **`286fb5398`** (walk cap ADDENDUM-r1 record). The
r1 commits carried over unchanged in content: `6636d7bc9` (r1 code, was `53fc92c93`), `0eccc8188` (r1 declaration, was
`71e50ea41`), `8608e2d10` (r1 evidence, was `d27106f5e`). The r2 code head is

**`c41c759fa5a861a32961e51024964066d8722db4`** (was `482f247a0` before the rebase, content identical; `git merge-tree`
clean, no conflict): C341's follow-ups (design note `docs/notes/2026-11-m8-dense-gate-design.md`):

- F1 the unshifted dense solve check measures eta in the equilibrated metric (D = Ruiz(K)), always (SSN step; IPQP when
  `ruiz_equilibration` is off -- a non-default option).
- F3 investigated, no code: the raw factors of a scale-cleared KKT are accurate in every block (<= 9e-16 componentwise,
  s = 1e8 .. 1e12); a pinning test added.
- F4 an SSN step from a shifted dense factor is never exempt from the Armijo test (SSN is not the SQP's default QP
  mode).
- The `DenseSolveFaultInjector` seam (`#ifdef HVEN_TESTING` in the two adapters; compiles to nothing in the library) and
  its fixtures; the IPQP's private wound removed (also `#ifdef HVEN_TESTING`).
- F7 the 1-norm estimator keeps max(est, est_old) on its no-growth exit (for a symmetric B a change only under
  rounding); F2/F5/F6 comments and documentation.

Expected effect on the corpus: none beyond r1's movers (F1/F4 reach only the SSN tier and a non-default IPQP option;
F7 only a rounding tie; the seam is test-only). Measured in the dev run (section 2).

Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, `-DHVEN_FP_MODE=SAFER_FAST`, `MKL_NUM_THREADS=1`.
Apple/Accelerate and Windows: UNOBSERVED (nothing pushed).

## 1. Binaries (frozen by `r2/legs/prebuild.sh`, `r2/dev/prebuild.out`)

- Trees `build-dg/rel` (Release), `build-dg/dbg` (Debug); bench stamp `check_bench_stamp: OK (c41c759fa5a8)`.
- Corpus binary `build-dg/dgbin-r2/hven_sqp_corpus` sha256
  `db46a7a11cfefba25cd1e7659aa03a0648f067110222649269284d4b4948c6bd`.
- Lane binary `build-dg/dgbin-r2/hven_bench_cell` sha256
  `c34d90d8e8af2787ed7e6a3ac869b3ba5d21c2e0d58f21d8624823d936216240`, built by `r2/sl/lane_setup.sh` in the LOCAL
  hven-bench clone `/home/ghecht/Projects/hven-bench-dn1` (never pushed): branch `dense-gate-legs-r2` (commit `fab548c`)
  = `dn3-flip-legs` (`adcb7d2`) with only the pin moved to the r2 code head; pin install `.hven/install-c41c759fa5a8`,
  libhven.a sha256 `248bea4b016261a5c89e4061c459ce0a3066a18ad00273c598e2f9b2106dc468`. The clone's HEAD was put back
  on `dn1b-legs`.
- Scripts: `r2/legs/`, `r2/sl/` are r1's `legs/`, `sl/` with only the binary directory (`dgbin-r2`), the output
  directories, the F1 count (an argument) and the references below changed.

## 2. Disclosures: what ran before this file (none is a leg)

- Dev suites before the rebase (scratch `build-dg/out/r2c_*`): Release and Debug 100% of 3651 on `482f247a0` /
  `030bae675`. Targeted runs of the new fixtures and two temporary probes (F3's accuracy table; F4's SSN behaviour),
  removed before the code head.
- `r2/legs/dev.sh` on the r2 code head (`r2/dev/`, `build-dg/out/r2dev.out`): prebuild (the binaries above); Release
  ctest **100% of 3671** (`build-dg/out/r2dev-ctest-rel.log`); one replay pass (three repetitions, `r2/dev/replay.out`):
  walk / ssn / ipm, 27 cells x 75 columns, **0 differences** in all 9 against the walk cap's ADDENDUM-r1 rows; one
  interior pass (`r2/dev/interior.out`, outputs `build-dg/out/r2dev-interior/`): against r1's references (DN-3 r5's run
  and the flip baseline) **2 differences each** -- `stop_reason` iteration_cap -> none on `f7_n1000_bound_neutral` and
  `hs071_x1_fixed` cap1, the base's own change (ap5-g1's declared "raw 2", `docs/notes/data/2026-11-m8-ap5-g1/
  DECLARATION.txt` D2/D3) -- and against ap5-g1's r3 references (below) **0 differences** each (`r2/dev/interior-vs-ap5g1.txt`, compared by hand with
  `scripts/compare_replay.py` after the run); so I3's references move to ap5-g1's.
- **The dev TRAIN run** (`r2/dev/identity/`, `r2/dev/enum.out`): `r2/sl/enum.sh` on the frozen lane binary, all 238
  cells x {default, never-default}, serial, against the walk cap's ADDENDUM-r1 leg rows
  (`docs/notes/data/2026-11-m8-walk-cap/addendum-r1/train/`): kNever rows **238/238 equal**; **41 default-row movers**,
  the same 41 cells as r1 with the same deltas (only SPANHYD's absolute factorization totals differ, the walk cap's own
  move: 45324 -> 45318 against r1's 47890 -> 47884), listed with their moved keys in `r2/train-movers.txt`; the adoption
  tally unchanged (dense better 3, same 235, worse 0). **The I4 criterion binds the leg's rows to these dev rows.**
- No census, install-smoke or Debug-suite run of the r2 code head before this file.

## 3. The declared movers

- **Tests**: r1's re-derivations stand (DECLARATION.md section 3). r2 re-derives none; it replaces the IPQP's private
  wound in `IpqpDenseSolveSeamTest.*` with the adapter seam (same four site readings) and adds tests (the F1 metric,
  the F7 no-growth exit, the F3 accuracy pin, `QpDenseArmWound.*`, `SsnDenseSolveWound.*`, `SsnShiftedStep.*`).
- **TRAIN**: the 41 cells of `r2/train-movers.txt` against the walk cap's leg rows (33 IPM cells: only
  `side.dn3b_kkt_dense_flagged_count`; 8 SQP cells: dense counters, SPANHYD/MESH eqp refinement counts, ALLINITC's
  kkt_stationarity in the residual gate). No status, outcome or iteration count moves.
- **Expected unmoved**: the 27-cell replay, the interior leg (against ap5-g1's references), the 57-cell census, every
  kNever row.

## 4. The legs and their criteria (fixed here, before data)

| leg | instrument | criterion |
|---|---|---|
| F1 | `r2/legs/ctest.sh` rel, dbg -> `r2/legs-out/suites/` | each log reads "100% tests passed, 0 tests failed out of 3671" |
| I1 | `r2/legs/replay.sh` -> `r2/legs-out/replay/` | 27 cells x walk / ssn / ipm, three times, against `docs/notes/data/2026-11-m8-walk-cap/addendum-r1/replay/<mode>.csv` (the base code's rows), `scripts/compare_replay.py` byte-exact, 0 differences in all 9 |
| I3 | `r2/legs/interior.sh` -> `r2/legs-out/interior/` | I3-1: the 27-cell interior run vs `docs/notes/data/2026-11-m8-ap5-g1/interior-r3/interior-B.csv`, 0 differences; I3-2: the baseline invocation vs `bench/baselines/2026-11-m8-ap5-g1-ipm-leg/interior_baseline.csv`, 0 differences (byte-exact) |
| F3 | `r2/legs/install_smoke.sh` -> `r2/legs-out/install-smoke/` | install smoke and export contract rc 0 on the default (GCC) and clang++ front ends |
| I4 | `r2/sl/enum.sh` + `r2/sl/tally.py` -> `r2/legs-out/identity/` | `I4_PASS`: I4K every `never-default` row equals the walk cap's ADDENDUM-r1 `K` row (238/238); I4D the default-row movers against the walk cap's `D` rows are exactly `r2/train-movers.txt` (none undeclared, none declared-but-unmoved); I4R every default row equals the dev run's (`r2/dev/identity/`), 238/238. Equality as r1's I4 (every .out key but `options_label` / `hven_pin_sha` plus the exit code; integers and strings byte-equal; residual-class values byte-equal or within hven-bench `docs/corun.md`'s calibrated gate, rel 1e-12, abs 1e-13; the .optdump byte-equal for I4K and I4D; no capped or missing row). The adoption tally and dense counter totals are REPORTED, not gating |
| I2 | `r2/legs/census.sh` -> `r2/legs-out/census/` | `CENSUS_AS_DECLARED`: DN-1d r3's `census_check.py` against `bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv` with PF-1's two declared movers, every non-deadline-bound row exact (r1's I2, and the walk cap's I2 reading: 0 movers) |

Protocols: as r1 (DECLARATION.md section 4): `pgrep -a hven_bench_cell` empty first, `flock /tmp/box-build.lock`,
`set -o pipefail`, FOREGROUND markers, `MKL_NUM_THREADS=1`, `TMPDIR=build-dg/tmp`, binary sha256 checked; F1 `ctest -j4`
under `setsid` with `GIT_CEILING_DIRECTORIES`; I1, I3, I4 serial (I4: 476 cell processes through DN-3's `sl/cell.sh`,
THREADS=1, core 2, CAP 300 s); I2 the census runner's own declared co-run protocol (parallel-tiered v1 at its
defaults), counters asserted, wall-clock informational only. No leg asserts wall-clock. `r2/legs/orchestrate.sh` checks
that `src include tests bench examples CMakeLists.txt cmake scripts` equal the code head and are clean.

## 5. STOP and ON FAIL

The orchestrator stops at the first failed leg: commit the evidence, report; no re-run, no criterion change after
data; a successor `DECLARATION-r3.md` after a fix.

## 6. NOT RUN

HOLDOUT (never). macOS and Windows CI (no push; Accelerate and Windows UNOBSERVED). Any wall-clock measurement. The
iparm leg (no Pardiso configuration moves). No F3 cost leg: F3 changed no code (the D K D refactor was not taken).

## 7. Order

```
flock /tmp/box-build.lock bash /home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7/docs/notes/data/2026-11-m8-dense-gate/r2/legs/orchestrate.sh c41c759fa5a861a32961e51024964066d8722db4 db46a7a11cfefba25cd1e7659aa03a0648f067110222649269284d4b4948c6bd c34d90d8e8af2787ed7e6a3ac869b3ba5d21c2e0d58f21d8624823d936216240 3671 > /home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7/docs/notes/data/2026-11-m8-dense-gate/r2/legs-out/orchestrate.out 2>&1
```
