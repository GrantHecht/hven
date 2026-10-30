#!/usr/bin/env bash
# M7 W2 M3b FIX ROUND 2 -- THE FAIL-CLOSED NON-DEBUG SECTION COMPARISON.
#
# It is FIX ROUND 1's `check_sections_fix1.sh`, copied and re-pointed at
# `section_digest_fix2.py`. Fix round 1's copy is NOT edited: it produced
# committed transcripts and a gate's evidence must stay reproducible from the
# script that produced it. THE ONLY DIFFERENCE is which digest it defaults to,
# and the fix2 digest differs from the fix1 digest only in REFUSING two
# malformed ELF shapes it used to guess at (an unsupported EI_DATA and an
# out-of-range or unterminated section-name offset). On a well-formed object the
# two digests are byte-identical, which `fold_proofs_psym_fix2.sh` folds.
#
# WHY IT EXISTS (the review's Important 1). The round that landed this seam
# proved its Debug claim -- "the byte difference is debug information and
# nothing else" -- by showing that the three differing objects had identical
# DISASSEMBLY, identical SYMBOL TABLES and identical RELOCATION records outside
# `.rela.debug_line`. That is real evidence and it is all true, but it is not
# CLOSED: `objdump -d`, `nm` and `readelf -r` between them do not look at every
# byte of an object. A changed `.rodata` constant, a changed unwind datum in
# `.eh_frame`, a changed `.comment`, an alignment pad -- any of those could have
# passed all three checks and still been a real change to the production
# library. The claim needed a check that FAILS CLOSED over the bytes rather than
# one that passes on the bytes it happens to inspect.
#
# WHAT IT DOES. For every object of the production library, in ONE configuration:
#
#   ARM 1, THE GATE -- `section_digest_fix2.py` emits, for EVERY section in
#     section-header order, its name, its header fields (type, flags, size,
#     link, info, alignment, entsize) and the sha256 of its bytes. The
#     NON-DEBUG lines of the two digests must be equal, in order and in full.
#     Sections are handled POSITIONALLY, so a COMDAT group or any other repeated
#     name cannot make two different sections collapse onto one comparison, and
#     the header fields are carried so that a section whose contents are
#     unchanged but whose flags or alignment moved is still a difference. Every
#     section that is not debug-class is in scope: `.text`, `.rodata`, every
#     `.rela.*` that is not a `.rela.debug*`, `.eh_frame`, `.comment`,
#     `.symtab`, `.strtab`, `.shstrtab`, `.group`, `.note.*`, `.bss`, all of
#     them. `sh_offset` and `sh_addr` are deliberately NOT carried: a file
#     offset shifts whenever anything earlier in the file changes size, which is
#     exactly what a debug-only change does.
#
#   ARM 2, THE CROSS-CHECK -- `objcopy --strip-debug` on both, then `cmp`. The
#     two arms must AGREE. If the stripped bytes differ while arm 1 found
#     nothing, arm 1 missed something and the run is refused; if arm 1 found a
#     difference while the stripped bytes match, one of the two is wrong and the
#     run is refused. Two independent views of the same claim, and a
#     disagreement is a refusal rather than a choice between them.
#
# DEBUG-CLASS means `.debug*`, `.zdebug*`, `.rela.debug*` and `.rel.debug*`, and
# nothing else. The list is deliberately short and is printed at the top of
# every run, because an over-wide exclusion list is precisely how a check like
# this stops failing closed.
#
# IT IS FAIL-CLOSED IN ITS OWN PLUMBING TOO. An object present on one side and
# not the other FAILS. A `readelf` or `objcopy` that exits non-zero, or produces
# no output where output is expected, FAILS. An EMPTY object list FAILS -- a
# check that inspected nothing is not a pass. There is no path through this
# script on which a difference it could not evaluate is reported as agreement.
#
# THE VERDICT PER OBJECT IS THREE-WAY, matching the classifier's:
#   BYTE-IDENTICAL   -- `cmp` on the whole object agrees.
#   DEBUG-ONLY       -- whole-object bytes differ; every non-debug section, and
#                       the stripped object, are identical.
#   NON-DEBUG-MOVER  -- anything else. ANY of these fails the run.
#
# USAGE: check_sections_fix2.sh <CFG> <before-obj-dir> <after-obj-dir> <objlist>
#   <objlist> is one object path per line, relative to both directories.
# Exit 0 iff every object is BYTE-IDENTICAL or DEBUG-ONLY and the two arms agree.
set -uo pipefail

