#!/usr/bin/env bash
# Toolchain re-baseline 2026-10-27: legs runner (G1-G6 shaped after docs/notes/data/2026-10-m7-a2-gates/wrappers-gates/gates-a2.sh segments).
# usage: legs.sh ROOT LEG   (each LEG is invoked by run.sh under flock /tmp/box-build.lock)
set -o pipefail
ROOT=$1; LEG=$2
S=$ROOT/.scratch/tcb
RD=$ROOT/docs/notes/data/2026-10-m7-toolchain-2026-10-27
L=$RD/logs
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$ROOT" || exit 90
case $LEG in
 g1)  rm -rf "$S/dbg"
      cmake -S "$ROOT" -B "$S/dbg" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON > "$L/configure-debug.log" 2>&1 || exit 91
      cmake --build "$S/dbg" -j "$(nproc)" > "$L/build-debug.log" 2>&1 || exit 91
      ctest --test-dir "$S/dbg" -j4 --no-tests=error -E "Continuation.ProbeBudgetBoundsAFailingProposal" --output-on-failure > "$L/suite-debug.log" 2>&1
      rc=$?; tail -6 "$L/suite-debug.log"; exit $rc;;
 g2)  ctest --test-dir "$S/dbg" --no-tests=error -R "^Continuation.ProbeBudgetBoundsAFailingProposal$" --output-on-failure > "$L/suite-debug-long.log" 2>&1
      rc=$?; tail -8 "$L/suite-debug-long.log"; exit $rc;;
 g3)  rm -rf "$S/rel"
      cmake -S "$ROOT" -B "$S/rel" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$L/configure-release.log" 2>&1 || exit 91
      cmake --build "$S/rel" -j "$(nproc)" > "$L/build-release.log" 2>&1 || exit 91
      ctest --test-dir "$S/rel" -j4 --no-tests=error --output-on-failure > "$L/suite-release.log" 2>&1
      rc=$?; tail -6 "$L/suite-release.log"; exit $rc;;
 g4)  BIN=$S/rel/bench/hven_sqp_corpus; D=$RD/replay; mkdir -p "$D"
      CELLS="$(grep -v '^#' "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
      for eng in walk ssn ipm; do
        taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$L/run-$eng.log" 2>&1
        echo "ran head-$eng rc=$?"
      done
      BASE=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay; PREV=$ROOT/docs/notes/data/2026-10-m7-a2-gates/gates/replay
      : > "$D/comparison-vs-m6-pin.txt"; : > "$D/exact-vs-a2-gates-record.txt"
      for eng in walk ssn ipm; do
        python3 -B scripts/compare_replay.py --residual-gate 1e-5 "$BASE/head-$eng.csv" "$D/head-$eng.csv" "M6 CLOSE HEAD-$eng vs TOOLCHAIN-2026-10-27 HEAD-$eng (GATED 1e-5)" | tee -a "$D/comparison-vs-m6-pin.txt"; echo "  CMP_RC_$eng=${PIPESTATUS[0]}"
        python3 -B scripts/compare_replay.py "$PREV/head-$eng.csv" "$D/head-$eng.csv" "A2 GATES RECORD HEAD-$eng vs TOOLCHAIN-2026-10-27 HEAD-$eng (EXACT)" | tee -a "$D/exact-vs-a2-gates-record.txt"; echo "  EXACT_RC_$eng=${PIPESTATUS[0]}"
      done; exit 0;;
 e1)  mkdir -p "$RD/e1"
      taskset -c 2 "$S/rel/bench/hven_sqp_ipqp_e1_arm" --gate --out "$RD/e1/e1-off.csv" > "$L/e1-off.log" 2>&1; echo "e1 rc=$?"
      python3 -B docs/notes/data/2026-10-m7-a1-impl/scripts/i2_compare.py repro docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv "$RD/e1/e1-off.csv" | tee "$RD/e1/repro-vs-a4-gate-mu1e-2.txt"; exit 0;;
 g6)  rm -rf "$S/smoke-work"
      HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-work" bash scripts/check_install_smoke.sh > "$L/install-smoke.log" 2>&1; echo "INSTALL_SMOKE_RC=$?"; tail -4 "$L/install-smoke.log"
      ALIGN=$(grep -o "EIGEN_MAX_ALIGN_BYTES resolved to [0-9]*" "$S/smoke-work/hven-configure.log" | tail -1 | awk '{print $NF}'); echo "ALIGN=$ALIGN"
      bash scripts/check_export_contract.sh "$S/smoke-work/hven-install/lib64/cmake/hven/hvenTargets.cmake" linux "$ALIGN" > "$L/export-contract.log" 2>&1; echo "EXPORT_CONTRACT_RC=$?"; tail -3 "$L/export-contract.log"
      clang-format --version
      git ls-files 'include/*.h' 'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' 'bench/*.cpp' 'bench/*.h' | xargs clang-format --dry-run -Werror > "$L/clang-format.log" 2>&1; echo "CLANG_FORMAT_ALL_RC=$?"
      git diff --check; echo "DIFF_CHECK_RC=$?"; exit 0;;
esac
