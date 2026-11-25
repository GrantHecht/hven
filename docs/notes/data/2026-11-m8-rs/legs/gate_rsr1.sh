#!/usr/bin/env bash
# usage: gate_rsr1.sh [steps...]   Run a COPY from outside the tree (psym checks the tree out at the before commit):
#   mkdir -p .scratch/instr; cp legs/{gate_rsr1,env,psym_arms,build,ctest,lists,replay,install_smoke,mutation_rsr1}.sh tools/neutral_flags.py .scratch/instr/
#   WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/gate_rsr1.sh [steps]
# The RS-R1 gate, DECLARATION-RS-R1.md (and -r2 when present). steps (default all): build psym flags lists ctest smoke replay pch debug sweep mutation
# The base measurement (lists, replay) comes from legs/baseline2.sh run in the detached base worktree rsr1-base (b02e88eb4).
set -o pipefail
source "$(dirname "$0")/env.sh"
export HVEN_PCH_NEUTRALITY_JOBS=12
STEPS=${*:-build psym flags lists ctest smoke replay pch debug sweep mutation}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$(dirname "$0")
BASEWT=${BASEWT:-/home/ghecht/Projects/hven/.claude/worktrees/rsr1-base}
BASE=${PSYM_BASE:-b02e88eb4}
GD=$E/gates/RS-R1; SC=$WT/.scratch/g
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-R1 head $(git rev-parse --short HEAD) base $BASE steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  export PSYM_BASE=$(git rev-parse $BASE)
  rm -rf "$SC/arms"; WT=$WT bash $L/psym_arms.sh "$SC/arms" > "$GD/psym-arms-run.txt" 2>&1
  rec psym-arms $?; tail -3 "$GD/psym-arms-run.txt"
  cp "$SC/arms/base-build.log" "$SC/arms/after-build.log" "$SC/arms.flags.txt" "$GD/" 2>/dev/null
  cp "$SC/arms/cc-base.json" "$SC/arms/cc-after.json" "$SC/" 2>/dev/null
fi
if has psym || has objcmp; then
  python3 $E/tools/rsr1_objcmp.py "$SC/arms/psym-base" "$SC/arms/psym-after" "$GD/declared-movers.txt" > "$GD/objcmp.txt" 2>&1
  rec objects-as-declared $?; tail -12 "$GD/objcmp.txt"
fi
if has flags || has psym; then
  python3 $E/tools/rsr1_flags.py "$SC/arms/cc-base.json" "$SC/arms/cc-after.json" "$WT" > "$GD/flags.txt" 2>&1
  rec flags-as-declared $?; tail -5 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  cp "$SC/lists/ctest-N.txt" "$GD/ctest-N-after.txt"; cp -r "$SC/lists/gtest" "$GD/gtest-after"; cp "$BASEWT/.scratch/base/lists/ctest-N.txt" "$GD/ctest-N-base.txt"
  python3 $E/tools/lists_verdict_rsr1.py "$BASEWT/.scratch/base/lists" "$SC/lists" > "$GD/lists-verdict.txt" 2>&1
  rec lists-as-declared $?; cat "$GD/lists-verdict.txt" | tee -a "$V" | tail -40
  cp "$WT/build/hven_test_membership.txt" "$GD/membership-after.txt"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  diff "$E/gates/RS-6a-r6/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" > "$GD/listing-default-diff.txt" 2>&1
  cat "$GD/listing-default-diff.txt" | tee -a "$V"
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$BASEWT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has pch; then bash scripts/check_pch_neutrality.sh > "$GD/pch-neutrality.txt" 2>&1; rec pch-neutrality $?; tail -3 "$GD/pch-neutrality.txt"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-debug.log" | tee -a "$V"
fi
if has sweep; then bash $E/tools/header_sweep.sh "$WT" "$GD/header-sweep-after.txt" | tee -a "$V"; fi
if has mutation; then WT=$WT bash $L/mutation_rsr1.sh build > "$GD/mutation-run.txt" 2>&1; rec mutation $?; tail -14 "$GD/mutation-run.txt"; fi
echo "== gate RS-R1 done $(date -Is)" | tee -a "$V"
