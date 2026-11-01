#!/usr/bin/env bash
# focused replays: the short/medium dumps under each configuration (one locked leg per configuration)
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
LIST="$D/dumps/DALLASM_f4/qp0.txt $D/dumps/DALLASM_f4/qp1.txt $D/dumps/DALLASM_f4/qp2.txt $D/dumps/DALLASM_f4/qp3.txt $D/dumps/YORKNET_def/qp0.txt $D/dumps/YORKNET_def/qp1.txt $D/dumps/STEENBRB_def/qp0.txt $D/dumps/STEENBRB_def/qp1.txt $D/dumps/STEENBRB_def/qp2.txt $D/dumps/STEENBRB_def/qp3.txt $D/dumps/ACOPR57_def/qp0.txt $D/dumps/ACOPR57_def/qp1.txt $D/dumps/ACOPR57_def/qp2.txt $D/dumps/ACOPR57_def/qp4.txt $D/dumps/ACOPR57_def/qp8.txt $D/dumps/ACOPP57_def/qp0.txt $D/dumps/SAWPATH_def/qp0.txt $D/dumps/SAWPATH_def/qp2.txt"
for cfg in "$@"; do
  name=${cfg//,/+}
  echo $LIST | tr ' ' '\n' | HVEN_D4_FIX=$cfg $D/tools/replay_trace.sh "f-$name" $D/replays/focus-$name "" > $D/replays/focus-$name.summary 2>&1
  echo "DONE $cfg"
done
