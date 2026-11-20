# PF-3 attribution tables (instructions)
cells read: 284 (272 full, 12 truncated); failed: 0

### SUITE (all full cells) -- all-in
cells: 272; total (instructions): 3.67427e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 61.17% | 0.00% | 2.248e+12 |
| la.sparse_factorize | 13.23% | 0.00% | 4.86e+11 |
| la.sparse_analyze | 10.92% | 0.00% | 4.012e+11 |
| qp.schur_border | 3.26% | 0.00% | 1.2e+11 |
| eval.callback | 2.47% | 0.00% | 9.065e+10 |
| qp.eqp_dependency | 1.65% | 0.00% | 6.052e+10 |
| qp.walk | 1.39% | 0.00% | 5.106e+10 |
| kkt.assembly | 1.23% | 0.00% | 4.518e+10 |
| la.dense_solve | 0.97% | 0.00% | 3.547e+10 |
| la.dense_factorize | 0.75% | 0.00% | 2.753e+10 |
| la.other | 0.67% | 0.00% | 2.476e+10 |
| qp.refine_polish | 0.60% | 0.00% | 2.218e+10 |
| outer.ipm_nlp_core | 0.43% | 0.00% | 1.577e+10 |
| setup.library_init | 0.35% | 12.25% | 1.302e+10 |
| eval.assembly | 0.28% | 0.00% | 1.012e+10 |
| kkt.factor_glue | 0.18% | 0.00% | 6.685e+09 |
| outer.acceptance_linesearch | 0.10% | 0.00% | 3.724e+09 |
| unattributed.unresolved | 0.10% | 0.00% | 3.501e+09 |
| proc.loader_startup | 0.09% | 42.11% | 3.282e+09 |
| la.matching_scaling | 0.07% | 0.00% | 2.642e+09 |
| proc.problem_load | 0.07% | 5.26% | 2.477e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 87.78% | 9.52% |
| qp | 6.91% | 0.00% |
| eval | 2.74% | 0.04% |
| kkt | 1.41% | 0.00% |
| outer | 0.55% | 0.00% |
| setup | 0.36% | 12.50% |
| proc | 0.16% | 55.56% |
| unattributed | 0.10% | 0.00% |
| instr | 0.00% | 0.00% |

### SUITE (all full cells) -- solver only (proc.* excluded)
cells: 272; total (instructions): 3.66841e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 61.27% | 0.00% | 2.248e+12 |
| la.sparse_factorize | 13.25% | 0.00% | 4.86e+11 |
| la.sparse_analyze | 10.94% | 0.00% | 4.012e+11 |
| qp.schur_border | 3.27% | 0.00% | 1.2e+11 |
| eval.callback | 2.47% | 0.00% | 9.065e+10 |
| qp.eqp_dependency | 1.65% | 0.00% | 6.052e+10 |
| qp.walk | 1.39% | 0.00% | 5.106e+10 |
| kkt.assembly | 1.23% | 0.00% | 4.518e+10 |
| la.dense_solve | 0.97% | 0.00% | 3.547e+10 |
| la.dense_factorize | 0.75% | 0.00% | 2.753e+10 |
| la.other | 0.67% | 0.00% | 2.476e+10 |
| qp.refine_polish | 0.60% | 0.00% | 2.218e+10 |
| outer.ipm_nlp_core | 0.43% | 0.00% | 1.577e+10 |
| setup.library_init | 0.35% | 28.57% | 1.302e+10 |
| eval.assembly | 0.28% | 0.00% | 1.012e+10 |
| kkt.factor_glue | 0.18% | 0.00% | 6.685e+09 |
| outer.acceptance_linesearch | 0.10% | 0.00% | 3.724e+09 |
| unattributed.unresolved | 0.10% | 0.00% | 3.501e+09 |
| la.matching_scaling | 0.07% | 0.00% | 2.642e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 87.92% | 21.11% |
| qp | 6.92% | 0.00% |
| eval | 2.75% | 0.04% |
| kkt | 1.41% | 0.00% |
| outer | 0.55% | 0.00% |
| setup | 0.36% | 28.57% |
| unattributed | 0.10% | 0.00% |
| instr | 0.00% | 0.00% |

### ipm / all -- all-in
cells: 138; total (instructions): 5.12293e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 59.41% | 0.00% | 3.044e+11 |
| la.sparse_solve | 18.23% | 0.00% | 9.338e+10 |
| eval.callback | 17.47% | 1.16% | 8.948e+10 |
| eval.assembly | 1.95% | 0.00% | 9.971e+09 |
| outer.ipm_nlp_core | 0.93% | 0.00% | 4.763e+09 |
| outer.acceptance_linesearch | 0.73% | 0.00% | 3.722e+09 |
| proc.problem_load | 0.40% | 6.67% | 2.066e+09 |
| la.dense_factorize | 0.23% | 0.00% | 1.182e+09 |
| la.sparse_analyze | 0.21% | 0.00% | 1.101e+09 |
| setup.library_init | 0.14% | 23.08% | 6.976e+08 |
| unattributed.unresolved | 0.13% | 0.00% | 6.617e+08 |
| proc.loader_startup | 0.12% | 53.33% | 6.138e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.12% | 1.22% |
| eval | 19.41% | 6.25% |
| outer | 1.66% | 0.80% |
| proc | 0.53% | 60.77% |
| setup | 0.15% | 23.08% |
| unattributed | 0.13% | 0.00% |
| instr | 0.00% | 0.00% |
| kkt | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### ipm / all -- solver only (proc.* excluded)
cells: 138; total (instructions): 5.09588e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 59.73% | 0.00% | 3.044e+11 |
| la.sparse_solve | 18.32% | 0.00% | 9.338e+10 |
| eval.callback | 17.56% | 1.33% | 8.948e+10 |
| eval.assembly | 1.96% | 0.00% | 9.971e+09 |
| outer.ipm_nlp_core | 0.93% | 0.00% | 4.763e+09 |
| outer.acceptance_linesearch | 0.73% | 0.00% | 3.722e+09 |
| la.dense_factorize | 0.23% | 0.00% | 1.182e+09 |
| la.sparse_analyze | 0.22% | 0.00% | 1.101e+09 |
| setup.library_init | 0.14% | 63.33% | 6.976e+08 |
| unattributed.unresolved | 0.13% | 0.00% | 6.617e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.54% | 1.34% |
| eval | 19.52% | 10.27% |
| outer | 1.67% | 0.81% |
| setup | 0.15% | 63.33% |
| unattributed | 0.13% | 0.00% |
| instr | 0.00% | 0.00% |
| kkt | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |

