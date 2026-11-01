#!/usr/bin/env bash
# M7 W2 M3c -- PASS 1: the DEFAULT-PATH failure census, one leg per failing
# CUTEst TRAIN cell, at DEFAULT options.
#
# TERMS, declared here and in RULE.md section 3: one cell per process, serial,
# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to ONE physical core, and the box
# lock held PER LEG -- `flock /tmp/box-build.lock` with NO timeout and NO
# fallback -- and RELEASED between legs so another lane's cells can interleave.
# A `pgrep` for a foreign cell binary runs as its OWN command before each lock
# and again INSIDE it, printing PGREP_EMPTY when it finds nothing. Wall is
# INFORMATIONAL (CLAUDE.md section 7); the counters are the currency.
#
# THE TWO BUDGETS ARE DIFFERENT THINGS (RULE.md section 3.1).
#   * the SOLVER deadline, enforced inside the probe by the callback: 600 s on
#     the error/infeasible cells (the pin's slowest error cell spent 275 s of a
#     300 s ceiling, and a 300 s deadline would truncate it and destroy the
#     identity check), 300 s on the wall cells. A leg it stops exits
#     `interrupted` with deadline_fired=1 and is TRUNCATED: counters reported,
#     status NOT ranked.
#   * the OUTER stop, `timeout 900`, for a solve that spends its whole budget
#     inside one subproblem and emits no event the deadline could fire on. A leg
#     it kills (rc 124) is CENSORED: NO ROW, ever.
#
# MODES:
#   prepare  install hven Release from the CLEAN tree and build the probe
#            against the INSTALL (never through the harness CMake).
#   smoke    three short legs proving the binary runs, that every new block
#            prints, and that the identity gate's pinned key set matches the
#            probe: STEENBRD (a census cell that dies instantly), and SMBANK at
#            --ladder off and --ladder on.
#   census   the twenty legs. DETACHED. Group E (11 error cells) and group I
#            (YORKNET) first, then the identity gate, then group W (8 wall
#            cells) unless the gate returned RULE.md section 3.2's structural
#            bar.
#
# `prepare` and `smoke` are SEPARATE lock holds and separate foreground calls,
# so neither approaches the lane's 600 s foreground bound.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3c
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census
BENCH=/home/ghecht/Projects/hven-bench   # READ-ONLY. Never written by this chain.
# Every solver invocation below is pinned with `taskset -c 2` -- ONE physical
# core, spelled at each call site rather than through a variable, because the
# lock-held bodies are single-quoted and cannot see this scope.

MODE=${1:-census}

# RULE.md section 1's cell list, in RULE.md section 3.2's run order.
GROUP_E="ACOPP118 ACOPP57 ACOPR57 BRITGAS DALLASM NET3 STEENBRB STEENBRC STEENBRD STEENBRE STEENBRG"
GROUP_I="YORKNET"
GROUP_W="ACOPR118 BIGBANK MSS2 MSS3 READING8 SAWPATH TAX213322 TAX53322"

# --------------------------------------------------------------------------
# prepare: one lock hold -- install hven, then build the probe against it.
# --------------------------------------------------------------------------
if [ "$MODE" = prepare ]; then
  mkdir -p "$S/logs" "$S/out" "$S/smoke"
  # The pgrep check is its OWN command, before the lock is taken (box protocol).
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3c
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census
echo "FOREGROUND_START m3c-prepare $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY m3c-prepare: no foreign cell binary under my lock"
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
sha256sum "$S/install/lib64/libhven.a" | tee "$S/library-sha256.txt"
# The probe project is COPIED out of the artifact dir, so what runs is what is
# committed and the artifact dir is never a build tree.
mkdir -p "$S/probe"
cp "$ART/wrappers/m3c_probe.cpp" "$S/probe/m3c_probe.cpp"
cp "$ART/wrappers/probe-CMakeLists.txt" "$S/probe/CMakeLists.txt"
# A FRESH probe configure every time: `hven_DIR` is a cache variable that
# find_package sets on first success and then reuses -- the defect the harness
# re-pin found and this chain does not repeat.
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$S/install" \
  > "$S/logs/probe-configure.log" 2>&1 || { tail -20 "$S/logs/probe-configure.log"; exit 92; }
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m3c_probe -j "$(nproc)" > "$S/logs/probe-build.log" 2>&1 \
  || { grep -E "error:|FAILED" "$S/logs/probe-build.log" | head -20; exit 92; }
echo "--- PROBE SHA256"
sha256sum "$S/probebuild/m3c_probe" | tee "$S/probe-sha256.txt"
echo "FOREGROUND_END m3c-prepare $(date -Is) rc=0"
'
  rc=$?
  echo "WRAPPER_RC m3c-prepare rc=${rc}"
  exit $rc
