#!/usr/bin/env bash
# every dump under each candidate configuration; each configuration is its own locked leg
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
for cfg in "$@"; do
  name=${cfg//,/+}
  if [ "$cfg" == "e2off" ]; then
    ls $D/dumps/*/qp*.txt | $D/tools/replay_trace.sh "m-$name" $D/replays/$name "" --e2 0 > $D/replays/$name.summary 2>&1
  else
    ls $D/dumps/*/qp*.txt | HVEN_D4_FIX=$cfg $D/tools/replay_trace.sh "m-$name" $D/replays/$name "" > $D/replays/$name.summary 2>&1
  fi
  echo "DONE $cfg"
done
