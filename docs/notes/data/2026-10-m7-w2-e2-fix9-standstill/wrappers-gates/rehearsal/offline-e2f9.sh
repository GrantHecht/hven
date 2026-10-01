#!/usr/bin/env bash
# M7 W2 E2 FIX ROUND 9 -- COPIED from the e2f8 offline-e2f8.sh (itself E4 run 2's offline-r2.sh)
# and re-pointed at ../gates-e2f9.sh (path, runner name, leg label, G4 label) and PREV at the e2f8
# gate record (docs/notes/data/2026-09-m7-w2-e2-fix7-fix8/gates/replay, whose sources are
# BEFORE's). Fix 9 is expected NOT to move U0 (the fix-9 report: 0 differences vs fix8), so the
# EXACT arm against PREV is expected rc 0 here; it stays RECORDED, not asserted, as in e2f8.
# E4's header follows:
# M7 W2 E4 RUN 2 -- the rehearsal's OFFLINE half, run after rehearse.sh under its own lock hold:
# (1) a fresh Release build of HEAD with g5's flags (the rehearsal's Release tree was replaced by
#     its Debug arm; the classifier reads only its ninja deps and object list), then the classifier
#     re-run on the rehearsal's compare files at the MEASURED differing counts, both
#     configurations (Debug against the rehearsal's own build tree, left at the before arm as g5
#     classifies); (2) G4 offline on these sources from that Release build: the 27-cell
#     three-engine replay, compared GATED (1e-5) against the M6 close pin and EXACT against the
#     previous round's gate record (whose sources are BEFORE's).
# Arguments: the measured Release and Debug differing counts. Scratch ASKED of the runner.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
W=${W:-$ROOT/docs/notes/data/2026-10-m7-w2-e2-fix9-standstill/wrappers-gates}
NREL=${1:?release differing}; NDBG=${2:?debug differing}
BASE_S=$(bash "$W/gates-e2f9.sh" --print-scratch) || { echo "REFUSED: no scratch from the runner"; exit 3; }
case "$BASE_S" in "$ROOT"/.scratch/?*) : ;; *) echo "REFUSED: scratch '$BASE_S'"; exit 3 ;; esac
export GATES_SCRATCH=$BASE_S/rehearsal NREL NDBG
cd "$ROOT" || exit 90
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$W/gates-e2f9.sh")
export BEFORE HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R-offline-e2f9 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R-offline-e2f9: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  ROOT=/home/ghecht/Projects/hven; S=$GATES_SCRATCH; OUT=$S/gates/psym; RB=$S/rel-deps
  CS=$(git -C "$ROOT" diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ bench/)
  CH=$(git -C "$ROOT" diff --name-only "$BEFORE..$HEADSHA" -- include/)
  rm -rf "$RB"
  cmake -S "$ROOT" -B "$RB" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$S/rel-deps-configure.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$RB" -j "$(nproc)" > "$S/rel-deps-build.log" 2>&1 || exit 91
  rc=0
  for CFG in Release Debug; do
    if [ $CFG = Release ]; then BD=$RB; N=$NREL; else BD=$S/psym-build; N=$NDBG; fi
    # shellcheck disable=SC2086
    python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
      --repo "$ROOT" --base "$BEFORE" --head "$HEADSHA" \
      --changed-sources $CS --changed-headers $CH \
      --expect-differing "$N" --min-negative-control 1 > "$S/offline-classification-$CFG.txt" 2>&1
    c=$?; echo "OFFLINE_CLASSIFY_RC_$CFG=$c"; [ $c -eq 0 ] || rc=1
  done
  D=$S/g4; mkdir -p "$D"; : > "$D/comparison.txt"
  CELLS="$(grep -v "^#" "$ROOT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d "\n")"
  for eng in walk ssn ipm; do
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 "$RB/bench/hven_sqp_corpus" --engine "$eng" \
      --cells "$CELLS" --csv "$D/head-$eng.csv" > "$D/run-$eng.log" 2>&1
    r=$?; echo "ran head-$eng rc=$r"; [ $r -eq 0 ] || { rc=1; break; }
  done
  PIN=$ROOT/docs/notes/data/2026-09-m6-close-gate/replay
  PREV=$ROOT/docs/notes/data/2026-09-m7-w2-e2-fix7-fix8/gates/replay
  for eng in walk ssn ipm; do
    python3 -B "$ROOT/scripts/compare_replay.py" --residual-gate 1e-5 "$PIN/head-$eng.csv" "$D/head-$eng.csv" \
      "M6 CLOSE HEAD-$eng vs M7 W2 E2 F9 HEAD-$eng (GATED 1e-5)" >> "$D/comparison.txt"; r=$?; echo "CMP_PIN_$eng=$r"
    # E4 DECLARES 43 DIFFERENCES against the pin, so rc 1 is the EXPECTED reading and the SET
    # comparison is made by the gate runner own assert on this transcript. Only an rc that
    # the transcript could not have come from is a refusal here. (NO APOSTROPHE IN THIS
    # BLOCK: it lives inside a single-quoted bash -c body.)
    [ $r -eq 1 ] || { echo "CMP_PIN_UNDECLARED_$eng=$r"; rc=1; }
    python3 -B "$ROOT/scripts/compare_replay.py" "$PREV/head-$eng.csv" "$D/head-$eng.csv" \
      "PREVIOUS GATE RECORD HEAD-$eng vs M7 W2 E2 F9 HEAD-$eng (EXACT)" >> "$D/comparison.txt"; r=$?; echo "CMP_PREV_$eng=$r"
    # The EXACT arm against the previous round gate record, whose sources are the BEFORE
    # sources, is EXPECTED rc 0 this round (fix 9 is reported not to move U0). RECORDED,
    # never asserted either way. (NO APOSTROPHE IN THIS BLOCK: single-quoted bash -c body.)
    echo "CMP_PREV_NOTE_$eng=$r (RECORDED, not asserted: the previous record is the e2f8 gates at the BEFORE sources)"
  done
  grep -E "cells compared" "$D/comparison.txt"
  echo "FOREGROUND_END R-offline-e2f9 $(date -Is) rc=${rc}"
  exit ${rc}
'
echo "LEG_RC R-offline-e2f9 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
