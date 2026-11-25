#!/usr/bin/env bash
# M8 DN-1 impl (DECLARATION P16): R11's objects leg re-pointed (worktree dn1, build-dn1e, this directory), criterion
# amended. Library objects, arm A (build-dn1e/armA) vs arm B (build-dn1e/rel): every object of the hven target cmp'd.
# Criterion: arm B's object set is arm A's plus exactly ./src/linear/dense_session.cpp.o, and
# ./src/linear/dense_symmetric_factor.cpp.o is BYTE-IDENTICAL (design N5: the Schur border stack's code path). Every
# other differing object is listed (reported, not judged: symmetric_factor.h, solver_counters.h, kkt_calls.h,
# qp_engine.h and the option headers change layout or declarations).
# usage: objects.sh <evidence-dir>   (reads two existing build trees; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
EV=${1:?evidence dir}
mkdir -p "$EV"
A=$WT/build-dn1e/armA/CMakeFiles/hven.dir
B=$WT/build-dn1e/rel/CMakeFiles/hven.dir
{
la=$(cd "$A" && find . -name '*.o' | sort)
lb=$(cd "$B" && find . -name '*.o' | sort)
added=$(comm -13 <(echo "$la") <(echo "$lb") | tr '\n' ' ')
removed=$(comm -23 <(echo "$la") <(echo "$lb") | tr '\n' ' ')
echo "objects A $(echo "$la" | wc -l), B $(echo "$lb" | wc -l); added: ${added:-none}; removed: ${removed:-none}"
n=0; same=0; diffs=""
for o in $la; do
  [ -f "$B/$o" ] || continue
  n=$((n+1))
  if cmp -s "$A/$o" "$B/$o"; then same=$((same+1)); else diffs="$diffs $o"; fi
done
echo "in both $n; identical $same; differing $((n-same)):"
for o in $diffs; do echo "  $o"; done
ok=1
[ "$added" = "./src/linear/dense_session.cpp.o " ] || { echo "ADDED SET IS NOT EXACTLY dense_session.cpp.o"; ok=0; }
[ -z "$removed" ] || { echo "AN OBJECT WAS REMOVED"; ok=0; }
if cmp -s "$A/src/linear/dense_symmetric_factor.cpp.o" "$B/src/linear/dense_symmetric_factor.cpp.o"; then
  echo "dense_symmetric_factor.cpp.o BYTE-IDENTICAL"
else
  echo "dense_symmetric_factor.cpp.o DIFFERS"; ok=0
fi
[ $ok = 1 ] && echo OBJECTS_AS_DECLARED || echo OBJECTS_NOT_AS_DECLARED
} | tee "$EV/objects.txt"
grep -q '^OBJECTS_AS_DECLARED$' "$EV/objects.txt"
