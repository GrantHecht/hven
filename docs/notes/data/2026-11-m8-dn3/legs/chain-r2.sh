#!/usr/bin/env bash
# M8 DN-3 r2 (../DECLARATION-r2.md): chain.sh re-pointed for r2 -- outputs chain-r2/, suites-r2/, replay-r2/, interior-r2/,
# failproof-r2/, install-smoke-r2/; TMPDIR exported to build-dn3/tmp (the home volume) for every step, because r1's
# chain failed P1 on /tmp's per-user quota (EDQUOT in the tests that write temp files). Nothing else changes.
# DN-3: one step added after the falsifier, the install smoke and export contract on both front ends (legs/install_smoke.sh,
# DN-1d r3's instrument), output install-smoke/.
# M8 DN-3 (../DECLARATION-r1.md): copied from ../../2026-11-m8-dn3b/legs/ and re-pointed in code lines only (worktree agent-a4a349c1df0402cb8, build-dn3/, this directory, arm A 09718f3dd); older provenance lines below are historical.
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-dn3b/, this directory, arm A ccd2e951f); older provenance lines below are historical. chain.sh: outputs suites/, replay/, interior/, failproof/; the falsifier legs/failproof.sh.
# M8 DN-3a r2 (../DECLARATION-r2.txt): legs/chain.sh re-pointed for r2 (outputs suites-r2/, replay-r2/, interior-r2/,
# failproof-r2/; the falsifier legs/failproof-r2.sh). P1, P2, P3, P4 in that order.
# usage: chain-r2.sh <code-head sha> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash chain-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
D=$WT/docs/notes/data/2026-11-m8-dn3
L=$D/legs
HEAD_SHA=${1:?code head sha}
ARMB=${2:?arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench CMakeLists.txt cmake || { echo "SOURCE_MOVED_SINCE_CODE_HEAD"; exit 2; }
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
export TMPDIR=$WT/build-dn3/tmp
mkdir -p "$TMPDIR" "$D/suites-r2" "$D/replay-r2" "$D/interior-r2" "$D/failproof-r2" "$D/install-smoke-r2"
echo "TMPDIR=$TMPDIR"
worst=0
step() { local name=$1; shift; "$@"; local rc=$?; echo "STEP $name rc=$rc $(date -Is)"; [ $rc -gt $worst ] && worst=$rc; }
step ctest-rel bash "$L/ctest.sh" rel "$D/suites-r2/ctest-rel.log"
step ctest-dbg bash "$L/ctest.sh" dbg "$D/suites-r2/ctest-dbg.log"
step replay bash "$L/replay.sh" "$D/replay-r2"
step interior bash "$L/interior.sh" "$D/interior-r2" "$ARMB"
step failproof bash "$L/failproof.sh" "$HEAD_SHA" "$D/failproof-r2"
step install-smoke bash "$L/install_smoke.sh" "$D/install-smoke-r2"
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
