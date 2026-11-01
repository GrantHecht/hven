#!/usr/bin/env bash
# THE LEG BODIES of the M7 close gate.  usage: legs.sh <leg>   (run by run_leg.sh, inside the box lock)
# Every status is CAPTURED and folded; a trailing tail/grep/echo cannot mask a failure.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
LEG="$1"; mkdir -p "$LOGS"; cd "$R" || exit 90

reference_line_once() { # configure-log -> requires exactly one REFERENCE line and no OFF-REFERENCE
  local n o; n=$(grep -c -- '-> REFERENCE (route pins asserted)' "$1"); o=$(grep -c -- 'OFF-REFERENCE' "$1")
  echo "REFERENCE_LINES=$n OFF_REFERENCE_LINES=$o"; grep -- 'hven tests: MKL dispatch' "$1"
  [ "$n" -eq 1 ] && [ "$o" -eq 0 ]
}
build_tree() { # <tag> <conf-array-name>
  local tag="$1"; local -n conf="$2"; local d="$F/build-$tag" worst=0 rc
  rm -rf "$d"
  cmake "${conf[@]}" > "$LOGS/configure-$tag.log" 2>&1; rc=$?; echo "CONFIGURE_RC_$tag=$rc"; worst=$((worst|rc))
  [ $rc -eq 0 ] || return $worst
  reference_line_once "$LOGS/configure-$tag.log"; rc=$?; echo "REFERENCE_CHECK_RC_$tag=$rc"; worst=$((worst|rc))
  grep -E 'EIGEN_MAX_ALIGN_BYTES resolved to' "$LOGS/configure-$tag.log"
  cmake --build "$d" -j "$(nproc)" > "$LOGS/build-$tag.log" 2>&1; rc=$?; echo "BUILD_RC_$tag=$rc"; worst=$((worst|rc))
  echo "error:/FAILED lines in build-$tag.log: $(grep -cE 'error:|FAILED' "$LOGS/build-$tag.log")"
  return $worst
}
ctest_leg() { # <tag> <extra ctest args...>   ctest then the -N inventory; both folded
  local tag="$1"; shift; local d="$F/build-$tag" worst=0 rc
  ctest --test-dir "$d" -j4 --no-tests=error "$@" > "$LOGS/suite-$tag.out" 2>&1; rc=$?
  echo "CTEST_RC_$tag=$rc"; worst=$((worst|rc)); tail -8 "$LOGS/suite-$tag.out" | cut -c1-200
  ctest --test-dir "$d" -N > "$LOGS/inventory-$tag.out" 2>&1; rc=$?; echo "INVENTORY_RC_$tag=$rc"; worst=$((worst|rc))
  return $worst
}

