# THE GATE RUNNER'S REFUSALS, VERSION 3 -- M7 W2 M3b's copy of version 2, which
# M7 W2 M3a FIX ROUND 1's settlement round wrote after that round's own G5
# FAILED. Versions 1 and 2 live in M3a's `wrappers/` and are NOT edited: they
# produced committed transcripts, and a gate's evidence must stay reproducible
# from the code that produced it.
#
# VERSION 3 CHANGES EXACTLY ONE THING, in `assert_psym`: the expected exit code
# of `psym_compare.sh compare` is DERIVED FROM THE DECLARED MOVER COUNT rather
# than fixed at 1. Version 2 hard-coded 1 because every round written until now
# moved something; this round's whole claim is that the production library
# moved NOTHING, and under that claim a `compare` that exits 1 is the failure.
# Everything else -- `assert_eq`, `assert_ge`, `assert_ctest`, `assert_replay`,
# `psym_set`, `source_changed`, `assert_capture`, `assert_capture_pair`,
# `accumulate_rc`, `gates_aggregate`, `assert_psym_cross_config` -- is carried
# over BYTE FOR BYTE and is unchanged in behaviour. The version-2 history below
# is kept because it says WHY each refusal exists.
#
# THE ZERO-MOVER CLAIM IS NOT WEAKER FOR BEING ZERO, and this is the reason the
# change is safe: what would make a false zero pass is a capture that compared
# nothing, and three independent things stand against that -- `assert_capture`
# (a non-empty manifest on each arm), `assert_capture_pair` (the two arms
# enumerate the same number of objects) and the classifier's own NEGATIVE
# CONTROL, which counts the objects that DO include a changed header and are
# identical anyway. A declaration of 0 with a negative control of 0 fails.
#
# ===========================================================================
# WHAT WENT WRONG IN M7 W2 M3a FIX ROUND 1, AND WHAT CHANGES HERE
# ---------------------------------------------------------------------------
# The round declared ONE mover shape -- `DIFFERING 12 = class A 9 + class B 3`
# -- and required it of BOTH configurations, and required the two
# classifications to be BYTE-IDENTICAL. The run produced
#
#     Release  DIFFERING 10 = class A 7 + class B 3, UNEXPLAINED 0, control 107
#     Debug    DIFFERING 11 = class A 8 + class B 3, UNEXPLAINED 0, control 106
#
# and the gate FAILED, correctly, because those are not the declared numbers.
# Two separate defects produced that, and only ONE of them was a bad
# declaration:
#
#   (1) THE DECLARATION'S UNIVERSE WAS WRONG. The derivation hashed every
#       object in a dev build tree -- 181 of them -- and predicted
#       `tests/CMakeFiles/hven_ipqp_seam_tests.dir/__/src/core/enum_names.cpp.o`
#       as a mover. `scripts/psym_compare.sh` DELIBERATELY excludes
#       `hven_ipqp_seam_tests` from its capture (it recompiles library TUs a
#       second time under `HVEN_TESTING`, so its objects are not the production
#       library's code); the gate's capture holds 146 objects, not 181. That
#       object could never appear in the differing set. Both configurations
#       lose it: 12 -> 11, class A 9 -> 8.
#
#   (2) THE CROSS-CONFIGURATION RULE WAS WRONG, not merely the number. Version
#       1's `assert_same_classification` argued that "which TU changed and
#       which TU includes a changed header are properties of the dependency
#       graph, not of the optimiser". That is true of the class A / class B
#       PREDICATES and false of the DIFFERING SET they are applied to, which is
#       whatever the compiler actually emitted. `src/drivers/trace_writer.cpp`
#       is the worked instance: its source changed, and in Debug the helpers it
#       edits (`key_value(..., SolveStatus, bool)`, `counter_never_absent`,
#       `to_json`) are out-of-line symbols whose mangled names carry the
#       renamed type, so P-SYM's object verdict is `DIFFERS` and it is class A.
#       In Release those helpers are INLINED into `counters_object`, the
#       instruction stream is unchanged (CHANGED 0 over 27870 insns) and the
#       only residue is one relocation target renamed
#       `to_string(SolveStatus)` -> `to_string(RestorationSubStatus)`, so
#       P-SYM's object verdict is `NOISE-ONLY` + `RELOCATIONS` and the object
#       is not in the differing set at all. A changed source MAY compile
#       identically in one configuration and not in the other. Requiring the
#       two classifications to be byte-identical forbids a legitimate fact.
#
# SO THE RULE BECOMES, and `assert_psym_cross_config` below is exactly this:
#
#   * the expected mover shape is PER CONFIGURATION -- a runner declares
#     (differing, class A, class B) for Release and for Debug SEPARATELY,
#     before the run, and `assert_psym` asserts each against its own config;
#   * the two CLASS B SETS must be EQUAL. Class B is "source untouched, but the
#     TU includes a changed header", and a header-driven mover that appears in
#     one configuration and not the other is unexplained by anything this rule
#     admits;
#   * UNEXPLAINED must be 0 in BOTH, as a count and as a list;
#   * the SYMMETRIC DIFFERENCE of the two CLASS A sets must be a subset of the
#     objects whose SOURCE this task changed. Such an object is allowed to
#     compile identically in one configuration.
#
#   ANYTHING ELSE FAILS. And note what makes the symmetric-difference clause
#   bite rather than hold vacuously: it is paired below with an explicit
#   INTERNAL-CONSISTENCY check that EVERY class A member in EITHER config is a
#   changed-source object. The classifier applies that predicate itself, so on
#   a classification this runner's own classifier wrote the two checks agree;
#   on a classification that was hand-edited, truncated, carried over from
#   another round, or produced with a different `--changed-sources` list, they
#   do not, and the pair is refused. Both are fold-proven in
#   `gates-fix1/fold-proofs-psym-v2.txt`.
#
# CONVENTION, unchanged from version 1. Every helper PRINTS one line and
# returns 0 or 1; nothing here exits, so a caller accumulates with
# `rc=$(( rc | $? ))` and a segment's failure can never be lost to a later
# success.

