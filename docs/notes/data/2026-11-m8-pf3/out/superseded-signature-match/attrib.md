# PF-3 attribution tables (instructions)
cells read: 284 (272 full, 12 truncated); failed: 0

### SUITE (all full cells) -- all-in
cells: 272; total (instructions): 3.67428e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 61.50% | 0.00% | 2.26e+12 |
| la.sparse_factorize | 12.94% | 0.00% | 4.755e+11 |
| la.sparse_analyze | 10.88% | 0.00% | 3.997e+11 |
| qp.schur_border | 3.16% | 0.00% | 1.161e+11 |
| eval.callback | 2.46% | 0.00% | 9.056e+10 |
| kkt.assembly | 1.46% | 0.00% | 5.35e+10 |
| qp.eqp_dependency | 1.19% | 0.00% | 4.371e+10 |
| qp.walk | 1.17% | 0.00% | 4.306e+10 |
| la.dense_solve | 0.98% | 0.00% | 3.612e+10 |
| la.other | 0.74% | 0.00% | 2.717e+10 |
| la.dense_factorize | 0.70% | 0.00% | 2.57e+10 |
| kkt.factor_glue | 0.64% | 0.00% | 2.364e+10 |
| qp.refine_polish | 0.43% | 0.00% | 1.597e+10 |
| outer.ipm_nlp_core | 0.42% | 0.00% | 1.546e+10 |
| setup.library_init | 0.33% | 12.50% | 1.23e+10 |
| eval.assembly | 0.29% | 0.00% | 1.05e+10 |
| instr.ledger_trace | 0.23% | 0.00% | 8.48e+09 |
| outer.acceptance_linesearch | 0.13% | 0.00% | 4.619e+09 |
| unattributed.unresolved | 0.09% | 0.00% | 3.35e+09 |
| proc.loader_startup | 0.08% | 42.11% | 2.99e+09 |
| la.matching_scaling | 0.08% | 0.00% | 2.769e+09 |
| proc.problem_load | 0.07% | 5.26% | 2.479e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 87.82% | 11.11% |
| qp | 5.96% | 0.00% |
| eval | 2.75% | 0.63% |
| kkt | 2.10% | 0.00% |
| outer | 0.56% | 0.00% |
| setup | 0.34% | 12.50% |
| instr | 0.23% | 0.00% |
| proc | 0.15% | 55.28% |
| unattributed | 0.09% | 0.00% |

### SUITE (all full cells) -- solver only (proc.* excluded)
cells: 272; total (instructions): 3.6687e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 61.60% | 0.00% | 2.26e+12 |
| la.sparse_factorize | 12.96% | 0.00% | 4.755e+11 |
| la.sparse_analyze | 10.90% | 0.00% | 3.997e+11 |
| qp.schur_border | 3.17% | 0.00% | 1.161e+11 |
| eval.callback | 2.47% | 0.00% | 9.056e+10 |
| kkt.assembly | 1.46% | 0.00% | 5.35e+10 |
| qp.eqp_dependency | 1.19% | 0.00% | 4.371e+10 |
| qp.walk | 1.17% | 0.00% | 4.306e+10 |
| la.dense_solve | 0.98% | 0.00% | 3.612e+10 |
| la.other | 0.74% | 0.00% | 2.717e+10 |
| la.dense_factorize | 0.70% | 0.00% | 2.57e+10 |
| kkt.factor_glue | 0.64% | 0.00% | 2.364e+10 |
| qp.refine_polish | 0.44% | 0.00% | 1.597e+10 |
| outer.ipm_nlp_core | 0.42% | 0.00% | 1.546e+10 |
| setup.library_init | 0.34% | 28.57% | 1.23e+10 |
| eval.assembly | 0.29% | 0.00% | 1.05e+10 |
| instr.ledger_trace | 0.23% | 0.00% | 8.48e+09 |
| outer.acceptance_linesearch | 0.13% | 0.00% | 4.619e+09 |
| unattributed.unresolved | 0.09% | 0.00% | 3.35e+09 |
| la.matching_scaling | 0.08% | 0.00% | 2.769e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 87.96% | 25.00% |
| qp | 5.97% | 0.00% |
| eval | 2.75% | 0.64% |
| kkt | 2.10% | 0.00% |
| outer | 0.56% | 0.00% |
| setup | 0.34% | 28.57% |
| instr | 0.23% | 0.00% |
| unattributed | 0.09% | 0.00% |

### ipm / all -- all-in
cells: 138; total (instructions): 5.12295e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 57.89% | 0.00% | 2.966e+11 |
| la.sparse_solve | 19.71% | 0.00% | 1.01e+11 |
| eval.callback | 17.44% | 5.06% | 8.934e+10 |
| eval.assembly | 1.98% | 0.00% | 1.014e+10 |
| outer.acceptance_linesearch | 0.90% | 0.00% | 4.618e+09 |
| outer.ipm_nlp_core | 0.71% | 0.00% | 3.651e+09 |
| proc.problem_load | 0.40% | 6.67% | 2.068e+09 |
| setup.library_init | 0.24% | 22.65% | 1.254e+09 |
| la.dense_factorize | 0.23% | 0.00% | 1.201e+09 |
| la.sparse_analyze | 0.22% | 0.00% | 1.103e+09 |
| proc.loader_startup | 0.12% | 53.33% | 6.078e+08 |
| unattributed.unresolved | 0.06% | 0.00% | 3.099e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.09% | 0.00% |
| eval | 19.42% | 6.67% |
| outer | 1.61% | 0.78% |
| proc | 0.53% | 61.54% |
| setup | 0.25% | 23.08% |
| unattributed | 0.06% | 0.00% |
| kkt | 0.04% | 0.00% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### ipm / all -- solver only (proc.* excluded)
cells: 138; total (instructions): 5.09592e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 58.20% | 0.00% | 2.966e+11 |
| la.sparse_solve | 19.82% | 0.00% | 1.01e+11 |
| eval.callback | 17.53% | 5.57% | 8.934e+10 |
| eval.assembly | 1.99% | 0.00% | 1.014e+10 |
| outer.acceptance_linesearch | 0.91% | 0.00% | 4.618e+09 |
| outer.ipm_nlp_core | 0.72% | 0.00% | 3.651e+09 |
| setup.library_init | 0.25% | 61.25% | 1.254e+09 |
| la.dense_factorize | 0.24% | 0.00% | 1.201e+09 |
| la.sparse_analyze | 0.22% | 0.00% | 1.103e+09 |
| unattributed.unresolved | 0.06% | 0.00% | 3.099e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.50% | 0.00% |
| eval | 19.52% | 14.29% |
| outer | 1.62% | 0.78% |
| setup | 0.26% | 66.67% |
| unattributed | 0.06% | 0.00% |
| kkt | 0.04% | 0.00% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### sqp / all -- all-in
cells: 134; total (instructions): 3.16198e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 68.28% | 0.00% | 2.159e+12 |
| la.sparse_analyze | 12.61% | 0.00% | 3.986e+11 |
| la.sparse_factorize | 5.66% | 0.00% | 1.79e+11 |
| qp.schur_border | 3.67% | 3.38% | 1.161e+11 |
| kkt.assembly | 1.69% | 0.00% | 5.331e+10 |
| qp.eqp_dependency | 1.38% | 0.48% | 4.371e+10 |
| qp.walk | 1.36% | 1.65% | 4.306e+10 |
| la.dense_solve | 1.14% | 7.18% | 3.605e+10 |
| la.other | 0.86% | 0.00% | 2.71e+10 |
| la.dense_factorize | 0.77% | 0.00% | 2.45e+10 |
| kkt.factor_glue | 0.75% | 0.00% | 2.364e+10 |
| qp.refine_polish | 0.50% | 0.58% | 1.597e+10 |
| outer.ipm_nlp_core | 0.37% | 0.00% | 1.181e+10 |
| setup.library_init | 0.35% | 9.09% | 1.105e+10 |
| instr.ledger_trace | 0.27% | 0.16% | 8.48e+09 |
| unattributed.unresolved | 0.10% | 0.00% | 3.041e+09 |
| la.matching_scaling | 0.09% | 0.00% | 2.729e+09 |
| proc.loader_startup | 0.08% | 36.36% | 2.382e+09 |
| proc.problem_load | 0.01% | 1.92% | 4.114e+08 |
| proc.main_other | 0.00% | 1.89% | 5.152e+07 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.40% | 17.52% |
| qp | 6.92% | 9.09% |
| kkt | 2.43% | 0.49% |
| outer | 0.39% | 0.00% |
| setup | 0.35% | 9.52% |
| instr | 0.27% | 0.16% |
| unattributed | 0.10% | 0.00% |
| proc | 0.09% | 47.49% |
| eval | 0.05% | 0.04% |

