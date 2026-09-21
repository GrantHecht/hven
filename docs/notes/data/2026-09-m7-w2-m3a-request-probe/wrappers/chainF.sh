#!/usr/bin/env bash
# M7 W2 M3a ADDENDUM F -- THE DEGENERATE-STALL PROBE: four measured legs on
# STEENBRB and YORKNET at tr_init in {1, inf}, zeta = 0, NO SOLVER OPTION OFF ITS
# DEFAULT, with hven's PUBLIC `Ledger` attached so the per-QP-solve `QpCounters`
# -- degenerate_steps, degenerate_run_max, ratio_ties, drop_ties, schur_updates
# and the other fifteen -- can be read at all.
#
# H-DEG (RULE-F.md, committed BEFORE any leg): the first QP's tens of thousands
# of minors at tr_init = inf are a DEGENERATE STALL of the active-set walk, not a
# long genuine walk. hven documents the mechanism and documents that it has no
# remedy -- "No anti-cycling rule is implemented; a degenerate stall is bounded
# by max_iter" (detail/qp/qp_engine.h's Degeneracy note).
#
# TERMS, declared here and in RULE-F.md section 5: one cell per process, serial,
# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to one physical core, and the box
# lock held PER LEG -- `flock /tmp/box-build.lock` with no timeout and no
# fallback -- and RELEASED between legs. A `pgrep` for a foreign cell binary runs
# as its own command before each hold and again INSIDE it, where it is the one
# that binds. Wall is INFORMATIONAL (CLAUDE.md section 7); counters are the
# currency.
#
# THE TWO BUDGETS ARE DIFFERENT THINGS (RULE-F.md section 5).
#   * 300 s SOLVER deadline, enforced inside the probe by the callback. A leg it
#     stops exits `interrupted` with deadline_fired=1 and is TRUNCATED: its
#     counters and its ledger records are reported and it yields NO VERDICT, and
#     its neutrality check is WITHHELD.
#   * 900 s OUTER stop, `timeout 900` around the process. A leg it kills (rc
#     124) is CENSORED: NO ROW, ever.
#
# TWO GATES, NOT ONE (RULE-F.md section 3).
#   IDENTITY    the rebuilt probe at `--ledger off` must reproduce the committed
#               stage-1 @trinf row. A failure means the new argument is not inert
#               at its default and no F row could be believed. ABORTS.
#   NEUTRALITY  the FIRST `--ledger on` leg must equal its own committed stage-1
#               twin on every shared key. If a solver counter moves with the
#               ledger ATTACHED, that is a finding ABOUT hven and the addendum
#               STOPS -- so the gate runs BEFORE the other three legs rather than
#               as a note appended to a completed run.
#
# THE CONTROLS ARE NOT RE-RUN. RULE-F.md section 2: each measured leg's control
# is its own committed `rows/stage1/<CELL>@{tr1,trinf}.txt`, landed at b663b834.
#
# MODES:
#   prepare  rebuild the probe against the SAME hven install stages 1 and E used.
#            The library is NOT rebuilt and NOT reinstalled -- its sha256 is
#            re-checked against the value RULE-F.md section 3 records, and a
#            mismatch aborts.
#   stageF   the identity leg, its gate, the first measured leg, ITS gate, then
#            the remaining three and the full neutrality fold. DETACHED.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
BENCH=/home/ghecht/Projects/hven-bench   # READ-ONLY. Never written by this chain.
# The hven install stages 1 and E measured on. RULE-F.md section 3 pins it by sha256.
LIBSHA=c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3

MODE=${1:-stageF}

# The three legs that follow the gated one, pre-registered in RULE-F.md section
# 2. tag:cell:extra-args. `STEENBRB@F-trinf` is NOT here: it runs first, alone,
# because its neutrality check gates these three.
LEGS="STEENBRB@F-tr1:STEENBRB:--tr-init 1 --ledger on
YORKNET@F-trinf:YORKNET:--tr-init inf --ledger on
YORKNET@F-tr1:YORKNET:--tr-init 1 --ledger on"

# --------------------------------------------------------------------------
# prepare: one lock hold -- rebuild the probe against the EXISTING install.
# --------------------------------------------------------------------------
if [ "$MODE" = prepare ]; then
  mkdir -p "$S/logs" "$S/out/f"
  echo "PGREP_BEFORE_LOCK: $(pgrep -x hven_bench_cell | tr '\n' ' ')"
  flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3a
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3a-request-probe
LIBSHA=c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3
echo "FOREGROUND_START f-prepare $(date -Is)"
foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
if [ -n "$foreign" ]; then
  echo "ABORTED: a foreign cell binary is alive while this run holds the lock: $foreign"
  exit 95
