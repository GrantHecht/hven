#!/usr/bin/env bash
# Runs alone_rerun.py (each cell under box-lane --exclusive, serial).  usage: run_alone_rerun.sh <out dir> <log>
D=$(cd "$(dirname "$0")" && pwd)
/home/ghecht/Projects/hven-bench-i19/.scratch/venv/bin/python $D/alone_rerun.py run "$1" > "$2" 2>&1
echo "DONE rc=$?" >> "$2"
