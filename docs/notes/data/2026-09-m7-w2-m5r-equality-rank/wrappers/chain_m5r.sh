#!/usr/bin/env bash
# M7 W2 M5r -- the chain: build the rank probe (one lock hold), then one lock
# hold PER CELL over the 23 TRAIN cells. NO SOLVER RUNS: the probe constructs no
# solver; each leg is a CUTEst setup, one Jacobian evaluation per point and a
# few dense QRs.
#
# THE ROSTER is DERIVED, never typed: the CUTEst TRAIN names of the harness's
# own split pin (`sets/split-v1.json`), and the chain refuses unless there are
# exactly 23 and every one is in the probe's roster. The probe itself refuses a
# HOLDOUT name, and that refusal is EXERCISED below before any leg.
#
# BOX PROTOCOL. `pgrep` for a foreign cell binary runs as its OWN command before
# the lock, and again INSIDE the lock, where it is the one that binds;
# PGREP_EMPTY / FOREGROUND_START / FOREGROUND_END are printed; MKL_NUM_THREADS=1.
# /home/ghecht/Projects/hven-bench is READ-ONLY (it is the working directory
# because the bridge's decoded-problem path is relative to it).
set -o pipefail

ROOT=/home/ghecht/Projects/hven
ART="$ROOT/docs/notes/data/2026-09-m7-w2-m5r-equality-rank"
S="$ROOT/.scratch/m7w2m5r"
BENCH=/home/ghecht/Projects/hven-bench
EXE="$S/build/m5r_rank_probe"
PAT='^/home/ghecht/Projects/[^ ]*(hven_bench_cell[^ ]*|m[0-9][a-z0-9_]*probe[^ ]*|c1_probe|hven_sqp_corpus|hven_bench_probe)( |$)'
DUMP_CELLS="SMBANK STEENBRB SAWPATH"

mkdir -p "$S/logs" "$S/rows" "$S/dump"
echo "CHAIN_M5R_BEGIN $(date -Is) HEAD $(git -C "$ROOT" rev-parse --short HEAD) dirty=$(git -C "$ROOT" status --porcelain | wc -l)"

# ---- the roster, derived ---------------------------------------------------
CELLS=$(python3 - "$BENCH/sets/split-v1.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(" ".join(a["problem"] for a in d["assignments"]
               if a["problem_set"] == "cutest" and a["split"] == "TRAIN"))
PY
)
HOLD=$(python3 - "$BENCH/sets/split-v1.json" <<'PY'
import json, sys
d = json.load(open(sys.argv[1]))
print(" ".join(a["problem"] for a in d["assignments"]
               if a["problem_set"] == "cutest" and a["split"] == "HOLDOUT"))
PY
)
echo "SPLIT_PIN_SHA256 $(sha256sum "$BENCH/sets/split-v1.json" | cut -d' ' -f1)"
echo "TRAIN ($(echo $CELLS | wc -w)): $CELLS"
echo "HOLDOUT ($(echo $HOLD | wc -w)) -- never run: $HOLD"
[ "$(echo $CELLS | wc -w)" = 23 ] || { echo "REFUSED: TRAIN is not 23 cells"; exit 90; }

# ---- build, one lock hold --------------------------------------------------
echo "PGREP_BEFORE_LOCK build: $(pgrep -af "$PAT" | tr '\n' ' ')"
flock /tmp/box-build.lock bash -c '
PAT=$1; ART=$2; S=$3; ROOT=$4
echo "FOREGROUND_START m5r-build $(date -Is)"
f=$(pgrep -af "$PAT")
if [ -n "$f" ]; then echo "ABORTED: foreign cell binary under my lock: $f"; echo "FOREGROUND_END m5r-build $(date -Is) rc=95"; exit 95; fi
echo "PGREP_EMPTY m5r-build: no foreign cell binary under my lock"
python3 -B "$ART/wrappers/replay_build_m5r.py" \
  --probebuild "$ROOT/.scratch/m7w2m3c/probebuild" \
  --install-lib "$ROOT/.scratch/m7w2m3c/install/lib64/libhven.a" \
  --lib-pin "$ROOT/.scratch/m7w2m3c/library-sha256.txt" \
  --probe-src "$ART/wrappers/m5r_rank_probe.cpp" \
  --out "$S/build" --transcript "$S/logs/build-m5r.txt" | tail -12
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END m5r-build $(date -Is) rc=$rc"
exit $rc
' _ "$PAT" "$ART" "$S" "$ROOT" || { echo "REFUSED: build failed"; exit 91; }
[ -x "$EXE" ] || { echo "REFUSED: no binary"; exit 91; }
BINSHA=$(sha256sum "$EXE" | cut -d' ' -f1)
echo "BINARY_SHA256 $BINSHA"

