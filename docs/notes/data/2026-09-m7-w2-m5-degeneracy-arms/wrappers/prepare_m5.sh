#!/usr/bin/env bash
# M7 W2 M5 -- build the ONE probe binary the four arms run, and SMOKE it.
#
# ONE BINARY, FOUR ARMS. `--arm off|a|b1|a+b1` selects the arm per leg inside
# one HVEN_TESTING build. That is not a loosening: RULE.md section 8's OFF
# control is what proves the `off` position of that switch is production
# behaviour, and it is proved against a row produced by a binary that had no
# arm in it at all.
#
# The build is M3c's EXACT REPLAY of the install build's own Ninja command
# lines plus `-DHVEN_TESTING=1` and nothing else, through
# `replay_build_m5.py` -- a BYTE COPY of M4's `replay_build_m4.py`, which is
# never edited. The compiler is pinned by being the install build tree's own:
# every command string is the one Ninja would run, expanded from that tree's
# rules, `${LAUNCHER}` included.
#
# Run INSIDE the box lock by its caller's `flock`; prints the protocol markers
# itself. Nothing under src/ or include/ is touched; hven-bench is READ-ONLY.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m5-degeneracy-arms"
M3C="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m5arm"
REL="$ROOT/.scratch/m7w2m3c/rel"
PB="$ROOT/.scratch/m7w2m3c/probebuild"
LIB="$ROOT/.scratch/m7w2m3c/install/lib64/libhven.a"
LIBSHA="$ROOT/.scratch/m7w2m3c/library-sha256.txt"
INSTALL_INC="$ROOT/.scratch/m7w2m3c/install/include"
TR="$S/logs/harvest-diff-m5.txt"
EXE="$S/build/m5_arm_probe"

# THE INSTALL'S OWN COMMIT. The header-tree equivalence check compares this
# SOURCE tree against the INSTALL's flattened header tree, and that install was
# taken at M3c's head. The declared delta must therefore be derived from THAT
# base -- `f5205770` -- and not from this round's own base `f255bae9`: M4's
# `qp_anti_cycling_arm.h` is in the source tree and absent from the install, so
# a delta derived from `f255bae9..HEAD` would omit it and the check would
# REFUSE a difference that is real, declared and older than this task. Both
# derivations are printed below, so a reader sees this round's own two headers
# inside the install-relative three.
INSTALL_BASE=f5205770
ROUND_BASE=f255bae9

mkdir -p "$S/logs" "$S/rows" "$S/traces" "$S/build"

echo "FOREGROUND_START m5-prepare $(date -Is)"
if pgrep -a -f "hven_bench_cell|m3c_probe|m3c_p2_probe|m3c_p2a_probe|m4_arm_probe|m5_arm_probe|hven_sqp_corpus" \
     | grep -v -e "eval " -e "pgrep" -e "prepare_m5"; then
    echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING TO BUILD"
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=99"
    exit 99
fi
echo "PGREP_EMPTY m5-prepare: no foreign cell binary under my lock"

echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

# EVERY PATH HANDED TO A SIBLING IS CHECKED TO EXIST (M3b C8).
for p in "$D/wrappers/replay_build_m5.py" "$D/wrappers/m5_arm_probe.cpp" \
         "$D/wrappers/gate_m5.py" "$D/RULE.md" \
         "$REL/build.ninja" "$REL/CMakeFiles/rules.ninja" \
         "$PB/build.ninja" "$PB/CMakeFiles/rules.ninja" "$LIB" "$LIBSHA" \
         "$INSTALL_INC/hven" "$M3C/rows/ACOPP57.txt"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "FOREGROUND_END m5-prepare $(date -Is) rc=90"; exit 90; }
done

