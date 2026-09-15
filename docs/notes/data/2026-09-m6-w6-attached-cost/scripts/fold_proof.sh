#!/usr/bin/env bash
# W6 T6 -- EVERY WRAPPER THIS TASK USES, PROVEN AGAINST A STUBBED FAILURE, and
# run INSIDE the box lock through run_leg.sh so the proof exercises the same
# code the timed legs use.  Each stub makes exactly ONE inner command fail while
# the wrapper's LATER commands SUCCEED -- the shape in which a wrapper that
# reported its last command's status would exit 0 and be believed.
# Expected: every RC below is NON-ZERO.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/w6t6/env.sh
P=$F/foldproof
rm -rf "$P"; mkdir -p "$P/bin"
fails=0
check() {
  local name="$1" rc="$2"
  if [ "$rc" -ne 0 ]; then echo "FOLD OK   $name -> RC=$rc (non-zero, as required)"
  else echo "FOLD FAIL $name -> RC=0 (a failure was MASKED)"; fails=$((fails+1)); fi
}

# A "binary" that always fails but prints to stdout first, so a wrapper reading
# a later echo or a trailing row count would still look fine.
cat > "$P/bin/failbin" <<'STUB'
#!/bin/sh
echo "STUB hven_sqp_corpus: some output on stdout"
echo "STUB hven_sqp_corpus: deliberate failure" >&2
exit 4
STUB
chmod +x "$P/bin/failbin"
# A cmake that always fails.
cat > "$P/bin/cmake" <<'STUB'
#!/bin/sh
echo "STUB cmake: deliberate failure" >&2
exit 1
STUB
chmod +x "$P/bin/cmake"

echo "=== 1. common.sh's fold: timed_run on a FAILING command, then a SUCCEEDING one ==="
(
  source $F/common.sh
  echo "  LEG_RC at entry = $LEG_RC"
  run /bin/true;  echo "  after a succeeding run:  LEG_RC=$LEG_RC"
  timed_run "foldproof/fail" /dev/null -- /bin/sh -c 'echo out; echo err >&2; exit 4'
  echo "  after a FAILING timed_run: LEG_RC=$LEG_RC"
  run /bin/true;  echo "  after a LATER succeeding run: LEG_RC=$LEG_RC (must stay 4)"
  exit $LEG_RC
)
check "common.sh timed_run/fold" $?

echo "=== 2. sqp_perf.sh with a failing binary (its last statements are echoes) ==="
BIN_OVERRIDE=$P/bin/failbin $F/sqp_perf.sh ipm 1 1 > "$P/sqp_perf.log" 2>&1
check "sqp_perf.sh" $?
echo "  last line: $(tail -1 "$P/sqp_perf.log")"

echo "=== 3. sqp_wall.sh with a failing binary (its DONE line runs a grep after) ==="
BIN_OVERRIDE=$P/bin/failbin $F/sqp_wall.sh ipm a00 > "$P/sqp_wall.log" 2>&1
check "sqp_wall.sh" $?
echo "  last line: $(tail -1 "$P/sqp_wall.log")"

echo "=== 4. ipm_leg.sh with a failing binary ==="
BIN_OVERRIDE=$P/bin/failbin $F/ipm_leg.sh perf 1 > "$P/ipm_leg.log" 2>&1
check "ipm_leg.sh" $?
echo "  last line: $(tail -1 "$P/ipm_leg.log")"

echo "=== 5. hs_leg.sh with a failing binary ==="
BIN_OVERRIDE=$P/bin/failbin $F/hs_leg.sh perf 1 > "$P/hs_leg.log" 2>&1
check "hs_leg.sh" $?
echo "  last line: $(tail -1 "$P/hs_leg.log")"

echo "=== 6. build_only.sh with a failing cmake (its last command is a grep that SUCCEEDS) ==="
PATH="$P/bin:$PATH" $F/build_only.sh > "$P/build.log" 2>&1
check "build_only.sh" $?
echo "  last line: $(tail -1 "$P/build.log")"

echo "=== 7. the comparator wrapper: a python3 that fails, with a succeeding step after ==="
cat > "$P/bin/python3" <<'STUB'
#!/bin/sh
echo "STUB python3: deliberate comparator failure" >&2
exit 1
STUB
chmod +x "$P/bin/python3"
(
  source $F/common.sh
  PATH="$P/bin:$PATH" run python3 -c 'pass'
  run /bin/true
  exit $LEG_RC
)
check "common.sh fold around the comparator" $?

echo "FOLD_PROOF_FAILURES=$fails"
[ "$fails" -eq 0 ] || exit 1
echo "ALL WRAPPERS CARRY A FAILING STATUS OUT."
exit 0