### sqp / all -- solver only (proc.* excluded)
cells: 134; total (instructions): 3.15911e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 68.34% | 0.00% | 2.159e+12 |
| la.sparse_analyze | 12.62% | 0.00% | 3.986e+11 |
| la.sparse_factorize | 5.66% | 0.00% | 1.79e+11 |
| qp.schur_border | 3.68% | 3.92% | 1.161e+11 |
| kkt.assembly | 1.69% | 0.00% | 5.331e+10 |
| qp.eqp_dependency | 1.38% | 0.54% | 4.371e+10 |
| qp.walk | 1.36% | 1.78% | 4.306e+10 |
| la.dense_solve | 1.14% | 16.67% | 3.605e+10 |
| la.other | 0.86% | 0.00% | 2.71e+10 |
| la.dense_factorize | 0.78% | 0.00% | 2.45e+10 |
| kkt.factor_glue | 0.75% | 0.00% | 2.364e+10 |
| qp.refine_polish | 0.51% | 0.62% | 1.597e+10 |
| outer.ipm_nlp_core | 0.37% | 0.00% | 1.181e+10 |
| setup.library_init | 0.35% | 18.18% | 1.105e+10 |
| instr.ledger_trace | 0.27% | 0.18% | 8.48e+09 |
| unattributed.unresolved | 0.10% | 0.00% | 3.041e+09 |
| la.matching_scaling | 0.09% | 0.00% | 2.729e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.48% | 33.33% |
| qp | 6.93% | 17.06% |
| kkt | 2.44% | 0.49% |
| outer | 0.39% | 0.00% |
| setup | 0.35% | 19.09% |
| instr | 0.27% | 0.16% |
| unattributed | 0.10% | 0.00% |
| eval | 0.05% | 0.04% |

### ipm / hs -- all-in
cells: 22; total (instructions): 1.44e+08

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| proc.loader_startup | 63.89% | 61.54% | 9.2e+07 |
| setup.library_init | 25.35% | 26.92% | 3.65e+07 |
| unattributed.unresolved | 2.08% | 0.00% | 3e+06 |
| eval.callback | 1.74% | 0.00% | 2.5e+06 |
| proc.main_other | 1.74% | 0.00% | 2.5e+06 |
| proc.harness_report | 1.39% | 0.00% | 2e+06 |
| outer.acceptance_linesearch | 1.04% | 0.00% | 1.5e+06 |
| eval.assembly | 0.69% | 0.00% | 1e+06 |
| outer.ipm_nlp_core | 0.69% | 0.00% | 1e+06 |
| proc.problem_load | 0.69% | 0.00% | 1e+06 |
| kkt.factor_glue | 0.35% | 0.00% | 5e+05 |
| setup.init_teardown | 0.35% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| proc | 67.71% | 69.23% |
| setup | 25.69% | 29.67% |
| eval | 2.43% | 0.00% |
| unattributed | 2.08% | 0.00% |
| outer | 1.74% | 0.00% |
| kkt | 0.35% | 0.00% |

### ipm / hs -- solver only (proc.* excluded)
cells: 22; total (instructions): 4.65e+07

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| setup.library_init | 78.49% | 80.00% | 3.65e+07 |
| unattributed.unresolved | 6.45% | 0.00% | 3e+06 |
| eval.callback | 5.38% | 0.00% | 2.5e+06 |
| outer.acceptance_linesearch | 3.23% | 0.00% | 1.5e+06 |
| eval.assembly | 2.15% | 0.00% | 1e+06 |
| outer.ipm_nlp_core | 2.15% | 0.00% | 1e+06 |
| kkt.factor_glue | 1.08% | 0.00% | 5e+05 |
| setup.init_teardown | 1.08% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| setup | 79.57% | 80.00% |
| eval | 7.53% | 0.00% |
| unattributed | 6.45% | 0.00% |
| outer | 5.38% | 0.00% |
| kkt | 1.08% | 0.00% |

