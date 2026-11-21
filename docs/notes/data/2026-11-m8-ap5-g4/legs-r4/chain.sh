#!/usr/bin/env bash
# M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/chain.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/chain.sh; changes: legs-r3 scripts, -r3 evidence directories, .scratch/g4r3-* raw, the TSan tail covers both TSan binaries. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): the chain, on the pattern of ../../2026-11-m8-ap5-g2/legs-r3/chain.sh. Every step runs; the
# worst rc is the chain's. Run AFTER ../runtime/run_all.sh and ../ic/run_ic.sh (those are solo).
# usage: chain.sh <code-head sha> <expected arm B sha256>      Run as: flock /tmp/box-build.lock bash chain.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
D=$WT/docs/notes/data/2026-11-m8-ap5-g4
L=$D/legs-r4
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench examples CMakeLists.txt cmake scripts || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no -- src include tests bench examples CMakeLists.txt cmake scripts)" ] || { echo "SOURCE_DIRTY"; exit 2; }
mkdir -p "$WT/.scratch/g4r4-suites" "$D/suites-r4" "$D/replay-r4" "$D/interior-r4" "$D/install-smoke-r4" "$D/tsan-r4" "$D/mutation-r4"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$WT/.scratch/g4r4-suites/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$WT/.scratch/g4r4-suites/ctest-dbg.log"
for b in rel dbg; do tail -40 "$WT/.scratch/g4r4-suites/ctest-$b.log" | grep -E 'tests passed|tests failed|Not Run|\*\*\*' > "$D/suites-r4/ctest-$b-summary.txt"; done
step replay bash "$L/replay.sh" "$D/replay-r4" "$ARMB"
step interior bash "$L/interior.sh" "$D/interior-r4" "$ARMB"
step install-smoke bash "$L/install_smoke.sh" "$WT/.scratch/g4r4-install-smoke"
grep -hE 'PASS|FAIL|rc=|TU' "$WT"/.scratch/g4r4-install-smoke/export-contract-*.log > "$D/install-smoke-r4/export-contract-summary.txt" 2>/dev/null
for a in default clang; do tail -4 "$WT/.scratch/g4r4-install-smoke/smoke-$a.log" > "$D/install-smoke-r4/smoke-$a-tail.txt" 2>/dev/null; done
step tsan bash "$L/tsan.sh" "$WT/.scratch/g4r4-tsan"
tail -5 "$WT/.scratch/g4r4-tsan/tsan-g4-run.log" > "$D/tsan-r4/tsan-run-tail.txt" 2>/dev/null
tail -5 "$WT/.scratch/g4r4-tsan/tsan-g4-seam-run.log" > "$D/tsan-r4/tsan-seam-run-tail.txt" 2>/dev/null
echo "reports=$(cat "$WT"/.scratch/g4r4-tsan/tsan-g4-run.log "$WT"/.scratch/g4r4-tsan/tsan-g4-seam-run.log | grep -c 'WARNING: ThreadSanitizer')" >> "$D/tsan-r4/tsan-run-tail.txt"
step mutations bash "$L/mutations.sh" "$HEAD_SHA" "$D/mutation-r4"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
