# Task 3 proof legs — raw artifacts

Protocol scripts and every raw run behind the Task 3 report. Base arm
`f07184b`, head arm the consumption switch as completed (the solver, the
globalization components, and the overlap ruling's landing). Fedora, AMD Ryzen
7 5800X3D 8C/16T, 31 GiB, Linux 7.1.5-201.fc44, `/usr/bin/clang++` 22.1.8, MKL
LP64, Release, `HVEN_FP_MODE=SAFER_FAST`.

| file | leg |
|---|---|
| `trace_leg.sh`, `ab_probe.cpp`, `build_probe.sh`, `trace_base.txt` | solve-trace A/B: full IPM print for HS071 + Rosenbrock, wall-clock lines dropped and ANSI stripped, compared by md5 |
| `wall_leg.sh`, `wall/` | standing bench wall leg (`hven_sqp_bench`, F3/F7), carve protocol, 30 per-run CSVs |
| `corpus_leg.sh`, `corpus/` | 17-cell `--engine ssn` corpus leg, 10 per-run CSVs, 36 asserted counter columns |

The standing bench and the corpus drive `SqpDriver`, which does not reach
`InteriorPointSolver`; both are neutrality checks for this diff. The leg that
observes it is the standing IPM wall leg, which lives at
`docs/notes/data/m4-ipm-wall-leg/` because it is now a per-task instrument for
the rest of the milestone rather than a Task 3 artifact; this task's run is
`runs/2026-08-21-task3.log` there.
