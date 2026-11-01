#!/usr/bin/env bash
# M7 WX2 gates run (NEW, derived from the classification half of offline-wx2.sh): re-classify the REHEARSAL's P-SYM compare files at the measured differing counts, both configurations.
# Release against a fresh Release build of HEAD with g5's flags (its ninja deps are what the classifier reads), Debug against the rehearsal's own build tree (left at the before arm, as g5 classifies).
# Arguments: the measured Release and Debug differing counts. Run AFTER rehearse-wx2.sh (the G4 / E1 half was measured once by offline-wx2.sh and is not repeated).
set -o pipefail
ROOT=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel) || { echo "REFUSED: no worktree root"; exit 3; }
export ROOT
W=${W:-$ROOT/docs/notes/data/2026-10-m7-wx2-gates/wrappers-gates}
NREL=${1:?release differing}; NDBG=${2:?debug differing}
BASE_S=$(bash "$W/gates-wx2.sh" --print-scratch) || exit 3
export GATES_SCRATCH=$BASE_S/rehearsal NREL NDBG
cd "$ROOT" || exit 90
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$W/gates-wx2.sh")
export BEFORE HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R-classify-wx2 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R-classify-wx2: no foreign cell binary under my lock" || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  ROOT=${ROOT:?}; S=$GATES_SCRATCH; OUT=$S/gates/psym; RB=$S/rel-deps
  CS=$(git -C "$ROOT" diff --name-only "$BEFORE..$HEADSHA" -- src/ tests/ bench/)
  CH=$(git -C "$ROOT" diff --name-only "$BEFORE..$HEADSHA" -- include/)
  rm -rf "$RB"
  cmake -S "$ROOT" -B "$RB" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > "$S/rel-deps-configure.log" 2>&1 || exit 91
  CCACHE_DISABLE=1 cmake --build "$RB" -j "$(nproc)" > "$S/rel-deps-build.log" 2>&1 || exit 91
  MKL_NUM_THREADS=1 ctest --test-dir "$RB" -N > "$S/ctest-N-release.txt" 2>&1; echo "CTEST_N_RC=$?"
  rc=0
  for CFG in Release Debug; do
    if [ $CFG = Release ]; then BD=$RB; N=$NREL; else BD=$S/psym-build; N=$NDBG; fi
    python3 -B "$S/classify_psym.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" --repo "$ROOT" --base "$BEFORE" --head "$HEADSHA" \
      --changed-sources $CS --changed-headers $CH --expect-differing "$N" --min-negative-control 1 > "$S/offline-classification-$CFG.txt" 2>&1
    c=$?; echo "OFFLINE_CLASSIFY_RC_$CFG=$c"; [ $c -eq 0 ] || rc=1
  done
  echo "FOREGROUND_END R-classify-wx2 $(date -Is) rc=${rc}"
  exit ${rc}
'
echo "LEG_RC R-classify-wx2 rc=$?"
