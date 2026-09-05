#!/usr/bin/env bash
# P-SYM: per-symbol disassembly identity between two builds, with the accepted
# noise class classified mechanically rather than eyeballed.
#
# WHAT P-SYM IS. The phase-C proof vocabulary's per-batch default for a
# relocation commit is "the code the compiler emits did not change." A
# relocation moves files, so object bytes are NOT expected to be identical --
# `__FILE__` moves with the file and `__LINE__` moves with every inserted line,
# and both reach the object. What must be identical is the INSTRUCTIONS, per
# symbol. This script draws that line in one place so every batch draws it the
# same way, instead of each agent re-inventing a comparison and then widening
# it by hand when the first legitimate difference appears.
#
# USAGE
#
#   scripts/psym_compare.sh capture <snapshot-dir> [build-dir]
#   scripts/psym_compare.sh compare [options] <before-dir> <after-dir>
#
#     --object-map <file>   old-object-relpath => new-object-relpath
#     --symbol-map <file>   old-demangled-symbol => new-demangled-symbol
#     --exceptions <file>   demangled-symbol ## reason it may differ
#
# Typical session, from a clean configure, CCACHE_DISABLE=1, same host, same
# preset (a cache hit would replay a stored object instead of compiling the
# source in front of you, which is the thing under test):
#
#   CCACHE_DISABLE=1 cmake --build build -j6
#   scripts/psym_compare.sh capture /tmp/psym-before
#   ... make the change ...
#   CCACHE_DISABLE=1 cmake --build build -j6
#   scripts/psym_compare.sh capture /tmp/psym-after
#   scripts/psym_compare.sh compare /tmp/psym-before /tmp/psym-after
#
# PRECONDITION, AND IT IS THE ONE MOST EASILY BROKEN: BOTH ARMS MUST BE BUILT
# AT THE SAME ABSOLUTE BUILD-DIRECTORY PATH. Note that the session above uses
# the literal path `build` for both captures, and that is not incidental. The
# build directory's absolute path reaches an object through more than one route
# -- `__FILE__` expansions in anything compiled out of the build tree, the
# configure-time stamps CMake generates into sources, and assertion/throw
# strings built from a source path -- so two arms compiled at, say,
# `build-before/` and `build-after/` will differ in ways that have nothing to
# do with the change under test, and the differences land in `.rodata`, where
# this script's TEXT comparison cannot see them to classify them. What it CAN
# see is the knock-on: a `.rodata` size change shifts later sections, which
# moves symbol addresses, which changes the rip-relative annotations
# `normalize()` strips -- so the failure mode is not a clean signal but a
# quiet, partial one. Configure ONE build directory, capture the before arm,
# rebuild IN PLACE, capture the after arm. This script cannot check the
# precondition for you: a capture snapshot records object bytes, not the path
# they came from, and inferring the path from the bytes is exactly the
# `.rodata` archaeology the comparison is defined not to do.
#
# Exit status is the verdict: 0 iff every object is either byte-identical or
# differs ONLY within the accepted noise class defined below, AND every object
# and every symbol on BOTH sides is accounted for (matched, mapped, or named in
# the exceptions file). Any other difference exits 1 and is printed with the
# offending instruction pair.
#
# THE OBJECT SET is the one the phase-C plan names for P-SYM, plus the three
# targets M6 W5 T0 added (see below): `libhven.a`, every `hven_sqp_tests`,
# `hven_interior_tests` and `hven_model_tests` object, and the objects of the
# bench binaries the asserted artifacts come from (`hven_sqp_corpus`,
# `hven_sqp_bench`, `hven_sqp_ssn_safeguard_probe`, `hven_sqp_f7_cold`,
# `hven_sqp_tau_bar_sweep_probe`, `hven_sqp_crossover`). Debug objects are NOT
# part of P-SYM -- they differ trivially, and Debug correctness is P-SUITE's
# job.
#
# The last two of the original bench set joined in phase-C T6 (2026-08-19).
# They were omitted originally because their artifacts are archived studies
# rather than gated baselines -- but three consecutive T-tasks (T4, T5, T6)
# each had to declare them as objects their blast radius covered and the script
# did not compare, and each had to reconcile that arithmetic by hand in its
# report. A probe the instrument cannot see is a probe whose regressions nobody
# is looking for; `bench/CMakeLists.txt` already makes the same argument for
# BUILDING them ("an uncompiled probe rots silently"), and this is that
# argument applied one step further. Both are ordinary
# `bench/CMakeFiles/<target>.dir` object sets, so nothing about the comparison
# changes -- only its coverage.
#
# `hven_interior_tests`, `hven_model_tests` and `hven_sqp_crossover` joined in
# M6 W5 T0 (2026-09-05) for the same reason one step further again: W5 changes
# the IPM callback surface, the model layer's type names and the driver's TU
# structure, and the two suites that exercise those surfaces -- plus the
# crossover runner that links the bench-side adapter -- were outside the
# instrument. Targets deliberately still OUTSIDE the set, so their absence is a
# stated choice rather than an oversight: `hven_tests` (the core/linear suite,
# untouched by the engine work P-SYM guards), `hven_fault_injection_tests` and
# `hven_ipqp_seam_tests` (both recompile library TUs a second time under
# `HVEN_TESTING`, so their objects are not the production library's code and
# would report a permanent, meaningless difference against it), the
# `hven_golden_rig*` targets (they compile against pinned OLD-SEAM checkouts,
# not this tree alone), `hven_sqp_ipqp_e1_arm` (an archived one-off arm with no
# gated artifact), the `hven_compile_fail_*` targets (they are expected NOT to
# compile), and `hven_sqp_snopt_f7` (optional, and gated behind a source
# firewall this repository does not cross).
#
# Adding a target here WIDENS what must be identical, so it can only make the
# instrument stricter, never laxer. A snapshot captured before such a change
# has fewer entries in its `.psym-manifest` than one captured after. That used
# to be a silent skip; since M6 W5 T0 it cannot be, on two counts: `compare`
# now walks BOTH manifests (an object present only in the after arm is a
# finding, not an omission), and every capture records the sha256 of the script
# that made it in `.psym-version`, which `compare` refuses to proceed past when
# the two arms disagree. Capture both arms with the same version of this
# script; the runner stamps its own sha into the compare header so a report
# says which version drew the line.
#
# THE ACCEPTED NOISE CLASS, stated so that widening it is a visible act:
#
#   (a) `__LINE__`-class -- an immediate-constant move whose two sides are the
#       SAME instruction with the SAME destination, differing only in the
#       immediate: `mov $0xNNN,%reg` or `movl $0xNNN,offset(%reg)`. This is how
#       `__LINE__` reaches an object: GoogleTest's EXPECT_/ASSERT_ macros, and
#       every assert/throw site, materialize the line number as an immediate.
#       Inserting or deleting a line above such a site shifts the constant and
#       nothing else.
#
#   (b) `__FILE__`-class -- string CONTENT. It is accepted by construction
#       rather than by rule: `objdump -d` disassembles executable sections, and
#       a file-name string lives in `.rodata`. A changed `__FILE__` therefore
#       does not appear in this comparison at all, and the rip-relative
#       reference to it disassembles identically (displacement 0 plus a
#       relocation) in both objects. This script proves TEXT identity and says
#       nothing about `.rodata`; that is exactly the licensed difference, not a
#       gap being papered over.
#
# NORMALIZATION NOTE: `objdump -d --no-show-raw-insn` appends a trailing
# annotation comment to rip-relative and branch/call instructions -- e.g.
# `lea 0x0(%rip),%rax        # 7 <sym+0x7>` -- carrying an absolute or
# section-relative target address. That address is a function of overall
# layout (e.g. an unrelated object's size shifting a later section), not of
# the instruction's own bytes, so two builds with byte-identical instructions
# can show different annotations and vice versa. `normalize()` strips
# everything from the first ` #` (space then comment marker) to end-of-line
# before comparison. This was checked against every disassembled line in this
# repo's build tree (149k+ `#`-bearing lines across all P-SYM objects): the
# marker is always a single ` #` per line, always preceded by whitespace, and
# always of the form `# <hex> <sym+off>` -- objdump never emits a bare `#`
# inside operand text under `--no-show-raw-insn`, so this strip cannot mangle
# a legitimate instruction.
#
# The same layout-derived argument applies to a SECOND annotation objdump
# emits inline on branch/call targets rather than as a trailing comment --
# e.g. `call   27f5 <.L.str.137+0x466>` -- where the bracketed
# `<symbol+off>` is objdump's nearest-symbol-by-address label for the target,
# not part of the instruction: a `.rodata` size change elsewhere in the
# object shifts what symbol is nearest, so the SAME call to the SAME address
# gets a different label across builds. `normalize()` strips a trailing
# `<...>` group the same way it strips the trailing `#` comment, leaving the
# target address and the mnemonic/operands untouched -- only the cosmetic
# symbolization goes.
#
# Everything else is a real difference. In particular an instruction-COUNT
# change is never noise: the script reports it as STRUCTURAL and refuses to
# classify that object further, because once the counts differ, pairing lines
# positionally stops meaning anything.
#
# THE ONE KNOWN LIMIT OF CLASS (a), stated rather than hidden: a genuine change
# to a numeric constant in an otherwise untouched instruction is
# indistinguishable from a line-number shift by shape alone. That is why the
# compare step PRINTS the set of observed immediate deltas per object. A line
# shift produces a small number of distinct deltas, each equal to a count of
# inserted/deleted lines; a changed constant shows up as a lone odd delta with
# no siblings. Read that summary -- it is the half of the classification a
# regex cannot do for you.
#
# COVERAGE LIMIT (inherited from P-SYM itself): the Accelerate `#ifdef` arms
# are not compiled on Linux at all, so nothing here says anything about them.
#
# ---------------------------------------------------------------------------
# M6 W5 T0: BIDIRECTIONAL COVERAGE, DEMANGLED-NAME MATCHING, MAPS, EXCEPTIONS
# ---------------------------------------------------------------------------
#
# WHY. Up to W5 this script compared normalized listings POSITIONALLY within
# each object and walked only the BEFORE manifest. That is exactly right for a
# relocation -- the same TU compiled from a moved file -- and exactly wrong for
# the two things M6 W5 does: T6 SPLITS a translation unit (one old object's
# symbols land in two new objects, and every symbol's start address moves), and
# T8 RENAMES types (every mangled name changes, and Itanium mangling encodes
# identifier LENGTHS -- `17AggregateEvalSeam` becomes `16AssemblyEvalSeam` --
# so no textual substitution on a mangled name can be trusted). Positional
# pairing under either change reports STRUCTURAL noise and proves nothing.
#
# WHAT WAS ADDED, and what deliberately did NOT change:
#
#   * The `capture` mode is unchanged except for the wider object set and the
#     new `.psym-version` stamp.
#
#   * The NO-MAP path through `compare` is unchanged. For a group of one old
#     object and one new object with no map entry in play, this script still
#     runs the byte comparison first and then the SAME positional
#     classification, emitting the SAME `IDENTICAL` / `NOISE-ONLY` /
#     `DIFFERS` / `MOVED` / `MISSING` lines and the same trailing
#     `P-SYM: N objects - ...` summary line. A relocation batch reads exactly
#     as it did before. The per-symbol layer engages only where that
#     positional comparison FAILS, or where a map is in play -- so it can only
#     turn a former FAIL into a classified, named finding, never a former PASS
#     into anything else.
#
#   * SYMBOL ACCOUNTING is new and runs on EVERY object in the set, both
#     arms, always. It is `nm --defined-only` -- every DEFINED symbol, text and
#     data alike -- put through `c++filt` by demangle_table(), which is also
#     where the disambiguating tag of the next paragraph is computed; no
#     textual surgery is ever performed on a mangled name. Each old symbol name
#     then has the symbol map applied to it;
#     the result must appear on the new side, and every new-side symbol must be
#     claimed by exactly one old-side symbol. A symbol on one side only is a
#     FINDING (`ONLY-BEFORE` / `ONLY-AFTER`), not a silent skip. This is what
#     makes the coverage bidirectional at the symbol level; the object level is
#     bidirectional because `compare` now walks the AFTER manifest too and
#     reports any object no BEFORE object claimed.
#
#     Why symbol accounting is not redundant with the instruction comparison:
#     it runs even on byte-identical objects (where it is free -- `nm` is
#     milliseconds against `objdump -d`'s tens of seconds on the large test
#     objects) so the report can state a real matched-symbol count, and it
#     covers data symbols (vtables, typeinfo, string tables) that a text-only
#     disassembly never sees but a type rename certainly moves.
#
#   * INSTRUCTION IDENTITY BY DEMANGLED NAME. Where the positional path does
#     not apply or does not pass, the two sides' disassemblies are split into
#     per-symbol blocks and paired by DEMANGLED name (through the symbol map),
#     not by position. Within a matched pair the classifier is the same awk
#     program the positional path uses -- byte for byte, one copy, called
#     twice -- so the accepted noise class is identical in both paths and
#     widening it is still a single visible act.
#
#     A THIRD layout artifact the per-symbol path excludes, and the positional
#     path does not: TRAILING inter-function alignment padding. objdump
#     attributes the assembler's inter-function nops to the symbol they follow,
#     so a function that was followed by another in the same section and is now
#     last in its own object loses them. That is a consequence of the split, not
#     of the code; only a run at the END of a block is trimmed, padding inside a
#     function is compared, and the count of symbols whose trailing padding
#     differed is reported on the PERSYM line so the exclusion is visible.
#
#     Two things are deliberately NOT compared per symbol: the symbol's start
#     ADDRESS (it is layout -- under a TU split every address moves, and P-SYM
#     is defined as per-symbol INSTRUCTION identity, see
#     docs/notes/2026-08-21-m4-plan-gate-review-hven.md:85) and the symbol's
#     own name inside its section name (`.text._Zold` vs `.text._Znew` is the
#     rename, not a code change). The section name IS compared, with the
#     symbol's own mangled name replaced by a placeholder on both sides, so a
#     symbol that moved from `.text` to `.text.unlikely` is still a finding.
#
#   * COLLISION DETECTION. The symbol map is rejected if two lines share an
#     old name or share a new name. Per object, it is an error if two old
#     symbols end up claiming one new name -- whether because two map entries
#     point at it, or because a map target is a name that ALREADY exists on the
#     old side unmapped. A map that quietly merges two symbols would let a real
#     deletion pass as a rename, which is the one failure mode a rename proof
#     must not have.
#
#   * EXCEPTIONS. `--exceptions <file>` names symbols allowed to differ, or to
#     exist on one side only, each WITH a reason. The reason is printed
#     verbatim on the line that consumes it, so a report cannot contain an
#     unexplained exception. An exception nobody used is printed as
#     `STALE-EXCEPTION` (it does not fail the run -- it is a claim that is no
#     longer needed, not a difference).
#
# FILE FORMATS (comments are whole lines beginning with optional spaces then
# `#`; blank lines ignored; leading/trailing spaces on each field trimmed):
#
#   --object-map    <old relpath> => <new relpath>
#                   Relative to the snapshot root, i.e. exactly as the paths
#                   appear in `.psym-manifest`. REPEAT the same old path on
#                   several lines to declare a SPLIT: the old object's symbols
#                   are then matched against the UNION of the listed new
#                   objects. Many-to-one is rejected (two old objects may not
#                   claim one new object).
#
#   --symbol-map    <old demangled> => <new demangled>
#                   Split on the FIRST ` => ` (space-arrow-space). Demangled
#                   C++ names do not contain that sequence; mangled names are
#                   never written in these files, precisely because their
#                   length prefixes make textual substitution unsound.
#
#   --exceptions    <demangled name> ## <reason>
#                   Split on the FIRST ` ## `. The name is the NEW-side name
#                   for a symbol that exists after, the OLD-side name for one
#                   that existed only before, and either for a mapped pair.
#
# CALIBRATION, so a later run has a baseline to recognize. Phase-C task C0.3
# (commit 6875712, a comments-and-docs-only change) was measured with exactly
# this method: 60 objects, 54 byte-identical -- including all of `libhven.a`
# and all of `bench/` -- and the 6 that differed were exactly the 6 directly
# edited TUs, with identical instruction counts and all 885 changed
# instructions in class (a). That is the floor: a comment-only edit perturbs
# only the edited TU, and only through `__LINE__`. A relocation batch should
# show that class and nothing else. The per-object DELTAS summaries are what
# make that readable: each of the six reported a single delta equal to the
# number of comment lines inserted above the affected assertions (+7, +8, +7,
# +5, +6, and +7/+9 for the one file edited in two places).
#
# The same calibration was run in the negative, because a classifier that
# cannot fail proves nothing: adding two real statements to one of those TUs
# and re-comparing reports it as STRUCTURAL (the normalized listing gains
# lines) and exits 1, with the other 59 objects still byte-identical.
#
# M6 W5 T0 re-calibrated the extended tool the same way, in both directions,
# on 117 objects and 118240 defined symbols; the transcripts are pasted in
# `.superpowers/w5-t0-report.md`.
#
#   (a) a FULL rebuild of the same commit into the SAME absolute build path
#       with CCACHE_DISABLE=1 (`ninja -t clean` then rebuild, so every object
#       is genuinely recompiled rather than replayed): 117/117 byte-identical,
#       118240 symbols matched, 0 findings, PASS.
#
#   (b) a deliberate one-symbol rename -- `hven::solvers::census_variable_bounds`
#       to `..._probe`, its declaration, its definition and both call sites,
#       with the fmt string literals left alone so nothing but the name moves.
#       WITHOUT a map: 113 byte-identical, the two CALLER objects noise-only
#       with `CHANGED 0` (their bytes differ only in the relocation's symbol
#       name, which is not an instruction), and `ipqp_trace.cpp.o` reporting
#       exactly one `ONLY-BEFORE` and one `ONLY-AFTER` -- FAIL, one unmatched
#       symbol on each side. WITH a one-line symbol map: the same object
#       reports `115 identical, 0 differing, 1 matched through the symbol map`
#       and the run PASSes. The mutant was reverted; it is not committed.