CFG="${1:?usage: check_sections_fix2.sh <CFG> <before-obj-dir> <after-obj-dir> <objlist>}"
BEFORE_DIR="${2:?}"
AFTER_DIR="${3:?}"
OBJLIST="${4:?}"
DIGEST="${DIGEST:-$(dirname "$0")/section_digest_fix2.py}"

TMP=$(mktemp -d) || exit 90
trap 'rm -rf "$TMP"' EXIT

echo "=== NON-DEBUG SECTION COMPARISON ($CFG) -- FAIL-CLOSED"
echo "DEBUG_CLASS_PREFIXES=.debug .zdebug .rela.debug .rel.debug"
echo "BEFORE_DIR=$BEFORE_DIR"
echo "AFTER_DIR=$AFTER_DIR"
echo "DIGEST=$DIGEST"
[ -f "$DIGEST" ] || { echo "REFUSED: no section digest at $DIGEST"; exit 3; }

# Drops the DEBUG-CLASS lines of a digest, keeping header order. The list is
# short on purpose: an over-wide exclusion is exactly how a check like this
# stops failing closed.
drop_debug() {
    awk -F'\t' '$1 !~ /^\.debug/ && $1 !~ /^\.zdebug/ && $1 !~ /^\.rela\.debug/ && $1 !~ /^\.rel\.debug/' "$1"
}

n_total=0
n_identical=0
n_debug_only=0
n_mover=0
n_refused=0
: > "$TMP/movers.txt"

if [ ! -s "$OBJLIST" ]; then
    echo "REFUSED: the object list $OBJLIST is empty -- a check that inspected nothing is not a pass"
    exit 3
fi

