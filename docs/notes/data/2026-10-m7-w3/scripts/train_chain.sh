#!/bin/bash
# W3 TRAIN chain (design page s5.5). Four scratch clones of hven-bench at b5423f6 (local commits, never pushed):
#   bench        pin = W3 measured head 7187a255 + three labels (sqp-ssn + one field each)
#   bench-base   pin = bb512d69 (control C-1)
#   bench-tau10  pin = w3-scratch-tau10  (head + kFunnelTauBar 10)
#   bench-tau1e3 pin = w3-scratch-tau1e3 (head + kFunnelTauBar 1000)
# ONE CELL PER LEG: pgrep -a hven_bench_cell in its own command first, then the clone's scripts/leg.sh
# (pre-lock + in-lock gates, flock /tmp/box-build.lock, no timeout, FOREGROUND_START/END) around the clone's
# scripts/w2_a4_sweep.sh (the set's pinned ceiling, TRAIN split, runners/run_serial.py). HOLDOUT never asked for.
# Usage: train_chain.sh PHASE   (build | cells | all)
set -o pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/w3/.scratch/w3
BENCH=$S/bench; BBASE=$S/bench-base; BT10=$S/bench-tau10; BT1E3=$S/bench-tau1e3
T=$S/train; LOGS=$T/logs; SUM=$LOGS/chain-summary.txt
mkdir -p "$LOGS"
PHASE=${1:-all}
W3_LABELS="sqp-ssn-sigma-armed sqp-ssn-farkas sqp-ssn-certify-face"

roster() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(sorted(p["problem"] for p in d["problems"] if p["split"]=="TRAIN")))' "$BENCH/sets/$1"; }
HS="$(roster hs-27.json)"; CU="$(roster cutest-nlp.json)"

leg() { # CLONE NAME COMMAND...
    local clone=$1 name=$2 log rc start
    shift 2
    log="$LOGS/leg-$name.log"; start="$(date -Is)"
    if pgrep -a hven_bench_cell > "$LOGS/pgrep-$name.txt"; then echo "PGREP_NONEMPTY $name" >> "$log"; else echo "PGREP_EMPTY $name" >> "$log"; fi
    "$clone/scripts/leg.sh" "$name" "$@" >> "$log" 2>&1
    rc=$?
    printf '%-56s rc=%s   %s -> %s\n' "$name" "$rc" "$start" "$(date -Is)" | tee -a "$SUM"
    return $rc
}

echo "W3 TRAIN CHAIN START $(date -Is) phase=$PHASE pid=$$ bench=$(git -C "$BENCH" rev-parse HEAD) base=$(git -C "$BBASE" rev-parse HEAD) tau10=$(git -C "$BT10" rev-parse HEAD) tau1e3=$(git -C "$BT1E3" rev-parse HEAD)" | tee -a "$SUM"
for c in "$BENCH" "$BBASE" "$BT10" "$BT1E3"; do
    [ -z "$(git -C "$c" status --porcelain)" ] || { echo "STOP: clone dirty: $c" | tee -a "$SUM"; exit 2; }
done

if [ "$PHASE" = build ] || [ "$PHASE" = all ]; then
    for pair in "w3-head:$BENCH" "w3-base:$BBASE" "w3-tau10:$BT10" "w3-tau1e3:$BT1E3"; do
        tag=${pair%%:*}; c=${pair#*:}
        leg "$c" "$tag-pin" "$c/scripts/build_hven_pin.sh" || { echo "STOP at $tag-pin" | tee -a "$SUM"; exit 1; }
        leg "$c" "$tag-harness" bash -c "set -euo pipefail; cd '$c'; ./configure.sh; cmake --build build --parallel \"\$(nproc)\"" \
            || { echo "STOP at $tag-harness" | tee -a "$SUM"; exit 1; }
    done
    mkdir -p "$T/dumps"
    dump() { # CLONE LABEL OUTNAME
        (cd "$1" && ./build/lanes/hven/hven_bench_cell --lane hven-sqp --options "$2" --dump-options-only --options-dump "$T/dumps/$3.txt") > "$T/dumps/$3.verdict" 2>&1
        echo "dump $3 rc=$? $(grep OPTIONS_VERDICT "$T/dumps/$3.verdict")" | tee -a "$SUM"
    }
    for L in sqp-ssn default $W3_LABELS; do dump "$BENCH" "$L" "$L"; done
    dump "$BBASE" sqp-ssn base-sqp-ssn
    dump "$BT10" default tau10-default
    dump "$BT1E3" default tau1e3-default
    for L in $W3_LABELS; do
        diff "$T/dumps/sqp-ssn.txt" "$T/dumps/$L.txt" > "$T/dumps/$L-vs-sqp-ssn.diff"
        echo "diff $L vs sqp-ssn: $(grep -c '^>' "$T/dumps/$L-vs-sqp-ssn.diff") line(s): $(grep '^>' "$T/dumps/$L-vs-sqp-ssn.diff" | tr '\n' ' ')" | tee -a "$SUM"
    done
    for x in base-sqp-ssn:sqp-ssn tau10-default:default tau1e3-default:default; do
        a=${x%%:*}; b=${x#*:}
        diff "$T/dumps/$b.txt" "$T/dumps/$a.txt" > "$T/dumps/$a-vs-$b.diff"
        echo "diff $a vs $b: $(grep -c '^[<>]' "$T/dumps/$a-vs-$b.diff") line(s)" | tee -a "$SUM"
    done
fi

if [ "$PHASE" = cells ] || [ "$PHASE" = all ]; then
    run_label_set() { # CLONE CELLDIR TAG LABEL SET RUN PROBLEMS...
        local C=$1 CD=$2 TAG=$3 L=$4 S_=$5 R=$6
        shift 6
        for P in "$@"; do
            leg "$C" "$TAG-$L-$S_-$R-$P" "$C/scripts/w2_a4_sweep.sh" "$L" hven-sqp "$S_" "$R-$P" "$CD" "$P"
            if [ $? -eq 2 ]; then echo "STOP: runner REFUSED on $TAG $L $S_ $R $P" | tee -a "$SUM"; exit 2; fi
        done
        echo "W3 SET $TAG $L $S_ run $R DONE $(date -Is)" | tee -a "$SUM"
    }
    for R in A B; do
        for L in sqp-ssn $W3_LABELS default; do
            run_label_set "$BENCH" "$T/cells" w3 "$L" hs "$R" $HS
            run_label_set "$BENCH" "$T/cells" w3 "$L" cutest "$R" $CU
        done
        run_label_set "$BT10" "$T/cells-tau10" w3t10 default hs "$R" $HS
        run_label_set "$BT10" "$T/cells-tau10" w3t10 default cutest "$R" $CU
        run_label_set "$BT1E3" "$T/cells-tau1e3" w3t1e3 default hs "$R" $HS
        run_label_set "$BT1E3" "$T/cells-tau1e3" w3t1e3 default cutest "$R" $CU
    done
    run_label_set "$BBASE" "$T/cells-base" w3base sqp-ssn hs A $HS
    run_label_set "$BBASE" "$T/cells-base" w3base sqp-ssn cutest A $CU
fi
echo "W3 TRAIN CHAIN END $(date -Is)" | tee -a "$SUM"