set -euo pipefail

usage() {
    echo "usage: $0 capture <snapshot-dir> [build-dir]" >&2
    echo "       $0 compare [--object-map <f>] [--symbol-map <f>] [--exceptions <f>] <before-dir> <after-dir>" >&2
    exit 2
}

OBJDUMP="${OBJDUMP:-objdump}"
NM="${NM:-nm}"

# This script's own identity, stamped into every capture and printed by every
# compare. `compare` refuses two captures whose stamps disagree: the object SET
# is a property of the capturing script, so an old-vs-new pairing would compare
# different universes and call the missing half nothing at all.
PSYM_SELF="${BASH_SOURCE[0]}"
tool_sha() {
    sha256sum "${PSYM_SELF}" | cut -d' ' -f1
}

# The P-SYM object set, enumerated from a build directory. Kept in one function
# so `capture` and any future caller cannot disagree about what P-SYM covers.
psym_objects() {
    local build="$1"
    find "${build}/CMakeFiles/hven.dir" -name '*.o' 2>/dev/null | sort
    local suite
    for suite in sqp interior model; do
        find "${build}/tests/${suite}/CMakeFiles/hven_${suite}_tests.dir" -name '*.o' 2>/dev/null | sort
    done
    local target
    for target in hven_sqp_corpus hven_sqp_bench hven_sqp_ssn_safeguard_probe \
                  hven_sqp_f7_cold hven_sqp_tau_bar_sweep_probe hven_sqp_crossover; do
        find "${build}/bench/CMakeFiles/${target}.dir" -name '*.o' 2>/dev/null | sort
    done
}

