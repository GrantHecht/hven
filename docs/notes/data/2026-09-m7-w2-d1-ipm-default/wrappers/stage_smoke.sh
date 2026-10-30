#!/usr/bin/env bash
# Install smoke on both front ends, then the export contract on the clang
# smoke's own install tree and its own align value.
set -o pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2d1
echo "=== HEAD $(git -C "$R" rev-parse --short HEAD)  dirty-files=$(git -C "$R" status --porcelain | wc -l) ==="
echo "=== INSTALL SMOKE (default front end) ==="
rm -rf "$S/smoke-default"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-default" "$R/scripts/check_install_smoke.sh" > "$S/logs/smoke-default.log" 2>&1
rc_s1=$?; tail -8 "$S/logs/smoke-default.log"; echo "SMOKE_DEFAULT_RC=$rc_s1"
echo "=== INSTALL SMOKE (clang front end) ==="
rm -rf "$S/smoke-clang"
HVEN_INSTALL_SMOKE_WORKDIR="$S/smoke-clang" "$R/scripts/check_install_smoke.sh" -DCMAKE_CXX_COMPILER=clang++ > "$S/logs/smoke-clang.log" 2>&1
rc_s2=$?; tail -8 "$S/logs/smoke-clang.log"; echo "SMOKE_CLANG_RC=$rc_s2"
echo "=== EXPORT CONTRACT ==="
ALIGN=$(grep -m1 -oE 'EIGEN_MAX_ALIGN_BYTES resolved to [0-9]+' "$S/smoke-clang/hven-configure.log" 2>/dev/null | grep -oE '[0-9]+$')
TGT=$(find "$S/smoke-clang/hven-install" -name hvenTargets.cmake 2>/dev/null | head -1)
echo "align=${ALIGN:-<absent>}  targets=${TGT:-<absent>}"
if [ -n "$TGT" ] && [ -n "$ALIGN" ]; then
  "$R/scripts/check_export_contract.sh" "$TGT" linux "$ALIGN" 2>&1 | tail -8; rc_e=$?
else
  echo "EXPORT: could not locate the clang smoke's install tree or its align value"; rc_e=1
fi
echo "EXPORT_RC=$rc_e"
echo "SMOKE_EXPORT_WORST=$(( rc_s1 | rc_s2 | rc_e ))"
exit $(( rc_s1 | rc_s2 | rc_e ))
