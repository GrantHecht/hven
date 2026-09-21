#!/usr/bin/env bash
# M7 W2 M3b task A -- THE PRE-RUN P-SYM DERIVATION, one CONFIGURATION per
# invocation (so each is its own lock hold and no leg runs long).
#
# WHY IT EXISTS. M3a fix round 1's G5 FAILED because its declared mover shape
# was derived over the wrong universe: it hashed every object in a dev build
# tree (181) while `scripts/psym_compare.sh` captures a DECLARED SUBSET (146),
# deliberately excluding the targets that recompile library TUs under
# HVEN_TESTING. This task adds a THIRD such target
# (`hven_qp_walk_seam_tests`), so getting the universe right is not a
# formality here -- it is the same error waiting to be repeated. This script
# therefore derives the shape from THE CAPTURE'S OWN MANIFEST and from nothing
# else, and it prints the manifest's object count so the declaration can be
# read against the universe it was taken over.
#
# WHAT IT MEASURES, and it is three separate properties of the PRODUCTION
# library rather than one (the settler's addendum to this task's brief):
#
#   BYTE       -- `cmp` on each `CMakeFiles/hven.dir/**.o` pair.
#   SYMBOL     -- P-SYM's own per-symbol disassembly verdict for the object.
#   RELOCATION -- P-SYM's own relocation comparison for the object.
#
# A Debug object may legitimately differ in BYTES while being identical in the
# other two: a header that gains a line moves the DWARF line programme of every
# inline function defined below the insertion point, and `objdump -d` does not
# print debug information. That is a real distinction and this script reports
# it rather than collapsing it.
#
# IT IS A DERIVATION, NOT A GATE. It declares nothing and asserts nothing; the
# numbers it prints are transcribed INTO `gates-m3b.sh` as this round's declared
# shape BEFORE that chain runs, exactly as M3a fix1's `psym-derivation.txt` was
# meant to be.
#
# USAGE: derive_psym_m3b.sh <Release|Debug>   (run inside the box lock)
set -uo pipefail
CFG="${1:?usage: derive_psym_m3b.sh <Release|Debug>}"
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m3b
BD=$S/psym-build
OUT=$S/derive
ART=$ROOT/docs/notes/data/2026-09-m7-w2-m3b-minor-trace
BEFORE=${BEFORE:-9fee5545}

mkdir -p "$OUT"
cd "$ROOT" || exit 90
[ -z "$(git status --porcelain)" ] || { echo "REFUSED: the tree is not clean"; exit 2; }
HEAD_SHA=$(git rev-parse HEAD)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
echo "=== DERIVE P-SYM $CFG  after=$(git rev-parse --short HEAD)  before=$BEFORE"

CHANGED_HEADERS=$(git diff --name-only "$BEFORE" -- include/ | tr '\n' ' ')
CHANGED_SOURCES=$(git diff --name-only "$BEFORE" -- src/ tests/ | tr '\n' ' ')
echo "=== CHANGED HEADERS: $CHANGED_HEADERS"
echo "=== CHANGED SOURCES: $CHANGED_SOURCES"

# The classifier is copied OUT of the tree first: the `before` arm below checks
# out $BEFORE, at which this task's wrappers do not exist.
cp "$ART/wrappers/classify_psym_m3b.py" "$S/classify_psym_m3b.py" || exit 93

rm -rf "$BD"
cmake -S "$ROOT" -B "$BD" -G Ninja -DCMAKE_BUILD_TYPE="$CFG" \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON \
  > "$OUT/configure-$CFG.log" 2>&1 || { tail -5 "$OUT/configure-$CFG.log"; exit 91; }
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-after-$CFG.log" 2>&1 \
  || { tail -30 "$OUT/build-after-$CFG.log"; exit 91; }

