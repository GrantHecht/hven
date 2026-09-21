#!/usr/bin/env bash
# M7 W2 M3a ADDENDUM E -- THE QP-CAP EXPERIMENTS: four legs on STEENBRB and
# YORKNET at tr_init = inf, zeta = 0, one option moved off its default per arm.
#
#   E1  --qp-cap-retry on        SqpOptions::qp_cap_retry, default false
#   E2  --qp-max-iter N          SqpOptions::qp.max_iter, default 0 (the
#                                size-derived sentinel); N = 46800 on STEENBRB,
#                                25600 on YORKNET -- 10x the per-major minor
#                                quotient OBSERVED in stage 1, fixed in
#                                RULE-E.md section 1 before any leg.
#
# TERMS, declared here and in RULE-E.md section 6: one cell per process, serial,
# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to one physical core, and the box
# lock held PER LEG -- `flock /tmp/box-build.lock` with no timeout and no
# fallback -- and RELEASED between legs. A `pgrep` for a foreign cell binary runs
# as its own command before each hold and again INSIDE it, where it is the one
# that binds. Wall is INFORMATIONAL (CLAUDE.md section 7); counters are the
# currency.
#
# THE TWO BUDGETS ARE DIFFERENT THINGS (RULE-E.md section 6).
#   * 300 s SOLVER deadline, enforced inside the probe by the callback. A leg it
#     stops exits `interrupted` with deadline_fired=1 and is TRUNCATED: its
#     counters are reported and it yields NO VERDICT.
#   * 900 s OUTER stop, `timeout 900` around the process. A leg it kills (rc
#     124) is CENSORED: NO ROW, ever.
#   E2's raised cap makes TRUNCATION likelier than in stage 1. That is a
#   reported outcome, not a reason to widen the budget mid-run.
#
# THE IDENTITY LEG GATES THE FOUR. The rebuilt probe first runs
# `STEENBRB --tr-init inf` with BOTH new options at their defaults and is checked
# against the COMMITTED stage-1 row by identity_e.py (RULE-E.md section 3). If
# that check fails, or if the identity leg is CENSORED or TRUNCATED, this chain
# ABORTS and no E leg runs.
#
# THE CONTROLS ARE NOT RE-RUN. RULE-E.md section 2: each cell's control is its
# own committed `rows/stage1/<CELL>@trinf.txt`, landed at b663b834.
#
# MODES:
#   prepare  rebuild the probe against the SAME hven install stage 1 used. The
#            library is NOT rebuilt and NOT reinstalled -- its sha256 is
#            re-checked against the value RULE-E.md section 7 records, and a
#            mismatch aborts.
#   stageE   the identity leg, its gate, then the four E legs. DETACHED.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
BENCH=/home/ghecht/Projects/hven-bench   # READ-ONLY. Never written by this chain.
# The hven install stage 1 measured on. RULE-E.md section 7 pins it by sha256.
LIBSHA=c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3

MODE=${1:-stageE}

# The four legs, pre-registered in RULE-E.md section 1. tag:cell:extra-args.
LEGS="STEENBRB@E1:STEENBRB:--qp-cap-retry on
YORKNET@E1:YORKNET:--qp-cap-retry on
STEENBRB@E2:STEENBRB:--qp-max-iter 46800
YORKNET@E2:YORKNET:--qp-max-iter 25600"