# assert_eq LABEL ACTUAL EXPECTED
assert_eq() {
    if [ "$2" = "$3" ]; then
        echo "ASSERT OK   $1 = $2"
        return 0
    fi
    echo "ASSERT FAIL $1: got '$2', expected '$3'"
    return 1
}

# assert_ge LABEL ACTUAL MINIMUM -- both must be integers.
assert_ge() {
    case "$2" in
    '' | *[!0-9-]*)
        echo "ASSERT FAIL $1: '$2' is not an integer"
        return 1
        ;;
    esac
    if [ "$2" -ge "$3" ]; then
        echo "ASSERT OK   $1 = $2 (>= $3)"
        return 0
    fi
    echo "ASSERT FAIL $1: got '$2', expected >= $3"
    return 1
}

# ctest_total LOG -- the total from CTest's own summary line, on stdout.
# Returns 1 (and prints nothing on stdout) when the line is absent, which is
# what a crashed or never-started ctest leaves behind.
ctest_total() {
    local line
    line=$(grep -E '^[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+' "$1" 2>/dev/null | tail -1)
    [ -n "$line" ] || return 1
    echo "$line" | sed -E 's/.* out of ([0-9]+).*/\1/'
}

# ctest_failed LOG -- the failed count from the same line.
ctest_failed() {
    local line
    line=$(grep -E '^[0-9]+% tests passed, [0-9]+ tests failed out of [0-9]+' "$1" 2>/dev/null | tail -1)
    [ -n "$line" ] || return 1
    echo "$line" | sed -E 's/.*, ([0-9]+) tests failed out of.*/\1/'
}

# assert_ctest LABEL LOG EXPECTED_TOTAL
#
# THE WRONG-BINARY INCIDENT'S REFUSAL, as an assertion rather than as a
# procedure: a gtest filter that matched 0 tests once exited 0.
# `--no-tests=error` catches the ctest form of that; the EXPECTED TOTAL catches
# the form `--no-tests=error` cannot -- a suite that ran SOME tests but not the
# ones this round added.
assert_ctest() {
    local label="$1" log="$2" expect="$3" total failed rc=0
    if [ ! -s "$log" ]; then
        echo "ASSERT FAIL $label: $log is missing or empty -- no ctest summary to check"
        return 1
    fi
    total=$(ctest_total "$log") || {
        echo "ASSERT FAIL $label: no CTest summary line in $log"
        return 1
    }
    failed=$(ctest_failed "$log")
    assert_eq "$label failed" "$failed" "0" || rc=1
    assert_eq "$label total" "$total" "$expect" || rc=1
    return $rc
}

# assert_replay COMPARISON CELLS COLUMNS DIFFS ENGINES
#
# compare_replay.py prints one line per engine; the gate is that ALL THREE are
# present and every one of them carries the pinned shape.
assert_replay() {
    local cmp="$1" cells="$2" cols="$3" diffs="$4" engines="$5" rc=0 n
    if [ ! -s "$cmp" ]; then
        echo "ASSERT FAIL replay: $cmp is missing or empty"
        return 1
    fi
    n=$(grep -c "cells compared: ${cells}  columns compared: ${cols}  differences: ${diffs}" "$cmp")
    assert_eq "replay lines at ${cells}/${cols}/${diffs}" "$n" "$engines" || rc=1
    n=$(grep -c "cells compared:" "$cmp")
    assert_eq "replay lines total" "$n" "$engines" || rc=1
    return $rc
}