### sqp / all -- all-in
cells: 134; total (instructions): 3.16198e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 68.13% | 0.00% | 2.154e+12 |
| la.sparse_analyze | 12.65% | 0.00% | 4.001e+11 |
| la.sparse_factorize | 5.74% | 0.00% | 1.816e+11 |
| qp.schur_border | 3.79% | 1.99% | 1.2e+11 |
| qp.eqp_dependency | 1.91% | 1.57% | 6.052e+10 |
| qp.walk | 1.61% | 3.04% | 5.106e+10 |
| kkt.assembly | 1.43% | 0.00% | 4.518e+10 |
| la.dense_solve | 1.12% | 6.07% | 3.538e+10 |
| la.dense_factorize | 0.83% | 0.00% | 2.635e+10 |
| la.other | 0.78% | 0.00% | 2.47e+10 |
| qp.refine_polish | 0.70% | 1.95% | 2.217e+10 |
| setup.library_init | 0.39% | 9.09% | 1.232e+10 |
| outer.ipm_nlp_core | 0.35% | 0.00% | 1.101e+10 |
| kkt.factor_glue | 0.21% | 0.00% | 6.682e+09 |
| unattributed.unresolved | 0.09% | 0.00% | 2.839e+09 |
| proc.loader_startup | 0.08% | 36.36% | 2.668e+09 |
| la.matching_scaling | 0.08% | 0.00% | 2.601e+09 |
| proc.problem_load | 0.01% | 1.92% | 4.114e+08 |
| proc.main_other | 0.00% | 1.33% | 4.623e+07 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.35% | 16.67% |
| qp | 8.03% | 11.76% |
| kkt | 1.64% | 0.00% |
| setup | 0.39% | 9.09% |
| outer | 0.37% | 0.00% |
| proc | 0.10% | 47.72% |
| unattributed | 0.09% | 0.00% |
| eval | 0.04% | 0.00% |

### sqp / all -- solver only (proc.* excluded)
cells: 134; total (instructions): 3.15883e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 68.20% | 0.00% | 2.154e+12 |
| la.sparse_analyze | 12.67% | 0.00% | 4.001e+11 |
| la.sparse_factorize | 5.75% | 0.00% | 1.816e+11 |
| qp.schur_border | 3.80% | 2.24% | 1.2e+11 |
| qp.eqp_dependency | 1.92% | 1.66% | 6.052e+10 |
| qp.walk | 1.62% | 3.50% | 5.106e+10 |
| kkt.assembly | 1.43% | 0.00% | 4.518e+10 |
| la.dense_solve | 1.12% | 14.29% | 3.538e+10 |
| la.dense_factorize | 0.83% | 0.01% | 2.635e+10 |
| la.other | 0.78% | 0.00% | 2.47e+10 |
| qp.refine_polish | 0.70% | 2.08% | 2.217e+10 |
| setup.library_init | 0.39% | 17.42% | 1.232e+10 |
| outer.ipm_nlp_core | 0.35% | 0.00% | 1.101e+10 |
| kkt.factor_glue | 0.21% | 0.00% | 6.682e+09 |
| unattributed.unresolved | 0.09% | 0.03% | 2.839e+09 |
| la.matching_scaling | 0.08% | 0.00% | 2.601e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.44% | 33.33% |
| qp | 8.03% | 20.86% |
| kkt | 1.64% | 0.00% |
| setup | 0.39% | 16.03% |
| outer | 0.37% | 0.00% |
| unattributed | 0.09% | 0.00% |
| eval | 0.04% | 0.00% |

### ipm / hs -- all-in
cells: 22; total (instructions): 1.44e+08

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| proc.loader_startup | 63.54% | 61.54% | 9.15e+07 |
| setup.library_init | 27.08% | 30.77% | 3.9e+07 |
| proc.main_other | 2.08% | 0.00% | 3e+06 |
| outer.acceptance_linesearch | 1.39% | 0.00% | 2e+06 |
| unattributed.unresolved | 1.39% | 0.00% | 2e+06 |
| eval.assembly | 0.69% | 0.00% | 1e+06 |
| eval.callback | 0.69% | 0.00% | 1e+06 |
| la.dense_solve | 0.69% | 0.00% | 1e+06 |
| proc.harness_report | 0.69% | 0.00% | 1e+06 |
| proc.problem_load | 0.69% | 0.00% | 1e+06 |
| kkt.factor_glue | 0.35% | 0.00% | 5e+05 |
| la.dense_factorize | 0.35% | 0.00% | 5e+05 |
| outer.ipm_nlp_core | 0.35% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| proc | 67.01% | 69.23% |
| setup | 27.08% | 30.77% |
| outer | 1.74% | 0.00% |
| eval | 1.39% | 0.00% |
| unattributed | 1.39% | 0.00% |
| la | 1.04% | 0.00% |
| kkt | 0.35% | 0.00% |

### ipm / hs -- solver only (proc.* excluded)
cells: 22; total (instructions): 4.75e+07

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| setup.library_init | 82.11% | 80.00% | 3.9e+07 |
| outer.acceptance_linesearch | 4.21% | 0.00% | 2e+06 |
| unattributed.unresolved | 4.21% | 0.00% | 2e+06 |
| eval.assembly | 2.11% | 0.00% | 1e+06 |
| eval.callback | 2.11% | 0.00% | 1e+06 |
| la.dense_solve | 2.11% | 0.00% | 1e+06 |
| kkt.factor_glue | 1.05% | 0.00% | 5e+05 |
| la.dense_factorize | 1.05% | 0.00% | 5e+05 |
| outer.ipm_nlp_core | 1.05% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| setup | 82.11% | 80.00% |
| outer | 5.26% | 0.00% |
| eval | 4.21% | 0.00% |
| unattributed | 4.21% | 0.00% |
| la | 3.16% | 0.00% |
| kkt | 1.05% | 0.00% |

### ipm / cutest -- all-in
cells: 19; total (instructions): 5.08033e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 59.76% | 50.90% | 3.036e+11 |
| la.sparse_solve | 18.34% | 8.37% | 9.319e+10 |
| eval.callback | 17.41% | 13.09% | 8.844e+10 |
| eval.assembly | 1.94% | 2.58% | 9.879e+09 |
| outer.ipm_nlp_core | 0.92% | 1.16% | 4.683e+09 |
| outer.acceptance_linesearch | 0.72% | 0.75% | 3.679e+09 |
| proc.problem_load | 0.38% | 2.46% | 1.937e+09 |
| la.sparse_analyze | 0.21% | 0.55% | 1.086e+09 |
| unattributed.unresolved | 0.12% | 0.12% | 6.147e+08 |
| setup.library_init | 0.09% | 1.44% | 4.814e+08 |
| proc.loader_startup | 0.02% | 0.50% | 1.198e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.37% | 73.25% |
| eval | 19.35% | 16.90% |
| outer | 1.65% | 1.93% |
| proc | 0.41% | 3.16% |
| unattributed | 0.12% | 0.12% |
| setup | 0.10% | 1.45% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |
| kkt | 0.00% | 0.00% |

