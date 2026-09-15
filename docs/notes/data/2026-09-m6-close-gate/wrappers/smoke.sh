#!/usr/bin/env bash
# The install smoke and the export contract on a FRESH install, each with its
# OWN status captured and OR-FOLDED, and NO tree under /tmp (the box lock is
# this task's only /tmp path).  The trailing echoes cannot mask either status.
#
# DIFFERENT FROM THE W6 T7 COPY IN ONE RESPECT: the expected align bytes are
# READ FROM THIS BUILD'S OWN CONFIGURE LOG, as check_export_contract.sh's usage
# text requires ("the value THIS SAME BUILD's configure log reported ... Never a
# literal picked ahead of time"), instead of the literal 32.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
SCRIPTS="${HVEN_CLOSE_SCRIPTS:-$R/scripts}"
W="$F/smokework"
rm -rf "$W"; mkdir -p "$W"
export HVEN_INSTALL_SMOKE_WORKDIR="$W"
export HVEN_INSTALL_SMOKE_JOBS=8
worst=0
echo "=== install smoke (workdir $W; LTO OFF -- the script's default) ==="
CCACHE_DISABLE=1 bash "$SCRIPTS/check_install_smoke.sh" > "$W/smoke.out" 2>&1
smoke_rc=$?; worst=$((worst | smoke_rc))
tail -8 "$W/smoke.out"
echo "standalone-include TUs named in the smoke log: $(grep -cE 'include_[a-z_]+\.cpp' "$W/smoke.out")"
echo "front end resolved to: $(grep -m1 -E 'The CXX compiler identification' "$W/hven-configure.log" 2>/dev/null || echo '<not found>')"
echo "SMOKE_RC=$smoke_rc  (worst $worst)"
echo "=== export contract (linux; align read from THIS build's configure log) ==="
ALIGN=$(grep -m1 -oE 'EIGEN_MAX_ALIGN_BYTES resolved to [0-9]+' "$W/hven-configure.log" 2>/dev/null | grep -oE '[0-9]+$')
echo "align from configure log: ${ALIGN:-<NOT FOUND>}"
TARGETS=$(find "$W/hven-install" -name hvenTargets.cmake 2>/dev/null | head -1)
echo "hvenTargets.cmake: ${TARGETS:-<NOT FOUND>}"
if [ -z "$TARGETS" ] || [ -z "$ALIGN" ]; then
  echo "EXPORT_RC=3 (no hvenTargets.cmake and/or no align value found)"; worst=$((worst | 3))
else
  bash "$SCRIPTS/check_export_contract.sh" "$TARGETS" linux "$ALIGN"
  export_rc=$?; worst=$((worst | export_rc))
  echo "EXPORT_RC=$export_rc  (worst $worst)"
fi
echo "SMOKE_EXPORT_WORST_RC=$worst"
exit $worst