# THE THREE WRAPPERS COPIED FROM M4/M4c ARE BYTE-IDENTICAL TO THEIR ORIGINALS.
# "Copy, never edit" is a claim about bytes, so it is MEASURED here rather than
# asserted in a comment.
echo "--- the copied wrappers against their originals"
copy_bad=0
while IFS='|' read -r mine theirs; do
    [ -n "$mine" ] || continue
    a=$(sha256sum "$D/wrappers/$mine" 2>/dev/null | cut -d' ' -f1)
    b=$(sha256sum "$theirs" 2>/dev/null | cut -d' ' -f1)
    if [ -z "$a" ] || [ "$a" != "$b" ]; then
        echo "REFUSED: $mine is not a byte copy of $theirs"
        copy_bad=1
    else
        echo "    $mine == $(basename "$theirs")  $a"
    fi
done <<REF
replay_build_m5.py|$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/replay_build_m4.py
gate_m5.py|$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm/wrappers/gate_m4.py
m4c_tails.py|$ROOT/docs/notes/data/2026-09-m7-w2-m4c-drop-direction-study/wrappers/m4c_tails.py
REF
if [ "$copy_bad" -ne 0 ]; then
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=96"; exit 96
fi

# THE ARCHIVE SHA IS CHECKED AGAINST M3c's RECORDED PIN, not merely recorded
# again. The replay reads THIS archive's build tree; an archive that is not the
# one M3c's rows were produced against would make the OFF control a comparison
# between two different libraries wearing one name.
echo "--- the install archive against M3c's recorded pin"
want_sha=$(awk '{print $1}' "$LIBSHA")
got_sha=$(sha256sum "$LIB" | awk '{print $1}')
echo "RECORDED_LIB_SHA256=$want_sha"
echo "MEASURED_LIB_SHA256=$got_sha"
if [ "$want_sha" != "$got_sha" ]; then
    echo "REFUSED: the install archive is not the one M3c pinned"
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=94"
    exit 94
fi
echo "the archive is M3c's own, byte for byte"

echo "--- THE HEADER-TREE DELTA, DERIVED (never typed), both ways"
DELTA_INSTALL=$(git -C "$ROOT" diff --name-only "$INSTALL_BASE..HEAD" -- include/ | sed 's|^include/||')
DELTA_ROUND=$(git -C "$ROOT" diff --name-only "$ROUND_BASE..HEAD" -- include/ | sed 's|^include/||')
echo "    install-relative ($INSTALL_BASE..HEAD): $(echo $DELTA_INSTALL)"
echo "    this round's own  ($ROUND_BASE..HEAD): $(echo $DELTA_ROUND)"
# THE ROUND'S OWN DELTA MUST BE A SUBSET OF THE INSTALL-RELATIVE ONE, or the
# two derivations describe different trees and neither can be trusted.
for f in $DELTA_ROUND; do
    echo "$DELTA_INSTALL" | grep -qx "$f" || {
        echo "REFUSED: $f is in this round's delta but not in the install-relative one"
        echo "FOREGROUND_END m5-prepare $(date -Is) rc=96"; exit 96; }
done
echo "    the round's $(echo $DELTA_ROUND | wc -w) header(s) are inside the install-relative $(echo $DELTA_INSTALL | wc -w)"

rm -f "$TR"
echo "--- THE REPLAY BUILD (arm B == -DHVEN_TESTING=1 and nothing else)"
# shellcheck disable=SC2086
python3 "$D/wrappers/replay_build_m5.py" \
    --arm B \
    --rel "$REL" \
    --probebuild "$PB" \
    --install-lib "$LIB" \
    --probe-src "$D/wrappers/m5_arm_probe.cpp" \
    --exe-name m5_arm_probe \
    --probe-extra-include "$ROOT/include" \
    --install-include "$INSTALL_INC" \
    --expect-include-delta $DELTA_INSTALL \
    --out "$S/build" \
    --transcript "$TR" \
    --jobs "$(nproc)" 2>&1 | tail -40
rc=${PIPESTATUS[0]}
if [ "$rc" -ne 0 ]; then
    echo "REFUSED: replay build rc=$rc -- see $TR"
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=$rc"
    exit "$rc"
fi

[ -x "$EXE" ] || { echo "REFUSED: no executable at $EXE"; echo "FOREGROUND_END m5-prepare $(date -Is) rc=91"; exit 91; }
echo "--- THE BINARY"
sha256sum "$EXE" | tee "$S/logs/binary-m5.txt"

