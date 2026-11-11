#!/usr/bin/env bash
# M8 DN-1d r3 (../../DECLARATION-r3.txt): r2's copy of this file re-pointed (r3/, build-dn1de3/); older provenance lines below are historical.
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (DECLARATION P16): library objects, arm A (build-dn1be/armA, 7784e4f59) vs arm B (build-dn1be/rel): every
# object of the hven target cmp'd. Criterion: the two object sets are equal, and every object under ./src/linear/ is
# BYTE-IDENTICAL (the guard lives above the linear layer). Every other differing object is listed (reported, not judged).
# DN-1c (../DECLARATION.txt P16): the ./src/linear/ rule amended -- dense_session.cpp.o and symmetric_factor_mkl.cpp.o
# are declared to differ; every other ./src/linear/ object BYTE-IDENTICAL.
# DN-1d (../DECLARATION.txt P16/D5): the same rule -- dense_session.cpp.o and symmetric_factor_mkl.cpp.o declared to
# differ; every other ./src/linear/ object BYTE-IDENTICAL.
# usage: objects.sh <evidence-dir>   (reads two existing build trees; builds nothing)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
EV=${1:?evidence dir}
mkdir -p "$EV"
A=$WT/build-dn1de3/armA/CMakeFiles/hven.dir
B=$WT/build-dn1de3/rel/CMakeFiles/hven.dir
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
[ -z "$added" ] || { echo "AN OBJECT WAS ADDED"; ok=0; }
[ -z "$removed" ] || { echo "AN OBJECT WAS REMOVED"; ok=0; }
# DN-1c: the two TUs the check lives in may differ; every other ./src/linear/ object (the MPL-derived
# pardiso_session and the Schur border's dense_symmetric_factor among them) must be BYTE-IDENTICAL.
for o in $(cd "$A" && find ./src/linear -name '*.o' | sort); do
  case "$o" in
    ./src/linear/dense_session.cpp.o|./src/linear/symmetric_factor_mkl.cpp.o) echo "linear (declared to differ) $o"; continue ;;
  esac
  if cmp -s "$A/$o" "$B/$o"; then echo "linear BYTE-IDENTICAL $o"; else echo "linear DIFFERS $o"; ok=0; fi
done
[ $ok = 1 ] && echo OBJECTS_AS_DECLARED || echo OBJECTS_NOT_AS_DECLARED
} | tee "$EV/objects.txt"
grep -q '^OBJECTS_AS_DECLARED$' "$EV/objects.txt"
