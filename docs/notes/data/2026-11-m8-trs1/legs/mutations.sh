#!/usr/bin/env bash
# usage: mutations.sh <out dir>   Run under `box-lane --`, with build-debug idle (no ctest in flight) and no other build of this tree.
# Each mutation (anchor occurs exactly once, checked) must make the named test target fail; the control must pass.
# The file is restored with `git checkout` after each; the tree is rebuilt at the end.
set -f
W=/home/ghecht/Projects/hven/.claude/worktrees/trs1; B=$W/build-debug
OUT=$(realpath -m "${1:?out}"); mkdir -p "$OUT"
export TMPDIR=$W/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 GIT_CEILING_DIRECTORIES=$W
cd $W || exit 1
[ -z "$(git status --porcelain -- src include tests)" ] || { echo TREE_DIRTY; exit 3; }
run() { # name target filter
  ninja -C $B -j${BOX_LANE_JOBS:-4} $2 > "$OUT/$1.build.log" 2>&1 || { echo "$1 BUILD_FAIL"; return 2; }
  d=${2#hven_}; d=${d%_tests}
  (cd $B && ./tests/$d/$2 --gtest_filter="$3" > "$OUT/$1.log" 2>&1); rc=$?
  echo "$1 rc=$rc ($(grep -E '^\[  (PASSED|FAILED)  \] [0-9]+ test' "$OUT/$1.log" | head -1))"
  return $rc
}
# control: all three targets, their filters
for c in "ipm_tests ObjectiveScaleReporting.*" "sqp_tests ProblemScaling*" "trace_tests *"; do
  set -- $c; run "control-$1" "hven_$1" "$2"; [ $? -eq 0 ] || { echo CONTROL_NOT_CLEAN; exit 5; }
done
# id|file|old|new|target|filter
MUTS=(
 "M1_ipm_tested_stationarity_caller|src/ipm/ipm_solver.cpp|rec.tested_stationarity = r.kkt_inf;|rec.tested_stationarity = r.kkt_inf / scale;|hven_ipm_tests|ObjectiveScaleReporting.*"
 "M2_ipm_tested_complementarity_caller|src/ipm/ipm_solver.cpp|rec.tested_complementarity = r.barr_inf;|rec.tested_complementarity = r.barr_inf / scale;|hven_ipm_tests|ObjectiveScaleReporting.*"
 "M3_sqp_tested_stationarity_caller|src/sqp/sqp_solver.cpp|mj.row.tested_stationarity = mj.kkt.stationarity;|mj.row.tested_stationarity = st.solve_scaling.active ? mj.kkt.stationarity / st.solve_scaling.obj : mj.kkt.stationarity;|hven_sqp_tests|ProblemScaling*"
 "M4_sqp_tested_feasibility_caller|src/sqp/sqp_solver.cpp|rec.tested_feasibility = r.tested_feasibility;|rec.tested_feasibility = r.feasibility;|hven_sqp_tests|ProblemScaling*"
 "M5_sqp_tested_complementarity_caller|src/sqp/sqp_solver.cpp|rec.tested_complementarity = r.tested_complementarity;|rec.tested_complementarity = r.complementarity;|hven_sqp_tests|ProblemScaling*"
 "M6_json_stationarity_from_caller_field|src/trace/json_lines_trace_sink.cpp|\"stationarity\", r.tested_stationarity);|\"stationarity\", r.stationarity);|hven_trace_tests|*"
 "M7_json_caller_key_from_tested_field|src/trace/json_lines_trace_sink.cpp|\"stationarity_caller\", r.stationarity);|\"stationarity_caller\", r.tested_stationarity);|hven_trace_tests|*"
 "M8_console_stat_from_caller_field|src/trace/console_trace_sink.cpp|r.tested_stationarity,|r.stationarity,|hven_trace_tests|*"
)
worst=0
for m in "${MUTS[@]}"; do
  IFS='|' read -r id f old new tgt filt <<< "$m"
  python3 - "$f" "$old" "$new" <<'PY' || { echo "$id ANCHOR_FAIL"; git checkout -- "$f"; worst=1; continue; }
import sys
f,old,new=sys.argv[1:]; s=open(f).read()
assert s.count(old)==1,(old,s.count(old))
open(f,'w').write(s.replace(old,new))
PY
  run "$id" "$tgt" "$filt"; rc=$?
  if [ $rc -eq 0 ]; then echo "$id SURVIVED"; worst=1; else echo "$id KILLED"; fi
  git checkout -- "$f"
done
for t in hven_ipm_tests hven_sqp_tests hven_trace_tests; do ninja -C $B -j${BOX_LANE_JOBS:-4} $t > "$OUT/restore-$t.build.log" 2>&1; done
echo "MUTATIONS worst=$worst"; exit $worst
