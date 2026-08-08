#!/usr/bin/env bash
#
# The consumed-surface audit's STATIC half: a scan over a source tree for every
# place it touches a sparse-backend surface.
#
# Emits one CSV row per touchpoint, so the audit's output is a diffable
# artifact rather than a paragraph. It is deliberately over-inclusive: a
# touchpoint reported that turns out not to matter costs a line in a review,
# while one missed silently is a hole in the option set the migration lands on.
#
# WHAT THIS HALF CANNOT DO, stated here because the audit's method depends on
# knowing it: a static scan reports what the source MENTIONS, not what a run
# EXECUTES. A parameter entry written on a path no fixture reaches looks
# identical here to one written on every call. That is why the audit has a
# runtime half (pardiso_recorder.h) and why the coverage test that guards the
# method is a runtime one.
#
# usage: static_scan.sh <root> [<root> ...]
#        static_scan.sh --self-test
#
# Output columns: kind,symbol,file,line,text

set -u

usage() {
    echo "usage: $(basename "$0") <root> [<root> ...]" >&2
    echo "       $(basename "$0") --self-test" >&2
}

# One scan pattern per touchpoint class. The `kind` is the first field of every
# row it produces; the pattern is an extended regular expression.
#
# The parameter-array patterns match a subscripted access on any array whose
# name ends in the backend's own parameter-array name, which covers every
# spelling the two seams and this library use for it (a bare array, a member
# with a trailing underscore, a std::array through .data()).
scan_one() {
    local kind="$1" pattern="$2" root="$3"
    grep -rInE --include='*.h' --include='*.hpp' --include='*.cpp' --include='*.cc' \
        --include='*.cxx' -- "$pattern" "$root" 2>/dev/null |
        while IFS=: read -r file line text; do
            # Strip leading whitespace and any comma, which would split the row.
            local cleaned
            cleaned=$(printf '%s' "$text" | sed -e 's/^[[:space:]]*//' -e 's/,/;/g')
            printf '%s,%s,%s,%s,%s\n' "$kind" "$(symbol_for "$kind" "$text")" "$file" "$line" \
                "$cleaned"
        done
}

symbol_for() {
    local kind="$1" text="$2"
    case "$kind" in
    parameter_write | parameter_read)
        printf '%s' "$text" | grep -oE 'iparm[A-Za-z_]*\[[0-9]+\]' | head -n 1
        ;;
    phase_call)
        printf '%s' "$text" | grep -oE 'pardiso(init|_64)?[[:space:]]*\(' | head -n 1 |
            tr -d ' ('
        ;;
    accelerate_call)
        printf '%s' "$text" | grep -oE 'Sparse[A-Z][A-Za-z]*' | head -n 1
        ;;
    lapack_call)
        printf '%s' "$text" | grep -oE 'LAPACKE?_[a-z0-9_]+' | head -n 1
        ;;
    thread_control)
        printf '%s' "$text" |
            grep -oE 'mkl_[a-z_]*num_threads[a-z_]*|omp_set_num_threads|BLASSetThreading|VECLIB_MAXIMUM_THREADS' |
            head -n 1
        ;;
    *)
        printf 'unknown'
        ;;
    esac
}

