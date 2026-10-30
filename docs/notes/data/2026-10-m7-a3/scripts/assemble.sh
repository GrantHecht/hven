#!/bin/bash
# Copy the A3 artifacts into docs/notes/data/2026-10-m7-a3/ (file processing only).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff
S=$W/.scratch/a3
D=$W/docs/notes/data/2026-10-m7-a3
mkdir -p "$D/out/meas" "$D/out/suites" "$D/out/mutants" "$D/out/psym" "$D/out/train" "$D/out/dev" "$D/scripts" "$D/logs"
python3 -B "$S/score_meas.py" "$S/meas" "$W/docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv" > "$D/out/meas/scored.txt"
cp "$S"/meas/*.csv "$S"/meas/*.txt "$S"/meas/*.patch "$D/out/meas/"
cp "$S"/meas/logs/*.log "$D/logs/"
cp "$S"/dev/*.csv "$D/out/dev/"
for s in rel dbg long; do grep -v "^\s*[0-9]*/[0-9]* Test\|^\s*Start " "$S/logs/final-suite-$s.log" > "$D/out/suites/final-suite-$s.txt"; done
cp "$S/logs/chain-a.txt" "$S/logs/chain-final.txt" "$D/out/suites/"
cp "$S/logs/mutants.txt" "$D/out/mutants/"
cp "$S/psym/compare-Release.txt" "$S/psym/compare-Debug.txt" "$S/psym/classification.txt" "$D/out/psym/"
for f in build.sh leg.sh suite.sh chain_base.sh chain_a.sh chain_meas.sh chain_final.sh dev_probe.sh dev_probe_dbg.sh \
    score_meas.py sticky_patch.py mutants.py psym.sh psym_classify.py set_pin.py edit_lane.py train_chain.sh \
    train_close.py assemble.sh; do
    cp "$S/$f" "$D/scripts/"
done
# (the two scratch-clone patches are written by `git format-patch` run separately)
# TRAIN
cp -r "$S/train/out/." "$D/out/train/"
mkdir -p "$D/out/train/dumps" "$D/out/train/leg-logs" "$D/out/train/aborted-attempt-1"
cp "$S"/train/dumps/* "$D/out/train/dumps/"
cp "$S/train/logs/chain-summary.txt" "$D/out/train/"
cp "$S"/train/logs/leg-*.log "$D/out/train/leg-logs/"
cp "$S/train/logs-aborted1/chain-summary.txt" "$D/out/train/aborted-attempt-1/"
for f in percell.py preview_a.py; do cp "$S/$f" "$D/scripts/"; done
echo "assembled into $D"
