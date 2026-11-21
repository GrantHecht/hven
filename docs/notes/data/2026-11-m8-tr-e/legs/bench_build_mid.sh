#!/usr/bin/env bash
# usage: bench_build_mid.sh     Run as: flock /tmp/box-build.lock bash bench_build_mid.sh
# ARM A' (r4): hven-bench main d7a55b9 -- the OLD lane code (hand-enumerated side file) -- built against the NEW hven pin prefix.
# The only edit is the pin file's sha/branch/date (one local commit in the private clone), so the stamp trap passes. Comparing A' with the
# TR-E arm isolates what TR-E's CONSUMER change did, at one and the same hven. Also re-syncs bench-new to the branch tip and rebuilds its lane
# (r4: the array-value fix) and copies the decoded CUTEst problems (ignored data) into every clone.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-e
S=$WT/.scratch
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY; echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$S/tmp GIT_CEILING_DIRECTORIES=$S
NEWSHA=24b2a7e42f3a56f1f3293fe9d70d13fc4d50e708
cd $S
rm -rf bench-mid
git clone -q /home/ghecht/Projects/hven-bench bench-mid && git -C bench-mid checkout -q d7a55b9 || exit 1
git -C bench-new fetch -q origin && git -C bench-new reset -q --hard origin/tr-e || exit 1
for a in mid new old; do
  mkdir -p $S/bench-$a/sets/cutest; cp -a /home/ghecht/Projects/hven-bench/sets/cutest/sif $S/bench-$a/sets/cutest/
  mkdir -p $S/bench-$a/.scratch/h3/decoded
  for p in ACOPP57 SMBANK READING6 ZAMB2-8 DALLASM; do cp -a /home/ghecht/Projects/hven-bench/.scratch/h3/decoded/$p $S/bench-$a/.scratch/h3/decoded/; done
done
cd bench-mid
sed -i "s/^set(HVEN_PIN_SHA \".*\")/set(HVEN_PIN_SHA \"$NEWSHA\")/; s/^set(HVEN_PIN_BRANCH \".*\")/set(HVEN_PIN_BRANCH \"tr-e\")/; s/^set(HVEN_PIN_DATE \".*\")/set(HVEN_PIN_DATE \"2026-11-17\")/" cmake/hven_pin.cmake
git -c user.name=tre -c user.email=tre@localhost commit -q -am "arm A prime: pin file set to the TR-E hven head (local clone only)"
mkdir -p .hven && ln -sfn $S/bench-new/.hven/install-${NEWSHA:0:12} .hven/install-${NEWSHA:0:12}
git rev-parse HEAD
GIT_DIR=$PWD/.git GIT_WORK_TREE=$PWD setsid -w bash ./configure.sh -DHVEN_BENCH_CUTEST_ROOT=/home/ghecht/Software/cutest > $S/legs/configure-mid.log 2>&1 || { echo CONFIGURE_FAIL; tail -20 $S/legs/configure-mid.log; exit 1; }
setsid -w ninja -C build -j16 hven_bench_cell || exit 1
sha256sum $PWD/build/lanes/hven/hven_bench_cell
cd ../bench-new && setsid -w ninja -C build -j16 hven_bench_cell || exit 1
git rev-parse HEAD; sha256sum $PWD/build/lanes/hven/hven_bench_cell
echo "FOREGROUND_END $(date -Is)"