rm -rf "$OUT/after-$CFG" "$OUT/after-obj-$CFG"
"$ROOT/scripts/psym_compare.sh" capture "$OUT/after-$CFG" "$BD" > "$OUT/capture-after-$CFG.log" 2>&1
echo "CAPTURE_AFTER_RC=$?"
mkdir -p "$OUT/after-obj-$CFG"
(cd "$BD" && find CMakeFiles/hven.dir -name '*.o' | LC_ALL=C sort) > "$OUT/libobjs-$CFG.txt"
while IFS= read -r o; do
    mkdir -p "$OUT/after-obj-$CFG/$(dirname "$o")"
    cp "$BD/$o" "$OUT/after-obj-$CFG/$o"
done < "$OUT/libobjs-$CFG.txt"
cp "$BD"/lib*/libhven.a "$OUT/libhven-after-$CFG.a" 2>/dev/null || \
  cp "$BD"/libhven.a "$OUT/libhven-after-$CFG.a" 2>/dev/null || echo "NOTE: no libhven.a found"

restore() { git -C "$ROOT" checkout -q "$BRANCH" 2>/dev/null || git -C "$ROOT" checkout -q "$HEAD_SHA"; }
trap restore EXIT
git checkout -q --detach "$BEFORE" || exit 92
CCACHE_DISABLE=1 cmake --build "$BD" -j "$(nproc)" > "$OUT/build-before-$CFG.log" 2>&1 \
  || { tail -30 "$OUT/build-before-$CFG.log"; restore; exit 91; }
rm -rf "$OUT/before-$CFG" "$OUT/before-obj-$CFG"
"$ROOT/scripts/psym_compare.sh" capture "$OUT/before-$CFG" "$BD" > "$OUT/capture-before-$CFG.log" 2>&1
echo "CAPTURE_BEFORE_RC=$?"
mkdir -p "$OUT/before-obj-$CFG"
while IFS= read -r o; do
    mkdir -p "$OUT/before-obj-$CFG/$(dirname "$o")"
    [ -f "$BD/$o" ] && cp "$BD/$o" "$OUT/before-obj-$CFG/$o"
done < "$OUT/libobjs-$CFG.txt"
git checkout -q "$BRANCH"
trap - EXIT
echo "tree restored to $(git rev-parse --abbrev-ref HEAD) $(git rev-parse --short HEAD)"

echo "=== MANIFEST UNIVERSE (the ONLY universe a declaration may be taken over)"
echo "MANIFEST_AFTER=$(wc -l < "$OUT/after-$CFG/.psym-manifest")"
echo "MANIFEST_BEFORE=$(wc -l < "$OUT/before-$CFG/.psym-manifest")"
echo "MANIFEST_HAS_QP_WALK_SEAM=$(grep -c qp_walk_seam "$OUT/after-$CFG/.psym-manifest")"

"$ROOT/scripts/psym_compare.sh" compare "$OUT/before-$CFG" "$OUT/after-$CFG" \
  > "$OUT/compare-$CFG.txt" 2>&1
echo "PSYM_COMPARE_RC_$CFG=$?"
grep -E "^(P-SYM:|DIFFERS|NOISE-ONLY|RELOCATIONS)" "$OUT/compare-$CFG.txt" | head -40

# shellcheck disable=SC2086
python3 -B "$S/classify_psym_m3b.py" --compare "$OUT/compare-$CFG.txt" --build "$BD" \
  --changed-sources $CHANGED_SOURCES --changed-headers $CHANGED_HEADERS \
  --expect-differing 0 --min-negative-control 1 > "$OUT/classification-$CFG.txt" 2>&1
echo "CLASSIFY_RC_$CFG=$?"
grep -E "^(P-SYM|DIFFERING|  class|  UNEXPLAINED|  NEGATIVE|REFUSED)" "$OUT/classification-$CFG.txt"

