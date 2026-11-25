# M8 DN-3 -- measured dense auto-selection (DECLARATION)

This file is the declaration. It is committed before any DN-3 leg and before the implementation it
measures, and it is never edited afterwards. A ruling that changes a criterion below, or a failed
leg, gets a successor `DECLARATION-r1.md` (then `-r2`, ...), committed before the next leg; this
file and every output stay beside it. Criteria are never changed after data.

Authority: `docs/notes/2026-11-m8-brief.md` DN-3 (`:286-295`); the design,
`docs/notes/2026-11-m8-dn3-design.md` (base `m8` at `b70e45cfb`); the settler's binding inputs
I1-I9 as relayed to the design lane; the owner's 2026-11-08 platform ruling (I6).

## 0. Status of this declaration

**HELD.** No leg in §4 runs until the settler has ruled on design Q1-Q5 and Q7. The criteria below
are written for the design's proposals: P-A, per-engine adoption, IPM `kSparseFallback` + guard, the
1-thread pin under every dense policy, I4 discharged by C283 + the detector, `kAuto` shipped as
opt-in. A ruling that differs from any of them is answered by `DECLARATION-r1.md`, committed before
the first leg. Q6 (the counter) changes only a reported column (§3 R-items), never a criterion.

No DN-3 data exist at this commit. Disclosed: the P-A threshold values (T_SQP = 105, T_IPM = 167)
are a deterministic function of frozen, already-published data (DN-1 design §4) and were known
before this file was written. No adoption-leg row (§3) has been produced or seen.

## 1. The threshold rule (criterion T)

