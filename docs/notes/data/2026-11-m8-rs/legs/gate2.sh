#!/usr/bin/env bash
# usage: gate2.sh [steps...]   Run as: flock /tmp/box-build.lock setsid -w bash gate2.sh
# The design 5.2 gate for RS-2 (tests re-layout), each step its own rc, recorded in gates/RS-2/VERDICTS.txt.  Mirrors gate.sh (the RS-1 gate); differences are
# declared in DECLARATION-RS2.md: the before arm is the RS-1e head measured by baseline2.sh (.scratch/), the map is groups/RS-2.map only, flags use flags_diff2.py,
# the symbol map is symbol-map-rs2.txt, lists are compared with the RS-1e measured lists AND the committed RS-0 lists.
# steps (default all): build psym flags lists ctest smoke replay debug
set -o pipefail
source "$(dirname "$0")/env.sh"
STEPS=${*:-build psym flags lists ctest smoke replay debug}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$E/legs
GD=$E/gates/RS-2; SC=$WT/.scratch/g2
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
MAP=$E/groups/RS-2.map
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-2 head $(git rev-parse --short HEAD) steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  rm -rf "$SC/psym"; scripts/psym_compare.sh capture "$SC/psym" build > "$GD/psym-capture.txt" 2>&1
  python3 scripts/rs_object_map.py --move-map "$MAP" --before "$WT/.scratch/psym-base/.psym-manifest" --after "$SC/psym/.psym-manifest" -o "$GD/object-map.txt" > "$GD/object-map-gen.txt" 2>&1
  rec objmap-gen $?; tail -3 "$GD/object-map-gen.txt"
  EXC=""; [ -s "$GD/exceptions.txt" ] && EXC="--exceptions $GD/exceptions.txt"
  scripts/psym_compare.sh compare --object-map "$GD/object-map.txt" --symbol-map "$E/symbol-map-rs2.txt" $EXC "$WT/.scratch/psym-base" "$SC/psym" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -4 "$GD/psym.txt"
fi
if has flags; then
  python3 $E/tools/flags_diff2.py "$WT/.scratch/cc-base.json" build/compile_commands.json --root "$WT" --map "$MAP" \
    --dir-move tests/drivers/golden=tests/trace/golden \
    --allow-removed '-I@/tests/(drivers|interior|model|sqp)' --allow-added '-I@/tests' > "$GD/flags.txt" 2>&1
  rec flags-diff $?; tail -30 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  python3 $E/tools/lists_compare.py "$WT/.scratch/base/lists" "$SC/lists" > "$GD/lists-diff.txt" 2>&1; r1=$?
  # the committed RS-0 lists predate the example executables' registration in the lists leg: compare the 12 test executables + ctest -N (examples' gtest-list files excluded)
  rm -rf "$SC/lists-noex"; cp -a "$SC/lists" "$SC/lists-noex"; rm -f "$SC"/lists-noex/gtest/hven_example_*.txt
  python3 $E/tools/lists_compare.py "$E/baseline" "$SC/lists-noex" > "$GD/lists-diff-rs0.txt" 2>&1; r2=$?
  rec lists-identical $((r1|r2)); tail -2 "$GD/lists-diff.txt" "$GD/lists-diff-rs0.txt"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-1e/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" "$MAP" > "$GD/listing-default.txt" 2>&1; rec listing-default $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-1e/smoke/installed-clang.txt" "$GD/smoke/installed-clang.txt" "$MAP" > "$GD/listing-clang.txt" 2>&1; rec listing-clang $?
  tail -1 "$GD/listing-default.txt"
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$WT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"
fi
echo "== gate RS-2 done $(date -Is)" | tee -a "$V"