do_capture() {
    local snapshot="$1"
    local build="${2:-build}"

    if [ ! -d "${build}" ]; then
        echo "psym_compare: no such build directory: ${build}" >&2
        exit 1
    fi

    mkdir -p "${snapshot}"
    : > "${snapshot}/.psym-manifest"

    local count=0 obj rel
    while IFS= read -r obj; do
        rel="${obj#"${build}"/}"
        mkdir -p "${snapshot}/$(dirname "${rel}")"
        cp "${obj}" "${snapshot}/${rel}"
        echo "${rel}" >> "${snapshot}/.psym-manifest"
        count=$((count + 1))
    done < <(psym_objects "${build}")

    if [ -f "${build}/libhven.a" ]; then
        cp "${build}/libhven.a" "${snapshot}/libhven.a"
        echo "libhven.a" >> "${snapshot}/.psym-manifest"
        count=$((count + 1))
    fi

    if [ "${count}" -eq 0 ]; then
        echo "psym_compare: captured nothing from ${build} -- is it built?" >&2
        exit 1
    fi
    tool_sha > "${snapshot}/.psym-version"
    echo "captured ${count} objects from ${build} into ${snapshot}"
    echo "psym_compare: tool sha256 $(tool_sha)"
}

# Normalized disassembly: instruction text only, no addresses, no raw bytes,
# no file-name header, no trailing address-target annotation, no inline
# call/jump-target symbolization (see the NORMALIZATION NOTE above). Symbol
# header lines ("0000... <_Zfoo>:") survive deliberately -- they are what
# makes this a PER-SYMBOL comparison rather than one flat instruction stream,
# so a symbol that moved between sections shows up as a difference instead of
# cancelling out.
normalize() {
    "${OBJDUMP}" -d --no-show-raw-insn "$1" | tail -n +3 \
        | sed -e 's/^ *[0-9a-f]*://' -e 's/[ \t]\+#.*$//' -e 's/[ \t]*<[^<>]*>[ \t]*$//'
}

