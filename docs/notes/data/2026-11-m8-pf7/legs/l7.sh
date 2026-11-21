#!/usr/bin/env bash
# L7: instrumentation experiment re-taken: A and NI over HS (44), nlp-35 full (34), small-dense (194), co-run width 4 (lane physical CPUs), instructions:u.  Under `box-lane --`.
W=/home/ghecht/Projects/hven/.claude/worktrees/pf7; D=$W/docs/notes/data/2026-11-m8-pf7
CPUS=$(echo $BOX_LANE_PHYS | tr ' ' ',')
cd /home/ghecht/Projects/hven-bench-pf7; mkdir -p $D/gate/l7
sha256sum $W/.scratch/bin/cell-A $W/.scratch/bin/cell-NI
for v in A NI; do for s in hs nlp35 sd; do
  python3 $D/tools/icount.py $W/.scratch/bin/cell-$v $D/cells-$s.csv $D/gate/l7/icount-$v-$s.csv $CPUS 300
done; done
