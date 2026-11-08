#!/usr/bin/env bash
# M8 F1 (DECLARATION P16): library objects, arm A (build-f1/armA) vs arm B (build-f1/rel): every object of the hven
# target cmp'd; criterion: exactly ONE differs, src/drivers/solver_init.cpp.o; arm B's libhven.a names no dsecnd symbol
# and its solver_init object references MKL_Get_Max_Threads; arm A's solver_init object references dsecnd.
# usage: objects.sh <evidence-dir>   (reads two existing build trees; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
EV=${1:?evidence dir}
mkdir -p "$EV"
A=$WT/build-f1/armA/CMakeFiles/hven.dir
B=$WT/build-f1/rel/CMakeFiles/hven.dir
{
n=0; diffs=""
for o in $(cd "$A" && find . -name '*.o' | sort); do
  n=$((n+1)); cmp -s "$A/$o" "$B/$o" || diffs="$diffs $o"
done
[ "$(cd "$A" && find . -name '*.o' | sort)" = "$(cd "$B" && find . -name '*.o' | sort)" ] && echo "object sets equal" || echo "OBJECT SETS DIFFER"
echo "objects $n; differing:$diffs"
echo "arm A solver_init undefined MKL/dsecnd refs: $(nm "$A/src/drivers/solver_init.cpp.o" | grep -E ' U (dsecnd|MKL_|mkl_)' | awk '{print $2}' | tr '\n' ' ')"
echo "arm B solver_init undefined MKL/dsecnd refs: $(nm "$B/src/drivers/solver_init.cpp.o" | grep -E ' U (dsecnd|MKL_|mkl_)' | awk '{print $2}' | tr '\n' ' ')"
LIBB=$WT/build-f1/rel/libhven.a
echo "arm B libhven.a dsecnd symbols: $(nm "$LIBB" 2>/dev/null | grep -ci dsecnd)"
ok=1
[ "$diffs" = " ./src/drivers/solver_init.cpp.o" ] || ok=0
nm "$A/src/drivers/solver_init.cpp.o" | grep -qE ' U dsecnd$' || ok=0
nm "$B/src/drivers/solver_init.cpp.o" | grep -qE ' U MKL_Get_Max_Threads$' || ok=0
[ "$(nm "$LIBB" 2>/dev/null | grep -ci dsecnd)" = 0 ] || ok=0
[ -f "$LIBB" ] || ok=0
[ $ok = 1 ] && echo OBJECTS_AS_DECLARED || echo OBJECTS_NOT_AS_DECLARED
} | tee "$EV/objects.txt"
grep -q '^OBJECTS_AS_DECLARED$' "$EV/objects.txt"
