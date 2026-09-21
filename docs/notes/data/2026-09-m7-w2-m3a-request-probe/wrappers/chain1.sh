#!/usr/bin/env bash
# M7 W2 M3a -- STAGE 1: the request probe's radius ladder on STEENBRB and
# YORKNET, at zeta = 0, over tr_init in {1 (control), 10, 1e2, 1e3, 1e4, +inf}.
#
# TERMS, declared here and repeated in RULE.md section 6: one cell per process,
# serial, MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to one physical core, and
# the box lock held PER LEG -- `flock /tmp/box-build.lock` with no timeout and no
# fallback -- and RELEASED between legs so another lane's cells can interleave.
# A `pgrep` for a foreign cell binary runs as its own command INSIDE each lock
# hold, before the solver starts, and prints PGREP_EMPTY when it finds nothing.
# Wall is INFORMATIONAL (CLAUDE.md section 7); the counters are the currency.
#
# THE TWO BUDGETS ARE DIFFERENT THINGS (RULE.md section 6).
#   * 300 s SOLVER deadline, enforced inside the probe by the callback. A leg it
#     stops exits `interrupted` with deadline_fired=1 and is TRUNCATED: its
#     counters are reported, its status is not ranked.
#   * 900 s OUTER stop, `timeout 900` around the process, for a solve that
#     spends its whole budget inside one subproblem and emits no event the
#     deadline could fire on. A leg it kills (rc 124) is CENSORED: NO ROW, ever.
#
# THE CONTROL LEGS GATE THE LADDER. The two tr_init = 1 legs run first and are
# checked against M2b's committed screen rows by identity_m3a.py (RULE.md
# section 4). If that check fails, or if either control is TRUNCATED or
# CENSORED, this chain ABORTS and no raised-radius leg runs.
#
# MODES:
#   prepare  install hven Release from the clean tree and build the probe
#            against the INSTALL (never through the harness CMake).
#   smoke    two short legs that prove the binary runs and prints the new keys:
#            ZAMB2-8 at the shipped tr_init in full, and ZAMB2-8 at +inf capped
#            at one major -- enough to prove the `inf` argument parses all the
#            way into SqpOptions without buying a second full solve.
#   stage1   the twelve legs. DETACHED.
#
# `prepare` and `smoke` are SEPARATE lock holds, and separate foreground calls,
# so neither approaches the lane's 600 s foreground bound.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
BENCH=/home/ghecht/Projects/hven-bench   # READ-ONLY. Never written by this chain.
# Every solver invocation below is pinned with `taskset -c 2` -- ONE physical
# core, spelled at each call site rather than through a variable, because the
# lock-held bodies are single-quoted and cannot see this scope.

MODE=${1:-stage1}

# The ladder, pre-registered in RULE.md section 3. tag:value, in run order.
CONTROL="tr1:1"
LADDER="tr1e1:10 tr1e2:1e2 tr1e3:1e3 tr1e4:1e4 trinf:inf"
CELLS="STEENBRB YORKNET"

# --------------------------------------------------------------------------
# prepare: one lock hold -- install hven, then build the probe against it.
# --------------------------------------------------------------------------
if [ "$MODE" = prepare ]; then
  mkdir -p "$S/logs" "$S/out/stage1"
  # The pgrep check is its OWN command, before the lock is taken (box protocol).
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
BENCH=/home/ghecht/Projects/hven-bench
echo "FOREGROUND_START m3a-prepare $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY m3a-prepare: no foreign cell binary under my lock"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean -- a stamped leg runs on a committed tree"; exit 2; }
rm -rf "$S/rel" "$S/install" "$S/probe" "$S/probebuild"
cmake -S "$ROOT" -B "$S/rel" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_INSTALL_PREFIX="$S/install" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF > "$S/logs/install-configure.log" 2>&1 || exit 91
cmake --build "$S/rel" -j "$(nproc)" > "$S/logs/install-build.log" 2>&1 \
  || { tail -30 "$S/logs/install-build.log"; exit 91; }