# psym_field FILE REGEX -- the integer after the last colon on the matching line.
psym_field() {
    local line
    line=$(grep -E "$2" "$1" 2>/dev/null | tail -1)
    [ -n "$line" ] || return 1
    echo "$line" | sed -E 's/.*: *([0-9]+) *$/\1/'
}

# psym_set FILE SECTION -- the OBJECT LIST of one classification section, one
# per line, sorted. SECTION is A, B or U (unexplained).
#
# It exists because version 1 read only the COUNTS. A pair of classifications
# can agree on `class B: 3` and name three different objects, and the counts
# alone cannot see it. The classifier indents every object by six spaces under
# its section header, and the NEGATIVE CONTROL list is indented the same way --
# so the section state is CLEARED on the control header and on any line that is
# not a section header or an indented object, and a stray indented line outside
# a section is therefore dropped rather than credited to the last section seen.
psym_set() {
    awk -v want="$2" '
        /^  class A -- /       { sec = "A"; next }
        /^  class B -- /       { sec = "B"; next }
        /^  UNEXPLAINED:/      { sec = "U"; next }
        /^  NEGATIVE CONTROL/  { sec = "";  next }
        /^$/                   { next }
        /^      [^ ]/          { if (sec == want) { s = $0; sub(/^ +/, "", s); print s } next }
        { sec = "" }
    ' "$1" 2>/dev/null | LC_ALL=C sort
}