### ipm / cutest -- all-in
cells: 19; total (instructions): 5.08034e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 58.23% | 51.06% | 2.958e+11 |
| la.sparse_solve | 19.84% | 8.55% | 1.008e+11 |
| eval.callback | 17.38% | 15.20% | 8.829e+10 |
| eval.assembly | 1.98% | 2.66% | 1.005e+10 |
| outer.acceptance_linesearch | 0.89% | 1.09% | 4.543e+09 |
| outer.ipm_nlp_core | 0.71% | 0.55% | 3.608e+09 |
| proc.problem_load | 0.38% | 2.46% | 1.937e+09 |
| la.sparse_analyze | 0.21% | 0.64% | 1.088e+09 |
| setup.library_init | 0.20% | 1.47% | 1.039e+09 |
| unattributed.unresolved | 0.05% | 0.07% | 2.689e+08 |
| proc.loader_startup | 0.02% | 0.50% | 1.163e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.33% | 73.22% |
| eval | 19.36% | 16.67% |
| outer | 1.60% | 1.92% |
| proc | 0.41% | 3.16% |
| setup | 0.21% | 1.53% |
| unattributed | 0.05% | 0.07% |
| kkt | 0.04% | 0.05% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### ipm / cutest -- solver only (proc.* excluded)
cells: 19; total (instructions): 5.05975e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 58.47% | 54.46% | 2.958e+11 |
| la.sparse_solve | 19.92% | 9.05% | 1.008e+11 |
| eval.callback | 17.45% | 16.40% | 8.829e+10 |
| eval.assembly | 1.99% | 2.68% | 1.005e+10 |
| outer.acceptance_linesearch | 0.90% | 1.10% | 4.543e+09 |
| outer.ipm_nlp_core | 0.71% | 0.56% | 3.608e+09 |
| la.sparse_analyze | 0.21% | 0.66% | 1.088e+09 |
| setup.library_init | 0.21% | 1.47% | 1.039e+09 |
| unattributed.unresolved | 0.05% | 0.08% | 2.689e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.65% | 74.84% |
| eval | 19.43% | 18.70% |
| outer | 1.61% | 2.10% |
| setup | 0.21% | 1.54% |
| unattributed | 0.05% | 0.08% |
| kkt | 0.04% | 0.05% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### ipm / sd -- all-in
cells: 97; total (instructions): 4.1165e+09

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| eval.callback | 25.43% | 4.86% | 1.047e+09 |
| la.dense_factorize | 25.40% | 0.00% | 1.045e+09 |
| la.sparse_factorize | 18.10% | 0.00% | 7.45e+08 |
| proc.loader_startup | 9.70% | 53.33% | 3.995e+08 |
| la.sparse_solve | 4.66% | 0.00% | 1.92e+08 |
| setup.library_init | 4.33% | 25.00% | 1.782e+08 |
| proc.problem_load | 3.15% | 7.14% | 1.295e+08 |
| eval.assembly | 2.22% | 0.00% | 9.15e+07 |
| outer.acceptance_linesearch | 1.80% | 0.00% | 7.4e+07 |
| la.dense_solve | 1.25% | 0.00% | 5.127e+07 |
| outer.ipm_nlp_core | 1.01% | 0.00% | 4.15e+07 |
| la.matching_scaling | 0.97% | 0.00% | 4e+07 |
| unattributed.unresolved | 0.92% | 0.00% | 3.8e+07 |
| la.sparse_analyze | 0.36% | 0.00% | 1.5e+07 |
| proc.main_other | 0.21% | 0.00% | 8.5e+06 |
| proc.harness_report | 0.16% | 0.00% | 6.5e+06 |
| la.other | 0.11% | 0.00% | 4.5e+06 |
| kkt.assembly | 0.10% | 0.00% | 4e+06 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 50.85% | 0.00% |
| eval | 27.66% | 6.67% |
| proc | 13.26% | 60.00% |
| setup | 4.34% | 25.00% |
| outer | 2.81% | 0.00% |
| unattributed | 0.96% | 0.00% |
| kkt | 0.11% | 0.00% |
| instr | 0.01% | 0.00% |

### ipm / sd -- solver only (proc.* excluded)
cells: 97; total (instructions): 3.5705e+09

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| eval.callback | 29.32% | 6.67% | 1.047e+09 |
| la.dense_factorize | 29.28% | 0.00% | 1.045e+09 |
| la.sparse_factorize | 20.87% | 0.00% | 7.45e+08 |
| la.sparse_solve | 5.38% | 0.00% | 1.92e+08 |
| setup.library_init | 4.99% | 66.67% | 1.782e+08 |
| eval.assembly | 2.56% | 0.00% | 9.15e+07 |
| outer.acceptance_linesearch | 2.07% | 0.00% | 7.4e+07 |
| la.dense_solve | 1.44% | 0.00% | 5.127e+07 |
| outer.ipm_nlp_core | 1.16% | 0.00% | 4.15e+07 |
| la.matching_scaling | 1.12% | 0.00% | 4e+07 |
| unattributed.unresolved | 1.06% | 0.00% | 3.8e+07 |
| la.sparse_analyze | 0.42% | 0.00% | 1.5e+07 |
| la.other | 0.13% | 0.00% | 4.5e+06 |
| kkt.assembly | 0.11% | 0.00% | 4e+06 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 58.63% | 0.00% |
| eval | 31.89% | 14.29% |
| setup | 5.01% | 66.67% |
| outer | 3.23% | 0.00% |
| unattributed | 1.11% | 0.00% |
| kkt | 0.13% | 0.00% |
| instr | 0.01% | 0.00% |

### sqp / hs -- all-in
cells: 22; total (instructions): 1.875e+08

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| proc.loader_startup | 48.80% | 52.94% | 9.15e+07 |
| setup.library_init | 16.53% | 17.65% | 3.1e+07 |
| la.dense_solve | 8.00% | 5.88% | 1.5e+07 |
| proc.harness_report | 5.60% | 5.88% | 1.05e+07 |
| proc.main_other | 5.33% | 5.88% | 1e+07 |
| qp.walk | 2.93% | 0.00% | 5.5e+06 |
| qp.schur_border | 2.13% | 0.00% | 4e+06 |
| qp.eqp_dependency | 1.87% | 0.00% | 3.5e+06 |
| qp.refine_polish | 1.60% | 0.00% | 3e+06 |
| instr.ledger_trace | 1.33% | 0.00% | 2.5e+06 |
| kkt.assembly | 1.33% | 0.00% | 2.5e+06 |
| unattributed.unresolved | 1.33% | 0.00% | 2.5e+06 |
| eval.assembly | 1.07% | 0.00% | 2e+06 |
| kkt.factor_glue | 0.53% | 0.00% | 1e+06 |
| la.dense_factorize | 0.53% | 0.00% | 1e+06 |
| setup.init_teardown | 0.53% | 0.00% | 1e+06 |
| eval.callback | 0.27% | 0.00% | 5e+05 |
| la.other | 0.27% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| proc | 59.73% | 63.60% |
| setup | 17.07% | 17.65% |
| la | 8.80% | 6.07% |
| qp | 8.53% | 5.72% |
| kkt | 1.87% | 0.00% |
| instr | 1.33% | 0.00% |
| unattributed | 1.33% | 0.00% |
| eval | 1.33% | 0.00% |

