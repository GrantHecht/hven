#!/usr/bin/env bash
# WX-R1 gates: legs runner, copied from docs/notes/data/2026-10-m7-toolchain-2026-10-27/wrappers/legs.sh
# (same G1-G4/E1 shapes; build dirs under .scratch/wxr1; record under this directory).
# usage: legs.sh ROOT LEG   (each LEG is invoked by run.sh under flock /tmp/box-build.lock)
set -o pipefail
ROOT=$1; LEG=$2
S=$ROOT/.scratch/wxr1
RD=$ROOT/docs/notes/data/2026-10-m7-wx-r1
L=$RD/logs
TC=$ROOT/docs/notes/data/2026-10-m7-toolchain-2026-10-27
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$ROOT" || exit 90
mkdir -p "$S" "$L"
case $LEG in
 g1)  rm -rf "$S/dbg"
      cmake -S "$ROOT" -B "$S/dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$L/configure-debug.log" 2>&1 || exit 91
      cmake --build "$S/dbg" -j "$(nproc)" > "$L/build-debug.log" 2>&1 || exit 91
      setsid ctest --test-dir "$S/dbg" -j4 --no-tests=error -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$L/suite-debug.log" 2>&1
      rc=$?; tail -6 "$L/suite-debug.log"; exit $rc;;
 g2)  setsid ctest --test-dir "$S/dbg" --no-tests=error -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$L/suite-debug-long.log" 2>&1
      rc=$?; tail -8 "$L/suite-debug-long.log"; exit $rc;;
 g3)  rm -rf "$S/rel"
      cmake -S "$ROOT" -B "$S/rel" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$L/configure-release.log" 2>&1 || exit 91
      cmake --build "$S/rel" -j "$(nproc)" > "$L/build-release.log" 2>&1 || exit 91
      setsid ctest --test-dir "$S/rel" -j4 --no-tests=error --output-on-failure > "$L/suite-release.log" 2>&1
      rc=$?; tail -6 "$L/suite-release.log"; exit $rc;;
 g4)  BIN=$S/rel/bench/hven_sqp_corpus; D=$RD/replay; mkdir -p "$D"
      CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
      for eng in walk ssn ipm; do
        taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$L/run-$eng.log" 2>&1
        echo "ran head-$eng rc=$?"
      done
      BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
      : > "$D/comparison-vs-m6-pin.txt"; : > "$D/exact-vs-toolchain-record.txt"
      for eng in walk ssn ipm; do
        python3 -B scripts/compare_replay.py --residual-gate 1e-5 "$BASE/head-$eng.csv" "$D/head-$eng.csv" "M6 CLOSE HEAD-$eng vs WX-R1 HEAD-$eng (GATED 1e-5)" | tee -a "$D/comparison-vs-m6-pin.txt"; echo "  CMP_RC_$eng=${PIPESTATUS[0]}"
        python3 -B scripts/compare_replay.py "$TC/replay/head-$eng.csv" "$D/head-$eng.csv" "TOOLCHAIN-2026-10-27 RECORD HEAD-$eng vs WX-R1 HEAD-$eng (EXACT)" | tee -a "$D/exact-vs-toolchain-record.txt"; echo "  EXACT_RC_$eng=${PIPESTATUS[0]}"
      done; exit 0;;
 e1)  mkdir -p "$RD/e1"
      taskset -c 2 "$S/rel/bench/hven_sqp_ipqp_e1_arm" --gate --out "$RD/e1/e1-off.csv" > "$L/e1-off.log" 2>&1; echo "e1 rc=$?"
      python3 -B docs/notes/data/2026-10-m7-a1-impl/scripts/i2_compare.py repro "$TC/e1/e1-off.csv" "$RD/e1/e1-off.csv" | tee "$RD/e1/repro-vs-toolchain-e1-off.txt"
      python3 -B docs/notes/data/2026-10-m7-a1-impl/scripts/i2_compare.py repro docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv "$RD/e1/e1-off.csv" | tee "$RD/e1/repro-vs-a4-gate-mu1e-2.txt"; exit 0;;
esac