### ipm / cutest -- solver only (proc.* excluded)
cells: 19; total (instructions): 5.0597e+11

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 60.01% | 54.21% | 3.036e+11 |
| la.sparse_solve | 18.42% | 8.54% | 9.319e+10 |
| eval.callback | 17.48% | 16.43% | 8.844e+10 |
| eval.assembly | 1.95% | 2.60% | 9.879e+09 |
| outer.ipm_nlp_core | 0.93% | 1.28% | 4.683e+09 |
| outer.acceptance_linesearch | 0.73% | 0.83% | 3.679e+09 |
| la.sparse_analyze | 0.21% | 0.56% | 1.086e+09 |
| unattributed.unresolved | 0.12% | 0.12% | 6.147e+08 |
| setup.library_init | 0.10% | 1.46% | 4.814e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 78.69% | 74.72% |
| eval | 19.43% | 18.31% |
| outer | 1.65% | 2.18% |
| unattributed | 0.12% | 0.13% |
| setup | 0.10% | 1.51% |
| instr | 0.00% | 0.00% |
| qp | 0.00% | 0.00% |
| kkt | 0.00% | 0.00% |

### ipm / sd -- all-in
cells: 97; total (instructions): 4.1155e+09

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| eval.callback | 25.28% | 2.33% | 1.04e+09 |
| la.dense_factorize | 24.68% | 0.00% | 1.016e+09 |
| la.sparse_factorize | 18.13% | 0.00% | 7.46e+08 |
| proc.loader_startup | 9.78% | 53.33% | 4.025e+08 |
| la.sparse_solve | 4.63% | 0.00% | 1.905e+08 |
| setup.library_init | 4.31% | 25.00% | 1.772e+08 |
| proc.problem_load | 3.10% | 7.14% | 1.275e+08 |
| eval.assembly | 2.21% | 0.00% | 9.1e+07 |
| outer.ipm_nlp_core | 1.94% | 0.00% | 8e+07 |
| la.dense_solve | 1.92% | 0.00% | 7.891e+07 |
| unattributed.unresolved | 1.09% | 0.00% | 4.5e+07 |
| la.matching_scaling | 1.00% | 0.00% | 4.1e+07 |
| outer.acceptance_linesearch | 1.00% | 0.00% | 4.1e+07 |
| la.sparse_analyze | 0.35% | 0.00% | 1.45e+07 |
| proc.main_other | 0.22% | 0.00% | 9e+06 |
| proc.harness_report | 0.13% | 0.00% | 5.5e+06 |
| la.other | 0.06% | 0.00% | 2.5e+06 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 50.77% | 0.72% |
| eval | 27.49% | 6.25% |
| proc | 13.24% | 60.00% |
| setup | 4.34% | 25.00% |
| outer | 2.95% | 0.00% |
| unattributed | 1.12% | 0.00% |
| instr | 0.04% | 0.00% |
| kkt | 0.04% | 0.00% |
| qp | 0.01% | 0.00% |

### ipm / sd -- solver only (proc.* excluded)
cells: 97; total (instructions): 3.5705e+09

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| eval.callback | 29.14% | 2.67% | 1.04e+09 |
| la.dense_factorize | 28.45% | 0.00% | 1.016e+09 |
| la.sparse_factorize | 20.89% | 0.00% | 7.46e+08 |
| la.sparse_solve | 5.34% | 0.00% | 1.905e+08 |
| setup.library_init | 4.96% | 66.67% | 1.772e+08 |
| eval.assembly | 2.55% | 0.00% | 9.1e+07 |
| outer.ipm_nlp_core | 2.24% | 0.00% | 8e+07 |
| la.dense_solve | 2.21% | 0.00% | 7.891e+07 |
| unattributed.unresolved | 1.26% | 0.00% | 4.5e+07 |
| la.matching_scaling | 1.15% | 0.00% | 4.1e+07 |
| outer.acceptance_linesearch | 1.15% | 0.00% | 4.1e+07 |
| la.sparse_analyze | 0.41% | 0.00% | 1.45e+07 |
| la.other | 0.07% | 0.00% | 2.5e+06 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 58.52% | 0.78% |
| eval | 31.69% | 13.24% |
| setup | 5.00% | 66.67% |
| outer | 3.40% | 0.00% |
| unattributed | 1.29% | 0.00% |
| instr | 0.04% | 0.00% |
| kkt | 0.04% | 0.00% |
| qp | 0.01% | 0.00% |

### sqp / hs -- all-in
cells: 22; total (instructions): 1.87e+08

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| proc.loader_startup | 48.40% | 51.47% | 9.05e+07 |
| setup.library_init | 16.31% | 17.65% | 3.05e+07 |
| la.dense_solve | 6.15% | 2.38% | 1.15e+07 |
| proc.harness_report | 5.61% | 5.88% | 1.05e+07 |
| proc.main_other | 5.61% | 5.88% | 1.05e+07 |
| qp.walk | 4.01% | 0.00% | 7.5e+06 |
| qp.refine_polish | 2.67% | 0.00% | 5e+06 |
| qp.schur_border | 2.67% | 0.00% | 5e+06 |
| unattributed.unresolved | 2.41% | 0.00% | 4.5e+06 |
| qp.eqp_dependency | 1.60% | 0.00% | 3e+06 |
| la.dense_factorize | 1.34% | 0.00% | 2.5e+06 |
| kkt.assembly | 1.07% | 0.00% | 2e+06 |
| outer.sqp_loop | 0.53% | 0.00% | 1e+06 |
| qp.elastic | 0.53% | 0.00% | 1e+06 |
| setup.init_teardown | 0.53% | 0.00% | 1e+06 |
| eval.assembly | 0.27% | 0.00% | 5e+05 |
| kkt.factor_glue | 0.27% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| proc | 59.63% | 63.60% |
| setup | 16.84% | 17.65% |
| qp | 11.50% | 6.90% |
| la | 7.49% | 5.88% |
| unattributed | 2.41% | 0.00% |
| kkt | 1.34% | 0.00% |
| outer | 0.53% | 0.00% |
| eval | 0.27% | 0.00% |

