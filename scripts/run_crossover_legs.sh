#!/bin/bash
# M5 W5 crossover-leg sweep — the serial protocol, with its own witness.
#
# Runs bench/hven_sqp_crossover over the dual-bindable replay-corpus cells and
# produces docs/notes/data/2026-08-m5-w5-crossover/. The runner itself is what
# measures; this script exists for the two things a runner cannot do for itself.
#
# ------------------------------------------------- measurement discipline ---
#
# The W5 protocol (docs/notes/2026-08-m5-ledger.md, 2026-08-25) declares NO
# co-run terms: "one solve at a time, MKL_NUM_THREADS=1, serialized". Unlike
# scripts/run_walk_census.sh -- a counter-asserting replay, which §7 permits to
# co-run under declared terms -- this sweep records WALL-DEPENDENT statuses (a
# cell that outlives the runner's deadline is a dnf_budget row), and §7 requires
# those to run solo. So there is no width flag here and there is nothing to
# tier: the whole script is one process, pinned, alone.
#
# THE TWO THINGS THIS ADDS TO A BARE INVOCATION:
#
#   1. A QUIET GATE. "Alone on the machine" is a precondition, and a sweep that
#      starts while another project's build is compiling has already broken it.
#      The gate waits for --quiet-samples consecutive clean samples before the
#      first solve. (A first sweep WAS discarded for exactly this reason; see
#      the artifact README §1.2.)
#   2. A WITNESS. box_witness.log records, once a minute for the whole sweep,
#      how many competing build/compiler processes were running. That turns
#      "alone on the machine" from an assertion into evidence a reader can
#      check.
#
#      A NONZERO SAMPLE IS NOT AUTOMATICALLY A RETRACTION. Counters at
#      MKL_NUM_THREADS=1 are scheduling-invariant -- the property §7 relies on
#      when it lets counter-asserting replays co-run at all -- so a contended
#      sample does not put a counter in doubt. What it does put in doubt is any
#      WALL-DEPENDENT outcome, i.e. a cell killed at the deadline, since
#      contention can push a cell toward its budget and never away from it.
#      Those are the rows that need adjudicating (a solo re-run, or independent
#      corroboration); the artifact's own contention section is where that
#      adjudication belongs, cell by cell, rather than in a blanket rule here.
#
# clangd and other editor indexers are deliberately NOT counted: they are
# nice-scheduled background work on one core, not a build, and treating them as
# contention would mean the gate never opens on a developer's machine.
#
# ---------------------------------------------------------------- usage ---
#
#   scripts/run_crossover_legs.sh [--build DIR] [--out-dir DIR] [--cells SPEC]
#                                 [--deadline-seconds S] [--cpu N]
#                                 [--quiet-samples K] [--sample-seconds S]
#
# Defaults are the ones the committed artifact was produced under.

set -u

BUILD=build-m5-release
OUT_DIR=docs/notes/data/2026-08-m5-w5-crossover
# CHEAPEST FIRST, and why the order is written down rather than left to the
# census's own.
#
# The runner takes an explicit cell list in the order given. `all` would run the
# census order, which puts the N = 10000 and N = 20000 path cells -- the ones
# the frozen walk baseline already shows running for the better part of an hour
# or hitting their budget -- BEFORE the N = 750/800/825 controls, which finish
# in minutes. A sweep that runs out of session that way loses the affordable
# cells and keeps nothing; run cheapest first and an interrupted sweep still
# leaves a usable artifact of everything that fits.
#
# The ordering is taken from bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv's
# wall_s column. USING WALL TO SCHEDULE IS NOT ASSERTING IT (CLAUDE.md §7):
# nothing here quotes those numbers as a measurement, and the order changes
# which rows exist if the sweep is cut short, never what any row says.
# scripts/run_walk_census.sh tiers from the same column for the same reason.
CELLS=\
f7_n1000_bound_neutral,f7_n1000_bound_physics,\
f7_n2000_bound_neutral,f7_n2000_bound_physics,\
f7_n5000_bound_neutral,f7_n5000_bound_physics,\
f7_n10000_bound_neutral,f7_n10000_bound_physics,\
f7_n20000_bound_neutral,f7_n20000_bound_physics,\
f7_n800_path_physics,f7_n1000_path_physics,f7_n2000_path_physics,\
f7_n750_path_neutral_control,f7_n825_path_neutral_control,\
f7_n1000_path_neutral,f7_n5000_path_physics,f7_n800_path_neutral,\
f7_n2000_path_neutral,f7_n5000_path_neutral,\
f7_n10000_path_physics,f7_n10000_path_neutral,\
f7_n20000_path_physics,f7_n20000_path_neutral
DEADLINE=1200
CPU=0
QUIET_SAMPLES=5
SAMPLE_SECONDS=20

while [ $# -gt 0 ]; do
    case "$1" in
        --build) BUILD=$2; shift 2 ;;
        --out-dir) OUT_DIR=$2; shift 2 ;;
        --cells) CELLS=$2; shift 2 ;;
        --deadline-seconds) DEADLINE=$2; shift 2 ;;
        --cpu) CPU=$2; shift 2 ;;
        --quiet-samples) QUIET_SAMPLES=$2; shift 2 ;;
        --sample-seconds) SAMPLE_SECONDS=$2; shift 2 ;;
        -h|--help) sed -n '2,45p' "$0"; exit 0 ;;
        *) echo "$0: unknown argument '$1' (try --help)" >&2; exit 2 ;;
    esac
