W5 T8.9r-attrib3 -- THE SCRIPTS, AS THEY RAN
============================================
Paths are absolute and point at this leg's scratch root,
/home/ghecht/Projects/hven/.scratch/w5t89ra3/, which is where every one of them
ran. They are retained as they ran, not rewritten to be portable.

  common.sh        THE SOLO MECHANICS. It is T8.9r-attrib2's common.sh
                   (../../raw/scripts/common.sh) with the two scratch paths
                   repointed and the leg label changed, AND NOTHING ELSE -- which
                   in turn is the T8.9r FIX-ROUND-1 common.sh with the same two
                   edits. So R2's discipline here is reproduced by running fix1's
                   own code: set -o pipefail; the worst-fold `fold`; `run`;
                   `box_snapshot`/`box_guard` (pause on a foreign `R`, never
                   signal); `cpu_stat`/`timed_run` (the /proc/stat bracket around
                   EVERY timed run, not around the batch); the driving shell
                   pinned OFF cpu2 and cpu10.
  run_batch.sh     the driver: the pgrep audit OUTSIDE the box lock, then the leg
                   UNDER it, then `rc=$?; echo WRAPPER_EXIT=$rc; exit $rc`.
  arms.sh          the six bisect arms and the cell list (the argv lock's note)
  arms_exp.sh      experiments 1 and 2's four arms
  arms_exp3.sh     experiment 3's five arms
  arms_mem.sh      the memory leg's three arms (two of them the same bytes)
  build_arms.sh    the six bisect arms' builds
  build_exp.sh     the experiment-1 and experiment-2 arms' builds
  build_exp3.sh    experiment 3's three padded arms' builds
  smoke.sh         the untimed smoke (row order, row counts, the pin accepted)
  fold_proof.sh    the worst-fold proof, run through common.sh's own code
  leg_wall.sh      Step A's wall rounds (six arms, rotated)
  leg_wall_exp.sh  experiments 1 and 2's wall rounds (four arms, rotated)
  leg_wall_exp3.sh experiment 3's wall rounds (five arms, rotated)
  leg_wall_pf.sh   the wall condition with the fault count added (%R)
  leg_alloc.sh     experiment 4 (four arms: two modes x two commits)
  leg_perf.sh      the first perf record (one big cell)
  leg_perf2.sh     perf record, parent vs culprit, three alternating rounds
  leg_perfstat.sh  perf stat across all six bisect arms, three rounds
  leg_mem.sh       page faults / dTLB / kernel time, six rounds
  screen_malloc.sh the screening probe (nothing in any table comes from it)
  idle_proof.py    the T8.9r artifact's own ../../scripts/idle_proof.py, BYTE
                   IDENTICAL, reused unchanged. It is what computes every
                   IDLE-PROOF*.md in logs/ and it exits 0 over all five.
  probe.cpp        prints sizeof/alignof(InteriorPointSolver); also the TU whose
                   record layouts layout.txt is cut from
  probe2.cpp       prints sizeof of the solver and of its result_ member type