### sqp / hs -- solver only (proc.* excluded)
cells: 22; total (instructions): 7.55e+07

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| setup.library_init | 40.40% | 46.43% | 3.05e+07 |
| la.dense_solve | 15.23% | 5.00% | 1.15e+07 |
| qp.walk | 9.93% | 0.00% | 7.5e+06 |
| qp.refine_polish | 6.62% | 0.00% | 5e+06 |
| qp.schur_border | 6.62% | 0.00% | 5e+06 |
| unattributed.unresolved | 5.96% | 0.00% | 4.5e+06 |
| qp.eqp_dependency | 3.97% | 0.00% | 3e+06 |
| la.dense_factorize | 3.31% | 0.00% | 2.5e+06 |
| kkt.assembly | 2.65% | 0.00% | 2e+06 |
| outer.sqp_loop | 1.32% | 0.00% | 1e+06 |
| qp.elastic | 1.32% | 0.00% | 1e+06 |
| setup.init_teardown | 1.32% | 0.00% | 1e+06 |
| eval.assembly | 0.66% | 0.00% | 5e+05 |
| kkt.factor_glue | 0.66% | 0.00% | 5e+05 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| setup | 41.72% | 46.43% |
| qp | 28.48% | 20.00% |
| la | 18.54% | 14.29% |
| unattributed | 5.96% | 0.00% |
| kkt | 3.31% | 0.00% |
| outer | 1.32% | 0.00% |
| eval | 0.66% | 0.00% |

### sqp / cutest -- all-in
cells: 15; total (instructions): 2.02101e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 62.62% | 66.57% | 1.266e+12 |
| la.sparse_analyze | 17.34% | 15.19% | 3.505e+11 |
| la.sparse_factorize | 7.80% | 7.10% | 1.576e+11 |
| qp.schur_border | 3.03% | 2.30% | 6.13e+10 |
| qp.eqp_dependency | 2.42% | 1.62% | 4.896e+10 |
| qp.walk | 2.11% | 1.70% | 4.269e+10 |
| kkt.assembly | 1.92% | 1.36% | 3.873e+10 |
| la.other | 0.92% | 0.79% | 1.863e+10 |
| qp.refine_polish | 0.50% | 0.68% | 1.014e+10 |
| setup.library_init | 0.30% | 0.36% | 6.11e+09 |
| la.dense_factorize | 0.23% | 0.23% | 4.727e+09 |
| kkt.factor_glue | 0.22% | 0.19% | 4.414e+09 |
| outer.ipm_nlp_core | 0.21% | 0.26% | 4.334e+09 |
| la.matching_scaling | 0.12% | 0.01% | 2.403e+09 |
| unattributed.unresolved | 0.07% | 0.08% | 1.409e+09 |
| la.dense_solve | 0.06% | 0.12% | 1.221e+09 |
| proc.loader_startup | 0.05% | 0.06% | 9.976e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.09% | 88.94% |
| qp | 8.07% | 7.75% |
| kkt | 2.13% | 1.66% |
| setup | 0.30% | 0.37% |
| outer | 0.24% | 0.28% |
| unattributed | 0.07% | 0.08% |
| proc | 0.06% | 0.13% |
| eval | 0.03% | 0.04% |

### sqp / cutest -- solver only (proc.* excluded)
cells: 15; total (instructions): 2.01972e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 62.66% | 66.98% | 1.266e+12 |
| la.sparse_analyze | 17.35% | 15.20% | 3.505e+11 |
| la.sparse_factorize | 7.80% | 7.37% | 1.576e+11 |
| qp.schur_border | 3.04% | 2.31% | 6.13e+10 |
| qp.eqp_dependency | 2.42% | 1.63% | 4.896e+10 |
| qp.walk | 2.11% | 1.75% | 4.269e+10 |
| kkt.assembly | 1.92% | 1.36% | 3.873e+10 |
| la.other | 0.92% | 0.79% | 1.863e+10 |
| qp.refine_polish | 0.50% | 0.68% | 1.014e+10 |
| setup.library_init | 0.30% | 0.36% | 6.11e+09 |
| la.dense_factorize | 0.23% | 0.23% | 4.727e+09 |
| kkt.factor_glue | 0.22% | 0.20% | 4.414e+09 |
| outer.ipm_nlp_core | 0.21% | 0.26% | 4.334e+09 |
| la.matching_scaling | 0.12% | 0.01% | 2.403e+09 |
| unattributed.unresolved | 0.07% | 0.08% | 1.409e+09 |
| la.dense_solve | 0.06% | 0.13% | 1.221e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.15% | 89.05% |
| qp | 8.08% | 7.76% |
| kkt | 2.14% | 1.66% |
| setup | 0.30% | 0.37% |
| outer | 0.24% | 0.28% |
| unattributed | 0.07% | 0.08% |
| eval | 0.03% | 0.04% |

### sqp / sd -- all-in
cells: 97; total (instructions): 1.14078e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 77.91% | 0.00% | 8.888e+11 |
| qp.schur_border | 5.14% | 2.63% | 5.865e+10 |
| la.sparse_analyze | 4.35% | 0.00% | 4.965e+10 |
| la.dense_solve | 2.99% | 10.00% | 3.415e+10 |
| la.sparse_factorize | 2.11% | 0.00% | 2.403e+10 |
| la.dense_factorize | 1.90% | 0.00% | 2.162e+10 |
| qp.refine_polish | 1.05% | 4.32% | 1.203e+10 |
| qp.eqp_dependency | 1.01% | 2.63% | 1.156e+10 |
| qp.walk | 0.73% | 4.17% | 8.36e+09 |
| outer.ipm_nlp_core | 0.58% | 0.00% | 6.672e+09 |
| kkt.assembly | 0.57% | 0.00% | 6.453e+09 |
| setup.library_init | 0.54% | 8.33% | 6.177e+09 |
| la.other | 0.53% | 0.00% | 6.063e+09 |
| kkt.factor_glue | 0.20% | 0.00% | 2.268e+09 |
| proc.loader_startup | 0.14% | 36.36% | 1.58e+09 |
| unattributed.unresolved | 0.12% | 0.00% | 1.425e+09 |
| eval.callback | 0.06% | 0.00% | 7.286e+08 |
| proc.problem_load | 0.01% | 5.00% | 1.204e+08 |
| proc.main_other | 0.00% | 1.25% | 3.35e+07 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.81% | 16.67% |
| qp | 7.94% | 13.64% |
| kkt | 0.76% | 0.00% |
| outer | 0.59% | 0.00% |
| setup | 0.54% | 8.33% |
| proc | 0.15% | 45.45% |
| unattributed | 0.13% | 0.00% |
| eval | 0.07% | 0.02% |