done

RUNNER="$BUILD/bench/hven_sqp_crossover"
if [ ! -x "$RUNNER" ]; then
    echo "$0: '$RUNNER' is not executable -- build the hven_sqp_crossover target first" >&2
    exit 1
fi
mkdir -p "$OUT_DIR" || exit 1

# Competing compute: another project's compiler or build driver.
#
# COUNTED BY EXECUTABLE, NOT BY COMMAND LINE, and that distinction is load
# bearing. The obvious spelling -- `pgrep -f conda-linux-gnu-clang` -- matches
# any process whose COMMAND LINE contains the pattern, which includes every
# shell that mentions it: a monitoring command, a `grep` for it, this function
# quoted in a log. The first version of this script did exactly that, and the
# effect was a gate that could never open, because the processes watching for
# quiet were themselves counted as noise. Reading /proc/PID/exe instead asks
# what the process IS rather than what its arguments say, and a bash shell is
# never a compiler however it was invoked.
#
# clangd is excluded by name: it contains "clang", but it is a nice-scheduled
# editor indexer on one core, not a build, and counting it would mean the gate
# never opens on a developer's machine.
competing() {
    local total=0 pid exe base
    for pid in /proc/[0-9]*; do
        exe=$(readlink "$pid/exe" 2>/dev/null) || continue
        base=${exe##*/}
        case "$base" in
            clangd|clangd-*) continue ;;
            *clang*|cc1plus|cc1|ninja|ld|ld.lld|lld|ld.gold|make|gmake)
                total=$((total + 1)) ;;
        esac
    done
    echo "$total"
}

echo "$0: waiting for the machine to go quiet ($QUIET_SAMPLES clean samples, ${SAMPLE_SECONDS}s apart)..."
clean=0
while [ "$clean" -lt "$QUIET_SAMPLES" ]; do
    if [ "$(competing)" -eq 0 ]; then
        clean=$((clean + 1))
    else
        clean=0
    fi
    sleep "$SAMPLE_SECONDS"
done
echo "$0: machine is quiet; starting the sweep."

{
    echo "# W5 sweep contention witness -- competing build/compiler process count, once a minute."
    echo "# commit: $(git -C "$(dirname "$0")/.." describe --always --dirty --abbrev=12 2>/dev/null || echo unknown)"
    echo "# host: $(hostname), started: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# A nonzero count means the 'alone on the machine' condition was broken at that moment."
    echo "# Counters at MKL_NUM_THREADS=1 are scheduling-invariant and are NOT in doubt because of"
    echo "# it; WALL-DEPENDENT outcomes (a cell killed at its deadline) are, since contention can"
    echo "# push a cell toward its budget and never away from it. Adjudicate those rows -- by a solo"
    echo "# re-run or by independent corroboration -- in the artifact's own contention section."
    while true; do
        echo "$(date -u +%Y-%m-%dT%H:%M:%SZ) competing=$(competing) load1=$(cut -d' ' -f1 /proc/loadavg)"
        sleep 60
    done
} > "$OUT_DIR/box_witness.log" &
WITNESS=$!
trap 'kill $WITNESS 2>/dev/null' EXIT

# One process, one pinned PHYSICAL core with its SMT sibling left idle,
# MKL_NUM_THREADS=1. That is the whole of the execution claim.
MKL_NUM_THREADS=1 taskset -c "$CPU" "$RUNNER" \
    --cells "$CELLS" --out-dir "$OUT_DIR" --deadline-seconds "$DEADLINE" \
    > "$OUT_DIR/sweep.log" 2>&1
RC=$?

kill $WITNESS 2>/dev/null
trap - EXIT
echo "sweep exited rc=$RC" >> "$OUT_DIR/sweep.log"

# The refused half of the census, listed with its reasons, as the protocol
# requires ("never silently dropped").
{
    echo "# hven_sqp_crossover -- replay-corpus cells NOT measured by the W5 legs, with the"
    echo "# reason each one cannot be reached through the dual-bind path. Generated by"
    echo "# scripts/run_crossover_legs.sh alongside the sweep it describes."
    echo "# commit: $(git -C "$(dirname "$0")/.." describe --always --dirty --abbrev=12 2>/dev/null || echo unknown)"
    echo "# generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    "$RUNNER" --list | awk 'NR == 1 || $3 == "no"'
} > "$OUT_DIR/cells_not_measured.txt"

WORST=$(grep -o 'competing=[0-9]*' "$OUT_DIR/box_witness.log" | cut -d= -f2 | sort -n | tail -1)
echo "$0: done (rc=$RC). Worst contention sample during the sweep: competing=$WORST."
if [ "${WORST:-0}" -ne 0 ]; then
    echo "$0: WARNING -- the machine was NOT alone for at least one sample. Re-run the cells" >&2
    echo "$0:          overlapping those samples solo before trusting their rows." >&2
fi
exit $RC
