#!/usr/bin/env bash
# M7 W2 M3b task A -- THE THREE-PROPERTY BREAKDOWN of the production library's
# byte-differing objects, redone correctly.
#
# WHY A SECOND SCRIPT. `derive_psym_m3b.sh`'s own breakdown compared
# `objdump -d` output that still carried its FILENAME HEADER LINE, and
# `readelf -r` output that still carried each relocation section's FILE OFFSET.
# Both of those differ for reasons that are not what the comparison is asking
# about -- the two arms' objects are saved under different directories, and a
# debug section that grows by sixteen bytes moves every later section's offset
# -- so that breakdown reported TEXT: DIFFERS and RELOCATIONS: DIFFER on
# objects whose instructions and whose relocation ENTRIES are identical. It was
# wrong in the STRICT direction (it claimed a difference that is not there),
# which is the harmless direction for a gate and the useless direction for a
# description. The capture, compare, classification and BYTE counts that script
# produced are unaffected and are not re-run; this re-does the breakdown ALONE,
# from the objects it already saved, and starts no build.
#
# WHAT IT COMPARES, per object, and each one separately:
#   TEXT        -- `objdump -d --no-show-raw-insn` with the filename line and
#                  the blank line after it removed.
#   RELOCATIONS -- `readelf -r` with the `at offset 0x...` clause removed from
#                  each section header, so what is compared is the ENTRIES and
#                  their count, not where the section landed in the file.
#   SYMBOLS     -- `nm -C`, sorted.
#   SECTION SIZES -- `readelf -S`'s name/size columns, which says WHICH section
#                  changed size and by how much.
#
# USAGE: analyse_objects_m3b.sh <Release|Debug>
set -uo pipefail
CFG="${1:?usage: analyse_objects_m3b.sh <Release|Debug>}"
S=/home/ghecht/Projects/hven/.scratch/m7w2m3b
OUT=$S/derive
B="$OUT/before-obj-$CFG"
A="$OUT/after-obj-$CFG"
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT

[ -d "$B" ] && [ -d "$A" ] || { echo "REFUSED: no saved object pair for $CFG"; exit 2; }
[ -s "$OUT/libobjs-$CFG.txt" ] || { echo "REFUSED: no object list for $CFG"; exit 2; }

norm_text() { objdump -d --no-show-raw-insn "$1" | sed '1,2d'; }
norm_reloc() { readelf -r "$1" | sed -E "s/ at offset 0x[0-9a-f]+ / at offset OFFSET /"; }
norm_syms() { nm -C "$1" | LC_ALL=C sort; }
norm_secs() { readelf -S -W "$1" | awk '$1 ~ /^\[/ || $2 ~ /^\[/ {print}' | sed -E 's/^ *//' ; }

echo "=== THREE-PROPERTY BREAKDOWN ($CFG), over the PRODUCTION LIBRARY's objects"
total=0; bytes_same=0; bytes_diff=0
text_diff=0; reloc_diff=0; sym_diff=0
while IFS= read -r o; do
    total=$((total + 1))
    if cmp -s "$B/$o" "$A/$o"; then bytes_same=$((bytes_same + 1)); continue; fi
    bytes_diff=$((bytes_diff + 1))
    echo "--- $o"
    norm_text "$B/$o" > "$T/tb"; norm_text "$A/$o" > "$T/ta"
    if cmp -s "$T/tb" "$T/ta"; then echo "    TEXT:        identical ($(wc -l < "$T/ta") disassembly lines)"
    else echo "    TEXT:        DIFFERS"; diff "$T/tb" "$T/ta" | head -20; text_diff=$((text_diff + 1)); fi
    norm_reloc "$B/$o" > "$T/rb"; norm_reloc "$A/$o" > "$T/ra"
    if cmp -s "$T/rb" "$T/ra"; then echo "    RELOCATIONS: identical ($(grep -c 'R_X86' "$T/ra") records)"
    else echo "    RELOCATIONS: DIFFER"; diff "$T/rb" "$T/ra" | head -20; reloc_diff=$((reloc_diff + 1)); fi
    norm_syms "$B/$o" > "$T/nb"; norm_syms "$A/$o" > "$T/na"
    if cmp -s "$T/nb" "$T/na"; then echo "    SYMBOLS:     identical ($(wc -l < "$T/na") entries)"
    else echo "    SYMBOLS:     DIFFER"; diff "$T/nb" "$T/na" | head -20; sym_diff=$((sym_diff + 1)); fi
    norm_secs "$B/$o" > "$T/sb"; norm_secs "$A/$o" > "$T/sa"
    echo "    SECTION HEADERS that differ (name/size/offset columns):"
    diff "$T/sb" "$T/sa" | head -20 | sed 's/^/        /'
done < "$OUT/libobjs-$CFG.txt"

echo "=== SUMMARY ($CFG)"
echo "LIB_OBJECTS=$total"
echo "BYTE_IDENTICAL=$bytes_same"
echo "BYTE_DIFFERING=$bytes_diff"
echo "OF THOSE, TEXT_DIFFERING=$text_diff"
echo "OF THOSE, RELOCATION_DIFFERING=$reloc_diff"
echo "OF THOSE, SYMBOL_DIFFERING=$sym_diff"
echo "=== ANALYSE $CFG DONE"