# Scratch directory for the normalized listings. Deliberately NOT a `local` in
# do_compare: the EXIT trap runs after that function has returned, and a local
# would be out of scope by then -- which under `set -u` turns a clean pass into
# a spurious failure.
#
# cleanup_tmp's own exit status matters: bash adopts an EXIT trap's exit
# status as the whole script's exit status whenever that status is nonzero
# (this is how a genuine `set -e` abort survives the trap, since the trap
# itself then exits 0 and the prior nonzero status is left standing -- but it
# also means a trap that itself exits nonzero silently overwrites a real
# success). `[ -n "${PSYM_TMP}" ] && rm -rf ...` exits nonzero via its own
# short-circuit whenever PSYM_TMP is unset -- true throughout `capture` mode,
# which never touches PSYM_TMP -- turning every successful capture into a
# reported failure. The `if` form below exits 0 when there is nothing to
# clean up, so the trap only ever reports failure for a genuine `rm` failure.
PSYM_TMP=""
cleanup_tmp() {
    if [ -n "${PSYM_TMP}" ]; then
        rm -rf "${PSYM_TMP}"
    fi
}
trap cleanup_tmp EXIT

# ---------------------------------------------------------------------------
# The classifier. ONE copy, used by the positional path and the per-symbol
# path alike, so the accepted noise class cannot drift between them. Reads two
# normalized listings and writes its verdict to stdout; exit 0 = within the
# noise class, 1 = unclassified differences, 2 = structural.
# ---------------------------------------------------------------------------
PSYM_CLASSIFY_AWK='
# Accepted noise class (a): an immediate-constant move, same instruction and
# same destination on both sides, differing only in the immediate.
function is_imm_move(s) {
    return s ~ /^\tmov[lqbw]?[ \t]+\$0x[0-9a-f]+,(%[a-z0-9]+|-?0x[0-9a-f]+\(%[a-z0-9]+(,%[a-z0-9]+,[0-9])?\)|\(%[a-z0-9]+\))$/
}
function redact(s) { gsub(/\$0x[0-9a-f]+/, "$IMM", s); return s }
function imm(s) { if (match(s, /\$0x[0-9a-f]+/)) return substr(s, RSTART + 3, RLENGTH - 3); return "" }

