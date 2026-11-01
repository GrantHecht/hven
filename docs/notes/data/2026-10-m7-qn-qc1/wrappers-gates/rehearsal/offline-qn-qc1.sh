#!/usr/bin/env bash
# M7 W2 QN + QC-1 gates run: COPIED from the W1 + CERT-1 offline script (NOT edited), re-pointed at ../gates-qn-qc1.sh. CHANGES: the EXACT
# arm now names the PREVIOUS GREEN RECORD (W1 + CERT-1's gates/replay, docs/notes/data/2026-10-m7-w2-w1-cert1/gates/replay, whose
# sources are BEFORE d495e15a's) -- the template's arm named an older record (NC3 r2 / NC3 run 1) and its comparison against W1 + CERT-1
# had to be run by hand afterwards; the run-1 arm is dropped. The EXACT arm is RECORDED, not asserted. The template header follows.
# M7 W2 NC4 + R2 gates run: COPIED from NC3 run-2 offline-nc3-r2.sh (NOT edited), re-pointed at ../gates-qn-qc1.sh; ROOT derived (worktree);
# PREV is NC3 r2 gates-r2/replay (sources == BEFORE 79775010), the EXACT arm stays RECORDED, not asserted. The NC3 run-2 text follows.
# NC3 RUN 2 (successor): COPIED from run 1 offline-nc3.sh (NOT edited), re-pointed at ../gates-qn-qc1.sh; a SECOND recorded EXACT arm
# against RUN 1 own gate replay (nc3 gates/replay, sources 054e4c78) is added -- recorded, not asserted. Run 1 text follows.
# M7 W2 ROUND NC3 -- COPIED from the P1 offline script (itself e2f9 / e2f8 / E4 run 2) and re-pointed
# (runner name, task directory, leg labels, G4 label); PREV is now the P1 gate record's replay
# (docs/notes/data/2026-10-m7-w2-p1-schur-cost/gates/replay, whose sources are BEFORE's). The EXACT arm
# against PREV is RECORDED, not asserted: the NC3 report says U0 is exact vs fix 9, so exact-identical
# is the expectation. The P1 text follows for provenance.
# M7 W2 ROUND P1 -- COPIED from the e2f9 rehearsal script (itself e2f8 / E4 run 2) and re-pointed
# (runner name, task directory, scratch ask, leg labels, previous gate record, G4 label); the e2f9 prose
# below is the e2f9 round's and is kept for provenance. P1: one header, two MODIFIED .cpp both inside
# the P-SYM universe, no ADDED .cpp, nothing outside the universe; the OUTS/OUTM lists are EMPTY.
# M7 W2 ROUND P1 (the e2f9 text follows) -- COPIED from the e2f8 offline-e2f8.sh (itself E4 run 2's offline-r2.sh)
# and re-pointed at ../gates-qn-qc1.sh (path, runner name, leg label, G4 label) and PREV at the e2f9
# gate record (docs/notes/data/2026-10-m7-w2-p1-schur-cost/gates/replay, whose sources are
# BEFORE's). NC3 is expected NOT to move U0 (the NC3 report: exact vs fix 9), so the
# EXACT arm against PREV is expected rc 0 here; it stays RECORDED, not asserted, as in e2f9.
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
ROOT=$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel) || { echo "REFUSED: no worktree root"; exit 3; }
export ROOT
W=${W:-$ROOT/docs/notes/data/2026-10-m7-qn-qc1/wrappers-gates}
NREL=${1:?release differing}; NDBG=${2:?debug differing}
BASE_S=$(bash "$W/gates-qn-qc1.sh" --print-scratch) || { echo "REFUSED: no scratch from the runner"; exit 3; }
case "$BASE_S" in "$ROOT"/.scratch/?*) : ;; *) echo "REFUSED: scratch '$BASE_S'"; exit 3 ;; esac
export GATES_SCRATCH=$BASE_S/rehearsal NREL NDBG
cd "$ROOT" || exit 90
BEFORE=$(sed -n 's/^BEFORE=\${BEFORE:-\([0-9a-f]*\)}$/\1/p' "$W/gates-qn-qc1.sh")
export BEFORE HEADSHA=$(git rev-parse HEAD)
echo "START $(date -Is) HEAD $HEADSHA BEFORE $BEFORE dirty=$(git status --porcelain | wc -l)"
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: tree not clean"; exit 2; }
pgrep -x hven_bench_cell > /dev/null && echo "NOTE: a foreign cell binary is alive before the lock"
flock /tmp/box-build.lock bash -c '
  set -o pipefail
  echo "FOREGROUND_START R-offline-qn-qc1 $(date -Is)"
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  [ -z "$foreign" ] && echo "PGREP_EMPTY R-offline-qn-qc1: no foreign cell binary under my lock" \
                    || echo "NOTE: a foreign cell binary is alive under my lock: $foreign"
  ROOT=${ROOT:?}; S=$GATES_SCRATCH; OUT=$S/gates/psym; RB=$S/rel-deps
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
  PREV=$ROOT/docs/notes/data/2026-10-m7-w2-p1e-c13/gates/replay
  for eng in walk ssn ipm; do
    python3 -B "$ROOT/scripts/compare_replay.py" --residual-gate 1e-5 "$PIN/head-$eng.csv" "$D/head-$eng.csv" \
      "M6 CLOSE HEAD-$eng vs M7 W2 QN QC1 HEAD-$eng (GATED 1e-5)" >> "$D/comparison.txt"; r=$?; echo "CMP_PIN_$eng=$r"
    # E4 DECLARES 43 DIFFERENCES against the pin, so rc 1 is the EXPECTED reading and the SET
    # comparison is made by the gate runner own assert on this transcript. Only an rc that
    # the transcript could not have come from is a refusal here. (NO APOSTROPHE IN THIS
    # BLOCK: it lives inside a single-quoted bash -c body.)
    [ $r -eq 1 ] || { echo "CMP_PIN_UNDECLARED_$eng=$r"; rc=1; }
    python3 -B "$ROOT/scripts/compare_replay.py" "$PREV/head-$eng.csv" "$D/head-$eng.csv" \
      "PREVIOUS (P1E + C13, record 7c24a78b) GATE RECORD HEAD-$eng vs M7 W2 QN QC1 HEAD-$eng (EXACT)" >> "$D/comparison.txt"; r=$?; echo "CMP_PREV_$eng=$r"
    # The EXACT arm against the previous round gate record, whose sources are the BEFORE
    # sources, is EXPECTED rc 0 this round (P1 is reported not to move U0). RECORDED,
    # never asserted either way. (NO APOSTROPHE IN THIS BLOCK: single-quoted bash -c body.)
    echo "CMP_PREV_NOTE_$eng=$r (RECORDED, not asserted: the previous record is P1E + C13 gates at the BEFORE sources)"
  done
  grep -E "cells compared" "$D/comparison.txt"
  echo "FOREGROUND_END R-offline-qn-qc1 $(date -Is) rc=${rc}"
  exit ${rc}
'
echo "LEG_RC R-offline-qn-qc1 rc=$?"
echo "END $(date -Is) tree=$(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD) dirty=$(git status --porcelain | wc -l)"
