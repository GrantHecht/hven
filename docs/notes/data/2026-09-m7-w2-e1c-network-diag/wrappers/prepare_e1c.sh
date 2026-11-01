#!/usr/bin/env bash
# M7 W2 E1c -- build the ONE probe binary and SMOKE it (brief:
# .superpowers/m7-w2-e1c-diag-brief.md).
#
# THE BUILD is M5's shape: an EXACT REPLAY of the E1 head's install build's own
# Ninja command lines (hven-bench/.hven/build-43800aae8827, the tree that made
# install-43800aae8827, the E1-RETAKE harness pin) plus `-DHVEN_TESTING=1` and
# nothing else, through `replay_build_e1c.py` -- a BYTE COPY of M4's
# `replay_build_m4.py`, never edited (checked below). The compiler is pinned by
# being the install build tree's own command strings.
#
# THE SOURCE TREE THOSE COMMANDS NAME. They compile
# /home/ghecht/Projects/hven/.claude/worktrees/e1-retake/{src,include,dep}, the
# E1-retake worktree at 43800aae, which its close-out REMOVED (2026-09-23
# 00:22). So that the command strings stay IDENTICAL (not remapped), this script
# RE-MATERIALIZES that path from `git archive 43800aae` plus the two vendored
# submodules at 43800aae's gitlinks (checked), as a plain directory (no git
# worktree registration), and REMOVES it again on exit. It creates it only if
# the path is absent, and removes only what it created.
#
# THE HEADER VIEW. The probe-side TUs read the SAME 43800aae include tree the
# library objects read (`--probe-extra-include <that tree>/include`), so the
# replay's equivalence check against the install's flattened headers must find
# an EMPTY delta. Separately, this repository's HEAD include/ is compared to the
# install: its delta must be exactly the E4-fix1 test-seam headers
# (qp_certificate_seam.h, qp_walk_trace.h), derived from git, or this STOPS.
#
# Run INSIDE the box lock by its caller's `flock`. Nothing under src/, include/
# or tests/ is written; hven-bench is READ-ONLY.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-e1c-network-diag"
S="$ROOT/.scratch/m7w2e1c"
BENCH=/home/ghecht/Projects/hven-bench
PIN=43800aae882724e3133b05b1d72708c91131e585
REL="$BENCH/.hven/build-43800aae8827"
INSTALL="$BENCH/.hven/install-43800aae8827"
LIB="$INSTALL/lib64/libhven.a"
SRC="$ROOT/.claude/worktrees/e1-retake"
M3C="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
M4="$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm"
M4C="$ROOT/docs/notes/data/2026-09-m7-w2-m4c-drop-direction-study"
PB="$S/probebuild"
TR="$S/logs/harvest-diff-e1c.txt"
EXE="$S/build/e1c_probe"
CUTEST_ROOT="$HOME/Software/cutest"
# The E4-fix1 headers, the ONLY ones allowed to differ between HEAD and the pin.
ALLOWED_HEAD_DELTA="hven/detail/qp/qp_certificate_seam.h hven/detail/qp/qp_walk_trace.h"

fin() { echo "FOREGROUND_END e1c-prepare $(date -Is) rc=$1"; exit "$1"; }

mkdir -p "$S/logs" "$S/build" "$S/smoke/rows"
echo "FOREGROUND_START e1c-prepare $(date -Is)"
if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|m5_arm_probe|e1c_probe|hven_sqp_corpus" \
     | grep -v -e "eval " -e "pgrep" -e "prepare_e1c" -e "bash -c"; then
    echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING TO BUILD"; fin 99
fi
echo "PGREP_EMPTY e1c-prepare: no foreign cell binary under my lock"
echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

for p in "$D/wrappers/replay_build_e1c.py" "$D/wrappers/e1c_probe.cpp" "$D/wrappers/identity_e1c.py" \
         "$D/wrappers/probe-CMakeLists.txt" "$M3C/wrappers/m3c_probe.cpp" \
         "$REL/build.ninja" "$REL/CMakeFiles/rules.ninja" "$LIB" "$INSTALL/HVEN_PIN.txt" \
         "$INSTALL/include/hven" "$CUTEST_ROOT"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; fin 90; }
done

echo "--- the copied wrappers against their originals (bytes, measured)"
copy_bad=0
while IFS='|' read -r mine theirs; do
    [ -n "$mine" ] || continue
    a=$(sha256sum "$D/wrappers/$mine" | cut -d' ' -f1); b=$(sha256sum "$theirs" | cut -d' ' -f1)
    if [ "$a" != "$b" ]; then echo "REFUSED: $mine is not a byte copy of $theirs"; copy_bad=1
    else echo "    $mine == $theirs  $a"; fi