# --------------------------------------------------------------------------
# prepare: one lock hold -- rebuild the probe against the EXISTING install.
# --------------------------------------------------------------------------
if [ "$MODE" = prepare ]; then
  mkdir -p "$S/logs" "$S/out/e"
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
LIBSHA=c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3
echo "FOREGROUND_START e-prepare $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY e-prepare: no foreign cell binary under my lock"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean -- a stamped leg runs on a committed tree"; exit 2; }
echo "--- THE LIBRARY IS NOT REBUILT. Stage 1 measured on this install; the E legs"
echo "--- must run on the SAME one, so its sha256 is re-checked, not re-made."
have=$(sha256sum "$S/install/lib64/libhven.a" | cut -d" " -f1)
echo "libhven.a sha256: $have"
[ "$have" = "$LIBSHA" ] || { echo "REFUSED: the install moved -- expected $LIBSHA"; exit 94; }
# The probe project is COPIED out of the artifact dir, so what runs is what is
# committed and the artifact dir is never a build tree.
rm -rf "$S/probe" "$S/probebuild"
mkdir -p "$S/probe"
cp "$ART/wrappers/m3a_probe.cpp" "$S/probe/m3a_probe.cpp"
cp "$ART/wrappers/probe-CMakeLists.txt" "$S/probe/CMakeLists.txt"
cmake -S "$S/probe" -B "$S/probebuild" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$S/install" \
  > "$S/logs/e-probe-configure.log" 2>&1 || { tail -20 "$S/logs/e-probe-configure.log"; exit 92; }
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m3a_probe -j "$(nproc)" > "$S/logs/e-probe-build.log" 2>&1 \
  || { grep -E "error:|FAILED" "$S/logs/e-probe-build.log" | head -20; exit 92; }
echo "--- PROBE SHA256 (differs from stage 1 BY CONSTRUCTION; the identity leg is the proof it is inert at the defaults)"
sha256sum "$S/probebuild/m3a_probe" | tee "$S/probe-e-sha256.txt"
echo "--- THE TWO NEW ARGUMENTS ARE PARSED (usage line)"
"$S/probebuild/m3a_probe" 2>&1 | head -3
echo "FOREGROUND_END e-prepare $(date -Is) rc=0"
'
  rc=$?
  echo "WRAPPER_RC e-prepare rc=${rc}"
  exit $rc
fi

# --------------------------------------------------------------------------
# stageE: the identity leg, its gate, then four legs. ONE LOCK HOLD PER LEG.
# --------------------------------------------------------------------------
if [ "$MODE" != stageE ]; then
  echo "usage: chainE.sh [prepare|stageE]"
  exit 2
fi

mkdir -p "$S/logs" "$S/out/e"
LOG=$S/logs/chainE.log
exec >"$LOG" 2>&1
echo "CHAINE_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
echo "=== probe sha256 recorded at prepare: $(cat "$S/probe-e-sha256.txt" 2>/dev/null)"
echo "=== library sha256: $(sha256sum "$S/install/lib64/libhven.a" 2>/dev/null)"

# run_leg TAG CELL [extra probe args...] -- one lock hold, one solve, one row.
# Returns 0 on a finished leg, 1 on CENSORED/failed. TRUNCATION is NOT a failure
# here: it is a row shape, and RULE-E.md section 5 says how to read it.
run_leg() {
  tag=$1; cell=$2; shift 2
  echo "PGREP_BEFORE_LOCK $tag: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
S=/home/ghecht/Projects/hven/.scratch/m7w2m3a
BIN=$S/probebuild/m3a_probe
tag=$1; cell=$2; shift 2
echo "FOREGROUND_START leg-$tag $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED before $tag: a foreign cell binary is alive while this run holds the lock: $foreign"
  echo "FOREGROUND_END leg-$tag $(date -Is) rc=95"
  exit 95
fi
echo "PGREP_EMPTY leg-$tag: no foreign cell binary under my lock"
cd /home/ghecht/Projects/hven-bench || exit 90
t0=$(date +%s.%N)
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
  "$BIN" --problem "$cell" --tr-init inf --deadline 300 "$@" \
  > "$S/out/e/$tag.txt" 2>"$S/out/e/$tag.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-20s rc=%-3s wall_s=%.1f (INFORMATIONAL)  extra: %s\n" "$tag" "$rc" "$(echo "$t1 - $t0" | bc)" "$*"
w=0
if [ "$rc" = 124 ]; then
  echo "    CENSORED: the 900 s outer stop fired -- NO ROW, never filled (RULE-E.md section 5)"
  rm -f "$S/out/e/$tag.txt"
  w=1
elif [ "$rc" != 0 ]; then
  echo "    CENSORED: leg exited rc=$rc -- NO ROW"
  head -5 "$S/out/e/$tag.stderr"
  rm -f "$S/out/e/$tag.txt"
  w=1
else
  grep -E "^(qp_cap_retry|qp_max_iter|deadline_fired|status|infeasibility_certified|major_iters|restoration_iters|qp_minor_iters|factorizations|symbolic_analyses|border_refine_steps|steps_accepted|rejected_steps|elastic_activations|qp_maxiter_exits|qp_numerical_exits|qp_failures_routed|qp_failures_in_a_row_peak|qp_cap_retries|restoration_entries|restoration_sub_status|x_k_source|history_rows|first_qp_|feasibility_e|feasibility_i)=" \
    "$S/out/e/$tag.txt" | sed "s/^/    /"
  if grep -q "^deadline_fired=1$" "$S/out/e/$tag.txt"; then
    echo "    TRUNCATED: the 300 s solver deadline fired -- counters reported, NO VERDICT"
  fi
fi
echo "FOREGROUND_END leg-$tag $(date -Is) rc=$w"
exit $w
' _ "$tag" "$cell" "$@"
  rc=$?
  echo "WRAPPER_RC leg-$tag rc=${rc}"
  return $rc
}