cmake --install "$S/rel" > "$S/logs/install.log" 2>&1 || exit 91
echo "--- LIBRARY SHA256 (Fedora installs to lib64)"
sha256sum "$S/install/lib64/libhven.a"
echo "--- THE FIELD THIS PROBE EXISTS TO READ, in the INSTALLED header"
grep -n "restoration_sub_status" "$S/install/include/hven/core/solver_counters.h" | head -2
# The probe project is COPIED out of the artifact dir, so what runs is what is
# committed and the artifact dir is never a build tree.
mkdir -p "$S/probe"
cp "$ART/wrappers/m3a_probe.cpp" "$S/probe/m3a_probe.cpp"
cp "$ART/wrappers/probe-CMakeLists.txt" "$S/probe/CMakeLists.txt"
# A FRESH probe configure every time: `hven_DIR` is a cache variable that
# find_package sets on first success and then reuses.
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$S/install" \
  > "$S/logs/probe-configure.log" 2>&1 || { tail -20 "$S/logs/probe-configure.log"; exit 92; }
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m3a_probe -j "$(nproc)" > "$S/logs/probe-build.log" 2>&1 \
  || { grep -E "error:|FAILED" "$S/logs/probe-build.log" | head -20; exit 92; }
echo "--- PROBE SHA256"
sha256sum "$S/probebuild/m3a_probe" | tee "$S/probe-sha256.txt"
echo "FOREGROUND_END m3a-prepare $(date -Is) rc=0"
'
  rc=$?
  echo "WRAPPER_RC m3a-prepare rc=${rc}"
  exit $rc
fi

# --------------------------------------------------------------------------
# smoke: one lock hold, two short legs. Proves the binary runs, that `inf`
# parses through to SqpOptions, and that every new key prints.
# --------------------------------------------------------------------------
if [ "$MODE" = smoke ]; then
  mkdir -p "$S/logs"
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3a
echo "FOREGROUND_START m3a-smoke $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY m3a-smoke: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 93
echo "probe sha256: $(sha256sum "$S/probebuild/m3a_probe")"
w=0
run() { # tag, then the probe arguments
  tag=$1; shift
  t0=$(date +%s.%N)
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
    "$S/probebuild/m3a_probe" "$@" > "$S/logs/smoke-$tag.txt" 2>&1
  rc=$?
  t1=$(date +%s.%N)
  printf "SMOKE %-8s rc=%-3s wall_s=%.1f (INFORMATIONAL)  args: %s\n" "$tag" "$rc" "$(echo "$t1 - $t0" | bc)" "$*"
  [ "$rc" = 0 ] || { head -5 "$S/logs/smoke-$tag.txt"; return 1; }
  grep -E "^(problem|proximity|tr_init|deadline_seconds|deadline_fired|status|major_iters|restoration_entries|restoration_sub_status|elastic_from_ipqp_escape|x_k_source|history_rows|first_qp_)" \
    "$S/logs/smoke-$tag.txt" | sed "s/^/      /"
  grep -q "^PROBE_END$" "$S/logs/smoke-$tag.txt" || { echo "      NO PROBE_END -- the block is truncated"; return 1; }
  return 0
}
run tr1   --problem ZAMB2-8 --tr-init 1   || w=1
run trinf --problem ZAMB2-8 --tr-init inf --budget-iters 1 || w=1
echo "FOREGROUND_END m3a-smoke $(date -Is) rc=$w"
exit $w
'
  rc=$?
  echo "WRAPPER_RC m3a-smoke rc=${rc}"
  exit $rc
fi

# --------------------------------------------------------------------------
# stage1: twelve legs, ONE LOCK HOLD PER LEG.
# --------------------------------------------------------------------------
if [ "$MODE" != stage1 ]; then
  echo "usage: chain1.sh [prepare|smoke|stage1]"
  exit 2
fi

mkdir -p "$S/logs" "$S/out/stage1"
LOG=$S/logs/chain1.log
exec >"$LOG" 2>&1
echo "CHAIN1_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
echo "=== probe sha256 recorded at prepare: $(cat "$S/probe-sha256.txt" 2>/dev/null)"