echo "=== THE PRODUCTION LIBRARY'S OBJECTS, BYTE BY BYTE ($CFG)"
same=0; diffn=0; missing=0
: > "$OUT/byte-differing-$CFG.txt"
while IFS= read -r o; do
    if [ ! -f "$OUT/before-obj-$CFG/$o" ]; then
        missing=$((missing + 1)); echo "ONLY-AFTER $o" >> "$OUT/byte-differing-$CFG.txt"; continue
    fi
    if cmp -s "$OUT/before-obj-$CFG/$o" "$OUT/after-obj-$CFG/$o"; then
        same=$((same + 1))
    else
        diffn=$((diffn + 1)); echo "BYTES-DIFFER $o" >> "$OUT/byte-differing-$CFG.txt"
    fi
done < "$OUT/libobjs-$CFG.txt"
echo "LIB_OBJECTS_$CFG=$(wc -l < "$OUT/libobjs-$CFG.txt")"
echo "LIB_BYTE_IDENTICAL_$CFG=$same"
echo "LIB_BYTE_DIFFERING_$CFG=$diffn"
echo "LIB_ONLY_AFTER_$CFG=$missing"
cat "$OUT/byte-differing-$CFG.txt"

echo "=== FOR EACH BYTE-DIFFERING LIBRARY OBJECT: is the difference TEXT, RELOCATIONS, or DWARF?"
while read -r tag o; do
    [ "$tag" = "BYTES-DIFFER" ] || continue
    echo "--- $o"
    objdump -d --no-show-raw-insn "$OUT/before-obj-$CFG/$o" | tail -n +2 > "$OUT/.d-before"
    objdump -d --no-show-raw-insn "$OUT/after-obj-$CFG/$o"  | tail -n +2 > "$OUT/.d-after"
    if cmp -s "$OUT/.d-before" "$OUT/.d-after"; then echo "    TEXT: identical"; else
        echo "    TEXT: DIFFERS"; diff "$OUT/.d-before" "$OUT/.d-after" | head -20; fi
    readelf -r "$OUT/before-obj-$CFG/$o" > "$OUT/.r-before" 2>/dev/null
    readelf -r "$OUT/after-obj-$CFG/$o"  > "$OUT/.r-after"  2>/dev/null
    if cmp -s "$OUT/.r-before" "$OUT/.r-after"; then echo "    RELOCATIONS: identical"; else
        echo "    RELOCATIONS: DIFFER"; diff "$OUT/.r-before" "$OUT/.r-after" | head -20; fi
    nm -C "$OUT/before-obj-$CFG/$o" | LC_ALL=C sort > "$OUT/.n-before"
    nm -C "$OUT/after-obj-$CFG/$o"  | LC_ALL=C sort > "$OUT/.n-after"
    if cmp -s "$OUT/.n-before" "$OUT/.n-after"; then echo "    SYMBOL TABLE: identical"; else
        echo "    SYMBOL TABLE: DIFFERS"; diff "$OUT/.n-before" "$OUT/.n-after" | head -20; fi
    echo "    SECTIONS THAT DIFFER:"
    for sec in $(objdump -h "$OUT/after-obj-$CFG/$o" | awk '/^ *[0-9]+ /{print $2}'); do
        objcopy -O binary --only-section="$sec" "$OUT/before-obj-$CFG/$o" "$OUT/.s-before" 2>/dev/null
        objcopy -O binary --only-section="$sec" "$OUT/after-obj-$CFG/$o"  "$OUT/.s-after"  2>/dev/null
        cmp -s "$OUT/.s-before" "$OUT/.s-after" || echo "        $sec"
    done
done < "$OUT/byte-differing-$CFG.txt"

echo "=== NO SEAM SYMBOL IN THE PRODUCTION LIBRARY ($CFG)"
if [ -f "$OUT/libhven-after-$CFG.a" ]; then
    echo "NM_QPWALK_HITS_$CFG=$(nm -C "$OUT/libhven-after-$CFG.a" 2>/dev/null | grep -c 'QpWalk')"
    echo "NM_TESTING_NS_HITS_$CFG=$(nm -C "$OUT/libhven-after-$CFG.a" 2>/dev/null | grep -c 'detail::testing')"
fi
echo "=== DERIVE $CFG DONE"
