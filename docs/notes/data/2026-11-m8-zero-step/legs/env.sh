# M8 zero-step gate: shared paths. Sourced by every leg script; nothing here is derived from a carried list.
WT=/home/ghecht/Projects/hven/.claude/worktrees/zero-step
E=$WT/docs/notes/data/2026-11-m8-zero-step
B=$WT/build-zs
export TMPDIR=$B/tmp
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
export GIT_CEILING_DIRECTORIES=$TMPDIR
HB=$WT/.scratch/hb
REPLAY_CELLS_FILE=$WT/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
pgrep_guard() { if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; pgrep -a hven_bench_cell; exit 3; fi; echo PGREP_EMPTY; }