fi

# --------------------------------------------------------------------------
# smoke: one lock hold, three short legs, then the key-set check.
# --------------------------------------------------------------------------
if [ "$MODE" = smoke ]; then
  mkdir -p "$S/logs" "$S/smoke"
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3c
echo "FOREGROUND_START m3c-smoke $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY m3c-smoke: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 93
echo "probe sha256: $(cat "$S/probe-sha256.txt")"
w=0
run() { # out-name, then the probe arguments
  name=$1; shift
  t0=$(date +%s.%N)
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
    "$S/probebuild/m3c_probe" "$@" > "$S/smoke/$name.txt" 2>"$S/smoke/$name.stderr"
  rc=$?
  t1=$(date +%s.%N)
  printf "SMOKE %-18s rc=%-3s wall_s=%.1f (INFORMATIONAL)  args: %s\n" "$name" "$rc" "$(echo "$t1 - $t0" | bc)" "$*"
  [ "$rc" = 0 ] || { head -5 "$S/smoke/$name.stderr"; return 1; }
  grep -E "^(problem|ladder|ledger|history_tail|deadline_fired|status|major_iters|qp_minor_iters|factorizations|x_k_source|c_field_count|h_field_count|h_rows_total|h_rows_printed|opt_tr_min|opt_qp_failure_budget|ledger_qp_records)=" \
    "$S/smoke/$name.txt" | sed "s/^/      /"
  for b in LADDER OPTIONS COUNTERS HISTORY LEDGER; do
    nb=$(grep -c "^${b}_BEGIN$" "$S/smoke/$name.txt"); ne=$(grep -c "^${b}_END$" "$S/smoke/$name.txt")
    [ "$nb" = 1 ] && [ "$ne" = 1 ] || { echo "      BLOCK $b: ${nb} BEGIN / ${ne} END -- expected 1 and 1"; return 1; }
  done
  grep -q "^PROBE_END$" "$S/smoke/$name.txt" || { echo "      NO PROBE_END -- the block is truncated"; return 1; }
  return 0
}
run STEENBRD --problem STEENBRD --ladder off --ledger on --deadline 600 || w=1
run SMBANK-ladder-off --problem SMBANK --ladder off --ledger on --deadline 600 || w=1
run SMBANK-ladder-on  --problem SMBANK --ladder on  --ledger on --deadline 600 || w=1
echo "FOREGROUND_END m3c-smoke $(date -Is) rc=$w"
exit $w
'
  rc=$?
  echo "WRAPPER_RC m3c-smoke rc=${rc}"
  [ "$rc" = 0 ] || exit $rc
  # THE KEY-SET CHECK, outside the lock (pure file folding, no solver): the
  # identity gate's pinned key set against the probe's own output, on a real
  # census cell. If this disagrees the LITERAL is wrong and is fixed BEFORE any
  # census leg runs -- never after a leg has been read.
  echo "--- IDENTITY GATE on the smoke's STEENBRD row (RULE.md section 4)"
  python3 -B "$ART/wrappers/identity_m3c.py" --rows "$S/smoke" --cells STEENBRD \
    | tee "$S/logs/smoke-identity.txt"
  echo "SMOKE_IDENTITY_RC=${PIPESTATUS[0]}"
  exit 0
fi

# --------------------------------------------------------------------------
# census: twenty legs, ONE LOCK HOLD PER LEG.
# --------------------------------------------------------------------------
if [ "$MODE" != census ]; then
  echo "usage: chain_census.sh [prepare|smoke|census]"
  exit 2
fi

mkdir -p "$S/logs" "$S/out"
LOG=$S/logs/chain-census.log
exec >"$LOG" 2>&1
echo "CHAIN_CENSUS_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
echo "=== probe sha256:   $(cat "$S/probe-sha256.txt" 2>/dev/null)"
echo "=== library sha256: $(cat "$S/library-sha256.txt" 2>/dev/null)"
if [ ! -x "$S/probebuild/m3c_probe" ]; then
  echo "CHAIN_ABORTED: no probe at $S/probebuild/m3c_probe -- run the prepare mode first"
  echo "CHAIN_CENSUS_END $(date -Is)"
  exit 90
fi

