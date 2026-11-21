#!/usr/bin/env bash
# I18 legs in order, each leg its own box-lane step (a lane per step, never a session); stops at the first failed leg.
# usage: orchestrate.sh <dev|gate> <code commit> <lane A sha256> <lane B sha256> <ctest count>
#   dev : outputs under dev/, criteria in report mode (the measured sets become the movers-*.txt files of the declaration)
#   gate: outputs under gate/, criteria against movers-*.txt and the dev rows (DECLARATION-I18.md section 4)
set -o pipefail
source "$(dirname "$0")/env.sh"
MODE=${1:?dev|gate}; CODE=${2:?code commit}; SHA_A=${3:?lane A sha}; SHA_B=${4:?lane B sha}; NCT=${5:?ctest count}
BASE=331b3183747b5c1849651706f648de2c337ddf75
BL=${BOX_LANE_BIN:-$HOME/.local/bin/box-lane}
O=$E/$MODE; mkdir -p "$O"
leg() { local n=$1; shift; echo "LEG $n start $(date -Is)"; "$@"; local rc=$?; echo "LEG $n rc=$rc $(date -Is)"; [ $rc -eq 0 ] || { echo "STOP at $n"; exit 1; }; }
lane() { "$BL" -- "$@"; }
mv_or_dash() { [ "$MODE" = gate ] && echo "$E/$1" || echo -; }
P0() {
  cd "$WT" || return 1
  local paths="src include tests bench examples bindings CMakeLists.txt cmake scripts"
  [ -z "$(git status --porcelain -- $paths)" ] || { echo CODE_PATHS_DIRTY; return 1; }
  git diff --quiet "$CODE" HEAD -- $paths || { echo CODE_PATHS_NOT_THE_CODE_COMMIT; return 1; }
  [ "$(git -C "$BASEWT" rev-parse HEAD)" = "$BASE" ] || { echo BASE_NOT_331b31837; return 1; }
  [ -z "$(git -C "$BASEWT" status --porcelain -- $paths)" ] || { echo BASE_DIRTY; return 1; }
  for d in "$WT/build" "$WT/build-debug" "$BASEWT/build"; do
    ! ninja -C "$d" -n 2>/dev/null | grep -qE "Building|Linking" || { echo "BUILD_TREE_STALE $d"; return 1; }   # dry run: no compile or link edge
  done
  [ "$(sha256sum "$WT/.scratch/bin/A/hven_bench_cell" | cut -d' ' -f1)" = "$SHA_A" ] || { echo LANE_A_SHA; return 1; }
  [ "$(sha256sum "$WT/.scratch/bin/B/hven_bench_cell" | cut -d' ' -f1)" = "$SHA_B" ] || { echo LANE_B_SHA; return 1; }
  echo P0_OK
}
ctest_ok() { if [ "$NCT" = - ]; then grep "tests passed" "$1"; else grep -q "100% tests passed, 0 tests failed out of $NCT\$" "$1"; fi; }
leg P0 P0
leg P1-rel lane bash "$L/ctest.sh" "$WT" build "$O/ctest-release.log"
leg P1-rel-criterion ctest_ok "$O/ctest-release.log"
leg P1-dbg lane bash "$L/ctest.sh" "$WT" build-debug "$O/ctest-debug.log"
leg P1-dbg-criterion ctest_ok "$O/ctest-debug.log"
leg P2-A lane bash "$L/replay.sh" "$BASEWT" build "$O/replay" A
leg P2-B lane bash "$L/replay.sh" "$WT" build "$O/replay" B
if [ "$MODE" = gate ]; then
  leg P2-criterion python3 "$L/replay_check.py" "$WT" "$O/replay" A "$O/replay" B "$E/dev/replay" B "$E/movers-replay.txt"
else
  for m in walk ssn ipm; do python3 "$WT/scripts/compare_replay.py" "$O/replay/A-$m.csv" "$O/replay/B-$m.csv" "A-vs-B $m"; done
fi
leg P3-A lane bash "$L/corpus.sh" "$BASEWT" build A "$O/corpus"
leg P3-B lane bash "$L/corpus.sh" "$WT" build B "$O/corpus"
leg P3-criterion python3 "$L/corpus_check.py" "$O/corpus/A-rows.txt" "$O/corpus/B-rows.txt" "$(mv_or_dash movers-corpus.txt)"
leg P4-A lane bash "$L/train_enum.sh" "$O/train-A" "$WT/.scratch/bin/A/hven_bench_cell" "$SHA_A"
leg P4-B lane bash "$L/train_enum.sh" "$O/train-B" "$WT/.scratch/bin/B/hven_bench_cell" "$SHA_B"
if [ "$MODE" = gate ]; then
  leg P4-criterion python3 "$L/train_check.py" "$O/train-B" "$WT/docs/notes/data/2026-11-m8-dn3" "$O/train-A" "$E/dev/train-B" "$E/movers-train.txt"
else
  leg P4-report python3 "$L/train_check.py" "$O/train-B" "$WT/docs/notes/data/2026-11-m8-dn3" "$O/train-A"
fi
leg P5-A lane bash "$L/nlp35.sh" "$O/nlp35-A" "$WT/.scratch/bin/A/hven_bench_cell" "$SHA_A"
leg P5-B lane bash "$L/nlp35.sh" "$O/nlp35-B" "$WT/.scratch/bin/B/hven_bench_cell" "$SHA_B"
leg P5-criterion python3 "$L/nlp35_check.py" "$O/nlp35-B" "$O/nlp35-A" "$(mv_or_dash movers-nlp35.txt)"
leg P6 lane bash "$L/mutations.sh" "$O/mutation" A1 A2 A3 A4 A5 A6 A7 A8 L1
if [ "$MODE" = gate ]; then
  leg P6-criterion python3 "$L/mutation_check.py" "$O/mutation" "$E/mutations-declared.txt"
fi
leg P7 lane bash "$L/install_smoke.sh" "$O/install-smoke"
echo "ALL_LEGS_PASSED $(date -Is)"
