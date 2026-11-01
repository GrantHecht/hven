#!/usr/bin/env bash
# M7 W2 M3c RULE-P2A -- build the two probe binaries the three arms run.
#
# ARM A : hven's 42 sources compiled by an EXACT REPLAY of the install build's
#         own Ninja command lines, with NO HVEN_TESTING.
# ARM B : the same commands plus -DHVEN_TESTING=1 and nothing else.
# ARM C : arm B's binary, run with the observer armed -- no third build.
#
# This script is run INSIDE the box lock by its caller's `flock`, and prints the
# protocol markers itself. Nothing under src/ or include/ is touched; the harness
# tree is READ-ONLY; no solver runs here.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m3c/p2a"
REL="$ROOT/.scratch/m7w2m3c/rel"
PB="$ROOT/.scratch/m7w2m3c/probebuild"
LIB="$ROOT/.scratch/m7w2m3c/install/lib64/libhven.a"
TR="$S/logs/harvest-diff-p2a.txt"

mkdir -p "$S/logs" "$S/rows" "$S/traces"

echo "FOREGROUND_START p2a-prepare $(date -Is)"
if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|hven_sqp_corpus" \
     | grep -v "eval \|pgrep"; then
    echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING TO BUILD"
    echo "FOREGROUND_END p2a-prepare $(date -Is) rc=99"
    exit 99
fi
echo "PGREP_EMPTY p2a-prepare: no foreign cell binary under my lock"

echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

# EVERY PATH HANDED TO A SIBLING IS CHECKED TO EXIST (M3b C8).
for p in "$D/wrappers/replay_build_p2a.py" "$D/wrappers/m3c_p2_probe.cpp" \
         "$REL/build.ninja" "$REL/CMakeFiles/rules.ninja" \
         "$PB/build.ninja" "$PB/CMakeFiles/rules.ninja" "$LIB" "$D/RULE-P2A.md"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "FOREGROUND_END p2a-prepare $(date -Is) rc=90"; exit 90; }
done

rm -f "$TR"
rc=0
for arm in A B; do
    echo "--- ARM $arm"
    python3 "$D/wrappers/replay_build_p2a.py" \
        --arm "$arm" \
        --rel "$REL" \
        --probebuild "$PB" \
        --install-lib "$LIB" \
        --probe-src "$D/wrappers/m3c_p2_probe.cpp" \
        --out "$S/arm$arm" \
        --transcript "$TR" \
        --jobs "$(nproc)" 2>&1 | tail -40
    rc=${PIPESTATUS[0]}
    if [ "$rc" -ne 0 ]; then
        echo "REFUSED: arm $arm build rc=$rc -- see $TR"
        echo "FOREGROUND_END p2a-prepare $(date -Is) rc=$rc"
        exit "$rc"
    fi
done

echo "--- THE TWO BINARIES"
sha256sum "$S/armA/m3c_p2a_probe_armA" "$S/armB/m3c_p2a_probe_armB" \
    | tee "$S/logs/binaries-p2a.txt"

echo "--- THE TWO BINARIES ARE NOT THE SAME FILE (a build that produced one twice is not two arms)"
if [ "$(sha256sum < "$S/armA/m3c_p2a_probe_armA")" = "$(sha256sum < "$S/armB/m3c_p2a_probe_armB")" ]; then
    echo "REFUSED: arm A and arm B are byte-identical binaries"
    echo "FOREGROUND_END p2a-prepare $(date -Is) rc=96"
    exit 96
fi
echo "they differ, as the define requires"

echo "--- USAGE LINE, both arms (each binary runs)"
"$S/armA/m3c_p2a_probe_armA" 2>&1 | head -2
"$S/armB/m3c_p2a_probe_armB" 2>&1 | head -2

echo "--- ARGUMENT REFUSALS, exercised (a checker never seen to refuse is not a checker)"
cd /home/ghecht/Projects/hven-bench || { echo "REFUSED: no bench tree"; exit 93; }
for args in "--problem NOT_A_CELL" "--problem STEENBRB --trace on"; do
    # shellcheck disable=SC2086
    if "$S/armB/m3c_p2a_probe_armB" $args > /dev/null 2>"$S/logs/refusal.txt"; then
        echo "REFUSED: \`$args\` was ACCEPTED and should not have been"
        echo "FOREGROUND_END p2a-prepare $(date -Is) rc=95"
        exit 95
    fi
    echo "refused as it should: $args  --  $(head -1 "$S/logs/refusal.txt")"
done

echo "=== P2A PREPARE OK $(date -Is)"
echo "FOREGROUND_END p2a-prepare $(date -Is) rc=0"
