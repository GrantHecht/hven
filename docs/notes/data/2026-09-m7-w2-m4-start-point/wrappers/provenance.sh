#!/usr/bin/env bash
# The artifact's section 4 provenance stamp (CLAUDE.md section 7: toolchain,
# hardware, date, commit). Run at the committed, clean tree.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m4
echo "date_utc:        $(date -u -Is)"
echo "host:            $(uname -sr)  $(nproc) logical cores"
echo "cpu:             $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')"
echo "compiler:        $(/usr/bin/clang++ --version | head -1)"
echo "cmake:           $(cmake --version | head -1)"
echo "hven commit:     $(git -C "$R" rev-parse HEAD)"
echo "hven branch:     $(git -C "$R" rev-parse --abbrev-ref HEAD)"
echo "hven tree state: $(git -C "$R" status --porcelain | wc -l) modified paths (0 = clean)"
echo "harness commit:  $(git -C /home/ghecht/Projects/hven-bench rev-parse HEAD) (READ ONLY; not modified)"
echo "harness pin:     3904a477771524b85b34678fb7d2ff55dd4768f1 (the stamp these binaries carry, and it is FALSE for them -- see section 2)"
echo "MKL threads:     MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 in every cell process"
echo "wall ceiling:    300 s per cell (sets/cutest-nlp.json run_terms.budget_wall_s)"
echo "budget-iters:    0 (the engine's own SqpOptions::max_iter = 100)"
for a in off on; do
  if [ -f "$S/install-$a/include/hven/drivers/sqp_solver_types.h" ]; then
    echo "arm $a header sha256:  $(sha256sum "$S/install-$a/include/hven/drivers/sqp_solver_types.h" | cut -d' ' -f1)"
    echo "arm $a default:        $(grep -m1 'bool project_start_point' "$S/install-$a/include/hven/drivers/sqp_solver_types.h" | sed 's/^ *//')"
  fi
  if [ -f "$S/bench-$a/lanes/hven/hven_bench_cell" ]; then
    echo "arm $a cell sha256:    $(sha256sum "$S/bench-$a/lanes/hven/hven_bench_cell" | cut -d' ' -f1)"
  fi
done