### sqp / sd -- solver only (proc.* excluded)
cells: 97; total (instructions): 1.13903e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 78.03% | 0.00% | 8.888e+11 |
| qp.schur_border | 5.15% | 3.70% | 5.865e+10 |
| la.sparse_analyze | 4.36% | 0.00% | 4.965e+10 |
| la.dense_solve | 3.00% | 20.00% | 3.415e+10 |
| la.sparse_factorize | 2.11% | 0.00% | 2.403e+10 |
| la.dense_factorize | 1.90% | 0.00% | 2.162e+10 |
| qp.refine_polish | 1.06% | 5.71% | 1.203e+10 |
| qp.eqp_dependency | 1.01% | 3.60% | 1.156e+10 |
| qp.walk | 0.73% | 5.88% | 8.36e+09 |
| outer.ipm_nlp_core | 0.59% | 0.00% | 6.672e+09 |
| kkt.assembly | 0.57% | 0.00% | 6.453e+09 |
| setup.library_init | 0.54% | 15.38% | 6.177e+09 |
| la.other | 0.53% | 0.00% | 6.063e+09 |
| kkt.factor_glue | 0.20% | 0.00% | 2.268e+09 |
| unattributed.unresolved | 0.13% | 0.00% | 1.425e+09 |
| eval.callback | 0.06% | 0.00% | 7.286e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 89.95% | 33.33% |
| qp | 7.96% | 25.00% |
| kkt | 0.77% | 0.00% |
| outer | 0.59% | 0.00% |
| setup | 0.54% | 15.38% |
| unattributed | 0.13% | 0.00% |
| eval | 0.07% | 0.02% |

## Truncated (deadline-class) cells: informational shares, totals not asserted

### ipm / truncated -- solver only
cells: 4; total (instructions): 1.05052e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_factorize | 89.84% | 92.72% | 9.438e+11 |
| eval.callback | 2.71% | 2.04% | 2.85e+10 |
| la.sparse_solve | 2.39% | 2.84% | 2.516e+10 |
| la.sparse_analyze | 2.17% | 0.43% | 2.28e+10 |
| outer.ipm_nlp_core | 1.34% | 0.65% | 1.406e+10 |
| eval.assembly | 0.79% | 0.88% | 8.26e+09 |
| setup.library_init | 0.51% | 0.15% | 5.346e+09 |
| unattributed.unresolved | 0.14% | 0.15% | 1.44e+09 |
| outer.acceptance_linesearch | 0.06% | 0.01% | 6e+08 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 94.43% | 95.74% |
| eval | 3.50% | 2.92% |
| outer | 1.40% | 0.65% |
| setup | 0.53% | 0.18% |
| unattributed | 0.14% | 0.15% |

### sqp / truncated -- solver only
cells: 8; total (instructions): 2.04658e+12

| component | corpus share | median per-cell share | total |
|---|---:|---:|---:|
| la.sparse_solve | 50.61% | 50.22% | 1.036e+12 |
| la.sparse_factorize | 20.79% | 8.97% | 4.255e+11 |
| la.sparse_analyze | 12.22% | 10.69% | 2.501e+11 |
| qp.walk | 8.64% | 1.08% | 1.769e+11 |
| qp.eqp_dependency | 1.76% | 0.85% | 3.608e+10 |
| qp.refine_polish | 1.13% | 0.50% | 2.314e+10 |
| qp.schur_border | 1.02% | 0.73% | 2.096e+10 |
| kkt.assembly | 0.99% | 0.92% | 2.022e+10 |
| outer.scaling_basis | 0.73% | 0.00% | 1.488e+10 |
| la.other | 0.44% | 0.41% | 9e+09 |
| eval.callback | 0.34% | 0.04% | 7.02e+09 |
| unattributed.unresolved | 0.33% | 0.11% | 6.84e+09 |
| setup.library_init | 0.26% | 0.21% | 5.235e+09 |
| kkt.factor_glue | 0.15% | 0.13% | 3.08e+09 |
| la.dense_solve | 0.15% | 0.03% | 2.973e+09 |
| eval.assembly | 0.12% | 0.03% | 2.36e+09 |
| la.matching_scaling | 0.10% | 0.01% | 2.14e+09 |
| outer.ipm_nlp_core | 0.08% | 0.04% | 1.7e+09 |
| outer.sqp_loop | 0.07% | 0.00% | 1.42e+09 |

owner categories:

| category | corpus share | median per-cell share |
|---|---:|---:|
| la | 84.36% | 92.13% |
| qp | 12.56% | 4.29% |
| kkt | 1.14% | 1.05% |
| outer | 0.88% | 0.23% |
| eval | 0.46% | 0.10% |
| unattributed | 0.34% | 0.11% |
| setup | 0.26% | 0.21% |

## Overlays (non-additive; share of each cell group's total)

| group | alloc_leaf | iomp_leaf | fortran_leaf | unattributed |
|---|---:|---:|---:|---:|
| suite | 1.69% | 2.97% | 2.22% | 0.10% |
| ipm | 0.39% | 3.67% | 15.67% | 0.13% |
| sqp | 1.90% | 2.86% | 0.04% | 0.09% |
| ipm/hs | 0.35% | 3.82% | 0.00% | 1.39% |
| ipm/cutest | 0.39% | 3.67% | 15.62% | 0.12% |
| ipm/sd | 0.72% | 3.77% | 22.17% | 1.12% |
| sqp/hs | 6.68% | 8.82% | 0.27% | 2.41% |
| sqp/cutest | 1.57% | 1.91% | 0.03% | 0.07% |
| sqp/sd | 2.49% | 4.54% | 0.06% | 0.13% |

## Cells where a component dominates (>= 50% of the cell's solver-only total)

