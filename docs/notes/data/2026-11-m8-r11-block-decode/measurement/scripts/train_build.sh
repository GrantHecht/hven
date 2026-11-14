#!/usr/bin/env bash
# R11 scratch: hven-bench clone (build-r11/hven-bench-meas, branch r11-meas, pin a8c540cf7) -> pin build, then the
# probe-v2 files dropped into the pin source and re-installed into the same prefix; then the harness build.
set -o pipefail
B=/home/ghecht/Projects/hven/.claude/worktrees/r11/build-r11
H=$B/hven-bench-meas
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$H" || exit 1
bash scripts/build_hven_pin.sh > "$B/train-pin.log" 2>&1 || { echo PIN_BUILD_FAIL; tail -20 "$B/train-pin.log"; exit 1; }
tail -3 "$B/train-pin.log"
for f in src/linear/dense_symmetric_factor.cpp include/hven/detail/kkt/schur_complement.h src/qp/qp_engine.cpp include/hven/linear/r11_probe.h; do
  cp "$B/probe-v2/$f" "$H/.hven/src/$f" || exit 1
done
cmake --build "$H/.hven/build-a8c540cf7868" --parallel 16 > "$B/train-probe-build.log" 2>&1 || { echo PROBE_BUILD_FAIL; tail -20 "$B/train-probe-build.log"; exit 1; }
cmake --install "$H/.hven/build-a8c540cf7868" --prefix "$H/.hven/install-a8c540cf7868" > "$B/train-probe-install.log" 2>&1 || { echo PROBE_INSTALL_FAIL; exit 1; }
cat "$H/.hven/install-a8c540cf7868/HVEN_PIN.txt"
grep -c r11 "$H/.hven/install-a8c540cf7868/include/hven/linear/r11_probe.h"
bash configure.sh > "$B/train-configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$B/train-configure.log"; exit 1; }
cmake --build build --parallel 16 > "$B/train-harness-build.log" 2>&1 || { echo HARNESS_BUILD_FAIL; tail -20 "$B/train-harness-build.log"; exit 1; }
nm -C build/lanes/hven/hven_bench_cell 2>/dev/null | grep -c "r11" || find build -name hven_bench_cell -type f
echo "FOREGROUND_END $(date -Is)"
