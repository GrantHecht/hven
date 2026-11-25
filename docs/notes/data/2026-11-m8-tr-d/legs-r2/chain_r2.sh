#!/usr/bin/env bash
# TR-D rider r2 (../DECLARATION-r2.md). Run as: flock /tmp/box-build.lock bash chain_r2.sh <code sha>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-d
D=$WT/docs/notes/data/2026-11-m8-tr-d
L=$D/legs
O=$D/r2
R=$WT/.scratch
HEAD_SHA=${1:?code sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
export GIT_CEILING_DIRECTORIES=$WT MKL_NUM_THREADS=1
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo SOURCE_MOVED; exit 2; }
mkdir -p "$O" "$R/r2"
step() { local n=$1; shift; "$@"; local rc=$?; echo "STEP $n rc=$rc $(date -Is)"; [ $rc -ne 0 ] && { echo "STOPPED_AT $n"; exit $rc; }; }
step build-rel bash -c "bash $L/build.sh Release rel > $R/r2/build-rel.log 2>&1"
step build-dbg bash -c "bash $L/build.sh Debug dbg > $R/r2/build-dbg.log 2>&1"
cp -p "$R/build-trd/rel/bench/hven_sqp_corpus" "$R/build-trd/armBbin/hven_sqp_corpus"
ARMA=$(sha256sum "$R/build-trd/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
ARMB=$(sha256sum "$R/build-trd/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
echo "ARMA=$ARMA ARMB=$ARMB" | tee "$O/binaries.txt"
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$R/r2/ctest-$b.log"; rc=$?
  tail -40 "$R/r2/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$O/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$R/r2/replay" "$ARMB" "$ARMA"
cp "$R"/r2/replay/trd-comparison-*.txt "$O/" 
step install-smoke bash "$L/install_smoke.sh" "$R/r2/smoke"
grep -hE 'export contract OK|FAIL|rc=' "$R"/r2/smoke/export-contract-*.log > "$O/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$R/r2/smoke/smoke-$a.log" > "$O/smoke-$a-tail.txt"; done
echo CHAIN_COMPLETE
