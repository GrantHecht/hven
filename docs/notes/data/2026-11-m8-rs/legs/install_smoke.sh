#!/usr/bin/env bash
# usage: install_smoke.sh <out-dir>   Run as: flock /tmp/box-build.lock setsid -w bash install_smoke.sh ...
# check_install_smoke.sh on both front ends (default, clang++), each followed by check_export_contract.sh; the installed
# file listing of each arm is written to <out-dir>/installed-<arm>.txt (prefix-relative, files only, sorted).
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
EV=$(realpath -m "${1:?out}")
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"; worst=0
for ARM in default clang; do
  W=$WT/.scratch/smoke-$ARM
  rm -rf "$W"; mkdir -p "$W"
  if [ "$ARM" = clang ]; then
    HVEN_INSTALL_SMOKE_WORKDIR=$W HVEN_INSTALL_SMOKE_JOBS=16 bash scripts/check_install_smoke.sh -DCMAKE_CXX_COMPILER=clang++ > "$EV/smoke-$ARM.log" 2>&1
  else
    HVEN_INSTALL_SMOKE_WORKDIR=$W HVEN_INSTALL_SMOKE_JOBS=16 bash scripts/check_install_smoke.sh > "$EV/smoke-$ARM.log" 2>&1
  fi
  rc=$?; tail -3 "$EV/smoke-$ARM.log"; echo "smoke $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  TARGETS=$(find "$W" -name hvenTargets.cmake -path '*hven-install*' | head -1)
  PFX=$(dirname "$(dirname "$(dirname "$(dirname "$TARGETS")")")")
  ( cd "$PFX" && find . -type f | sed 's#^\./##' | LC_ALL=C sort ) > "$EV/installed-$ARM.txt"
  ALIGN=$(grep -oE 'resolved to [0-9]+' "$W/hven-configure.log" | head -1 | grep -oE '[0-9]+')
  bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$EV/export-contract-$ARM.log" 2>&1
  rc=$?; tail -3 "$EV/export-contract-$ARM.log"; echo "export-contract $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  echo "installed files $ARM: $(wc -l < "$EV/installed-$ARM.txt"); include/hven/drivers present: $(grep -c '^include/hven/drivers/' "$EV/installed-$ARM.txt")"
done
echo "worst rc=$worst"; echo "FOREGROUND_END $(date -Is)"
exit $worst
