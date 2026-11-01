#!/usr/bin/env bash
# Copies the gate's scratch evidence into the record directory (gates/). Run AFTER the chain and the follow-up; writes only under gates/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; R="$(git -C "$HERE" rev-parse --show-toplevel)"
F="$R/.scratch/m7-close"; G="$HERE/gates"; rm -rf "$G"; mkdir -p "$G/logs" "$G/suites" "$G/rig" "$G/replay" "$G/suitev1" "$G/census"
cp "$F/chain.txt" "$F/chain.out" "$F/chain-r2.out" "$G/" 2>/dev/null || true
for f in "$F"/logs/leg-*.log "$F"/logs/configure-*.log "$F"/logs/build-*.log "$F"/logs/preflight-run.txt "$F"/logs/census-*.out "$F"/logs/census-stamp.csv "$F"/logs/replay-*.out \
         "$F"/logs/check-suitev1.txt "$F"/logs/export-contract.out "$F"/logs/install-smoke.out "$F"/logs/smoke-configure.log; do [ -e "$f" ] && cp "$f" "$G/logs/"; done
cp "$F"/selftest-*.txt "$G/logs/"
for f in suite-release suite-debug suite-debug-long inventory-release inventory-debug; do cp "$F/logs/$f.out" "$G/suites/"; done
for f in rig-ctest-scoped rig-ctest-full rig-inventory-scoped rig-inventory-full; do cp "$F/logs/$f.out" "$G/rig/"; done
cp "$F"/replay/* "$G/replay/"
cp -r "$F/suitev1/run-hs" "$F/suitev1/run-cutest" "$G/suitev1/"; cp "$F"/suitev1/*guard*.log "$F"/suitev1/r2-* "$G/suitev1/" 2>/dev/null || true
cp -r "$F/census/rows" "$G/census/"; cp "$F"/census/{walk_census.csv,compare.txt,compare-gated.txt,compare-vs-m6-census-exact.txt,run.log,serial_confirm_list.txt,tier-reading.txt,provenance_header.txt} "$G/census/" 2>/dev/null || true
[ -d "$F/census/confirm" ] && { mkdir -p "$G/census/confirm"; cp "$F"/census/confirm/*.csv "$F"/census/confirm/candidates.txt "$G/census/confirm/"; cp "$F/logs/leg-census-confirm.log" "$G/census/confirm/"; }
du -sh "$G"
