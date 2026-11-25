## Escape counters over every measured cell (all rows of this task at the pin)

hven-sqp full cells examined: 134 (HS 22, nlp35 15, small-dense 97); truncated hven-sqp cells (counters absent, trunc30 mode): 8; hven-ipm cells carry no SQP escape counters.

| counter | sum over cells | cells with value > 0 |
|---|---:|---|
| side.ssn.ssn_escapes | 0 | - |
| side.ipqp.ipqp_escapes | 0 | - |
| side.elastic_from_ipqp_escape | 0 | - |

## Fixed overhead on small cells (per-cell instructions; full cells)

| set / lane | cells | median total | median loader | median library_init | median problem_load | median net solver (total - proc.* - library_init) | net as share of total |
|---|---:|---:|---:|---:|---:|---:|---:|
| hs / hven-ipm | 22 | 6.76e+06 | 4e+06 | 2e+06 | 0 | 7.87e+05 | 11.6% |
| hs / hven-sqp | 22 | 8.49e+06 | 4e+06 | 1.5e+06 | 0 | 1.54e+06 | 19.4% |
| sd / hven-ipm | 97 | 7.99e+06 | 4e+06 | 2e+06 | 5e+05 | 1.49e+06 | 18.5% |
| sd / hven-sqp | 97 | 1.2e+07 | 4e+06 | 1.5e+06 | 5e+05 | 5.46e+06 | 45.0% |
| nlp35 / hven-ipm | 19 | 8.06e+08 | 4e+06 | 8e+06 | 1.45e+07 | 7.66e+08 | 95.3% |
| nlp35 / hven-sqp | 15 | 1.95e+11 | 3.39e+07 | 1.63e+08 | 8.85e+06 | 1.95e+11 | 99.6% |

## Named factorization-pathology cells (small-dense TRAIN)

- **hven-ipm__SPANHYD**: status optimal, iterations 110, factorizations 189, instructions 4.031e+08; top components unattributed.truncated_unwind 49.0%, la.dense_factorize 36.3%, la.matching_scaling 4.2%, eval.callback 4.1%, la.dense_solve 1.6%; counters {'symbolic_analyses': '2', 'dense_factorizations': '189', 'kkt_factor_counters.analyze_count': '1', 'kkt_factor_counters.factorize_count': '189', 'kkt_factor_counters.dense_factorize_count': '189', 'kkt_factor_counters.dense_flagged_count': '11'}
- **hven-sqp__SPANHYD**: status numerical_error, iterations 32, factorizations 36497, instructions 4.35e+11; top components la.sparse_solve 82.9%, unattributed.truncated_unwind 7.0%, qp.schur_border 3.3%, la.sparse_factorize 1.6%, la.dense_factorize 1.4%; counters {'major_iters': '32', 'qp_minor_iters': '18208', 'border_refine_steps': '1611', 'symbolic_analyses': '32', 'inertia_wrong_verdicts': '2932', 'dense_factorizations': '19312'}
- **hven-ipm__CRESC100**: status optimal, iterations 142, factorizations 245, instructions 3.859e+08; top components la.sparse_factorize 43.8%, eval.callback 33.9%, la.sparse_solve 7.6%, proc.problem_load 3.2%, eval.assembly 2.7%; counters {'symbolic_analyses': '1', 'kkt_factor_counters.analyze_count': '1', 'kkt_factor_counters.factorize_count': '245'}
- **hven-sqp__CRESC100**: status optimal, iterations 40, factorizations 26023, instructions 3.78e+11; top components la.sparse_solve 74.1%, qp.schur_border 7.5%, la.sparse_analyze 5.4%, unattributed.truncated_unwind 2.9%, la.sparse_factorize 2.1%; counters {'major_iters': '40', 'qp_minor_iters': '23791', 'border_refine_steps': '19096', 'symbolic_analyses': '5704', 'inertia_wrong_verdicts': '1703', 'dense_factorizations': '6600'}
- **hven-ipm__MSS1**: status numerical_error, iterations 2, factorizations 19, instructions 8.648e+07; top components unattributed.truncated_unwind 53.2%, la.dense_factorize 23.1%, proc.problem_load 7.5%, proc.loader_startup 4.6%, la.matching_scaling 4.6%; counters {'symbolic_analyses': '1', 'dense_factorizations': '19', 'kkt_factor_counters.analyze_count': '1', 'kkt_factor_counters.factorize_count': '19', 'kkt_factor_counters.dense_factorize_count': '19', 'kkt_factor_counters.dense_flagged_count': '2'}
- **hven-sqp__MSS1**: status numerical_error, iterations 11, factorizations 10119, instructions 9.935e+10; top components la.sparse_solve 59.7%, la.sparse_analyze 19.8%, unattributed.truncated_unwind 6.0%, la.sparse_factorize 4.6%, qp.refine_polish 1.7%; counters {'major_iters': '11', 'qp_minor_iters': '9525', 'border_refine_steps': '7085', 'symbolic_analyses': '12', 'inertia_wrong_verdicts': '3400', 'dense_factorizations': '48'}

