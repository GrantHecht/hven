#!/usr/bin/env bash
# M7 W2 M3c PASS 2 -- build the HVEN_TESTING walk-seam probe at HEAD, and prove
# the three properties the seam convention rests on BEFORE any leg runs.
#
# Nothing under src/ or include/ is touched. The harness's own CMake is never
# entered. /home/ghecht/Projects/hven-bench is READ-ONLY.
set -euo pipefail

ROOT=/home/ghecht/Projects/hven
BENCH=/home/ghecht/Projects/hven-bench
D="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m3c/p2"
mkdir -p "$S/logs" "$S/probebuild" "$S/out" "$S/traces"

CUTEST_ROOT="${HVEN_BENCH_CUTEST_ROOT:-$HOME/Software/cutest}"

echo "=== M3c PASS 2 PREPARE $(date -Is)"
echo "=== HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

# EVERY PATH HANDED TO A SIBLING IS CHECKED TO EXIST (M3b Correction C8).
for p in "$D/wrappers/m3c_p2_probe.cpp" "$D/wrappers/probe-p2-CMakeLists.txt" \
         "$ROOT/CMakeLists.txt" "$BENCH/lanes/cutest/cutest_source.h" \
         "$BENCH/lanes/hven/problem_source.h"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; exit 90; }
done
[ -d "$CUTEST_ROOT" ] || { echo "REFUSED: no CUTEst root at $CUTEST_ROOT"; exit 90; }

# The build directory takes the probe source and the CMakeLists under the names
# the CMakeLists expects; the committed originals are never built in place.
cp "$D/wrappers/m3c_p2_probe.cpp" "$S/m3c_p2_probe.cpp"
cp "$D/wrappers/probe-p2-CMakeLists.txt" "$S/CMakeLists.txt"

echo "--- CONFIGURE"
cmake -S "$S" -B "$S/probebuild" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DHVEN_ROOT="$ROOT" \
    -DHVEN_BENCH_ROOT="$BENCH" \
    -DHVEN_BENCH_CUTEST_ROOT="$CUTEST_ROOT" \
    > "$S/logs/probe-configure.log" 2>&1 \
  || { echo "CONFIGURE FAILED"; tail -40 "$S/logs/probe-configure.log"; exit 91; }
grep -E "recompiling [0-9]+ hven source" "$S/logs/probe-configure.log" || true

echo "--- BUILD"
cmake --build "$S/probebuild" --target m3c_p2_probe -j "$(nproc)" \
    > "$S/logs/probe-build.log" 2>&1 \
  || { echo "BUILD FAILED"; grep -E "error:|FAILED|undefined reference" \
       "$S/logs/probe-build.log" | head -30; exit 92; }

echo "--- PROBE SHA256"
sha256sum "$S/probebuild/m3c_p2_probe" | tee "$S/probe-sha256.txt"

echo "--- NO hven::hven IN THE LINK (M3b fix1's REPAIRED check, copied)"
# M3b's own chain grepped `CMakeFiles/<target>.dir/link.txt`, which only the
# MAKEFILE generator writes; under Ninja the file is absent, grep exited nonzero
# because the FILE was missing rather than because the pattern was, and a PASS
# was printed. The check was vacuous. This form locates the link description
# wherever the generator put it and REFUSES when it finds none.
LINKDESC=""
for cand in "$S/probebuild/CMakeFiles/m3c_p2_probe.dir/link.txt" "$S/probebuild/build.ninja"; do
    [ -s "$cand" ] && { LINKDESC=$cand; break; }
done
[ -n "$LINKDESC" ] || { echo "REFUSED: no link description under $S/probebuild"; exit 93; }
if [ "$(basename "$LINKDESC")" = build.ninja ]; then
    LINKLINE=$(grep -E "^build m3c_p2_probe: " "$LINKDESC" || true)
else
    LINKLINE=$(cat "$LINKDESC")
fi
[ -n "$LINKLINE" ] || { echo "REFUSED: $LINKDESC carries no link line for m3c_p2_probe"; exit 93; }
if printf "%s" "$LINKLINE" | grep -qE "libhven\.a|-lhven"; then
    echo "REFUSED: the probe links hven::hven -- the seam convention forbids it"; exit 93
fi
echo "the link line (from $(basename "$LINKDESC"), $(printf "%s" "$LINKLINE" | wc -c) bytes) names no libhven.a and no -lhven"

echo "--- NON-VACUITY OF THAT CHECK: the same grep FINDS libhven when one is there"
if printf "%s" "-o probe libhven.a -lfoo" | grep -qE "libhven\.a|-lhven"; then
    echo "the refusal pattern matches a link line that DOES name libhven.a"
else
    echo "REFUSED: the no-hven-link pattern cannot match a link line that names libhven.a"; exit 93
fi

echo "--- THE OBSERVER IS IN THIS BINARY"
NSYM=$(nm -C "$S/probebuild/m3c_p2_probe" | grep -c "QpWalkObserver" || true)
echo "QpWalkObserver symbols: $NSYM"
[ "$NSYM" -ge 1 ] || { echo "REFUSED: the observer is not in the probe binary"; exit 94; }

echo "--- AND IS IN NO PRODUCTION LIBRARY (pass 1's own installed libhven.a, at the same HEAD)"
LIB="$ROOT/.scratch/m7w2m3c/install/lib64/libhven.a"
if [ -s "$LIB" ]; then
    PSYM=$(nm -C "$LIB" 2>/dev/null | grep -c "QpWalkObserver" || true)
    echo "QpWalkObserver symbols in libhven.a: $PSYM"
    [ "$PSYM" -eq 0 ] || { echo "REFUSED: the production library carries the seam"; exit 94; }
else
    echo "NOTE: no pass-1 libhven.a at $LIB -- the production-side check is ABSENT, not passed"
fi

echo "--- USAGE LINE"
"$S/probebuild/m3c_p2_probe" 2>&1 | head -5 || true

echo "--- ARGUMENT REFUSALS, exercised (a checker never seen to refuse is not a checker)"
# RUN FROM THE BENCH TREE: the CUTEst source resolves its decoded problem
# directory RELATIVE to the cwd (`.scratch/h3/decoded/<CELL>`), so a refusal
# tested from anywhere else would be the wrong refusal -- as it was on this
# script's first run, which is how the `--stop-after-first-qp on` parse defect
# was found. READ-ONLY: nothing is written under the bench tree.
cd "$BENCH"
for args in "--problem STEENBRB --trace on" \
            "--problem NOT_A_CELL" \
            "--problem STEENBRB --capture-solve -7" \
            "--problem STEENBRB --trace on --trace-out /tmp/x --stop-after-first-qp on --capture-solve 3"; do
    # shellcheck disable=SC2086
    if "$S/probebuild/m3c_p2_probe" $args > /dev/null 2>"$S/logs/refusal.txt"; then
        echo "REFUSED: \`$args\` was ACCEPTED and should not have been"; exit 95
    fi
    echo "refused as it should: $args  --  $(head -1 "$S/logs/refusal.txt")"
done

echo "=== M3c PASS 2 PREPARE OK $(date -Is)"