# source_changed OBJ CHANGED_SOURCE... -- 0 when OBJ's basename minus the
# trailing `.o` equals the basename of one of the changed sources. This is the
# classifier's OWN class A predicate (`classify_psym*.py`, `main`), restated
# here so the cross-config rule is checked against the round's git-derived
# source list and not against the classifier's say-so.
source_changed() {
    local obj="$1"; shift
    local base src
    base=${obj##*/}
    base=${base%.o}
    for src in "$@"; do
        [ "${src##*/}" = "$base" ] && return 0
    done
    return 1
}

# assert_psym CFG CLASSIFICATION COMPARE_RC CLASSIFY_RC MIN_CONTROL \
#             EXPECT_DIFFERING EXPECT_CLASS_A EXPECT_CLASS_B
#
# UNCHANGED IN BEHAVIOUR from version 1. What changes is the CALLER: the three
# expected counts are now declared PER CONFIGURATION, so a runner calls this
# with Release's shape for Release and Debug's shape for Debug. Requiring one
# shape of both configurations is what version 1's runner did, and it is wrong
# for the reason set out at the top of this file.
#
# `compare`'s expected exit code is DERIVED FROM THE DECLARATION rather than
# fixed at 1 (VERSION 3, M7 W2 M3b): 1 when the round declares movers, and 0
# when it declares none. Version 2 hard-coded 1 with the reason "byte-identity
# is not on offer for a round with movers, and a 0 would mean the capture
# compared nothing" -- true of every round written so far and false of a round
# whose whole claim is that the production library did not move. The
# capture-compared-nothing worry is answered where it belongs and always was:
# `assert_capture` (a non-empty manifest), `assert_capture_pair` (equal object
# counts on the two arms) and the classifier's own NEGATIVE CONTROL, which is
# what makes "0 differing" mean "the header was seen and changed nothing".
#
# The classifier must exit 0; the movers must be fully accounted for by class A
# plus class B; UNEXPLAINED must be 0; the negative control must be nonzero, or
# "explained by a changed header" explains nothing; and the shape must be the
# declared one.
assert_psym() {
    local cfg="$1" cls="$2" cmp_rc="$3" cls_rc="$4" min_control="$5" rc=0
    local want_differing="$6" want_a="$7" want_b="$8"
    local differing a b unexpl control
    if [ ! -s "$cls" ]; then
        echo "ASSERT FAIL P-SYM $cfg: $cls is missing or empty"
        return 1
    fi
    local want_cmp_rc=1
    [ "$want_differing" = "0" ] && want_cmp_rc=0
    assert_eq "P-SYM $cfg compare rc (declared ${want_differing} mover(s))" "$cmp_rc" "$want_cmp_rc" || rc=1
    assert_eq "P-SYM $cfg classify rc" "$cls_rc" "0" || rc=1
    differing=$(psym_field "$cls" '^DIFFERING OBJECTS:') || differing=""
    a=$(psym_field "$cls" '^  class A') || a=""
    b=$(psym_field "$cls" '^  class B') || b=""
    unexpl=$(psym_field "$cls" '^  UNEXPLAINED:') || unexpl=""
    control=$(psym_field "$cls" '^  NEGATIVE CONTROL') || control=""
    if [ -z "$differing" ] || [ -z "$a" ] || [ -z "$b" ] || [ -z "$unexpl" ] || [ -z "$control" ]; then
        echo "ASSERT FAIL P-SYM $cfg: the classification is missing one of its five counts"
        return 1
    fi
    if [ "$want_differing" != "0" ]; then
        assert_ge "P-SYM $cfg differing" "$differing" "1" || rc=1
    fi
    assert_eq "P-SYM $cfg class A + class B" "$((a + b))" "$differing" || rc=1
    assert_eq "P-SYM $cfg unexplained" "$unexpl" "0" || rc=1
    assert_ge "P-SYM $cfg negative control" "$control" "$min_control" || rc=1
    assert_eq "P-SYM $cfg differing (declared)" "$differing" "$want_differing" || rc=1
    assert_eq "P-SYM $cfg class A (declared)" "$a" "$want_a" || rc=1
    assert_eq "P-SYM $cfg class B (declared)" "$b" "$want_b" || rc=1
    return $rc
}

# assert_capture LABEL RC SNAPSHOT_DIR
#
# THE STALE-OR-PARTIAL CAPTURE'S REFUSAL. `psym_compare.sh capture` TRUNCATES
# its `.psym-manifest` before it may refuse an incomplete object universe, so a
# failed capture could leave a short manifest, or a previous run's manifest, for
# `compare` to read. A nonzero rc, a missing snapshot and an empty manifest
# each fail the gate.
assert_capture() {
    local label="$1" rc="$2" dir="$3" n
    if [ "$rc" != "0" ]; then
        echo "ASSERT FAIL capture $label: psym_compare.sh capture exited $rc"
        return 1
    fi
    if [ ! -s "$dir/.psym-manifest" ]; then
        echo "ASSERT FAIL capture $label: no non-empty .psym-manifest under $dir"
        return 1
    fi
    n=$(wc -l < "$dir/.psym-manifest")
    echo "ASSERT OK   capture $label: rc=0, $n object(s) in $dir/.psym-manifest"
    return 0
}

# assert_capture_pair BEFORE_DIR AFTER_DIR
#
# The other half of the same refusal: the two arms must enumerate the SAME
# NUMBER of objects. A pair narrowed on BOTH sides by the same amount is
# invisible to `compare` and is exactly what a shared stale snapshot root
# produces.
assert_capture_pair() {
    local before="$1" after="$2" nb na
    if [ ! -s "$before/.psym-manifest" ] || [ ! -s "$after/.psym-manifest" ]; then
        echo "ASSERT FAIL capture pair: one of the two manifests is missing or empty"
        return 1
    fi
    nb=$(wc -l < "$before/.psym-manifest")
    na=$(wc -l < "$after/.psym-manifest")
    assert_eq "capture pair object count" "$na" "$nb"
}

# accumulate_rc CURRENT SUB -- G6's own sub-gate accumulation, PRINTED (so the
# caller writes `rc=$(accumulate_rc "$rc" "$sub")`) and returned.
accumulate_rc() {
    if [ "$1" -ne 0 ] || [ "$2" -ne 0 ]; then
        echo 1
        return 1
    fi
    echo 0
    return 0
}

# gates_aggregate RC1 .. RCN -- the chain's FINAL exit code. A return code that
# is not a non-negative integer is itself a refusal -- an unset variable
# reaching here used to read as 0.
gates_aggregate() {
    local agg=0 rc
    for rc in "$@"; do
        case "$rc" in
        '' | *[!0-9]*)
            echo "ASSERT FAIL gates aggregate: '$rc' is not a return code"
            return 1
            ;;
        esac
        [ "$rc" -eq 0 ] || agg=1
    done
    echo "GATES_AGGREGATE rc=$agg over [$*]"
    return $agg
}