scan_root() {
    local root="$1"
    if [ ! -d "$root" ]; then
        echo "static_scan.sh: not a directory: $root" >&2
        return 1
    fi
    # A write is an assignment INTO a parameter slot; a read is any other
    # mention of one. The write pattern is checked first and the read pattern
    # excludes it, so a line is classified once.
    scan_one parameter_write 'iparm[A-Za-z_]*\[[0-9]+\][[:space:]]*=[^=]' "$root"
    grep -rInE --include='*.h' --include='*.hpp' --include='*.cpp' --include='*.cc' \
        --include='*.cxx' -- 'iparm[A-Za-z_]*\[[0-9]+\]' "$root" 2>/dev/null |
        grep -vE 'iparm[A-Za-z_]*\[[0-9]+\][[:space:]]*=[^=]' |
        while IFS=: read -r file line text; do
            cleaned=$(printf '%s' "$text" | sed -e 's/^[[:space:]]*//' -e 's/,/;/g')
            printf 'parameter_read,%s,%s,%s,%s\n' \
                "$(symbol_for parameter_read "$text")" "$file" "$line" "$cleaned"
        done
    scan_one phase_call 'pardiso(init|_64)?[[:space:]]*\(' "$root"
    scan_one accelerate_call 'Sparse(Factor|Solve|GetInertia|Cleanup|CreateSubfactor|Retain|Refactor)[A-Za-z]*' "$root"
    scan_one lapack_call 'LAPACKE?_(dsytrf|dsytrs|dsytri|dpotrf|dpotrs)[a-z0-9_]*' "$root"
    scan_one thread_control 'mkl_[a-z_]*num_threads[a-z_]*|omp_set_num_threads|BLASSetThreading|VECLIB_MAXIMUM_THREADS' "$root"
}

# The self-test runs the scan over a committed sample carrying one line of each
# touchpoint class plus two negative controls, and checks that every class is
# found and neither control is. It exercises the SCANNER, not the trees it will
# be pointed at, so it stays green on a machine that has neither sibling
# checkout.
self_test() {
    local here
    here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local data="$here/testdata"
    local out
    out=$(scan_root "$data")
    local status=0

    check_kind() {
        local kind="$1" want="$2"
        local got
        got=$(printf '%s\n' "$out" | grep -c "^$kind," || true)
        if [ "$got" != "$want" ]; then
            echo "FAIL: expected $want '$kind' rows, got $got" >&2
            printf '%s\n' "$out" | grep "^$kind," >&2 || true
            status=1
        else
            echo "ok: $kind -- $got row(s)"
        fi
    }

    check_kind parameter_write 3
    check_kind parameter_read 2
    check_kind phase_call 3
    check_kind accelerate_call 3
    check_kind lapack_call 2
    check_kind thread_control 2

    # Negative controls: an identifier that merely CONTAINS the parameter
    # array's name, and a LAPACK-shaped name that is not one of the routines
    # this scan is about, must not be reported.
    if printf '%s\n' "$out" | grep -q 'negative_control_identifier'; then
        echo "FAIL: an identifier that only contains the parameter-array name was reported" >&2
        status=1
    else
        echo "ok: negative control (identifier containing the array name) not reported"
    fi
    if printf '%s\n' "$out" | grep -q 'LAPACKE_dgesv'; then
        echo "FAIL: an unrelated LAPACK routine was reported" >&2
        status=1
    else
        echo "ok: negative control (unrelated LAPACK routine) not reported"
    fi

    # The pre-registered pairing, asserted here in its static half: the
    # refinement-cap rule IS visible to this scan as a raw parameter write --
    # which is strictly more than a scan of the consumed OPTION surface would
    # see, and is why this scan looks at parameter subscripts rather than at
    # option names. Finding it at RUNTIME is a separate, stronger claim and is
    # asserted by test_audit_shim.cpp.
    if printf '%s\n' "$out" | grep -q '^parameter_write,iparm_\[7\]'; then
        echo "ok: the refinement-cap write is visible to the static scan"
    else
        echo "FAIL: the static scan missed the refinement-cap write in the sample" >&2
        status=1
    fi

    if [ "$status" -eq 0 ]; then
        echo "==== PASS: static_scan.sh finds every touchpoint class and no negative control ===="
    fi
    return "$status"
}

if [ "$#" -eq 0 ]; then
    usage
    exit 2
fi

if [ "$1" = "--self-test" ]; then
    self_test
    exit "$?"
fi

echo "kind,symbol,file,line,text"
rc=0
for root in "$@"; do
    scan_root "$root" || rc=1
done
exit "$rc"