# ---- refusals, exercised (a checker never seen to refuse is not a checker) --
cd "$BENCH" || exit 93
first_hold=$(echo $HOLD | awk '{print $1}')
if "$EXE" --problem "$first_hold" >/dev/null 2>"$S/logs/refusal.txt"; then
  echo "REFUSED: the HOLDOUT cell $first_hold was ACCEPTED"; exit 96; fi
echo "HOLDOUT refused as it should: $(cat "$S/logs/refusal.txt")"
if "$EXE" --problem NOT_A_CELL >/dev/null 2>"$S/logs/refusal.txt"; then
  echo "REFUSED: an unknown name was ACCEPTED"; exit 96; fi
echo "unknown name refused as it should: $(cat "$S/logs/refusal.txt")"

# ---- the legs ---------------------------------------------------------------
fails=0
for cell in $CELLS; do
  echo "PGREP_BEFORE_LOCK $cell: $(pgrep -af "$PAT" | tr '\n' ' ')"
  dump=""
  case " $DUMP_CELLS " in *" $cell "*) mkdir -p "$S/dump/$cell"; dump="--dump-dir $S/dump/$cell";; esac
  flock /tmp/box-build.lock bash -c '
PAT=$1; EXE=$2; S=$3; cell=$4; dump=$5; BINSHA=$6; ROOT=$7
echo "FOREGROUND_START leg-$cell $(date -Is)"
f=$(pgrep -af "$PAT")
if [ -n "$f" ]; then echo "ABORTED before $cell: foreign cell binary under my lock: $f"; echo "FOREGROUND_END leg-$cell $(date -Is) rc=95"; exit 95; fi
echo "PGREP_EMPTY leg-$cell: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 90
sif=sets/cutest/sif/$cell.SIF
{
  echo "provenance_hven_commit=$(git -C "$ROOT" rev-parse HEAD)"
  echo "provenance_bench_commit=$(git -C /home/ghecht/Projects/hven-bench rev-parse HEAD)"
  echo "provenance_binary_sha256=$BINSHA"
  echo "provenance_model_path=$sif"
  echo "provenance_model_sha256=$(sha256sum "$sif" | cut -d" " -f1)"
  echo "provenance_decoded_outsdif_sha256=$(sha256sum .scratch/h3/decoded/$cell/OUTSDIF.d | cut -d" " -f1)"
  echo "provenance_date=$(date -Is)"
  echo "provenance_host=$(hostname) $(lscpu | grep -m1 "Model name" | sed "s/.*: *//")"
  echo "provenance_env=MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2"
} > "$S/rows/$cell.txt"
t0=$(date +%s.%N)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" --problem "$cell" $dump \
  >> "$S/rows/$cell.txt" 2> "$S/logs/$cell.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-10s rc=%s wall_s=%.1f (INFORMATIONAL)\n" "$cell" "$rc" "$(echo "$t1 - $t0" | bc)"
grep -E "^(n|me|fixed_bounds|eq_dflt_deficiency|eq_alt_deficiency|skel_dflt_deficiency|entry_dflt_deficiency|f_finite|jac_finite)=" "$S/rows/$cell.txt" | tr "\n" " "; echo
echo "FOREGROUND_END leg-$cell $(date -Is) rc=$rc"
exit $rc
' _ "$PAT" "$EXE" "$S" "$cell" "$dump" "$BINSHA" "$ROOT"
  rc=$?
  echo "WRAPPER_RC leg-$cell rc=$rc"
  [ "$rc" = 0 ] || fails=$((fails + 1))
done
echo "CHAIN_M5R_END $(date -Is) failed_legs=$fails"