# assert_psym_cross_config RELEASE_CLS DEBUG_CLS -- CHANGED_SOURCE...
#
# THE REPLACEMENT FOR `assert_same_classification`, which required the two
# configurations' classifications to be BYTE-IDENTICAL and therefore refused a
# fact that is allowed to be true: a source this task changed may compile to an
# identical object in one configuration and not in the other.
#
# The four clauses, in order, are the ones set out at the top of this file. The
# `--` separates the two file arguments from the round's changed-source list so
# that an empty list is a visible mistake rather than a silently permissive one.
assert_psym_cross_config() {
    local rel="$1" dbg="$2" sep="$3"; shift 3
    local rc=0 obj n
    if [ "$sep" != "--" ]; then
        echo "ASSERT FAIL P-SYM cross-config: expected '--' before the changed-source list, got '$sep'"
        return 1
    fi
    if [ "$#" -eq 0 ]; then
        echo "ASSERT FAIL P-SYM cross-config: the changed-source list is EMPTY -- the "
        echo "            symmetric-difference clause would admit anything"
        return 1
    fi
    if [ ! -s "$rel" ] || [ ! -s "$dbg" ]; then
        echo "ASSERT FAIL P-SYM cross-config: one of the two classifications is missing or empty"
        return 1
    fi

    local ta tb tu
    ta=$(mktemp) || return 1
    tb=$(mktemp) || { rm -f "$ta"; return 1; }
    tu=$(mktemp) || { rm -f "$ta" "$tb"; return 1; }

    # (1) the class B sets must be EQUAL.
    psym_set "$rel" B > "$ta"
    psym_set "$dbg" B > "$tb"
    if cmp -s "$ta" "$tb"; then
        echo "ASSERT OK   P-SYM cross-config: class B sets equal ($(wc -l < "$ta") object(s))"
    else
        echo "ASSERT FAIL P-SYM cross-config: the class B sets differ (Release '<' vs Debug '>'):"
        diff "$ta" "$tb" | sed 's/^/            /' | head -20
        rc=1
    fi

    # (2) UNEXPLAINED must be 0 in BOTH, as a count and as a list.
    local cfg f
    for cfg in Release:"$rel" Debug:"$dbg"; do
        f=${cfg#*:}
        n=$(psym_field "$f" '^  UNEXPLAINED:') || n=""
        if [ -z "$n" ]; then
            echo "ASSERT FAIL P-SYM cross-config: ${cfg%%:*} carries no UNEXPLAINED count"
            rc=1
            continue
        fi
        psym_set "$f" U > "$tu"
        if [ "$n" = "0" ] && [ ! -s "$tu" ]; then
            echo "ASSERT OK   P-SYM cross-config: ${cfg%%:*} unexplained = 0 (count and list)"
        else
            echo "ASSERT FAIL P-SYM cross-config: ${cfg%%:*} unexplained count '$n', list:"
            sed 's/^/            /' "$tu" | head -10
            rc=1
        fi
    done

    # (3) INTERNAL CONSISTENCY, and what makes (4) bite: every class A member of
    #     EITHER config must be an object whose source this task changed.
    local stray
    for cfg in Release:"$rel" Debug:"$dbg"; do
        f=${cfg#*:}
        n=0
        stray=0
        while IFS= read -r obj; do
            [ -n "$obj" ] || continue
            if ! source_changed "$obj" "$@"; then
                echo "ASSERT FAIL P-SYM cross-config: ${cfg%%:*} class A names '$obj', whose source"
                echo "            is NOT in this round's changed-source list"
                stray=1
            fi
            n=$((n + 1))
        done < <(psym_set "$f" A)
        if [ "$stray" -eq 0 ]; then
            echo "ASSERT OK   P-SYM cross-config: ${cfg%%:*} class A ($n) all changed-source"
        else
            rc=1
        fi
    done

    # (4) the SYMMETRIC DIFFERENCE of the two class A sets must be a subset of
    #     the changed-source objects.
    psym_set "$rel" A > "$ta"
    psym_set "$dbg" A > "$tb"
    local bad=0 sym=0
    while IFS= read -r obj; do
        [ -n "$obj" ] || continue
        sym=$((sym + 1))
        if source_changed "$obj" "$@"; then
            echo "ASSERT OK   P-SYM cross-config: class A differs on '$obj' -- source changed by this"
            echo "            round, so it is allowed to compile identically in one configuration"
        else
            echo "ASSERT FAIL P-SYM cross-config: class A differs on '$obj', whose source this round"
            echo "            did NOT change -- a configuration-only mover is not admitted"
            bad=1
        fi
    done < <(LC_ALL=C comm -3 "$ta" "$tb" | tr -d '\t')
    [ "$bad" -eq 0 ] || rc=1
    [ "$sym" -ne 0 ] || echo "ASSERT OK   P-SYM cross-config: the two class A sets are identical"

    rm -f "$ta" "$tb" "$tu"
    return $rc
}