fi
echo "PGREP_EMPTY f-prepare: no foreign cell binary under my lock"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean -- a stamped leg runs on a committed tree"; exit 2; }
echo "--- THE LIBRARY IS NOT REBUILT. Stages 1 and E measured on this install; the F"
echo "--- legs must run on the SAME one, so its sha256 is re-checked, not re-made."
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
  > "$S/logs/f-probe-configure.log" 2>&1 || { tail -20 "$S/logs/f-probe-configure.log"; exit 92; }
echo "--- THE hven PACKAGE THIS PROBE RESOLVED"
grep -m1 "^hven_DIR:" "$S/probebuild/CMakeCache.txt"
cmake --build "$S/probebuild" --target m3a_probe -j "$(nproc)" > "$S/logs/f-probe-build.log" 2>&1 \
  || { grep -E "error:|FAILED" "$S/logs/f-probe-build.log" | head -20; exit 92; }
echo "--- PROBE SHA256 (differs from stage 1 and E BY CONSTRUCTION; the identity leg is the proof it is inert at the default)"
sha256sum "$S/probebuild/m3a_probe" | tee "$S/probe-f-sha256.txt"
echo "--- THE NEW ARGUMENT IS PARSED (usage line)"
"$S/probebuild/m3a_probe" 2>&1 | head -4
echo "FOREGROUND_END f-prepare $(date -Is) rc=0"
'
  rc=$?
  echo "WRAPPER_RC f-prepare rc=${rc}"
  exit $rc
fi

# --------------------------------------------------------------------------
# stageF: identity leg, its gate, the gated leg, its gate, then three legs.
# ONE LOCK HOLD PER LEG.
# --------------------------------------------------------------------------
if [ "$MODE" != stageF ]; then
  echo "usage: chainF.sh [prepare|stageF]"
  exit 2
fi

mkdir -p "$S/logs" "$S/out/f"
LOG=$S/logs/chainF.log
exec >"$LOG" 2>&1
echo "CHAINF_START $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
echo "=== probe sha256 recorded at prepare: $(cat "$S/probe-f-sha256.txt" 2>/dev/null)"
echo "=== library sha256: $(sha256sum "$S/install/lib64/libhven.a" 2>/dev/null)"

# run_leg TAG CELL [extra probe args...] -- one lock hold, one solve, one row.
# Returns 0 on a finished leg, 1 on CENSORED/failed. TRUNCATION is NOT a failure
# here: it is a row shape, and RULE-F.md section 5 says how to read it.
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
  "$BIN" --problem "$cell" --deadline 300 "$@" \
  > "$S/out/f/$tag.txt" 2>"$S/out/f/$tag.stderr"
rc=$?
t1=$(date +%s.%N)
printf "LEG %-20s rc=%-3s wall_s=%.1f (INFORMATIONAL)  args: %s\n" "$tag" "$rc" "$(echo "$t1 - $t0" | bc)" "$*"
w=0
if [ "$rc" = 124 ]; then
  echo "    CENSORED: the 900 s outer stop fired -- NO ROW, never filled (RULE-F.md section 5)"
  rm -f "$S/out/f/$tag.txt"
  w=1
elif [ "$rc" != 0 ]; then
  echo "    CENSORED: leg exited rc=$rc -- NO ROW"
  head -5 "$S/out/f/$tag.stderr"
  rm -f "$S/out/f/$tag.txt"
  w=1
else
  grep -E "^(tr_init|ledger|deadline_fired|status|infeasibility_certified|major_iters|restoration_iters|qp_minor_iters|factorizations|symbolic_analyses|border_refine_steps|steps_accepted|rejected_steps|elastic_activations|qp_maxiter_exits|restoration_entries|x_k_source|history_rows|first_qp_status|first_qp_plain_status|first_qp_minor_iters|ledger_qp_records|ledger_records_print_truncated|first_label|first_qp_record_status|first_minor_iters|first_degenerate_steps|first_degenerate_run_max|first_degenerate_fraction|first_ratio_ties|first_drop_ties|first_ws_adds|first_ws_drops|first_ws_net_change|first_ws_adds_minus_drops|first_schur_updates|first_suspect_escalations|feasibility_e|feasibility_i)=" \
    "$S/out/f/$tag.txt" | sed "s/^/    /"
  if grep -q "^deadline_fired=1$" "$S/out/f/$tag.txt"; then
    echo "    TRUNCATED: the 300 s solver deadline fired -- counters reported, NO VERDICT,"
    echo "    and the neutrality check on this leg is WITHHELD (RULE-F.md section 5)"
  fi
