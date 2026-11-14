#!/usr/bin/env bash
# M8 DN-1d r3 (../../DECLARATION-r3.txt): r2's copy of this file re-pointed (r3/, build-dn1de3/); older provenance lines below are historical.
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt D): the detector on E0's dumped factorizations (the 250 the E0 probe dumped: every one its
# rook D-ratio screened or found exactly singular, 106 CAT / 53 EXACT / 91 OTHER by E0's classes), with the CODE HEAD's
# installed library (the lane setup's prefix .hven/install-<head12> in the local clone), then analyze.py against E0's
# per-factorization records (build-dn1de2/det-inputs/fac/, 98,409 raw dense factorizations). Inputs are checked against
# det/inputs.sha256 first (copied from the E0 scratch tree before the declaration).
# usage: run.sh <code head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash run.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
D=$WT/docs/notes/data/2026-11-m8-dn1d-singular/r3/det
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
PREFIX=/home/ghecht/Projects/hven-bench-dn1/.hven/install-${HEAD_SHA:0:12}
B=$WT/build-dn1de3/det-build
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
cd "$WT" || exit 1
sha256sum --quiet -c "$D/inputs.sha256" || { echo INPUTS_SHA_MISMATCH; exit 4; }
grep -q "^HVEN_PIN_SHA $HEAD_SHA\$" "$PREFIX/HVEN_PIN.txt" || { echo PREFIX_NOT_THE_HEAD; exit 4; }
rm -rf "$B"
cmake -S "$D" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$PREFIX" > "$EV/det-configure.log" 2>&1 || { echo CONFIGURE_FAIL; exit 1; }
ninja -C "$B" > "$EV/det-build.log" 2>&1 || { echo BUILD_FAIL; exit 1; }
tail -n +2 "$D/manifest_class.csv" | cut -d, -f1 | sed "s#^#$WT/#" > "$B/manifest.txt"
MKL_NUM_THREADS=1 taskset -c 2 "$B/det" "$EV/det.csv" < "$B/manifest.txt"; echo "det rc=$?"
sed -i "s#^$WT/##" "$EV/det.csv"
python3 "$D/analyze.py" "$D/manifest_class.csv" "$EV/det.csv" "$WT/build-dn1de3/det-inputs/fac" | tee "$EV/det-analysis.txt"
echo "FOREGROUND_END $(date -Is)"
