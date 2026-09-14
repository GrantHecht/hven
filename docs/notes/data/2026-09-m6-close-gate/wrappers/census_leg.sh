#!/usr/bin/env bash
# THE 57-CELL WALK CENSUS, all three tiers, in the runner's own order.
# scripts/run_walk_census.sh IS the protocol (T1 6-wide pinned / T2 solo /
# T3 5-wide full-budget, MKL_NUM_THREADS=1 per pinned PHYSICAL core, SMT
# siblings idle); this wrapper adds nothing to it but the stamp preflight,
# a captured status and a fold-safe tail.
#
# THE RUNNER'S EXIT DISCIPLINE, which this wrapper PRESERVES rather than
# reinterprets (run_walk_census.sh:87-106): 0 = the sweep is whole (a MISMATCH
# is a REPORT and leaves the status 0); 1 = INFRASTRUCTURE failure; 2 = usage /
# preflight; 3 = fatal mid-sweep.  So a non-zero status here is never a counter
# finding -- it means the evidence is not whole.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
BIN=${HVEN_CLOSE_CENSUS_BIN:-$F/build-release/bench/hven_sqp_corpus}
OUT=${HVEN_CLOSE_CENSUS_OUT:-$F/census/run}
worst=0
echo "=== stamp preflight (the runner's header, :49-57) ==="
# ERRATUM AT SOURCE, run_walk_census.sh:56.  Its preflight is spelled
#   "$BIN" --from-csv <any committed baseline> --csv /dev/null   # header only
# but the stamp is written into the CSV, NOT to stdout, so `--csv /dev/null`
# DISCARDS the very lines the preflight exists to read (observed here: that
# invocation prints only "merged 57 row(s) into /dev/null").  The check is the
# same check; it just has to be pointed at a real file.
MKL_NUM_THREADS=1 "$BIN" --from-csv "$R/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv" \
    --csv "$F/logs/census-stamp-preflight.csv" > "$F/logs/census-stamp-preflight.out" 2>&1
rc=$?; worst=$((worst | rc))
echo "PREFLIGHT_RC=$rc"
grep -E '^# (binary|schema|budget_table_hash):' "$F/logs/census-stamp-preflight.csv" || true
if ! grep -qE '^# binary: [0-9a-f]' "$F/logs/census-stamp-preflight.csv"; then
    echo "FATAL: no binary stamp line in the preflight CSV -- the preflight did not observe a stamp."
    echo "CENSUS_WORST_RC=2"; exit 2
fi
if grep -qE '^# binary:.*-dirty' "$F/logs/census-stamp-preflight.csv"; then
    echo "FATAL: the corpus binary's stamp carries -dirty; the gate's provenance requirement fails."
    echo "CENSUS_WORST_RC=2"; exit 2
fi
[ $rc -eq 0 ] || { echo "FATAL: stamp preflight failed"; echo "CENSUS_WORST_RC=$worst"; exit $worst; }
echo "=== the sweep ==="
bash "$R/scripts/run_walk_census.sh" \
    --binary "$BIN" \
    --out "$OUT" \
    --compare "$R/scripts/census_compare.py" \
    > "$F/logs/census-run.out" 2>&1
cen_rc=$?; worst=$((worst | cen_rc))
echo "CENSUS_RUN_RC=$cen_rc  (worst $worst)"
tail -20 "$F/logs/census-run.out"
echo "CENSUS_WORST_RC=$worst"
exit $worst
