#!/usr/bin/env bash
# I17 r2 proof chain (../DECLARATION-I17-r2.md section 4): orchestrate.sh with arm A = origin/m8 afc9744bf, the r2 movers lists, mutations_r2.sh and the round
# directory gate-r2. P0 checks, P1 ctest Release and Debug, P2 replay A vs B, P3 TRAIN default config A vs B, P4 the Always-forced measurement, P5 mutations,
# P6 install smoke. Stops at the first failed leg.
# usage: orchestrate_r2.sh <code sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count> [round dir name, default gate-r2]
#   Run as: flock /tmp/box-build.lock setsid -w bash orchestrate_r2.sh ... > ../orchestrate-r2.out 2>&1
set -o pipefail
source "$(dirname "$0")/env.sh"
L=$E/legs
CODE=${1:?code sha}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NTESTS=${6:?ctest count}; R=${7:-gate-r2}
D=$E/$R
box_check
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
git -C "$BASEWT" diff --quiet --ignore-submodules=all afc9744bf8050a87e678d0dfa77a0d8efeff40b5 HEAD -- $PATHS || { echo BASE_NOT_afc9744bf; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD); base $(git -C "$BASEWT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$BASEWT/build/bench/hven_sqp_corpus" "$CA"; chk "$WT/build/bench/hven_sqp_corpus" "$CB"
chk "$WT/.scratch/bin/A/hven_bench_cell" "$LA"; chk "$WT/.scratch/bin/B/hven_bench_cell" "$LB"
for T in "$WT/build" "$WT/build-debug" "$BASEWT/build"; do
  ninja -C "$T" 2>&1 | tail -n 1 | grep -q "no work to do" || { echo "BUILD_TREE_STALE $T"; exit 4; }
done
echo "build trees up to date: $WT/build $WT/build-debug $BASEWT/build"
mkdir -p "$D"/{suites,replay,train,always,mutation,install-smoke}
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
train_ok() { python3 "$L/train_check.py" "$D/train/B" "$D/train/A" "$WT/docs/notes/data/2026-11-m8-dn3" "$E/movers-train-r2.txt" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|  FAIL|I17)' "$D/train/train_check.txt" | head -n 60; return $rc; }
always_ok() { python3 "$L/always_gate.py" "$D/always/A" "$D/always/B" "$WT/docs/notes/data/2026-11-m8-dn3" "$E/movers-always-r2.txt" 10 > "$D/always/always_gate.txt" 2>&1; local rc=$?
  cat "$D/always/always_gate.txt" | cut -c1-200 | head -n 80; return $rc; }
CAPS=""; for k in 6 8 10 13 16 20; do CAPS="$CAPS cap$k-always-default:A$k cap$k-default:N$k"; done
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
LANE_FILTER=hven-sqp leg P4-A bash "$L/train_enum.sh" "$WT" "$D/always/A" "$WT/.scratch/bin/A/hven_bench_cell" "$LA" always-default:X
LANE_FILTER=hven-sqp leg P4-B bash "$L/train_enum.sh" "$WT" "$D/always/B" "$WT/.scratch/bin/B/hven_bench_cell" "$LB" always-default:X default:N $CAPS
leg P4-criterion always_ok
leg P5 bash "$L/mutations_r2.sh" "$D/mutation"
leg P6 env WT="$WT" bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
