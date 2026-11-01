#!/usr/bin/env bash
# M7 W2 M3b FIX ROUND 1 -- THE DISCRIMINATING RE-TRACE CHAIN. Four legs, one
# lock hold each, detached.
#
# A COPY of `chain_trace_m3b.sh` beside it, which is NEVER edited: it produced
# M3b's committed traces. The differences are the scratch root, the probe
# (`fix1_probe`, built from `probe-fix1-CMakeLists.txt`), the leg TAGS -- which
# carry `-G` so a row or trace from this chain can never be mistaken for M3b's
# -- and this header. Every argument, tolerance, pin and abort below is that
# file's and is unchanged, which is the point: the re-trace must be the SAME
# measurement with MORE fields recorded, not a new one.
#
# Pre-registered by RULE-G.md, committed at `f916e82d` BEFORE this chain runs,
# including before its identity legs. RULE.md is NOT edited and its
# CYCLING_PROPER verdict is untouched by anything here.
#
# THE IDENTITY GATE IS RULE-G.md section 2.1, which restates RULE.md section
# 2.1 unchanged and is enforced by the same `check_identity_m3b.py`: the
# rebuilt HVEN_TESTING binary must reproduce M3a's COMMITTED record 0 on all
# 20 QpCounters fields plus the status, against that file's BYTES. It gates
# the trace legs, which is why the identity legs run first and why a failure
# there ends the chain with no trace and no verdict.
#
# RULE-G.md section 2.2 additionally aborts on `allocation_failed`, which this
# probe writes into the trace header and which `check_identity_m3b.py` (M3b's,
# unedited) does not know about. The CLASSIFIER checks it; it is named here so
# the omission is a disclosed division of labour, not a gap.
#
# RULE.md section 2's ABORT conditions are checked by
# wrappers/check_identity_m3b.py, whose own fold proofs run FIRST in this
# transcript so that "the gate passed" is never a statement about a checker
# that compares nothing.
#
# THE LEGS, in order. The two IDENTITY legs run first and gate the two TRACE
# legs, which is M3a's chainF shape and for the same reason: the probe is a
# NEW BINARY -- it recompiles hven's own sources with HVEN_TESTING instead of
# linking an installed library -- so "this binary reproduces M3a's committed
# record 0" has to be established BEFORE the observer is switched on, or a
# later disagreement could not be attributed.
#
#   1  STEENBRB@identity-G   --trace off   gates leg 3
#   2  YORKNET@identity-G    --trace off   gates leg 4
#   3  STEENBRB@trace-G      --trace on
#   4  YORKNET@trace-G       --trace on
#
# EVERY leg: `tr_init = inf`, default options otherwise, FIRST QP ONLY (the
# driver is stopped through the public iteration callback as soon as the first
# subproblem returns), `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, pinned to one
# PHYSICAL core with `taskset -c 2`, a 300 s SOLVER deadline inside a 900 s
# OUTER `timeout`, the box lock held for the leg and RELEASED after it, a
# `pgrep` for a foreign cell binary as a SEPARATE command before the lock and
# again inside it, and `set -o pipefail`. Wall-clock is printed and is
# INFORMATIONAL ONLY (CLAUDE.md section 7); the counters and the trace are the
# currency.
#
# /home/ghecht/Projects/hven-bench is READ-ONLY -- three bridge sources are
# compiled out of it by the probe project and its CMake is never entered. The
# legs `cd` into it because that is where the decoded CUTEst problems live,
# exactly as M3a's chainF did. HOLDOUT is never run.

set -o pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3bfix1
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
M3A=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe

mkdir -p "$S/logs" "$S/out" "$S/trace"
LOG=$S/logs/chain-trace-fix1.log
exec >"$LOG" 2>&1

echo "CHAIN_TRACE_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
if [ -n "$(git status --porcelain)" ]; then
  echo "REFUSED: the tree is not clean -- a stamped leg runs on a committed tree"
  echo "CHAIN_TRACE_END $(date -Is)"
  exit 2