### sqp / hs -- solver only (proc.* excluded)
cells: 22; total (instructions): 7.55e+07

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| setup.library_init | 41.06% | 46.43% | 3.1e+07 |
| la.dense_solve | 19.87% | 15.48% | 1.5e+07 |
| qp.walk | 7.28% | 0.00% | 5.5e+06 |
| qp.schur_border | 5.30% | 0.00% | 4e+06 |
| qp.eqp_dependency | 4.64% | 0.00% | 3.5e+06 |
| qp.refine_polish | 3.97% | 0.00% | 3e+06 |
| instr.ledger_trace | 3.31% | 0.00% | 2.5e+06 |
| kkt.assembly | 3.31% | 0.00% | 2.5e+06 |
| unattributed.unresolved | 3.31% | 0.00% | 2.5e+06 |
| eval.assembly | 2.65% | 0.00% | 2e+06 |
| kkt.factor_glue | 1.32% | 0.00% | 1e+06 |
| la.dense_factorize | 1.32% | 0.00% | 1e+06 |
| setup.init_teardown | 1.32% | 0.00% | 1e+06 |
| eval.callback | 0.66% | 0.00% | 5e+05 |
| la.other | 0.66% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| setup | 42.38% | 50.00% |
| la | 21.85% | 16.67% |
| qp | 21.19% | 14.29% |
| kkt | 4.64% | 0.00% |
| instr | 3.31% | 0.00% |
| unattributed | 3.31% | 0.00% |
| eval | 3.31% | 0.00% |

### sqp / cutest -- all-in
cells: 15; total (instructions): 2.02106e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 62.71% | 67.04% | 1.267e+12 |
| la.sparse_analyze | 17.24% | 14.92% | 3.484e+11 |
| la.sparse_factorize | 7.69% | 7.36% | 1.553e+11 |
| qp.schur_border | 2.97% | 2.25% | 6.006e+10 |
| kkt.assembly | 2.29% | 1.63% | 4.623e+10 |
| qp.walk | 1.85% | 1.46% | 3.735e+10 |
| qp.eqp_dependency | 1.75% | 1.24% | 3.543e+10 |
| la.other | 1.01% | 0.84% | 2.048e+10 |
| kkt.factor_glue | 0.97% | 0.78% | 1.958e+10 |
| setup.library_init | 0.30% | 0.32% | 6.003e+09 |
| qp.refine_polish | 0.25% | 0.47% | 4.955e+09 |
| outer.ipm_nlp_core | 0.22% | 0.25% | 4.441e+09 |
| la.dense_factorize | 0.21% | 0.20% | 4.344e+09 |
| instr.ledger_trace | 0.17% | 0.26% | 3.527e+09 |
| la.matching_scaling | 0.13% | 0.02% | 2.562e+09 |
| unattributed.unresolved | 0.07% | 0.08% | 1.415e+09 |
| la.dense_solve | 0.05% | 0.11% | 1.039e+09 |
| proc.loader_startup | 0.04% | 0.07% | 9.09e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.04% | 89.23% |
| qp | 6.82% | 6.35% |
| kkt | 3.26% | 2.42% |
| setup | 0.30% | 0.32% |
| outer | 0.24% | 0.27% |
| instr | 0.17% | 0.26% |
| unattributed | 0.07% | 0.08% |
| proc | 0.06% | 0.07% |
| eval | 0.04% | 0.04% |

### sqp / cutest -- solver only (proc.* excluded)
cells: 15; total (instructions): 2.01985e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 62.75% | 67.09% | 1.267e+12 |
| la.sparse_analyze | 17.25% | 14.92% | 3.484e+11 |
| la.sparse_factorize | 7.69% | 7.36% | 1.553e+11 |
| qp.schur_border | 2.97% | 2.27% | 6.006e+10 |
| kkt.assembly | 2.29% | 1.63% | 4.623e+10 |
| qp.walk | 1.85% | 1.48% | 3.735e+10 |
| qp.eqp_dependency | 1.75% | 1.24% | 3.543e+10 |
| la.other | 1.01% | 0.88% | 2.048e+10 |
| kkt.factor_glue | 0.97% | 0.78% | 1.958e+10 |
| setup.library_init | 0.30% | 0.32% | 6.003e+09 |
| qp.refine_polish | 0.25% | 0.47% | 4.955e+09 |
| outer.ipm_nlp_core | 0.22% | 0.25% | 4.441e+09 |
| la.dense_factorize | 0.22% | 0.20% | 4.344e+09 |
| instr.ledger_trace | 0.17% | 0.26% | 3.527e+09 |
| la.matching_scaling | 0.13% | 0.02% | 2.562e+09 |
| unattributed.unresolved | 0.07% | 0.08% | 1.415e+09 |
| la.dense_solve | 0.05% | 0.11% | 1.039e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.10% | 89.86% |
| qp | 6.82% | 6.36% |
| kkt | 3.26% | 2.42% |
| setup | 0.30% | 0.32% |
| outer | 0.24% | 0.27% |
| instr | 0.17% | 0.26% |
| unattributed | 0.07% | 0.08% |
| eval | 0.04% | 0.04% |

### sqp / sd -- all-in
cells: 97; total (instructions): 1.14074e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 78.14% | 0.00% | 8.914e+11 |
| qp.schur_border | 4.92% | 5.00% | 5.607e+10 |
| la.sparse_analyze | 4.40% | 0.00% | 5.02e+10 |
| la.dense_solve | 3.07% | 11.76% | 3.499e+10 |
| la.sparse_factorize | 2.07% | 0.00% | 2.362e+10 |
| la.dense_factorize | 1.77% | 0.00% | 2.015e+10 |
| qp.refine_polish | 0.96% | 1.68% | 1.101e+10 |
| qp.eqp_dependency | 0.73% | 0.87% | 8.278e+09 |
| outer.ipm_nlp_core | 0.65% | 0.00% | 7.37e+09 |
| kkt.assembly | 0.62% | 0.00% | 7.08e+09 |
| la.other | 0.58% | 0.00% | 6.621e+09 |
| qp.walk | 0.50% | 2.44% | 5.705e+09 |
| setup.library_init | 0.44% | 8.70% | 5.016e+09 |
| instr.ledger_trace | 0.43% | 0.44% | 4.95e+09 |
| kkt.factor_glue | 0.36% | 0.00% | 4.062e+09 |
| unattributed.unresolved | 0.14% | 0.00% | 1.623e+09 |
| proc.loader_startup | 0.12% | 34.78% | 1.382e+09 |
| eval.callback | 0.06% | 0.00% | 7.087e+08 |
| proc.problem_load | 0.01% | 5.00% | 1.204e+08 |
| proc.main_other | 0.00% | 1.85% | 3.831e+07 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 90.04% | 18.18% |
| qp | 7.11% | 11.90% |
| kkt | 0.98% | 0.00% |
| outer | 0.65% | 0.00% |
| setup | 0.44% | 9.09% |
| instr | 0.43% | 0.44% |
| unattributed | 0.14% | 0.00% |
| proc | 0.14% | 45.45% |
| eval | 0.07% | 0.11% |