echo "--- USAGE LINE (the binary runs)"
"$EXE" 2>&1 | head -6

echo "--- ARGUMENT REFUSALS, EXERCISED (a checker never seen to refuse is not a checker)"
cd /home/ghecht/Projects/hven-bench || { echo "REFUSED: no bench tree"; echo "FOREGROUND_END m5-prepare $(date -Is) rc=93"; exit 93; }
refusals=0
while IFS='|' read -r label args; do
    [ -n "$label" ] || continue
    # shellcheck disable=SC2086
    if "$EXE" $args > /dev/null 2>"$S/logs/refusal.txt"; then
        echo "REFUSED: \`$args\` was ACCEPTED and should not have been ($label)"
        echo "FOREGROUND_END m5-prepare $(date -Is) rc=95"
        exit 95
    fi
    refusals=$((refusals + 1))
    echo "refused as it should ($label): $(head -1 "$S/logs/refusal.txt")"
done <<'REF'
unknown cell|--problem NOT_A_CELL
trace without an out path|--problem STEENBRB --trace on
an arm label that is not one of the four|--problem ACOPP57 --arm Q
M4's arm label, which this probe does NOT carry|--problem ACOPP57 --arm E
a bare `a+` that is not `a+b1`|--problem ACOPP57 --arm a+
a non-positive dependency tolerance|--problem ACOPP57 --arm b1 --dependency-tol 0
a negative dependency-test ceiling|--problem ACOPP57 --arm b1 --max-dep-tests -1
the inertia diagnostic asked for on an UNARMED run|--problem ACOPP57 --arm off --diag-inertia on
REF
echo "ARGUMENT_REFUSALS_EXERCISED=$refusals"

echo "--- THE FOUR ARM LABELS ARE ACCEPTED, and a+b1 is grepped as a FIXED STRING"
for a in off a b1 a+b1; do
    if timeout 120 "$EXE" --problem ACOPP57 --arm "$a" --deadline 1 --ledger off --trace off \
         > "$S/logs/armcheck-$a.txt" 2>&1; then :; else
        echo "    note: the 1 s probe at --arm $a exited non-zero, which is expected -- only the keys are read"
    fi
    got=$(grep -m1 -F 'arm=' "$S/logs/armcheck-$a.txt" | head -1)
    echo "    --arm $a -> $got"
    if ! grep -qxF "arm=$a" "$S/logs/armcheck-$a.txt"; then
        echo "REFUSED: --arm $a did not print arm=$a"
        echo "FOREGROUND_END m5-prepare $(date -Is) rc=97"; exit 97
    fi
done

echo "--- THE ARM IS REACHABLE AND THE BINARY SAYS SO (arm_compiled_in must be 1)"
if grep -qx 'arm_compiled_in=1' "$S/logs/armcheck-off.txt"; then
    echo "ARM_COMPILED_IN=1 -- the switch is reachable from outside src/ and include/"
else
    echo "REFUSED: arm_compiled_in is not 1 -- the arm is not reachable (RULE.md section 8)"
    grep -E '^arm' "$S/logs/armcheck-off.txt" | head
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=97"
    exit 97
fi

echo "--- AMENDMENT 1's TWO KEYS ARE PRINTED, and the per-solve one is a HARVEST"
# A counter the probe does not harvest is indistinguishable from a counter the
# arm never moved, so the KEY's presence is asserted here rather than
# discovered in a read-out. Its VALUE on a 1 s run is not a claim.
for k in indep_max_cond_r indep_leak_bound_exceeded scaled_candidates_refused; do
    grep -qE "^$k=" "$S/logs/armcheck-a+b1.txt" || {
        echo "REFUSED: $k is not printed at --arm a+b1"
        echo "FOREGROUND_END m5-prepare $(date -Is) rc=97"; exit 97; }
    echo "    $(grep -m1 -E "^$k=" "$S/logs/armcheck-a+b1.txt")"
done

