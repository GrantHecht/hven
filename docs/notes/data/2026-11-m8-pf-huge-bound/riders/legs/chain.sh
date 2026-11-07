#!/usr/bin/env bash
# M8 PF-HB riders chain (C246). usage: bash chain.sh <code head> <armB sha256>   -- wait on the pid of `bash chain.sh`.
# Pre-flight (fail closed), then each leg under its own `flock /tmp/box-build.lock`, in the declared order.
set -o pipefail
RL=$(cd "$(dirname "$0")" && pwd); RD=$(dirname "$RL"); WT=$(git -C "$RL" rev-parse --show-toplevel); BP=$WT/build-pfe
CODE=${1:?}; PIN=${2:?}
cd "$WT" || exit 1
pf() { echo "PREFLIGHT_FAIL: $*"; exit 2; }
[ -n "$(pgrep -a hven_bench_cell)" ] && pf "bench cell running"
git merge-base --is-ancestor "$CODE" HEAD || pf "code head not an ancestor"
[ -z "$(git diff --name-only "$CODE" HEAD | grep -v "^docs/notes/data/2026-11-m8-pf-huge-bound/riders/")" ] || pf "HEAD moved code past $CODE"
[ -z "$(git status --porcelain --untracked-files=no)" ] || pf "tracked tree not clean"
[ "$(sha256sum "$BP/armBbin/hven_sqp_corpus" | cut -d' ' -f1)" = "$PIN" ] || pf "arm B pin"
[ -d "$BP/rel/CMakeFiles/hven.dir" ] || pf "arm B build dir"
for d in rel-r dbg-r offref-r; do [ -e "$BP/$d" ] && pf "$BP/$d exists"; done
[ -x "$WT/build-pfhb/rel/tests/drivers/hven_drivers_tests" ] || pf "dev tree"
grep -l '/home/\|/worktrees/' $(ls "$RL"/*.sh | grep -v '/chain.sh$') && pf "hard-coded path in a leg"
echo "PREFLIGHT_OK $(date -Is) head $(git rev-parse --short HEAD) code $CODE"
leg() { # name script args...
  local name=$1; shift
  [ -n "$(pgrep -a hven_bench_cell)" ] && { echo "PGREP_BUSY before $name"; exit 3; }
  echo "### $name $(date -Is)"
  flock /tmp/box-build.lock bash "$RL/$1" "${@:2}" > "$RD/$name.out" 2>&1
  local rc=$?; tail -4 "$RD/$name.out"; echo "### $name rc=$rc"
  return $rc
}
fails=0
leg failproof failproof.sh "$RD/failproof" || fails=$((fails+1))
leg build-rel build.sh Release rel-r || fails=$((fails+1))
leg engine-bytes engine_bytes.sh "$PIN" || fails=$((fails+1))
leg build-dbg build.sh Debug dbg-r || fails=$((fails+1))
mkdir -p "$RD/suites"
leg ctest-release ctest.sh rel-r "$RD/suites/ctest-release.log" || fails=$((fails+1))
leg ctest-debug ctest.sh dbg-r "$RD/suites/ctest-debug.log" || fails=$((fails+1))
leg examples examples.sh rel-r || fails=$((fails+1))
leg offref offref.sh "$RD/offref" || fails=$((fails+1))
leg format format.sh 072ad8d2 || fails=$((fails+1))
echo "CHAIN_END $(date -Is) failed-legs=$fails"