# run_leg CELL DEADLINE -- one lock hold, one solve, one row file.
# Returns 0 on a finished or TRUNCATED leg, 1 on CENSORED/failed. Truncation is
# NOT a failure here: it is a row shape, and RULE.md section 3.1 says how to
# read it.
run_leg() {
  cell=$1; dl=$2
  # The pgrep check is its OWN command, outside the lock, as the box protocol
  # requires; it is repeated INSIDE the lock, where it is the one that binds.
  echo "PGREP_BEFORE_LOCK $cell: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3c
BIN=$S/probebuild/m3c_probe
cell=$1; dl=$2
echo "FOREGROUND_START leg-$cell $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED before $cell: a foreign cell binary is alive while this run holds the lock: $foreign"
  echo "FOREGROUND_END leg-$cell $(date -Is) rc=95"
  exit 95
fi
echo "PGREP_EMPTY leg-$cell: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 90
t0=$(date +%s.%N)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
  "$BIN" --problem "$cell" --deadline "$dl" --ledger on --ladder off \
  > "$S/out/$cell.txt" 2>"$S/out/$cell.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-10s deadline=%-5s rc=%-3s wall_s=%.1f (INFORMATIONAL)\n" "$cell" "$dl" "$rc" "$(echo "$t1 - $t0" | bc)"
w=0
if [ "$rc" = 124 ]; then
  echo "    CENSORED: the 900 s outer stop fired -- NO ROW, never filled (RULE.md section 3.1)"
  rm -f "$S/out/$cell.txt"
  w=1
elif [ "$rc" != 0 ]; then
  echo "    CENSORED: leg exited rc=$rc -- NO ROW"
  head -5 "$S/out/$cell.stderr"
  rm -f "$S/out/$cell.txt"
  w=1
else
  grep -E "^(status|deadline_fired|major_iters|qp_minor_iters|factorizations|f|feasibility_e|feasibility_i|steps_accepted|rejected_steps|qp_maxiter_exits|qp_numerical_exits|qp_failures_routed|qp_failures_in_a_row_peak|restoration_entries|restoration_sub_status|elastic_activations|history_rows|ledger_qp_records)=" \
    "$S/out/$cell.txt" | sed "s/^/    /"
  if grep -q "^deadline_fired=1$" "$S/out/$cell.txt"; then
    echo "    TRUNCATED: the solver deadline fired -- counters reported, status UNRANKED"
  fi
fi
echo "FOREGROUND_END leg-$cell $(date -Is) rc=$w"
exit $w
' _ "$cell" "$dl"
  rc=$?
  echo "WRAPPER_RC leg-$cell rc=${rc}"
  return $rc
}

censored=0
# --- group E, then group I: the finishing cells, at a 600 s solver deadline --
for cell in $GROUP_E $GROUP_I; do
  run_leg "$cell" 600 || censored=$((censored + 1))
done

# --- the identity gate, OUTSIDE the lock (pure file folding, no solver) ------
echo "--- IDENTITY GATE over groups E and I (RULE.md section 4)"
EI_CELLS=$(echo $GROUP_E $GROUP_I | tr ' ' ',')
python3 -B "$ART/wrappers/identity_m3c.py" --rows "$S/out" --cells "$EI_CELLS" --bar \
  | tee "$S/out/identity-EI.txt"
id_rc=${PIPESTATUS[0]}
echo "IDENTITY_RC=$id_rc  (0 all identical, 1 differences are FINDINGS, 2 structural refusal, 3 RULE.md 3.2's bar)"
if [ "$id_rc" = 2 ] || [ "$id_rc" = 3 ]; then
  echo "CHAIN_ABORTED before group W: the identity gate reports an INSTRUMENT failure,"
  echo "  not a finding (RULE.md section 3.2). No wall-cell leg runs, because a row taken"
  echo "  with an instrument that reproduces no committed cell could not be believed."
  echo "CHAIN_CENSUS_END $(date -Is)"
  exit 5
fi

# --- group W: the wall cells, at a 300 s solver deadline --------------------
for cell in $GROUP_W; do
  run_leg "$cell" 300 || censored=$((censored + 1))
done

echo "--- IDENTITY GATE over all twenty (group W reports NOT COMPARABLE by construction)"
ALL_CELLS=$(echo $GROUP_E $GROUP_I $GROUP_W | tr ' ' ',')
python3 -B "$ART/wrappers/identity_m3c.py" --rows "$S/out" --cells "$ALL_CELLS" \
  | tee "$S/out/identity-all.txt"
echo "IDENTITY_ALL_RC=${PIPESTATUS[0]}"

echo "CENSORED_LEGS=$censored"
if [ "$censored" != 0 ]; then
  echo "CHAIN_INCOMPLETE: $censored leg(s) were CENSORED -- their rows were never filled,"
  echo "  so the census has a hole in it. The rows that DO exist stand."
  echo "CHAIN_CENSUS_END $(date -Is)"
  exit 6
fi
echo "CHAIN_CENSUS_END $(date -Is)"
exit 0
