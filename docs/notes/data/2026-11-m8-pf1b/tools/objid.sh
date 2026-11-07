#!/usr/bin/env bash
# PF-1b comment rider: libhven.a Release of the worktree (comment edit) vs Release of the 7a95a612 export (deps copied
# at the pins), same flags; every archive member compared byte for byte.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1b
B=$WT/build-pf1b
BASE=7a95a612d605da2ed49a463eee0ecea3db5bce56
export CCACHE_DISABLE=1
cd "$WT" || exit 1
rm -rf "$B/base-src" "$B/base" "$B/rel"; mkdir -p "$B/base-src"
git archive "$BASE" | tar -x -C "$B/base-src" || exit 1
[ "$(git rev-parse $BASE:dep/eigen)" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse $BASE:dep/fmt)" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$B/base-src/dep/eigen" "$B/base-src/dep/fmt"; cp -a dep/eigen "$B/base-src/dep/eigen" && cp -a dep/fmt "$B/base-src/dep/fmt" || exit 1
for arm in base rel; do
  src=$WT; [ $arm = base ] && src=$B/base-src
  cmake -S "$src" -B "$B/$arm" -G Ninja -DCMAKE_BUILD_TYPE=Release -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ > "$B/$arm.configure.log" 2>&1 || { echo CONFIGURE_FAIL $arm; exit 1; }
  ninja -C "$B/$arm" -j16 hven > "$B/$arm.build.log" 2>&1 || { echo BUILD_FAIL $arm; tail -20 "$B/$arm.build.log"; exit 1; }
  echo "built $arm: $(tail -1 "$B/$arm.build.log")"
done
git diff --stat "$BASE" -- src include
rm -rf "$B/objA" "$B/objB"; mkdir "$B/objA" "$B/objB"
(cd "$B/objA" && ar x ../base/libhven.a); (cd "$B/objB" && ar x ../rel/libhven.a)
diff <(ls "$B/objA") <(ls "$B/objB") && echo MEMBER_LISTS_EQUAL
n=0; d=0
for f in "$B"/objA/*; do b=$(basename "$f"); n=$((n+1)); cmp -s "$f" "$B/objB/$b" || { d=$((d+1)); echo "DIFF $b"; }; done
echo "objects=$n differ=$d"
