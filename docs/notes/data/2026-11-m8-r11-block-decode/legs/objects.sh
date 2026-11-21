#!/usr/bin/env bash
# M8 R11 (DECLARATION P16): library objects, arm A (build-r11e/armA) vs arm B (build-r11e/rel): every object of the
# hven target cmp'd; criterion: the object sets are equal and exactly ONE object differs,
# ./src/linear/dense_symmetric_factor.cpp.o (the decode); every TU that includes the two edited headers (comment-only
# edits, line counts kept in schur_complement.h) is byte-identical.
# usage: objects.sh <evidence-dir>   (reads two existing build trees; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
EV=${1:?evidence dir}
mkdir -p "$EV"
A=$WT/build-r11e/armA/CMakeFiles/hven.dir
B=$WT/build-r11e/rel/CMakeFiles/hven.dir
{
n=0; diffs=""
for o in $(cd "$A" && find . -name '*.o' | sort); do
  n=$((n+1)); cmp -s "$A/$o" "$B/$o" || diffs="$diffs $o"
done
sets=0
[ "$(cd "$A" && find . -name '*.o' | sort)" = "$(cd "$B" && find . -name '*.o' | sort)" ] && { echo "object sets equal"; sets=1; } || echo "OBJECT SETS DIFFER"
echo "objects $n; differing:$diffs"
ok=1
[ $sets = 1 ] || ok=0
[ "$diffs" = " ./src/linear/dense_symmetric_factor.cpp.o" ] || ok=0
[ $ok = 1 ] && echo OBJECTS_AS_DECLARED || echo OBJECTS_NOT_AS_DECLARED
} | tee "$EV/objects.txt"
grep -q '^OBJECTS_AS_DECLARED$' "$EV/objects.txt"