fi

# --------------------------------------------------------------------------
# SEGMENT 0 -- the probe: configure, build, stamp. ONE LOCK HOLD.
# The project is COPIED out of the artifact dir, so what runs is what is
# committed and the artifact dir is never a build tree.
# --------------------------------------------------------------------------
echo "PGREP_BEFORE_LOCK prepare: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3bfix1
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
echo "FOREGROUND_START prepare $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  echo "FOREGROUND_END prepare $(date -Is) rc=95"
  exit 95
fi
echo "PGREP_EMPTY prepare: no foreign cell binary under my lock"
rm -rf "$S/probe" "$S/probebuild"
mkdir -p "$S/probe"
cp "$ART/wrappers/fix1_probe.cpp" "$S/probe/fix1_probe.cpp"
cp "$ART/wrappers/probe-fix1-CMakeLists.txt" "$S/probe/CMakeLists.txt"
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ > "$S/logs/probe-configure.log" 2>&1 \
  || { echo "CONFIGURE FAILED"; tail -25 "$S/logs/probe-configure.log"; exit 92; }
echo "--- THE BUILD THIS PROBE IS: hven'"'"'s OWN SOURCES, RECOMPILED WITH HVEN_TESTING,"
echo "--- and hven::hven NOT LINKED (CLAUDE.md section 8'"'"'s seam convention)."
grep -E "fix1_probe: recompiling|fix1_probe: hven directory CMAKE_CXX_FLAGS|EIGEN_MAX_ALIGN_BYTES resolved" \
  "$S/logs/probe-configure.log"
cmake --build "$S/probebuild" --target fix1_probe -j "$(nproc)" > "$S/logs/probe-build.log" 2>&1 \
  || { echo "BUILD FAILED"; grep -E "error:|FAILED|undefined reference" "$S/logs/probe-build.log" | head -25; exit 92; }
echo "--- PROBE SHA256"
sha256sum "$S/probebuild/fix1_probe" | tee "$S/probe-sha256.txt"
echo "--- NO hven::hven IN THE LINK (the seam convention'"'"'s own property, checked)"
# FAIL-CLOSED, and FIXED IN THIS COPY. M3b'"'"'s chain grepped
# `CMakeFiles/<target>.dir/link.txt`, which only the MAKEFILE generator writes;
# under Ninja that path does not exist, `grep` exited nonzero because the FILE
# was missing rather than because the pattern was absent, and the chain printed
# a PASS. The check was vacuous. It now locates the link description wherever
# the generator put it and REFUSES when it can find none, so an absent file can
# never read as an absent link.
LINKDESC=""
for cand in "$S/probebuild/CMakeFiles/fix1_probe.dir/link.txt" "$S/probebuild/build.ninja"; do
  [ -s "$cand" ] && { LINKDESC=$cand; break; }
done
if [ -z "$LINKDESC" ]; then
  echo "REFUSED: no link description under $S/probebuild -- the no-hven-link check cannot be made"
  exit 93
fi
if [ "$(basename "$LINKDESC")" = build.ninja ]; then
  LINKLINE=$(grep -E "^build fix1_probe: " "$LINKDESC")
else
  LINKLINE=$(cat "$LINKDESC")
fi
if [ -z "$LINKLINE" ]; then
  echo "REFUSED: $LINKDESC carries no link line for fix1_probe"
  exit 93
fi
if printf "%s" "$LINKLINE" | grep -qE "libhven\.a|-lhven"; then
  echo "REFUSED: the probe links hven::hven -- the seam convention forbids it"
  exit 93
