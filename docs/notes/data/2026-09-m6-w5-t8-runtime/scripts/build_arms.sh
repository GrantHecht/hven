#!/bin/bash
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89r
LOG=$R/logs/L1-build.log
LEG_RC=0
fold() { local rc=$1; [ $rc -gt $LEG_RC ] && LEG_RC=$rc; return 0; }
{
echo "PGREP_SEPARATE $(date -u +%Y-%m-%dT%H:%M:%SZ)"
cat $R/logs/L1-pgrep-separate.txt
echo "--- inside lock ---"
echo "PGREP_INLOCK $(date -u +%Y-%m-%dT%H:%M:%SZ)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(date -u +%Y-%m-%dT%H:%M:%SZ)"
for name in arm-base arm-head arm-interior-base; do
  echo "=== configure $name $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  rm -rf $R/$name/build
  CCACHE_DISABLE=1 cmake -S $R/$name -B $R/$name/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -20
  fold ${PIPESTATUS[0]}
  echo "=== build $name $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  CCACHE_DISABLE=1 cmake --build $R/$name/build --target hven_sqp_corpus -j 8 2>&1 | tail -15
  fold ${PIPESTATUS[0]}
  echo "=== done $name rc_so_far=$LEG_RC $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
done
echo "FOREGROUND_END $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