done <<REF
replay_build_e1c.py|$M4/wrappers/replay_build_m4.py
readouts_m4.py|$M4/wrappers/readouts_m4.py
m4c_tails.py|$M4C/wrappers/m4c_tails.py
probe-CMakeLists.txt|$M3C/wrappers/probe-CMakeLists.txt
REF
[ "$copy_bad" -eq 0 ] || fin 96

echo "--- THE PIN"
grep -qx "HVEN_PIN_SHA $PIN" "$INSTALL/HVEN_PIN.txt" || { echo "REFUSED: install pin is not $PIN"; fin 94; }
a=$(sha256sum "$LIB" | cut -d' ' -f1); b=$(sha256sum "$REL/libhven.a" | cut -d' ' -f1)
echo "INSTALL_LIB_SHA256=$a"; echo "BUILDTREE_LIB_SHA256=$b"
[ "$a" = "$b" ] || { echo "REFUSED: the installed archive is not the build tree's"; fin 94; }
echo "$a  $LIB" > "$S/library-sha256.txt"

echo "--- THE HEAD-vs-PIN HEADER DELTA, derived from git and measured on disk"
DERIVED=$(git -C "$ROOT" diff --name-only "$PIN..HEAD" -- include/ | sed 's|^include/||' | sort)
echo "    derived ($PIN..HEAD -- include/): $(echo $DERIVED)"
for f in $DERIVED; do
    echo " $ALLOWED_HEAD_DELTA " | grep -q " $f " || { echo "REFUSED/STOP: $f differs HEAD vs pin and is not an E4-fix1 test-seam header"; fin 97; }
done
MEASURED=$(cd "$ROOT/include" && { diff -rq hven "$INSTALL/include/hven" 2>&1 || true; } \
    | grep -v gitkeep | sed -E 's|^Files (hven/[^ ]*) and .* differ$|\1|; s|^Only in ([^:]*): (.*)$|\1/\2|' \
    | sed "s|^$INSTALL/include/||" | sort)
echo "    measured (HEAD include/ vs install): $(echo $MEASURED)"
[ "$(echo $DERIVED)" = "$(echo $MEASURED)" ] || { echo "REFUSED/STOP: measured delta != derived delta"; fin 97; }
echo "HEAD_DELTA_OK -- exactly the E4-fix1 test-seam headers; the probe does NOT read HEAD's include/"

CREATED_SRC=0
cleanup_src() {
    if [ "$CREATED_SRC" -eq 1 ] && [ -f "$SRC/.e1c-materialized" ]; then
        rm -rf "$SRC"; echo "--- removed the re-materialized $SRC (this script created it)"
    fi
}
trap cleanup_src EXIT
echo "--- THE SOURCE TREE THE INSTALL COMMANDS NAME: $SRC"
if [ -e "$SRC" ]; then
    echo "REFUSED: $SRC exists; this script only re-materializes an ABSENT path"; fin 95
fi
mkdir -p "$SRC" && CREATED_SRC=1 && touch "$SRC/.e1c-materialized"
git -C "$ROOT" archive "$PIN" src include dep | tar -x -C "$SRC" || { echo "REFUSED: git archive failed"; fin 95; }
for sm in dep/eigen dep/fmt; do
    want=$(git -C "$ROOT" ls-tree "$PIN" "$sm" | awk '{print $3}')
    have=$(git -C "$ROOT/$sm" rev-parse HEAD)
    dirty=$(git -C "$ROOT/$sm" status --porcelain | wc -l)
    echo "    $sm gitlink@pin=$want checkout=$have dirty=$dirty"
    [ "$want" = "$have" ] && [ "$dirty" -eq 0 ] || { echo "REFUSED: $sm is not at the pin's gitlink"; fin 95; }
    rm -rf "$SRC/$sm"; mkdir -p "$SRC/$sm"
    git -C "$ROOT/$sm" archive HEAD | tar -x -C "$SRC/$sm" || fin 95
done
# The install's flattened hven/ headers must equal the re-materialized tree's.
if diff -rq -x .gitkeep "$SRC/include/hven" "$INSTALL/include/hven" > "$S/logs/src-vs-install.txt" 2>&1; then
    echo "SRC_INCLUDE == INSTALL_INCLUDE (hven/)"
else
    echo "REFUSED: the re-materialized include/ is not the install's"; head "$S/logs/src-vs-install.txt"; fin 95
fi

echo "--- CONFIGURE the pass-1 probe tree against the pinned install (configure only)"
rm -rf "$S/probe" "$PB"; mkdir -p "$S/probe"
cp "$M3C/wrappers/m3c_probe.cpp" "$S/probe/m3c_probe.cpp"
cp "$D/wrappers/probe-CMakeLists.txt" "$S/probe/CMakeLists.txt"
cmake -S "$S/probe" -B "$PB" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$INSTALL" \
  > "$S/logs/probe-configure.log" 2>&1 || { tail -20 "$S/logs/probe-configure.log"; fin 92; }
