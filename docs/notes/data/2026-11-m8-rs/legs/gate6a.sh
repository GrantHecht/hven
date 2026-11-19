#!/usr/bin/env bash
# usage: gate6a.sh [steps...]   Run a COPY from outside the tree (the psym step checks the tree out at the before commit):
#   mkdir -p .scratch/instr; cp legs/{gate6a,env,psym_arms,build,ctest,lists,replay,install_smoke,q9,mutation6a}.sh tools/neutral_flags.py .scratch/instr/
#   WT=<worktree> flock /tmp/box-build.lock setsid -w bash .scratch/instr/gate6a.sh [steps]
# The design 5.2 / 5.4 gate for RS-6a, DECLARATION-RS6a.md (+ -r2 when present). The base measurement (lists, replay, ctest) comes from legs/baseline2.sh run in the separate base worktree.
# steps (default all): build psym flags lists ctest smoke replay pch debug sweep mutation q9; extra steps: q9base, q9after (one arm each), psymcmp (compare the captured arms only), membership (compare the build's membership table with the declared one)
set -o pipefail
source "$(dirname "$0")/env.sh"
export HVEN_PCH_NEUTRALITY_JOBS=12
STEPS=${*:-build psym flags lists ctest smoke replay pch debug sweep mutation q9}
E=$WT/docs/notes/data/2026-11-m8-rs; L=$(dirname "$0")
BASEWT=${BASEWT:-/home/ghecht/Projects/hven/.claude/worktrees/rs6a-base}
GD=$E/gates/RS-6a; SC=$WT/.scratch/g6
mkdir -p "$GD" "$SC"
V=$GD/VERDICTS.txt
cd "$WT" || exit 1
rec() { echo "$1 rc=$2 $(date -Is)" | tee -a "$V"; }
has() { case " $STEPS " in *" $1 "*) return 0;; esac; return 1; }
echo "== gate RS-6a head $(git rev-parse --short HEAD) base ${PSYM_BASE:-2f43fee5e} steps: $STEPS $(date -Is)" | tee -a "$V"
if has build; then bash $L/build.sh Release build clean > "$GD/build-release.txt" 2>&1; rec build-release $?; tail -2 "$GD/build-release.txt"; fi
if has psym; then
  export PSYM_BASE=${PSYM_BASE:-$(git rev-parse 2f43fee5e)}
  rm -rf "$SC/arms"; WT=$WT bash $L/psym_arms.sh "$SC/arms" > "$GD/psym-arms-run.txt" 2>&1
  rec psym-arms $?; tail -3 "$GD/psym-arms-run.txt"
  cp "$SC/arms/base-build.log" "$SC/arms/after-build.log" "$SC/arms.flags.txt" "$GD/" 2>/dev/null
fi
if has psym || has psymcmp; then
  DECL="$GD/declared-object-map.txt"; [ -f "$GD/declared-object-map-r2.txt" ] && DECL="$DECL $GD/declared-object-map-r2.txt"
  python3 $E/tools/rs6a_objmap_check.py "$SC/arms/psym-base/.psym-manifest" "$SC/arms/psym-after/.psym-manifest" $DECL --out "$GD/object-map-generated.txt" --unity-before "$GD/unity-before.txt" --unity-after "$GD/unity-after.txt" --unity-class-source tests/core/test_type_storage.cpp --unity-class-source tests/kkt/test_kkt_factorization.cpp > "$GD/objmap-check.txt" 2>&1
  rec objmap-equals-declared $?; tail -3 "$GD/objmap-check.txt"
  for ARM in base after; do   # the unity class is compared by psym_union, never paired: take it out of the compared snapshots
    rm -rf "$SC/arms/psym-$ARM-nu"; cp -al "$SC/arms/psym-$ARM" "$SC/arms/psym-$ARM-nu"
    U=$GD/unity-$([ $ARM = base ] && echo before || echo after).txt
    while read -r o; do [ -n "$o" ] && rm -f "$SC/arms/psym-$ARM-nu/$o"; done < "$U"
    grep -vxFf "$U" "$SC/arms/psym-$ARM/.psym-manifest" > "$SC/arms/psym-$ARM-nu/.psym-manifest.new"
    rm -f "$SC/arms/psym-$ARM-nu/.psym-manifest"; mv "$SC/arms/psym-$ARM-nu/.psym-manifest.new" "$SC/arms/psym-$ARM-nu/.psym-manifest"
    cp "$SC/arms/psym-$ARM/.psym-version" "$SC/arms/psym-$ARM-nu/.psym-version"
  done
  scripts/psym_compare.sh compare --object-map "$GD/object-map-generated.txt" "$SC/arms/psym-base-nu" "$SC/arms/psym-after-nu" > "$GD/psym.txt" 2>&1
  rec psym-compare $?; tail -12 "$GD/psym.txt"
  python3 $E/tools/psym_union.py --before "$GD/unity-before.txt" --root-before "$SC/arms/psym-base" --after "$GD/unity-after.txt" --root-after "$SC/arms/psym-after" --jobs 4 --detail "$GD/psym-union-detail.txt" > "$GD/psym-union.txt" 2>&1
  rec psym-union $?; tail -6 "$GD/psym-union.txt"