### sqp / sd -- solver only (proc.* excluded)
cells: 97; total (instructions): 1.13918e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 78.25% | 0.00% | 8.914e+11 |
| qp.schur_border | 4.92% | 7.24% | 5.607e+10 |
| la.sparse_analyze | 4.41% | 0.00% | 5.02e+10 |
| la.dense_solve | 3.07% | 23.53% | 3.499e+10 |
| la.sparse_factorize | 2.07% | 0.00% | 2.362e+10 |
| la.dense_factorize | 1.77% | 0.00% | 2.015e+10 |
| qp.refine_polish | 0.97% | 2.17% | 1.101e+10 |
| qp.eqp_dependency | 0.73% | 0.95% | 8.278e+09 |
| outer.ipm_nlp_core | 0.65% | 0.00% | 7.37e+09 |
| kkt.assembly | 0.62% | 0.00% | 7.08e+09 |
| la.other | 0.58% | 0.00% | 6.621e+09 |
| qp.walk | 0.50% | 2.94% | 5.705e+09 |
| setup.library_init | 0.44% | 18.18% | 5.016e+09 |
| instr.ledger_trace | 0.43% | 0.52% | 4.95e+09 |
| kkt.factor_glue | 0.36% | 0.00% | 4.062e+09 |
| unattributed.unresolved | 0.14% | 0.00% | 1.623e+09 |
| eval.callback | 0.06% | 0.00% | 7.087e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 90.17% | 33.33% |
| qp | 7.12% | 20.00% |
| kkt | 0.98% | 0.00% |
| outer | 0.65% | 0.00% |
| setup | 0.44% | 16.67% |
| instr | 0.43% | 0.45% |
| unattributed | 0.14% | 0.00% |
| eval | 0.07% | 0.11% |

## Truncated (deadline-class) cells: informational shares, totals not asserted

### ipm / truncated -- solver only
cells: 4; total (instructions): 1.05338e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 89.46% | 93.33% | 9.424e+11 |
| eval.callback | 2.71% | 2.05% | 2.858e+10 |
| la.sparse_solve | 2.22% | 2.50% | 2.34e+10 |
| la.sparse_analyze | 2.17% | 0.43% | 2.282e+10 |
| outer.ipm_nlp_core | 1.31% | 0.64% | 1.38e+10 |
| unattributed.unresolved | 1.13% | 0.67% | 1.186e+10 |
| eval.assembly | 0.79% | 0.88% | 8.28e+09 |
| setup.library_init | 0.09% | 0.08% | 8.983e+08 |
| outer.acceptance_linesearch | 0.08% | 0.01% | 8.2e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 93.87% | 96.06% |
| eval | 3.50% | 2.93% |
| outer | 1.39% | 0.64% |
| unattributed | 1.13% | 0.67% |
| setup | 0.11% | 0.10% |
| kkt | 0.01% | 0.00% |

### sqp / truncated -- solver only
cells: 8; total (instructions): 2.1621e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 52.67% | 50.36% | 1.139e+12 |
| la.sparse_factorize | 19.94% | 9.13% | 4.312e+11 |
| la.sparse_analyze | 11.79% | 10.63% | 2.548e+11 |
| instr.ledger_trace | 5.08% | 0.11% | 1.099e+11 |
| qp.walk | 3.10% | 0.88% | 6.692e+10 |
| qp.eqp_dependency | 1.38% | 0.57% | 2.976e+10 |
| kkt.assembly | 1.08% | 1.00% | 2.342e+10 |
| qp.schur_border | 1.06% | 0.69% | 2.296e+10 |
| qp.refine_polish | 1.00% | 0.32% | 2.162e+10 |
| outer.scaling_basis | 0.69% | 0.00% | 1.488e+10 |
| la.other | 0.49% | 0.44% | 1.066e+10 |
| kkt.factor_glue | 0.42% | 0.32% | 9.12e+09 |
| eval.callback | 0.33% | 0.04% | 7.04e+09 |
| setup.library_init | 0.25% | 0.22% | 5.47e+09 |
| unattributed.unresolved | 0.19% | 0.07% | 4e+09 |
| eval.assembly | 0.17% | 0.05% | 3.78e+09 |
| la.dense_solve | 0.11% | 0.01% | 2.274e+09 |
| outer.ipm_nlp_core | 0.10% | 0.07% | 2.26e+09 |
| la.matching_scaling | 0.10% | 0.01% | 2.14e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 85.15% | 92.22% |
| qp | 6.53% | 3.62% |
| instr | 5.08% | 0.11% |
| kkt | 1.51% | 1.32% |
| outer | 0.79% | 0.17% |
| eval | 0.50% | 0.13% |
| setup | 0.26% | 0.22% |
| unattributed | 0.19% | 0.08% |

## Overlays (non-additive; share of each cell group's total)

| group | alloc_leaf | iomp_leaf | fortran_leaf | unattributed |
|---|---:|---:|---:|---:|
| suite | 1.71% | 2.90% | 2.24% | 0.09% |
| ipm | 0.39% | 3.59% | 15.81% | 0.06% |
| sqp | 1.92% | 2.78% | 0.04% | 0.10% |
| ipm/hs | 1.39% | 2.43% | 0.00% | 2.08% |
| ipm/cutest | 0.38% | 3.59% | 15.76% | 0.05% |
| ipm/sd | 0.72% | 3.63% | 22.42% | 0.96% |
| sqp/hs | 5.33% | 8.53% | 0.27% | 1.33% |
| sqp/cutest | 1.50% | 1.85% | 0.03% | 0.07% |
| sqp/sd | 2.66% | 4.44% | 0.06% | 0.14% |

## Cells where a component dominates (>= 50% of the cell's solver-only total)

