# M7 W2 M2b FIX ROUND 1 -- the gate runner's refusals, in one sourced file so
# that the six lock segments (each a separate bash process) and the fold proofs
# (`fold_proofs.sh`) run the SAME code.
#
# WHY THIS FILE EXISTS. The first round's runner printed return codes and
# aggregated some of them: `ctest` could pass over zero tests, the P-SYM shape
# was echoed rather than checked, `rc5` never entered the aggregate, and G6's
# sub-gates logged their failures and exited 0. The review's verdict was that
# the transcripts were valid but the runner could not be relied on to reproduce
# the claimed gate. Every refusal below is FOLD-PROVEN once -- a deliberately
# broken input makes it exit non-zero -- in `gates-fix1/fold-proofs.txt`.
#
# CONVENTION. Every helper PRINTS one line and returns 0 or 1; nothing here
# exits, so a caller accumulates with `rc=$(( rc | $? ))` and a segment's
# failure can never be lost to a later success.

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
# procedure: this task's own report section 8 declares a gtest filter that
# matched 0 tests and still exited 0. `--no-tests=error` catches the ctest form
# of that; the EXPECTED TOTAL catches the form `--no-tests=error` cannot -- a
# suite that ran SOME tests but not the ones this round added.
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
# present and every one of them carries the pinned shape. The first round read
# only the return codes.
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

# assert_psym CFG CLASSIFICATION COMPARE_RC CLASSIFY_RC MIN_CONTROL
#
# THE SHAPE, CHECKED. `compare` must exit 1 (the options struct grew, so
# byte-identity is not on offer and a 0 would mean the capture compared nothing);
# the classifier must exit 0; the movers must be fully accounted for by class A
# plus class B; UNEXPLAINED must be 0; and the negative control must be nonzero,
# or "explained by a changed header" explains nothing.
assert_psym() {
    local cfg="$1" cls="$2" cmp_rc="$3" cls_rc="$4" min_control="$5" rc=0
    local differing a b unexpl control
    if [ ! -s "$cls" ]; then
        echo "ASSERT FAIL P-SYM $cfg: $cls is missing or empty"
        return 1
    fi
    assert_eq "P-SYM $cfg compare rc" "$cmp_rc" "1" || rc=1
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
    assert_ge "P-SYM $cfg differing" "$differing" "1" || rc=1
    assert_eq "P-SYM $cfg class A + class B" "$((a + b))" "$differing" || rc=1
    assert_eq "P-SYM $cfg unexplained" "$unexpl" "0" || rc=1
    assert_ge "P-SYM $cfg negative control" "$control" "$min_control" || rc=1
    return $rc
}

# assert_same_classification RELEASE_FILE DEBUG_FILE
#
# THE FIRST ROUND ACCEPTED A DIFFERENCE HERE WITH AN `echo`. The two configs
# classify the SAME object set by the SAME rule -- which TU changed and which
# TU includes a changed header are properties of the dependency graph, not of
# the optimiser -- so a difference is a defect in the classification and not
# "Debug carries DWARF-only movers". It FAILS.
assert_same_classification() {
    if diff -q "$1" "$2" > /dev/null 2>&1; then
        echo "ASSERT OK   P-SYM classification byte-identical between configs"
        return 0
    fi
    echo "ASSERT FAIL P-SYM classification differs between configs:"
    diff "$1" "$2" | head -40
    return 1
}
