W5 T8.9r-attrib4 -- THE SCRIPTS, AND WHERE THEY EXPECT TO RUN
============================================================

Every script here ran from this leg's scratch root,
/home/ghecht/Projects/hven/.scratch/w5t89ra4, and the absolute paths inside them
are that root's. They are retained VERBATIM as they ran -- not rewritten to the
artifact's own paths -- because a rewritten script is not the script that
produced the data.

  common.sh        the T8.9r fix1 common.sh with this leg's scratch root, this
                   leg's title, and ONE addition: a batch-wide /proc/stat
                   bracket (batch_start/batch_end), so no jiffy of a batch
                   window falls outside some bracket. Nothing else differs from
                   ../../t84/scripts/common.sh, which is itself the fix1 file.
  run_batch.sh     the driver: the pgrep audit OUTSIDE the lock, then the batch
                   under flock /tmp/box-build.lock.
  arms.sh          the four MEASURED arms and the cell list (the argv lock:
                   every binary directory is seven characters).
  arms_build.sh    the three BUILT arms, `cal____` being the recipe calibration.
  build_arms.sh    the builds. Same flags as ../../../scripts/build_arms.sh.
  gate.sh          the correctness gate's captures (`--cells all`, untimed).
  gate_check.py    the gate's comparison. Run:
                     python3 -B gate_check.py <head csv> <patched csv>...
                   against ../raw/gate/. Exit 0 when every arm passes.
  leg_wall.sh      ONE wall round = ONE batch. Asserts wall clock.
  leg_pf.sh        the page-fault pass. Declared co-run tolerant.
  leg_e3.sh        E3: the fault-site profile and the dTLB/L1 counters.
  fold_proof.sh    the worst-fold proof, both ways (logs/F1-fold-proof-*.log).
  idle_proof.py    the idle proof, WITH the nice-inclusive correction astra's
                   fix1-review item 7 required. Run:
                     python3 -B idle_proof.py <batch logs...> <out.md>
                   It reproduces ../logs/IDLE-PROOF-WALL.md byte for byte from
                   ../logs/W*-wall-r*.log and exits 0.
  redraw.py        the per-row wall table and the recovery. It reads
                   <its own dir>/art/csv/w-<arm>-r<round>.csv and WRITES
                   <its own dir>/wall.csv, so running it from this directory
                   needs ../raw/csv copied to ./art/csv first. Run from the
                   scratch root it reproduces ../redraw.out byte for byte.