# --- the identity leg, first ---------------------------------------------
# BOTH new options at their defaults: no --qp-cap-retry, no --qp-max-iter.
if ! run_leg "STEENBRB@identity" "STEENBRB"; then
  echo "CHAINE_ABORTED: the identity leg was CENSORED. No gate, no E legs (RULE-E.md sections 3, 5)."
  echo "CHAINE_END $(date -Is)"
  exit 3
fi
if grep -q "^deadline_fired=1$" "$S/out/e/STEENBRB@identity.txt"; then
  echo "CHAINE_ABORTED: the identity leg was TRUNCATED at the 300 s deadline -- it cannot"
  echo "  prove the rebuilt binary reproduces a row that ran to completion (RULE-E.md section 3)."
  echo "CHAINE_END $(date -Is)"
  exit 4
fi

# --- the identity gate, OUTSIDE the lock (pure file folding, no solver) ---
echo "--- IDENTITY ON THE REBUILT PROBE (RULE-E.md section 3)"
python3 -B "$ART/wrappers/identity_e.py" | tee "$S/out/identity-e.txt"
id_rc=${PIPESTATUS[0]}
echo "IDENTITY_E_RC=$id_rc"
if [ "$id_rc" != 0 ]; then
  echo "CHAINE_ABORTED: the rebuilt probe does not reproduce the committed stage-1 row"
  echo "  at both new options' defaults. The two new arguments are not inert, so no E row"
  echo "  taken with this binary could be believed. No E leg runs (RULE-E.md section 3)."
  echo "CHAINE_END $(date -Is)"
  exit 5
fi

# --- the four E legs ------------------------------------------------------
worst=0
echo "$LEGS" | while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  tag=${spec%%:*}
  rest=${spec#*:}
  cell=${rest%%:*}
  extra=${rest#*:}
  # `extra` is one flag and one value, both whitespace-free by construction
  # (RULE-E.md section 1 fixes them), so word splitting here is exact.
  # shellcheck disable=SC2086
  run_leg "$tag" "$cell" $extra || echo "LEG_CENSORED $tag"
done
# The loop above runs in a subshell (pipeline), so `worst` is recomputed here
# from the rows that actually exist rather than carried out of it.
for spec in "STEENBRB@E1" "YORKNET@E1" "STEENBRB@E2" "YORKNET@E2"; do
  [ -f "$S/out/e/$spec.txt" ] || { echo "MISSING ROW: $spec (CENSORED)"; worst=1; }
done

echo "LEGS_E_WORST=$worst  (1 means at least one leg was CENSORED; see its LEG line)"
# fix1 (settler adjudication R2): this chain used to RECORD `worst` and then
# exit 0 regardless, so a censored E leg was visible in the log and invisible to
# anything reading the exit status. A recorded failure that does not reach the
# caller is not a reported failure. It does now.
if [ "$worst" != 0 ]; then
  echo "CHAINE_INCOMPLETE: at least one E leg was CENSORED -- its row was never filled,"
  echo "  so the four-leg E table this chain was run to produce has a hole in it. The"
  echo "  identity gate passed; the rows that DO exist stand."
  echo "CHAINE_END $(date -Is)"
  exit 6
fi
echo "CHAINE_END $(date -Is)"
exit 0