- `eval.callback`: hven-ipm__ACOPP118 (57%), hven-ipm__ACOPP57 (53%), hven-ipm__ACOPR118 (51%), hven-ipm__ACOPP30 (50%), hven-ipm__DECONVC (65%), hven-ipm__HS111LNP (50%), hven-ipm__HS70 (75%), hven-ipm__HS88 (88%), hven-ipm__HS89 (90%), hven-ipm__HS90 (93%), hven-ipm__HS91 (97%), hven-ipm__HS92 (95%), hven-sqp__HS85 (58%), hven-sqp__HS89 (71%), hven-sqp__HS90 (73%), hven-sqp__HS91 (74%), hven-sqp__HS92 (54%)
- `la.dense_factorize`: hven-ipm__READING6 (71%), hven-ipm__DEMBO7 (76%), hven-ipm__HS117 (50%), hven-ipm__HS84 (50%), hven-ipm__HS85 (50%), hven-ipm__HS99EXP (69%), hven-ipm__LEWISPOL (53%), hven-ipm__MESH (85%), hven-ipm__MSS1 (84%), hven-ipm__NET1 (69%), hven-ipm__OPTPRLOC (64%), hven-ipm__SPANHYD (84%)
- `la.dense_solve`: hven-sqp__DECONVC (65%), hven-sqp__DEMBO7 (63%), hven-sqp__DIXCHLNG (50%), hven-sqp__HS100LNP (50%), hven-sqp__HS108 (55%), hven-sqp__HS111LNP (57%), hven-sqp__HS74 (51%), hven-sqp__HS99EXP (67%), hven-sqp__OPTPRLOC (67%), hven-sqp__WATER (64%), hven-sqp__ZECEVIC3 (50%)
- `la.sparse_factorize`: hven-ipm__BIGBANK (58%), hven-ipm__BRITGAS (70%), hven-ipm__MSS2 (97%*), hven-ipm__MSS3 (98%*), hven-ipm__NET3 (54%), hven-ipm__READING8 (89%*), hven-ipm__STEENBRB (79%), hven-ipm__STEENBRC (83%), hven-ipm__STEENBRD (83%), hven-ipm__STEENBRE (83%), hven-ipm__STEENBRG (82%), hven-ipm__TAX213322 (53%*), hven-ipm__TAX53322 (61%), hven-ipm__YORKNET (65%), hven-sqp__MSS3 (67%*)
- `la.sparse_solve`: hven-sqp__ACOPP118 (67%), hven-sqp__ACOPP57 (69%), hven-sqp__ACOPR118 (85%*), hven-sqp__ACOPR57 (71%), hven-sqp__BIGBANK (56%), hven-sqp__BRITGAS (54%*), hven-sqp__DALLASM (87%), hven-sqp__NET3 (57%*), hven-sqp__READING6 (83%), hven-sqp__SAWPATH (87%), hven-sqp__SMBANK (88%), hven-sqp__STEENBRB (61%), hven-sqp__STEENBRC (61%), hven-sqp__STEENBRD (63%), hven-sqp__STEENBRE (64%), hven-sqp__STEENBRG (64%), hven-sqp__TAX53322 (83%*), hven-sqp__YORKNET (61%), hven-sqp__ZAMB2-8 (74%), hven-sqp__ACOPP30 (68%), hven-sqp__CRESC100 (76%), hven-sqp__CRESC50 (71%), hven-sqp__DNIEPER (65%), hven-sqp__LAKES (80%), hven-sqp__MESH (71%), hven-sqp__MSS1 (65%), hven-sqp__NET1 (76%), hven-sqp__SPANHYD (89%)
- `qp.walk`: hven-sqp__TAX213322 (77%*)
- `setup.library_init`: hven-ipm__hs003 (100%), hven-ipm__hs005 (100%), hven-ipm__hs006 (100%), hven-ipm__hs007 (100%), hven-ipm__hs010 (80%), hven-ipm__hs011 (100%), hven-ipm__hs012 (80%), hven-ipm__hs014 (60%), hven-ipm__hs022 (100%), hven-ipm__hs026 (80%), hven-ipm__hs027 (80%), hven-ipm__hs028 (100%), hven-ipm__hs030 (80%), hven-ipm__hs033 (80%), hven-ipm__hs035 (100%), hven-ipm__hs039 (60%), hven-ipm__hs043 (75%), hven-ipm__hs045 (75%), hven-ipm__hs071 (75%), hven-ipm__hs077 (75%), hven-ipm__hs079 (100%), hven-sqp__hs003 (50%), hven-sqp__hs005 (67%), hven-sqp__hs006 (50%), hven-sqp__hs014 (60%), hven-sqp__hs022 (100%), hven-sqp__hs028 (75%), hven-sqp__hs035 (100%), hven-sqp__hs039 (57%), hven-sqp__hs043 (50%), hven-sqp__hs045 (60%), hven-sqp__hs079 (60%), hven-ipm__ALSOTAME (100%), hven-ipm__BT11 (80%), hven-ipm__BT12 (100%), hven-ipm__BT2 (100%), hven-ipm__BT6 (80%), hven-ipm__BT7 (67%), hven-ipm__BT8 (57%), hven-ipm__BURKEHAN (100%), hven-ipm__HIMMELP2 (100%), hven-ipm__HIMMELP3 (80%), hven-ipm__HIMMELP4 (80%), hven-ipm__HIMMELP6 (67%), hven-ipm__HS100LNP (67%), hven-ipm__HS107 (50%), hven-ipm__HS11 (100%), hven-ipm__HS113 (50%), hven-ipm__HS15 (80%), hven-ipm__HS17 (80%), hven-ipm__HS19 (67%), hven-ipm__HS22 (100%), hven-ipm__HS26 (80%), hven-ipm__HS27 (80%), hven-ipm__HS30 (80%), hven-ipm__HS31 (100%), hven-ipm__HS32 (75%), hven-ipm__HS33 (80%), hven-ipm__HS40 (100%), hven-ipm__HS42 (100%), hven-ipm__HS43 (67%), hven-ipm__HS46 (80%), hven-ipm__HS56 (67%), hven-ipm__HS6 (100%), hven-ipm__HS60 (100%), hven-ipm__HS61 (80%), hven-ipm__HS63 (100%), hven-ipm__HS64 (57%), hven-ipm__HS68 (80%), hven-ipm__HS7 (80%), hven-ipm__HS74 (80%), hven-ipm__HS75 (67%), hven-ipm__HS79 (100%), hven-ipm__HS80 (80%), hven-ipm__HS81 (57%), hven-ipm__HS83 (57%), hven-ipm__HS93 (80%), hven-ipm__LOOTSMA (67%), hven-ipm__LSNNODOC (57%), hven-ipm__MATRIX2 (67%), hven-ipm__MWRIGHT (80%), hven-ipm__S365 (67%), hven-ipm__S365MOD (100%), hven-ipm__SYNTHES1 (67%), hven-ipm__TRY-B (80%), hven-ipm__TWOBARS (100%), hven-ipm__ZECEVIC3 (80%), hven-ipm__ZECEVIC4 (100%), hven-ipm__ZY2 (100%), hven-sqp__ALSOTAME (60%), hven-sqp__HS22 (50%), hven-sqp__HS31 (50%), hven-sqp__HS32 (67%), hven-sqp__HS79 (50%), hven-sqp__HS80 (50%), hven-sqp__SYNTHES1 (50%), hven-sqp__ZY2 (50%)
- `unattributed.unresolved`: hven-sqp__hs025 (100%)
(* = truncated cell, share only)

## Top leaf symbols, full cells (corpus; from each cell's top 60)

