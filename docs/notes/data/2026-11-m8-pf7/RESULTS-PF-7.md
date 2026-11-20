# RESULTS-PF-7

Declaration `DECLARATION-PF-7.md` (+ `-r2`: L1 re-taken, instrument defect). Code commit `dc0aacc30`; arm A = m8 `331b31837`. All legs under `box-lane`, MKL_NUM_THREADS=1; no wall-clock is claimed.

## Attribution (declaration section 1, repeated here)
- On tiny cells almost all instructions are not hven's: harness loader ~3.1-4.3M, OpenMP/MKL bring-up ~2.07M (PF-6), then the solve. Net solve instructions: HS ipm 0.57-0.65M, HS sqp 1.1-2.3M, small-dense sqp ~5M (BT11 4.9M).
- Only one fixable per-solve setup cost: `get_core_count()` in `IpmSolverImpl`'s constructor, 0.285M (45 % of a tiny ipm solve's net). Fixed (buffered scan of `/proc/cpuinfo`, probe once per process).
- The dense arm "LAPACKE wrapper overhead" is not wrapper overhead: per `dsytrs` call ~1 800 instructions, of which the LAPACKE/Fortran-entry layers are ~140, the thread-scope pair 141, the nan-check pair 316 (behaviour, kept) and ~1 400 are MKL's own `mkl_lapack_dsytrs` -> gemv/ger/swap/scal on 3x3-class matrices. Column-major `_work` calls copy and transpose nothing. No behaviour-neutral change exists there; not made.
- hven's own frames (options validation, workspace construction, SolveState/MajorState, trace/ledger plumbing, adapters) are each < 0.1 % of a cell; allocation churn is ~0.1M of solver-side mallocs in `SchurComplement::solve` and Eigen resizes (pf4's files, untouched).

## Gate table
| leg | result |
|---|---|
| L1 ctest Release (arm B) | 100 % passed, 0 failed out of 3957 (3959 listed; 2 skipped by design/disabled), the 3 new `CoreCount.*` tests ran; `gate/ctest-rel.log`. First take failed on `BenchStampGuard.SelfTest` (instrument defect, r2) |
| L1 ctest Debug (arm B) | 100 % passed, 0 failed out of 3957; `gate/ctest-dbg.log` |
| L2 replay x3, A vs B | walk, ssn, ipm: 27 cells x 75 columns, differences 0 each; `gate/replay-compare.txt` |
| L3 TRAIN 238 serial A vs B | 238/238 rows, every non-wall key identical (status, iterations, factorizations, objective, every counter), 0 DNF; `gate/train-A.csv`, `train-B.csv`, `train-icmp.md` |
| L4 instruction table | below; bound "no cell above 1.005": one cell exceeded it in the serial leg (hven-sqp hs005, 1.0083); re-run alone 7 times each, alternating: A 6 814 707-6 819 460, B 6 811 316-6 817 332 (B/A 0.9995-1.0000; `gate/outlier-hs005-sqp.txt`): not reproduced, run-to-run address noise of an 6.8M-instruction cell |
| L5 mutations | control passes (3 tests run); K1, K2, K3, K4 each killed (`gate/mutation.out`); tree restored and rebuilt |
| L6 install smoke | smoke and export contract rc 0 on the default and clang++ front ends (`gate/install-smoke/`) |
| L7 instrumentation cost | below; identity 0 violations |
| CI | see the hand-back |

### B/A instructions:u (serial, one pinned CPU, TRAIN roster)
| set | lane | cells | median A | median B | sum B/A | median B/A | max B/A |
|---|---|---:|---:|---:|---:|---:|---:|
| HS TRAIN | hven-ipm | 22 | 6,708,217 | 6,512,090 | 0.97083 | 0.97063 | 0.97299 |
| HS TRAIN | hven-sqp | 22 | 8,418,478 | 8,407,742 | 1.00015 | 0.99963 | 1.00827 (noise, above) |
| small-dense TRAIN | hven-ipm | 97 | 7,942,850 | 7,744,611 | 0.99535 | 0.97552 | 0.99988 |
| small-dense TRAIN | hven-sqp | 97 | 11,942,271 | 11,935,109 | 1.00013 | 0.99976 | 1.00050 |
The sqp lane does not call `get_core_count`; its +-0.02 % is address noise. The ipm lane saves ~0.2M instructions per solver construction (3 % of a tiny cell, 45 % of its net solve); nlp-35 cells are unchanged (the saving is a one-off).

### Instrumentation cost (L7; variant NI = every trace/ledger emission guard constant-folded, `tools/ni_patch.py`; instructions:u, co-run width 4)
| set | lane | cells | sum NI/A | median NI/A | min | max |
|---|---|---:|---:|---:|---:|---:|
| HS TRAIN | ipm | 22 | 0.99929 | 0.99939 | 0.99860 | 1.00039 |
| HS TRAIN | sqp | 22 | 0.99792 | 0.99830 | 0.99502 | 0.99995 |
| nlp-35 (full cells) | ipm | 19 | 1.00000 | 1.00000 | 0.99989 | 1.00001 |
| nlp-35 (full cells) | sqp | 15 | 0.99999 | 0.99999 | 0.99934 | 1.00019 |
| small-dense TRAIN | ipm | 97 | 0.99993 | 0.99957 | 0.99741 | 1.00189 |
| small-dense TRAIN | sqp | 97 | 0.99995 | 0.99815 | 0.99116 | 1.00033 |
Reading: the harness attaches no sink or ledger on the ipm lane, so NI/A there is the cost of the OFF guards: **0.07 % of a tiny HS ipm cell (~4.8k instructions), 0.007 % on small-dense, 0.000 % on nlp-35** -- zero at any size that matters, and the sub-0.1 % part on HS cells is the guard compares plus code layout (the same cells move +-0.04 % between identical binaries). On the sqp lane the harness always attaches a Ledger, so NI/A = guards + ledger-ON cost: 0.2 % of a tiny HS cell (~17k instructions), 0.005 % on small-dense and 0.001 % on nlp-35. Status, iterations, factorizations, objective and every non-ledger counter equal on all 272 cells (HS 44, nlp-35 34, small-dense 194) (the ledger-derived `side.harness_*` columns are excluded by construction). Trace-sink-attached cost was not measured (the harness's `--trace` writes a file; the cost contract is "no sink, no event built").

## Open items
- OpenMP/MKL bring-up (2.07M, 25-30 % of a tiny cell) and the harness loader (3-4M) are the remaining fixed cost; PF-6 owns the first, the second is the 116 MB static harness binary.
- A dense `dsytrs` of n <= ~6 costs ~1.8k instructions, ~1.4k inside MKL; only a hand-written small-n kernel would remove it, and it would not be bit-identical to MKL's gemv/ger order. Not proposed without a numerics decision.
- `EIGEN_INITIALIZE_MATRICES_BY_ZERO` memset is 1.3-1.9 % of sqp tiny cells; a project-wide define, not touched.
- Schur/KKT allocation churn (pf4's files) left to pf4.