grep -m1 "^hven_DIR:" "$PB/CMakeCache.txt"
grep -q "^hven_DIR:PATH=$INSTALL/lib64/cmake/hven$" "$PB/CMakeCache.txt" || { echo "REFUSED: probe resolved another hven"; fin 92; }

rm -f "$TR"
echo "--- THE REPLAY BUILD (arm B == -DHVEN_TESTING=1 and nothing else)"
python3 "$D/wrappers/replay_build_e1c.py" \
    --arm B --rel "$REL" --probebuild "$PB" --install-lib "$LIB" \
    --probe-src "$D/wrappers/e1c_probe.cpp" --exe-name e1c_probe \
    --probe-extra-include "$SRC/include" --install-include "$INSTALL/include" \
    --out "$S/build" --transcript "$TR" --jobs "$(nproc)" 2>&1 | tail -30
rc=${PIPESTATUS[0]}
[ "$rc" -eq 0 ] || { echo "REFUSED: replay build rc=$rc -- see $TR"; fin "$rc"; }
[ -x "$EXE" ] || { echo "REFUSED: no executable"; fin 91; }
sha256sum "$EXE" | tee "$S/logs/binary-e1c.txt"

echo "--- ARGUMENT REFUSALS, EXERCISED"
cd "$BENCH" || fin 93
while IFS='|' read -r label args; do
    [ -n "$label" ] || continue
    # shellcheck disable=SC2086
    if "$EXE" $args > /dev/null 2>"$S/logs/refusal.txt"; then
        echo "REFUSED: \`$args\` was ACCEPTED ($label)"; fin 95
    fi
    echo "refused as it should ($label): $(head -1 "$S/logs/refusal.txt")"
done <<'REF'
unknown cell|--problem NOT_A_CELL
trace without an out path|--problem STEENBRB --trace on
an --equality-basis value that is neither on nor off|--problem ACOPP57 --equality-basis maybe
an arm label that is not one of M5's four|--problem ACOPP57 --arm Q
REF

echo "--- THE SWITCH REACHES THE OPTION (1 s probes; only the keys are read)"
for v in on off; do
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 120 "$EXE" --problem ACOPP57 --arm off --equality-basis "$v" --deadline 1 --ledger off \
        --trace off > "$S/logs/eqbcheck-$v.txt" 2>&1 || true
    want=1; [ "$v" = off ] && want=0
    grep -qx "opt_equality_basis=$want" "$S/logs/eqbcheck-$v.txt" || { echo "REFUSED: --equality-basis $v did not realize $want"; fin 97; }
    echo "    --equality-basis $v -> opt_equality_basis=$want, $(grep -m1 '^eqb_computed=' "$S/logs/eqbcheck-$v.txt")"
done

echo
echo "=========== THE SMOKE: ACOPP57 --arm off, basis at its DEFAULT, observer armed"
echo "FOREGROUND_START smoke-ACOPP57 $(date -Is)"
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
    --problem ACOPP57 --arm off --deadline 600 --ledger on \
    --trace on --trace-out "$S/smoke/on-ACOPP57.csv" \
    --capture-solve -1 --max-records 2000000 \
    > "$S/smoke/rows/ACOPP57.txt" 2> "$S/logs/smoke-ACOPP57.err"
src=$?
echo "FOREGROUND_END smoke-ACOPP57 $(date -Is) rc=$src"
[ "$src" -eq 0 ] || { echo "REFUSED: smoke leg rc=$src"; head -3 "$S/logs/smoke-ACOPP57.err"; fin "$src"; }
grep -qx "equality_basis_touched=0" "$S/smoke/rows/ACOPP57.txt" || { echo "REFUSED: smoke touched the option"; fin 98; }
python3 "$D/wrappers/identity_e1c.py" --rows "$S/smoke/rows" --cells ACOPP57 \
    > "$S/logs/gate-smoke.txt" 2>&1
grc=$?
cat "$S/logs/gate-smoke.txt"
echo "GATE_SMOKE_RC=$grc"
[ "$grc" -eq 0 ] || { echo "REFUSED: the smoke does not reproduce the E1-retake ACOPP57 row"; fin 98; }
echo "eqb report on the smoke:"; grep -E '^eqb_(computed|undecidable|dropped_count|qr_factorizations)=' "$S/smoke/rows/ACOPP57.txt" | sed 's/^/    /'
echo "=== E1c PREPARE OK $(date -Is)"
fin 0
