# Shared constants for the M7 CLOSE GATE.  Sourced, never executed.
# Every path is DERIVED from this file's own location (the worktree root) or from git;
# nothing is carried from the M6 close gate's env.sh.
_here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="$(git -C "$_here" rev-parse --show-toplevel)"
REC=$R/docs/notes/data/2026-10-m7-close-gate
W=$REC/wrappers-close
F=$R/.scratch/m7-close          # scratch: trees, logs, raw outputs (git-ignored); copied into $REC/gates at the end
LOGS=$F/logs
# THE PIN: the gated code head.  HEAD of this worktree is a DOCS-ONLY declaration commit on top of it
# (preflight asserts that, mechanically).
PIN=d4b8e84b0283c83e89afa9959544c25577d80519
PIN12=${PIN:0:12}
BENCH=/home/ghecht/Projects/hven-bench
# Records this gate reads (all frozen):
WX2=$R/docs/notes/data/2026-10-m7-wx2-gates            # the latest gated record (exact arm baseline, suite name baseline)
M6=$R/docs/notes/data/2026-09-m6-close-gate            # the 43-set's M6 pin and the last census
CELLS_FILE=$R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt
CENSUS_BASELINE=$R/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv
# Configure lines: the linux-clang presets (the uniform flag regime), binaryDir redirected under $F.
CONF_REL=(--preset linux-clang-release -B "$F/build-release" -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON)
CONF_DBG=(--preset linux-clang-debug   -B "$F/build-debug"   -DHVEN_BUILD_TESTS=ON)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
export CCACHE_DISABLE=1
LONG_TEST='Continuation.ProbeBudgetBoundsAFailingProposal'
