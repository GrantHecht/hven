# M8 AP-5 G4 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g2/ and re-pointed in code lines only (this worktree, build-g4/, this directory); older provenance lines below are historical.
# M8 AP-5 G2 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g1/ (legs-r3 / runtime-r3) and re-pointed in code lines only (worktree agent-a41fdd4ad89efc301, build-g2/, this directory); replay.sh, interior.sh and chain.sh are changed as the declaration says; older provenance lines below are historical.
#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt P5): copied from ../../2026-11-m8-ap2-b8-riders/legs/install_smoke.sh and re-pointed in code lines only (worktree, build-g2/); TMPDIR set to the git-ignored build-g2/tmp; older provenance lines below are historical.
# AP-2 B8 riders (copied from B7 riders, re-pointed) (B5's instrument): the install smoke on both front ends (default/GCC and
# clang++), each followed by scripts/check_export_contract.sh on its install tree.
# usage: install_smoke.sh <evidence-dir>     Run as: flock /tmp/box-build.lock bash install_smoke.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-g4/tmp
mkdir -p "$TMPDIR"
cd "$WT" || exit 1
mkdir -p "$EV"
worst=0
for ARM in default clang; do
  W=$WT/build-g4/smoke-$ARM
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
