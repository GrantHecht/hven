# sourced by every RS leg: the one place the worktree paths live
export WT=${WT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)}   # the worktree this evidence directory lives in (WT may be preset)
export TMPDIR=$WT/.scratch/tmp
export GIT_CEILING_DIRECTORIES=$WT
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$TMPDIR"
box_check() {
  if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; pgrep -a hven_bench_cell; exit 3; fi
  echo PGREP_EMPTY
}
