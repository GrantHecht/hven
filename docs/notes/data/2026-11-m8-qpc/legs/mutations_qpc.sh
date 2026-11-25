#!/usr/bin/env bash
# QPC mutation leg (../DECLARATION-QPC.md section 8): I19-2a's mutations_i19.sh with the table mutate_qpc.py, the QPC test
# filters and an ARGUED-PARITY check: the argued set is PARSED from the declaration's section 8 (rows whose killer column
# starts with ARGUED) and must equal the set this script treats as expected survivors (ARGUED below); a mismatch is
# VERDICT ARGUED_PARITY_FAIL before any build; the table's ids must equal mutate_qpc.py's (ID_PARITY_FAIL). A control, then
# each mutation, rebuilt in the Release tree and run on the pinning tests; the tree is restored after each. KILLED iff a
# pinning test fails. DECLARED-KILLER PARITY (after the run): every test the table names as a killer of a must-kill id
# (Suite.Name, or Suite.* for any test of the suite) must be among that id's observed killers, else VERDICT
# KILLER_PARITY_FAIL (RST-B's mutations_rstb.sh lists killers; this checks them). Paths derive from this script's
# location and the tree argument.
# usage: mutations_qpc.sh <out> <tree> <build dir> [ids...]   (no ids: every id of the table)
# Run under: ~/.local/bin/box-lane -- setsid -w bash mutations_qpc.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=$(realpath "${2:?tree}"); B=$(realpath "${3:?build dir}"); shift 3
L=$(cd "$(dirname "$0")" && pwd)
M="python3 $L/mutate_qpc.py"
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
ARGUED="Q17"   # argued in section 8 (pre-freeze review M5: Q17 only); must equal `mutate_qpc.py argued`
DECL_ARGUED=$($M argued)
norm() { tr ' ' '\n' | sed '/^$/d' | sort | tr '\n' ' '; }
if [ "$(echo "$ARGUED" | norm)" != "$(echo "$DECL_ARGUED" | norm)" ]; then
  echo "ARGUED script=[$ARGUED] declaration=[$DECL_ARGUED]"; echo "VERDICT ARGUED_PARITY_FAIL"; exit 3
fi
TABLE_IDS=$(python3 - "$L/../DECLARATION-QPC.md" <<'PY'
import re, sys
sec = open(sys.argv[1]).read().split("## 8.", 1)[1].split("\n## ", 1)[0]
print(" ".join(m.group(1) for m in (re.match(r"^\|\s*(Q\d+)\s*\|", l) for l in sec.splitlines()) if m))
PY
)
if [ "$(echo "$TABLE_IDS" | norm)" != "$($M ids | norm)" ]; then
  echo "IDS table=[$TABLE_IDS] script=[$($M ids)]"; echo "VERDICT ID_PARITY_FAIL"; exit 3
fi
IDS="$*"; [ -n "$IDS" ] || IDS=$($M ids)
echo "FOREGROUND_START $(date -Is) tree=$TREE head=$(git -C "$TREE" rev-parse HEAD) argued=[$ARGUED]"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_DIRTY_BEFORE; exit 2; }
$M check > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY"; $M restore; exit 2; }
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
TARGETS="hven_qp_seam_tests hven_qp_tests hven_sqp_tests hven_sqp_seam_tests"
SEAM_FILTER='QpCertificateData.*:QpReleaseReadmission.*:QpQc8ImpliedBlocker.*:QpCertificateSign.*'
UNIT_FILTER='EqpSolve*:EqpConvergedRefinement.*'
# The SQP fold of the two counters (Q18) is seen only through a driver: B1Gate's watchdog fixture asserts the folded
# certificate_gate_refusals of its major-0 QP. Q41 (the seed guard's Jacobian screen): the poisoned-seed fixture.
SQP_FILTER='B1Gate.TheWatchdogRestoreCarriesTheComplementarityGateWithTheMultipliers:SqpElasticWitnessStart.Sawpath*:SqpSolverRestorationSeed.AJacobianPoisonedSeedCandidateReachesTheScreenAndIsRefused'
# The four other fold sites (Q32-Q35): the gate-seam fixtures of hven_sqp_seam_tests.
SQP_SEAM_FILTER='SqpQpcFold.*'
run_one() { local id=$1 r1 r2 r3 r4
  [ "$id" = control ] || $M "$id" > /dev/null || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "MUT $id BUILD_FAIL"; $M restore > /dev/null; return 8; }
  { timeout 2400 "$B/tests/qp/hven_qp_seam_tests" --gtest_filter="$SEAM_FILTER"; r1=$?
    timeout 600 "$B/tests/qp/hven_qp_tests" --gtest_filter="$UNIT_FILTER"; r2=$?
    timeout 600 "$B/tests/sqp/hven_sqp_tests" --gtest_filter="$SQP_FILTER"; r3=$?
    timeout 1200 "$B/tests/sqp/hven_sqp_seam_tests" --gtest_filter="$SQP_SEAM_FILTER"; r4=$?; } > "$OUT/$id.tests.log" 2>&1
  local r=$((r1 + r2 + r3 + r4))
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
# DECLARED-KILLER PARITY: each must-kill id's declared killers (section 8) among its observed killers.
python3 - "$L/../DECLARATION-QPC.md" "$OUT" "$ARGUED" $IDS <<'PY' || verdict=KILLER_PARITY_FAIL
import os, re, sys
decl, out, argued, ids = sys.argv[1], sys.argv[2], set(sys.argv[3].split()), sys.argv[4:]
sec = open(decl).read().split("## 8.", 1)[1].split("\n## ", 1)[0]
declared = {}
for line in sec.splitlines():
    m = re.match(r"^\|\s*(Q\d+)\s*\|[^|]*\|(.*)\|\s*$", line)
    if m:
        declared[m.group(1)] = re.findall(r"\b([A-Z][A-Za-z0-9]*)\.([A-Za-z0-9_]+|\*)", m.group(2))
bad = []
for i in ids:
    if i in argued:
        continue
    f = os.path.join(out, f"{i}.killers.txt")
    seen = set(open(f).read().split()) if os.path.exists(f) else set()
    if not declared.get(i):
        bad.append(f"{i}: no declared killer in the table")
    for suite, name in declared.get(i, []):
        ok = any(k.startswith(suite + ".") for k in seen) if name == "*" else f"{suite}.{name}" in seen
        if not ok:
            bad.append(f"{i}: declared killer {suite}.{name} did not kill")
for b in bad:
    print("KILLER_PARITY " + b)
print(f"KILLER_PARITY checked={len([i for i in ids if i not in argued])} failures={len(bad)}")
sys.exit(1 if bad else 0)
PY
timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
