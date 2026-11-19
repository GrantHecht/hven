#!/usr/bin/env bash
# usage: gate5.sh [steps...]   Run a COPY from outside the tree (the psym step checks the tree out at the before commit):
#   mkdir -p .scratch/instr; cp legs/{gate5,env,psym_arms,build,ctest,lists,replay,install_smoke}.sh tools/neutral_flags.py .scratch/instr/
#   WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/gate5.sh [steps]
# The design 5.2 gate for RS-5, DECLARATION-RS5.md: no move map (every path is unchanged), P-SYM arms __FILE__-neutralised, flags diff against the base with the
# declared header-selfcheck TUs filtered out, the base measurement (.scratch/base, .scratch/cc-base.json) comes from baseline2.sh run on the base commit (separate worktree).
# steps (default all): build psym flags lists ctest smoke replay pch debug sweep
set -o pipefail
source "$(dirname "$0")/env.sh"
export HVEN_PCH_NEUTRALITY_JOBS=12
STEPS=${*:-build psym flags lists ctest smoke replay pch debug sweep}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$(dirname "$0")
GD=$E/gates/RS-5; SC=$WT/.scratch/g5
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-5 head $(git rev-parse --short HEAD) base ${PSYM_BASE:-c9a4253cc} steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  export PSYM_BASE=${PSYM_BASE:-$(git rev-parse c9a4253cc)}
  rm -rf "$SC/arms"; WT=$WT bash $L/psym_arms.sh "$SC/arms" > "$GD/psym-arms-run.txt" 2>&1
  rec psym-arms $?; tail -3 "$GD/psym-arms-run.txt"
  cp "$SC/arms/base-build.log" "$SC/arms/after-build.log" "$SC/arms.flags.txt" "$GD/" 2>/dev/null
  scripts/psym_compare.sh compare "$SC/arms/psym-base" "$SC/arms/psym-after" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -12 "$GD/psym.txt"
fi
if has flags; then
  # the declared delta: hven_header_selfcheck TUs exist only after; they are filtered out of the after side and listed
  python3 - "$WT/build/compile_commands.json" "$SC/cc-after-filtered.json" "$GD/selfcheck-tus.txt" <<'PY'
import json, sys
src, dst, lst = sys.argv[1:4]
cc = json.load(open(src))
keep = [e for e in cc if '/header_selfcheck/' not in e['file']]
drop = sorted(e['file'].split('/header_selfcheck/')[1] for e in cc if '/header_selfcheck/' in e['file'])
json.dump(keep, open(dst, 'w'))
open(lst, 'w').write('\n'.join(drop) + '\n')
print('after TUs %d, filtered selfcheck TUs %d' % (len(keep), len(drop)))
PY
  python3 $E/tools/flags_diff2.py "$WT/.scratch/cc-base.json" "$SC/cc-after-filtered.json" --root "$WT" > "$GD/flags.txt" 2>&1
  rec flags-diff $?; tail -8 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  python3 $E/tools/lists_compare.py "$WT/.scratch/base/lists" "$SC/lists" > "$GD/lists-diff.txt" 2>&1
  # declared: hven_tests +4 (Layering.*), every other gtest list RAW-identical; ctest -N +5, nothing removed
  { for f in "$WT"/.scratch/base/lists/gtest/*.txt; do n=$(basename $f); if [ "$n" = hven_tests.txt ]; then echo "== $n"; diff $f "$SC/lists/gtest/$n"; else cmp -s $f "$SC/lists/gtest/$n" && echo "identical $n" || echo "DIFFERS $n"; fi; done
    echo "== ctest-N"; diff "$WT/.scratch/base/lists/ctest-N.txt" "$SC/lists/ctest-N.txt"; } > "$GD/lists-declared-diff.txt" 2>&1
  python3 - "$GD/lists-declared-diff.txt" <<'PY' > "$GD/lists-verdict.txt"; r=$?
import sys, re
t = open(sys.argv[1]).read()
assert 'DIFFERS' not in t, 'a list other than hven_tests differs'
parts = t.split('== ctest-N')
gt = [l for l in parts[0].splitlines() if l.startswith(('<', '>'))]
ct = [l for l in parts[1].splitlines() if l.startswith(('<', '>'))]
assert all(l.startswith('>') for l in gt) and len(gt) == 4 and all(l.split()[1].startswith('Layering.') for l in gt), gt
assert all(l.startswith('>') for l in ct) and len(ct) == 5, ct
print('LISTS_AS_DECLARED: hven_tests +4 (Layering.*), ctest -N +5 (the 4 + HeaderSelfContained), nothing removed, every other list identical')
PY
  rec lists-as-declared $r; cat "$GD/lists-verdict.txt" | tee -a "$V"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-4/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" /dev/null > "$GD/listing-default.txt" 2>&1
  tail -4 "$GD/listing-default.txt"
  if diff <(grep -E '^(MISSING|EXTRA)' "$GD/listing-default.txt") <(echo "EXTRA   include/hven/detail/sqp/nlp_eval.h") >/dev/null; then rec listing-equals-rs4-plus-nlp_eval 0; else rec listing-equals-rs4-plus-nlp_eval 1; fi
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$WT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has pch; then bash scripts/check_pch_neutrality.sh > "$GD/pch-neutrality.txt" 2>&1; rec pch-neutrality $?; tail -3 "$GD/pch-neutrality.txt"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-debug.log" | tee -a "$V"
fi
if has sweep; then bash $E/tools/header_sweep.sh "$WT" "$GD/header-sweep-after.txt" | tee -a "$V"; fi
echo "== gate RS-5 done $(date -Is)" | tee -a "$V"
