#!/usr/bin/env bash
# usage: gate.sh <group: a|b|c|d|e> [steps...]   Run as: flock /tmp/box-build.lock setsid -w bash gate.sh a
# The design 5.2 gate for one RS-1 group, each step its own rc, all recorded in gates/RS-1<g>/VERDICTS.txt.
# steps (default all): build psym flags lists ctest smoke replay pch debug
set -o pipefail
source "$(dirname "$0")/env.sh"
export HVEN_PCH_NEUTRALITY_JOBS=12
G=${1:?group}; shift
STEPS=${*:-build psym flags lists ctest smoke replay pch debug}
E=$WT/docs/notes/data/2026-11-m8-rs
L=$E/legs
GD=$E/gates/RS-1$G
SC=$WT/.scratch/g$G
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
# cumulative map: every group up to and including this one
: > "$SC/cumulative.map"
for g in a b c d e; do cat "$E/groups/RS-1$g.map" >> "$SC/cumulative.map"; [ "$g" = "$G" ] && break; done
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-1$G head $(git rev-parse --short HEAD) steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  rm -rf "$SC/psym" "$SC/psym-before"; scripts/psym_compare.sh capture "$SC/psym" build > "$GD/psym-capture.txt" 2>&1
  python3 scripts/rs_object_map.py --move-map "$SC/cumulative.map" --before "$WT/.scratch/psym-base/.psym-manifest" --after "$SC/psym/.psym-manifest" -o "$GD/object-map.txt" > "$GD/object-map-gen.txt" 2>&1
  rec objmap-gen $?; tail -3 "$GD/object-map-gen.txt"
  # Objects whose source the map DELETES have no counterpart; P-SYM cannot map to nothing.  The before arm is
  # therefore compared WITHOUT them, and what they carried is recorded (the removed symbols) so the deletion is
  # accounted for in the open instead of as a MISSING.
  cp -a "$WT/.scratch/psym-base" "$SC/psym-before"
  : > "$GD/removed-symbols.txt"
  for o in $(sed -n 's/^DELETED //p' "$GD/object-map-gen.txt"); do
    echo "== removed object $o" >> "$GD/removed-symbols.txt"
    nm -C --defined-only "$WT/.scratch/psym-base/$o" >> "$GD/removed-symbols.txt"
    rm -f "$SC/psym-before/$o"; grep -vxF "$o" "$SC/psym-before/.psym-manifest" > "$SC/psym-before/.m" && mv "$SC/psym-before/.m" "$SC/psym-before/.psym-manifest"
  done
  EXC=""; [ -f "$GD/exceptions.txt" ] && EXC="--exceptions $GD/exceptions.txt"
  scripts/psym_compare.sh compare --object-map "$GD/object-map.txt" $EXC "$SC/psym-before" "$SC/psym" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -4 "$GD/psym.txt"
fi
if has flags; then
  EXP=""; [ "$G" = a ] && EXP="--expect-removed=-I@/src/interior/utils"
  python3 $E/tools/flags_diff.py "$WT/.scratch/cc-base.json" build/compile_commands.json --root "$WT" --map "$SC/cumulative.map" $EXP > "$GD/flags.txt" 2>&1
  rec flags-diff $?; tail -8 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  diff -r "$SC/lists/gtest" "$WT/.scratch/base/lists/gtest" > "$GD/lists-diff.txt" 2>&1; r1=$?
  diff "$SC/lists/ctest-N.txt" "$WT/.scratch/base/lists/ctest-N.txt" >> "$GD/lists-diff.txt" 2>&1; r2=$?
  rec lists-identical $((r1|r2)); wc -l "$GD/lists-diff.txt"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/baseline/installed-default.txt" "$GD/smoke/installed-default.txt" "$SC/cumulative.map" > "$GD/listing-default.txt" 2>&1; rec listing-default $?
  python3 $E/tools/listing_apply_map.py "$E/baseline/installed-clang.txt" "$GD/smoke/installed-clang.txt" "$SC/cumulative.map" > "$GD/listing-clang.txt" 2>&1; rec listing-clang $?
  tail -1 "$GD/listing-default.txt"
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$WT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has pch; then bash scripts/check_pch_neutrality.sh > "$GD/pch-neutrality.txt" 2>&1; rec pch-neutrality $?; tail -3 "$GD/pch-neutrality.txt"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"
fi
echo "== gate RS-1$G done $(date -Is)" | tee -a "$V"