fi
echo "the link line (from $(basename "$LINKDESC"), $(printf "%s" "$LINKLINE" | wc -c) bytes) names no libhven.a and no -lhven"
echo "--- THE OBSERVER IS IN THIS BINARY (and is in no production one)"
nm -C "$S/probebuild/fix1_probe" | grep -c "QpWalkObserver" | sed "s/^/QpWalkObserver symbols: /"
echo "--- USAGE LINE"
"$S/probebuild/fix1_probe" 2>&1 | head -4
echo "FOREGROUND_END prepare $(date -Is) rc=0"
'
PREP_RC=$?
echo "WRAPPER_RC prepare rc=${PREP_RC}"
if [ "$PREP_RC" != 0 ]; then
  echo "CHAIN_TRACE_ABORTED: the probe did not build. No legs."
  echo "CHAIN_TRACE_END $(date -Is)"
  exit 3
fi

# --------------------------------------------------------------------------
# SEGMENT 1 -- the CHECKER'S OWN FOLD PROOFS, before any leg it gates.
# No lock: this runs no solver and builds nothing.
# --------------------------------------------------------------------------
echo "--- CHECKER FOLD PROOFS (RULE.md section 6's non-vacuity requirement,"
echo "--- applied to the ABORT checker; the classifier's own folds come with it)"
python3 "$ART/wrappers/check_identity_m3b.py" --selftest > "$S/out/identity-folds.txt" 2>&1
FOLD_RC=$?
tail -20 "$S/out/identity-folds.txt"
echo "IDENTITY_FOLDS_RC=${FOLD_RC}"
if [ "$FOLD_RC" != 0 ]; then
  echo "CHAIN_TRACE_ABORTED: the ABORT checker failed its own fold proofs. No legs."
  echo "CHAIN_TRACE_END $(date -Is)"
  exit 4
fi

# --------------------------------------------------------------------------
# run_leg TAG CELL TRACE_ON [extra args...] -- one lock hold, one solve.
# --------------------------------------------------------------------------
run_leg() {
  tag=$1; cell=$2; trace=$3; shift 3
  echo "PGREP_BEFORE_LOCK $tag: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3bfix1
BIN=$S/probebuild/fix1_probe
tag=$1; cell=$2; trace=$3; shift 3
echo "FOREGROUND_START leg-$tag $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED before $tag: a foreign cell binary is alive while this run holds the lock: $foreign"
  echo "FOREGROUND_END leg-$tag $(date -Is) rc=95"
  exit 95
fi
echo "PGREP_EMPTY leg-$tag: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 90
traceargs=""
if [ "$trace" = on ]; then
  traceargs="--trace on --trace-out $S/trace/$tag.csv"
else
  traceargs="--trace off"
fi
t0=$(date +%s.%N)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
  "$BIN" --problem "$cell" --tr-init inf --deadline 300 --ledger on \
         --stop-after-first-qp on $traceargs "$@" \
  > "$S/out/$tag.txt" 2>"$S/out/$tag.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-22s rc=%-3s wall_s=%.1f (INFORMATIONAL ONLY)\n" "$tag" "$rc" "$(echo "$t1 - $t0" | bc)"
w=0
if [ "$rc" = 124 ]; then
  echo "    CENSORED: the 900 s outer stop fired -- NO ROW"
  rm -f "$S/out/$tag.txt"
  w=1
elif [ "$rc" != 0 ]; then
  echo "    CENSORED: leg exited rc=$rc -- NO ROW"
  head -8 "$S/out/$tag.stderr"
  rm -f "$S/out/$tag.txt"
  w=1
else
  grep -E "^(tr_init|trace|stop_after_first_qp|stopped_after_first_qp|callback_events|deadline_fired|status|major_iters|restoration_iters|qp_minor_iters|qp_maxiter_exits|n|me|mi|ledger_qp_records|first_label|first_warm|first_qp_record_status|first_minor_iters|first_ws_adds|first_ws_drops|first_degenerate_steps|first_degenerate_run_max|first_distinct_bound_added|first_drop_ties|first_ratio_ties|trace_records|trace_truncated|trace_solves_seen|trace_exit_status|trace_incoming_ws_size|trace_written)=" \
    "$S/out/$tag.txt" | sed "s/^/    /"
  if grep -q "^deadline_fired=1$" "$S/out/$tag.txt"; then
    echo "    TRUNCATED: the 300 s solver deadline fired -- RULE.md section 2 ABORTS this leg"
    w=1
  fi
fi
echo "FOREGROUND_END leg-$tag $(date -Is) rc=$w"
exit $w
' _ "$tag" "$cell" "$trace" "$@"
  rc=$?
  echo "WRAPPER_RC leg-$tag rc=${rc}"
  return $rc
}

