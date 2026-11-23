#!/usr/bin/env bash
# I19-1: install smoke and export contract on both front ends (the walk-cap round's install_smoke.sh,
# re-pointed). Run as: box-lane -- bash install_smoke.sh <repo-root> <scratch-dir> <evidence-dir>
set -o pipefail
WT=${1:?repo root}; SCR=${2:?scratch dir}; EV=${3:?evidence dir}
export MKL_NUM_THREADS=1 TMPDIR=$SCR/tmp
mkdir -p "$TMPDIR" "$EV"
cd "$WT" || exit 1
worst=0
for ARM in default clang; do
  W=$SCR/smoke-$ARM
  rm -rf "$W"; mkdir -p "$W"
  echo "SMOKE_START $ARM $(date -Is)"
  EXTRA=()
  [ "$ARM" = clang ] && EXTRA=(-DCMAKE_CXX_COMPILER=clang++)
  HVEN_INSTALL_SMOKE_WORKDIR=$W HVEN_INSTALL_SMOKE_JOBS=${BOX_LANE_JOBS:-4} bash scripts/check_install_smoke.sh "${EXTRA[@]}" > "$EV/smoke-$ARM.log" 2>&1
  rc=$?; tail -n 4 "$EV/smoke-$ARM.log"; echo "smoke $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  TARGETS=$(find "$W" -name hvenTargets.cmake -path '*hven-install*' | head -n 1)
  ALIGN=$(grep -oE 'resolved to [0-9]+' "$W/hven-configure.log" | head -n 1 | grep -oE '[0-9]+')
  bash scripts/check_export_contract.sh "$TARGETS" linux "$ALIGN" > "$EV/export-contract-$ARM.log" 2>&1
  rc=$?; cat "$EV/export-contract-$ARM.log"; echo "export-contract $ARM rc=$rc"; [ $rc -gt $worst ] && worst=$rc
done
echo "smoke worst rc=$worst"
exit $worst
