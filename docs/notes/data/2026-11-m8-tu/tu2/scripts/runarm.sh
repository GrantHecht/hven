#!/bin/bash
# usage: runarm.sh ARM...   (sequential; stops on first failure)
cd /home/ghecht/Projects/hven-bench-tu2
export TMPDIR=$PWD/.scratch/tmp
for a in "$@"; do
  .scratch/venv/bin/python tu/tu2_run.py arm $a --store .scratch/tu2-store >> .scratch/logs/arm-$a.log 2>&1 || { echo "ARM $a FAILED rc=$?" >> .scratch/logs/driver.log; exit 1; }
  echo "ARM $a DONE $(date -u +%FT%TZ)" >> .scratch/logs/driver.log
done