# gate_leg TAG CELL [TRACE_CSV] -- RULE.md section 2, against M3a's COMMITTED row.
gate_leg() {
  tag=$1; cell=$2; tracecsv=$3
  echo "--- RULE.md section 2 on $tag (M3a's committed rows/f/${cell}@F-trinf.txt is the gate)"
  if [ -n "$tracecsv" ]; then
    python3 "$ART/wrappers/check_identity_m3b.py" \
      --m3a-row "$M3A/rows/f/${cell}@F-trinf.txt" \
      --probe-row "$S/out/$tag.txt" --trace "$tracecsv" > "$S/out/gate-$tag.txt" 2>&1
  else
    python3 "$ART/wrappers/check_identity_m3b.py" \
      --m3a-row "$M3A/rows/f/${cell}@F-trinf.txt" \
      --probe-row "$S/out/$tag.txt" > "$S/out/gate-$tag.txt" 2>&1
  fi
  rc=$?
  cat "$S/out/gate-$tag.txt"
  echo "GATE_RC $tag = $rc"
  return $rc
}

# --------------------------------------------------------------------------
# THE FOUR LEGS.
# --------------------------------------------------------------------------
for spec in "STEENBRB@identity-G:STEENBRB" "YORKNET@identity-G:YORKNET"; do
  tag=${spec%%:*}; cell=${spec##*:}
  if ! run_leg "$tag" "$cell" off; then
    echo "CHAIN_TRACE_ABORTED: identity leg $tag was CENSORED or TRUNCATED. No trace legs."
    echo "CHAIN_TRACE_END $(date -Is)"
    exit 5
  fi
  if ! gate_leg "$tag" "$cell"; then
    echo "CHAIN_TRACE_ABORTED: identity leg $tag did NOT reproduce M3a's record 0."
    echo "  RULE.md section 2.1 aborts. No trace legs, and no verdict from this chain."
    echo "CHAIN_TRACE_END $(date -Is)"
    exit 6
  fi
done
echo "=== BOTH IDENTITY LEGS GREEN. The rebuilt HVEN_TESTING binary reproduces M3a's"
echo "=== committed record 0 on both cells, with no observer armed. The trace legs follow."

TRACE_RC=0
for spec in "STEENBRB@trace-G:STEENBRB" "YORKNET@trace-G:YORKNET"; do
  tag=${spec%%:*}; cell=${spec##*:}
  if ! run_leg "$tag" "$cell" on; then
    echo "CHAIN_TRACE: trace leg $tag was CENSORED or TRUNCATED."
    TRACE_RC=7
    continue
  fi
  if ! gate_leg "$tag" "$cell" "$S/trace/$tag.csv"; then
    echo "CHAIN_TRACE: trace leg $tag failed RULE.md section 2 -- its trace is NOT a measurement"
    TRACE_RC=8
    continue
  fi
  echo "    trace rows: $(( $(grep -vc '^#' "$S/trace/$tag.csv") - 1 ))  bytes: $(stat -c%s "$S/trace/$tag.csv")"
  sha256sum "$S/trace/$tag.csv"
done

echo "CHAIN_TRACE_SUMMARY prepare=$PREP_RC folds=$FOLD_RC traces=$TRACE_RC"
echo "CHAIN_TRACE_END $(date -Is)"
exit $TRACE_RC
