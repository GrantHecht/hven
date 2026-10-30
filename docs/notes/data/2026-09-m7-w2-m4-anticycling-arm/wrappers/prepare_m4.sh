#!/usr/bin/env bash
# M7 W2 M4 -- build the ONE probe binary the four arms run, and smoke it.
#
# ONE BINARY, FOUR ARMS. M3c's RULE-P2A needed two binaries because its arms
# were a COMPILE-TIME question (is the seam compiled in?). M4's arms are a
# RUN-TIME switch inside one HVEN_TESTING build, so there is one binary and
# `--arm off|E|L|L+` selects the arm per leg. That is not a loosening: RULE.md
# section 4.1's OFF controls are what prove the `off` position of that switch is
# production behaviour, cell by cell, against M3c's committed rows.
#
# The build is M3c's EXACT REPLAY of the install build's own Ninja command
# lines plus `-DHVEN_TESTING=1` and nothing else (RULE.md section 3.2).
#
# Run INSIDE the box lock by its caller's `flock`; prints the protocol markers
# itself. Nothing under src/ or include/ is touched; hven-bench is READ-ONLY.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm"
M3C="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m4/arm"
REL="$ROOT/.scratch/m7w2m3c/rel"
PB="$ROOT/.scratch/m7w2m3c/probebuild"
LIB="$ROOT/.scratch/m7w2m3c/install/lib64/libhven.a"
LIBSHA="$ROOT/.scratch/m7w2m3c/library-sha256.txt"
TR="$S/logs/harvest-diff-m4.txt"
EXE="$S/build/m4_arm_probe"

mkdir -p "$S/logs" "$S/rows" "$S/traces" "$S/build"

echo "FOREGROUND_START m4-prepare $(date -Is)"
if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|m4_arm_probe|hven_sqp_corpus" \
     | grep -v -e "eval " -e "pgrep" -e "prepare_m4"; then
    echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING TO BUILD"
    echo "FOREGROUND_END m4-prepare $(date -Is) rc=99"
    exit 99
fi
echo "PGREP_EMPTY m4-prepare: no foreign cell binary under my lock"

echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

# EVERY PATH HANDED TO A SIBLING IS CHECKED TO EXIST (M3b C8).
for p in "$D/wrappers/replay_build_m4.py" "$D/wrappers/m4_arm_probe.cpp" \
         "$D/RULE.md" "$REL/build.ninja" "$REL/CMakeFiles/rules.ninja" \
         "$PB/build.ninja" "$PB/CMakeFiles/rules.ninja" "$LIB" "$LIBSHA" \
         "$M3C/rows/ACOPP57.txt"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "FOREGROUND_END m4-prepare $(date -Is) rc=90"; exit 90; }
done

# THE ARCHIVE SHA IS CHECKED AGAINST M3c's RECORDED PIN, not merely recorded
# again. The replay reads THIS archive's build tree; an archive that is not the
# one M3c's rows were produced against would make every OFF control a
# comparison between two different libraries wearing one name.
echo "--- the install archive against M3c's recorded pin"
want_sha=$(awk '{print $1}' "$LIBSHA")
got_sha=$(sha256sum "$LIB" | awk '{print $1}')
echo "RECORDED_LIB_SHA256=$want_sha"
echo "MEASURED_LIB_SHA256=$got_sha"
if [ "$want_sha" != "$got_sha" ]; then
    echo "REFUSED: the install archive is not the one M3c pinned"
    echo "FOREGROUND_END m4-prepare $(date -Is) rc=94"
    exit 94
fi
echo "the archive is M3c's own, byte for byte"

