#!/usr/bin/env bash
# usage: gate3.sh [steps...]   Run a COPY from outside the tree (the psym step checks the tree out at the before commit):
#   mkdir -p .scratch/instr; cp legs/{gate3,env,psym_arms,build,ctest,lists,install_smoke}.sh tools/neutral_flags.py .scratch/instr/
#   WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/gate3.sh
# The reduced RS-3 gate (DECLARATION-RS3.md): clean Release build, P-SYM before/after (same-path, __FILE__-neutralised arms), ctest Release, ctest -N / gtest lists vs the
# committed RS-0 lists, install smoke. Verdicts are recorded in gates/RS-3/VERDICTS.txt.  steps (default all): build psym ctest lists smoke
set -o pipefail
source "$(dirname "$0")/env.sh"
STEPS=${*:-build psym ctest lists smoke}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$(dirname "$0")
GD=$E/gates/RS-3; SC=$WT/.scratch/g3
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-3 head $(git rev-parse --short HEAD) base ${PSYM_BASE:-aa1330cad} steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  export PSYM_BASE=${PSYM_BASE:-aa1330cad46b6faa1a563bf7cb152a2d83c5e89a}
  rm -rf "$SC/arms"; WT=$WT bash $L/psym_arms.sh "$SC/arms" > "$GD/psym-arms-run.txt" 2>&1
  rec psym-arms $?; tail -3 "$GD/psym-arms-run.txt"
  cp "$SC/arms/base-build.log" "$SC/arms/after-build.log" "$SC/arms.flags.txt" "$GD/" 2>/dev/null
  scripts/psym_compare.sh compare "$SC/arms/psym-base" "$SC/arms/psym-after" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -12 "$GD/psym.txt"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  rm -rf "$SC/lists-noex"; cp -a "$SC/lists" "$SC/lists-noex"; rm -f "$SC"/lists-noex/gtest/hven_example_*.txt
  python3 $E/tools/lists_compare.py "$E/baseline" "$SC/lists-noex" > "$GD/lists-diff-rs0.txt" 2>&1; rec lists-identical-rs0 $?; tail -2 "$GD/lists-diff-rs0.txt"
fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-2/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" /dev/null > "$GD/listing-default.txt" 2>&1; rec listing-default $?; tail -1 "$GD/listing-default.txt"
fi
echo "== gate RS-3 done $(date -Is)" | tee -a "$V"