fi
echo "FOREGROUND_END leg-$tag $(date -Is) rc=$w"
exit $w
' _ "$tag" "$cell" "$@"
  rc=$?
  echo "WRAPPER_RC leg-$tag rc=${rc}"
  return $rc
}

# --- GATE 1: the identity leg, first --------------------------------------
# The new argument at its DEFAULT: `--ledger off`, which constructs no Ledger
# and never calls attach_ledger.
if ! run_leg "STEENBRB@identity-F" "STEENBRB" --tr-init inf --ledger off; then
  echo "CHAINF_ABORTED: the identity leg was CENSORED. No gate, no F legs (RULE-F.md sections 3.1, 5)."
  echo "CHAINF_END $(date -Is)"
  exit 3
fi
if grep -q "^deadline_fired=1$" "$S/out/f/STEENBRB@identity-F.txt"; then
  echo "CHAINF_ABORTED: the identity leg was TRUNCATED at the 300 s deadline -- it cannot"
  echo "  prove the rebuilt binary reproduces a row that ran to completion (RULE-F.md section 5)."
  echo "CHAINF_END $(date -Is)"
  exit 4
fi

echo "--- IDENTITY ON THE REBUILT PROBE (RULE-F.md section 3.1)"
python3 -B "$ART/wrappers/identity_f.py" identity | tee "$S/out/identity-f.txt"
id_rc=${PIPESTATUS[0]}
echo "IDENTITY_F_RC=$id_rc"
if [ "$id_rc" != 0 ]; then
  echo "CHAINF_ABORTED: the rebuilt probe does not reproduce the committed stage-1 row"
  echo "  at the new argument's default. --ledger is not inert, so no F row taken with"
  echo "  this binary could be believed. No F leg runs (RULE-F.md section 3.1)."
  echo "CHAINF_END $(date -Is)"
  exit 5
fi

# --- GATE 2: the first measured leg, and its neutrality check -------------
if ! run_leg "STEENBRB@F-trinf" "STEENBRB" --tr-init inf --ledger on; then
  echo "CHAINF_ABORTED: the gating measured leg was CENSORED -- NO ROW, so the neutrality"
  echo "  gate cannot be taken and the remaining three legs do not run (RULE-F.md section 3.2)."
  echo "CHAINF_END $(date -Is)"
  exit 6
fi

echo "--- NEUTRALITY, THE GATING LEG (RULE-F.md section 3.2)"
python3 -B "$ART/wrappers/identity_f.py" neutral-first | tee "$S/out/neutral-first-f.txt"
nf_rc=${PIPESTATUS[0]}
echo "NEUTRAL_FIRST_RC=$nf_rc"
if [ "$nf_rc" != 0 ]; then
  echo "CHAINF_ABORTED: A SOLVER COUNTER MOVED WITH THE LEDGER ATTACHED."
  echo "  That is a FINDING ABOUT hven -- a public, documented-inert instrument perturbing"
  echo "  a solve -- and RULE-F.md section 3.2 says the addendum STOPS on it. No H-DEG"
  echo "  verdict is taken from a leg whose neutrality check failed, so the remaining"
  echo "  three legs do not run. The differing keys are in the report above."
  echo "CHAINF_END $(date -Is)"
  exit 7
fi

# --- the remaining three legs ---------------------------------------------
echo "$LEGS" | while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  tag=${spec%%:*}
  rest=${spec#*:}
  cell=${rest%%:*}
  extra=${rest#*:}
  # `extra` is two flags and two values, all whitespace-free by construction
  # (RULE-F.md section 2 fixes them), so word splitting here is exact.
  # shellcheck disable=SC2086
  run_leg "$tag" "$cell" $extra || echo "LEG_CENSORED $tag"
done
# The loop above runs in a subshell (pipeline), so `worst` is computed here from
# the rows that actually exist rather than carried out of it.
worst=0
for spec in "STEENBRB@F-trinf" "STEENBRB@F-tr1" "YORKNET@F-trinf" "YORKNET@F-tr1"; do
  [ -f "$S/out/f/$spec.txt" ] || { echo "MISSING ROW: $spec (CENSORED)"; worst=1; }
done

# --- the full neutrality fold, OUTSIDE the lock (file folding, no solver) --
echo "--- NEUTRALITY, ALL FOUR MEASURED LEGS (RULE-F.md section 3.2)"
python3 -B "$ART/wrappers/identity_f.py" neutrality | tee "$S/out/neutrality-f.txt"
nt_rc=${PIPESTATUS[0]}
echo "NEUTRALITY_F_RC=$nt_rc"

echo "LEGS_F_WORST=$worst  (1 means at least one leg was CENSORED; see its LEG line)"
echo "CHAINF_END $(date -Is)"
exit 0
