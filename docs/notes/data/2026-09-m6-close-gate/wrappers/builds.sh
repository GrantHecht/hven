#!/usr/bin/env bash
# THE TWO BUILD TREES, from EMPTY, at the frozen head.  Configure then build,
# Release then Debug, every status OR-FOLDED into `worst`.  One lock hold for
# all four steps (the caller wraps this in run_leg.sh).
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
worst=0
step() { echo; echo "########## $* ##########"; }
step "configure Release -> $F/build-release"
bash "$F/configure_one.sh" "$F/build-release" Release; rc=$?; worst=$((worst | rc))
echo "worst after configure-release: $worst"
[ $rc -eq 0 ] || { echo "BUILDS_WORST_RC=$worst"; exit $worst; }
step "build Release (full, -j 8)"
bash "$F/build_one.sh" "$F/build-release" release; rc=$?; worst=$((worst | rc))
echo "worst after build-release: $worst"
[ $rc -eq 0 ] || { echo "BUILDS_WORST_RC=$worst"; exit $worst; }
step "configure Debug -> $F/build-debug"
bash "$F/configure_one.sh" "$F/build-debug" Debug; rc=$?; worst=$((worst | rc))
echo "worst after configure-debug: $worst"
[ $rc -eq 0 ] || { echo "BUILDS_WORST_RC=$worst"; exit $worst; }
step "build Debug (full, -j 8)"
bash "$F/build_one.sh" "$F/build-debug" debug; rc=$?; worst=$((worst | rc))
echo "worst after build-debug: $worst"
step "the corpus binary's stamp (both trees)"
for t in release debug; do
  b="$F/build-$t/bench/hven_sqp_corpus"
  if [ -x "$b" ]; then
    MKL_NUM_THREADS=1 "$b" --from-csv "$R/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv" \
        --csv /dev/null 2>&1 | grep -E '^# (binary|schema|budget_table_hash):' | sed "s/^/[$t] /"
  else
    echo "[$t] corpus binary MISSING"; worst=$((worst | 3))
  fi
done
echo "BUILDS_WORST_RC=$worst"
exit $worst