## SQP per-minor and per-factorization cost, cells with >= 100 QP minors (full cells)

| cell | instructions | minors | factorizations | instr/minor | instr/factorization | la.sparse_solve | la.sparse_analyze | la.sparse_factorize | qp.schur_border | qp.* total |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| hven-sqp__SPANHYD | 4.35e+11 | 18208 | 36497 | 2.39e+07 | 1.19e+07 | 82.9% | 0.1% | 1.6% | 3.3% | 4.1% |
| hven-sqp__CRESC100 | 3.78e+11 | 23791 | 26023 | 1.59e+07 | 1.45e+07 | 74.1% | 5.4% | 2.1% | 7.5% | 11.2% |
| hven-sqp__STEENBRC | 3.42e+11 | 27973 | 35787 | 1.22e+07 | 9.55e+06 | 57.0% | 17.0% | 7.9% | 2.9% | 9.2% |
| hven-sqp__STEENBRG | 3.24e+11 | 23432 | 31079 | 1.38e+07 | 1.04e+07 | 59.7% | 15.4% | 7.6% | 2.6% | 8.4% |
| hven-sqp__STEENBRE | 2.56e+11 | 14948 | 23434 | 1.71e+07 | 1.09e+07 | 60.3% | 15.2% | 7.4% | 3.4% | 7.8% |
| hven-sqp__STEENBRD | 2.44e+11 | 21336 | 30397 | 1.14e+07 | 8.03e+06 | 58.9% | 16.8% | 7.7% | 2.3% | 7.7% |
| hven-sqp__ACOPP118 | 2.06e+11 | 10361 | 11372 | 1.99e+07 | 1.81e+07 | 62.7% | 18.3% | 6.2% | 1.3% | 4.8% |
| hven-sqp__YORKNET | 2.03e+11 | 13391 | 14825 | 1.52e+07 | 1.37e+07 | 59.5% | 19.2% | 7.2% | 3.7% | 8.8% |
| hven-sqp__BIGBANK | 1.96e+11 | 3196 | 3219 | 6.13e+07 | 6.08e+07 | 55.2% | 24.0% | 7.6% | 6.7% | 9.6% |
| hven-sqp__STEENBRB | 1.95e+11 | 19099 | 25305 | 1.02e+07 | 7.72e+06 | 57.4% | 17.9% | 7.9% | 2.2% | 8.0% |
| hven-sqp__MSS1 | 9.94e+10 | 9525 | 10119 | 1.04e+07 | 9.82e+06 | 59.7% | 19.8% | 4.6% | 1.7% | 5.9% |
| hven-sqp__CRESC50 | 9.14e+10 | 13673 | 14180 | 6.68e+06 | 6.45e+06 | 68.4% | 3.6% | 1.3% | 9.1% | 12.9% |
| hven-sqp__LAKES | 6.49e+10 | 6388 | 5380 | 1.02e+07 | 1.21e+07 | 77.2% | 7.7% | 2.7% | 2.5% | 6.1% |
| hven-sqp__MESH | 3.37e+10 | 4011 | 7544 | 8.39e+06 | 4.46e+06 | 65.3% | 0.4% | 0.8% | 5.3% | 8.4% |
| hven-sqp__ACOPR57 | 1.86e+10 | 2296 | 1618 | 8.1e+06 | 1.15e+07 | 67.1% | 13.6% | 5.9% | 0.5% | 4.6% |
| hven-sqp__HS99EXP | 1.36e+10 | 8722 | 11111 | 1.56e+06 | 1.22e+06 | 0.0% | 0.0% | 0.0% | 5.6% | 13.2% |
| hven-sqp__DALLASM | 1.18e+10 | 759 | 413 | 1.55e+07 | 2.86e+07 | 85.3% | 2.5% | 1.8% | 1.8% | 6.5% |
| hven-sqp__SAWPATH | 8.99e+09 | 334 | 61 | 2.69e+07 | 1.47e+08 | 86.2% | 3.1% | 1.7% | 0.7% | 5.7% |
| hven-sqp__READING6 | 8.74e+09 | 520 | 567 | 1.68e+07 | 1.54e+07 | 75.7% | 5.2% | 1.8% | 2.3% | 4.3% |
| hven-sqp__NET1 | 7.85e+09 | 787 | 298 | 9.97e+06 | 2.63e+07 | 71.0% | 0.6% | 0.5% | 5.9% | 11.4% |
| hven-sqp__DECONVC | 4.91e+09 | 3218 | 1932 | 1.53e+06 | 2.54e+06 | 0.0% | 0.0% | 0.0% | 6.1% | 20.4% |
| hven-sqp__ZAMB2-8 | 3.37e+09 | 551 | 147 | 6.11e+06 | 2.29e+07 | 67.4% | 0.4% | 0.3% | 5.9% | 13.7% |
| hven-sqp__ACOPP30 | 2.52e+09 | 615 | 488 | 4.1e+06 | 5.16e+06 | 63.6% | 16.1% | 5.8% | 0.3% | 4.2% |
| hven-sqp__WATER | 2.24e+09 | 1526 | 243 | 1.47e+06 | 9.2e+06 | 0.0% | 0.0% | 0.0% | 10.6% | 24.5% |
| hven-sqp__OPTPRLOC | 1.74e+09 | 1553 | 842 | 1.12e+06 | 2.07e+06 | 0.0% | 0.0% | 0.0% | 7.3% | 21.2% |
| hven-sqp__DNIEPER | 1.35e+09 | 333 | 581 | 4.07e+06 | 2.33e+06 | 59.7% | 0.2% | 1.0% | 4.1% | 7.5% |
| hven-sqp__SMBANK | 1.27e+09 | 357 | 29 | 3.54e+06 | 4.36e+07 | 86.2% | 0.2% | 0.6% | 1.5% | 6.9% |
| hven-sqp__DEMBO7 | 1.24e+09 | 1348 | 894 | 9.21e+05 | 1.39e+06 | 0.0% | 0.0% | 0.0% | 9.8% | 23.8% |
| hven-sqp__BT7 | 2.48e+08 | 1905 | 1428 | 1.3e+05 | 1.74e+05 | 0.0% | 0.2% | 0.0% | 5.4% | 39.8% |
| hven-sqp__CRESC4 | 2.37e+08 | 1240 | 570 | 1.91e+05 | 4.15e+05 | 0.0% | 0.0% | 0.0% | 8.9% | 35.1% |
| hven-sqp__HS92 | 2.2e+08 | 789 | 130 | 2.79e+05 | 1.69e+06 | 0.0% | 0.0% | 0.0% | 4.3% | 17.0% |
| hven-sqp__HS101 | 1.74e+08 | 692 | 417 | 2.52e+05 | 4.17e+05 | 0.0% | 0.0% | 0.0% | 8.0% | 36.8% |
| hven-sqp__HS91 | 1.73e+08 | 308 | 88 | 5.62e+05 | 1.97e+06 | 0.0% | 0.0% | 0.0% | 1.4% | 11.3% |
| hven-sqp__HS75 | 1.59e+08 | 976 | 157 | 1.62e+05 | 1.01e+06 | 0.0% | 0.0% | 0.0% | 6.3% | 38.8% |
| hven-sqp__HS74 | 1.58e+08 | 981 | 165 | 1.61e+05 | 9.56e+05 | 0.0% | 0.0% | 0.0% | 7.3% | 37.7% |
| hven-sqp__HS56 | 1.26e+08 | 551 | 401 | 2.28e+05 | 3.13e+05 | 0.0% | 0.0% | 0.0% | 8.4% | 31.5% |
| hven-sqp__HS90 | 1.23e+08 | 255 | 89 | 4.84e+05 | 1.39e+06 | 0.0% | 0.0% | 0.0% | 2.4% | 12.2% |
| hven-sqp__HS111LNP | 8.03e+07 | 253 | 65 | 3.17e+05 | 1.23e+06 | 0.0% | 0.6% | 0.0% | 6.9% | 26.8% |
| hven-sqp__BURKEHAN | 7.2e+07 | 908 | 154 | 7.93e+04 | 4.68e+05 | 0.0% | 0.0% | 0.0% | 6.2% | 59.0% |
| hven-sqp__HS102 | 6.93e+07 | 275 | 51 | 2.52e+05 | 1.36e+06 | 0.0% | 0.0% | 0.0% | 9.4% | 33.9% |
| hven-sqp__HS103 | 5.21e+07 | 201 | 39 | 2.59e+05 | 1.34e+06 | 0.0% | 0.0% | 0.0% | 5.8% | 31.6% |
| hven-sqp__HS88 | 4.64e+07 | 262 | 43 | 1.77e+05 | 1.08e+06 | 0.0% | 0.0% | 0.0% | 7.5% | 29.1% |
| hven-sqp__HS108 | 3.87e+07 | 147 | 42 | 2.64e+05 | 9.23e+05 | 0.0% | 0.0% | 0.0% | 7.7% | 31.0% |
| hven-sqp__HS81 | 3.7e+07 | 171 | 44 | 2.16e+05 | 8.41e+05 | 0.0% | 0.0% | 0.0% | 5.4% | 29.7% |
| hven-sqp__HS83 | 2.62e+07 | 201 | 186 | 1.31e+05 | 1.41e+05 | 0.0% | 0.0% | 0.0% | 3.8% | 36.2% |
| hven-sqp__HS64 | 2.35e+07 | 179 | 23 | 1.31e+05 | 1.02e+06 | 0.0% | 0.0% | 0.0% | 4.3% | 40.4% |
| hven-sqp__BT12 | 2.22e+07 | 104 | 21 | 2.14e+05 | 1.06e+06 | 0.0% | 2.2% | 0.0% | 9.0% | 33.7% |
| hven-sqp__hs010 | 2.12e+07 | 184 | 22 | 1.15e+05 | 9.65e+05 | 0.0% | 0.0% | 0.0% | 7.1% | 40.0% |
| hven-sqp__HS59 | 1.82e+07 | 117 | 36 | 1.56e+05 | 5.06e+05 | 0.0% | 0.0% | 0.0% | 5.5% | 41.2% |

## Leaf symbols inside la.sparse_solve (hven-sqp full cells; share of the component, from each cell's top-60 leaves)

| leaf | share of la.sparse_solve |
|---|---:|
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | 29.0% |
| `mkl_spblas_lp64_def_dcsr1nsunf__mvout_par` | 18.4% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | 11.4% |
| `find_norm` | 4.3% |
| `mkl_pds_lp64_do_all_pardiso_fc` | 4.3% |
| `mkl_pds_lp64_dsytrs_bklbw_pardiso` | 4.0% |
| `mkl_pds_lp64_dsytrs_bklfw_noscal_pardiso` | 3.7% |
| `mkl_blas_lp64_dgemv` | 1.9% |
| `mkl_blas_dscal` | 1.8% |
| `L_not_one_loopgas_1` | 1.8% |
| `mkl_blas_def_xdgemv` | 1.7% |
| `mkl_blas_lp64_dscal` | 1.4% |
