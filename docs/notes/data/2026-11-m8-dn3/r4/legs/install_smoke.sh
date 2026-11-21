#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, F3): ../../legs/install_smoke.sh re-pointed (worktree agent-a739270f237b3e30c,
# build-flip/), TMPDIR on the home volume. The install smoke and export contract on both front ends.
# usage: install_smoke.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash install_smoke.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
cd "$WT" || exit 1
mkdir -p "$EV"
worst=0
for ARM in default clang; do
  W=$WT/build-flip/smoke-$ARM
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