NR == FNR { b[FNR] = $0; nb = FNR; next }
{ a[FNR] = $0; na = FNR }
END {
    if (nb != na) {
        printf "STRUCTURAL: normalized listing is %d lines vs %d -- instructions were added or removed\n", nb, na
        exit 2
    }
    changed = 0; bad = 0; insn_b = 0
    for (i = 1; i <= nb; i++) {
        if (b[i] ~ /^\t/) insn_b++
        if (b[i] == a[i]) continue
        changed++
        if (is_imm_move(b[i]) && is_imm_move(a[i]) && redact(b[i]) == redact(a[i])) {
            d = strtonum("0x" imm(a[i])) - strtonum("0x" imm(b[i]))
            deltas[d]++
        } else {
            bad++
            if (bad <= 5) printf "  UNCLASSIFIED  - %s\n                + %s\n", b[i], a[i]
        }
    }
    if (bad > 5) printf "  ... and %d more unclassified differences\n", bad - 5
    ds = ""
    for (d in deltas) ds = ds sprintf(" %+d(x%d)", d, deltas[d])
    printf "COUNTS %d insns; CHANGED %d; UNCLASSIFIED %d; DELTAS%s\n", insn_b, changed, bad, ds
    exit (bad > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# Map / exception file loading. Each emits a TAB-separated two-column file.
# ---------------------------------------------------------------------------
load_pairs() {
    # $1 = file, $2 = separator (" => " or " ## "), $3 = human name for errors
    local file="$1" sep="$2" what="$3"
    awk -v sep="${sep}" -v what="${what}" -v file="${file}" '
        { line = $0 }
        line ~ /^[[:space:]]*#/ { next }
        line ~ /^[[:space:]]*$/ { next }
        {
            i = index(line, sep)
            if (i == 0) {
                printf "psym_compare: %s: line %d has no \"%s\" separator: %s\n", file, FNR, sep, line > "/dev/stderr"
                bad = 1
                next
            }
            l = substr(line, 1, i - 1)
            r = substr(line, i + length(sep))
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", l)
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", r)
            if (l == "" || r == "") {
                printf "psym_compare: %s: line %d has an empty %s field: %s\n", file, FNR, what, line > "/dev/stderr"
                bad = 1
                next
            }
            printf "%s\t%s\n", l, r
        }
        END { if (bad) exit 1 }
    ' "${file}"
}

# ---------------------------------------------------------------------------
# Symbol enumeration: every DEFINED symbol of an object, MANGLED. Demangling
# happens later, through demangle_table(), because the demangled form alone is
# not a key: a destructor emits D0/D1/D2 variants (and a constructor C1/C2/C3)
# that all demangle to one string, and `nm -C` would silently merge them.
# Emits "<type letter>\t<mangled name>".
# ---------------------------------------------------------------------------
obj_symbols() {
    local obj
    for obj in "$@"; do
        "${NM}" --defined-only "${obj}" 2>/dev/null
    done | sed -nE 's/^[0-9a-fA-F]+[[:space:]]+([A-Za-z?])[[:space:]]+(.*)$/\1\t\2/p' \
         | awk -F'\t' '!seen[$2]++'
}

# ---------------------------------------------------------------------------
# Mangled -> (bare demangled, disambiguating tag). Reads a sorted, unique list
# of mangled names on stdin; writes "<mangled>\t<demangled>\t<tag>".
#
# The tag is EMPTY unless several mangled names in this object share one
# demangled name -- the ctor/dtor variant case above, essentially. When they
# do, the tag is the Itanium variant marker (`[C1]`, `[D2]`, ...) taken from
# the mangled name, which a type RENAME does not touch, so a tagged key
# survives the rename the map describes. The `[#n]` fallback (deterministic
# rank in sorted-mangled order) exists so two genuinely indistinguishable
# names are still kept apart rather than merged: merging them would let a
# deleted symbol hide behind a surviving twin.
#
# The tag is appended AFTER the symbol map is applied, so map files are always
# written in plain demangled names and never mention a variant marker.
# ---------------------------------------------------------------------------
demangle_table() {
    local names
    names="$(mktemp)"
    cat > "${names}"
    paste -d'\t' "${names}" <(c++filt < "${names}") \
        | awk -F'\t' '
            # The structural, identifier-free parts of a mangled name: the
            # thunk prefix (`_ZThn8_`, `_ZTv0_n24_` -- the adjustment is NOT in
            # the demangled text) and the ctor/dtor variant marker. Both are
            # untouched by a type rename, which is what makes them usable as a
            # tag on both arms of a rename comparison.
            function tagof(mg,   t) {
                t = ""
                if (match(mg, /^_ZT[a-zA-Z]+[0-9]*_(n[0-9]+_)?/)) t = t "[" substr(mg, 1, RLENGTH) "]"
                if (match(mg, /[CD][0-9]E/)) t = t "[" substr(mg, RSTART, 2) "]"
                return t
            }
            { mg[NR] = $1; bare[NR] = $2; cnt[$2]++ }
            END {
                for (i = 1; i <= NR; i++) {
                    tg[i] = (cnt[bare[i]] > 1) ? tagof(mg[i]) : ""
                    cnt2[bare[i] tg[i]]++
                }
                for (i = 1; i <= NR; i++) {
                    t = tg[i]
                    # Still indistinguishable: fall back to the rank in
                    # sorted-mangled order rather than merging two symbols,
                    # because a merge would let a deleted symbol hide behind a
                    # surviving twin.
                    if (cnt2[bare[i] t] > 1) { rank[bare[i] t]++; t = t "[#" rank[bare[i] t] "]" }
                    printf "%s\t%s\t%s\n", mg[i], bare[i], t
                }
            }'
    rm -f "${names}"
}

# ---------------------------------------------------------------------------
# The keying both comparison layers share: a symbol is identified by its
# DEMANGLED name, with the symbol map applied on the before arm, plus the
# disambiguating tag demangle_table() computed. Prepended verbatim to both awk
# programs so the two layers cannot key differently.
# ---------------------------------------------------------------------------
PSYM_KEYING_AWK='
function keyof(mangled, is_before,   d) {
    d = (mangled in dem) ? dem[mangled] : mangled
    if (is_before && (d in m)) d = m[d]
    return d ((mangled in tag) ? tag[mangled] : "")
}
function bareof(mangled) { return (mangled in dem) ? dem[mangled] : mangled }
function load_demangle(f,   line, n1, n2) {
    while ((getline line < f) > 0) {
        n1 = index(line, "\t")
        n2 = index(substr(line, n1 + 1), "\t") + n1
        dem[substr(line, 1, n1 - 1)] = substr(line, n1 + 1, n2 - n1 - 1)
        tag[substr(line, 1, n1 - 1)] = substr(line, n2 + 1)
    }
    close(f)
}
function load_pairs_into(f, arr,   line, n) {
    while ((getline line < f) > 0) { n = index(line, "\t"); arr[substr(line, 1, n - 1)] = substr(line, n + 1) }
    close(f)
}
'


# ---------------------------------------------------------------------------
# Symbol COVERAGE, both arms, every object. Reads two "<type>\t<mangled>"
# listings; applies the symbol map to the before arm; reports any name that
# exists on one side only. Writes a TAB-separated stat line to `f_stat` so the
# caller never has to parse prose.
# ---------------------------------------------------------------------------
PSYM_SYMCOVER_AWK='
BEGIN {
    FS = "\t"
    load_pairs_into(f_map, m)
    load_pairs_into(f_exc, exc)
    load_demangle(f_dem)
}
NR == FNR {
    key = keyof($2, 1)
    if (key in b) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", key; coll++ }
    b[key] = $1
    borig[key] = bareof($2)
    if (bareof($2) in m) wasmapped[key] = 1
    next
}
{ a[keyof($2, 0)] = $1 }
END {
    for (k in b) {
        if (k in a) { matched++; if (k in wasmapped) mapped++; continue }
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", k, exc[k]; continue }
        if (borig[k] in exc) { excused++; used[borig[k]] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", borig[k], exc[borig[k]]; continue }
        onlyb++
        if (onlyb <= 20) printf "  ONLY-BEFORE   %s\n", k
    }
    if (onlyb > 20) printf "  ... and %d more symbols present only in the before arm\n", onlyb - 20
    for (k in a) {
        if (k in b) continue
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", k, exc[k]; continue }
        onlya++
        if (onlya <= 20) printf "  ONLY-AFTER    %s\n", k
    }
    if (onlya > 20) printf "  ... and %d more symbols present only in the after arm\n", onlya - 20
    for (k in used) print k >> f_used
    close(f_used)
    printf "%d\t%d\t%d\t%d\t%d\t%d\n", matched + 0, mapped + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0 > f_stat
    close(f_stat)
    exit (onlyb + onlya + coll > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# INSTRUCTION identity per symbol, paired by DEMANGLED name through the symbol
# map. Reads the two flattened listings produced by flatten_symbols(). The
# noise classification is not re-implemented here: each differing block is
# handed to the ONE classifier in `f_cls`, so both paths accept exactly the
# same class.
# ---------------------------------------------------------------------------
PSYM_PERSYM_AWK='
function field3(s,   i, j) { i = index(s, "\t"); j = index(substr(s, i + 1), "\t") + i; return substr(s, j + 1) }
# Literal (never regex) replacement -- a mangled name can contain "." and "$",
# and treating it as a pattern would corrupt the section name it edits.
function lit_replace(hay, needle, repl,   p, out) {
    out = ""
    while ((p = index(hay, needle)) > 0) { out = out substr(hay, 1, p - 1) repl; hay = substr(hay, p + length(needle)) }
    return out hay
}
# TRAILING inter-function alignment padding. objdump attributes the nops the
# assembler inserts between two functions to the symbol they follow, so a
# function that used to be followed by another in the same section and is now
# last in its own object loses them -- a pure layout consequence of a TU split,
# not a code change. Only a run at the very END of a block is trimmed, and only
# on the per-symbol path; padding INSIDE a function (loop alignment) is code and
# is compared. The positional path is untouched by this.
function is_pad(s) {
    return s ~ /^\t(cs[ \t]+)?(data16[ \t]+)*(nop[lwqb]?([ \t]|$)|xchg[ \t]+%ax,%ax$)/
}
BEGIN {
    FS = "\t"
    load_pairs_into(f_map, m)
    load_pairs_into(f_exc, exc)
    load_demangle(f_dem)
    fb = tmpd "/blk-b.txt"
    fa = tmpd "/blk-a.txt"
}
NR == FNR {
    if ($1 == "S") {
        k = keyof($2, 1)
        bcur = k
        if (k in bsec) { printf "  COLLISION     two before-arm symbols claim one name: %s\n", k; coll++; bskip = 1 }
        else {
            bsec[k] = lit_replace($3, $2, "<SYM>")
            bn[k] = 0
            bskip = 0
            borig[k] = bareof($2)
            if (bareof($2) in m) bmapped[k] = 1
        }
    } else if (!bskip && bcur != "") { bn[bcur]++; bi[bcur, bn[bcur]] = field3($0) }
    next
}
{
    if ($1 == "S") {
        k = keyof($2, 0)
        acur = k
        # A split can legitimately place the SAME weak/COMDAT symbol in both
        # new objects. Keep the first copy, count the rest, and say so.
        if (k in asec) { adup[k]++; askip = 1 }
        else { asec[k] = lit_replace($3, $2, "<SYM>"); an[k] = 0; askip = 0 }
    } else if (!askip && acur != "") { an[acur]++; ai[acur, an[acur]] = field3($0) }
}
END {
    for (k in bsec) {
        if (!(k in asec)) {
            if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", k, exc[k]; continue }
            if (borig[k] in exc) { excused++; used[borig[k]] = 1; printf "  EXCEPTION     ONLY-BEFORE %s ## %s\n", borig[k], exc[borig[k]]; continue }
            onlyb++
            printf "  ONLY-BEFORE   %s\n", k
            continue
        }
        if (bsec[k] != asec[k]) {
            printf "  SECTION-MOVED %s: %s -> %s\n", k, bsec[k], asec[k]
            diff++
            continue
        }
        nb = bn[k]; while (nb > 0 && is_pad(bi[k, nb])) nb--
        na = an[k]; while (na > 0 && is_pad(ai[k, na])) na--
        if (bn[k] - nb != an[k] - na) padtrim++
        same = (nb == na)
        if (same) { for (i = 1; i <= nb; i++) if (bi[k, i] != ai[k, i]) { same = 0; break } }
        if (same) { ident++; if (k in bmapped) mapped++; continue }
        printf "" > fb
        printf "" > fa
        for (i = 1; i <= nb; i++) print bi[k, i] > fb
        for (i = 1; i <= na; i++) print ai[k, i] > fa
        close(fb)
        close(fa)
        cmd = "awk -f " f_cls " " fb " " fa " 2>&1"
        verdict = ""
        while ((cmd | getline l) > 0) verdict = verdict (verdict == "" ? "" : "\n") l
        status = close(cmd)
        if (status == 0) {
            noise++
            if (k in bmapped) mapped++
            printf "  NOISE-ONLY    %s\n                %s\n", k, verdict
        } else if (k in exc) {
            excused++; used[k] = 1
            printf "  EXCEPTION     DIFFERS %s ## %s\n", k, exc[k]
        } else if (borig[k] in exc) {
            excused++; used[borig[k]] = 1
            printf "  EXCEPTION     DIFFERS %s ## %s\n", borig[k], exc[borig[k]]
        } else {
            diff++
            printf "  DIFFERS       %s\n", k
            n = split(verdict, vl, "\n")
            for (i = 1; i <= n; i++) printf "                %s\n", vl[i]
        }
    }
    for (k in asec) {
        if (k in bsec) continue
        if (k in exc) { excused++; used[k] = 1; printf "  EXCEPTION     ONLY-AFTER  %s ## %s\n", k, exc[k]; continue }
        onlya++
        printf "  ONLY-AFTER    %s\n", k
    }
    for (k in adup) printf "  DUPLICATE     %s is defined %d times across the after-arm objects (weak/COMDAT; the first copy is the one compared)\n", k, adup[k] + 1
    for (k in used) print k >> f_used
    close(f_used)
    printf "PERSYM %d identical, %d noise-only, %d differing, %d only-before, %d only-after, %d excepted, %d collisions, %d matched through the symbol map, %d with unequal trailing alignment padding\n",
           ident + 0, noise + 0, diff + 0, onlyb + 0, onlya + 0, excused + 0, coll + 0, mapped + 0, padtrim + 0
    exit (diff + onlyb + onlya + coll > 0) ? 1 : 0
}
'

# ---------------------------------------------------------------------------
# Per-symbol flattening of an ALREADY-NORMALIZED listing (so the expensive
# `objdump -d` runs once per object per arm, whichever path consumes it).
# Emits, for each symbol:
#   S<TAB><mangled><TAB><section>
#   I<TAB><mangled><TAB><instruction line, leading tab preserved>
# ---------------------------------------------------------------------------
flatten_symbols() {
    awk '
        /^Disassembly of section / {
            sec = $0
            sub(/^Disassembly of section /, "", sec)
            sub(/:$/, "", sec)
            next
        }
        /^[0-9a-f]+ <.*>:$/ {
            name = $0
            sub(/^[0-9a-f]+ </, "", name)
            sub(/>:$/, "", name)
            cur = name
            printf "S\t%s\t%s\n", cur, sec
            next
        }
        /^\t/ { if (cur != "") printf "I\t%s\t%s\n", cur, $0 }
    ' "$1"
}

do_compare() {
    local before="" after=""
    local object_map="" symbol_map="" exceptions=""

    while [ $# -gt 0 ]; do
        case "$1" in
            --object-map) [ $# -ge 2 ] || usage; object_map="$2"; shift 2 ;;
            --symbol-map) [ $# -ge 2 ] || usage; symbol_map="$2"; shift 2 ;;
            --exceptions) [ $# -ge 2 ] || usage; exceptions="$2"; shift 2 ;;
            --) shift ;;
            -*) echo "psym_compare: unknown option $1" >&2; usage ;;
            *)  if [ -z "${before}" ]; then before="$1"
                elif [ -z "${after}" ]; then after="$1"
                else echo "psym_compare: too many positional arguments" >&2; usage
                fi
                shift ;;
        esac
    done
    [ -n "${before}" ] && [ -n "${after}" ] || usage

    local f
    for f in "${object_map}" "${symbol_map}" "${exceptions}"; do
        if [ -n "${f}" ] && [ ! -f "${f}" ]; then
            echo "psym_compare: no such file: ${f}" >&2
            exit 1
        fi
    done

    PSYM_TMP="$(mktemp -d)"
    local tmp="${PSYM_TMP}"
    printf '%s' "${PSYM_CLASSIFY_AWK}" > "${tmp}/classify.awk"
    printf '%s%s' "${PSYM_KEYING_AWK}" "${PSYM_SYMCOVER_AWK}" > "${tmp}/symcover.awk"
    printf '%s%s' "${PSYM_KEYING_AWK}" "${PSYM_PERSYM_AWK}" > "${tmp}/persym.awk"

    # --- tool-version gate -------------------------------------------------
    local self_sha ver_b ver_a
    self_sha="$(tool_sha)"
    if [ ! -f "${before}/.psym-version" ] || [ ! -f "${after}/.psym-version" ]; then
        echo "psym_compare: a capture without a .psym-version stamp cannot be compared -- it was" >&2
        echo "              made by a script version whose object SET is unknown. Recapture both arms." >&2
        exit 1
    fi
    ver_b="$(cat "${before}/.psym-version")"
    ver_a="$(cat "${after}/.psym-version")"
    if [ "${ver_b}" != "${ver_a}" ]; then
        echo "psym_compare: the two captures were made by DIFFERENT versions of this script" >&2
        echo "              before: ${ver_b}" >&2
        echo "              after:  ${ver_a}" >&2
        echo "              The object set is a property of the capturing script; comparing across" >&2
        echo "              versions would silently omit whatever the older one did not capture." >&2
        exit 1
    fi

    echo "P-SYM tool sha256 (runner):   ${self_sha}"
    echo "P-SYM tool sha256 (captures): ${ver_b}"
    if [ "${ver_b}" != "${self_sha}" ]; then
        echo "P-SYM note: both captures were made by a DIFFERENT version of this script than the"
        echo "            one now running (the classification below is this runner's)."
    fi
    echo "P-SYM maps: object-map=${object_map:-none} symbol-map=${symbol_map:-none} exceptions=${exceptions:-none}"
    echo

    # --- load the maps -----------------------------------------------------
    : > "${tmp}/objmap.tsv"
    : > "${tmp}/symmap.tsv"
    : > "${tmp}/exc.tsv"
    if [ -n "${object_map}" ]; then load_pairs "${object_map}" " => " "object-map" > "${tmp}/objmap.tsv"; fi
    if [ -n "${symbol_map}" ]; then load_pairs "${symbol_map}" " => " "symbol-map" > "${tmp}/symmap.tsv"; fi
    if [ -n "${exceptions}" ]; then load_pairs "${exceptions}" " ## " "exceptions" > "${tmp}/exc.tsv"; fi

    # Map-file level collision checks: a repeated old name, or a repeated new
    # name, is rejected before any object is looked at.
    local dup
    if [ -s "${tmp}/symmap.tsv" ]; then
        dup="$(cut -f1 "${tmp}/symmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: symbol map maps these OLD names more than once:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
        dup="$(cut -f2 "${tmp}/symmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: COLLISION -- symbol map points several old names at one new name:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
    fi
    if [ -s "${tmp}/objmap.tsv" ]; then
        dup="$(cut -f2 "${tmp}/objmap.tsv" | LC_ALL=C sort | uniq -d)"
        if [ -n "${dup}" ]; then
            echo "psym_compare: COLLISION -- object map points several old objects at one new object:" >&2
            echo "${dup}" | sed 's/^/  /' >&2
            exit 1
        fi
    fi

    : > "${tmp}/exc-used"
    : > "${tmp}/claimed"
    LC_ALL=C sort "${after}/.psym-manifest" > "${tmp}/after-manifest"

    local identical=0 noise=0 unclassified=0 moved=0 missing=0 total=0
    local mapped_objects=0 unmatched_after=0 persym_objects=0
    local sym_matched=0 sym_mapped=0 sym_only_before=0 sym_only_after=0
    local sym_exception=0 sym_collision=0
    local rc=0

    local rel path_b targets kind ntargets t
    while IFS= read -r rel; do
        [ -n "${rel}" ] || continue
        total=$((total + 1))
        path_b="${before}/${rel}"
        targets=""
        kind="direct"

        if awk -F'\t' -v k="${rel}" '$1 == k { found = 1 } END { exit found ? 0 : 1 }' "${tmp}/objmap.tsv"; then
            kind="mapped"
            while IFS= read -r t; do
                if [ ! -f "${after}/${t}" ]; then
                    echo "MAP-ERROR   ${rel} => ${t} (no such object in ${after})"
                    rc=1
                    continue
                fi
                targets="${targets}${targets:+ }${t}"
            done < <(awk -F'\t' -v k="${rel}" '$1 == k { print $2 }' "${tmp}/objmap.tsv")
            if [ -z "${targets}" ]; then
                missing=$((missing + 1))
                echo "MISSING     ${rel} (object map named no reachable counterpart)"
                rc=1
                continue
            fi
            mapped_objects=$((mapped_objects + 1))
            ntargets="$(echo "${targets}" | wc -w)"
            if [ "${ntargets}" -gt 1 ]; then
                echo "SPLIT       ${rel} -> ${targets// /, }"
            else
                echo "RENAMED     ${rel} -> ${targets}"
            fi
        elif [ -f "${after}/${rel}" ]; then
            targets="${rel}"
        else
            local candidates
            candidates="$(find "${after}" -name "$(basename "${rel}")" -type f)"
            if [ "$(echo "${candidates}" | grep -c .)" -eq 1 ] && [ -n "${candidates}" ]; then
                targets="${candidates#"${after}"/}"
                moved=$((moved + 1))
                kind="moved"
                echo "MOVED       ${rel} -> ${targets}"
            else
                missing=$((missing + 1))
                echo "MISSING     ${rel} (no counterpart in ${after})"
                rc=1
                continue
            fi
        fi

        ntargets="$(echo "${targets}" | wc -w)"
        local path_a_list=()
        for t in ${targets}; do
            if grep -qxF "${t}" "${tmp}/claimed"; then
                echo "COLLISION   ${rel} and an earlier object both claim ${t}"
                rc=1
            fi
            echo "${t}" >> "${tmp}/claimed"
            path_a_list+=("${after}/${t}")
        done

        # ---- symbol accounting (every object, both arms, always) ----------
        #
        # `libhven.a` is excluded here and only here: its members are captured
        # and accounted for individually, so counting the archive's symbols
        # again would double every one of them.
        if [ "${rel}" != "libhven.a" ]; then
            obj_symbols "${path_b}" | LC_ALL=C sort -u > "${tmp}/syms-b.tsv"
            obj_symbols "${path_a_list[@]}" | LC_ALL=C sort -u > "${tmp}/syms-a.tsv"
            cut -f2 "${tmp}/syms-b.tsv" "${tmp}/syms-a.tsv" | LC_ALL=C sort -u \
                | demangle_table > "${tmp}/syms-demangle.tsv"
            local sym_out sym_rc
            rm -f "${tmp}/symstat"
            set +e
            sym_out="$(awk -F'\t' \
                            -v f_map="${tmp}/symmap.tsv" -v f_exc="${tmp}/exc.tsv" \
                            -v f_dem="${tmp}/syms-demangle.tsv" \
                            -v f_used="${tmp}/exc-used" -v f_stat="${tmp}/symstat" \
                            -f "${tmp}/symcover.awk" \
                            "${tmp}/syms-b.tsv" "${tmp}/syms-a.tsv")"
            sym_rc=$?
            set -e
            local st
            st="$(cat "${tmp}/symstat")"
            sym_matched=$((sym_matched + $(echo "${st}" | cut -f1)))
            sym_mapped=$((sym_mapped + $(echo "${st}" | cut -f2)))
            sym_only_before=$((sym_only_before + $(echo "${st}" | cut -f3)))
            sym_only_after=$((sym_only_after + $(echo "${st}" | cut -f4)))
            sym_exception=$((sym_exception + $(echo "${st}" | cut -f5)))
            sym_collision=$((sym_collision + $(echo "${st}" | cut -f6)))
            if [ "${sym_rc}" -ne 0 ]; then
                rc=1
                echo "SYMBOLS     ${rel}: COVERAGE FINDINGS"
                echo "${sym_out}" | sed 's/^/            /'
            elif [ -n "${sym_out}" ]; then
                echo "SYMBOLS     ${rel}: exceptions consumed"
                echo "${sym_out}" | sed 's/^/            /'
            fi
        fi

        # ---- instruction identity ------------------------------------------
        if [ "${ntargets}" -eq 1 ] && cmp -s "${path_b}" "${path_a_list[0]}"; then
            identical=$((identical + 1))
            continue
        fi

        # `libhven.a` is an archive: its member objects are compared through
        # their own entries in the object set, and the archive itself carries
        # timestamps and an index that are not instructions. Report and move on.
        if [ "${rel}" = "libhven.a" ]; then
            echo "ARCHIVE     libhven.a differs -- see its member objects above; archive"
            echo "            metadata (member timestamps, symbol index) is not instructions"
            continue
        fi

        normalize "${path_b}" > "${tmp}/b.txt"
        : > "${tmp}/a.txt"
        for t in "${path_a_list[@]}"; do normalize "${t}" >> "${tmp}/a.txt"; done

        # The POSITIONAL comparison, unchanged, first. It is strictly stronger
        # than the per-symbol comparison -- it requires the symbol header lines
        # (name AND address) to line up too -- so a pass here is a pass there,
        # and reporting it exactly as before keeps a relocation batch's output
        # byte-for-byte what it has always been. Only a failure, or an object
        # the map splits, reaches the per-symbol layer.
        if [ "${ntargets}" -eq 1 ] && [ "${kind}" != "mapped" ]; then
            local out awk_rc
            set +e
            out="$(awk -f "${tmp}/classify.awk" "${tmp}/b.txt" "${tmp}/a.txt")"
            awk_rc=$?
            set -e
            if [ "${awk_rc}" -eq 0 ]; then
                noise=$((noise + 1))
                echo "NOISE-ONLY  ${rel}"
                echo "            ${out##*$'\n'}"
                continue
            fi
            echo "DIFFERS     ${rel} (positional pairing did not hold; the PER-SYMBOL line below is the verdict)"
            echo "${out}" | sed 's/^/            /'
        fi

        # ---- per-symbol, by DEMANGLED name, through the maps ---------------
        persym_objects=$((persym_objects + 1))
        flatten_symbols "${tmp}/b.txt" > "${tmp}/flat-b.tsv"
        flatten_symbols "${tmp}/a.txt" > "${tmp}/flat-a.tsv"
        cut -f2 "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv" | LC_ALL=C sort -u > "${tmp}/mangled.txt"
        demangle_table < "${tmp}/mangled.txt" > "${tmp}/demangle.tsv"

        local per_out per_rc
        set +e
        per_out="$(awk -F'\t' \
                        -v f_map="${tmp}/symmap.tsv" -v f_exc="${tmp}/exc.tsv" \
                        -v f_dem="${tmp}/demangle.tsv" -v f_used="${tmp}/exc-used" \
                        -v f_cls="${tmp}/classify.awk" -v tmpd="${tmp}" \
                        -f "${tmp}/persym.awk" \
                        "${tmp}/flat-b.tsv" "${tmp}/flat-a.tsv")"
        per_rc=$?
        set -e
        echo "PER-SYMBOL  ${rel}: ${per_out##*$'\n'}"
        if [ -n "$(echo "${per_out}" | sed '$d')" ]; then
            echo "${per_out}" | sed '$d' | sed 's/^/            /'
        fi
        if [ "${per_rc}" -ne 0 ]; then
            unclassified=$((unclassified + 1))
            rc=1
        else
            noise=$((noise + 1))
        fi
    done < "${before}/.psym-manifest"

    # --- the other direction: after-arm objects nothing claimed ------------
    LC_ALL=C sort -u "${tmp}/claimed" > "${tmp}/claimed-sorted"
    while IFS= read -r rel; do
        [ -n "${rel}" ] || continue
        unmatched_after=$((unmatched_after + 1))
        echo "ONLY-AFTER  ${rel} (no object in the before arm accounts for it)"
        rc=1
    done < <(LC_ALL=C comm -13 "${tmp}/claimed-sorted" "${tmp}/after-manifest")

    # --- exceptions nobody needed ------------------------------------------
    local stale=0
    if [ -s "${tmp}/exc.tsv" ]; then
        cut -f1 "${tmp}/exc.tsv" | LC_ALL=C sort -u > "${tmp}/exc-all"
        LC_ALL=C sort -u "${tmp}/exc-used" > "${tmp}/exc-used-sorted"
        while IFS= read -r rel; do
            [ -n "${rel}" ] || continue
            stale=$((stale + 1))
            echo "STALE-EXCEPTION  ${rel} (listed as expected to differ; nothing used it)"
        done < <(LC_ALL=C comm -23 "${tmp}/exc-all" "${tmp}/exc-used-sorted")
    fi

    echo
    echo "P-SYM: ${total} objects — ${identical} byte-identical, ${noise} differing within the accepted noise class, ${unclassified} with unclassified differences, ${moved} matched by basename after a path move, ${missing} missing"
    echo "P-SYM coverage: ${mapped_objects} objects matched through the object map, ${persym_objects} compared per symbol, ${unmatched_after} after-arm objects unaccounted for; symbols — ${sym_matched} matched (${sym_mapped} through the symbol map), ${sym_only_before} only-before, ${sym_only_after} only-after, ${sym_exception} excepted, ${sym_collision} collisions, ${stale} stale exceptions"
    if [ "${rc}" -eq 0 ]; then
        echo "P-SYM: PASS — read the DELTAS summaries above before accepting (see this script's header)"
    else
        echo "P-SYM: FAIL — differences outside the accepted noise class"
    fi
    return "${rc}"
}

[ $# -ge 2 ] || usage
case "$1" in
    capture) [ $# -le 3 ] || usage; do_capture "$2" "${3:-build}" ;;
    compare) shift; do_compare "$@" ;;
    *) usage ;;
esac
