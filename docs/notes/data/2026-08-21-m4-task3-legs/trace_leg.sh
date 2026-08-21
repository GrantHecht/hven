#!/bin/bash
# =============================================================================
# SOLVE-TRACE A/B.
#
# The IPM's full deterministic print, base vs head, compared by md5. Print level
# 0 is the full report; the only non-deterministic content in it is wall-clock
# ("... ms") lines, which are dropped, and ANSI colour, which is stripped, so
# the comparison is over the characters the solver decided to print. Everything
# that survives is a counter: problem statistics, KKT dimensions and NNZ, every
# iteration-table row (mu, prim obj, bar obj, KKT inf, bar inf, econ inf, icon
# inf, alphaP, alphaD, LS, PPS, HF, HPert), the exit block, and the returned x
# to 17 digits.
#
# TWO INSTRUMENTS, kept separate on purpose.
#
#   probe_*  HS071 + Rosenbrock on default settings. Unchanged since the Task 2b
#            carve, so its md5 is comparable across the whole M4 chain; do not
#            add cells to it.
#   modes_*  Five cells that each drive a solver path the default settings never
#            reach, all configured from PUBLIC settings:
#              hs071_lang             LANG line search -> the merit acceptance's
#                                     own first-order right-hand-side request
#              hs071_filter           filter acceptance -> the generic
#                                     trial-point evaluator
#              maratos_soc            the Maratos problem with SOC enabled ->
#                                     the second-order correction's trial
#                                     constraint evaluation
#              hs071_optno            objective-free phase -> the OPTNO arm of
#                                     the evaluation switch
#              infeasible_restoration two conflicting linear inequalities ->
#                                     feasibility restoration, its evaluation
#                                     branch, and both restoration-exit
#                                     objective sites
#
# Cells can be run individually: `modes_head 0 hs071_lang`.
# =============================================================================
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
strip() { grep -v " ms" | sed -e 's/\x1b\[[0-9;]*m//g'; }
status=0
for pair in "probe:trace" "modes:modes_trace"; do
  bin="${pair%%:*}"; out="${pair##*:}"
  "$HERE/${bin}_base" 0 2>&1 | strip > "$HERE/${out}_base.txt"
  "$HERE/${bin}_head" 0 2>&1 | strip > "$HERE/${out}_head.txt"
  md5sum "$HERE/${out}_base.txt" "$HERE/${out}_head.txt"
  if diff -q "$HERE/${out}_base.txt" "$HERE/${out}_head.txt" > /dev/null; then
    echo "  $bin: IDENTICAL"
  else
    echo "  $bin: DIFFERS"; diff "$HERE/${out}_base.txt" "$HERE/${out}_head.txt" | head -40; status=1
  fi
done
exit $status
