#!/usr/bin/env bash
# restore the touched files from the pristine archive / the real harness, then instrument (keeps the build dirs incremental)
set -euo pipefail
R=/home/ghecht/Projects/hven/.scratch/m7w2r1
for f in src/qp/qp_engine.cpp include/hven/detail/globalization/sqp/restoration.h src/globalization/sqp/soc_elastic_restoration.cpp src/drivers/sqp_solver.cpp; do cp $R/pristine/$f $R/tree/$f; done
cp /home/ghecht/Projects/hven-bench/lanes/hven/hven_bench_cell.cpp $R/bench/lanes/hven/hven_bench_cell.cpp
python3 $R/tools/instrument_r1.py $R/tree $R/bench
python3 $R/tools/instrument_d4_r1.py $R/tree
