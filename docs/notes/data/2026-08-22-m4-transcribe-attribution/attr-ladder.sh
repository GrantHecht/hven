#!/bin/bash
# M4 attribution ladder: tycho bench cells vs hven commit. One leg per argument.
# Usage: attr-ladder.sh <hven-commit> [<hven-commit> ...]
# Pre-Task-6 hven heads build against tycho cec58e8a (two-arg API); 3f04c0b and
# later against 22a91465 (one-arg API). Runs SOLO: refuses to start a leg while
# any ninja/clang/ctest/bench process is alive.
set -u
WT=/home/ghecht/Projects/tycho/.scratch/attr-wt
OUT=/home/ghecht/Projects/tycho/.scratch/attr-results; mkdir -p "$OUT"
FILTER='BM_Phase_Transcribe|BM_Phase_Construct|BM_InteriorPointSolver_'
wait_idle() { while ps -eo comm | grep -qE '^(ninja|clang|clang\+\+|ctest|bench_all|bench_base|bench_head|ipm_base|ipm_head|probe-.*)$'; do sleep 60; done; }
for C in "$@"; do
  cd "$WT" || exit 1
  case "$C" in 72baafd*) TY=cec58e8a;; 3f04c0b*|d96a51f*|eb3a73d*|d71b945*|46d6ff8*|7f72857*|6b5fc12*) TY=22a91465;; *) TY=attr-mid;; esac
  git checkout -q --detach "$TY" || exit 1
  git -C dep/hven checkout -q "$C" || exit 1
  echo "=== leg hven=$C tycho=$TY $(date -Is)" | tee -a "$OUT/ladder.log"
  wait_idle
  ( source "$HOME/miniconda3/etc/profile.d/conda.sh" 2>/dev/null || source "$HOME/miniforge3/etc/profile.d/conda.sh"; conda activate tycho
    cmake --preset linux-clang-conda -DBUILD_CPP_BENCHMARKS=ON -DBUILD_CPP_TESTS=OFF -DTYCHO_HEAVY_COMPILE_JOBS=2 > "$OUT/configure-$C.log" 2>&1 || { echo "configure FAILED $C" | tee -a "$OUT/ladder.log"; exit 2; }
    wait_idle
    flock -w 14400 /tmp/box-build.lock ninja -C build -j6 bench_all > "$OUT/build-$C.log" 2>&1 || { echo "build FAILED $C" | tee -a "$OUT/ladder.log"; exit 3; }
  ) || continue
  python3 /home/ghecht/Projects/tycho/.scratch/probe/build_probe.py "$WT/build" "$OUT/probe-$C" >> "$OUT/ladder.log" 2>&1 || echo "probe build FAILED $C" | tee -a "$OUT/ladder.log"
  wait_idle
  { echo "box: load=$(cut -d' ' -f1-3 /proc/loadavg) foreign=$(ps -eo comm --sort=-pcpu | grep -vE '^(ps|bash|grep|bench_all|COMMAND)$' | head -3 | tr '\n' ',')"; } | tee -a "$OUT/ladder.log"
  cp build/bench/cpp/bench_all "$OUT/bench_all-$C"; flock -w 14400 /tmp/box-build.lock ./build/bench/cpp/bench_all --benchmark_filter="$FILTER" --benchmark_repetitions=5 \
     --benchmark_report_aggregates_only=true --benchmark_out="$OUT/bench-$C.json" --benchmark_out_format=json \
     > "$OUT/bench-$C.txt" 2>&1
  echo "leg done hven=$C rc=$? $(date -Is)" | tee -a "$OUT/ladder.log"
done
