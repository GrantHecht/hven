#!/usr/bin/env bash
# TU-0 proof chain (../DECLARATION-TU-0.md section 4): P0 checks, P1 ctest Release and Debug, P2 replay A vs B, P3 TRAIN default configuration A vs B, P5 mutations, P6 install smoke.
# (P4, the library-behaviour proof, is carried by P2 + P3 and has no instrument.) Stops at the first failed leg.
# usage: orchestrate_tu0.sh <code sha> <base sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count> [round dir name, default gate]
#   Run as: flock /tmp/box-build.lock setsid -w bash orchestrate_tu0.sh ... > ../orchestrate.out 2>&1
set -o pipefail
source "$(dirname "$0")/env.sh"
L=$E/legs
CODE=${1:?code sha}; BASE=${2:?base sha}; CA=${3:?}; CB=${4:?}; LA=${5:?}; LB=${6:?}; NTESTS=${7:?ctest count}; R=${8:-gate}
D=$E/$R
box_check
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
git -C "$BASEWT" diff --quiet --ignore-submodules=all "$BASE" HEAD -- $PATHS || { echo "BASE_NOT_$BASE"; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD); base $(git -C "$BASEWT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$BASEWT/build/bench/hven_sqp_corpus" "$CA"
# corpus B embeds the git stamp of HEAD, so its sha moves with every commit: recorded here, not predeclared ($CB is the dev-time reading, informational).
echo "corpus B sha256 $(sha256sum "$WT/build/bench/hven_sqp_corpus" | cut -d' ' -f1) (declared dev-time $CB)"
chk "$WT/.scratch/bin/A/hven_bench_cell" "$LA"; chk "$WT/.scratch/bin/B/hven_bench_cell" "$LB"
for T in "$WT/build" "$WT/build-debug" "$BASEWT/build"; do
  # the git stamp re-configures a tree whose HEAD moved (docs commits): that relinks the bench executables only; the code paths are checked above
  echo "pre-ninja $T: $(ninja -C "$T" 2>&1 | tail -n 1)"
  ninja -C "$T" 2>&1 | tail -n 1 | grep -q "no work to do" || { echo "BUILD_TREE_STALE $T"; exit 4; }
done
echo "build trees up to date: $WT/build $WT/build-debug $BASEWT/build"
mkdir -p "$D"/{suites,replay,train,mutation,install-smoke}
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $NTESTS" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_ok() { local bad=0
  for M in walk ssn ipm; do
    python3 "$WT/scripts/compare_replay.py" "$D/replay/A-$M.csv" "$D/replay/B-$M.csv" "A-vs-B-$M" > "$D/replay/cmp-$M.txt" 2>&1; tail -n 1 "$D/replay/cmp-$M.txt"
    grep -q "differences: 0$" "$D/replay/cmp-$M.txt" || bad=1
  done
  [ $bad -eq 0 ] && echo REPLAY_ZERO_DIFFERENCES
  return $bad; }
train_ok() { python3 "$L/train_check.py" "$D/train/B" "$D/train/A" "$WT/docs/notes/data/2026-11-m8-dn3" "$E/movers-train.txt" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|  FAIL|TU0)' "$D/train/train_check.txt" | head -n 60; return $rc; }
leg P1-rel bash "$L/ctest.sh" "$WT" build "$D/suites/ctest-rel.log"
leg P1-rel-criterion suites_ok "$D/suites/ctest-rel.log"
leg P1-dbg bash "$L/ctest.sh" "$WT" build-debug "$D/suites/ctest-dbg.log"
leg P1-dbg-criterion suites_ok "$D/suites/ctest-dbg.log"
leg P2-A bash "$L/replay.sh" "$BASEWT" build "$D/replay" A
leg P2-B bash "$L/replay.sh" "$WT" build "$D/replay" B
leg P2-criterion replay_ok
leg P3-A bash "$L/train_enum.sh" "$WT" "$D/train/A" "$WT/.scratch/bin/A/hven_bench_cell" "$LA" default:D never-default:K
leg P3-B bash "$L/train_enum.sh" "$WT" "$D/train/B" "$WT/.scratch/bin/B/hven_bench_cell" "$LB" default:D never-default:K
leg P3-criterion train_ok
leg P5 bash "$L/mutations_tu0.sh" "$D/mutation"
leg P6 env WT="$WT" bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
