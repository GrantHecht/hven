# sourced by every TU-0 leg. WT = the worktree this evidence directory lives in; BASEWT = arm A (origin/m8 at the declared base).
export WT=${WT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)}
export BASEWT=${BASEWT:-/home/ghecht/Projects/hven/.claude/worktrees/tu0-base}
export E=$WT/docs/notes/data/2026-11-m8-tu
export TMPDIR=$WT/.scratch/tmp
export GIT_CEILING_DIRECTORIES=$WT
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "$TMPDIR"
box_check() { echo "PGREP_NOT_GATED (box-lane)"; }
