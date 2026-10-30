#!/bin/bash
# A7 TRAIN chain (design page section 4.4), against the scratch clone of hven-bench at 40bb726 whose
# lane gains exactly the two A7 labels (sqp-wsrule, sqp-qn-wsrule) plus the A7_FIRE_LOG observation
# hook, and whose pin is this branch's measured head (local commits in the clone, never pushed).
# ONE CELL PER LEG: `pgrep -a hven_bench_cell` in its own command first, then the clone's
# scripts/leg.sh (its own pre-lock and in-lock gates, flock /tmp/box-build.lock, no timeout,
# FOREGROUND_START/END) around the clone's scripts/w2_a4_sweep.sh (the set's pinned ceiling, TRAIN
# split, runners/run_serial.py). HOLDOUT is never asked for.
#
# Usage: train_chain.sh PHASE   (PHASE = build | cells | all)
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
BENCH=$W/.scratch/a7/bench
T=$W/.scratch/a7/train
LOGS=$T/logs
CELLS=$T/cells
SUM=$LOGS/chain-summary.txt
mkdir -p "$LOGS" "$CELLS"
PHASE=${1:-all}
export A7_FIRE_LOG=$T/fire-log.txt

roster() { # SET -> sorted TRAIN problems
    python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(sorted(p["problem"] for p in d["problems"] if p["split"]=="TRAIN")))' "$BENCH/sets/$1"
}
HS="$(roster hs-27.json)"
CU="$(roster cutest-nlp.json)"

leg() { # NAME COMMAND...
    local name=$1 log rc start
    shift
    log="$LOGS/leg-$name.log"
    start="$(date -Is)"
    if pgrep -a hven_bench_cell > "$LOGS/pgrep-$name.txt"; then
        echo "PGREP_NONEMPTY $name" >> "$log"
    else
        echo "PGREP_EMPTY $name" >> "$log"
    fi
    "$BENCH/scripts/leg.sh" "$name" "$@" >> "$log" 2>&1
    rc=$?
    printf '%-52s rc=%s   %s -> %s\n' "$name" "$rc" "$start" "$(date -Is)" | tee -a "$SUM"
    return $rc
}

echo "A7 TRAIN CHAIN START $(date -Is) phase=$PHASE pid=$$ bench=$(git -C "$BENCH" rev-parse HEAD)" | tee -a "$SUM"
[ -z "$(git -C "$BENCH" status --porcelain)" ] || { echo "STOP: bench clone dirty" | tee -a "$SUM"; exit 2; }

if [ "$PHASE" = build ] || [ "$PHASE" = all ]; then
    leg a7-pin "$BENCH/scripts/build_hven_pin.sh" \
        || { echo "STOP at a7-pin" | tee -a "$SUM"; exit 1; }
    leg a7-harness bash -c "set -euo pipefail; cd '$BENCH'; ./configure.sh; cmake --build build --parallel \"\$(nproc)\"" \
        || { echo "STOP at a7-harness" | tee -a "$SUM"; exit 1; }
    # The label dumps, and each A7 label's diff against its base (must be exactly its one field).
    mkdir -p "$T/dumps"
    for L in default sqp-wsrule sqp-qn sqp-qn-wsrule; do
        (cd "$BENCH" && ./build/lanes/hven/hven_bench_cell --lane hven-sqp --options "$L" \
            --dump-options-only --options-dump "$T/dumps/$L.txt") > "$T/dumps/$L.verdict" 2>&1
        echo "dump $L rc=$? $(grep OPTIONS_VERDICT "$T/dumps/$L.verdict")" | tee -a "$SUM"
    done
    for pair in default:sqp-wsrule sqp-qn:sqp-qn-wsrule; do
        B=${pair%%:*}; L=${pair##*:}
        diff "$T/dumps/$B.txt" "$T/dumps/$L.txt" > "$T/dumps/$L-vs-$B.diff"
        echo "diff $L vs $B: $(grep -c '^[<>]' "$T/dumps/$L-vs-$B.diff") line(s): $(grep '^[<>]' "$T/dumps/$L-vs-$B.diff" | tr '\n' ' ')" | tee -a "$SUM"
    done
fi

if [ "$PHASE" = cells ] || [ "$PHASE" = all ]; then
    run_label_set() { # LABEL SET RUN PROBLEMS...
        local L=$1 S=$2 R=$3
        shift 3
        for P in "$@"; do
            echo "RUN $L $S $R $P" >> "$A7_FIRE_LOG"
            leg "a7-$L-$S-$R-$P" "$BENCH/scripts/w2_a4_sweep.sh" "$L" hven-sqp "$S" "$R-$P" "$CELLS" "$P"
            if [ $? -eq 2 ]; then
                echo "STOP: runner REFUSED on $L $S $R $P" | tee -a "$SUM"
                exit 2
            fi
        done
        echo "A7 SET $L $S run $R DONE $(date -Is)" | tee -a "$SUM"
    }
    for R in A B; do
        for L in default sqp-wsrule sqp-qn sqp-qn-wsrule; do
            run_label_set "$L" hs "$R" $HS
            run_label_set "$L" cutest "$R" $CU
        done
    done
fi
echo "A7 TRAIN CHAIN END $(date -Is)" | tee -a "$SUM"