# ---------------------------------------------------------------------------
echo
echo "=========== THE SMOKE: ACOPP57 at --arm off against M3c's COMMITTED ROW"
echo "RULE.md section 8 step 1. The reference is M3c's pass-1 row, produced by a"
echo "binary with NO arm and NO observer compiled into it. The bar is EVERY"
echo "field the two rows share, and this round adds one: ZERO near-ulp"
echo "acceptances -- the reproduction must be by BYTES, not by tolerance."
mkdir -p "$S/smoke/rows"
echo "FOREGROUND_START smoke-off-ACOPP57 $(date -Is)"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
    --problem ACOPP57 --arm off --deadline 600 --ledger on \
    --trace on --trace-out "$S/smoke/off-ACOPP57.csv" \
    --capture-solve -1 --max-records 2000000 \
    > "$S/smoke/rows/off-ACOPP57.txt" 2> "$S/logs/smoke-off-ACOPP57.err"
smoke_rc=$?
echo "FOREGROUND_END smoke-off-ACOPP57 $(date -Is) rc=$smoke_rc"
if [ "$smoke_rc" -ne 0 ]; then
    echo "REFUSED: the smoke leg exited rc=$smoke_rc -- nothing is filled in later"
    head -3 "$S/logs/smoke-off-ACOPP57.err" | sed 's/^/    /'
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=$smoke_rc"
    exit "$smoke_rc"
fi

# `trace_out` IS EXCLUDED AS A LABEL, by name and not as a prefix (M4's
# close-out rule). M3c's row does not carry the key at all, so the exclusion is
# expected to report `(none)`; it is passed anyway, because a gate that would
# have compared a path if one appeared is not the gate this step declares.
python3 "$D/wrappers/gate_m5.py" \
    --p1-rows "$M3C/rows" \
    --leg-dir "$S/smoke/rows" \
    --cells ACOPP57 \
    --arms off \
    --rule-outcome off \
    --exclude-label trace_out \
    --note "M7 W2 M5 -- THE OFF SMOKE. This task's own binary at \`--arm off\`,
compared against M3c's COMMITTED pass-1 row for ACOPP57 on EVERY field the two
rows share. Ordinary (non-strict) mode, because the two rows come from two
DIFFERENTLY BUILT binaries and CLAUDE.md section 7's near-ulp allowance is
exactly the case for that -- but this step additionally requires ZERO near-ulp
acceptances, asserted by its caller from this transcript, so the reproduction is
by bytes. \`trace_out\` is excluded AS A LABEL (exactly one field, not a
prefix). A failure here STOPS the chain: no arm is entered." \
    --out "$S/logs/gate-smoke.txt" > /dev/null 2>&1
echo "GATE_SMOKE_RC=$?"
grep -F "  off-ACOPP57 " "$S/logs/gate-smoke.txt" | tail -2
grep -E "^  (fields compared|near-ulp acceptances|VERDICT)" "$S/logs/gate-smoke.txt" | tail -5

if ! grep -q "VERDICT: REPRODUCES" "$S/logs/gate-smoke.txt"; then
    echo "REFUSED: the OFF smoke does not reproduce M3c's committed ACOPP57 row"
    sed -n '/VERDICT: DOES NOT REPRODUCE/,+6p' "$S/logs/gate-smoke.txt" | sed 's/^/    /'
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=98"
    exit 98
fi
nulp=$(grep -oE "near-ulp acceptances \(passed by tolerance, not by bytes\): [0-9]+" \
       "$S/logs/gate-smoke.txt" | tail -1 | grep -oE "[0-9]+$")
echo "NEAR_ULP_ACCEPTANCES=$nulp"
if [ "$nulp" != "0" ]; then
    echo "REFUSED: the smoke reproduces only WITH $nulp near-ulp acceptance(s);"
    echo "         this step declares zero, and a loosened bar after seeing the"
    echo "         result is not a bar"
    echo "FOREGROUND_END m5-prepare $(date -Is) rc=98"
    exit 98
fi
echo "SMOKE OK -- every shared field reproduces BY BYTES, with 0 near-ulp acceptances"

echo "=== M5 PREPARE OK $(date -Is)"
echo "FOREGROUND_END m5-prepare $(date -Is) rc=0"