# run_leg CELL TAG TR_INIT -- one lock hold, one solve, one row file.
# Returns 0 on a finished leg, 1 on CENSORED/failed. TRUNCATION is NOT a
# failure here: it is a row shape, and RULE.md section 6 says how to read it.
run_leg() {
  cell=$1; tag=$2; r=$3
  # The pgrep check is its OWN command, outside the lock, as the box protocol
  # requires; it is repeated INSIDE the lock, where it is the one that binds.
  echo "PGREP_BEFORE_LOCK $cell@$tag: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3a
BIN=$S/probebuild/m3a_probe
cell=$1; tag=$2; r=$3
echo "FOREGROUND_START leg-$cell@$tag $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED before $cell@$tag: a foreign cell binary is alive while this run holds the lock: $foreign"
  echo "FOREGROUND_END leg-$cell@$tag $(date -Is) rc=95"
  exit 95
fi
echo "PGREP_EMPTY leg-$cell@$tag: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 90
t0=$(date +%s.%N)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
  "$BIN" --problem "$cell" --tr-init "$r" --deadline 300 \
  > "$S/out/stage1/$cell@$tag.txt" 2>"$S/out/stage1/$cell@$tag.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-10s %-14s rc=%-3s wall_s=%.1f (INFORMATIONAL)\n" "$cell" "tr_init=$r" "$rc" "$(echo "$t1 - $t0" | bc)"
w=0
if [ "$rc" = 124 ]; then
  echo "    CENSORED: the 900 s outer stop fired -- NO ROW, never filled (RULE.md section 6)"
  rm -f "$S/out/stage1/$cell@$tag.txt"
  w=1
elif [ "$rc" != 0 ]; then
  echo "    CENSORED: leg exited rc=$rc -- NO ROW"
  head -5 "$S/out/stage1/$cell@$tag.stderr"
  rm -f "$S/out/stage1/$cell@$tag.txt"
  w=1
else
  grep -E "^(tr_init|deadline_fired|status|infeasibility_certified|major_iters|restoration_iters|qp_minor_iters|factorizations|restoration_entries|restoration_sub_status|restoration_h_in|restoration_h_out|elastic_from_ipqp_escape|x_k_source|history_rows|first_qp_status|first_qp_elastic_applied|first_qp_plain_status|feasibility_e|feasibility_i)=" \
    "$S/out/stage1/$cell@$tag.txt" | sed "s/^/    /"
  if grep -q "^deadline_fired=1$" "$S/out/stage1/$cell@$tag.txt"; then
    echo "    TRUNCATED: the 300 s solver deadline fired -- counters reported, status UNRANKED"
  fi
fi
echo "FOREGROUND_END leg-$cell@$tag $(date -Is) rc=$w"
exit $w
' _ "$cell" "$tag" "$r"
  rc=$?
  echo "WRAPPER_RC leg-$cell@$tag rc=${rc}"
  return $rc
}

# --- the two control legs, first -----------------------------------------
control_bad=0
for cell in $CELLS; do
  run_leg "$cell" "${CONTROL%%:*}" "${CONTROL#*:}" || control_bad=1
done
if [ $control_bad -ne 0 ]; then
  echo "CHAIN1_ABORTED: a CONTROL leg was CENSORED. No control, no ladder (RULE.md sections 3, 6)."
  echo "CHAIN1_END $(date -Is)"
  exit 3
fi
for cell in $CELLS; do
  if grep -q "^deadline_fired=1$" "$S/out/stage1/$cell@tr1.txt"; then
    echo "CHAIN1_ABORTED: control leg $cell was TRUNCATED at the 300 s deadline (RULE.md section 6)."
    echo "CHAIN1_END $(date -Is)"
    exit 4
  fi
done

# --- the identity gate, OUTSIDE the lock (pure file folding, no solver) ---
echo "--- CONTROL IDENTITY (RULE.md section 4)"
python3 -B "$ART/wrappers/identity_m3a.py" | tee "$S/out/identity-stage1.txt"
id_rc=${PIPESTATUS[0]}
echo "IDENTITY_RC=$id_rc"
if [ "$id_rc" != 0 ]; then
  echo "CHAIN1_ABORTED: the control legs do not reproduce M2b's screen rows."
  echo "  The instrument is not measuring what M2b measured, so no radius row taken"
  echo "  with it could be believed. No raised-radius leg runs (RULE.md section 4)."
  echo "CHAIN1_END $(date -Is)"
  exit 5
fi

# --- the ladder ----------------------------------------------------------
worst=0
for cell in $CELLS; do
  for spec in $LADDER; do
    run_leg "$cell" "${spec%%:*}" "${spec#*:}" || worst=1
  done
done

echo "LEGS_WORST=$worst  (1 means at least one leg was CENSORED; see its LEG line)"
echo "CHAIN1_END $(date -Is)"
exit 0