- `eval.callback`: hven-ipm__ACOPP118 (57%), hven-ipm__ACOPP57 (53%), hven-ipm__ACOPR118 (51%), hven-ipm__ACOPP30 (51%), hven-ipm__ALLINITC (50%), hven-ipm__DECONVC (65%), hven-ipm__HS111LNP (50%), hven-ipm__HS59 (50%), hven-ipm__HS70 (75%), hven-ipm__HS88 (91%), hven-ipm__HS89 (88%), hven-ipm__HS90 (92%), hven-ipm__HS91 (97%), hven-ipm__HS92 (95%), hven-sqp__HS85 (58%), hven-sqp__HS89 (71%), hven-sqp__HS90 (73%), hven-sqp__HS91 (73%), hven-sqp__HS92 (54%)
- `instr.ledger_trace`: hven-sqp__TAX213322 (52%*)
- `la.dense_factorize`: hven-ipm__READING6 (67%), hven-ipm__DEMBO7 (79%), hven-ipm__DNIEPER (70%), hven-ipm__HS99EXP (79%), hven-ipm__LAUNCH (71%), hven-ipm__LEWISPOL (53%), hven-ipm__MESH (89%), hven-ipm__MSS1 (87%), hven-ipm__NET1 (74%), hven-ipm__OPTPRLOC (69%), hven-ipm__SPANHYD (83%)
- `la.dense_solve`: hven-sqp__DECONVC (65%), hven-sqp__DEMBO7 (63%), hven-sqp__DIXCHLNG (64%), hven-sqp__HS101 (51%), hven-sqp__HS108 (63%), hven-sqp__HS111LNP (58%), hven-sqp__HS113 (55%), hven-sqp__HS99EXP (67%), hven-sqp__LEWISPOL (50%), hven-sqp__OPTPRLOC (67%), hven-sqp__WATER (64%)
- `la.sparse_factorize`: hven-ipm__BIGBANK (58%), hven-ipm__BRITGAS (70%), hven-ipm__MSS2 (98%*), hven-ipm__MSS3 (97%*), hven-ipm__NET3 (54%), hven-ipm__READING8 (90%*), hven-ipm__STEENBRB (81%), hven-ipm__STEENBRC (82%), hven-ipm__STEENBRD (83%), hven-ipm__STEENBRE (83%), hven-ipm__STEENBRG (82%), hven-ipm__TAX53322 (59%), hven-ipm__YORKNET (65%), hven-sqp__MSS3 (67%*)
- `la.sparse_solve`: hven-sqp__ACOPP118 (67%), hven-sqp__ACOPP57 (70%), hven-sqp__ACOPR118 (87%*), hven-sqp__ACOPR57 (71%), hven-sqp__BIGBANK (56%), hven-sqp__BRITGAS (54%*), hven-sqp__DALLASM (86%), hven-sqp__NET3 (56%*), hven-sqp__READING6 (83%), hven-sqp__SAWPATH (87%), hven-sqp__SMBANK (89%), hven-sqp__STEENBRB (61%), hven-sqp__STEENBRC (61%), hven-sqp__STEENBRD (63%), hven-sqp__STEENBRE (64%), hven-sqp__STEENBRG (64%), hven-sqp__TAX53322 (84%*), hven-sqp__YORKNET (61%), hven-sqp__ZAMB2-8 (74%), hven-sqp__ACOPP30 (68%), hven-sqp__CRESC100 (76%), hven-sqp__CRESC50 (72%), hven-sqp__DNIEPER (67%), hven-sqp__LAKES (80%), hven-sqp__MESH (71%), hven-sqp__MSS1 (64%), hven-sqp__NET1 (76%), hven-sqp__SPANHYD (90%)
- `qp.schur_border`: hven-sqp__HS42 (50%)
- `setup.init_teardown`: hven-sqp__hs025 (100%)
- `setup.library_init`: hven-ipm__hs005 (100%), hven-ipm__hs006 (100%), hven-ipm__hs007 (100%), hven-ipm__hs010 (60%), hven-ipm__hs011 (67%), hven-ipm__hs012 (80%), hven-ipm__hs014 (75%), hven-ipm__hs022 (100%), hven-ipm__hs026 (80%), hven-ipm__hs027 (80%), hven-ipm__hs028 (100%), hven-ipm__hs030 (80%), hven-ipm__hs033 (80%), hven-ipm__hs035 (100%), hven-ipm__hs039 (60%), hven-ipm__hs043 (75%), hven-ipm__hs045 (75%), hven-ipm__hs071 (75%), hven-ipm__hs077 (75%), hven-ipm__hs079 (100%), hven-sqp__hs003 (50%), hven-sqp__hs005 (67%), hven-sqp__hs006 (50%), hven-sqp__hs014 (60%), hven-sqp__hs022 (100%), hven-sqp__hs028 (75%), hven-sqp__hs035 (67%), hven-sqp__hs043 (50%), hven-sqp__hs045 (60%), hven-sqp__hs071 (57%), hven-sqp__hs079 (60%), hven-ipm__ALSOTAME (100%), hven-ipm__BT11 (80%), hven-ipm__BT12 (100%), hven-ipm__BT2 (100%), hven-ipm__BT6 (80%), hven-ipm__BT7 (67%), hven-ipm__BT8 (57%), hven-ipm__BURKEHAN (80%), hven-ipm__HIMMELP2 (100%), hven-ipm__HIMMELP3 (80%), hven-ipm__HIMMELP4 (80%), hven-ipm__HIMMELP6 (67%), hven-ipm__HS100LNP (50%), hven-ipm__HS107 (62%), hven-ipm__HS11 (100%), hven-ipm__HS113 (50%), hven-ipm__HS15 (80%), hven-ipm__HS17 (80%), hven-ipm__HS19 (80%), hven-ipm__HS22 (100%), hven-ipm__HS26 (80%), hven-ipm__HS27 (80%), hven-ipm__HS30 (80%), hven-ipm__HS31 (75%), hven-ipm__HS32 (80%), hven-ipm__HS33 (80%), hven-ipm__HS40 (80%), hven-ipm__HS42 (100%), hven-ipm__HS43 (75%), hven-ipm__HS46 (80%), hven-ipm__HS56 (67%), hven-ipm__HS59 (50%), hven-ipm__HS6 (100%), hven-ipm__HS60 (100%), hven-ipm__HS61 (80%), hven-ipm__HS63 (100%), hven-ipm__HS64 (71%), hven-ipm__HS68 (80%), hven-ipm__HS7 (100%), hven-ipm__HS74 (80%), hven-ipm__HS75 (67%), hven-ipm__HS79 (100%), hven-ipm__HS80 (80%), hven-ipm__HS81 (57%), hven-ipm__HS83 (57%), hven-ipm__HS93 (80%), hven-ipm__LOOTSMA (67%), hven-ipm__LSNNODOC (57%), hven-ipm__MATRIX2 (67%), hven-ipm__MWRIGHT (80%), hven-ipm__S365 (100%), hven-ipm__S365MOD (100%), hven-ipm__SYNTHES1 (67%), hven-ipm__TRY-B (80%), hven-ipm__TWOBARS (100%), hven-ipm__ZECEVIC3 (80%), hven-ipm__ZECEVIC4 (100%), hven-ipm__ZY2 (100%), hven-sqp__ALSOTAME (60%), hven-sqp__HS22 (50%), hven-sqp__HS27 (50%), hven-sqp__HS31 (50%), hven-sqp__HS32 (67%), hven-sqp__HS42 (50%), hven-sqp__HS60 (50%), hven-sqp__HS79 (50%), hven-sqp__HS80 (57%), hven-sqp__MWRIGHT (50%), hven-sqp__SYNTHES1 (50%)
(* = truncated cell, share only)

