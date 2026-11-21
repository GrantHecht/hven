# PENDING -- PF-3 solo wall leg (not run; schedule when the box is quiet)

Sampled wall time, serial and solo: one cell at a time, nothing else running on the machine (CLAUDE.md section 7). Same cells, same
symbol -> component table (`tools/components.py`), same callchain method as the instruction leg; the sampler is
`perf record -e cpu-clock -F 499` (component values come out in seconds). Deadline-class cells are cut at 300 s.
Expected cost: the sum of the baseline walls in `cells.csv` plus pass overhead (~1 h 40 min for the 90 cells, dominated by
the SQP STEENBR* and ACOPP118 cells and the 12 deadline-class cells at 300 s each).

Preconditions (check first, in their own command): `pgrep -af 'ninja|ctest|hven_|run_walk_census'` empty, and `lslocks | grep box-build`
shows no holder.

```bash
cd /home/ghecht/Projects/hven-bench-pf3          # hven-bench e03cbad, pin 48e1eeae built; sets/cutest/sif and .scratch/h3 symlinked
PF3=/home/ghecht/Projects/hven/.claude/worktrees/pf3/docs/notes/data/2026-11-m8-pf3
mkdir -p $PF3/out/solo
MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$PWD/.scratch setsid -w flock /tmp/box-build.lock \
  python3 $PF3/tools/pf3_corun.py $PF3/tools/cells_solo.csv $PF3/out/solo 1 2 \
  > $PF3/out/solo/run.log 2>&1
python3 $PF3/tools/pf3_report.py $PF3/out/solo $PF3/out/solo-tables      # same aggregator, seconds instead of instructions
```

The width argument 1 and the single cpu `2` make it serial. The aggregate reads the `unit` field of each JSON.