T1. For each engine E ∈ {IPM (`hven-ipm` cells), SQP (`hven-sqp` cells)}, read
`docs/notes/data/2026-11-m8-dn1-design/matrices.csv.gz`, which must hash to sha256
`f204be2c2a444b8fcdda8081a53f5940e04fc8ab8cf858b4c257f6e9b53b078c` (else STOP S-T0).
rho(m) = `dense_total / sparse_total` per row; a missing, zero-denominator or non-finite value
counts as rho ≥ 1. **T_E = (the smallest n among E's matrices with rho ≥ 1) − 1**, or E's largest
replayed n if there is none.

T2. Density is recorded, not used. nnz / n² per matrix is read from DN-0's `index.tsv` dumps and
reported per size bin beside T_E. No DN-1 close declaration pre-registered a two-coordinate rule
(design §2.1).

T3. Expected: T_SQP = 105 (first: `sd-hven-sqp-CRESC50 mat_0049`, n 106, rho 1.58) and T_IPM = 167
(first: `sd-hven-ipm-LAKES`, n 168). A different result is STOP S-T1: the input or the instrument
has drifted.

T4. Instrument: `tools/threshold.py` (planned; written, committed and dry-run against nothing
before L1), output `results/threshold.txt`.

## 2. The identity criteria (the N set and the I set)

N: the implementation head (`kAuto` accepted, the default still `kNever`, R1-R5 of design §7.3)
against its parent (arm A), at the defaults. The DN-1d r3 N set, instruments copied from
`docs/notes/data/2026-11-m8-dn1d-singular/legs/` and `runtime/` and re-pointed:

| # | criterion |
|---|---|
| N1 | compile-flags diff: no TU's flags differ (`legs/flags.sh`, `flags_check.py`) |
| N2 | 27-cell replay × walk/ssn/ipm, 0 differences, ×3 (`legs/replay.sh`, `scripts/compare_replay.py`) |
| N3 | 57-cell census: every non-deadline-bound row exact against arm A (`legs/census.sh`, `census_check.py`; DN-1d P5's criterion) |
| N4 | runtime gate: counter identity in all four modes, THROTTLE 0, period 400000; filtered B/A within the ±0.5 % corpus bar (`runtime/run_all.sh`, `analyze_r11.py`) |
| N5 | iparm dump byte-identical (`legs/iparm.sh`); interior unmoved and promoted (`legs/interior.sh`) |
| N6 | suites Release and Debug, examples, install smoke, export contract, PCH (`legs/ctest.sh`, `examples.sh`, `install_smoke.sh`, `misc.sh`) |
| N7 | objects: the differing set listed and every `src/linear/` object other than `dense_session.cpp.o` and the two adapters byte-identical (`legs/objects.sh`) |
| N8 | falsifier: each mutation fails exactly its declared tests (`legs/failproof.sh`, rewritten for DN-3: threshold ignored, pin removed, `kAuto` refused at one engine, threshold dropped at one consumer) |
| N9 | Accelerate syntax sweep pass A as declared (`legs/accelerate_sweep.py`) |

Declared differences at N (the only ones): the R1-R5 changes; with the R1 pin, `kAlways` at the
default thread setting may move (it is not on any N leg, which run at `MKL_NUM_THREADS=1` or at
`kNever`); new tests; doc comments. Nothing at `kNever` moves.

I: the flip commit (`kAuto` the default for each engine §3 adopts) against the implementation head
at `kNever`:

| # | criterion |
|---|---|
| I1 | 27-cell replay × walk/ssn/ipm, 0 differences, ×3 |
| I2 | 57-cell census, every non-deadline-bound row exact against the implementation head |
| I3 | iparm dump byte-identical; interior leg unmoved |
| I4 | counter identity on the 238 TRAIN cells: the flip commit's `default` label equals the implementation head's `auto-default` label in status, iterations, factorizations and every engine counter, per cell (one serial pass, `MKL_NUM_THREADS=1`) |

## 3. The adoption criterion (criterion A)

A0. Population: DN-0's 238 TRAIN cells (HS TRAIN 22 + small-dense CUTEst TRAIN 97, both hven
lanes), roster `docs/notes/data/2026-11-m8-dn0/raw/roster.tsv` re-checked by DN-0's
`tools/roster.py` (split-v1, holdout guard). **HOLDOUT is never run.**

A1. Arms: one lane binary built against the implementation head's install; labels `default`
(`dense_factorization = kNever`) and `auto-default` (`kAuto`, `max_dense_dimension` empty).
Per cell: DN-0's argv (`--lane L --problem P --options LABEL --budget-iters 0 --options-dump F
--assert-reference`), wall cap 300 s, `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `common.threads = 1`,
`taskset -c 2`.

A2. The counter, per (cell, arm):
- `raw` = the median of three `perf stat -x, -e instructions:u,cycles:u` rounds (whole process,
  all threads), rounds in the order N A A N N A after one untimed warm-up per arm;
- from `perf record -e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains -m 1M`
  on the CFI record build (A3), period-weighted over all samples:
  - `s_init` = the share whose chain holds any frame matching
    `hven::detail::ensure_solver_initialized|mkl_get_max_threads|MKL_Read_ThreadENV_Count_Cores|`
    `__kmp_(do_)?(serial|middle|parallel)_initialize|__kmp_internal_begin|`
    `(mkl_serv_)?dsecnd|mkl_serv_get_clocks_frequency`;
  - `s_proc` = the B_proc share by DN-0's frozen `tools/bucket_map.tsv` and its leaf-to-root rule,
    over the samples not in `s_init`;
- **solver instructions = raw × (1 − s_init − s_proc).** This is the criterion's counter. `raw` and
  `raw × (1 − s_init)` are reported beside it;
- PERIOD: base `max(400009, ⌊T/8000⌋) | 1`, T = the arm's stat median; `R = min(40, ⌈8000·base/T⌉)`
  repeats, repeat k at `(base·(1000+7k)/1000) | 1`. Every period ≥ 400,000. The sampling standard
  error of `s_init + s_proc` (binomial over samples) is reported per cell.

A3. CFI record build: the implementation head built with DN-0 r1's `cfi_launcher.sh`
(`-fasynchronous-unwind-tables` appended), the lane linked against it; every executable section
byte-identical to the stat binary's (`code_identity.py`), else STOP S-A1.

A4. Per cell: converged = status optimal or acceptable. Win: auto-default converges and default
does not, or both converge and auto ≤ 0.90 × default (solver instructions). Loss: the mirror.
Otherwise tie. A crash, a throw out of the lane, an `error` outcome or the wall cap = not
converged.

A5. Per engine E, per family F ∈ {HS TRAIN, small-dense TRAIN}: sign = sign(wins − losses).
**`kAuto` becomes E's default iff at least one of E's families has sign +1 and none has sign −1.**
The I set (§2) must then pass on the flip commit, or the flip does not land.

A6. Applied mechanically by `sl/adopt_verdict.py` (planned), output `results/verdict.txt`. The
verdict is recorded whether it adopts or not.

Reported, never gating (R-items):
- R-a per cell, both arms: status, objective and the relative difference (flagged above
  1e-8 · max(1, |f|)), iterations, factorizations, raw / s_init / s_proc / solver instructions,
  the margin to the 10 % boundary in standard errors, and the dense counters (SQP:
  `dense_platform_retakes`, `dense_flagged_factorizations`, `dense_singular_refactors`,
  `dense_shifted_solve_refusals`, `dense_shifted_k0_fallbacks`, `dense_shifted_terminations`; IPM:
  `dense_residual_ratio_triggers`);
- R-b by name: IPM ALLINITC and SQP HS91 (DN-1's `kAlways` movers at n ≤ T);
- R-c the cell's dense share (all-dense / mixed / all-sparse), only if design Q6 is approved;
- R-d the truncated-chain share: main-thread samples with no hven frame and a leaf in libiomp5,
  libc or ld.so, per cell;
- R-e the four median cells (hs026, HS67 on the IPM; hs043, HS84 on the SQP) at the engines' default
  threads (`common.threads = 0`, MKL variables unset, unpinned), both arms, raw and solver
  instructions;
- R-f a gdb call-count probe on the four median cells, both arms
  (`init_probe_gdb_count.sh` pattern): entries of `MKL_Read_ThreadENV_Count_Cores` and
  `__kmp_serial_initialize` per process.

## 4. Legs, instruments and order

Every leg: its own script, `pgrep -a hven_bench_cell` empty first (PGREP_EMPTY marker), then
`flock /tmp/box-build.lock bash <script>` with `set -o pipefail`, FOREGROUND_START/END markers, the
real pid waited on; binary sha256 checked against `tools/expected.sha256` before the first cell.
Instruction-count legs are serial, alone on the box (CLAUDE.md §7). The identity legs (N2-N5, I1-I3)
keep DN-1d's declared protocol. hven-bench is used through a LOCAL clone (DN-1's
`/home/ghecht/Projects/hven-bench-dn1` pattern, never pushed); the main harness is not modified.

| leg | when | what | instruments (planned paths, under this directory) |
|---|---|---|---|
| L0 | before anything | the settler's rulings on Q1-Q5, Q7; successor declaration if any differs | — |
| L1 | after L0 | criterion T | `tools/threshold.py` → `results/threshold.txt` |
| L2 | after the implementation round lands | the N set | `legs/` (copied from DN-1d r3, re-pointed), `runtime/` |
| L3 | after L2 passes | criterion A, both arms, 238 cells; then R-e, R-f | `sl/dn3_cell.sh`, `sl/hb_setup_f.sh`, `sl/hb_patch_f.py` (adds `auto-default`), `sl/hb_restore.sh`, `sl/adopt_run.sh`, `sl/adopt_analyze.py`, `sl/adopt_verdict.py`, `sl/init_markers.txt`, `tools/build_cfi.sh` + `tools/code_identity.py` (from DN-0 r1), `tools/bucket_map.tsv` (DN-0's, byte-copied) |
| L4 | after L3 adopts for ≥ 1 engine | the flip commit; the I set | `legs/` re-pointed to the flip commit; `sl/counter_identity.py` |
| L5 | after L4 | macOS and Windows CI lanes on the flip commit | the PR's CI; result recorded with run URLs |

## 5. STOP conditions

| id | condition | then |
|---|---|---|
| S-0 | any leg before L0's rulings | refused by this declaration |
| S-H | any HOLDOUT name in a roster | abort before the first cell |
| S-T0 | `matrices.csv.gz` sha256 differs | STOP; settler |
| S-T1 | T_E differs from §1 T3 | STOP; settler |
| S-N | any N criterion fails | STOP; one merged fix round, then a successor declaration re-derived on the new head |
| S-A0 | a binary sha256 mismatch, a dirty harness clone, or a lane binary not built against the declared install | refused |
| S-A1 | the CFI record build's executable sections differ from the stat binary's | STOP; instrument defect |
| S-A2 | any throttled perf record in a cell | that cell's record is VOID and re-run once alone; a second throttle is STOP |
| S-A3 | `s_init` = 0 on every IPM cell of the `default` arm (init is known to occur there) | VOID: the marker set matched nothing, instrument defect |
| S-A4 | R-d > 0.01 of solver instructions on any of the four median cells | STOP; settler (the symbol-set exclusion is not adequate) |
| S-A5 | a cell's status differs between its three stat rounds within one arm | that cell is re-run alone under the serial rule; a reproduced difference is STOP |
| S-I | any I1-I4 difference | the flip does not land; STOP; settler with the cell, the factor and its n (design Q8) |
| S-P | a CI lane already red on the pre-flip head | STOP; settler (I6's condition cannot be read) |
| S-P2 | a CI lane red on the flip commit and green on the pre-flip head | the default does not flip on that platform; settler |

No STOP condition is waived after data. The verdict A5 is never re-cut: families, the 10 % bar and
the counter are fixed here.

## 6. What is measured where

All rows: Linux x86-64, AMD Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2, Release,
`HVEN_FP_MODE=SAFER_FAST`. Apple/Accelerate and Windows: correctness by their CI lanes only; every
instruction count, timing, rho and saving UNOBSERVED. Wall-clock informational only.
