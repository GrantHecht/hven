#!/usr/bin/env bash
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-f1, this directory); was: AP-2 B8 riders (copied from B7 riders, re-pointed) (B5's instrument): the install smoke on both front ends (default/GCC and
# clang++), each followed by scripts/check_export_contract.sh on its install tree.
# usage: install_smoke.sh <evidence-dir>     Run as: flock /tmp/box-build.lock bash install_smoke.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$EV"
worst=0
for ARM in default clang; do
  W=$WT/build-r11e/smoke-$ARM
  rm -rf "$W"; mkdir -p "$W"
  echo "SMOKE_START $ARM $(date -Is)"
  if [ "$ARM" = clang ]; then
    HVEN_INSTALL_SMOKE_WORKDIR=$W HVEN_INSTALL_SMOKE_JOBS=16 bash scripts/check_install_smoke.sh -DCMAKE_CXX_COMPILER=clang++ > "$EV/smoke-$ARM.log" 2>&1
  else
    HVEN_INSTALL_SMOKE_WORKDIR=$W HVEN_INSTALL_SMOKE_JOBS=16 bash scripts/check_install_smoke.sh > "$EV/smoke-$ARM.log" 2>&1
  fi
  rc=$?; tail -4 "$EV/smoke-$ARM.log"; echo "smoke $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  TARGETS=$(find "$W" -name hvenTargets.cmake -path '*hven-install*' | head -1)
  ALIGN=$(grep -oE 'resolved to [0-9]+' "$W/hven-configure.log" | head -1 | grep -oE '[0-9]+')
  bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$EV/export-contract-$ARM.log" 2>&1
  rc=$?; cat "$EV/export-contract-$ARM.log"; echo "export-contract $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
echo "smoke worst rc=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
