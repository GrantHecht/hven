#!/usr/bin/env bash
# THE THREE-SEAM TREE: configure build-3seam from EMPTY with BOTH old-seam
# checkouts named, then build it in full.  Both statuses OR-FOLDED.  The two
# checkouts are READ-ONLY inputs -- this configure is the only thing that reads
# them, and nothing here writes into either.
#
# The recipe is docs/testing.md:1043-1054.  HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM
# is NOT set: the rig's pin checks must FATAL rather than warn.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
worst=0
echo "########## configure build-3seam (both seams) ##########"
bash "$F/configure_one.sh" "$F/build-3seam" Release \
    -DHVEN_RIG_PSIOPT_SEAM=/home/ghecht/Projects/tycho \
    -DHVEN_RIG_SQP_SEAM=/home/ghecht/Projects/tycho_sqp
rc=$?; worst=$((worst | rc)); echo "worst after configure: $worst"
echo "--- the rig's own configure-time seam verdicts ---"
grep -iE 'seam|rig|pin|verified|tycho' "$F/logs/configure-build-3seam.out" | head -40 || true
[ $rc -eq 0 ] || { echo "RIG_BUILD_WORST_RC=$worst"; exit $worst; }
echo "########## build build-3seam (full, -j 8) ##########"
bash "$F/build_one.sh" "$F/build-3seam" 3seam
rc=$?; worst=$((worst | rc)); echo "worst after build: $worst"
echo "RIG_BUILD_WORST_RC=$worst"
exit $worst
