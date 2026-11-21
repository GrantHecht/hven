# M8 AP-5 G5 r2 (../DECLARATION-r2.md): copied from ../legs/chain.sh; changes: legs-r2, -r2 evidence directories, .scratch/g5r2-*, the export-contract extraction greps 'export contract OK' (r1's grep matched no line). Older provenance lines below are historical.
#!/usr/bin/env bash
# M8 AP-5 G5 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g4/legs-r4/chain.sh, re-pointed (worktree g5, build-g5, unsuffixed evidence dirs, .scratch/g5-*); stops at the first failed step.
# Was: M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/chain.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/chain.sh; changes: legs-r3 scripts, -r3 evidence directories, .scratch/g4r3-* raw, the TSan tail covers both TSan binaries. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): the chain, on the pattern of ../../2026-11-m8-ap5-g2/legs-r3/chain.sh. Every step runs; the
# worst rc is the chain's. Run AFTER ../runtime/run_all.sh and ../ic/run_ic.sh (those are solo).
# usage: chain.sh <code-head sha> <expected arm B sha256> <expected arm A sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g5
D=$WT/docs/notes/data/2026-11-m8-ap5-g5
L=$D/legs-r2
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
ARMA=${3:?arm A sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
mkdir -p "$WT/.scratch/g5r2-suites" "$D/suites-r2" "$D/replay-r2" "$D/interior-r2" "$D/install-smoke-r2" "$D/tsan-r2" "$D/mutation-r2"
worst=0
# G5: STOPS at the first failed step (the evidence written so far stays).
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; if [ $rc -ne 0 ]; then echo "CHAIN_STOPPED_AT $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
for b in rel dbg; do
  bash "$L/ctest.sh" $b "$WT/.scratch/g5r2-suites/ctest-$b.log"; rc=$?
  tail -40 "$WT/.scratch/g5r2-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites-r2/ctest-$b-summary.txt"
  step ctest-$b test $rc -eq 0
done
step replay bash "$L/replay.sh" "$D/replay-r2" "$ARMB" "$ARMA"
step interior bash "$L/interior.sh" "$D/interior-r2" "$ARMB"
bash "$L/install_smoke.sh" "$WT/.scratch/g5r2-install-smoke"; rc=$?
grep -hE 'export contract OK|FAIL|rc=' "$WT"/.scratch/g5r2-install-smoke/export-contract-*.log > "$D/install-smoke-r2/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$WT/.scratch/g5r2-install-smoke/smoke-$a.log" > "$D/install-smoke-r2/smoke-$a-tail.txt" 2>/dev/null; done
step install-smoke test $rc -eq 0
bash "$L/tsan.sh" "$WT/.scratch/g5r2-tsan"; rc=$?
tail -5 "$WT/.scratch/g5r2-tsan/tsan-g5-run.log" > "$D/tsan-r2/tsan-run-tail.txt" 2>/dev/null
tail -5 "$WT/.scratch/g5r2-tsan/tsan-g5-seam-run.log" > "$D/tsan-r2/tsan-seam-run-tail.txt" 2>/dev/null
echo "reports=$(cat "$WT"/.scratch/g5r2-tsan/tsan-g5-run.log "$WT"/.scratch/g5r2-tsan/tsan-g5-seam-run.log | grep -c 'WARNING: ThreadSanitizer')" >> "$D/tsan-r2/tsan-run-tail.txt"
step tsan test $rc -eq 0
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation-r2"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