## Top leaf symbols, full cells (corpus; from each cell's top 60)

| leaf symbol | dso | component | share |
|---|---|---|---:|
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | hven_bench_cell | la.sparse_solve | 16.99% |
| `mkl_spblas_lp64_def_dcsr1nsunf__mvout_par` | hven_bench_cell | la.sparse_solve | 10.35% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | hven_bench_cell | la.sparse_solve | 6.71% |
| `mkl_pds_lp64_blkl_ll_real.extracted` | hven_bench_cell | la.sparse_factorize | 3.22% |
| `hven::detail::SchurComplement::append_border(Eigen::Matrix<double, -1, 1, 0, -1, 1> const&` | hven_bench_cell | qp.schur_border | 2.65% |
| `find_norm` | hven_bench_cell | la.sparse_solve | 2.42% |
| `mkl_pds_lp64_do_all_pardiso_fc` | hven_bench_cell | la.sparse_solve | 2.38% |
| `mkl_pds_lp64_dsytrs_bklbw_pardiso` | hven_bench_cell | la.sparse_solve | 2.34% |
| `mkl_blas_def_xdger_kernel` | hven_bench_cell | unattributed.truncated_unwind | 2.27% |
| `mkl_pds_lp64_upd_real_block` | hven_bench_cell | la.sparse_factorize | 2.24% |
| `mkl_pds_lp64_dsytrs_bklfw_noscal_pardiso` | hven_bench_cell | la.sparse_solve | 2.13% |
| `mkl_blas_def_xdgemv_kernel_t` | hven_bench_cell | unattributed.truncated_unwind | 1.77% |
| `mkl_blas_def_dgemm_kernel_zen` | hven_bench_cell | unattributed.truncated_unwind | 1.41% |
| `L_not_one_loopgas_1` | hven_bench_cell | la.dense_solve | 1.34% |
| `mkl_pds_lp64_fct_real_block` | hven_bench_cell | la.sparse_factorize | 1.14% |
| `mkl_blas_dscal` | hven_bench_cell | la.dense_solve | 1.13% |
| `mkl_blas_def_xdgemv` | hven_bench_cell | la.dense_solve | 1.10% |
| `void Eigen::internal::generic_product_impl<Eigen::SparseSelfAdjointView<Eigen::SparseMatri` | hven_bench_cell | qp.refine_polish | 1.05% |
| `mkl_pds_lp64_metis_fm_2waynoderefine_onesided` | hven_bench_cell | la.sparse_analyze | 1.02% |
| `mkl_blas_lp64_dgemv` | hven_bench_cell | la.sparse_solve | 0.98% |
| `mkl_blas_def_xdger` | hven_bench_cell | la.dense_solve | 0.91% |
| `mkl_blas_def_xdscal` | hven_bench_cell | la.dense_solve | 0.85% |
| `cutest_ccfsg_threadsafe_` | libcutest_double.so | eval.callback | 0.83% |
| `mkl_blas_lp64_dscal` | hven_bench_cell | la.sparse_solve | 0.81% |
| `__cutest_double_MOD_cutest_assemble_hessian` | libcutest_double.so | eval.callback | 0.77% |
| `mkl_pds_lp64_metis_fm_2wayedgerefine` | hven_bench_cell | la.sparse_analyze | 0.77% |
| `mkl_pds_lp64_dsytrs_bklfw_pardiso` | hven_bench_cell | la.sparse_factorize | 0.76% |
| `mkl_pds_lp64_metis_createcoarsegraphnomask` | hven_bench_cell | la.sparse_analyze | 0.76% |
| `mkl_pds_lp64_dsytf2_pardiso` | hven_bench_cell | la.sparse_factorize | 0.75% |
| `_int_malloc` | libc.so.6 | qp.schur_border | 0.72% |
| `__kmpc_flush` | libiomp5.so | la.sparse_factorize | 0.72% |
| `mkl_pds_lp64_pds_compute_scaled_norm_real` | hven_bench_cell | la.sparse_solve | 0.71% |
| `hven::detail::assemble_kkt_core(hven::detail::QpProblem const&, hven::detail::WorkingSet c` | hven_bench_cell | kkt.assembly | 0.70% |
| `mkl_blas_def_xdgemv_kernel_n` | hven_bench_cell | unattributed.truncated_unwind | 0.69% |
| `mkl_blas_xdscal` | hven_bench_cell | la.dense_factorize | 0.58% |
| `mkl_pds_lp64_metis_mmdelm` | hven_bench_cell | la.sparse_analyze | 0.57% |
| `__memset_avx2_unaligned_erms` | libc.so.6 | qp.walk | 0.54% |
| `mkl_pds_lp64_metis_mmdupd` | hven_bench_cell | la.sparse_analyze | 0.52% |
| `mkl_pds_lp64_psol_bwscat_pardiso_mic.DIR.OMP.PARALLEL.6` | hven_bench_cell | la.sparse_solve | 0.49% |
| `mkl_blas_def_dgemm_pst` | hven_bench_cell | la.dense_factorize | 0.43% |

## Top hven frames, inclusive (full cells; from each cell's top 80)

| frame | inclusive share |
|---|---:|
| `hven::detail::QpEngine::run` | 81.96% |
| `hven::linear::detail::FactorSession::run_phase` | 80.00% |
| `hven::detail::QpEngine::border_candidate` | 69.14% |
| `hven::detail::QpEngine::solve` | 68.49% |
| `hven::linear::SymmetricFactor::solve_single` | 58.78% |
| `hven::linear::` | 58.45% |
| `hven::linear::detail::FactorSession::solve` | 58.30% |
| `hven::detail::SqpSolverImpl::run_major` | 57.22% |
| `hven::detail::solve_vec` | 56.82% |
| `hven::detail::SqpSolverImpl::solve_with_walk` | 52.05% |
| `hven::detail::QpEngine::sync_borders` | 37.79% |
| `hven::detail::SchurComplement::add_border` | 37.31% |
| `hven::detail::QpEngine::eliminated_candidate` | 27.75% |
| `hven::detail::solve_eqp` | 27.74% |
| `hven::detail::factorize_checked` | 16.35% |
| `hven::linear::SymmetricFactor::analyze` | 11.30% |
| `hven::linear::SymmetricFactor::factorize` | 11.05% |
| `hven::detail::IpmSolverImpl::alg_impl` | 11.00% |
| `hven::linear::detail::FactorSession::analyze` | 10.89% |
| `hven::linear::detail::FactorSession::factorize` | 10.70% |
| `hven::detail::IpmSolverImpl::run_phase_sequence` | 10.67% |
| `hven::detail::IpmSolverImpl::solve` | 10.35% |
| `hven::IpmSolver::solve` | 10.31% |
| `hven::detail::QpEngine::border_solve_or_fall_back` | 9.78% |
| `hven::detail::QpEngine::probe_inertia` | 9.67% |
| `hven::detail::SqpSolverImpl::solve_impl_body` | 9.60% |
| `hven::detail::run_elastic_ladder` | 9.21% |
| `hven::detail::` | 8.39% |
| `hven::detail::QpEngine::test_candidate` | 6.47% |
| `hven::detail::QpEngine::repair_temporary_vertex` | 6.21% |
| `hven::detail::IpmSolverImpl::factor_impl` | 6.05% |
| `hven::detail::KktFactorization::refactorize` | 6.05% |
| `hven::detail::SchurComplement::solve` | 5.66% |
| `hven::detail::dependency_test` | 5.51% |
| `hven::detail::solve_bordered_eqp` | 4.92% |
| `hven::detail::polish_eqp_core` | 4.42% |
| `hven::detail::QpEngine::probe_zero_multiplier_drops` | 4.31% |
| `hven::detail::certificate_coefficients` | 2.81% |
| `hven::detail::SchurComplement::append_border` | 2.73% |
| `hven::NlpAssembly::assemble` | 2.73% |
| `hven::detail::NonLinearProgram::assemble_impl` | 2.70% |
| `hven::detail::IpmSolverImpl::eval_nlp` | 2.45% |
| `hven::detail::IpmSolverImpl::assemble_dispatch` | 2.45% |
| `hven::detail::NonLinearProgram::full_kkt_pass` | 2.43% |
| `hven::ConstraintModel<hven::detail::NlpConstraintPiece>::constraints_jacobian_adjointgradient_adjointhessian` | 2.36% |

