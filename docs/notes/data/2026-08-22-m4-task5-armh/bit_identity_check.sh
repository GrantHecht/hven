#!/bin/bash
# =============================================================================
# ARM-H bit-identity falsification harness.
#
# ARM-H's use as a discriminator rests on a semantic-inertness premise: on
# every timed IPM wall-leg cell, the dispatch tail's terminal arm is never
# entered (the standing leg's own finding, docs/notes/data/2026-08-21-m4-
# task5-wall/README.md: "no throw occurred, every flag is 0"), so reverting
# it should change no answer any solve produces -- only, if the mechanism
# hypothesis is right, code layout. This harness FALSIFIES that premise if it
# is wrong: it runs the IPM cells the wall leg times and diffs their answers
# -- not their wall times -- between the ARM-H build and the LANDED build.
#
# "ANSWER ARTIFACT" CONVENTION, AND ITS ABSENCE HERE. The standing bench wall
# leg (docs/notes/data/2026-08-21-m4-task2b-wall/wall_leg.sh) has a real
# per-cell answer artifact: the bench CSV's non-wall columns, diffable
# byte-for-byte. The IPM wall leg's probe
# (docs/notes/data/m4-ipm-wall-leg/ipm_time.cpp) has NO separate captured-
# answer file -- its only per-cell output is one stdout line per invocation,
# already printed by the standing instrument:
#   wide n=<N> reps=<R> median_s=<...> min_s=<...> flag=<exit code> xnorm2=<||x||^2>
# `flag` and `xnorm2` are the fields that determine the answer; `median_s`/
# `min_s` are wall time and are EXPECTED to differ (that is the whole
# question this discriminator asks). Because the standing protocol has no
# per-cell answer-artifact file for this probe to point at, this harness
# diffs those two fields extracted from that line, and says so here rather
# than inventing a file convention the standing legs never used.
#
# This is the same probe binaries run_arm_h.sh builds (ipm_armh, ipm_landed,
# ipm_fixed under .scratch/task5-armh/ab/); run run_arm_h.sh first. FIXED is
# checked against LANDED on the same terms: the fix batch's two changes on
# these paths are a refusal message and a refusal check, so a moved answer
# there would be a finding in its own right.
#
# USAGE: ./bit_identity_check.sh [INNER]   (default 3 -- answer identity does
#   not depend on repetition count, since the probe's flag/xnorm2 come from
#   the last of INNER identical, deterministic solves; a small INNER is
#   enough to also confirm no rep-to-rep answer drift within one binary.)
# =============================================================================
set -u
set -o pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

if [ -e "$HERE/HOLD" ]; then
    echo "REFUSING TO START: $HERE/HOLD is present." >&2
    echo "The controller removes HOLD at the measurement window's end mark; this" >&2
    echo "script does not run while it is there." >&2
    exit 1
fi

ROOT=/home/ghecht/Projects/hven/.scratch/task5-armh
ARMH_BIN=$ROOT/ab/ipm_armh
LANDED_BIN=$ROOT/ab/ipm_landed
FIXED_BIN=$ROOT/ab/ipm_fixed
INNER=${1:-3}

for bin in "$ARMH_BIN" "$LANDED_BIN" "$FIXED_BIN"; do
    if [ ! -x "$bin" ]; then
        echo "FATAL: $bin not found. Run run_arm_h.sh first -- it builds all four" >&2
        echo "arms before this harness has anything to compare." >&2
        exit 1
    fi
done

# Pull just the n=, flag=, xnorm2= tokens off a probe's stdout, one line per
# size (n=60, n=120, n=240), in field order -- so median_s/min_s (expected
# to differ; that is not this harness's concern) never enter the diff.
extract() {
    awk '{
        line="";
        for (i = 1; i <= NF; i++) {
            if ($i ~ /^n=/ || $i ~ /^flag=/ || $i ~ /^xnorm2=/) line = line $i " ";
        }
        print line;
    }' <<<"$1"
}

overall=0
for mode in serial threaded; do
    echo "=== mode: $mode"
    if [ "$mode" = serial ]; then
        a_out=$(MKL_NUM_THREADS=1 taskset -c 2 "$ARMH_BIN" "$INNER")
        l_out=$(MKL_NUM_THREADS=1 taskset -c 2 "$LANDED_BIN" "$INNER")
        f_out=$(MKL_NUM_THREADS=1 taskset -c 2 "$FIXED_BIN" "$INNER")
    else
        a_out=$("$ARMH_BIN" "$INNER")
        l_out=$("$LANDED_BIN" "$INNER")
        f_out=$("$FIXED_BIN" "$INNER")
    fi
    a_ans=$(extract "$a_out")
    l_ans=$(extract "$l_out")
    f_ans=$(extract "$f_out")
    echo "ARM-H   : $a_ans"
    echo "LANDED  : $l_ans"
    echo "FIXED   : $f_ans"
    if [ "$a_ans" = "$l_ans" ]; then
        echo "RESULT ($mode, ARM-H): byte-identical across all probed sizes (n=60,120,240)."
    else
        echo "RESULT ($mode, ARM-H): MISMATCH -- the semantic-inertness premise is FALSIFIED." >&2
        overall=1
    fi
    # The fix batch's own inertness claim, asked the same way: its dispatch
    # message and its new scatter check are both refusal paths, so no answer on
    # any timed cell may move.
    if [ "$f_ans" = "$l_ans" ]; then
        echo "RESULT ($mode, FIXED): byte-identical to LANDED across all probed sizes."
    else
        echo "RESULT ($mode, FIXED): MISMATCH -- the fix batch moved an answer." >&2
        overall=1
    fi
done

if [ "$overall" -eq 0 ]; then
    echo "=== bit_identity_check: PASS (ARM-H == LANDED and FIXED == LANDED, answers only,"
    echo "=== both modes)"
else
    echo "=== bit_identity_check: FAIL -- see mismatches above; ARM-H's wall delta cannot" >&2
    echo "=== be read as inert-in-answer until this is explained." >&2
fi
exit "$overall"
