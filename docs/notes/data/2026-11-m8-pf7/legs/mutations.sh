#!/usr/bin/env bash
# usage: mutations.sh <out dir>   Run under `box-lane --`.  Each mutation of src/util/get_core_count.cpp (anchor occurs exactly once, checked) must make
# hven_core_tests CoreCount.* fail; the control (unmutated) must pass.  The file is restored with `git checkout` after each one and the tree rebuilt at the end.
W=/home/ghecht/Projects/hven/.claude/worktrees/pf7; F=$W/src/util/get_core_count.cpp; B=$W/build-rel
OUT=$(realpath -m "${1:?out}"); mkdir -p "$OUT"
export TMPDIR=$W/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd $W || exit 1
[ -z "$(git status --porcelain -- src include tests)" ] || { echo TREE_DIRTY; exit 3; }
run() { # name
  ninja -C $B -j${BOX_LANE_JOBS:-4} hven_core_tests > "$OUT/$1.build.log" 2>&1 || { echo "$1 BUILD_FAIL"; return 2; }
  (cd $B && ctest -R 'CoreCount' --output-on-failure > "$OUT/$1.log" 2>&1); rc=$?
  echo "$1 ctest rc=$rc"
  if [ $rc -eq 0 ]; then grep -q "0 tests failed out of 3\b" "$OUT/$1.log" || { echo "$1 EXPECTED_3_TESTS_NOT_RUN"; return 7; }; fi
  return $rc
}
run control; [ $? -eq 0 ] || { echo CONTROL_NOT_CLEAN; exit 5; }
declare -A M=(
 [K1_core_id_key_broken]='klen == 7 \&\& std::memcmp(a, "core id", 7)|klen == 8 \&\& std::memcmp(a, "core id", 7)'
 [K2_dedupe_lost]='cores.insert(current);|cores.insert(std::pair<int, int>(current.first, static_cast<int>(cores.size())));'
 [K3_count_off_by_one]='return cores.size() != 0 ? int(cores.size()) : tcount;|return cores.size() != 0 ? int(cores.size()) + 1 : tcount;'
 [K4_cache_ignores_probe]='static const int probed = std::max<int>(1, Run());|static const int probed = 1;'
)
worst=0
for id in "${!M[@]}"; do
  old=${M[$id]%%|*}; new=${M[$id]#*|}
  python3 - "$F" "$old" "$new" <<'PY' || { echo "$id ANCHOR_FAIL"; git checkout -- "$F"; continue; }
import sys
f,old,new=sys.argv[1:]
old=old.replace('\\&','&'); new=new.replace('\\&','&'); s=open(f).read()
assert s.count(old)==1, (old, s.count(old))
open(f,'w').write(s.replace(old,new))
PY
  run "$id"; rc=$?
  if [ $rc -eq 0 ]; then echo "$id SURVIVED"; worst=1; else echo "$id KILLED"; fi
  git checkout -- "$F"
done
ninja -C $B -j${BOX_LANE_JOBS:-4} hven_core_tests > "$OUT/restore.build.log" 2>&1
echo "MUTATIONS worst=$worst"; exit $worst
