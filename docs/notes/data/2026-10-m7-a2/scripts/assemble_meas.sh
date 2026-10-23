#!/bin/bash
# Copy the hven-side measurement artifacts into the data directory (file processing only).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
S=$W/.scratch/a2
D=$W/docs/notes/data/2026-10-m7-a2
mkdir -p "$D/out/meas" "$D/out/psym" "$D/out/mutants" "$D/out/suites" "$D/scripts" "$D/logs"
python3 -B "$S/score_meas.py" "$S/meas" "$W/docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv" \
    > "$D/out/meas/scored.txt" || exit 1
cp "$S"/meas/*.csv "$S"/meas/*.txt "$D/out/meas/"
cp "$S"/meas/logs/*.log "$D/logs/"
cp "$S/psym/compare-Release.txt" "$S/psym/compare-Debug.txt" "$S/psym/classification.txt" "$D/out/psym/"
cp "$S/logs/mutants.txt" "$D/out/mutants/mutants-round1.txt"
cp "$S/logs/mutants-r2.txt" "$D/out/mutants/mutants-round2.txt"
grep -v "^\s*[0-9]* - " "$S/logs/chain-final.txt" > "$D/out/suites/chain-final.txt"
for f in build.sh leg.sh suite.sh chain_suites.sh chain_build_suites.sh chain_meas.sh chain_final.sh \
    mutants.py run_mutants.sh score_meas.py psym.sh psym_classify.py train_chain.sh train_close.py \
    assemble_meas.sh; do
    cp "$S/$f" "$D/scripts/"
done
echo "assembled into $D"
