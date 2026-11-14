# M8 PF-HB riders (C246): shared prologue, sourced by every leg. Paths derive from this file's location.
set -o pipefail
RL=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)          # riders/legs
RD=$(dirname "$RL")                                          # riders/
WT=$(git -C "$RL" rev-parse --show-toplevel)                 # the worktree
BP=$WT/build-pfe
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
fin() { echo "rc=$1"; echo "FOREGROUND_END $(date -Is)"; exit "$1"; }
