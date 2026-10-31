#!/usr/bin/env bash
# Copies the r3 scratch evidence into gates-r3/ (record). Run AFTER the r3 chain; writes only under gates-r3/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; R="$(git -C "$HERE" rev-parse --show-toplevel)"
F3="$R/.scratch/m7-close/r3"; G="$HERE/gates-r3"; rm -rf "$G"; mkdir -p "$G/logs" "$G/census"
cp "$F3/chain.txt" "$F3/chain.out" "$G/" 2>/dev/null || true
cp "$F3"/selftest-*.txt "$G/"; cp -r "$F3/r2-guard-falsifier-logs" "$G/" 2>/dev/null || true
cp "$F3"/logs/* "$G/logs/" 2>/dev/null || true
cp -r "$F3/census/rows" "$G/census/"; cp "$F3"/census/{walk_census.csv,compare.txt,compare-gated.txt,compare-gated-vs-r2.txt,compare-exact-vs-r2.txt,run.log,serial_confirm_list.txt,tier-reading.txt,provenance_header.txt} "$G/census/" 2>/dev/null || true
[ -d "$F3/char" ] && { mkdir -p "$G/char"; cp "$F3"/char/* "$G/char/"; }
du -sh "$G"
