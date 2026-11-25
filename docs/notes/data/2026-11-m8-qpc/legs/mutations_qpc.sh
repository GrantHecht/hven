#!/usr/bin/env bash
# QPC mutation leg (../DECLARATION-QPC.md section 8): I19-2a's mutations_i19.sh with the table mutate_qpc.py, the QPC test
# filters and an ARGUED-PARITY check: the argued set is PARSED from the declaration's section 8 (rows whose killer column
# starts with ARGUED) and must equal the set this script treats as expected survivors (ARGUED below); a mismatch is
# VERDICT ARGUED_PARITY_FAIL before any build. A control, then each mutation, rebuilt in the Release tree and run on the
# pinning tests; the tree is restored after each. KILLED iff a pinning test fails. Paths derive from this script's location
# and the tree argument.
# usage: mutations_qpc.sh <out> <tree> <build dir> [ids...]   (no ids: every id of the table)
# Run under: ~/.local/bin/box-lane -- setsid -w bash mutations_qpc.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=$(realpath "${2:?tree}"); B=$(realpath "${3:?build dir}"); shift 3
L=$(cd "$(dirname "$0")" && pwd)
M="python3 $L/mutate_qpc.py"
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
ARGUED="Q14 Q17 Q21"   # the development dry run's survivors, argued in section 8; must equal `mutate_qpc.py argued`
DECL_ARGUED=$($M argued)
norm() { tr ' ' '\n' | sed '/^$/d' | sort | tr '\n' ' '; }
if [ "$(echo "$ARGUED" | norm)" != "$(echo "$DECL_ARGUED" | norm)" ]; then
  echo "ARGUED script=[$ARGUED] declaration=[$DECL_ARGUED]"; echo "VERDICT ARGUED_PARITY_FAIL"; exit 3
fi
IDS="$*"; [ -n "$IDS" ] || IDS=$($M ids)
echo "FOREGROUND_START $(date -Is) tree=$TREE head=$(git -C "$TREE" rev-parse HEAD) argued=[$ARGUED]"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_DIRTY_BEFORE; exit 2; }
$M check > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY"; $M restore; exit 2; }
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
TARGETS="hven_qp_seam_tests hven_qp_tests hven_sqp_tests"
SEAM_FILTER='QpCertificateData.*:QpReleaseReadmission.*:QpQc8ImpliedBlocker.*:QpCertificateSign.*'
UNIT_FILTER='EqpSolve*:EqpConvergedRefinement.*'
# The SQP fold of the two counters (Q18) is seen only through a driver: B1Gate's watchdog fixture asserts the folded
# certificate_gate_refusals of its major-0 QP.
SQP_FILTER='B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers'
run_one() { local id=$1 r1 r2 r3
  [ "$id" = control ] || $M "$id" > /dev/null || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "MUT $id BUILD_FAIL"; $M restore > /dev/null; return 8; }
  { timeout 2400 "$B/tests/qp/hven_qp_seam_tests" --gtest_filter="$SEAM_FILTER"; r1=$?
    timeout 600 "$B/tests/qp/hven_qp_tests" --gtest_filter="$UNIT_FILTER"; r2=$?
    timeout 600 "$B/tests/sqp/hven_sqp_tests" --gtest_filter="$SQP_FILTER"; r3=$?; } > "$OUT/$id.tests.log" 2>&1
  local r=$((r1 + r2 + r3))
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.tests.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || $M restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"; sed 's/^/   /' "$OUT/$id.killers.txt"
  case "$id" in control) [ $r -eq 0 ] ;; *) [ $r -ne 0 ] ;; esac
}
is_argued() { case " $ARGUED " in *" $1 "*) return 0 ;; esac; return 1; }
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $IDS; do
  if run_one "$m"; then
    is_argued "$m" && echo "KILLED_ARGUED $m (an argued survivor was killed: reported, not a failure)"
  else
    rc=$?
    if [ $rc -ge 8 ]; then echo "MUT $m DID_NOT_RUN rc=$rc"; verdict="MUTATION_DID_NOT_RUN"
    elif is_argued "$m"; then echo "SURVIVED_AS_ARGUED $m"
    else echo "SURVIVED $m"; verdict="MUTATION_SURVIVED"; fi
  fi
done
timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
