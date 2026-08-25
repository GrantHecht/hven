#!/bin/bash
# =============================================================================
# The model-surface SCORER-LEG DEMONSTRATION RUN (settler-mandated, W1-W5).
#
# NOT a census of record: this is the Task 5 model-surface scorer demonstration.
# The 57-cell replay's asserted counter columns ride along and are co-asserted
# against the frozen baseline as supporting evidence at zero marginal cost; the
# milestone's close census remains the Task 9 instrument.
#
# What it does, in order:
#   1. runs scripts/run_walk_census.sh (parallel-tiered v1, unmodified) with the
#      scorer shim as --binary, so every SOLVE also writes a per-cell model-
#      surface scorer row;
#   2. concatenates the per-cell scorer rows into ONE wgate_scorer.csv;
#   3. restores the launch provenance record, which the runner's own
#      merge-time provenance_header.txt would otherwise overwrite, as
#      launch record first then runner record;
#   4. writes the runner's exit status to DONE, which is the completion signal.
#
# Detached-launch contract: nohup + setsid, MKL_NUM_THREADS=1, all output to
# census.log, DONE written on completion whatever the outcome.
# =============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO=/home/ghecht/Projects/hven
RUN="$HERE"
BASELINE="$REPO/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv"
CMP="$REPO/.superpowers/sdd/2026-08-14-hven-m3-plan-revB/preserved-census/census_compare.py"

export HVEN_CORPUS_BIN="$REPO/.scratch/task-5/build/bench/hven_sqp_corpus"
export HVEN_SCORER_DIR="$RUN/scorer_rows"
export MKL_NUM_THREADS=1

# The launch record is written before the run; the runner overwrites the same
# filename at merge time, so keep a copy to restore afterwards. The wrapper
# stamps its own pid and start time into both, which is the only way the
# detached process's identity gets into the record without a launch-time race.
{
    printf '# census.launch_pid: %s\n' "$$"
    printf '# census.launch_started: %s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >>"$RUN/provenance_header.txt"
cp -f "$RUN/provenance_header.txt" "$RUN/.launch_provenance.txt"

"$REPO/scripts/run_walk_census.sh" \
    --binary "$HERE/corpus_with_scorer.sh" \
    --out "$RUN" \
    --baseline "$BASELINE" \
    --compare "$CMP"
rc=$?

# One scorer artifact from the per-cell rows: header once, data rows in cell
# order. An absent row means that cell produced no answer (DNF / engine error),
# which the hook skips by design.
if [ -d "$HVEN_SCORER_DIR" ]; then
    {
        printf 'cell_id,kkt_stationarity,kkt_complementarity,kkt_primal,'
        printf 'ms_stationarity,ms_complementarity,ms_primal,verdict_equal\n'
        for f in "$HVEN_SCORER_DIR"/*.wgate.csv; do
            [ -r "$f" ] || continue
            tail -n +2 "$f"
        done
    } >"$RUN/wgate_scorer.csv"
fi

if [ -r "$RUN/.launch_provenance.txt" ]; then
    cat "$RUN/.launch_provenance.txt" "$RUN/provenance_header.txt" \
        >"$RUN/provenance_header.txt.new" 2>/dev/null &&
        mv "$RUN/provenance_header.txt.new" "$RUN/provenance_header.txt"
    rm -f "$RUN/.launch_provenance.txt"
fi

echo "$rc" >"$RUN/DONE"
exit "$rc"
