#!/usr/bin/env bash
# M: the mutation leg. CONTROL = the code head's own hven_sqp_seam_tests (build-zs/rel) on the zero-step seam test, which must pass.
# MUTANT = an export of the code head with the driver's zero-step threshold reverted to the pre-fix
# `kZeroStepScale * max(1, |x|inf)`, hven_sqp_seam_tests built from it (Release), the same test, which must FAIL (all six
# policy x mode configurations). usage: mutate.sh <code head sha> <out dir>   Run as: flock /tmp/box-build.lock bash mutate.sh ...
set -o pipefail
. "$(dirname "$0")/env.sh"
HEADSHA=${1:?code head}; OUT=${2:?out dir}
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$OUT"
F='SqpLadderSeam.ZeroStep*'
( cd "$B/rel" && setsid -w ./tests/hven_sqp_seam_tests --gtest_filter="$F" > "$OUT/control.txt" 2>&1 ); crc=$?
echo "control rc=$crc"; tail -3 "$OUT/control.txt"
rm -rf "$B/mut-src" "$B/mut"
mkdir -p "$B/mut-src"
git -C "$WT" archive "$HEADSHA" | tar -x -C "$B/mut-src" || exit 1
rm -rf "$B/mut-src/dep/eigen" "$B/mut-src/dep/fmt"
cp -a "$WT/dep/eigen" "$B/mut-src/dep/eigen" && cp -a "$WT/dep/fmt" "$B/mut-src/dep/fmt" || exit 1
python3 - "$B/mut-src/src/drivers/sqp_solver.cpp" <<'PY' || exit 1
import sys
p = sys.argv[1]
s = open(p).read()
old = "mj.row.step_norm <= qp_no_motion_tol(opts_.qp.feasibility_tol, mj.row.step_norm);"
new = "mj.row.step_norm <= kZeroStepScale * std::max(1.0, st.x.lpNorm<Eigen::Infinity>());"
assert s.count(old) == 1
open(p, "w").write(s.replace(old, new))
PY
diff <(git -C "$WT" show "$HEADSHA:src/drivers/sqp_solver.cpp") "$B/mut-src/src/drivers/sqp_solver.cpp" > "$OUT/mutation.diff"
cat "$OUT/mutation.diff"
SRC=$B/mut-src bash "$E/legs/build.sh" Release mut hven_sqp_seam_tests || { echo MUT_BUILD_FAIL; exit 1; }
( cd "$B/mut" && setsid -w ./tests/hven_sqp_seam_tests --gtest_filter="$F" > "$OUT/mutant.txt" 2>&1 ); mrc=$?
echo "mutant rc=$mrc"; grep -c "Failure" "$OUT/mutant.txt"; grep -E "policy [0-9]+ mode [0-9]+" "$OUT/mutant.txt" | sed 's/.*: policy/policy/' | sort | uniq -c
tail -3 "$OUT/mutant.txt"
if [ $crc -eq 0 ] && [ $mrc -ne 0 ]; then echo "MUTATION_KILLED"; rc=0; else echo "MUTATION_NOT_KILLED"; rc=1; fi
echo "FOREGROUND_END $(date -Is)"
exit $rc
