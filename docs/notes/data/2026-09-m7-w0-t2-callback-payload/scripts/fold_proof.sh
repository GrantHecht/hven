#!/usr/bin/env bash
# M7 W0 T2 -- EVERY WRAPPER THIS TASK USES, PROVEN AGAINST A STUBBED FAILURE,
# and run INSIDE the box lock through run_leg.sh so the proof exercises the same
# code the timed legs use.  Each stub makes exactly ONE inner command fail while
# the wrapper's LATER commands SUCCEED -- the shape in which a wrapper that
# reported its last command's status would exit 0 and be believed.
# Expected: every RC below is NON-ZERO.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m7t2/env.sh
P=$F/foldproof
rm -rf "$P"; mkdir -p "$P/bin"
fails=0
check() {
  local name="$1" rc="$2"
  if [ "$rc" -ne 0 ]; then echo "FOLD OK   $name -> RC=$rc (non-zero, as required)"
  else echo "FOLD FAIL $name -> RC=0 (a failure was MASKED)"; fails=$((fails+1)); fi
}
cat > "$P/bin/failbin" <<'STUB'
#!/bin/sh
echo "STUB hven_sqp_corpus: some output on stdout"
echo "STUB hven_sqp_corpus: deliberate failure" >&2
exit 4
STUB
chmod +x "$P/bin/failbin"
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

echo "=== 2. ipm_leg.sh with a failing binary (its last statements are echoes) ==="
BIN_OVERRIDE=$P/bin/failbin $F/ipm_leg.sh perf 1 > "$P/ipm_leg.log" 2>&1
check "ipm_leg.sh" $?
echo "  last line: $(tail -1 "$P/ipm_leg.log")"

echo "=== 3. sqp_perf.sh with a failing binary (its DONE line runs after) ==="
BIN_OVERRIDE=$P/bin/failbin $F/sqp_perf.sh 1 1-1 > "$P/sqp_perf.log" 2>&1
check "sqp_perf.sh" $?
echo "  last line: $(tail -1 "$P/sqp_perf.log")"

echo "=== 4. build_only.sh with a failing cmake (its last command is a grep that SUCCEEDS) ==="
PATH="$P/bin:$PATH" $F/build_only.sh > "$P/build.log" 2>&1
check "build_only.sh" $?
echo "  last line: $(tail -1 "$P/build.log")"

echo "=== 5. configure.sh with a failing cmake (its last command is a tail that SUCCEEDS) ==="
PATH="$P/bin:$PATH" HVEN_FOLDPROOF_CONFIGURE=1 bash -o pipefail -c '
  source '"$F"'/env.sh
  B='"$P"'/fakebuild
  rm -rf "$B"; mkdir -p "$B"
  CCACHE_DISABLE=1 cmake -S "$R" -B "$B" > /dev/null 2>&1
  rc=$?
  echo "CONFIGURE_RC=$rc"
  tail -1 /dev/null
  exit $rc'
check "configure.sh shape" $?

echo "=== 6. the comparator wrapper: a python3 that fails, with a succeeding step after ==="
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