while IFS= read -r obj; do
    [ -n "$obj" ] || continue
    n_total=$((n_total + 1))
    b="$BEFORE_DIR/$obj"
    a="$AFTER_DIR/$obj"
    if [ ! -f "$b" ] || [ ! -f "$a" ]; then
        echo "REFUSED $obj -- present on only one side (before=$([ -f "$b" ] && echo yes || echo no) after=$([ -f "$a" ] && echo yes || echo no))"
        n_refused=$((n_refused + 1))
        continue
    fi
    if cmp -s "$b" "$a"; then
        n_identical=$((n_identical + 1))
        continue
    fi

    # ---- ARM 1: every NON-DEBUG section, positionally, header fields and all -
    if ! python3 -B "$DIGEST" "$b" > "$TMP/dig-b.txt" 2>"$TMP/dig-b.err" ||
       ! python3 -B "$DIGEST" "$a" > "$TMP/dig-a.txt" 2>"$TMP/dig-a.err"; then
        echo "REFUSED $obj -- the section digest could not be taken:"
        sed 's/^/        /' "$TMP/dig-b.err" "$TMP/dig-a.err" 2>/dev/null | head -4
        n_refused=$((n_refused + 1))
        continue
    fi
    if [ ! -s "$TMP/dig-b.txt" ] || [ ! -s "$TMP/dig-a.txt" ]; then
        echo "REFUSED $obj -- the section digest is empty for one or both sides"
        n_refused=$((n_refused + 1))
        continue
    fi
    drop_debug "$TMP/dig-b.txt" > "$TMP/nd-b.txt"
    drop_debug "$TMP/dig-a.txt" > "$TMP/nd-a.txt"
    if [ ! -s "$TMP/nd-b.txt" ] || [ ! -s "$TMP/nd-a.txt" ]; then
        echo "REFUSED $obj -- no NON-DEBUG section survived the filter, which cannot be right"
        n_refused=$((n_refused + 1))
        continue
    fi

    arm1_diff=""
    if [ "$(wc -l < "$TMP/nd-b.txt")" != "$(wc -l < "$TMP/nd-a.txt")" ]; then
        arm1_diff="SECTION-COUNT($(wc -l < "$TMP/nd-b.txt")->$(wc -l < "$TMP/nd-a.txt"))"
    elif ! cmp -s <(cut -f1 "$TMP/nd-b.txt") <(cut -f1 "$TMP/nd-a.txt"); then
        arm1_diff="SECTION-LIST"
    else
        arm1_diff=$(paste <(cut -f1 "$TMP/nd-b.txt") \
                          <(cut -f2- "$TMP/nd-b.txt") <(cut -f2- "$TMP/nd-a.txt") |
                    awk -F'\t' '{ b=""; a=""; for (i=2;i<=9;i++) b=b $i "|";
                                  for (i=10;i<=17;i++) a=a $i "|";
                                  if (b != a) print $1 }' |
                    sort -u | paste -sd, -)
    fi

    # ---- ARM 2: the stripped object, byte for byte -------------------------
    objcopy --strip-debug "$b" "$TMP/strip-b.o" 2>/dev/null
    rsb=$?
    objcopy --strip-debug "$a" "$TMP/strip-a.o" 2>/dev/null
    rsa=$?
    if [ "$rsb" -ne 0 ] || [ "$rsa" -ne 0 ] || [ ! -s "$TMP/strip-b.o" ] || [ ! -s "$TMP/strip-a.o" ]; then
        echo "REFUSED $obj -- objcopy --strip-debug failed or produced nothing (rc=$rsb/$rsa)"
        n_refused=$((n_refused + 1))
        continue
    fi
    if cmp -s "$TMP/strip-b.o" "$TMP/strip-a.o"; then arm2_diff=""; else arm2_diff="STRIPPED-BYTES"; fi

    # ---- the two arms must AGREE -------------------------------------------
    if { [ -n "$arm1_diff" ] && [ -z "$arm2_diff" ]; } ||
       { [ -z "$arm1_diff" ] && [ -n "$arm2_diff" ]; }; then
        echo "REFUSED $obj -- THE TWO ARMS DISAGREE (sections='$arm1_diff' stripped='$arm2_diff'):"
        echo "        one of them is wrong, and which is not decidable from here"
        n_refused=$((n_refused + 1))
        continue
    fi

    if [ -z "$arm1_diff" ]; then
        n_debug_only=$((n_debug_only + 1))
        echo "DEBUG-ONLY       $obj"
    else
        n_mover=$((n_mover + 1))
        echo "NON-DEBUG-MOVER  $obj  sections=[$arm1_diff]"
        echo "$obj  [$arm1_diff]" >> "$TMP/movers.txt"
    fi
done < "$OBJLIST"

echo "SECTIONS_${CFG}_OBJECTS=$n_total"
echo "SECTIONS_${CFG}_BYTE_IDENTICAL=$n_identical"
echo "SECTIONS_${CFG}_DEBUG_ONLY=$n_debug_only"
echo "SECTIONS_${CFG}_NON_DEBUG_MOVERS=$n_mover"
echo "SECTIONS_${CFG}_REFUSED=$n_refused"
if [ "$n_mover" -gt 0 ]; then
    echo "--- the non-debug movers, in full:"
    cat "$TMP/movers.txt"
fi
if [ "$n_mover" -gt 0 ] || [ "$n_refused" -gt 0 ]; then
    echo "SECTIONS_${CFG}_RC=1"
    exit 1
fi
echo "SECTIONS_${CFG}_RC=0"
exit 0