rm -f "$TR"
echo "--- THE REPLAY BUILD (arm B == -DHVEN_TESTING=1 and nothing else)"
python3 "$D/wrappers/replay_build_m4.py" \
    --arm B \
    --rel "$REL" \
    --probebuild "$PB" \
    --install-lib "$LIB" \
    --probe-src "$D/wrappers/m4_arm_probe.cpp" \
    --exe-name m4_arm_probe \
    --probe-extra-include "$ROOT/include" \
    --install-include "$ROOT/.scratch/m7w2m3c/install/include" \
    --expect-include-delta $(git -C "$ROOT" diff --name-only f5205770..HEAD -- include/ \
                             | sed 's|^include/||' | tr '\n' ' ') \
    --out "$S/build" \
    --transcript "$TR" \
    --jobs "$(nproc)" 2>&1 | tail -40
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
    echo "REFUSED: replay build rc=$rc -- see $TR"
    echo "FOREGROUND_END m4-prepare $(date -Is) rc=$rc"
    exit "$rc"
fi

[ -x "$EXE" ] || { echo "REFUSED: no executable at $EXE"; echo "FOREGROUND_END m4-prepare $(date -Is) rc=91"; exit 91; }
echo "--- THE BINARY"
sha256sum "$EXE" | tee "$S/logs/binary-m4.txt"

echo "--- USAGE LINE (the binary runs)"
"$EXE" 2>&1 | head -6

echo "--- ARGUMENT REFUSALS, EXERCISED (a checker never seen to refuse is not a checker)"
cd /home/ghecht/Projects/hven-bench || { echo "REFUSED: no bench tree"; echo "FOREGROUND_END m4-prepare $(date -Is) rc=93"; exit 93; }
refusals=0
# Each of these MUST be refused. The last three are M4's own: a bad arm label,
# and the two self-contradictions between `--arm L/L+` and `--lex-full-pricing`.
while IFS='|' read -r label args; do
    [ -n "$label" ] || continue
    # shellcheck disable=SC2086
    if "$EXE" $args > /dev/null 2>"$S/logs/refusal.txt"; then
        echo "REFUSED: \`$args\` was ACCEPTED and should not have been ($label)"
        echo "FOREGROUND_END m4-prepare $(date -Is) rc=95"
        exit 95
    fi
    refusals=$((refusals + 1))
    echo "refused as it should ($label): $(head -1 "$S/logs/refusal.txt")"
done <<'REF'
unknown cell|--problem NOT_A_CELL
trace without an out path|--problem STEENBRB --trace on
an arm label that is not one of the four|--problem ACOPP57 --arm Q
L+ with full pricing switched OFF contradicts itself|--problem ACOPP57 --arm L+ --lex-full-pricing off
L with full pricing switched ON is L+|--problem ACOPP57 --arm L --lex-full-pricing on
a non-positive expand-k|--problem ACOPP57 --arm E --expand-k 0
a delta factor outside (0,1)|--problem ACOPP57 --arm E --delta0-factor 1.5
REF
echo "ARGUMENT_REFUSALS_EXERCISED=$refusals"

echo "--- THE ARM IS REACHABLE AND THE BINARY SAYS SO (arm_compiled_in must be 1)"
# A binary built without the define would print `arm_compiled_in=0` and accept
# only `--arm off`; RULE.md section 8 makes that a STOP, so it is asserted here
# rather than discovered in leg 1.
if ! timeout 300 "$EXE" --problem ACOPP57 --arm E --deadline 1 --ledger off --trace off \
      > "$S/logs/armcheck.txt" 2>&1; then
    echo "note: the 1 s probe exited non-zero, which is expected -- only the key is read"
fi
if grep -q '^arm_compiled_in=1$' "$S/logs/armcheck.txt"; then
    echo "ARM_COMPILED_IN=1 -- the switch is reachable from outside src/ and include/"
else
    echo "REFUSED: arm_compiled_in is not 1 -- the arm is not reachable (RULE.md section 8)"
    grep -E '^arm' "$S/logs/armcheck.txt" | head
    echo "FOREGROUND_END m4-prepare $(date -Is) rc=97"
    exit 97
fi

echo "=== M4 PREPARE OK $(date -Is)"
echo "FOREGROUND_END m4-prepare $(date -Is) rc=0"
