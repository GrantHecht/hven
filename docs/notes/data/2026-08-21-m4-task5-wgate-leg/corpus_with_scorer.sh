#!/bin/bash
# =============================================================================
# Corpus-binary shim: turns the model-surface scorer ON for SOLVE invocations
# only, leaving every other invocation byte-identical to the bare binary.
#
# WHY A SHIM. scripts/run_walk_census.sh builds its worker command lines itself
# (`$BIN --engine walk --cells <cell> --csv <row>`) and has no pass-through for
# extra flags, and `--score-model-surface-out` is a per-PROCESS output path, so
# a single shared path would be clobbered by six concurrent workers. The shim
# keeps the standing runner unmodified and gives each cell its own scorer row
# file, which the leg wrapper concatenates afterwards into one wgate_scorer.csv.
#
# PASS-THROUGH IS EXACT for the runner's two non-solve invocations:
#   `--list`      (roster cross-check)
#   `--from-csv`  (merge + --score-gates) -- which REFUSES the scorer flags by
#                 design, since a committed row carries no (x, lambda, z).
#
# Env:
#   HVEN_CORPUS_BIN    the real corpus binary (required)
#   HVEN_SCORER_DIR    directory for per-cell scorer rows (required for solves)
# =============================================================================
set -u
BIN=${HVEN_CORPUS_BIN:?HVEN_CORPUS_BIN must name the real corpus binary}

want_scorer=0
csv=""
prev=""
for a in "$@"; do
    case "$a" in
        --engine) want_scorer=1 ;;
        --list | --from-csv) want_scorer=0; break ;;
    esac
    [ "$prev" = "--csv" ] && csv="$a"
    prev="$a"
done

if [ "$want_scorer" -eq 1 ] && [ -n "$csv" ]; then
    dir=${HVEN_SCORER_DIR:?HVEN_SCORER_DIR must be set for solve invocations}
    mkdir -p "$dir"
    cell=$(basename "$csv"); cell=${cell%.part}; cell=${cell%.csv}
    exec "$BIN" "$@" --score-model-surface --score-model-surface-out "$dir/$cell.wgate.csv"
fi
exec "$BIN" "$@"
