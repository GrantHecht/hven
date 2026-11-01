#!/bin/bash
# A3 TRAIN chain (design page s5.3). Two scratch clones of hven-bench at 40bb726 (local commits,
# never pushed): `bench` pinned at this branch's measured head with the four A3 labels in its lane;
# `bench-base` pinned at the base 534bc87f, nothing else changed (control C-1). ONE CELL PER LEG:
# `pgrep -a hven_bench_cell` in its own command first, then the clone's scripts/leg.sh (its own
# pre-lock and in-lock gates, flock /tmp/box-build.lock, no timeout, FOREGROUND_START/END) around
# the clone's scripts/w2_a4_sweep.sh (the set's pinned ceiling, TRAIN split, runners/run_serial.py).
# HOLDOUT is never asked for.
#
# Usage: train_chain.sh PHASE   (PHASE = build | cells | all)
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff/.scratch/a3
BENCH=$S/bench
BBASE=$S/bench-base
T=$S/train
LOGS=$T/logs
CELLS=$T/cells
CELLS_BASE=$T/cells-base
SUM=$LOGS/chain-summary.txt
mkdir -p "$LOGS" "$CELLS" "$CELLS_BASE"
PHASE=${1:-all}
A3_LABELS="sqp-ipm-slack1 sqp-ipm-slack1e3 sqp-ipm-skip1 sqp-ipm-skipnever"

roster() { # SET -> sorted TRAIN problems
    python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(sorted(p["problem"] for p in d["problems"] if p["split"]=="TRAIN")))' "$BENCH/sets/$1"
}
HS="$(roster hs-27.json)"
CU="$(roster cutest-nlp.json)"

leg() { # CLONE NAME COMMAND...
    local clone=$1 name=$2 log rc start
    shift 2
    log="$LOGS/leg-$name.log"
    start="$(date -Is)"
    if pgrep -a hven_bench_cell > "$LOGS/pgrep-$name.txt"; then
        echo "PGREP_NONEMPTY $name" >> "$log"
    else
        echo "PGREP_EMPTY $name" >> "$log"
    fi
    "$clone/scripts/leg.sh" "$name" "$@" >> "$log" 2>&1
    rc=$?
    printf '%-52s rc=%s   %s -> %s\n' "$name" "$rc" "$start" "$(date -Is)" | tee -a "$SUM"
    return $rc
}

echo "A3 TRAIN CHAIN START $(date -Is) phase=$PHASE pid=$$ bench=$(git -C "$BENCH" rev-parse HEAD) bench-base=$(git -C "$BBASE" rev-parse HEAD)" | tee -a "$SUM"
for c in "$BENCH" "$BBASE"; do
    [ -z "$(git -C "$c" status --porcelain)" ] || { echo "STOP: clone dirty: $c" | tee -a "$SUM"; exit 2; }
done

if [ "$PHASE" = build ] || [ "$PHASE" = all ]; then
    for pair in "a3-head:$BENCH" "a3-base:$BBASE"; do
        tag=${pair%%:*}
        c=${pair#*:}
        leg "$c" "$tag-pin" "$c/scripts/build_hven_pin.sh" \
            || { echo "STOP at $tag-pin" | tee -a "$SUM"; exit 1; }
        leg "$c" "$tag-harness" bash -c "set -euo pipefail; cd '$c'; ./configure.sh; cmake --build build --parallel \"\$(nproc)\"" \
            || { echo "STOP at $tag-harness" | tee -a "$SUM"; exit 1; }
    done
    mkdir -p "$T/dumps"
    for L in sqp-ipm $A3_LABELS; do
        (cd "$BENCH" && ./build/lanes/hven/hven_bench_cell --lane hven-sqp --options "$L" \
            --dump-options-only --options-dump "$T/dumps/$L.txt") > "$T/dumps/$L.verdict" 2>&1
        echo "dump $L rc=$? $(grep OPTIONS_VERDICT "$T/dumps/$L.verdict")" | tee -a "$SUM"
    done
    (cd "$BBASE" && ./build/lanes/hven/hven_bench_cell --lane hven-sqp --options sqp-ipm \
        --dump-options-only --options-dump "$T/dumps/base-sqp-ipm.txt") > "$T/dumps/base-sqp-ipm.verdict" 2>&1
    echo "dump base sqp-ipm rc=$?" | tee -a "$SUM"
    for L in $A3_LABELS; do
        diff "$T/dumps/sqp-ipm.txt" "$T/dumps/$L.txt" > "$T/dumps/$L-vs-sqp-ipm.diff"
        echo "diff $L vs sqp-ipm: $(grep -c '^>' "$T/dumps/$L-vs-sqp-ipm.diff") line(s): $(grep '^>' "$T/dumps/$L-vs-sqp-ipm.diff" | tr '\n' ' ')" | tee -a "$SUM"
    done
    diff "$T/dumps/base-sqp-ipm.txt" "$T/dumps/sqp-ipm.txt" > "$T/dumps/head-vs-base-sqp-ipm.diff"
    echo "diff head vs base sqp-ipm: $(grep -c '^[<>]' "$T/dumps/head-vs-base-sqp-ipm.diff") line(s): $(grep '^[<>]' "$T/dumps/head-vs-base-sqp-ipm.diff" | tr '\n' ' ')" | tee -a "$SUM"
fi

if [ "$PHASE" = cells ] || [ "$PHASE" = all ]; then
    run_label_set() { # CLONE CELLDIR TAG LABEL SET RUN PROBLEMS...
        local C=$1 CD=$2 TAG=$3 L=$4 S_=$5 R=$6
        shift 6
        for P in "$@"; do
            leg "$C" "$TAG-$L-$S_-$R-$P" "$C/scripts/w2_a4_sweep.sh" "$L" hven-sqp "$S_" "$R-$P" "$CD" "$P"
            if [ $? -eq 2 ]; then
                echo "STOP: runner REFUSED on $TAG $L $S_ $R $P" | tee -a "$SUM"
                exit 2
            fi
        done
        echo "A3 SET $TAG $L $S_ run $R DONE $(date -Is)" | tee -a "$SUM"
    }
    for R in A B; do
        for L in sqp-ipm $A3_LABELS; do
            run_label_set "$BENCH" "$CELLS" a3 "$L" hs "$R" $HS
            run_label_set "$BENCH" "$CELLS" a3 "$L" cutest "$R" $CU
        done
    done
    run_label_set "$BBASE" "$CELLS_BASE" a3base sqp-ipm hs A $HS
    run_label_set "$BBASE" "$CELLS_BASE" a3base sqp-ipm cutest A $CU
fi
echo "A3 TRAIN CHAIN END $(date -Is)" | tee -a "$SUM"
