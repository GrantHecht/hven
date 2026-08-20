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
#   scripts/psym_compare.sh compare <before-dir> <after-dir>
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
# differs ONLY within the accepted noise class defined below. Any other
# difference exits 1 and is printed with the offending instruction pair.
#
# THE OBJECT SET is the one the phase-C plan names for P-SYM: `libhven.a`,
# every `hven_sqp_tests` object, and the objects of the bench binaries the
# asserted artifacts come from (`hven_sqp_corpus`, `hven_sqp_bench`,
# `hven_sqp_ssn_safeguard_probe`, `hven_sqp_f7_cold`,
# `hven_sqp_tau_bar_sweep_probe`). Debug objects are NOT part of P-SYM -- they
# differ trivially, and Debug correctness is P-SUITE's job.
#
# The last two joined the set in phase-C T6 (2026-08-19). They were omitted
# originally because their artifacts are archived studies rather than gated
# baselines -- but three consecutive T-tasks (T4, T5, T6) each had to declare
# them as objects their blast radius covered and the script did not compare,
# and each had to reconcile that arithmetic by hand in its report. A probe the
# instrument cannot see is a probe whose regressions nobody is looking for;
# `bench/CMakeLists.txt` already makes the same argument for BUILDING them
# ("an uncompiled probe rots silently"), and this is that argument applied one
# step further. Both are ordinary `bench/CMakeFiles/<target>.dir` object sets,
# so nothing about the comparison changes -- only its coverage.
#
# Adding a target here WIDENS what must be identical, so it can only make the
# instrument stricter, never laxer. A snapshot captured before this change has
# fewer entries in its `.psym-manifest` than one captured after; `compare`
# iterates the BEFORE manifest, so an old-vs-new pairing silently skips the two
# new targets rather than reporting them missing. Capture both arms with the
# same version of this script.
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

set -euo pipefail

usage() {
    echo "usage: $0 capture <snapshot-dir> [build-dir]" >&2
    echo "       $0 compare <before-dir> <after-dir>" >&2
    exit 2
}

OBJDUMP="${OBJDUMP:-objdump}"

# The P-SYM object set, enumerated from a build directory. Kept in one function
# so `capture` and any future caller cannot disagree about what P-SYM covers.
psym_objects() {
    local build="$1"
    find "${build}/CMakeFiles/hven.dir" -name '*.o' 2>/dev/null | sort
    find "${build}/tests/sqp/CMakeFiles/hven_sqp_tests.dir" -name '*.o' 2>/dev/null | sort
    local target
    for target in hven_sqp_corpus hven_sqp_bench hven_sqp_ssn_safeguard_probe \
                  hven_sqp_f7_cold hven_sqp_tau_bar_sweep_probe; do
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
    echo "captured ${count} objects from ${build} into ${snapshot}"
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

do_compare() {
    local before="$1" after="$2"
    PSYM_TMP="$(mktemp -d)"
    local tmp="${PSYM_TMP}"

    local identical=0 noise=0 unclassified=0 moved=0 missing=0 total=0
    local rc=0

    local rel path_b path_a
    while IFS= read -r rel; do
        total=$((total + 1))
        path_b="${before}/${rel}"
        path_a="${after}/${rel}"

        # A relocation batch renames TUs, so an object may legitimately live at
        # a new relative path. Fall back to a unique basename match and say so;
        # only a genuinely absent object is reported as missing.
        if [ ! -f "${path_a}" ]; then
            local candidates
            candidates="$(find "${after}" -name "$(basename "${rel}")" -type f)"
            if [ "$(echo "${candidates}" | grep -c .)" -eq 1 ] && [ -n "${candidates}" ]; then
                path_a="${candidates}"
                moved=$((moved + 1))
                echo "MOVED       ${rel} -> ${path_a#"${after}"/}"
            else
                missing=$((missing + 1))
                echo "MISSING     ${rel} (no counterpart in ${after})"
                rc=1
                continue
            fi
        fi

        if cmp -s "${path_b}" "${path_a}"; then
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
        normalize "${path_a}" > "${tmp}/a.txt"

        local out
        set +e
        out="$(awk -f /dev/stdin "${tmp}/b.txt" "${tmp}/a.txt" <<'AWK'
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
AWK
)"
        local awk_rc=$?
        set -e

        if [ "${awk_rc}" -eq 0 ]; then
            noise=$((noise + 1))
            echo "NOISE-ONLY  ${rel}"
            echo "            ${out##*$'\n'}"
        else
            unclassified=$((unclassified + 1))
            rc=1
            echo "DIFFERS     ${rel}"
            echo "${out}" | sed 's/^/            /'
        fi
    done < "${before}/.psym-manifest"

    echo
    echo "P-SYM: ${total} objects — ${identical} byte-identical, ${noise} differing within the accepted noise class, ${unclassified} with unclassified differences, ${moved} matched by basename after a path move, ${missing} missing"
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
    compare) [ $# -eq 3 ] || usage; do_compare "$2" "$3" ;;
    *) usage ;;
esac