| leaf symbol | dso | component | share |
|---|---|---|---:|
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | hven_bench_cell | la.sparse_solve | 16.85% |
| `mkl_spblas_lp64_def_dcsr1nsunf__mvout_par` | hven_bench_cell | la.sparse_solve | 10.53% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | hven_bench_cell | la.sparse_solve | 6.62% |
| `mkl_pds_lp64_blkl_ll_real.extracted` | hven_bench_cell | la.sparse_factorize | 3.21% |
| `hven::detail::SchurComplement::append_border(Eigen::Matrix<double, -1, 1, 0, -1, 1> const&` | hven_bench_cell | qp.schur_border | 2.71% |
| `find_norm` | hven_bench_cell | la.sparse_solve | 2.40% |
| `mkl_pds_lp64_do_all_pardiso_fc` | hven_bench_cell | la.sparse_solve | 2.39% |
| `mkl_pds_lp64_dsytrs_bklbw_pardiso` | hven_bench_cell | la.sparse_solve | 2.37% |
| `mkl_blas_def_xdger_kernel` | hven_bench_cell | unattributed.truncated_unwind | 2.28% |
| `mkl_pds_lp64_upd_real_block` | hven_bench_cell | la.sparse_factorize | 2.24% |
| `mkl_pds_lp64_dsytrs_bklfw_noscal_pardiso` | hven_bench_cell | la.sparse_solve | 2.19% |
| `mkl_blas_def_xdgemv_kernel_t` | hven_bench_cell | unattributed.truncated_unwind | 1.85% |
| `mkl_blas_def_dgemm_kernel_zen` | hven_bench_cell | unattributed.truncated_unwind | 1.38% |
| `L_not_one_loopgas_1` | hven_bench_cell | la.dense_solve | 1.31% |
| `mkl_pds_lp64_fct_real_block` | hven_bench_cell | la.sparse_factorize | 1.15% |
| `mkl_blas_def_xdgemv` | hven_bench_cell | la.dense_solve | 1.11% |
| `mkl_blas_dscal` | hven_bench_cell | la.dense_solve | 1.09% |
| `mkl_blas_lp64_dgemv` | hven_bench_cell | la.sparse_solve | 1.06% |
| `void Eigen::internal::generic_product_impl<Eigen::SparseSelfAdjointView<Eigen::SparseMatri` | hven_bench_cell | qp.refine_polish | 1.03% |
| `mkl_pds_lp64_metis_fm_2waynoderefine_onesided` | hven_bench_cell | la.sparse_analyze | 1.03% |
| `mkl_blas_def_xdger` | hven_bench_cell | la.dense_solve | 0.90% |
| `mkl_pds_lp64_dsytrs_bklfw_pardiso` | hven_bench_cell | la.sparse_factorize | 0.84% |
| `mkl_blas_lp64_dscal` | hven_bench_cell | la.sparse_solve | 0.83% |
| `cutest_ccfsg_threadsafe_` | libcutest_double.so | eval.callback | 0.81% |
| `mkl_pds_lp64_metis_fm_2wayedgerefine` | hven_bench_cell | la.sparse_analyze | 0.79% |
| `mkl_blas_def_xdscal` | hven_bench_cell | la.dense_solve | 0.79% |
| `mkl_pds_lp64_dsytf2_pardiso` | hven_bench_cell | la.sparse_factorize | 0.78% |
| `__cutest_double_MOD_cutest_assemble_hessian` | libcutest_double.so | eval.callback | 0.77% |
| `mkl_pds_lp64_metis_createcoarsegraphnomask` | hven_bench_cell | la.sparse_analyze | 0.74% |
| `__kmpc_flush` | libiomp5.so | la.sparse_factorize | 0.73% |
| `_int_malloc` | libc.so.6 | qp.schur_border | 0.71% |
| `hven::detail::assemble_kkt_core(hven::detail::QpProblem const&, hven::detail::WorkingSet c` | hven_bench_cell | kkt.assembly | 0.70% |
| `mkl_pds_lp64_pds_compute_scaled_norm_real` | hven_bench_cell | la.sparse_solve | 0.70% |
| `mkl_blas_def_xdgemv_kernel_n` | hven_bench_cell | unattributed.truncated_unwind | 0.70% |
| `mkl_blas_xdscal` | hven_bench_cell | la.dense_solve | 0.58% |
| `__memset_avx2_unaligned_erms` | libc.so.6 | qp.eqp_dependency | 0.56% |
| `mkl_pds_lp64_metis_mmdelm` | hven_bench_cell | la.sparse_analyze | 0.55% |
| `mkl_pds_lp64_psol_bwscat_pardiso_mic.DIR.OMP.PARALLEL.6` | hven_bench_cell | la.sparse_solve | 0.51% |
| `mkl_pds_lp64_metis_mmdupd` | hven_bench_cell | la.sparse_analyze | 0.50% |
| `mkl_blas_def_dgemm_pst` | hven_bench_cell | la.sparse_factorize | 0.45% |

## Top hven frames, inclusive (full cells; from each cell's top 80)

| frame | inclusive share |
|---|---:|
| `hven::detail::QpEngine::run` | 81.86% |
| `hven::linear::detail::FactorSession::run_phase` | 79.99% |
| `hven::detail::QpEngine::border_candidate` | 69.05% |
| `hven::detail::QpEngine::solve` | 68.16% |
| `hven::linear::SymmetricFactor::solve_single` | 58.58% |
| `hven::linear::` | 58.26% |
| `hven::linear::detail::FactorSession::solve` | 58.10% |
| `hven::detail::SqpSolverImpl::run_major` | 56.69% |
| `hven::detail::solve_vec` | 56.63% |
| `hven::detail::SqpSolverImpl::solve_with_walk` | 51.70% |
| `hven::detail::QpEngine::sync_borders` | 37.66% |
| `hven::detail::SchurComplement::add_border` | 37.16% |
| `hven::detail::QpEngine::eliminated_candidate` | 27.80% |
| `hven::detail::solve_eqp` | 27.78% |
| `hven::detail::factorize_checked` | 16.49% |
| `hven::linear::SymmetricFactor::analyze` | 11.34% |
| `hven::linear::SymmetricFactor::factorize` | 11.19% |
| `hven::detail::IpmSolverImpl::alg_impl` | 11.04% |
| `hven::linear::detail::FactorSession::analyze` | 10.93% |
| `hven::linear::detail::FactorSession::factorize` | 10.82% |
| `hven::detail::IpmSolverImpl::run_phase_sequence` | 10.71% |
| `hven::detail::IpmSolverImpl::solve` | 10.38% |
| `hven::IpmSolver::solve` | 10.34% |
| `hven::detail::QpEngine::probe_inertia` | 9.70% |
| `hven::detail::QpEngine::border_solve_or_fall_back` | 9.70% |
| `hven::detail::SqpSolverImpl::solve_impl_body` | 9.55% |
| `hven::detail::run_elastic_ladder` | 9.20% |
| `hven::detail::` | 8.36% |
| `hven::detail::QpEngine::test_candidate` | 6.43% |
| `hven::detail::QpEngine::repair_temporary_vertex` | 6.21% |
| `hven::detail::IpmSolverImpl::factor_impl` | 6.09% |
| `hven::detail::KktFactorization::refactorize` | 6.09% |
| `hven::detail::SchurComplement::solve` | 5.66% |
| `hven::detail::dependency_test` | 5.41% |
| `hven::detail::solve_bordered_eqp` | 4.89% |
| `hven::detail::polish_eqp_core` | 4.44% |
| `hven::detail::QpEngine::probe_zero_multiplier_drops` | 4.32% |
| `hven::detail::certificate_coefficients` | 2.87% |
| `hven::detail::SchurComplement::append_border` | 2.79% |
| `hven::NlpAssembly::assemble` | 2.72% |
| `hven::detail::NonLinearProgram::assemble_impl` | 2.70% |
| `hven::detail::IpmSolverImpl::eval_nlp` | 2.45% |
| `hven::detail::IpmSolverImpl::assemble_dispatch` | 2.45% |
| `hven::detail::NonLinearProgram::full_kkt_pass` | 2.43% |
| `hven::ConstraintModel<hven::detail::NlpConstraintPiece>::constraints_jacobian_adjointgradient_adjointhessian` | 2.36% |