## Backend drivers: the first hven frame above the linear/KKT glue, by backend component (full cells, corpus)

### ipm / la.sparse_factorize (total 2.966e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::IpmSolverImpl::factor_impl` | 74.6% |
| `hven::detail::IpmSolverImpl::init_impl` | 0.1% |

### ipm / la.sparse_solve (total 1.01e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::IpmSolverImpl::alg_impl` | 73.5% |
| `hven::detail::IpmSolverImpl::init_impl` | 0.1% |

### sqp / la.dense_factorize (total 2.45e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::factor_c` | 54.7% |
| `hven::detail::QpEngine::run` | 22.0% |
| `hven::detail::QpEngine::rebuild_k0` | 3.6% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 1.8% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |

### sqp / la.dense_solve (total 3.605e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::solve` | 29.8% |
| `hven::detail::SchurComplement::add_border` | 11.1% |
| `hven::detail::QpEngine::run` | 8.1% |
| `hven::detail::take_latch` | 5.5% |
| `hven::detail::` | 5.5% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.5% |
| `hven::detail::detect_dependency` | 0.3% |
| `hven::detail::face_solve` | 0.2% |

### sqp / la.other (total 2.71e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 25.9% |
| `hven::detail::QpEngine::rebuild_k0` | 0.6% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.3% |
| `hven::detail::SchurComplement::solve` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |
| `hven::detail::SchurComplement::factor_c` | 0.0% |
| `hven::detail::EqualityBasis::Impl::~Impl` | 0.0% |

### sqp / la.sparse_analyze (total 3.986e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 95.4% |
| `hven::detail::QpEngine::rebuild_k0` | 3.8% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.7% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |
| `hven::detail::QpEngine::refine_terminal_mu_free` | 0.0% |

### sqp / la.sparse_factorize (total 1.79e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 81.9% |
| `hven::detail::QpEngine::rebuild_k0` | 11.8% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.5% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |

### sqp / la.sparse_solve (total 2.159e+12)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::add_border` | 57.9% |
| `hven::detail::QpEngine::run` | 17.6% |
| `hven::detail::` | 10.3% |
| `hven::detail::SchurComplement::solve` | 8.7% |
| `hven::detail::detect_dependency` | 0.2% |
| `hven::detail::face_solve` | 0.1% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.1% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |

## Non-hven leaf symbols by nearest hven caller (full cells; from each cell's top 80 pairs)

### ipm

| leaf | nearest hven frame | share |
|---|---|---:|
| `mkl_pds_lp64_blkl_ll_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 12.94% |
| `mkl_blas_dgemm_kernel_zen` | `-` | 9.87% |
| `mkl_pds_lp64_upd_real_block` | `hven::linear::detail::FactorSession::run_phase` | 8.77% |
| `mkl_blas_xdger_kernel` | `-` | 8.60% |
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 5.53% |
| `__cutest_double_MOD_cutest_assemble_hessian` | `hven::detail::CountingNlp::eval_hessian` | 5.53% |
| `mkl_pds_lp64_fct_real_block` | `hven::linear::detail::FactorSession::run_phase` | 5.15% |
| `cutest_ccfsg_threadsafe_` | `hven::detail::CountingNlp::eval_jacobian` | 4.45% |
| `mkl_pds_lp64_dsytrs_bklfw_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 4.21% |
| `__kmpc_flush` | `hven::linear::detail::FactorSession::run_phase` | 3.52% |
| `mkl_pds_lp64_dsytf2_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 3.12% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.38% |
| `mkl_spblas_dcsr1nsunf__mvout_par` | `hven::linear::detail::FactorSession::run_phase` | 2.32% |
| `__cutest_double_MOD_cutest_form_gradients` | `hven::detail::CountingNlp::eval_hessian` | 2.26% |

### sqp

| leaf | nearest hven frame | share |
|---|---|---:|
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 18.85% |
| `mkl_spblas_dcsr1nsunf__mvout_par` | `hven::linear::detail::FactorSession::run_phase` | 11.66% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 7.41% |
| `find_norm` | `hven::linear::detail::FactorSession::run_phase` | 2.78% |
| `mkl_pds_lp64_do_all_pardiso_fc` | `hven::linear::detail::FactorSession::run_phase` | 2.74% |
| `mkl_pds_lp64_dsytrs_bklbw_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.57% |
| `mkl_pds_lp64_dsytrs_bklfw_noscal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.35% |
| `mkl_blas_xdgemv_kernel_t` | `-` | 2.02% |
| `mkl_pds_lp64_blkl_ll_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 1.64% |
| `mkl_blas_xdscal` | `hven::linear::detail::FactorSession::run_phase` | 1.57% |
| `mkl_blas_xdgemv` | `hven::linear::detail::FactorSession::run_phase` | 1.57% |
| `L_not_one_loopgas_1` | `hven::linear::detail::FactorSession::run_phase` | 1.46% |
| `mkl_blas_xdger` | `hven::linear::detail::FactorSession::run_phase` | 1.35% |
| `mkl_blas_dscal` | `hven::linear::detail::FactorSession::run_phase` | 1.25% |

Raw unattributed.truncated_unwind before redistribution (full cells): 6.73% of all-in total; unresolved after: 0.09%.
