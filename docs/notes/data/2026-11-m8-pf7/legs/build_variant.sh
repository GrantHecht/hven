#!/usr/bin/env bash
# usage: build_variant.sh <name>  -- build hven from the worktree HEAD+tree, install, build lane binary, copy to .scratch/bin/<name>
set -euo pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/pf7
SRC=${SRC:-$W}
B=/home/ghecht/Projects/hven-bench-pf7
name=$1; shift
export TMPDIR=$W/.scratch/tmp
sha12=$(sed -n 's/^set(HVEN_PIN_SHA "\(.*\)")$/\1/p' $B/cmake/hven_pin.cmake | cut -c1-12)
prefix=$B/.hven/install-$sha12
rm -rf $prefix
cmake -S $SRC -B $W/.scratch/build-$name -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF -DHVEN_LINK_TIME_OPT=OFF -DCMAKE_INSTALL_PREFIX=$prefix "$@" > $W/.scratch/cfg-$name.log 2>&1
cmake --build $W/.scratch/build-$name --parallel ${BOX_LANE_JOBS:-4} > $W/.scratch/bld-$name.log 2>&1
cmake --install $W/.scratch/build-$name > $W/.scratch/inst-$name.log 2>&1
align=$(grep -m1 'EIGEN_MAX_ALIGN_BYTES resolved to' $W/.scratch/cfg-$name.log)
cat > $prefix/HVEN_PIN.txt <<EOT
HVEN_PIN_SHA $(sed -n 's/^set(HVEN_PIN_SHA "\(.*\)")$/\1/p' $B/cmake/hven_pin.cmake)
HVEN_PIN_BRANCH m8
HVEN_PIN_DATE 2026-11-19
HVEN_BUILT_UTC $(date -u +%Y-%m-%dT%H:%M:%SZ)
HVEN_CXX_COMPILER $(clang++ --version | head -1)
HVEN_EIGEN_ALIGN ${align#*hven: }
HVEN_CMAKE_ARGS variant $name (pf7 worktree $(git -C $W rev-parse --short HEAD))
EOT
cd $B
rm -rf build-$name
cmake -S $B -B $B/build-$name -G Ninja -Uhven_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH=$prefix > $W/.scratch/bcfg-$name.log 2>&1
cmake --build $B/build-$name --target hven_bench_cell --parallel ${BOX_LANE_JOBS:-4} > $W/.scratch/bbld-$name.log 2>&1
mkdir -p $W/.scratch/bin
cp $B/build-$name/lanes/hven/hven_bench_cell $W/.scratch/bin/cell-$name
sha256sum $W/.scratch/bin/cell-$name