## Backend drivers: the first hven frame above the linear/KKT glue, by backend component (full cells, corpus)

### ipm / la.sparse_factorize (total 3.044e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::IpmSolverImpl::factor_impl` | 73.3% |
| `hven::detail::IpmSolverImpl::init_impl` | 0.1% |

### ipm / la.sparse_solve (total 9.338e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::IpmSolverImpl::alg_impl` | 79.5% |
| `hven::detail::IpmSolverImpl::init_impl` | 0.1% |

### sqp / la.dense_factorize (total 2.635e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::factor_c` | 51.8% |
| `hven::detail::QpEngine::run` | 23.0% |
| `hven::detail::QpEngine::rebuild_k0` | 3.4% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 1.7% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.2% |
| `hven::detail::QpEngine::border_inertia_verdict` | 0.0% |
| `hven::detail::BorderState::~BorderState` | 0.0% |

### sqp / la.dense_solve (total 3.538e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::solve` | 29.9% |
| `hven::detail::SchurComplement::add_border` | 11.2% |
| `hven::detail::QpEngine::run` | 7.9% |
| `hven::detail::take_latch` | 5.5% |
| `hven::detail::` | 4.5% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.3% |
| `hven::detail::detect_dependency` | 0.3% |
| `hven::detail::face_solve` | 0.1% |

### sqp / la.other (total 2.47e+10)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 29.4% |
| `hven::detail::QpEngine::rebuild_k0` | 0.8% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.3% |
| `hven::detail::SchurComplement::solve` | 0.1% |
| `hven::detail::BorderState::~BorderState` | 0.0% |
| `hven::detail::take_latch` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |
| `hven::detail::SchurComplement::factor_c` | 0.0% |

### sqp / la.sparse_analyze (total 4.001e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 95.4% |
| `hven::detail::QpEngine::rebuild_k0` | 3.8% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.7% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |
| `hven::detail::QpEngine::refine_terminal_mu_free` | 0.0% |

### sqp / la.sparse_factorize (total 1.816e+11)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::QpEngine::run` | 82.3% |
| `hven::detail::QpEngine::rebuild_k0` | 11.9% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.5% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |
| `hven::detail::EqualityBasis::compute` | 0.0% |

### sqp / la.sparse_solve (total 2.154e+12)

| driver frame | share of this backend component |
|---|---:|
| `hven::detail::SchurComplement::add_border` | 57.6% |
| `hven::detail::QpEngine::run` | 17.6% |
| `hven::detail::` | 10.3% |
| `hven::detail::SchurComplement::solve` | 8.7% |
| `hven::detail::detect_dependency` | 0.3% |
| `hven::detail::face_solve` | 0.1% |
| `hven::detail::QpEngine::reread_wrong_inertia` | 0.1% |
| `hven::detail::QpEngine::polish_terminal_point` | 0.0% |

## Non-hven leaf symbols by nearest hven caller (full cells; from each cell's top 80 pairs)

### ipm

| leaf | nearest hven frame | share |
|---|---|---:|
| `mkl_pds_lp64_blkl_ll_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 13.02% |
| `mkl_blas_dgemm_kernel_zen` | `-` | 9.66% |
| `mkl_pds_lp64_upd_real_block` | `hven::linear::detail::FactorSession::run_phase` | 8.83% |
| `mkl_blas_xdger_kernel` | `-` | 8.57% |
| `__cutest_double_MOD_cutest_assemble_hessian` | `hven::detail::CountingNlp::eval_hessian` | 5.53% |
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 5.32% |
| `mkl_pds_lp64_fct_real_block` | `hven::linear::detail::FactorSession::run_phase` | 5.02% |
| `cutest_ccfsg_threadsafe_` | `hven::detail::CountingNlp::eval_jacobian` | 4.31% |
| `mkl_pds_lp64_dsytrs_bklfw_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 4.24% |
| `__kmpc_flush` | `hven::linear::detail::FactorSession::run_phase` | 3.61% |
| `mkl_pds_lp64_dsytf2_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 3.18% |
| `mkl_spblas_dcsr1nsunf__mvout_par` | `hven::linear::detail::FactorSession::run_phase` | 2.32% |
| `__cutest_double_MOD_cutest_form_gradients` | `hven::detail::CountingNlp::eval_hessian` | 2.26% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.24% |

### sqp

| leaf | nearest hven frame | share |
|---|---|---:|
| `mkl_pds_lp64_blkslv_ll_undef_bk_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 18.72% |
| `mkl_spblas_dcsr1nsunf__mvout_par` | `hven::linear::detail::FactorSession::run_phase` | 11.85% |
| `mkl_pds_lp64_dsytrs_bkl_scal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 7.33% |
| `find_norm` | `hven::linear::detail::FactorSession::run_phase` | 2.76% |
| `mkl_pds_lp64_do_all_pardiso_fc` | `hven::linear::detail::FactorSession::run_phase` | 2.76% |
| `mkl_pds_lp64_dsytrs_bklbw_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.61% |
| `mkl_pds_lp64_dsytrs_bklfw_noscal_pardiso` | `hven::linear::detail::FactorSession::run_phase` | 2.38% |
| `mkl_blas_xdgemv_kernel_t` | `-` | 2.12% |
| `mkl_pds_lp64_blkl_ll_real.extracted` | `hven::linear::detail::FactorSession::run_phase` | 1.62% |
| `mkl_blas_xdscal` | `hven::linear::detail::FactorSession::run_phase` | 1.49% |
| `L_not_one_loopgas_1` | `hven::linear::detail::FactorSession::run_phase` | 1.43% |
| `mkl_blas_xdger` | `hven::linear::detail::FactorSession::run_phase` | 1.37% |
| `mkl_blas_xdgemv` | `hven::linear::detail::FactorSession::run_phase` | 1.27% |
| `mkl_blas_xdger_kernel` | `-` | 1.27% |

Raw unattributed.truncated_unwind before redistribution (full cells): 6.79% of all-in total; unresolved after: 0.10%.
