#!/usr/bin/env bash
# usage: gate4.sh [steps...]   Run a COPY from outside the tree (the psym step checks the tree out at the before commit):
#   mkdir -p .scratch/instr; cp legs/{gate4,env,psym_arms,build,ctest,lists,replay,install_smoke}.sh tools/neutral_flags.py .scratch/instr/
#   WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/gate4.sh [steps]
# The design 5.2 gate for RS-4 (includes), DECLARATION-RS4.md: no move map (every path is unchanged), P-SYM arms __FILE__-neutralised, flags diff with an empty map,
# the base measurement (.scratch/base) comes from baseline2.sh run on the base commit before any edit.  Verdicts: gates/RS-4/VERDICTS.txt.
# steps (default all): build psym flags lists ctest smoke replay pch debug
set -o pipefail
source "$(dirname "$0")/env.sh"
export HVEN_PCH_NEUTRALITY_JOBS=12
STEPS=${*:-build psym flags lists ctest smoke replay pch debug}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$(dirname "$0")
GD=$E/gates/RS-4; SC=$WT/.scratch/g4
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-4 head $(git rev-parse --short HEAD) base ${PSYM_BASE:-4064ff97a} steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  export PSYM_BASE=${PSYM_BASE:-4064ff97a58bbe84d57f15b8b2b38a311665fe0d}
  rm -rf "$SC/arms"; WT=$WT bash $L/psym_arms.sh "$SC/arms" > "$GD/psym-arms-run.txt" 2>&1
  rec psym-arms $?; tail -3 "$GD/psym-arms-run.txt"
  cp "$SC/arms/base-build.log" "$SC/arms/after-build.log" "$SC/arms.flags.txt" "$GD/" 2>/dev/null
  scripts/psym_compare.sh compare "$SC/arms/psym-base" "$SC/arms/psym-after" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -12 "$GD/psym.txt"
fi
if has flags; then
  python3 $E/tools/flags_diff2.py "$WT/.scratch/cc-base.json" build/compile_commands.json --root "$WT" > "$GD/flags.txt" 2>&1
  rec flags-diff $?; tail -8 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  python3 $E/tools/lists_compare.py "$WT/.scratch/base/lists" "$SC/lists" > "$GD/lists-diff.txt" 2>&1; r1=$?
  rm -rf "$SC/lists-noex"; cp -a "$SC/lists" "$SC/lists-noex"; rm -f "$SC"/lists-noex/gtest/hven_example_*.txt
  python3 $E/tools/lists_compare.py "$E/baseline" "$SC/lists-noex" > "$GD/lists-diff-rs0.txt" 2>&1; r2=$?
  rec lists-identical $((r1|r2)); tail -2 "$GD/lists-diff.txt"; tail -2 "$GD/lists-diff-rs0.txt"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-3/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" /dev/null > "$GD/listing-default.txt" 2>&1; rec listing-default $?; tail -1 "$GD/listing-default.txt"
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$WT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has pch; then bash scripts/check_pch_neutrality.sh > "$GD/pch-neutrality.txt" 2>&1; rec pch-neutrality $?; tail -3 "$GD/pch-neutrality.txt"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"
fi
echo "== gate RS-4 done $(date -Is)" | tee -a "$V"
