# sourced by every I17 leg. WT = the worktree this evidence directory lives in (preset for the base arm); BASEWT = arm A.
export WT=${WT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)}
export BASEWT=${BASEWT:-/home/ghecht/Projects/hven/.claude/worktrees/i17-base}
export E=$WT/docs/notes/data/2026-11-m8-i17
export TMPDIR=$WT/.scratch/tmp
export GIT_CEILING_DIRECTORIES=$WT
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$TMPDIR"
box_check() {
  if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; pgrep -a hven_bench_cell; exit 3; fi
  echo PGREP_EMPTY
}
