# sourced by every I18 leg. WT = the worktree this evidence directory lives in; BASEWT = arm A (the sibling worktree i18-base).
export WT=${WT:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)}
export BASEWT=${BASEWT:-$(dirname "$WT")/i18-base}
export E=$WT/docs/notes/data/2026-11-m8-i18
export L=$E/legs
export TMPDIR=$WT/.scratch/tmp
export GIT_CEILING_DIRECTORIES=$WT
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 CCACHE_DISABLE=1
export JOBS=${BOX_LANE_JOBS:-4}
export PHYS=${BOX_LANE_PHYS:-0 1 2 3}
mkdir -p "$TMPDIR"
[ -f "$WT/docs/notes/data/2026-11-m8-i18/legs/env.sh" ] || { echo "ENV_WT_NOT_DERIVED $WT"; exit 90; }
[ -d "$BASEWT/.git" ] || [ -f "$BASEWT/.git" ] || { echo "NO_BASE_WORKTREE $BASEWT"; exit 90; }