case "$LEG" in
 build-release) build_tree release CONF_REL; rc=$?; echo "LEG_BODY_RC=$rc"; exit $rc;;
 build-debug)   build_tree debug CONF_DBG;   rc=$?; echo "LEG_BODY_RC=$rc"; exit $rc;;
 suite-release) ctest_leg release --output-on-failure; exit $?;;
 suite-debug)   ctest_leg debug -E "^${LONG_TEST}\$" --output-on-failure; exit $?;;
 long-debug)
   ctest --test-dir "$F/build-debug" --no-tests=error -R "^${LONG_TEST}\$" --output-on-failure > "$LOGS/suite-debug-long.out" 2>&1
   rc=$?; echo "CTEST_RC_long=$rc"; tail -6 "$LOGS/suite-debug-long.out" | cut -c1-200; exit $rc;;
 smoke)
   S="$F/smokework"; rm -rf "$S"; mkdir -p "$S"; worst=0
   HVEN_INSTALL_SMOKE_WORKDIR="$S" HVEN_INSTALL_SMOKE_JOBS="$(nproc)" bash scripts/check_install_smoke.sh > "$LOGS/install-smoke.out" 2>&1
   rc=$?; echo "SMOKE_RC=$rc"; worst=$((worst|rc)); tail -6 "$LOGS/install-smoke.out" | cut -c1-200
   echo "standalone-include TUs named: $(grep -cE 'include_[a-z_]+\.cpp' "$LOGS/install-smoke.out")"
   ALIGN=$(grep -m1 -oE 'EIGEN_MAX_ALIGN_BYTES resolved to [0-9]+' "$S/hven-configure.log" 2>/dev/null | grep -oE '[0-9]+$')
   TARGETS=$(find "$S/hven-install" -name hvenTargets.cmake 2>/dev/null | head -1)
   echo "align from the smoke's configure log: ${ALIGN:-<NOT FOUND>}"; echo "targets: ${TARGETS:-<NOT FOUND>}"
   if [ -z "$ALIGN" ] || [ -z "$TARGETS" ]; then echo "EXPORT_RC=3 (inputs not found)"; worst=$((worst|3))
   else bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$LOGS/export-contract.out" 2>&1; rc=$?
        echo "EXPORT_RC=$rc"; worst=$((worst|rc)); tail -4 "$LOGS/export-contract.out"; fi
   cp "$S/smoke.out" "$LOGS/smoke-workdir.out" 2>/dev/null; cp "$S/hven-configure.log" "$LOGS/smoke-configure.log" 2>/dev/null
   exit $worst;;
 rig-build)
   worst=0; d="$F/build-3seam"; rm -rf "$d"
   cmake --preset linux-clang-release -B "$d" -DHVEN_BUILD_TESTS=ON -DHVEN_RIG_PSIOPT_SEAM=/home/ghecht/Projects/tycho \
         -DHVEN_RIG_SQP_SEAM=/home/ghecht/Projects/tycho_sqp > "$LOGS/configure-3seam.log" 2>&1
   rc=$?; echo "CONFIGURE_RC_3seam=$rc"; worst=$((worst|rc)); grep -iE 'golden rig' "$LOGS/configure-3seam.log" | cut -c1-260
   [ $rc -eq 0 ] || exit $worst
   cmake --build "$d" -j "$(nproc)" > "$LOGS/build-3seam.log" 2>&1; rc=$?; echo "BUILD_RC_3seam=$rc"; worst=$((worst|rc))
   echo "error:/FAILED lines: $(grep -cE 'error:|FAILED' "$LOGS/build-3seam.log")"; exit $worst;;
 rig-run)
   # ctest's status is CAPTURED, NOT folded: the rig's EXPECTED result is exactly one designed failure.  The
   # assertion that the failure is that one entry (and that ctest ran at all) is close_check.py's, after the leg.
   d="$F/build-3seam"; worst=0
   ctest --test-dir "$d" -N > "$LOGS/rig-inventory-full.out" 2>&1; rc=$?; worst=$((worst|rc)); echo "INVENTORY_RC=$rc"
   ctest --test-dir "$d" -N -R 'Arms/|GoldenRig|FailByDesignControl' > "$LOGS/rig-inventory-scoped.out" 2>&1; rc=$?; worst=$((worst|rc))
   ctest --test-dir "$d" -j4 -R 'Arms/|GoldenRig|FailByDesignControl' --output-on-failure > "$LOGS/rig-ctest-scoped.out" 2>&1
   echo "RIG_SCOPED_CTEST_RC=$? (captured, not folded)"
   ctest --test-dir "$d" -j4 --output-on-failure > "$LOGS/rig-ctest-full.out" 2>&1
   echo "RIG_FULL_CTEST_RC=$? (captured, not folded)"
   for f in rig-ctest-scoped rig-ctest-full; do grep -qE 'tests passed,' "$LOGS/$f.out" || { echo "WRAPPER FAILURE: no ctest summary in $f"; worst=$((worst|4)); }; done
   exit $worst;;
 replay)
   BIN="$F/build-release/bench/hven_sqp_corpus"; D="$F/replay"; mkdir -p "$D"; worst=0
   CELLS="$(grep -v '^#' "$CELLS_FILE" | tr -d '\n')"
   for eng in walk ssn ipm; do
     taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" > "$LOGS/replay-$eng.out" 2>&1
     rc=$?; echo "ran head-$eng rc=$rc"; worst=$((worst|rc)); [ $rc -eq 0 ] || break
   done
   exit $worst;;
 census)
   BIN="$F/build-release/bench/hven_sqp_corpus"; OUT="$F/census"; worst=0
   # the runner's own stamp preflight, pointed at a real file (the M6 erratum): the binary must stamp CLEAN
   "$BIN" --from-csv "$CENSUS_BASELINE" --csv "$LOGS/census-stamp.csv" > "$LOGS/census-stamp.out" 2>&1; rc=$?; worst=$((worst|rc))
   grep -E '^# (binary|schema|budget_table_hash):' "$LOGS/census-stamp.csv"
   grep -qE '^# binary: [0-9a-f]' "$LOGS/census-stamp.csv" || { echo "FATAL: no stamp"; exit 2; }
   grep -qE '^# binary:.*-dirty' "$LOGS/census-stamp.csv" && { echo "FATAL: stamp is -dirty"; exit 2; }
   rm -rf "$OUT"
   bash scripts/run_walk_census.sh --binary "$BIN" --out "$OUT" --compare "$R/scripts/census_compare.py" > "$LOGS/census-run.out" 2>&1
   rc=$?; echo "CENSUS_RUN_RC=$rc"; worst=$((worst|rc)); tail -12 "$LOGS/census-run.out" | cut -c1-220
   exit $worst;;
 *) echo "unknown leg $LEG"; exit 64;;
esac