fi
if has flags; then
  python3 $E/tools/flags_diff6a.py "$SC/arms/cc-base.json" "$SC/arms/cc-after.json" --root-before "$WT" --root-after "$WT" --allow-only-after '^tests/core/test_type_storage\.cpp$' --allow-only-after '^tests/kkt/test_kkt_factorization\.cpp$' --allow-vanished '^tests/qp/test_dense_solve_wound_seams\.cpp$' > "$GD/flags.txt" 2>&1
  rec flags-diff $?; tail -14 "$GD/flags.txt"
fi
if has lists; then
  rm -rf "$SC/lists"; bash $L/lists.sh build "$SC/lists" > "$GD/lists-run.txt" 2>&1
  cp "$SC/lists/ctest-N.txt" "$GD/ctest-N-after.txt"; cp -r "$SC/lists/gtest" "$GD/gtest-after"; cp "$BASEWT/.scratch/base/lists/ctest-N.txt" "$GD/ctest-N-base.txt"
  python3 $E/tools/lists_verdict6a.py "$BASEWT/.scratch/base/lists" "$SC/lists" > "$GD/lists-verdict.txt" 2>&1
  rec lists-as-declared $?; cat "$GD/lists-verdict.txt" | tee -a "$V" | tail -30
fi
if has lists || has membership; then
  cp "$WT/build/hven_test_membership.txt" "$GD/membership-after.txt"
  python3 - "$GD/declared-membership.txt" "$GD/membership-after.txt" <<'PY' > "$GD/membership-verdict.txt"; rec membership-equals-declared $?
import re, sys
decl = {}; cur = None
for ln in open(sys.argv[1]):
    m = re.match(r'^(hven_\w+?)(?: \[\w+\])?\s+\(\d+ sources\)', ln)
    if m: cur = m.group(1); continue
    if ln.startswith('  '): decl[ln.split()[0]] = cur
got = {}
for ln in open(sys.argv[2]):
    a, b = ln.split(None, 1); got[a] = b.strip()
bad = [(k, decl.get(k), got.get(k)) for k in sorted(set(decl) | set(got)) if decl.get(k) != got.get(k)]
print('declared %d sources, built %d' % (len(decl), len(got)))
if bad: print('MEMBERSHIP_DIFFERS', bad); sys.exit(1)
print('MEMBERSHIP_EQUALS_DECLARED')
PY
  cat "$GD/membership-verdict.txt" | tee -a "$V"
fi
if has ctest; then bash $L/ctest.sh build "$GD/ctest-release.log" > "$GD/ctest-release-run.txt" 2>&1; rec ctest-release $?; grep -E 'tests passed|tests failed' "$GD/ctest-release.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-release.log" | tee -a "$V"; fi
if has smoke; then
  bash $L/install_smoke.sh "$GD/smoke" > "$GD/smoke-run.txt" 2>&1; rec install-smoke $?
  python3 $E/tools/listing_apply_map.py "$E/gates/RS-5/smoke/installed-default.txt" "$GD/smoke/installed-default.txt" /dev/null > "$GD/listing-default.txt" 2>&1
  tail -4 "$GD/listing-default.txt"
  if cmp -s "$E/gates/RS-5/smoke/installed-default.txt" "$GD/smoke/installed-default.txt"; then rec listing-equals-rs5 0; else rec listing-equals-rs5 1; fi
fi
if has replay; then bash $L/replay.sh build "$SC/replay" "$BASEWT/.scratch/base/replay" > "$GD/replay-run.txt" 2>&1; rec replay $?; cp "$SC"/replay/comparison-*.txt "$GD/" 2>/dev/null; grep -h differences "$GD"/comparison-*.txt | tee -a "$V"; fi
if has pch; then bash scripts/check_pch_neutrality.sh > "$GD/pch-neutrality.txt" 2>&1; rec pch-neutrality $?; tail -3 "$GD/pch-neutrality.txt"; fi
if has debug; then
  bash $L/build.sh Debug build-debug clean > "$GD/build-debug.txt" 2>&1; rec build-debug $?
  bash $L/ctest.sh build-debug "$GD/ctest-debug.log" > "$GD/ctest-debug-run.txt" 2>&1; rec ctest-debug $?; grep -E 'tests passed|tests failed' "$GD/ctest-debug.log" | tee -a "$V"; grep -E 'HeaderSelfContained|Layering\.' "$GD/ctest-debug.log" | tee -a "$V"
fi
if has sweep; then bash $E/tools/header_sweep.sh "$WT" "$GD/header-sweep-after.txt" | tee -a "$V"; fi
if has mutation; then WT=$WT bash $L/mutation6a.sh > "$GD/mutation-run.txt" 2>&1; rec mutation $?; tail -12 "$GD/mutation-run.txt"; fi
if has q9 || has q9base; then
  mkdir -p "$GD/q9"
  WT=$BASEWT bash $L/q9.sh base "$GD/q9" hven_fault_injection_tests hven_ipqp_seam_tests hven_qp_walk_seam_tests hven_sqp_seam_tests > "$GD/q9/base-run.txt" 2>&1; rec q9-base $?
fi
if has q9 || has q9after; then
  mkdir -p "$GD/q9"
  bash $L/q9.sh after "$GD/q9" hven_linear_seam_tests hven_kkt_seam_tests hven_qp_seam_tests hven_solver_seam_tests hven_ipm_seam_tests hven_sqp_seam_tests > "$GD/q9/after-run.txt" 2>&1; rec q9-after $?
fi
if has q9 || has q9base || has q9after; then cat "$GD"/q9/*-seams.json "$GD"/q9/*-rest.json | tee -a "$V"; fi
echo "== gate RS-6a done $(date -Is)" | tee -a "$V"
