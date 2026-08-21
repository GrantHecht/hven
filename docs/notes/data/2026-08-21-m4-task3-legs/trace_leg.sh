#!/bin/bash
# Solve-trace A/B: the IPM's full deterministic print for HS071 + Rosenbrock,
# base vs head, compared by md5. Print level 0 is the full report; the only
# non-deterministic content in it is wall-clock ("... ms") lines, which are
# dropped, and ANSI colour, which is stripped so the comparison is over the
# characters the solver decided to print. Everything that survives -- problem
# statistics, KKT dimensions/NNZ, every iteration-table row (mu, prim obj, bar
# obj, KKT inf, bar inf, econ inf, icon inf, alphaP, alphaD, LS, PPS, HF,
# HPert), the exit block and the returned x to 17 digits -- is a counter.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
strip() { grep -v " ms" | sed -e 's/\x1b\[[0-9;]*m//g'; }
"$HERE/probe_base" 0 2>&1 | strip > "$HERE/trace_base.txt"
"$HERE/probe_head" 0 2>&1 | strip > "$HERE/trace_head.txt"
md5sum "$HERE/trace_base.txt" "$HERE/trace_head.txt"
if diff -q "$HERE/trace_base.txt" "$HERE/trace_head.txt" > /dev/null; then
  echo "TRACE A/B: IDENTICAL"
else
  echo "TRACE A/B: DIFFERS"; diff "$HERE/trace_base.txt" "$HERE/trace_head.txt"; exit 1
fi
